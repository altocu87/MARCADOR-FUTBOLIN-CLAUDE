/**
 * Récords y Hall of Fame. Cada marca conserva el partido o jugador que la justifica
 * y se recalcula desde el historial (coherente ante correcciones posteriores).
 */
import type { Player, StoredMatch } from '../persistence';
import { personalGoals } from '../statistics/extras';
import { comebackSize, computePlayerStats, fastestGoalMs, formatDuration, sortMatches } from '../statistics/statistics';
import type { ProgressionSnapshot } from './progression';

const plural = (n: number, one: string, many: string) => `${n} ${n === 1 ? one : many}`;

/** Muestra mínima propuesta para marcas porcentuales. */
export const MIN_MATCHES_FOR_PCT = 10;

export interface RecordEntry {
  id: string;
  title: string;
  value: string;
  holder: string;
  matchId?: string;
  playerId?: string;
  at?: number;
}

const teamNames = (m: StoredMatch, team: 'white' | 'blue') =>
  m.participants
    .filter((p) => p.team === team)
    .sort((a, b) => a.slot - b.slot)
    .map((p) => p.nameSnapshot)
    .join(' + ');

export function matchLabel(m: StoredMatch): string {
  return `${teamNames(m, 'white')} ${m.result.score.white}–${m.result.score.blue} ${teamNames(m, 'blue')}`;
}

export function computeRecords(
  players: Player[],
  matches: StoredMatch[],
  progression: ProgressionSnapshot | null,
): RecordEntry[] {
  const records: RecordEntry[] = [];
  const sorted = sortMatches(matches);
  // Ante empate de marca se mantiene el primero en conseguirla.
  const bestMatch = (score: (m: StoredMatch) => number | null, lower = false) => {
    let best: StoredMatch | null = null;
    let bestVal = 0;
    for (const m of sorted) {
      const v = score(m);
      if (v === null) continue;
      if (best === null || (lower ? v < bestVal : v > bestVal)) {
        best = m;
        bestVal = v;
      }
    }
    return best ? { match: best, value: bestVal } : null;
  };

  const fast = bestMatch(fastestGoalMs, true);
  if (fast) records.push({ id: 'fastest_goal', title: 'Gol más rápido', value: `${(fast.value / 1000).toFixed(1)} s`, holder: matchLabel(fast.match), matchId: fast.match.id, at: fast.match.finishedAt });

  const rout = bestMatch((m) => Math.abs(m.result.score.white - m.result.score.blue));
  if (rout && rout.value > 0) records.push({ id: 'biggest_win', title: 'Mayor goleada', value: `+${rout.value}`, holder: matchLabel(rout.match), matchId: rout.match.id, at: rout.match.finishedAt });

  const goals = bestMatch((m) => m.result.score.white + m.result.score.blue);
  if (goals && goals.value > 0) records.push({ id: 'most_goals', title: 'Más goles en un encuentro', value: String(goals.value), holder: matchLabel(goals.match), matchId: goals.match.id, at: goals.match.finishedAt });

  const comeback = bestMatch(comebackSize);
  if (comeback && comeback.value > 0) records.push({ id: 'comeback', title: 'Mayor remontada', value: `−${comeback.value}`, holder: matchLabel(comeback.match), matchId: comeback.match.id, at: comeback.match.finishedAt });

  const longest = bestMatch((m) => m.result.totalTimeMs);
  if (longest) records.push({ id: 'longest_match', title: 'Encuentro más largo', value: formatDuration(longest.value), holder: matchLabel(longest.match), matchId: longest.match.id, at: longest.match.finishedAt });

  const bestPlayer = (score: (p: Player) => number | null, fmt: (v: number) => string, id: string, title: string) => {
    let best: Player | null = null;
    let bestVal = 0;
    for (const p of players) {
      const v = score(p);
      if (v === null) continue;
      if (best === null || v > bestVal) {
        best = p;
        bestVal = v;
      }
    }
    if (best && bestVal > 0) records.push({ id, title, value: fmt(bestVal), holder: best.name, playerId: best.id });
  };

  const stats = new Map(players.map((p) => [p.id, computePlayerStats(p.id, matches)]));
  bestPlayer((p) => stats.get(p.id)!.bestWinStreak, (v) => plural(v, 'victoria', 'victorias'), 'best_streak', 'Racha más larga');
  bestPlayer((p) => stats.get(p.id)!.general.played, (v) => plural(v, 'partido', 'partidos'), 'most_matches', 'Más partidos');
  bestPlayer((p) => stats.get(p.id)!.general.wins, (v) => plural(v, 'victoria', 'victorias'), 'most_wins', 'Más victorias');
  bestPlayer(
    (p) => {
      const g = stats.get(p.id)!.general;
      return g.played >= MIN_MATCHES_FOR_PCT ? g.winPct : null;
    },
    (v) => `${v.toFixed(0)} %`,
    'best_pct',
    `Mejor % de victorias (mín. ${MIN_MATCHES_FOR_PCT})`,
  );
  if (progression) {
    bestPlayer((p) => progression.players.get(p.id)?.maxElo ?? null, (v) => `${v} ELO`, 'max_elo', 'Mayor ELO');
  }
  return records;
}

export interface HallOfFameRow {
  id: string;
  title: string;
  leaders: { playerId: string; name: string; value: string; photo?: string }[];
}

export function computeHallOfFame(
  players: Player[],
  matches: StoredMatch[],
  progression: ProgressionSnapshot | null,
): HallOfFameRow[] {
  const stats = new Map(players.map((p) => [p.id, computePlayerStats(p.id, matches)]));
  const top = (id: string, title: string, score: (p: Player) => number | null, fmt: (v: number) => string): HallOfFameRow => ({
    id,
    title,
    leaders: players
      .map((p) => ({ p, v: score(p) }))
      .filter((x): x is { p: Player; v: number } => x.v !== null && x.v > 0)
      .sort((a, b) => b.v - a.v)
      .slice(0, 3)
      .map(({ p, v }) => ({ playerId: p.id, name: p.name, value: fmt(v), photo: p.photo })),
  });
  const rows = [
    top('wins', 'Más victorias', (p) => stats.get(p.id)!.general.wins, (v) => String(v)),
    top('streak', 'Mejor racha', (p) => stats.get(p.id)!.bestWinStreak, (v) => String(v)),
    top('pct', `% victorias (mín. ${MIN_MATCHES_FOR_PCT})`, (p) => {
      const g = stats.get(p.id)!.general;
      return g.played >= MIN_MATCHES_FOR_PCT ? g.winPct : null;
    }, (v) => `${v.toFixed(0)} %`),
    top('matches', 'Más partidos', (p) => stats.get(p.id)!.general.played, (v) => String(v)),
  ];
  const goals = personalGoals(matches);
  rows.push(top('scorer', 'Pichichi (goles asignados)', (p) => goals.get(p.id) ?? null, (v) => String(v)));
  if (progression) {
    rows.push(top('tournaments', 'Torneos ganados', (p) => progression.players.get(p.id)?.tournamentsWon ?? null, (v) => String(v)));
  }
  if (progression) {
    rows.unshift(
      top('elo', 'Mayor ELO', (p) => (progression.players.get(p.id)?.rankedPlayed ? progression.players.get(p.id)!.elo : null), (v) => String(v)),
      top('xp', 'Más experiencia', (p) => progression.players.get(p.id)?.xp ?? null, (v) => `${v} XP`),
    );
  }
  return rows;
}
