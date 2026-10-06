/**
 * Muestrario de logos y copas por categorías (Genéricos, Retro 80…). Cada categoría puede estar
 * bloqueada hasta que alguno de los jugadores llegue a un nivel, un rango, un logro o un número de títulos.
 * Las imágenes se llaman `escudo-<categoría>-NN` y `copa-<categoría>-NN` (sin categoría = genéricos).
 */
import type { PlayerProgress } from './progression';
import { ACHIEVEMENTS, CATEGORIES, categoryFor } from './progression';

export type IconKind = 'logo' | 'cup';

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
}

/** Para añadir una categoría: una línea aquí y sus imágenes con el prefijo `escudo-<id>-` o `copa-<id>-`. */
export const ICON_CATEGORIES: IconCategory[] = [
  { id: 'generico', kind: 'logo', label: 'Genéricos' },
  { id: 'retro80', kind: 'logo', label: 'Retro 80' },
  { id: 'generico', kind: 'cup', label: 'Genéricas' },
  { id: 'retro90', kind: 'cup', label: 'Retro 90' },
];

export const ICON_PREFIX: Record<IconKind, string> = { logo: 'escudo-', cup: 'copa-' };

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
