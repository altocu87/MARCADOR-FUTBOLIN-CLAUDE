/**
 * Reglas de progresión: ELO, rangos, XP y niveles. Los valores de aquí son los recomendados; el panel de
 * administración puede publicar otros (gameRules.ts los aplica). Todo se recalcula siempre desde el historial.
 */
import type { MatchMode } from '../../match-engine';

export const PROGRESSION_RULES_VERSION = 'progresion-propuesta-1';

// ---------------------------------------------------------------- ELO

export function eloExpected(own: number, rival: number): number {
  return 1 / (1 + 10 ** ((rival - own) / 400));
}

/** Multiplicador por diferencia de goles (si las reglas lo activan). */
export function goalDiffMultiplier(diff: number): number {
  const d = Math.abs(diff);
  if (d <= 1) return 1;
  if (d === 2) return 1.05;
  if (d === 3) return 1.1;
  if (d === 4) return 1.15;
  return 1.2;
}

export interface CategoryDef {
  id: string;
  name: string;
  min: number;
  color: string;
}

/** Rangos de menor a mayor. Se empieza en Chatarra (ELO inicial 1200) y hay que ganarse el resto. */
export const CATEGORIES: CategoryDef[] = [
  { id: 'scrap', name: 'Chatarra', min: -Infinity, color: '#9C8F86' },
  { id: 'wood', name: 'Madera', min: 1225, color: '#B5814A' },
  { id: 'bronze', name: 'Bronce', min: 1275, color: '#C98A54' },
  { id: 'silver', name: 'Plata', min: 1350, color: '#C9D3E0' },
  { id: 'gold', name: 'Oro', min: 1425, color: '#F2C94C' },
  { id: 'platinum', name: 'Platino', min: 1500, color: '#7FE3D6' },
  { id: 'diamond', name: 'Diamante', min: 1600, color: '#8AB8FF' },
];

/** Rango siguiente (null si ya es el máximo). */
export function nextCategory(category: CategoryDef): CategoryDef | null {
  const i = CATEGORIES.findIndex((c) => c.id === category.id);
  return CATEGORIES[i + 1] ?? null;
}

export function categoryFor(elo: number): CategoryDef {
  let cat = CATEGORIES[0];
  for (const c of CATEGORIES) if (elo >= c.min) cat = c;
  return cat;
}

// ---------------------------------------------------------------- XP

export const XP_TABLE = {
  complete: 50,
  win: 100,
  draw: 60,
  loss: 25,
  rankedWinBonus: 50,
  overtimeWin: 25,
  penaltiesWin: 25,
  tournamentWin: 300,
};

// Curva de niveles: XP para el nivel N = base × N^exponente. La cambian las reglas del panel (gameRules.ts).
let levelBase = 100;
let levelExponent = 1.35;
export let MAX_LEVEL = 100;

export function setLevelCurve(base: number, exponent: number, max: number): void {
  levelBase = base;
  levelExponent = exponent;
  MAX_LEVEL = max;
}

/** XP acumulado necesario para alcanzar el nivel N (base × N^exponente, redondeado). */
export function xpForLevel(level: number): number {
  if (level <= 0) return 0;
  return Math.round(levelBase * level ** levelExponent);
}

export function levelForXp(xp: number): number {
  let level = 0;
  while (level < MAX_LEVEL && xp >= xpForLevel(level + 1)) level += 1;
  return level;
}

/** Progreso (0–1) hacia el siguiente nivel. */
export function levelProgress(xp: number): number {
  const level = levelForXp(xp);
  if (level >= MAX_LEVEL) return 1;
  const a = xpForLevel(level);
  const b = xpForLevel(level + 1);
  return (xp - a) / (b - a);
}

/** Modalidades que modifican ELO en la propuesta actual. */
export const ELO_MODES: MatchMode[] = ['ranked'];
