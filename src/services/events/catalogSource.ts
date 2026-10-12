/**
 * De dónde saca la app el catálogo de personalización:
 *   1. El que trae la propia app (BUNDLED_CATALOG), para que funcione sin internet ni base de datos.
 *   2. El publicado en el panel de administración (Supabase, solo lectura y sin cuenta), cuando hay conexión.
 *      Se guarda una copia en este aparato y, si un día no hay internet, se usa la última.
 * Lo del panel manda sobre lo que trae la app si comparten id.
 */
import type { Catalog } from './rewards';
import type { CatalogCollection, CatalogItem } from './model';

const dia = (y: number, m: number, d: number, fin = false) => new Date(y, m - 1, d, fin ? 23 : 0, fin ? 59 : 0, fin ? 59 : 0).getTime();

/** Colección de un evento que viene dentro de la app: una pieza, para un tipo de torneo y solo esas fechas. */
function piezaDeEvento(
  id: string,
  tipo: CatalogCollection['tipo'],
  nombre: string,
  pieza: string,
  archivo: string,
  obtencion: CatalogCollection['obtencion'],
  desde: number,
  hasta: number,
  orden: number,
  torneo: CatalogCollection['torneo'],
): { col: CatalogCollection; item: CatalogItem } {
  return {
    col: {
      id,
      tipo,
      nombre,
      descripcion: '',
      rareza: 'epica',
      estado: 'publicada',
      ambito: 'coleccion',
      obtencion,
      disponibilidad: { modo: 'fechas', desde, hasta },
      torneo,
      orden,
      creada: desde,
    },
    item: { id: `${id}-1`, coleccionId: id, nombre: pieza, archivo, orden: 0, activo: true },
  };
}

const H26 = [dia(2026, 10, 31), dia(2026, 10, 31, true)] as const;
const entrar = { tipo: 'entrar' } as const;
const jugar = { tipo: 'jugarPartido', modo: 'cualquiera' } as const;
const ganar = { tipo: 'ganarPartido', modo: 'cualquiera' } as const;

/** Halloween 2026: escudo al entrar, banner jugando y copa ganando un partido el 31 oct; un juego para cada tipo de torneo (liga, copa y pool). */
const HALLOWEEN_2026 = [
  piezaDeEvento('app-halloween-2026-escudo', 'escudo', 'Halloween 2026 · Escudo de liga', 'Escudo fantasma 2026', 'evento-halloween-2026-liga-escudo', entrar, ...H26, 0, 'league'),
  piezaDeEvento('app-halloween-2026-banner', 'fondo', 'Halloween 2026 · Banner de liga', 'Banner noche de brujas 2026', 'evento-halloween-2026-liga-banner', jugar, ...H26, 1, 'league'),
  piezaDeEvento('app-halloween-2026-copa', 'copa', 'Halloween 2026 · Copa de liga', 'Copa calabaza 2026', 'evento-halloween-2026-liga-copa', ganar, ...H26, 2, 'league'),
  piezaDeEvento('app-halloween-2026-copa-escudo', 'escudo', 'Halloween 2026 · Escudo de copa', 'Escudo calabaza 2026', 'evento-halloween-2026-copa-escudo', entrar, ...H26, 3, 'bracket'),
  piezaDeEvento('app-halloween-2026-copa-banner', 'fondo', 'Halloween 2026 · Banner de copa', 'Banner calabazas 2026', 'evento-halloween-2026-copa-banner', jugar, ...H26, 4, 'bracket'),
  piezaDeEvento('app-halloween-2026-copa-copa', 'copa', 'Halloween 2026 · Copa de copa', 'Copa fantasma 2026', 'evento-halloween-2026-copa-copa', ganar, ...H26, 5, 'bracket'),
  piezaDeEvento('app-halloween-2026-pool-escudo', 'escudo', 'Halloween 2026 · Escudo de pool', 'Escudo futbolín embrujado 2026', 'evento-halloween-2026-pool-escudo', entrar, ...H26, 6, 'pool'),
  piezaDeEvento('app-halloween-2026-pool-banner', 'fondo', 'Halloween 2026 · Banner de pool', 'Banner barras fantasma 2026', 'evento-halloween-2026-pool-banner', jugar, ...H26, 7, 'pool'),
  piezaDeEvento('app-halloween-2026-pool-copa', 'copa', 'Halloween 2026 · Copa de pool', 'Copa calabaza negra 2026', 'evento-halloween-2026-pool-copa', ganar, ...H26, 8, 'pool'),
];

/** Eventos que vienen dentro de la app (sus imágenes están en src/assets/images, `evento-…`). */
export const BUNDLED_CATALOG: Catalog = {
  collections: HALLOWEEN_2026.map((x) => x.col),
  items: HALLOWEEN_2026.map((x) => x.item),
};

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
