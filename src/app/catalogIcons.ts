/**
 * Mete las piezas del catálogo (escudos, copas y banners de los eventos) en el muestrario de iconos:
 * cada colección es una categoría más, con candado en lo que aún no se ha conseguido.
 */
import { cuandoTexto, disponibleAhora, obtencionDe, requisitoTexto, visibleItems, type Catalog, type CatalogKind } from '../services/events';
import { setCatalogIcons, type CatalogIconInfo, type IconCategory, type IconKind } from '../services/icons';
import { assetUrl, registerAsset } from '../ui/components/assets';

/** «Halloween 2026 · Copa» → «Halloween»: una pestaña por evento, con todos sus años dentro. */
export function eventTabName(nombre: string): string {
  const base = nombre.split(/\s[·\-–]\s/)[0].trim() || nombre;
  return base.replace(/\s*\d{4}$/, '').trim() || base;
}
const yearOf = (nombre: string, creada: number) => Number(/(\d{4})/.exec(nombre)?.[1] ?? new Date(creada).getFullYear());
const slug = (s: string) => s.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase().replace(/[^a-z0-9]+/g, '-');

const KIND: Partial<Record<CatalogKind, IconKind>> = { copa: 'cup', escudo: 'logo', fondo: 'banner' };

export function applyCatalogIcons(catalog: Catalog): void {
  const cats = new Map<string, { cat: IconCategory; items: { id: string; info: CatalogIconInfo; year: number; orden: number }[] }>();
  for (const { item, col } of visibleItems(catalog)) {
    const kind = KIND[col.tipo];
    // Una pieza sin imagen no se puede enseñar en el muestrario.
    const src = item.archivo ? (/^(https?:|data:|\/)/.test(item.archivo) ? item.archivo : assetUrl(item.archivo)) : undefined;
    if (!kind || !src) continue;
    const id = `cat:${item.id}`;
    registerAsset(id, src);
    // Una pestaña por evento y tipo de torneo («Halloween»), con las piezas de todos sus años dentro.
    const format = col.torneo && col.torneo !== 'todos' ? col.torneo : undefined;
    const label = eventTabName(col.nombre);
    const catId = `cat-${slug(label)}${format ? `-${format}` : ''}`;
    const key = `${kind}|${catId}`;
    const g = cats.get(key) ?? { cat: { id: catId, kind, label, catalog: true, format }, items: [] };
    const d = col.disponibilidad;
    g.items.push({
      id,
      year: yearOf(col.nombre, col.creada),
      orden: col.orden * 1000 + item.orden,
      info: {
        itemId: item.id,
        requisito: requisitoTexto(obtencionDe(item, col)),
        cuando: cuandoTexto(d),
        usable: (now) => !d.soloUsoEnFechas || disponibleAhora(d, now),
      },
    });
    cats.set(key, g);
  }
  // Dentro de la pestaña, lo más reciente primero.
  setCatalogIcons([...cats.values()].map((g) => ({ ...g, items: g.items.sort((a, b) => b.year - a.year || a.orden - b.orden) })));
}
