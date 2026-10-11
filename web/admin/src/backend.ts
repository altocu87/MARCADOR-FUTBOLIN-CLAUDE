/**
 * De dónde lee y dónde guarda el panel.
 * - LocalBackend (modo DEMOSTRACIÓN): datos de ejemplo en este navegador. Sirve para ver y probar el panel.
 *   Su contraseña solo evita miradas casuales: NO es seguridad real.
 * - SupabaseBackend (modo REAL, ver supabase.ts): base de datos con usuarios, roles y permisos en el servidor.
 * Las pantallas solo conocen esta interfaz, así que pasar de uno a otro no cambia el panel.
 */
import { seedData } from './seed';
import type { AdminData, AdminSession, Collection, ItemOf, Settings } from './types';

/** Segundo paso al entrar en modo real: código de la app de verificación (con QR la primera vez). */
export interface MfaStep {
  kind: 'mfa';
  factorId: string;
  /** Primera vez: código QR (imagen) y clave para escribirla a mano en la app. */
  enroll?: { qr: string; secret: string };
}

export interface AdminBackend {
  readonly mode: 'demo' | 'real';
  /** Modo demo: true si todavía no se ha creado la cuenta de administrador local. */
  needsSetup(): boolean;
  createAdmin(email: string, password: string): Promise<AdminSession>;
  /** Demo: entra directamente. Real: devuelve el paso del código de verificación. */
  signIn(email: string, password: string): Promise<AdminSession | MfaStep>;
  /** Solo real: comprueba el código de 6 cifras y abre la sesión. */
  verifyMfa?(step: MfaStep, code: string): Promise<AdminSession>;
  signOut(): void;
  session(): AdminSession | null;
  load(): Promise<AdminData>;
  upsert<C extends Collection>(col: C, item: ItemOf<C>): Promise<void>;
  remove(col: Collection, id: string): Promise<void>;
  saveSettings(s: Settings): Promise<void>;
  /** Catálogo: sube un archivo (ruta dentro del almacén) y devuelve su dirección. */
  uploadFile(path: string, file: Blob): Promise<string>;
  removeFiles(paths: string[]): Promise<void>;
  /** Solo demo: vuelve a los datos de ejemplo. */
  reset?(): Promise<void>;
}

export interface KeyValueStore {
  getItem(k: string): string | null;
  setItem(k: string, v: string): void;
  removeItem(k: string): void;
}

const memoryStore = (): KeyValueStore => {
  const m = new Map<string, string>();
  return { getItem: (k) => m.get(k) ?? null, setItem: (k, v) => void m.set(k, v), removeItem: (k) => void m.delete(k) };
};

function safeStore(get: () => Storage): KeyValueStore {
  try {
    const s = get();
    s.setItem('__prueba', '1');
    s.removeItem('__prueba');
    return s;
  } catch {
    return memoryStore();
  }
}

export async function hashPassword(password: string, salt: string): Promise<string> {
  const data = new TextEncoder().encode(`${salt}:${password}`);
  const buf = await crypto.subtle.digest('SHA-256', data);
  return [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

const DATA_KEY = 'mfv3-admin-demo-datos';
const ACCOUNT_KEY = 'mfv3-admin-demo-cuenta';
const SESSION_KEY = 'mfv3-admin-sesion';

export class LocalBackend implements AdminBackend {
  readonly mode = 'demo' as const;
  private data: AdminData | null = null;

  constructor(
    private store: KeyValueStore = typeof localStorage !== 'undefined' ? safeStore(() => localStorage) : memoryStore(),
    private sessionStore: KeyValueStore = typeof sessionStorage !== 'undefined' ? safeStore(() => sessionStorage) : memoryStore(),
    private now: () => number = Date.now,
  ) {}

  needsSetup(): boolean {
    return !this.store.getItem(ACCOUNT_KEY);
  }

  async createAdmin(email: string, password: string): Promise<AdminSession> {
    if (!this.needsSetup()) throw new Error('Ya existe una cuenta de administrador');
    if (!/^\S+@\S+\.\S+$/.test(email)) throw new Error('Escribe un correo válido');
    if (password.length < 8) throw new Error('La contraseña debe tener al menos 8 caracteres');
    const salt = crypto.getRandomValues(new Uint32Array(2)).join('-');
    this.store.setItem(ACCOUNT_KEY, JSON.stringify({ email: email.toLowerCase(), salt, hash: await hashPassword(password, salt) }));
    return this.startSession(email.toLowerCase());
  }

  async signIn(email: string, password: string): Promise<AdminSession> {
    const raw = this.store.getItem(ACCOUNT_KEY);
    if (!raw) throw new Error('Primero crea la cuenta de administrador');
    const acc = JSON.parse(raw) as { email: string; salt: string; hash: string };
    if (acc.email !== email.trim().toLowerCase() || acc.hash !== (await hashPassword(password, acc.salt)))
      throw new Error('Correo o contraseña incorrectos');
    return this.startSession(acc.email);
  }

  private startSession(email: string): AdminSession {
    const s: AdminSession = { email, role: 'admin' };
    this.sessionStore.setItem(SESSION_KEY, JSON.stringify(s));
    return s;
  }

  signOut(): void {
    this.sessionStore.removeItem(SESSION_KEY);
  }

  session(): AdminSession | null {
    const raw = this.sessionStore.getItem(SESSION_KEY);
    return raw ? (JSON.parse(raw) as AdminSession) : null;
  }

  async load(): Promise<AdminData> {
    if (this.data) return this.data;
    const raw = this.store.getItem(DATA_KEY);
    this.data = raw ? (JSON.parse(raw) as AdminData) : seedData(this.now());
    this.data.catalogCollections ??= [];
    this.data.catalogItems ??= [];
    if (!raw) this.persist();
    return this.data;
  }

  private persist() {
    try {
      this.store.setItem(DATA_KEY, JSON.stringify(this.data));
    } catch {
      /* almacenamiento lleno o bloqueado: los cambios duran hasta cerrar la pestaña */
    }
  }

  async upsert<C extends Collection>(col: C, item: ItemOf<C>): Promise<void> {
    const d = await this.load();
    const list = d[col] as ItemOf<C>[];
    const i = list.findIndex((x) => x.id === item.id);
    if (i >= 0) list[i] = item;
    else list.unshift(item);
    this.persist();
  }

  async remove(col: Collection, id: string): Promise<void> {
    const d = await this.load();
    (d[col] as { id: string }[]) = (d[col] as { id: string }[]).filter((x) => x.id !== id);
    this.persist();
  }

  async saveSettings(s: Settings): Promise<void> {
    const d = await this.load();
    d.settings = s;
    this.persist();
  }

  /** Demo: el archivo se guarda dentro de los datos (en este navegador). */
  async uploadFile(_path: string, file: Blob): Promise<string> {
    return new Promise((resolve, reject) => {
      const r = new FileReader();
      r.onload = () => resolve(String(r.result));
      r.onerror = () => reject(new Error('No se pudo leer el archivo'));
      r.readAsDataURL(file);
    });
  }

  async removeFiles(): Promise<void> {}

  async reset(): Promise<void> {
    this.data = seedData(this.now());
    this.persist();
  }
}

// ---------------------------------------------------------------- conexión real (guardada en este navegador)
export interface RealConnection {
  url: string;
  anonKey: string;
}
const CONN_KEY = 'mfv3-admin-conexion';

/**
 * Conexión fijada al publicar la web (variables VITE_SUPABASE_URL y VITE_SUPABASE_ANON_KEY).
 * Si existe, el panel trabaja siempre con la base de datos real: no hay modo demostración ni se puede cambiar.
 */
export function builtInConnection(): RealConnection | null {
  const url = (import.meta.env.VITE_SUPABASE_URL as string | undefined)?.trim();
  const anonKey = (import.meta.env.VITE_SUPABASE_ANON_KEY as string | undefined)?.trim();
  return url && anonKey ? { url, anonKey } : null;
}

export function loadConnection(store: KeyValueStore = safeStore(() => localStorage)): RealConnection | null {
  const raw = store.getItem(CONN_KEY);
  return raw ? (JSON.parse(raw) as RealConnection) : null;
}

export function saveConnection(c: RealConnection | null, store: KeyValueStore = safeStore(() => localStorage)): void {
  if (c) store.setItem(CONN_KEY, JSON.stringify(c));
  else store.removeItem(CONN_KEY);
}
