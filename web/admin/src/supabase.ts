/**
 * Conector del modo REAL con Supabase. Habla directamente con su API (sin bibliotecas extra):
 *   - Entrar: correo y contraseña (/auth/v1/token) y después, SIEMPRE, el código de 6 cifras de una app
 *     de verificación (TOTP, /auth/v1/factors). La primera vez se da de alta escaneando un código QR.
 *   - Datos: /rest/v1/<tabla>, con el token del usuario. Los permisos los decide el SERVIDOR
 *     (políticas RLS de supabase/schema.sql): sin rol admin/soporte, sin cuenta bloqueada y sin haber
 *     pasado la verificación en dos pasos (aal2), la base de datos no devuelve nada, aunque alguien trucase el panel.
 * La clave «anon» de Supabase es pública por diseño; lo que protege los datos son esas políticas.
 * La sesión vive solo en esta pestaña (sessionStorage), se renueva sola y al salir se revoca en el servidor.
 */
import type { AdminBackend, KeyValueStore, MfaStep } from './backend';
import type { AdminData, AdminSession, Collection, ItemOf, Role, Settings } from './types';

/** Tabla de la base de datos para cada colección del panel. */
export const TABLES: Record<Collection, string> = {
  users: 'profiles',
  venues: 'venues',
  products: 'products',
  orders: 'orders',
  licenses: 'licenses',
  coupons: 'coupons',
  announcements: 'announcements',
  audit: 'audit_log',
};

const toSnake = (k: string) => k.replace(/[A-Z]/g, (c) => `_${c.toLowerCase()}`);
const toCamel = (k: string) => k.replace(/_([a-z])/g, (_, c: string) => c.toUpperCase());
// Las fechas viajan como milisegundos (bigint) para que el panel y la base de datos hablen igual.
export const toDb = (o: object) => Object.fromEntries(Object.entries(o).map(([k, v]) => [toSnake(k), v === undefined ? null : v]));
export const fromDb = <T,>(o: Record<string, unknown>): T =>
  Object.fromEntries(Object.entries(o).filter(([, v]) => v !== null).map(([k, v]) => [toCamel(k), v])) as T;

type Fetch = typeof fetch;
const SESSION_KEY = 'mfv3-admin-sesion-real';

interface AuthTokens {
  access_token: string;
  refresh_token: string;
  expires_in?: number;
  user: { id: string; email: string; factors?: { id: string; factor_type: string; status: string }[] };
}

/** Inicio de sesión a medias: contraseña correcta, falta el código. Solo vive en memoria. */
interface Pending {
  email: string;
  role: Role;
  tokens: AuthTokens;
}

export class SupabaseBackend implements AdminBackend {
  readonly mode = 'real' as const;

  constructor(
    private url: string,
    private anonKey: string,
    private sessionStore: KeyValueStore,
    private http: Fetch = (...a) => fetch(...a),
  ) {
    this.url = url.replace(/\/+$/, '');
  }

  needsSetup(): boolean {
    return false; // las cuentas se crean en Supabase (Authentication → Users) y el rol en la tabla profiles
  }

  async createAdmin(): Promise<AdminSession> {
    throw new Error('En modo real, crea el usuario en Supabase y ponle el rol «admin» (ver docs/ADMIN.md).');
  }

  private headers(extra: Record<string, string> = {}): Record<string, string> {
    const s = this.session();
    return { apikey: this.anonKey, Authorization: `Bearer ${s?.token ?? this.anonKey}`, 'Content-Type': 'application/json', ...extra };
  }

  private pending: Pending | null = null;

  private async req<T>(path: string, init: RequestInit = {}): Promise<T> {
    const r = await this.http(`${this.url}${path}`, init);
    if (!r.ok) {
      let msg = `${r.status}`;
      try {
        const j = (await r.json()) as { message?: string; error_description?: string; msg?: string };
        msg = j.message ?? j.error_description ?? j.msg ?? msg;
      } catch {
        /* sin cuerpo */
      }
      throw new Error(`Servidor: ${msg}`);
    }
    return r.status === 204 ? (undefined as T) : ((await r.json().catch(() => undefined)) as T);
  }

  private authHeaders(token: string): Record<string, string> {
    return { apikey: this.anonKey, Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' };
  }

  /** Paso 1: contraseña. Devuelve el paso del código (alta con QR la primera vez). */
  async signIn(email: string, password: string): Promise<MfaStep> {
    this.pending = null;
    const tokens = await this.req<AuthTokens>('/auth/v1/token?grant_type=password', {
      method: 'POST',
      headers: { apikey: this.anonKey, 'Content-Type': 'application/json' },
      body: JSON.stringify({ email: email.trim(), password }),
    });
    const h = this.authHeaders(tokens.access_token);
    const rows = await this.req<{ role: Role; bloqueado?: boolean }[]>(
      `/rest/v1/profiles?id=eq.${encodeURIComponent(tokens.user.id)}&select=role,bloqueado`,
      { headers: h },
    );
    const role = rows[0]?.role;
    if ((role !== 'admin' && role !== 'soporte') || rows[0]?.bloqueado) {
      await this.revoke(tokens.access_token);
      throw new Error('Esta cuenta no tiene permiso de administración');
    }
    this.pending = { email: tokens.user.email, role, tokens };
    const factor = tokens.user.factors?.find((f) => f.factor_type === 'totp' && f.status === 'verified');
    if (factor) return { kind: 'mfa', factorId: factor.id };
    // Primera vez: se borran altas a medias y se da de alta una nueva (código QR para la app).
    for (const f of tokens.user.factors?.filter((x) => x.status !== 'verified') ?? []) {
      await this.req(`/auth/v1/factors/${encodeURIComponent(f.id)}`, { method: 'DELETE', headers: h }).catch(() => undefined);
    }
    const enrolled = await this.req<{ id: string; totp: { qr_code: string; secret: string } }>('/auth/v1/factors', {
      method: 'POST',
      headers: h,
      body: JSON.stringify({ factor_type: 'totp', friendly_name: `Panel ${new Date().toISOString().slice(0, 16)}`, issuer: 'Marcador Futbolín' }),
    });
    return { kind: 'mfa', factorId: enrolled.id, enroll: { qr: enrolled.totp.qr_code, secret: enrolled.totp.secret } };
  }

  /** Paso 2: código de 6 cifras de la app de verificación. Sin él no hay sesión. */
  async verifyMfa(step: MfaStep, code: string): Promise<AdminSession> {
    const p = this.pending;
    if (!p) throw new Error('La verificación ha caducado: vuelve a escribir la contraseña');
    if (!/^\d{6}$/.test(code.trim())) throw new Error('El código tiene 6 cifras');
    const h = this.authHeaders(p.tokens.access_token);
    const challenge = await this.req<{ id: string }>(`/auth/v1/factors/${encodeURIComponent(step.factorId)}/challenge`, { method: 'POST', headers: h });
    const tokens = await this.req<AuthTokens>(`/auth/v1/factors/${encodeURIComponent(step.factorId)}/verify`, {
      method: 'POST',
      headers: h,
      body: JSON.stringify({ challenge_id: challenge.id, code: code.trim() }),
    });
    this.pending = null;
    const s = this.toSession(p.email, p.role, tokens);
    this.sessionStore.setItem(SESSION_KEY, JSON.stringify(s));
    return s;
  }

  private toSession(email: string, role: Role, t: AuthTokens): AdminSession {
    return { email, role, token: t.access_token, refreshToken: t.refresh_token, expiresAt: Date.now() + (t.expires_in ?? 3600) * 1000 };
  }

  private async revoke(token: string): Promise<void> {
    await this.http(`${this.url}/auth/v1/logout`, { method: 'POST', headers: this.authHeaders(token) }).catch(() => undefined);
  }

  /** Renueva el token si le queda menos de un minuto (sigue con la verificación en dos pasos hecha). */
  private async fresh(): Promise<void> {
    const s = this.session();
    if (!s?.refreshToken || (s.expiresAt ?? 0) - Date.now() > 60_000) return;
    try {
      const t = await this.req<AuthTokens>('/auth/v1/token?grant_type=refresh_token', {
        method: 'POST',
        headers: { apikey: this.anonKey, 'Content-Type': 'application/json' },
        body: JSON.stringify({ refresh_token: s.refreshToken }),
      });
      this.sessionStore.setItem(SESSION_KEY, JSON.stringify(this.toSession(s.email, s.role, t)));
    } catch {
      this.sessionStore.removeItem(SESSION_KEY);
      throw new Error('La sesión ha caducado: vuelve a entrar');
    }
  }

  signOut(): void {
    const s = this.session();
    this.pending = null;
    this.sessionStore.removeItem(SESSION_KEY);
    if (s?.token) void this.revoke(s.token);
  }

  session(): AdminSession | null {
    const raw = this.sessionStore.getItem(SESSION_KEY);
    return raw ? (JSON.parse(raw) as AdminSession) : null;
  }

  async load(): Promise<AdminData> {
    await this.fresh();
    const get = async <C extends Collection>(c: C, order: string) =>
      (await this.req<Record<string, unknown>[]>(`/rest/v1/${TABLES[c]}?select=*&order=${order}&limit=10000`, { headers: this.headers() })).map(
        (r) => fromDb<ItemOf<C>>(r),
      ) as AdminData[C];
    const [users, venues, products, orders, licenses, coupons, announcements, audit, settings] = await Promise.all([
      get('users', 'created_at.desc'),
      get('venues', 'desde.desc'),
      get('products', 'precio.asc'),
      get('orders', 'fecha.desc'),
      get('licenses', 'creada.desc'),
      get('coupons', 'code.asc'),
      get('announcements', 'desde.desc'),
      get('audit', 'at.desc'),
      this.req<Record<string, unknown>[]>(`/rest/v1/settings?id=eq.ajustes&select=*`, { headers: this.headers() }),
    ]);
    return { users, venues, products, orders, licenses, coupons, announcements, audit, settings: fromDb<Settings>(settings[0] ?? {}) };
  }

  /**
   * Modificar si existe (PATCH) y crear si no (POST). No se usa «upsert» del servidor porque exige permiso de
   * crear además de modificar, y soporte puede modificar pedidos (envíos) pero no crearlos.
   */
  async upsert<C extends Collection>(col: C, item: ItemOf<C>): Promise<void> {
    await this.fresh();
    const body = JSON.stringify(toDb(item));
    const updated = await this.req<unknown[]>(`/rest/v1/${TABLES[col]}?id=eq.${encodeURIComponent(item.id)}`, {
      method: 'PATCH',
      headers: this.headers({ Prefer: 'return=representation' }),
      body,
    });
    if (Array.isArray(updated) && updated.length > 0) return;
    await this.req(`/rest/v1/${TABLES[col]}`, { method: 'POST', headers: this.headers({ Prefer: 'return=minimal' }), body });
  }

  async remove(col: Collection, id: string): Promise<void> {
    await this.fresh();
    await this.req(`/rest/v1/${TABLES[col]}?id=eq.${encodeURIComponent(id)}`, { method: 'DELETE', headers: this.headers() });
  }

  async saveSettings(s: Settings): Promise<void> {
    await this.fresh();
    await this.req(`/rest/v1/settings`, {
      method: 'POST',
      headers: this.headers({ Prefer: 'resolution=merge-duplicates,return=minimal' }),
      body: JSON.stringify(toDb(s)),
    });
  }
}
