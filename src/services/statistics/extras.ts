/**
 * Estadísticas ampliadas: lado de la mesa, horarios, parejas, goleadores asignados,
 * y resúmenes por periodo. Todo derivado del historial guardado.
 */
import { validGoalsFromEvents, type Team } from '../../match-engine';
import type { Player, StoredMatch } from '../persistence';
import { WEEKDAYS, dayPart, type DayPart } from './calendar';
import { sortMatches, teamOf } from './statistics';

export interface WinRecord {
  played: number;
  wins: number;
  winPct: number | null;
}

const rec = (): WinRecord => ({ played: 0, wins: 0, winPct: null });
const add = (r: WinRecord, won: boolean) => {
  r.played += 1;
  if (won) r.wins += 1;
  r.winPct = (r.wins / r.played) * 100;
};

/** Rendimiento con Blanco (izquierda) y con Azul (derecha). */
export function sideStats(playerId: string, matches: StoredMatch[]): Record<Team, WinRecord> {
  const out = { white: rec(), blue: rec() };
  for (const m of matches) {
    const t = teamOf(m, playerId);
    if (t) add(out[t], m.result.winner === t);
  }
  return out;
}

export interface Habits {
  byWeekday: { label: string; rec: WinRecord }[];
  byDayPart: { label: DayPart; rec: WinRecord }[];
  favoriteDay: string | null;
  bestDayPart: DayPart | null;
}

export function habits(playerId: string, matches: StoredMatch[]): Habits {
  const days = WEEKDAYS.map((label) => ({ label, rec: rec() }));
  const parts: { label: DayPart; rec: WinRecord }[] = (['mañana', 'mediodía', 'tarde', 'noche'] as DayPart[]).map((label) => ({ label, rec: rec() }));
  for (const m of matches) {
    const t = teamOf(m, playerId);
    if (!t) continue;
    const won = m.result.winner === t;
    add(days[new Date(m.finishedAt).getDay()].rec, won);
    add(parts.find((p) => p.label === dayPart(m.finishedAt))!.rec, won);
  }
  // Lunes primero.
  const byWeekday = [...days.slice(1), days[0]];
  const fav = [...byWeekday].sort((a, b) => b.rec.played - a.rec.played)[0];
  const best = parts.filter((p) => p.rec.played >= 3).sort((a, b) => (b.rec.winPct ?? 0) - (a.rec.winPct ?? 0))[0];
  return {
    byWeekday,
    byDayPart: parts,
    favoriteDay: fav && fav.rec.played > 0 ? fav.label : null,
    bestDayPart: best ? best.label : null,
  };
}

export interface PairStat {
  key: string;
  playerIds: [string, string];
  names: string;
  rec: WinRecord;
}

/** Parejas de 2v2 (mismo equipo) con su balance. */
export function pairStats(matches: StoredMatch[]): PairStat[] {
  const map = new Map<string, PairStat>();
  for (const m of matches) {
    for (const team of ['white', 'blue'] as Team[]) {
      const ps = m.participants.filter((p) => p.team === team);
      if (ps.length !== 2) continue;
      const sorted = [...ps].sort((a, b) => a.playerId.localeCompare(b.playerId));
      const key = sorted.map((p) => p.playerId).join('|');
      const entry = map.get(key) ?? {
        key,
        playerIds: [sorted[0].playerId, sorted[1].playerId] as [string, string],
        names: '',
        rec: rec(),
      };
      entry.names = sorted.map((p) => p.nameSnapshot).join(' + ');
      add(entry.rec, m.result.winner === team);
      map.set(key, entry);
    }
  }
  return [...map.values()].sort((a, b) => (b.rec.winPct ?? 0) - (a.rec.winPct ?? 0) || b.rec.played - a.rec.played);
}

/** Goles personales asignados (solo goles válidos; los dobles de Caos cuentan 1 gol). */
export function personalGoals(matches: StoredMatch[]): Map<string, number> {
  const out = new Map<string, number>();
  for (const m of matches) {
    if (!m.scorers) continue;
    for (const g of validGoalsFromEvents(m.events)) {
      const pid = m.scorers[g.id];
      if (pid) out.set(pid, (out.get(pid) ?? 0) + 1);
    }
  }
  return out;
}

export function personalGoalsInMatch(m: StoredMatch, playerId: string): number {
  if (!m.scorers) return 0;
  return validGoalsFromEvents(m.events).filter((g) => m.scorers![g.id] === playerId).length;
}

/** Si un equipo tiene un solo jugador (1v1, 1v2…), sus goles son suyos: se asignan automáticamente. */
export function autoScorers(m: StoredMatch): Record<string, string> | undefined {
  const solo = (team: string) => {
    const members = m.participants.filter((p) => p.team === team);
    return members.length === 1 ? members[0].playerId : undefined;
  };
  if (!solo('white') && !solo('blue')) return m.scorers;
  const out: Record<string, string> = { ...(m.scorers ?? {}) };
  for (const g of m.events) {
    if (g.type !== 'GOAL' || !g.team) continue;
    const pid = solo(g.team);
    if (pid) out[g.id] = pid;
  }
  return out;
}

export interface PeriodDigest {
  matches: number;
  goals: number;
  playerOfPeriod: { playerId: string; name: string; wins: number; played: number } | null;
  mostActive: { playerId: string; name: string; played: number } | null;
  closest: StoredMatch | null;
  biggest: StoredMatch | null;
  topScorer: { playerId: string; name: string; goals: number } | null;
}

/** Resumen de un intervalo (semana/mes): jugador destacado, partido más igualado, etc. */
export function periodDigest(matches: StoredMatch[], players: Player[], from: number, to: number): PeriodDigest {
  const list = sortMatches(matches).filter((m) => m.finishedAt >= from && m.finishedAt < to);
  const names = new Map(players.map((p) => [p.id, p.name]));
  const wins = new Map<string, { wins: number; played: number }>();
  for (const m of list) {
    for (const p of m.participants) {
      const w = wins.get(p.playerId) ?? { wins: 0, played: 0 };
      w.played += 1;
      if (p.team === m.result.winner) w.wins += 1;
      wins.set(p.playerId, w);
    }
  }
  const entries = [...wins.entries()];
  const pop = entries.sort((a, b) => b[1].wins - a[1].wins || a[1].played - b[1].played)[0];
  const act = [...wins.entries()].sort((a, b) => b[1].played - a[1].played)[0];
  const diff = (m: StoredMatch) => Math.abs(m.result.score.white - m.result.score.blue);
  const total = (m: StoredMatch) => m.result.score.white + m.result.score.blue;
  const closest = [...list].sort((a, b) => diff(a) - diff(b) || total(b) - total(a))[0] ?? null;
  const biggest = [...list].sort((a, b) => diff(b) - diff(a))[0] ?? null;
  const pg = [...personalGoals(list).entries()].sort((a, b) => b[1] - a[1])[0];
  return {
    matches: list.length,
    goals: list.reduce((s, m) => s + total(m), 0),
    playerOfPeriod: pop && pop[1].wins > 0 ? { playerId: pop[0], name: names.get(pop[0]) ?? '?', ...pop[1] } : null,
    mostActive: act ? { playerId: act[0], name: names.get(act[0]) ?? '?', played: act[1].played } : null,
    closest,
    biggest: biggest && diff(biggest) > 0 ? biggest : null,
    topScorer: pg ? { playerId: pg[0], name: names.get(pg[0]) ?? '?', goals: pg[1] } : null,
  };
}

/** Enfrentamientos entre dos jugadores en equipos contrarios (cualquier alineación). */
export function duel(aId: string, bId: string, matches: StoredMatch[]): { played: number; aWins: number; bWins: number; last: StoredMatch[] } {
  const list = sortMatches(matches).filter((m) => {
    const ta = teamOf(m, aId);
    const tb = teamOf(m, bId);
    return ta && tb && ta !== tb;
  });
  let aWins = 0;
  for (const m of list) if (m.result.winner === teamOf(m, aId)) aWins += 1;
  return { played: list.length, aWins, bWins: list.length - aWins, last: list.slice(-5).reverse() };
}
