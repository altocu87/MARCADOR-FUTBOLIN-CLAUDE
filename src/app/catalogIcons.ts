/**
 * Mete las piezas del catálogo (escudos, copas y banners de los eventos) en el muestrario de iconos:
 * cada colección es una categoría más, con candado en lo que aún no se ha conseguido.
 */
import { cuandoTexto, disponibleAhora, obtencionDe, requisitoTexto, visibleItems, type Catalog, type CatalogKind } from '../services/events';
import { setCatalogIcons, type CatalogIconInfo, type IconCategory, type IconKind } from '../services/icons';
import { assetUrl, registerAsset } from '../ui/components/assets';

const KIND: Partial<Record<CatalogKind, IconKind>> = { copa: 'cup', escudo: 'logo', fondo: 'banner' };

export function applyCatalogIcons(catalog: Catalog): void {
  const cats = new Map<string, { cat: IconCategory; items: { id: string; info: CatalogIconInfo }[] }>();
  for (const { item, col } of visibleItems(catalog)) {
    const kind = KIND[col.tipo];
    // Una pieza sin imagen no se puede enseñar en el muestrario.
    const src = item.archivo ? (/^(https?:|data:|\/)/.test(item.archivo) ? item.archivo : assetUrl(item.archivo)) : undefined;
    if (!kind || !src) continue;
    const id = `cat:${item.id}`;
    registerAsset(id, src);
    const catId = `cat-${col.id}`;
    const g = cats.get(catId) ?? {
      cat: { id: catId, kind, label: col.nombre.split(/\s[·\-–]\s/)[0].trim() || col.nombre, catalog: true, format: col.torneo && col.torneo !== 'todos' ? col.torneo : undefined },
      items: [],
    };
    const d = col.disponibilidad;
    g.items.push({
      id,
      info: {
        itemId: item.id,
        requisito: requisitoTexto(obtencionDe(item, col)),
        cuando: cuandoTexto(d),
        usable: (now) => !d.soloUsoEnFechas || disponibleAhora(d, now),
      },
    });
    cats.set(catId, g);
  }
  setCatalogIcons([...cats.values()]);
}
