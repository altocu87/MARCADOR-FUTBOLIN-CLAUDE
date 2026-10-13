/** Claves de calendario locales: día, semana ISO, temporada. */
import type { SeasonLength } from '../persistence';

const pad = (n: number) => String(n).padStart(2, '0');

export function dayKey(ts: number): string {
  const d = new Date(ts);
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
}

/** Semana ISO 8601 (lunes a domingo), p. ej. «2026-W40». */
export function weekKey(ts: number): string {
  const d = new Date(ts);
  const date = new Date(Date.UTC(d.getFullYear(), d.getMonth(), d.getDate()));
  const day = date.getUTCDay() || 7;
  date.setUTCDate(date.getUTCDate() + 4 - day);
  const yearStart = new Date(Date.UTC(date.getUTCFullYear(), 0, 1));
  const week = Math.ceil(((date.getTime() - yearStart.getTime()) / 86_400_000 + 1) / 7);
  return `${date.getUTCFullYear()}-W${pad(week)}`;
}

export function seasonKey(ts: number, length: SeasonLength): string {
  const d = new Date(ts);
  if (length === 'month') return `${d.getFullYear()}-${pad(d.getMonth() + 1)}`;
  return `${d.getFullYear()}-T${Math.floor(d.getMonth() / 3) + 1}`;
}

const MONTHS = ['enero', 'febrero', 'marzo', 'abril', 'mayo', 'junio', 'julio', 'agosto', 'septiembre', 'octubre', 'noviembre', 'diciembre'];

export function seasonLabel(key: string): string {
  const [y, rest] = key.split('-');
  if (rest.startsWith('T')) return `${rest.slice(1)}º trimestre ${y}`;
  return `${MONTHS[Number(rest) - 1]} ${y}`;
}

export const WEEKDAYS = ['Dom', 'Lun', 'Mar', 'Mié', 'Jue', 'Vie', 'Sáb'];

export type DayPart = 'mañana' | 'mediodía' | 'tarde' | 'noche';

export function dayPart(ts: number): DayPart {
  const h = new Date(ts).getHours();
  if (h >= 6 && h < 12) return 'mañana';
  if (h >= 12 && h < 16) return 'mediodía';
  if (h >= 16 && h < 21) return 'tarde';
  return 'noche';
}

/** Número pseudoaleatorio determinista a partir de un texto (para retos del día/semana). */
export function seededRandom(seed: string): () => number {
  let h = 2166136261;
  for (const ch of seed) h = Math.imul(h ^ ch.charCodeAt(0), 16777619);
  let x = h >>> 0;
  return () => {
    x ^= x << 13;
    x ^= x >>> 17;
    x ^= x << 5;
    return ((x >>> 0) % 1_000_000) / 1_000_000;
  };
}
