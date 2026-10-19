/**
 * Sincronización de los datos de la mesa con la nube (tabla `mesa_datos`, una fila por jugador, partido, torneo y
 * las preferencias). Sirve igual para la app y para la pantalla de 7" (docs/NUBE.md).
 *
 * Cada vez:
 *   1. Se baja lo que ha cambiado en la nube desde la última vez (`actualizado` > última bajada) y se aplica aquí,
 *      salvo lo que se ha cambiado aquí desde la última sincronización (gana lo de aquí, que es lo último).
 *   2. Se sube lo que ha cambiado aquí (comparando con la huella de lo último que se sincronizó) y se marca como
 *      borrado en la nube lo que ya no está aquí.
 * La primera vez en un aparato (sin huellas): lo de la nube manda, salvo un jugador cambiado aquí más tarde.
 * Sin conexión no pasa nada: se vuelve a intentar en el siguiente cambio o al rato.
 */
import type { SupabaseConn } from '../profiles';
import type { Player, Preferences, StoredMatch, Tournament } from '../persistence';
import { CloudError } from './auth';

export type Kind = 'player' | 'match' | 'tournament' | 'prefs';

export interface CloudRow {
  tipo: Kind;
  id: string;
  datos: unknown;
  borrado: boolean;
  actualizado: string;
}

export interface CloudApi {
  /** Filas cambiadas después de `since` (todas si es null), de la más vieja a la más nueva. */
  pull(since: string | null): Promise<CloudRow[]>;
  push(rows: Omit<CloudRow, 'actualizado'>[]): Promise<void>;
}

type Http = typeof fetch;

/** API REST de Supabase (PostgREST) con la sesión del dueño. */
export function restApi(conn: SupabaseConn, token: () => Promise<string>, http: Http = (...a) => fetch(...a)): CloudApi {
  const headers = async (extra: Record<string, string> = {}) => ({
    apikey: conn.anonKey,
    Authorization: `Bearer ${await token()}`,
    'Content-Type': 'application/json',
    ...extra,
  });
  const fail = async (res: Response, what: string) => {
    const text = await res.text().catch(() => '');
    throw new CloudError(`${what}: ${res.status} ${text}`.trim(), String(res.status));
  };
  return {
    async pull(since) {
      const out: CloudRow[] = [];
      const PAGE = 500;
      for (let offset = 0; ; offset += PAGE) {
        const q = new URLSearchParams({ select: 'tipo,id,datos,borrado,actualizado', order: 'actualizado.asc,tipo.asc,id.asc', limit: String(PAGE), offset: String(offset) });
        if (since) q.set('actualizado', `gt.${since}`);
        let res: Response;
        try {
          res = await http(`${conn.url}/rest/v1/mesa_datos?${q}`, { headers: await headers() });
        } catch {
          throw new CloudError('Sin conexión con la nube', 'offline');
        }
        if (!res.ok) await fail(res, 'Bajar datos');
        const page = (await res.json()) as CloudRow[];
        out.push(...page);
        if (page.length < PAGE) return out;
      }
    },
    async push(rows) {
      for (let i = 0; i < rows.length; i += 50) {
        let res: Response;
        try {
          res = await http(`${conn.url}/rest/v1/mesa_datos?on_conflict=owner_id,tipo,id`, {
            method: 'POST',
            headers: await headers({ Prefer: 'resolution=merge-duplicates,return=minimal' }),
            body: JSON.stringify(rows.slice(i, i + 50)),
          });
        } catch {
          throw new CloudError('Sin conexión con la nube', 'offline');
        }
        if (!res.ok) await fail(res, 'Subir datos');
      }
    },
  };
}

// ---------------------------------------------------------------- huellas
/** JSON con las claves ordenadas: la nube (jsonb) cambia el orden y no debe parecer un cambio. */
export function stableStringify(v: unknown): string {
  if (v === null || typeof v !== 'object') return JSON.stringify(v) ?? 'null';
  if (Array.isArray(v)) return `[${v.map((x) => (x === undefined ? 'null' : stableStringify(x))).join(',')}]`;
  const o = v as Record<string, unknown>;
  return `{${Object.keys(o)
    .filter((k) => o[k] !== undefined)
    .sort()
    .map((k) => `${JSON.stringify(k)}:${stableStringify(o[k])}`)
    .join(',')}}`;
}

export function fingerprint(v: unknown): string {
  const s = stableStringify(v);
  let h1 = 0x811c9dc5,
    h2 = 0x01000193;
  for (let i = 0; i < s.length; i++) {
    const c = s.charCodeAt(i);
    h1 = Math.imul(h1 ^ c, 16777619);
    h2 = Math.imul(h2 ^ c, 2246822519);
  }
  return `${(h1 >>> 0).toString(16)}${(h2 >>> 0).toString(16)}:${s.length}`;
}

// ---------------------------------------------------------------- datos de la mesa
export interface MesaData {
  players: Player[];
  matches: StoredMatch[];
  tournaments: Tournament[];
  prefs: Preferences;
}

export interface SyncState {
  lastPull: string | null;
  /** Huella de lo último sincronizado de cada cosa («player:abc»). */
  hashes: Record<string, string>;
}

export const emptyState = (): SyncState => ({ lastPull: null, hashes: {} });

/** Lo de las preferencias que es de este aparato y no de la mesa (no se sube ni se pisa). */
const DEVICE_PREFS: (keyof Preferences)[] = ['hardware', 'brightness'];
function mesaPrefs(p: Preferences): Partial<Preferences> {
  const out: Record<string, unknown> = { ...p };
  for (const k of DEVICE_PREFS) delete out[k];
  return out as Partial<Preferences>;
}

type Item = { kind: Kind; id: string; data: unknown };
function items(d: MesaData): Map<string, Item> {
  const m = new Map<string, Item>();
  for (const p of d.players) m.set(`player:${p.id}`, { kind: 'player', id: p.id, data: p });
  for (const x of d.matches) m.set(`match:${x.id}`, { kind: 'match', id: x.id, data: x });
  for (const t of d.tournaments) m.set(`tournament:${t.id}`, { kind: 'tournament', id: t.id, data: t });
  m.set('prefs:mesa', { kind: 'prefs', id: 'mesa', data: mesaPrefs(d.prefs) });
  return m;
}

export interface SyncResult {
  /** Los datos con lo bajado de la nube (null si no ha cambiado nada aquí). */
  data: MesaData | null;
  state: SyncState;
  pulled: number;
  pushed: number;
}

export async function syncOnce(api: CloudApi, local: MesaData, prev: SyncState): Promise<SyncResult> {
  const state: SyncState = { lastPull: prev.lastPull, hashes: { ...prev.hashes } };
  const firstTime = Object.keys(prev.hashes).length === 0;
  const remote = await api.pull(prev.lastPull);
  const mine = items(local);
  // Lo cambiado aquí desde la última sincronización (no se pisa con lo de la nube).
  const dirty = new Set<string>();
  for (const [k, it] of mine) if (state.hashes[k] !== undefined && state.hashes[k] !== fingerprint(it.data)) dirty.add(k);
  // Y lo borrado aquí desde entonces (estaba sincronizado y ya no está).
  for (const k of Object.keys(state.hashes)) if (!mine.has(k)) dirty.add(k);

  const players = new Map(local.players.map((p) => [p.id, p]));
  const matches = new Map(local.matches.map((m) => [m.id, m]));
  const tournaments = new Map(local.tournaments.map((t) => [t.id, t]));
  let prefs = local.prefs;
  let changed = false;
  let pulled = 0;
  for (const row of remote) {
    if (!state.lastPull || row.actualizado > state.lastPull) state.lastPull = row.actualizado;
    const key = `${row.tipo}:${row.id}`;
    // Lo cambiado o borrado aquí no se pisa, salvo que en la nube lo hayan cambiado después (otra versión).
    if (dirty.has(key) && (row.borrado || fingerprint(row.datos) === prev.hashes[key] || mine.has(key))) continue;
    const here = mine.get(key);
    if (firstTime && here && !row.borrado && row.tipo === 'player') {
      // Primera vez: un jugador cambiado aquí después que en la nube se queda como está aquí.
      const a = (here.data as Player).updatedAt ?? 0;
      const b = (row.datos as Player)?.updatedAt ?? 0;
      if (a > b) continue;
    }
    if (row.borrado) {
      delete state.hashes[key];
      if (!here) continue;
    } else {
      state.hashes[key] = fingerprint(row.datos);
      if (here && fingerprint(here.data) === state.hashes[key]) continue;  // ya era igual
    }
    pulled++;
    changed = true;
    const del = row.borrado;
    if (row.tipo === 'player') del ? players.delete(row.id) : players.set(row.id, row.datos as Player);
    else if (row.tipo === 'match') del ? matches.delete(row.id) : matches.set(row.id, row.datos as StoredMatch);
    else if (row.tipo === 'tournament') del ? tournaments.delete(row.id) : tournaments.set(row.id, row.datos as Tournament);
    else if (row.tipo === 'prefs' && !del) {
      const keep: Record<string, unknown> = {};
      for (const k of DEVICE_PREFS) keep[k] = (local.prefs as unknown as Record<string, unknown>)[k];
      prefs = { ...local.prefs, ...(row.datos as Partial<Preferences>), ...keep } as Preferences;
    }
  }
  const next: MesaData = { players: [...players.values()], matches: [...matches.values()], tournaments: [...tournaments.values()], prefs };

  // Subir lo cambiado aquí y marcar como borrado lo que ya no está.
  const after = items(next);
  const up: Omit<CloudRow, 'actualizado'>[] = [];
  const newHashes: Record<string, string> = {};
  for (const [k, it] of after) {
    const h = fingerprint(it.data);
    if (state.hashes[k] !== h) {
      up.push({ tipo: it.kind, id: it.id, datos: it.data, borrado: false });
      newHashes[k] = h;
    }
  }
  const gone: string[] = [];
  for (const k of Object.keys(state.hashes)) {
    if (after.has(k)) continue;
    const [tipo, ...rest] = k.split(':');
    up.push({ tipo: tipo as Kind, id: rest.join(':'), datos: {}, borrado: true });
    gone.push(k);
  }
  if (up.length) await api.push(up);
  Object.assign(state.hashes, newHashes);
  for (const k of gone) delete state.hashes[k];
  return { data: changed ? next : null, state, pulled, pushed: up.length };
}

// ---------------------------------------------------------------- estado guardado (por cuenta)
export const stateKey = (userId: string) => `mfv3:nube:estado:${userId}`;

export function readState(store: Pick<Storage, 'getItem'>, userId: string): SyncState {
  try {
    const raw = store.getItem(stateKey(userId));
    const s = raw ? (JSON.parse(raw) as SyncState) : null;
    return s && typeof s.hashes === 'object' ? s : emptyState();
  } catch {
    return emptyState();
  }
}

export function writeState(store: Pick<Storage, 'setItem'>, userId: string, s: SyncState): void {
  try {
    store.setItem(stateKey(userId), JSON.stringify(s));
  } catch {
    /* sin almacenamiento: la próxima vez se compara todo otra vez */
  }
}
