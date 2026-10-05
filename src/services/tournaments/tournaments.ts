/**
 * Torneos (propuesta «torneos-2», pendiente de aprobación):
 * - Liguilla: todos contra todos a una vuelta, 3 a 8 equipos. Victoria 3 puntos.
 *   Desempate: puntos → diferencia de goles → goles a favor → enfrentamiento directo → nombre.
 *   Opcional: final entre los dos primeros.
 * - Cuadro: eliminación directa para 3 a 8 equipos; los huecos dan pase directo.
 *   Siembra por ELO (1 contra el último) o aleatoria.
 * - Pool: 4 a 16 jugadores, parejas que cambian en cada partido y puntos individuales
 *   (ver pool.ts). Opcional: final 1º contra 2º o 1º+4º contra 2º+3º.
 * La final puede jugarse al mejor de 3 y con otros goles. Ganar el torneo concede XP una sola vez.
 */
import type { MatchConfig, ParticipantRef, Team } from '../../match-engine';
import { newId } from '../ids';
import {
  STORAGE_FORMAT_VERSION,
  type Fixture,
  type Player,
  type StoredMatch,
  type Tournament,
  type TournamentFinal,
  type TournamentFormat,
  type TournamentTeam,
} from '../persistence';
import { POOL_MAX_PLAYERS, POOL_MIN_PLAYERS, poolGameOptions, schedulePool } from './pool';

export const TOURNAMENT_RULES_VERSION = 'torneos-2';
export const MIN_TEAMS = 3;
export const MAX_TEAMS = 8;

export const FORMAT_LABEL: Record<TournamentFormat, string> = {
  league: 'Liguilla',
  bracket: 'Cuadro eliminatorio',
  pool: 'Pool rotativo',
};

export interface TournamentDraft {
  name: string;
  format: TournamentFormat;
  teamSize: 1 | 2;
  ranked: boolean;
  config: MatchConfig;
  /** Liguilla y cuadro: equipos ya formados. */
  teams: { playerIds: string[] }[];
  seeding: 'elo' | 'random';
  /** Pool: jugadores inscritos. */
  entrants?: string[];
  /** Pool: partidos por jugador (debe ser un valor de `poolGameOptions`). */
  gamesPerPlayer?: number;
  /** Pool: parejas equilibradas por ELO o al azar. */
  pairing?: 'elo' | 'random';
  final?: TournamentFinal;
  finalBestOf?: number;
  /** Reglas de la final si difieren. */
  finalConfig?: MatchConfig;
  templateName?: string;
  templateId?: string;
  edition?: number;
}

export function validateDraft(d: TournamentDraft): string[] {
  const errors: string[] = [];
  if (!d.name.trim()) errors.push('Ponle un nombre al torneo.');
  if (d.format === 'pool') {
    const ids = d.entrants ?? [];
    if (ids.length < POOL_MIN_PLAYERS || ids.length > POOL_MAX_PLAYERS)
      errors.push(`El Pool necesita entre ${POOL_MIN_PLAYERS} y ${POOL_MAX_PLAYERS} jugadores.`);
    else if (new Set(ids).size !== ids.length) errors.push('Un jugador está repetido.');
    else if (!poolGameOptions(ids.length).includes(d.gamesPerPlayer ?? 0))
      errors.push('Ese número de partidos por jugador no cuadra con los inscritos.');
    return errors;
  }
  if (d.final === 'top4') errors.push('La final 1º+4º contra 2º+3º solo existe en el Pool.');
  if (d.teams.length < MIN_TEAMS || d.teams.length > MAX_TEAMS) errors.push(`Entre ${MIN_TEAMS} y ${MAX_TEAMS} equipos.`);
  const all = d.teams.flatMap((t) => t.playerIds);
  if (new Set(all).size !== all.length) errors.push('Un jugador no puede estar en dos equipos.');
  if (d.teams.some((t) => t.playerIds.length !== d.teamSize))
    errors.push(d.teamSize === 2 ? 'Con parejas fijas hace falta un número par de jugadores.' : 'Cada equipo necesita 1 jugador.');
  return errors;
}

export function teamName(playerIds: string[], players: Player[]): string {
  return playerIds.map((id) => players.find((p) => p.id === id)?.name ?? '?').join(' + ');
}

/**
 * Forma equipos fijos con los jugadores elegidos. Parejas equilibradas: el mejor ELO con el peor,
 * el segundo con el penúltimo… Al azar: se barajan. Si sobra uno, queda en `leftover`.
 */
export function formTeams(
  selected: string[],
  teamSize: 1 | 2,
  pairing: 'elo' | 'random',
  eloOf: (id: string) => number,
  rnd: () => number = Math.random,
): { teams: { playerIds: string[] }[]; leftover: string[] } {
  if (teamSize === 1) return { teams: selected.map((id) => ({ playerIds: [id] })), leftover: [] };
  const teams: { playerIds: string[] }[] = [];
  let pool = [...selected];
  if (pairing === 'elo') {
    pool.sort((a, b) => eloOf(b) - eloOf(a));
    // Con número impar se queda fuera el del medio para no descompensar.
    const leftover = pool.length % 2 ? pool.splice(Math.floor(pool.length / 2), 1) : [];
    while (pool.length >= 2) teams.push({ playerIds: [pool.shift()!, pool.pop()!] });
    return { teams, leftover };
  }
  pool = shuffle(pool, rnd);
  while (pool.length >= 2) teams.push({ playerIds: [pool.shift()!, pool.shift()!] });
  return { teams, leftover: pool };
}

function shuffle<T>(list: T[], rnd: () => number): T[] {
  const out = [...list];
  for (let i = out.length - 1; i > 0; i -= 1) {
    const j = Math.floor(rnd() * (i + 1));
    [out[i], out[j]] = [out[j], out[i]];
  }
  return out;
}

/** Emparejamientos a una vuelta por el método del círculo. */
export function roundRobin(teamIds: string[]): Fixture[] {
  const ids: (string | null)[] = [...teamIds];
  if (ids.length % 2 === 1) ids.push(null);
  const n = ids.length;
  const fixtures: Fixture[] = [];
  for (let r = 0; r < n - 1; r += 1) {
    for (let i = 0; i < n / 2; i += 1) {
      const a = ids[i];
      const b = ids[n - 1 - i];
      if (a && b) {
        // Alternar lados para repartir Blanco/Azul.
        const [w, bl] = (r + i) % 2 === 0 ? [a, b] : [b, a];
        fixtures.push({ id: newId('fx'), round: r + 1, whiteTeamId: w, blueTeamId: bl });
      }
    }
    ids.splice(1, 0, ids.pop()!);
  }
  return fixtures;
}

/** Orden de siembra estándar para un cuadro de tamaño `size` (1-8, 4-5, 2-7, 3-6…). */
function seedOrder(size: number): number[] {
  let order = [1];
  while (order.length < size) {
    const next = order.length * 2 + 1;
    order = order.flatMap((s) => [s, next - s]);
  }
  return order;
}

/** Cuadro de eliminación con pases directos para completar potencia de 2. */
export function buildBracket(seededTeamIds: string[]): Fixture[] {
  const size = seededTeamIds.length <= 4 ? 4 : 8;
  const order = seedOrder(size);
  const rounds = Math.log2(size);
  const fixtures: Fixture[][] = [];
  for (let r = 1; r <= rounds; r += 1) {
    const count = size / 2 ** r;
    fixtures.push(Array.from({ length: count }, () => ({ id: newId('fx'), round: r, whiteTeamId: null, blueTeamId: null })));
  }
  for (let r = 0; r < rounds - 1; r += 1) {
    fixtures[r].forEach((f, i) => {
      f.nextFixtureId = fixtures[r + 1][Math.floor(i / 2)].id;
      f.nextSlot = i % 2 === 0 ? 'white' : 'blue';
    });
  }
  fixtures[0].forEach((f, i) => {
    f.whiteTeamId = seededTeamIds[order[i * 2] - 1] ?? null;
    f.blueTeamId = seededTeamIds[order[i * 2 + 1] - 1] ?? null;
  });
  const flat = fixtures.flat();
  // Pases directos de la primera ronda.
  for (const f of fixtures[0]) {
    if (f.whiteTeamId && !f.blueTeamId) advanceWinner(flat, f, f.whiteTeamId, true);
    else if (!f.whiteTeamId && f.blueTeamId) advanceWinner(flat, f, f.blueTeamId, true);
  }
  return flat;
}

function advanceWinner(fixtures: Fixture[], f: Fixture, winnerTeamId: string, bye = false): void {
  f.winnerTeamId = winnerTeamId;
  if (bye) f.bye = true;
  const next = fixtures.find((x) => x.id === f.nextFixtureId);
  if (next && f.nextSlot) {
    if (f.nextSlot === 'white') next.whiteTeamId = winnerTeamId;
    else next.blueTeamId = winnerTeamId;
  }
}

/** Partido decisivo: la final añadida o la última ronda del cuadro. */
export function isFinalFixture(t: Tournament, f: Fixture): boolean {
  if (f.stage === 'final') return true;
  return t.format === 'bracket' && f.round === Math.max(...t.fixtures.map((x) => x.round));
}

export function createTournament(
  draft: TournamentDraft,
  players: Player[],
  eloOf: (id: string) => number,
  now: number,
  rnd: () => number = Math.random,
): Tournament {
  const errors = validateDraft(draft);
  if (errors.length) throw new Error(errors.join(' '));
  const bestOf = draft.finalBestOf && draft.finalBestOf > 1 ? draft.finalBestOf : undefined;
  let teams: TournamentTeam[];
  let fixtures: Fixture[];

  if (draft.format === 'pool') {
    const entrants = draft.entrants!;
    // Un «equipo» de 1 por jugador (clasificación y campeón) y las parejas que salgan en el sorteo.
    teams = entrants.map((id) => ({ id: newId('tt'), name: teamName([id], players), playerIds: [id] }));
    const pairTeam = (pair: string[]) => {
      const key = [...pair].sort().join('|');
      let tt = teams.find((x) => x.playerIds.length === 2 && [...x.playerIds].sort().join('|') === key);
      if (!tt) {
        tt = { id: newId('tt'), name: teamName(pair, players), playerIds: [...pair] };
        teams.push(tt);
      }
      return tt.id;
    };
    const rounds = schedulePool(entrants, draft.gamesPerPlayer!, draft.pairing ?? 'random', eloOf, rnd);
    fixtures = rounds.flatMap((r, i) =>
      r.matches.map(([w, b]) => ({
        id: newId('fx'),
        round: i + 1,
        whiteTeamId: pairTeam(w),
        blueTeamId: pairTeam(b),
        ...(r.resting.length ? { resting: [...r.resting] } : {}),
      })),
    );
  } else {
    teams = draft.teams.map((t) => ({ id: newId('tt'), name: teamName(t.playerIds, players), playerIds: [...t.playerIds] }));
    if (draft.seeding === 'elo') {
      const avg = (t: TournamentTeam) => t.playerIds.reduce((s, id) => s + eloOf(id), 0) / t.playerIds.length;
      teams = [...teams].sort((a, b) => avg(b) - avg(a));
    } else teams = shuffle(teams, rnd);
    const ids = teams.map((t) => t.id);
    fixtures = draft.format === 'league' ? roundRobin(ids) : buildBracket(ids);
    if (draft.format === 'bracket' && bestOf) {
      const last = Math.max(...fixtures.map((f) => f.round));
      fixtures.filter((f) => f.round === last).forEach((f) => (f.bestOf = bestOf));
    }
  }

  const final: TournamentFinal = draft.format === 'bracket' ? 'none' : draft.final ?? 'none';
  if (final !== 'none') {
    fixtures.push({
      id: newId('fx'),
      round: Math.max(0, ...fixtures.map((f) => f.round)) + 1,
      whiteTeamId: null,
      blueTeamId: null,
      stage: 'final',
      ...(bestOf ? { bestOf } : {}),
    });
  }

  const mode = draft.ranked ? 'ranked' : 'quick';
  return {
    formatVersion: STORAGE_FORMAT_VERSION,
    id: newId('t'),
    name: draft.name.trim(),
    format: draft.format,
    teamSize: draft.format === 'pool' ? 2 : draft.teamSize,
    ranked: draft.ranked,
    config: { ...draft.config, mode, testMode: false },
    teams,
    fixtures,
    status: 'active',
    createdAt: now,
    ...(draft.format === 'pool' ? { entrants: [...draft.entrants!] } : {}),
    ...(final !== 'none' ? { final } : {}),
    ...(draft.finalConfig ? { finalConfig: { ...draft.finalConfig, mode, testMode: false } } : {}),
    ...(draft.templateName ? { templateName: draft.templateName } : {}),
    ...(draft.templateId ? { templateId: draft.templateId } : {}),
    ...(draft.edition ? { edition: draft.edition } : {}),
  };
}

export function playableFixtures(t: Tournament): Fixture[] {
  if (t.status !== 'active') return [];
  return t.fixtures.filter((f) => f.whiteTeamId && f.blueTeamId && !f.winnerTeamId);
}

/** Siguiente partido: el primero pendiente por orden de jornada y de calendario. */
export function nextFixture(t: Tournament): Fixture | undefined {
  const order = new Map(t.fixtures.map((f, i) => [f.id, i]));
  return [...playableFixtures(t)].sort((a, b) => a.round - b.round || order.get(a.id)! - order.get(b.id)!)[0];
}

/** Partidos decididos y totales (sin contar pases directos). */
export function fixtureProgress(t: Tournament): { done: number; total: number } {
  const real = t.fixtures.filter((f) => !f.bye);
  return { done: real.filter((f) => f.winnerTeamId).length, total: real.length };
}

/** Reglas del partido de un cruce (la final puede tener las suyas). */
export function fixtureConfig(t: Tournament, f: Fixture): MatchConfig {
  return t.finalConfig && isFinalFixture(t, f) ? t.finalConfig : t.config;
}

export function fixtureParticipants(t: Tournament, f: Fixture, players: Player[]): ParticipantRef[] {
  const out: ParticipantRef[] = [];
  for (const [team, tid] of [['white', f.whiteTeamId], ['blue', f.blueTeamId]] as [Team, string | null][]) {
    const tt = t.teams.find((x) => x.id === tid);
    tt?.playerIds.forEach((pid, i) =>
      out.push({ playerId: pid, team, slot: (i + 1) as 1 | 2, nameSnapshot: players.find((p) => p.id === pid)?.name ?? '?' }),
    );
  }
  return out;
}

export interface StandingRow {
  team: TournamentTeam;
  played: number;
  wins: number;
  losses: number;
  points: number;
  goalsFor: number;
  goalsAgainst: number;
  diff: number;
}

/** Clasificación de la fase regular: por equipos (liguilla) o por jugador (Pool). */
export function standings(t: Tournament, matches: StoredMatch[]): StandingRow[] {
  return t.format === 'pool' ? poolStandings(t, matches) : leagueStandings(t, matches);
}

export function leagueStandings(t: Tournament, matches: StoredMatch[]): StandingRow[] {
  const byId = new Map(matches.map((m) => [m.id, m]));
  const rows = new Map<string, StandingRow>(t.teams.map((team) => [team.id, emptyRow(team)]));
  const h2h = new Map<string, string>(); // "a|b" → ganador
  for (const f of t.fixtures) {
    const m = f.matchId ? byId.get(f.matchId) : undefined;
    if (f.stage === 'final' || !m || !f.whiteTeamId || !f.blueTeamId || !f.winnerTeamId) continue;
    for (const [side, tid] of [['white', f.whiteTeamId], ['blue', f.blueTeamId]] as [Team, string][]) {
      addResult(rows.get(tid)!, m, side, f.winnerTeamId === tid);
    }
    h2h.set([f.whiteTeamId, f.blueTeamId].sort().join('|'), f.winnerTeamId);
  }
  return [...rows.values()].sort((a, b) => {
    const c = compareRows(a, b);
    if (c) return c;
    const w = h2h.get([a.team.id, b.team.id].sort().join('|'));
    if (w === a.team.id) return -1;
    if (w === b.team.id) return 1;
    return a.team.name.localeCompare(b.team.name, 'es');
  });
}

/** Pool: cada jugador suma lo que haga su pareja de ese partido. */
export function poolStandings(t: Tournament, matches: StoredMatch[]): StandingRow[] {
  const byId = new Map(matches.map((m) => [m.id, m]));
  const solo = (pid: string) => t.teams.find((x) => x.playerIds.length === 1 && x.playerIds[0] === pid)!;
  const rows = new Map<string, StandingRow>((t.entrants ?? []).map((pid) => [pid, emptyRow(solo(pid))]));
  for (const f of t.fixtures) {
    const m = f.matchId ? byId.get(f.matchId) : undefined;
    if (f.stage === 'final' || !m || !f.winnerTeamId) continue;
    for (const [side, tid] of [['white', f.whiteTeamId], ['blue', f.blueTeamId]] as [Team, string | null][]) {
      for (const pid of t.teams.find((x) => x.id === tid)?.playerIds ?? []) {
        const r = rows.get(pid);
        if (r) addResult(r, m, side, f.winnerTeamId === tid);
      }
    }
  }
  return [...rows.values()].sort((a, b) => compareRows(a, b) || a.team.name.localeCompare(b.team.name, 'es'));
}

function emptyRow(team: TournamentTeam): StandingRow {
  return { team, played: 0, wins: 0, losses: 0, points: 0, goalsFor: 0, goalsAgainst: 0, diff: 0 };
}

function addResult(r: StandingRow, m: StoredMatch, side: Team, won: boolean): void {
  const opp: Team = side === 'white' ? 'blue' : 'white';
  r.played += 1;
  r.goalsFor += m.result.score[side];
  r.goalsAgainst += m.result.score[opp];
  r.diff = r.goalsFor - r.goalsAgainst;
  if (won) {
    r.wins += 1;
    r.points += 3;
  } else r.losses += 1;
}

function compareRows(a: StandingRow, b: StandingRow): number {
  return b.points - a.points || b.diff - a.diff || b.goalsFor - a.goalsFor;
}

/** Registra el resultado de un partido del torneo. Devuelve el torneo actualizado. */
export function recordFixtureResult(t: Tournament, fixtureId: string, match: StoredMatch, matches: StoredMatch[]): Tournament {
  const next: Tournament = { ...t, teams: [...t.teams], fixtures: t.fixtures.map((x) => ({ ...x })) };
  const f = next.fixtures.find((x) => x.id === fixtureId);
  if (!f || f.winnerTeamId || !f.whiteTeamId || !f.blueTeamId) return t;
  if (f.matchId === match.id || f.matchIds?.includes(match.id)) return t;
  const side = match.result.winner;
  f.matchId = match.id;
  if ((f.bestOf ?? 1) > 1) {
    f.matchIds = [...(f.matchIds ?? []), match.id];
    const s = { white: f.series?.white ?? 0, blue: f.series?.blue ?? 0 };
    s[side] += 1;
    f.series = s;
    // La serie sigue hasta que uno llegue a las victorias necesarias.
    if (s[side] < Math.ceil(f.bestOf! / 2)) return next;
  }
  advanceWinner(next.fixtures, f, side === 'white' ? f.whiteTeamId : f.blueTeamId);
  const all = [...matches.filter((m) => m.id !== match.id), match];

  if (isFinalFixture(next, f)) {
    finish(next, f.winnerTeamId!, match);
    return next;
  }
  const regularDone = next.fixtures.every((x) => x.stage === 'final' || x.winnerTeamId || (!x.whiteTeamId && !x.blueTeamId));
  if (next.format === 'bracket' || !regularDone) return next;

  const table = standings(next, all);
  const finalFx = next.fixtures.find((x) => x.stage === 'final');
  if (!finalFx || !next.final || next.final === 'none') {
    // Sin final: gana el líder. El premio va en su último partido del torneo.
    const leader = table[0].team;
    const theirs = next.fixtures
      .filter((x) => x.matchId && [x.whiteTeamId, x.blueTeamId].some((id) => id && next.teams.find((tt) => tt.id === id)?.playerIds.some((p) => leader.playerIds.includes(p))))
      .map((x) => all.find((m) => m.id === x.matchId))
      .filter((m): m is StoredMatch => !!m)
      .sort((a, b) => b.finishedAt - a.finishedAt);
    finish(next, leader.id, theirs[0] ?? match);
    next.finishedAt = match.finishedAt;
    return next;
  }
  if (next.final === 'top4' && table.length >= 4) {
    finalFx.whiteTeamId = ensureTeam(next, [table[0].team, table[3].team]);
    finalFx.blueTeamId = ensureTeam(next, [table[1].team, table[2].team]);
  } else {
    finalFx.whiteTeamId = table[0].team.id;
    finalFx.blueTeamId = table[1].team.id;
  }
  return next;
}

/** Equipo con los jugadores de varios equipos de 1 (lo crea si no existe). */
function ensureTeam(t: Tournament, solos: TournamentTeam[]): string {
  const ids = solos.flatMap((s) => s.playerIds);
  const key = [...ids].sort().join('|');
  const found = t.teams.find((x) => [...x.playerIds].sort().join('|') === key);
  if (found) return found.id;
  const tt: TournamentTeam = { id: newId('tt'), name: solos.map((s) => s.name).join(' + '), playerIds: ids };
  t.teams.push(tt);
  return tt.id;
}

function finish(t: Tournament, winnerTeamId: string, prizeMatch: StoredMatch): void {
  t.status = 'finished';
  t.winnerTeamId = winnerTeamId;
  t.finishedAt = prizeMatch.finishedAt;
  t.finalMatchId = prizeMatch.id;
}

export function roundLabel(t: Tournament, round: number): string {
  const fixtures = t.fixtures.filter((f) => f.round === round);
  if (fixtures.some((f) => f.stage === 'final')) return 'Final';
  if (t.format === 'league') return `Jornada ${round}`;
  if (t.format === 'pool') return `Ronda ${round}`;
  const total = Math.max(...t.fixtures.map((f) => f.round));
  const fromEnd = total - round;
  return fromEnd === 0 ? 'Final' : fromEnd === 1 ? 'Semifinales' : fromEnd === 2 ? 'Cuartos' : `Ronda ${round}`;
}

/** Resultado para mostrar: marcador o, en series, victorias de cada lado. */
export function fixtureScore(f: Fixture, byId: Map<string, StoredMatch>): string | null {
  if ((f.bestOf ?? 1) > 1 && f.series) return `${f.series.white}–${f.series.blue}`;
  const m = f.matchId ? byId.get(f.matchId) : undefined;
  if (!m) return null;
  return `${m.result.score.white}–${m.result.score.blue}${m.result.penaltyScore ? ` (p ${m.result.penaltyScore.white}–${m.result.penaltyScore.blue})` : ''}`;
}
