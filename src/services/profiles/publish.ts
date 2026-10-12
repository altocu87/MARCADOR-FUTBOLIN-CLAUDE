/**
 * Publicar el perfil de un jugador en internet (Supabase) y leerlo desde el móvil.
 * Cada jugador tiene un código (va en su QR y en el enlace) y una clave secreta que solo conoce este aparato:
 * con el código se ve el perfil; para cambiarlo hace falta la clave.
 */
import type { ProfileSnapshot } from './snapshot';

export interface ProfileCredentials {
  token: string;
  clave: string;
  /** Huella de lo último subido (para no repetir). */
  hash?: string;
}

const CREDS_KEY = 'mfv3:perfiles';
const ALPHABET = 'abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789';

export function randomCode(length: number, rnd: (n: number) => Uint8Array = (n) => crypto.getRandomValues(new Uint8Array(n))): string {
  const bytes = rnd(length);
  let out = '';
  for (let k = 0; k < length; k++) out += ALPHABET[bytes[k] % ALPHABET.length];
  return out;
}

type Store = Pick<Storage, 'getItem' | 'setItem'>;

export function readCredentials(store: Store): Record<string, ProfileCredentials> {
  try {
    const raw = store.getItem(CREDS_KEY);
    return raw ? (JSON.parse(raw) as Record<string, ProfileCredentials>) : {};
  } catch {
    return {};
  }
}

export function writeCredentials(store: Store, all: Record<string, ProfileCredentials>): void {
  try {
    store.setItem(CREDS_KEY, JSON.stringify(all));
  } catch {
    /* sin almacenamiento */
  }
}

/** Código y clave del jugador; se crean la primera vez. */
export function credentialsFor(store: Store, playerId: string): ProfileCredentials {
  const all = readCredentials(store);
  if (!all[playerId]) {
    all[playerId] = { token: randomCode(22), clave: randomCode(32) };
    writeCredentials(store, all);
  }
  return all[playerId];
}

/** Web publicada: el perfil vive en su carpeta «perfil/». */
export const WEB_URL = 'https://altocu87.github.io/MARCADOR-FUTBOLIN-CLAUDE/';

export function profileUrl(token: string, base = WEB_URL): string {
  return `${base}perfil/#${token}`;
}

/** Código del perfil a partir de la dirección de la página («…/perfil/#CODIGO»). */
export function tokenFromLocation(hash: string, search = ''): string | null {
  const t = hash.replace(/^#\/?/, '').trim() || new URLSearchParams(search).get('t') || '';
  return /^[A-Za-z0-9]{16,64}$/.test(t) ? t : null;
}

export interface SupabaseConn {
  url: string;
  anonKey: string;
}

/** Conexión con Supabase fijada al publicar (las mismas variables que el panel). */
export function supabaseConn(env: Record<string, unknown> = import.meta.env): SupabaseConn | null {
  const url = (env.VITE_SUPABASE_URL as string | undefined)?.trim();
  const anonKey = (env.VITE_SUPABASE_ANON_KEY as string | undefined)?.trim();
  return url && anonKey ? { url: url.replace(/\/$/, ''), anonKey } : null;
}

async function rpc(conn: SupabaseConn, fn: string, body: object, http: typeof fetch): Promise<Response> {
  const res = await http(`${conn.url}/rest/v1/rpc/${fn}`, {
    method: 'POST',
    headers: { apikey: conn.anonKey, Authorization: `Bearer ${conn.anonKey}`, 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`${fn}: ${res.status} ${await res.text().catch(() => '')}`.trim());
  return res;
}

const defaultHttp: typeof fetch = (...a) => fetch(...a);

export async function publishProfile(conn: SupabaseConn, c: ProfileCredentials, snap: ProfileSnapshot, http = defaultHttp): Promise<void> {
  await rpc(conn, 'publicar_perfil_jugador', { p_token: c.token, p_clave: c.clave, p_datos: snap }, http);
}

export async function deleteProfile(conn: SupabaseConn, c: ProfileCredentials, http = defaultHttp): Promise<void> {
  await rpc(conn, 'borrar_perfil_jugador', { p_token: c.token, p_clave: c.clave }, http);
}

/** Lo que ve el móvil: null si ese código no tiene perfil (todavía). */
export async function fetchProfile(conn: SupabaseConn, token: string, http = defaultHttp): Promise<ProfileSnapshot | null> {
  const res = await rpc(conn, 'perfil_jugador', { p_token: token }, http);
  const text = await res.text();
  if (!text || text === 'null') return null;
  return JSON.parse(text) as ProfileSnapshot;
}
