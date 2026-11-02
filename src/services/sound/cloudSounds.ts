/**
 * Sonidos de la SD de la pantalla, desde la nube (almacén privado «sonidos» de Supabase; los sube
 * scripts/subir-sonidos-nube.py, guía en docs/SONIDOS_NUBE.md). Se bajan una vez a este aparato (IndexedDB) y
 * desde ahí suenan al instante y sin internet. Cada sonido se conoce por el mismo código de 8 cifras que en la SD
 * (FNV-1a de su id: «handicap/F01», «arbitro/pitido_final», «fc26/es/gol/…»).
 *
 * Para no llenar la memoria: solo se descodifican los que se van a usar (las voces cortas al cargar; el resto
 * al pedirlos, con un tope de los últimos usados).
 */
import { soundCode } from '../../inputs/hardware/boardSounds';
import { sound, type PlayOptions } from './sound';

export const CLOUD_BUCKET = 'sonidos';
export const CLOUD_INDEX = 'indice.json';
export const PITIDO_ID = 'arbitro/pitido_final';

export interface CloudIndexEntry {
  bytes: number;
  ms: number;
  mp3Sha256: string;
}
export interface CloudIndex {
  version: number;
  updatedAt: number;
  files: Record<string, CloudIndexEntry>;
}

export interface CloudClip {
  code: string;
  sha: string;
  data: ArrayBuffer;
}

/** Dónde se guardan en el aparato. */
export interface CloudClipRepository {
  codes(): Promise<Map<string, string>>; // código → huella
  get(code: string): Promise<ArrayBuffer | null>;
  put(clip: CloudClip): Promise<void>;
  remove(code: string): Promise<void>;
}

export function memoryClipRepository(): CloudClipRepository {
  const items = new Map<string, CloudClip>();
  return {
    async codes() {
      return new Map([...items.values()].map((c) => [c.code, c.sha]));
    },
    async get(code) {
      return items.get(code)?.data ?? null;
    },
    async put(c) {
      items.set(c.code, c);
    },
    async remove(code) {
      items.delete(code);
    },
  };
}

const DB_NAME = 'mfv3-sonidos-nube';
const STORE = 'clips';

function indexedDbClipRepository(): CloudClipRepository {
  let db: Promise<IDBDatabase> | null = null;
  const open = () =>
    new Promise<IDBDatabase>((resolve, reject) => {
      const req = indexedDB.open(DB_NAME, 1);
      req.onupgradeneeded = () => req.result.createObjectStore(STORE, { keyPath: 'code' });
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => reject(req.error);
    });
  const run = async <T>(mode: IDBTransactionMode, fn: (s: IDBObjectStore) => IDBRequest<T> | void): Promise<T | undefined> => {
    db ??= open();
    const tx = (await db).transaction(STORE, mode);
    const req = fn(tx.objectStore(STORE));
    return new Promise((resolve, reject) => {
      tx.oncomplete = () => resolve(req ? req.result : undefined);
      tx.onerror = () => reject(tx.error);
      tx.onabort = () => reject(tx.error);
    });
  };
  return {
    async codes() {
      const out = new Map<string, string>();
      await run('readonly', (s) => {
        const cur = s.openCursor();
        cur.onsuccess = () => {
          const c = cur.result;
          if (!c) return;
          const v = c.value as CloudClip;
          out.set(v.code, v.sha);
          c.continue();
        };
      });
      return out;
    },
    async get(code) {
      const v = (await run('readonly', (s) => s.get(code))) as CloudClip | undefined;
      return v?.data ?? null;
    },
    async put(c) {
      await run('readwrite', (s) => s.put(c));
    },
    async remove(code) {
      await run('readwrite', (s) => s.delete(code));
    },
  };
}

export function browserClipRepository(): CloudClipRepository {
  try {
    if (typeof indexedDB !== 'undefined') return indexedDbClipRepository();
  } catch {
    /* sin IndexedDB */
  }
  return memoryClipRepository();
}

export interface SyncResult {
  total: number;
  downloaded: number;
  removed: number;
  failed: number;
}

/**
 * Pone este aparato al día con la nube: baja lo nuevo o cambiado (comprobando el tamaño), borra lo que ya no está.
 * Si se corta, lo bajado se queda y la próxima vez sigue por donde iba.
 */
export async function syncCloudSounds(opts: {
  url: string;
  anonKey: string;
  token: string;
  repo: CloudClipRepository;
  http?: typeof fetch;
  onProgress?: (done: number, todo: number) => void;
  parallel?: number;
}): Promise<SyncResult> {
  const http = opts.http ?? fetch;
  const base = `${opts.url.replace(/\/$/, '')}/storage/v1/object/authenticated/${CLOUD_BUCKET}`;
  const headers = { Authorization: `Bearer ${opts.token}`, apikey: opts.anonKey };
  const res = await http(`${base}/${CLOUD_INDEX}`, { headers, cache: 'no-store' });
  if (res.status === 400 || res.status === 404) return { total: 0, downloaded: 0, removed: 0, failed: 0 }; // aún no se ha subido nada
  if (!res.ok) throw new Error(`La nube no deja ver los sonidos (${res.status})`);
  const index = (await res.json()) as CloudIndex;
  const files = index.files ?? {};
  const have = await opts.repo.codes();
  let removed = 0;
  for (const code of have.keys()) {
    if (!files[code]) {
      await opts.repo.remove(code);
      removed += 1;
    }
  }
  const todo = Object.entries(files).filter(([code, f]) => have.get(code) !== f.mp3Sha256);
  let done = 0;
  let failed = 0;
  const queue = [...todo];
  const worker = async () => {
    for (let item = queue.shift(); item; item = queue.shift()) {
      const [code, f] = item;
      try {
        const r = await http(`${base}/${code}.mp3`, { headers });
        const data = r.ok ? await r.arrayBuffer() : null;
        if (data && data.byteLength === f.bytes) await opts.repo.put({ code, sha: f.mp3Sha256, data });
        else failed += 1;
      } catch {
        failed += 1;
      }
      done += 1;
      opts.onProgress?.(done, todo.length);
    }
  };
  await Promise.all(Array.from({ length: Math.max(1, opts.parallel ?? 4) }, worker));
  return { total: Object.keys(files).length, downloaded: todo.length - failed, removed, failed };
}

interface Decoded {
  samples: Float32Array;
  rate: number;
}

/** Descodifica un MP3 a 16 kHz mono (la mitad de memoria que a la frecuencia del aparato y suena igual). */
async function decodeMp3(data: ArrayBuffer): Promise<Decoded | null> {
  try {
    const Off =
      (globalThis as { OfflineAudioContext?: typeof OfflineAudioContext }).OfflineAudioContext ??
      (globalThis as unknown as { webkitOfflineAudioContext?: typeof OfflineAudioContext }).webkitOfflineAudioContext;
    if (!Off) return null;
    const ctx = new Off(1, 1, 16000);
    const buf = await ctx.decodeAudioData(data.slice(0));
    return { samples: buf.getChannelData(0), rate: buf.sampleRate };
  } catch {
    return null;
  }
}

/** Sonidos de la nube que hay en este aparato, listos para sonar. */
export class CloudVoices {
  private codes = new Set<string>();
  private ready = new Map<string, Decoded>();
  private keep = new Set<string>();
  private recent: string[] = [];
  repo: CloudClipRepository | null = null;
  decode: (data: ArrayBuffer) => Promise<Decoded | null> = decodeMp3;
  /** Cambia al cargar o actualizar (para la pantalla de Ajustes). */
  version = 0;
  private listeners = new Set<() => void>();

  get count(): number {
    return this.codes.size;
  }

  subscribe(fn: () => void): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  /** Lee qué hay en el aparato y deja preparadas las voces cortas (las de los hándicaps, nombres y el pitido). */
  async load(repo: CloudClipRepository, preload: string[] = []): Promise<void> {
    this.repo = repo;
    this.codes = new Set((await repo.codes()).keys());
    this.ready = new Map([...this.ready].filter(([c]) => this.codes.has(c)));
    this.keep = new Set(preload.filter((c) => this.codes.has(c)));
    for (const c of this.keep) await this.prepare(c);
    this.version += 1;
    this.listeners.forEach((fn) => fn());
  }

  has(code: string): boolean {
    return this.codes.has(code);
  }

  hasId(id: string): boolean {
    return this.codes.has(soundCode(id));
  }

  /** Duración (ms) si ya está preparado; 0 si no. */
  durationMs(code: string): number {
    const d = this.ready.get(code);
    return d ? Math.round((d.samples.length / d.rate) * 1000) : 0;
  }

  async prepare(code: string): Promise<Decoded | null> {
    const have = this.ready.get(code);
    if (have) return have;
    if (!this.repo || !this.codes.has(code)) return null;
    const data = await this.repo.get(code);
    const d = data ? await this.decode(data) : null;
    if (!d) return null;
    this.ready.set(code, d);
    if (!this.keep.has(code)) {
      this.recent = [...this.recent.filter((c) => c !== code), code];
      while (this.recent.length > 24) this.ready.delete(this.recent.shift()!);
    }
    return d;
  }

  /** Suena ya si está preparado (sin esperar). false si no está. */
  playNow(code: string, opts: PlayOptions = {}): boolean {
    const d = this.ready.get(code);
    if (!d || !sound.audible) return false;
    sound.playClip(d.samples, d.rate, opts);
    return true;
  }

  /**
   * Dice una serie de voces seguidas: la primera corta lo que suene (salvo `queue`) y las demás van detrás.
   * false si no hay ninguna (para usar la voz del navegador en su lugar).
   */
  say(codes: string[], opts: PlayOptions = {}): boolean {
    const list = codes.filter((c) => c && this.codes.has(c));
    if (!list.length || !sound.audible) return false;
    if (list.every((c) => this.ready.has(c))) {
      list.forEach((c, i) => this.playNow(c, { queue: i > 0 || !!opts.queue }));
      return true;
    }
    void Promise.all(list.map((c) => this.prepare(c))).then(() =>
      list.forEach((c, i) => this.playNow(c, { queue: i > 0 || !!opts.queue })),
    );
    return true;
  }
}

export const cloudVoices = new CloudVoices();
