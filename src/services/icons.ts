/**
 * Muestrario de logos y copas por categorías (Genéricos, Retro 80…). Cada categoría puede estar
 * bloqueada hasta que alguno de los jugadores llegue a un nivel, un rango, un logro o un número de títulos.
 * Las imágenes se llaman `escudo-<categoría>-NN` y `copa-<categoría>-NN` (sin categoría = genéricos).
 */
import type { TournamentFormat } from './persistence';
import type { PlayerProgress } from './progression';
import { ACHIEVEMENTS, CATEGORIES, categoryFor } from './progression';

/** Escudo, copa o banner (franja ancha de fondo del cartel del torneo). */
export type IconKind = 'logo' | 'cup' | 'banner';

export type Unlock =
  | { level: number }
  /** Rango máximo alcanzado (id: bronze, silver, gold…). */
  | { rank: string }
  | { achievement: string }
  /** Torneos ganados. */
  | { titles: number };

export interface IconCategory {
  id: string;
  kind: IconKind;
  label: string;
  unlock?: Unlock;
  /**
   * Exclusiva de un tipo de torneo: solo se ofrece (y es la de por defecto) en ese tipo, y nunca en
   * los otros ni en los equipos. Un tipo sin categorías propias usa las comunes.
   */
  format?: TournamentFormat;
  /** Colección del catálogo descargado (eventos): sus piezas van con candado hasta conseguirlas. */
  catalog?: boolean;
}

/** Datos de una pieza del catálogo, para el candado y la explicación de cómo se consigue. */
export interface CatalogIconInfo {
  /** Id de la pieza en el catálogo (con él se mira quién la tiene). */
  itemId: string;
  /** «ganando un partido de liga», «entrando en la app»… */
  requisito: string;
  /** «31 oct», «del 25 oct al 2 nov»… */
  cuando: string;
  /** Además, solo se puede usar en sus fechas: fuera de ellas no se ofrece. */
  usable: (now: number) => boolean;
}

let catalogCats: { cat: IconCategory; items: string[] }[] = [];
const catalogInfo = new Map<string, CatalogIconInfo>();

/** Pone las colecciones del catálogo (eventos) como categorías del muestrario. */
export function setCatalogIcons(list: { cat: IconCategory; items: { id: string; info: CatalogIconInfo }[] }[]): void {
  catalogCats = list.map((x) => ({ cat: x.cat, items: x.items.map((i) => i.id) }));
  catalogInfo.clear();
  for (const x of list) for (const i of x.items) catalogInfo.set(i.id, i.info);
}

/** Categorías del catálogo (primero) y las fijas de la app. */
export function allIconCategories(): IconCategory[] {
  return [...catalogCats.map((x) => x.cat), ...ICON_CATEGORIES];
}

/** Piezas de una categoría del catálogo (undefined si no es del catálogo). */
export function catalogCategoryItems(kind: IconKind, id: string): string[] | undefined {
  return catalogCats.find((x) => x.cat.kind === kind && x.cat.id === id)?.items;
}

export function catalogIconInfo(id: string): CatalogIconInfo | undefined {
  return catalogInfo.get(id);
}

/** Para añadir una categoría: una línea aquí y sus imágenes con el prefijo `escudo-<id>-` o `copa-<id>-`. */
export const ICON_CATEGORIES: IconCategory[] = [
  { id: 'generico', kind: 'logo', label: 'Genéricos' },
  { id: 'retro80', kind: 'logo', label: 'Retro 80' },
  { id: 'liga', kind: 'logo', label: 'Liga', format: 'league' },
  { id: 'liga', kind: 'banner', label: 'Liga', format: 'league' },
  { id: 'generico', kind: 'cup', label: 'Genéricas' },
  { id: 'retro90', kind: 'cup', label: 'Retro 90' },
];

export const ICON_PREFIX: Record<IconKind, string> = { logo: 'escudo-', cup: 'copa-', banner: 'banner-' };

/** Categoría de una imagen por su nombre: `escudo-retro80-03` → retro80; `escudo-07` → generico. */
export function iconCategory(name: string, kind: IconKind): string {
  const rest = name.slice(ICON_PREFIX[kind].length);
  const m = /^([a-z][a-z0-9]*)-/.exec(rest);
  return m && ICON_CATEGORIES.some((c) => c.kind === kind && c.id === m[1]) ? m[1] : 'generico';
}

/** ¿Lo cumple alguno de estos jugadores? Sin condición, siempre. */
export function isUnlocked(unlock: Unlock | undefined, players: (PlayerProgress | undefined)[]): boolean {
  if (!unlock) return true;
  const ps = players.filter((p): p is PlayerProgress => !!p);
  if ('level' in unlock) return ps.some((p) => p.level >= unlock.level);
  if ('titles' in unlock) return ps.some((p) => p.tournamentsWon >= unlock.titles);
  if ('achievement' in unlock) return ps.some((p) => p.achievements.some((a) => a.id === unlock.achievement));
  const need = CATEGORIES.findIndex((c) => c.id === unlock.rank);
  return ps.some((p) => CATEGORIES.findIndex((c) => c.id === categoryFor(p.maxElo).id) >= need);
}

/** «Nivel 10», «Rango Oro», «Logro: Hat-trick», «Ganar 3 torneos». */
export function unlockText(unlock: Unlock): string {
  if ('level' in unlock) return `Nivel ${unlock.level}`;
  if ('titles' in unlock) return `Ganar ${unlock.titles} torneo${unlock.titles > 1 ? 's' : ''}`;
  if ('achievement' in unlock) return `Logro «${ACHIEVEMENTS.find((a) => a.id === unlock.achievement)?.name ?? unlock.achievement}»`;
  return `Rango ${CATEGORIES.find((c) => c.id === unlock.rank)?.name ?? unlock.rank}`;
}
