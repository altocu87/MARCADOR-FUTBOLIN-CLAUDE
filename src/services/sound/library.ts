/**
 * Biblioteca de sonidos propios de la mesa. Los audios pesan mucho más que los datos de siempre, así que van aparte,
 * en IndexedDB del aparato (si no hay, en memoria: duran hasta cerrar la app). Son comunes al modo prueba y al real.
 */
import type { StoredSound } from '../persistence';

export interface SoundRepository {
  list(): Promise<StoredSound[]>;
  save(sound: StoredSound): Promise<void>;
  remove(id: string): Promise<void>;
  /** Sustituye todos (importar copia con «Reemplazar», borrar todo). */
  saveAll(sounds: StoredSound[]): Promise<void>;
}

export function memorySoundRepository(initial: StoredSound[] = []): SoundRepository {
  let items = [...initial];
  return {
    async list() {
      return [...items].sort((a, b) => a.createdAt - b.createdAt);
    },
    async save(s) {
      items = [...items.filter((x) => x.id !== s.id), s];
    },
    async remove(id) {
      items = items.filter((x) => x.id !== id);
    },
    async saveAll(list) {
      items = [...list];
    },
  };
}

const DB_NAME = 'mfv3-sonidos';
const STORE = 'sonidos';

function openDb(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open(DB_NAME, 1);
    req.onupgradeneeded = () => req.result.createObjectStore(STORE, { keyPath: 'id' });
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}

function indexedDbSoundRepository(): SoundRepository {
  let db: Promise<IDBDatabase> | null = null;
  const run = async <T>(mode: IDBTransactionMode, fn: (store: IDBObjectStore) => IDBRequest<T> | void): Promise<T | undefined> => {
    db ??= openDb();
    const tx = (await db).transaction(STORE, mode);
    const req = fn(tx.objectStore(STORE));
    return new Promise((resolve, reject) => {
      tx.oncomplete = () => resolve(req ? req.result : undefined);
      tx.onerror = () => reject(tx.error);
      tx.onabort = () => reject(tx.error);
    });
  };
  return {
    async list() {
      const all = ((await run('readonly', (s) => s.getAll())) ?? []) as StoredSound[];
      return all.sort((a, b) => a.createdAt - b.createdAt);
    },
    async save(sound) {
      await run('readwrite', (s) => s.put(sound));
    },
    async remove(id) {
      await run('readwrite', (s) => s.delete(id));
    },
    async saveAll(list) {
      await run('readwrite', (s) => {
        s.clear();
        for (const x of list) s.put(x);
      });
    },
  };
}

/** Almacén de este aparato: IndexedDB si existe; si no, memoria. */
export function browserSoundRepository(): SoundRepository {
  try {
    if (typeof indexedDB !== 'undefined') return indexedDbSoundRepository();
  } catch {
    /* sin IndexedDB */
  }
  return memorySoundRepository();
}
