/**
 * Estadísticas derivadas del historial guardado (los partidos de prueba nunca se guardan).
 * Definiciones centralizadas (versión STATS_DEFINITIONS_VERSION):
 * - Los goles del perfil son «goles del equipo mientras participaba», nunca goles personales.
 *   En 2v2 cada participante recibe los goles de su equipo; los totales globales se calculan
 *   por partido, no sumando perfiles, para no contar dos veces.
 * - Goles ordinarios y de penaltis se cuentan por separado.
 * - Racha: secuencia de victorias (o derrotas) consecutivas en todas las modalidades.
 * - Empate: con las reglas actuales todo partido tiene ganador (penaltis incluidos);
 *   el campo existe por si una regla futura lo permite.
 * - Remontada: máxima desventaja en el marcador ordinario superada por el ganador.
 */
import { goalValue, validGoalsFromEvents, type MatchMode, type Team } from '../../match-engine';
import type { StoredMatch } from '../persistence';

export const STATS_DEFINITIONS_VERSION = 'estadisticas-1';
export const RIVAL_MIN_MATCHES = 5;

export type ResultLetter = 'G' | 'P' | 'E';

export interface StatBlock {
  played: number;
  wins: number;
  losses: number;
  draws: number;
  /** null si no hay partidos (estado vacío, sin división por cero). */
  winPct: number | null;
  goalsFor: number;
  goalsAgainst: number;
  goalDiff: number;
  penaltyGoalsFor: number;
  penaltyGoalsAgainst: number;
}

export interface Streak {
  type: 'G' | 'P' | null;
  count: number;
}

export interface RivalStat {
  playerId: string;
  name: string;
  played: number;
  wins: number;
  losses: number;
  winPct: number;
}

export interface PlayerStats {
  playerId: string;
  general: StatBlock;
  ranked: StatBlock;
  byMode: Record<MatchMode, StatBlock>;
  currentStreak: Streak;
  bestWinStreak: number;
  /** Últimas 5 clasificatorias, de más antigua a más reciente. */
  form: ResultLetter[];
  /** Últimos 5 partidos de cualquier modalidad. */
  recent: ResultLetter[];
  rivals: RivalStat[];
  teammates: RivalStat[];
  favoriteRival: RivalStat | null;
  nemesis: RivalStat | null;
  totalPlayTimeMs: number;
}

export const emptyBlock = (): StatBlock => ({
  played: 0,
  wins: 0,
  losses: 0,
  draws: 0,
  winPct: null,
  goalsFor: 0,
  goalsAgainst: 0,
  goalDiff: 0,
  penaltyGoalsFor: 0,
  penaltyGoalsAgainst: 0,
});

export function sortMatches(matches: StoredMatch[]): StoredMatch[] {
  return [...matches].sort((a, b) => a.finishedAt - b.finishedAt || a.id.localeCompare(b.id));
}

export function teamOf(match: StoredMatch, playerId: string): Team | null {
  return match.participants.find((p) => p.playerId === playerId)?.team ?? null;
}

function addToBlock(block: StatBlock, match: StoredMatch, team: Team): void {
  const opp: Team = team === 'white' ? 'blue' : 'white';
  block.played += 1;
  if (match.result.winner === team) block.wins += 1;
  else block.losses += 1;
  block.goalsFor += match.result.score[team];
  block.goalsAgainst += match.result.score[opp];
  block.goalDiff = block.goalsFor - block.goalsAgainst;
  if (match.result.penaltyScore) {
    block.penaltyGoalsFor += match.result.penaltyScore[team];
    block.penaltyGoalsAgainst += match.result.penaltyScore[opp];
  }
  block.winPct = block.played > 0 ? (block.wins / block.played) * 100 : null;
}

export function computePlayerStats(playerId: string, matches: StoredMatch[]): PlayerStats {
  const general = emptyBlock();
  const ranked = emptyBlock();
  const byMode: Record<MatchMode, StatBlock> = { quick: emptyBlock(), chaos: emptyBlock(), ranked: emptyBlock() };
  const letters: ResultLetter[] = [];
  const rankedLetters: ResultLetter[] = [];
  const rivals = new Map<string, RivalStat>();
  const mates = new Map<string, RivalStat>();
  let bestWinStreak = 0;
  let run = 0;
  let totalPlayTimeMs = 0;

  for (const m of sortMatches(matches)) {
    const team = teamOf(m, playerId);
    if (!team) continue;
    const won = m.result.winner === team;
    addToBlock(general, m, team);
    addToBlock(byMode[m.config.mode], m, team);
    if (m.config.mode === 'ranked') {
      addToBlock(ranked, m, team);
      rankedLetters.push(won ? 'G' : 'P');
    }
    letters.push(won ? 'G' : 'P');
    run = won ? run + 1 : 0;
    bestWinStreak = Math.max(bestWinStreak, run);
    totalPlayTimeMs += m.result.totalTimeMs;

    for (const p of m.participants) {
      if (p.playerId === playerId) continue;
      const map = p.team === team ? mates : rivals;
      const r = map.get(p.playerId) ?? { playerId: p.playerId, name: p.nameSnapshot, played: 0, wins: 0, losses: 0, winPct: 0 };
      r.name = p.nameSnapshot;
      r.played += 1;
      if (won) r.wins += 1;
      else r.losses += 1;
      r.winPct = (r.wins / r.played) * 100;
      map.set(p.playerId, r);
    }
  }

  const currentStreak: Streak = { type: null, count: 0 };
  for (let i = letters.length - 1; i >= 0; i -= 1) {
    const l = letters[i] as 'G' | 'P';
    if (currentStreak.type === null) currentStreak.type = l;
    if (l !== currentStreak.type) break;
    currentStreak.count += 1;
  }

  const rivalList = [...rivals.values()].sort((a, b) => b.played - a.played);
  const eligible = rivalList.filter((r) => r.played >= RIVAL_MIN_MATCHES);
  // Propuesta de algoritmo: favorito = mayor % de victorias; némesis = menor. Desempate: más partidos.
  const favoriteRival =
    eligible.length > 0 ? [...eligible].sort((a, b) => b.winPct - a.winPct || b.played - a.played)[0] : null;
  const nemesis =
    eligible.length > 0 ? [...eligible].sort((a, b) => a.winPct - b.winPct || b.played - a.played)[0] : null;

  return {
    playerId,
    general,
    ranked,
    byMode,
    currentStreak,
    bestWinStreak,
    form: rankedLetters.slice(-5),
    recent: letters.slice(-5),
    rivals: rivalList,
    teammates: [...mates.values()].sort((a, b) => b.played - a.played),
    favoriteRival,
    nemesis,
    totalPlayTimeMs,
  };
}

/** Máxima desventaja que superó el ganador (0 si nunca fue por detrás). */
export function comebackSize(match: StoredMatch): number {
  const winner = match.result.winner;
  const loser: Team = winner === 'white' ? 'blue' : 'white';
  const score = { white: 0, blue: 0 };
  let maxDeficit = 0;
  for (const g of validGoalsFromEvents(match.events)) {
    if (!g.team) continue;
    score[g.team] += goalValue(g);
    maxDeficit = Math.max(maxDeficit, score[loser] - score[winner]);
  }
  return maxDeficit;
}

/** Tiempo (ms) desde el inicio de su periodo del gol válido más rápido del partido. */
export function fastestGoalMs(match: StoredMatch): number | null {
  const goals = validGoalsFromEvents(match.events);
  if (goals.length === 0) return null;
  return Math.min(...goals.map((g) => g.periodTimeMs));
}

/** Enfrentamientos directos entre dos alineaciones exactas (orden de equipo indiferente). */
export function headToHead(
  matches: StoredMatch[],
  whiteIds: string[],
  blueIds: string[],
  onlyRanked = true,
): { played: number; whiteWins: number; blueWins: number } {
  const key = (ids: string[]) => [...ids].sort().join('|');
  const kw = key(whiteIds);
  const kb = key(blueIds);
  let played = 0;
  let whiteWins = 0;
  let blueWins = 0;
  for (const m of matches) {
    if (onlyRanked && m.config.mode !== 'ranked') continue;
    const mw = key(m.participants.filter((p) => p.team === 'white').map((p) => p.playerId));
    const mb = key(m.participants.filter((p) => p.team === 'blue').map((p) => p.playerId));
    let ourWhiteTeam: Team | null = null;
    if (mw === kw && mb === kb) ourWhiteTeam = 'white';
    else if (mw === kb && mb === kw) ourWhiteTeam = 'blue';
    if (!ourWhiteTeam) continue;
    played += 1;
    if (m.result.winner === ourWhiteTeam) whiteWins += 1;
    else blueWins += 1;
  }
  return { played, whiteWins, blueWins };
}

/**
 * Últimos enfrentamientos entre estas dos alineaciones (cualquier modalidad), del más reciente al
 * más antiguo. El marcador se da siempre desde el punto de vista de las alineaciones pedidas:
 * «white» es el equipo que hoy juega de Blanco, aunque aquel día jugase de Azul.
 */
export function lastMeetings(
  matches: StoredMatch[],
  whiteIds: string[],
  blueIds: string[],
  limit = 3,
): { match: StoredMatch; white: number; blue: number; winner: Team }[] {
  const key = (ids: string[]) => [...ids].sort().join('|');
  const kw = key(whiteIds);
  const kb = key(blueIds);
  const out: { match: StoredMatch; white: number; blue: number; winner: Team }[] = [];
  for (const m of [...matches].sort((a, b) => b.finishedAt - a.finishedAt)) {
    const mw = key(m.participants.filter((p) => p.team === 'white').map((p) => p.playerId));
    const mb = key(m.participants.filter((p) => p.team === 'blue').map((p) => p.playerId));
    const swapped = mw === kb && mb === kw;
    if (!(mw === kw && mb === kb) && !swapped) continue;
    const { white, blue } = m.result.score;
    out.push(
      swapped
        ? { match: m, white: blue, blue: white, winner: m.result.winner === 'white' ? 'blue' : 'white' }
        : { match: m, white, blue, winner: m.result.winner },
    );
    if (out.length >= limit) break;
  }
  return out;
}

export function formatDuration(ms: number): string {
  const total = Math.max(0, Math.floor(ms / 1000));
  const m = Math.floor(total / 60);
  const s = total % 60;
  return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
}
