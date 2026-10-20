/**
 * Cuenta del dueño de la mesa en Supabase (correo y contraseña), con peticiones HTTP sencillas a /auth/v1: las
 * mismas que hará la pantalla de 7" (docs/NUBE.md). La sesión (tokens) se guarda en este aparato y se renueva sola.
 */
import type { SupabaseConn } from '../profiles';

export interface Session {
  accessToken: string;
  refreshToken: string;
  /** Cuándo caduca el token de acceso (epoch ms). */
  expiresAt: number;
  userId: string;
  email: string;
}

export class CloudError extends Error {
  constructor(message: string, public readonly code?: string) {
    super(message);
    this.name = 'CloudError';
  }
}

type Http = typeof fetch;
const defaultHttp: Http = (...a) => fetch(...a);

/** Mensajes de Supabase → frases para la pantalla. */
export function friendlyError(status: number, body: string): string {
  const b = body.toLowerCase();
  if (b.includes('invalid login credentials') || b.includes('invalid_credentials')) return 'Correo o contraseña incorrectos';
  if (b.includes('email not confirmed') || b.includes('email_not_confirmed')) return 'Falta confirmar el correo: mira tu bandeja de entrada';
  if (b.includes('already registered') || b.includes('user_already_exists')) return 'Ya hay una cuenta con ese correo';
  if (b.includes('password') && (b.includes('at least') || b.includes('weak'))) return 'La contraseña tiene que tener al menos 6 caracteres';
  if (b.includes('invalid') && b.includes('email')) return 'Ese correo no es válido';
  if (status === 429 || b.includes('rate limit')) return 'Demasiados intentos: espera un minuto';
  if (status === 401 || status === 403) return 'La sesión ha caducado: vuelve a entrar';
  return `No se pudo hablar con la nube (${status})`;
}

async function call(conn: SupabaseConn, path: string, body: object, http: Http, token?: string): Promise<any> {
  let res: Response;
  try {
    res = await http(`${conn.url}/auth/v1/${path}`, {
      method: 'POST',
      headers: { apikey: conn.anonKey, Authorization: `Bearer ${token ?? conn.anonKey}`, 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
  } catch {
    throw new CloudError('Sin conexión con la nube', 'offline');
  }
  const text = await res.text().catch(() => '');
  if (!res.ok) {
    if (path === 'token?grant_type=refresh_token' && (res.status === 400 || res.status === 401 || res.status === 403)) {
      throw new CloudError('La sesión ha caducado: vuelve a entrar', 'session_expired');
    }
    throw new CloudError(friendlyError(res.status, text), String(res.status));
  }
  try {
    return text ? JSON.parse(text) : {};
  } catch {
    return {};
  }
}

function toSession(data: any, now: number): Session {
  if (!data?.access_token || !data?.refresh_token) throw new CloudError('Respuesta de la nube incompleta');
  return {
    accessToken: data.access_token,
    refreshToken: data.refresh_token,
    expiresAt: now + Number(data.expires_in ?? 3600) * 1000,
    userId: data.user?.id ?? '',
    email: data.user?.email ?? '',
  };
}

export async function signIn(conn: SupabaseConn, email: string, password: string, http: Http = defaultHttp, now = Date.now()): Promise<Session> {
  return toSession(await call(conn, 'token?grant_type=password', { email: email.trim(), password }, http), now);
}

/** Cuenta nueva. Si el proyecto pide confirmar el correo, no hay sesión hasta confirmarlo. */
export async function signUp(
  conn: SupabaseConn,
  email: string,
  password: string,
  http: Http = defaultHttp,
  now = Date.now(),
): Promise<{ session: Session | null; needsConfirm: boolean }> {
  const data = await call(conn, 'signup', { email: email.trim(), password }, http);
  if (data?.access_token) return { session: toSession(data, now), needsConfirm: false };
  return { session: null, needsConfirm: true };
}

export async function recover(conn: SupabaseConn, email: string, http: Http = defaultHttp): Promise<void> {
  await call(conn, 'recover', { email: email.trim() }, http);
}

export async function refreshSession(conn: SupabaseConn, s: Session, http: Http = defaultHttp, now = Date.now()): Promise<Session> {
  const next = toSession(await call(conn, 'token?grant_type=refresh_token', { refresh_token: s.refreshToken }, http), now);
  return { ...next, userId: next.userId || s.userId, email: next.email || s.email };
}

export async function signOut(conn: SupabaseConn, s: Session, http: Http = defaultHttp): Promise<void> {
  await call(conn, 'logout?scope=local', {}, http, s.accessToken).catch(() => undefined);  // sin conexión: se cierra igual aquí
}

// ---------------------------------------------------------------- sesión guardada en este aparato
export const SESSION_KEY = 'mfv3:nube:sesion';
export const REMEMBER_KEY = 'mfv3:nube:recordar';

/** Es una preferencia del aparato: no se sincroniza con la cuenta. Las sesiones antiguas se conservan. */
export function readRememberSession(store: Pick<Storage, 'getItem'>): boolean {
  try { return store.getItem(REMEMBER_KEY) !== '0'; } catch { return true; }
}

export function readSession(store: Pick<Storage, 'getItem'>): Session | null {
  try {
    const raw = store.getItem(SESSION_KEY);
    const s = raw ? (JSON.parse(raw) as Session) : null;
    return s && s.accessToken && s.refreshToken ? s : null;
  } catch {
    return null;
  }
}

export function writeSession(store: Pick<Storage, 'setItem' | 'removeItem'>, s: Session | null): void {
  try {
    if (s) store.setItem(SESSION_KEY, JSON.stringify(s));
    else store.removeItem(SESSION_KEY);
  } catch {
    /* sin almacenamiento: la sesión dura lo que la app abierta */
  }
}

/** Sesión lista para usar: si caduca en menos de un minuto, se renueva antes. */
export async function freshSession(conn: SupabaseConn, s: Session, http: Http = defaultHttp, now = Date.now()): Promise<Session> {
  return s.expiresAt - now > 60_000 ? s : refreshSession(conn, s, http, now);
}
