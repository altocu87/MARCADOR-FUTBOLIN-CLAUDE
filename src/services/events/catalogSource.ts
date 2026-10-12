/**
 * De dónde saca la app el catálogo de personalización:
 *   1. El que trae la propia app (BUNDLED_CATALOG), para que funcione sin internet ni base de datos.
 *   2. El publicado en el panel de administración (Supabase, solo lectura y sin cuenta), cuando hay conexión.
 *      Se guarda una copia en este aparato y, si un día no hay internet, se usa la última.
 * Lo del panel manda sobre lo que trae la app si comparten id.
 */
import type { Catalog } from './rewards';
import type { CatalogCollection, CatalogItem } from './model';

/** Eventos que vienen dentro de la app (sus imágenes están en src/assets/images). */
export const BUNDLED_CATALOG: Catalog = { collections: [], items: [] };

const CACHE_KEY = 'mfv3:catalogo';

const toCamel = (k: string) => k.replace(/_([a-z])/g, (_, c: string) => c.toUpperCase());
const fromDb = <T,>(o: Record<string, unknown>): T =>
  Object.fromEntries(Object.entries(o).filter(([, v]) => v !== null).map(([k, v]) => [toCamel(k), v])) as T;

export function readCachedCatalog(store: Pick<Storage, 'getItem'>): Catalog | null {
  try {
    const raw = store.getItem(CACHE_KEY);
    if (!raw) return null;
    const c = JSON.parse(raw) as Catalog;
    return Array.isArray(c.collections) && Array.isArray(c.items) ? c : null;
  } catch {
    return null;
  }
}

export function writeCachedCatalog(store: Pick<Storage, 'setItem'>, c: Catalog): void {
  try {
    store.setItem(CACHE_KEY, JSON.stringify(c));
  } catch {
    // Sin sitio: se volverá a descargar.
  }
}

/** Descarga lo publicado (y lo retirado, para quien ya lo tenga). Lanza si no hay conexión o falla. */
export async function fetchRemoteCatalog(url: string, anonKey: string, http: typeof fetch = (...a) => fetch(...a)): Promise<Catalog> {
  const base = url.replace(/\/+$/, '');
  const get = async (path: string) => {
    const r = await http(`${base}/rest/v1/${path}`, { headers: { apikey: anonKey, Authorization: `Bearer ${anonKey}` } });
    if (!r.ok) throw new Error(`Catálogo: ${r.status}`);
    return (await r.json()) as Record<string, unknown>[];
  };
  const [cols, items] = await Promise.all([
    get('catalog_collections?select=*&estado=neq.borrador&order=orden.asc'),
    get('catalog_items?select=*&activo=eq.true&order=orden.asc'),
  ]);
  return { collections: cols.map((c) => fromDb<CatalogCollection>(c)), items: items.map((i) => fromDb<CatalogItem>(i)) };
}

/** Lo del panel sobre lo de la app (por id). */
export function mergeCatalogs(base: Catalog, over: Catalog | null): Catalog {
  if (!over) return base;
  const cols = new Map(base.collections.map((c) => [c.id, c]));
  over.collections.forEach((c) => cols.set(c.id, c));
  const items = new Map(base.items.map((i) => [i.id, i]));
  over.items.forEach((i) => items.set(i.id, i));
  return { collections: [...cols.values()], items: [...items.values()] };
}
