/**
 * Torneos (propuesta «torneos-2», pendiente de aprobación):
 * - Liga: todos contra todos a una vuelta (o ida y vuelta), 3 a 8 equipos. Puntos por victoria,
 *   empate y derrota configurables (3/1/0 por defecto); a tiempo se puede empatar, sin prórroga ni
 *   penaltis. Desempate: puntos → diferencia de goles → goles a favor → enfrentamiento directo → nombre.
 *   Opcional: final entre los dos primeros (la final nunca acaba en empate).
 * - Cuadro: eliminación directa para 3 a 8 equipos; los huecos dan pase directo.
 *   Siembra por ELO (1 contra el último) o aleatoria.
 * - Pool: 2 a 8 parejas en una escalera de pistas; se sube ganando y se baja perdiendo, con o sin
 *   cambio de pareja, por tiempo o por rondas, y acaba con la final en la pista 1 (ver ladder.ts).
 *   Los Pool antiguos (parejas rotativas con calendario fijo, pool.ts) se siguen pudiendo terminar.
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
  type TournamentPoints,
  type TournamentTeam,
} from '../persistence';
import { POOL_MAX_PLAYERS, POOL_MIN_PLAYERS, poolGameOptions, schedulePool } from './pool';
import { LADDER_MAX_PAIRS, LADDER_MIN_PAIRS, ladderAfterResult, ladderPlaces, ladderRoundFixtures, startLineup } from './ladder';

export const TOURNAMENT_RULES_VERSION = 'torneos-2';
export const MIN_TEAMS = 3;
export const MAX_TEAMS = 8;
/** Puntos por defecto: victoria 3, empate 1, derrota 0. */
export const DEFAULT_POINTS: TournamentPoints = { win: 3, draw: 1, loss: 0 };

/** ¿Ya se ha jugado este cruce? (con ganador o, en liga, en empate). */
export function fixtureDone(f: Fixture): boolean {
  return !!(f.winnerTeamId || f.draw);
}

export const FORMAT_LABEL: Record<TournamentFormat, string> = {
  league: 'Liga',
  bracket: 'Copa',
  pool: 'Pool',
};

/** Lo que decide cómo se acaba un partido. */
export type MatchRules = Pick<MatchConfig, 'endCondition' | 'goalsPerPeriod' | 'minutesPerPeriod'>;

/** «a 5 goles», «5 min», «a 5 goles o 5 min». */
export function rulesShort(r: MatchRules): string {
  if (r.endCondition === 'time') return `${r.minutesPerPeriod} min`;
  if (r.endCondition === 'both') return `a ${r.goalsPerPeriod} goles o ${r.minutesPerPeriod} min`;
  return `a ${r.goalsPerPeriod} goles`;
}

export function sameRules(a: MatchRules, b: MatchRules): boolean {
  return (
    a.endCondition === b.endCondition &&
    (a.endCondition === 'time' || a.goalsPerPeriod === b.goalsPerPeriod) &&
    (a.endCondition === 'goals' || a.minutesPerPeriod === b.minutesPerPeriod)
  );
}

export interface TournamentDraft {
  name: string;
  format: TournamentFormat;
  teamSize: 1 | 2;
  ranked: boolean;
  config: MatchConfig;
  /** Liguilla y cuadro: equipos ya formados (con nombre y logo si son un equipo guardado). */
  teams: { playerIds: string[]; name?: string; logo?: string; clubId?: string }[];
  seeding: 'elo' | 'random';
  /** Pool: jugadores inscritos. */
  entrants?: string[];
  /** Pool: partidos por jugador (debe ser un valor de `poolGameOptions`). */
  gamesPerPlayer?: number;
  /** Pool: parejas equilibradas por ELO o al azar. */
  pairing?: 'elo' | 'random';
  /** Pool en escalera (las parejas de salida van en `teams`). */
  ladder?: { swap: boolean; limit: 'time' | 'rounds'; minutes: number; rounds: number };
  /** Liga: a una vuelta o ida y vuelta. */
  legs?: 1 | 2;
  /** Liga: puntos por victoria, empate y derrota. */
  points?: TournamentPoints;
  /** Título que se lleva el último. */
  lastTitle?: string;
  final?: TournamentFinal;
  finalBestOf?: number;
  /** Reglas de la final si difieren. */
  finalConfig?: MatchConfig;
  templateName?: string;
  templateId?: string;
  edition?: number;
  /** Nombre propio de la competición, logo, banner y copa. */
  competition?: string;
  logo?: string;
  banner?: string;
  cup?: string;
}

export function validateDraft(d: TournamentDraft): string[] {
  const errors: string[] = [];
  if (!d.name.trim()) errors.push('Ponle un nombre al torneo.');
  if (d.format === 'pool' && d.ladder) {
    const all = d.teams.flatMap((t) => t.playerIds);
    if (d.teams.length < LADDER_MIN_PAIRS || d.teams.length > LADDER_MAX_PAIRS)
      errors.push(`El Pool necesita entre ${LADDER_MIN_PAIRS} y ${LADDER_MAX_PAIRS} parejas (${LADDER_MIN_PAIRS * 2} a ${LADDER_MAX_PAIRS * 2} jugadores).`);
    else if (d.teams.some((t) => t.playerIds.length !== 2)) errors.push('En el Pool se juega por parejas: hace falta un número par de jugadores.');
    else if (new Set(all).size !== all.length) errors.push('Un jugador no puede estar en dos parejas.');
    if (d.ladder.limit === 'time' ? !(d.ladder.minutes >= 5) : !(d.ladder.rounds >= 1)) errors.push('Pon cuánto dura el Pool.');
    return errors;
  }
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

/**
 * Emparejamientos todos contra todos por el método del círculo. Con `legs` 2, la vuelta repite las
 * mismas jornadas con los lados cambiados (quien fue Blanco en la ida es Azul en la vuelta).
 */
export function roundRobin(teamIds: string[], legs: 1 | 2 = 1): Fixture[] {
  const ida = singleRoundRobin(teamIds);
  if (legs === 1) return ida;
  const rounds = Math.max(0, ...ida.map((f) => f.round));
  const vuelta = ida.map((f) => ({ id: newId('fx'), round: f.round + rounds, whiteTeamId: f.blueTeamId, blueTeamId: f.whiteTeamId }));
  return [...ida, ...vuelta];
}

function singleRoundRobin(teamIds: string[]): Fixture[] {
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
  // Con ELO no juegan invitados: moverían el ELO de los demás contra alguien que no lo tiene.
  const ids = [...draft.teams.flatMap((x) => x.playerIds), ...(draft.entrants ?? [])];
  if (draft.ranked && ids.some((id) => players.find((p) => p.id === id)?.guest)) errors.push('Un torneo con ELO no admite invitados.');
  if (errors.length) throw new Error(errors.join(' '));
  if (draft.format === 'pool' && draft.ladder) return createLadder(draft, players, now, rnd);
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
    teams = draft.teams.map((t) => ({
      id: newId('tt'),
      name: t.name?.trim() || teamName(t.playerIds, players),
      playerIds: [...t.playerIds],
      ...(t.logo ? { logo: t.logo } : {}),
      ...(t.clubId ? { clubId: t.clubId } : {}),
    }));
    if (draft.seeding === 'elo') {
      const avg = (t: TournamentTeam) => t.playerIds.reduce((s, id) => s + eloOf(id), 0) / t.playerIds.length;
      teams = [...teams].sort((a, b) => avg(b) - avg(a));
    } else teams = shuffle(teams, rnd);
    const ids = teams.map((t) => t.id);
    fixtures = draft.format === 'league' ? roundRobin(ids, draft.legs ?? 1) : buildBracket(ids);
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
    ...(draft.format === 'league' && draft.points ? { points: { ...draft.points } } : {}),
    ...(draft.lastTitle?.trim() ? { lastTitle: draft.lastTitle.trim() } : {}),
    ...(final !== 'none' ? { final } : {}),
    ...(draft.finalConfig ? { finalConfig: { ...draft.finalConfig, mode, testMode: false } } : {}),
    ...(draft.templateName ? { templateName: draft.templateName } : {}),
    ...(draft.templateId ? { templateId: draft.templateId } : {}),
    ...(draft.edition ? { edition: draft.edition } : {}),
    ...(draft.competition?.trim() ? { competition: draft.competition.trim() } : {}),
    ...(draft.logo ? { logo: draft.logo } : {}),
    ...(draft.banner ? { banner: draft.banner } : {}),
    ...(draft.cup ? { cup: draft.cup } : {}),
  };
}

/** Pool en escalera: las parejas de salida se colocan en las pistas al azar y empieza la ronda 1. */
function createLadder(draft: TournamentDraft, players: Player[], now: number, rnd: () => number): Tournament {
  const mode = draft.ranked ? 'ranked' : 'quick';
  const entrants = draft.teams.flatMap((x) => x.playerIds);
  const pairs = shuffle(draft.teams, rnd);
  const finalBestOf = draft.finalBestOf && draft.finalBestOf > 1 ? draft.finalBestOf : 1;
  const t: Tournament = {
    formatVersion: STORAGE_FORMAT_VERSION,
    id: newId('t'),
    name: draft.name.trim(),
    format: 'pool',
    teamSize: 2,
    ranked: draft.ranked,
    config: { ...draft.config, mode, testMode: false },
    // Un «equipo» de 1 por jugador (sus nombres) y las parejas de salida, con su nombre y escudo si los tienen.
    teams: [
      ...entrants.map((id) => ({ id: newId('tt'), name: teamName([id], players), playerIds: [id] })),
      ...pairs.map((p) => ({
        id: newId('tt'),
        name: p.name?.trim() || teamName(p.playerIds, players),
        playerIds: [...p.playerIds],
        ...(p.logo ? { logo: p.logo } : {}),
        ...(p.clubId ? { clubId: p.clubId } : {}),
      })),
    ],
    fixtures: [],
    status: 'active',
    createdAt: now,
    entrants,
    ladder: { ...draft.ladder!, finalBestOf, start: pairs.map((p) => [...p.playerIds]) },
    ...(draft.lastTitle?.trim() ? { lastTitle: draft.lastTitle.trim() } : {}),
    ...(draft.finalConfig ? { finalConfig: { ...draft.finalConfig, mode, testMode: false } } : {}),
    ...(draft.templateName ? { templateName: draft.templateName } : {}),
    ...(draft.templateId ? { templateId: draft.templateId } : {}),
    ...(draft.edition ? { edition: draft.edition } : {}),
    ...(draft.competition?.trim() ? { competition: draft.competition.trim() } : {}),
    ...(draft.logo ? { logo: draft.logo } : {}),
    ...(draft.banner ? { banner: draft.banner } : {}),
    ...(draft.cup ? { cup: draft.cup } : {}),
  };
  t.fixtures = firstLadderRound(t);
  return t;
}

/** Ronda 1 del Pool en escalera (si solo se juega una ronda, su pista 1 ya es la final). */
function firstLadderRound(t: Tournament): Fixture[] {
  const s = t.ladder!;
  return ladderRoundFixtures(t, 1, startLineup(s.start), s.limit === 'rounds' && s.rounds <= 1);
}

export function playableFixtures(t: Tournament): Fixture[] {
  if (t.status !== 'active') return [];
  return t.fixtures.filter((f) => f.whiteTeamId && f.blueTeamId && !fixtureDone(f) && !f.skipped);
}

/** Siguiente partido: el primero pendiente por orden de jornada y de calendario. */
export function nextFixture(t: Tournament): Fixture | undefined {
  const order = new Map(t.fixtures.map((f, i) => [f.id, i]));
  return [...playableFixtures(t)].sort((a, b) => a.round - b.round || order.get(a.id)! - order.get(b.id)!)[0];
}

/** Partidos decididos y totales (sin contar pases directos). */
export function fixtureProgress(t: Tournament): { done: number; total: number } {
  const real = t.fixtures.filter((f) => !f.bye && !f.skipped);
  return { done: real.filter(fixtureDone).length, total: real.length };
}

/** Reglas del partido de un cruce (la final puede tener las suyas, y nunca acaba en empate). */
export function fixtureConfig(t: Tournament, f: Fixture): MatchConfig {
  if (!isFinalFixture(t, f)) return t.config;
  const { allowDraw: _d, ...cfg } = t.finalConfig ?? t.config;
  return cfg;
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
  draws: number;
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

export type Movement = 'up' | 'down' | 'same';

/**
 * Cómo ha cambiado cada puesto con el último partido jugado de la fase regular:
 * se compara la clasificación de antes de ese partido con la de ahora. Vacío si aún no se ha jugado.
 */
export function standingsMovement(t: Tournament, matches: StoredMatch[]): Map<string, Movement> {
  const byId = new Map(matches.map((m) => [m.id, m]));
  const played = t.fixtures
    .filter((f) => f.stage !== 'final' && f.matchId && byId.has(f.matchId))
    .map((f) => byId.get(f.matchId!)!)
    .sort((a, b) => a.finishedAt - b.finishedAt);
  const out = new Map<string, Movement>();
  if (!played.length) return out;
  const lastId = played[played.length - 1].id;
  const before = standings(t, matches.filter((m) => m.id !== lastId)).map((r) => r.team.id);
  standings(t, matches).forEach((r, i) => {
    const was = before.indexOf(r.team.id);
    out.set(r.team.id, was > i ? 'up' : was < i ? 'down' : 'same');
  });
  return out;
}

export function leagueStandings(t: Tournament, matches: StoredMatch[]): StandingRow[] {
  const byId = new Map(matches.map((m) => [m.id, m]));
  const pts = t.points ?? DEFAULT_POINTS;
  const rows = new Map<string, StandingRow>(t.teams.map((team) => [team.id, emptyRow(team)]));
  // Enfrentamiento directo: puntos que ha sacado cada equipo contra el otro (con ida y vuelta, sumados).
  const h2h = new Map<string, number>(); // "equipo>rival" → puntos
  for (const f of t.fixtures) {
    const m = f.matchId ? byId.get(f.matchId) : undefined;
    if (f.stage === 'final' || !m || !f.whiteTeamId || !f.blueTeamId || !fixtureDone(f)) continue;
    for (const [side, tid, other] of [['white', f.whiteTeamId, f.blueTeamId], ['blue', f.blueTeamId, f.whiteTeamId]] as [Team, string, string][]) {
      const outcome = f.draw ? 'draw' : f.winnerTeamId === tid ? 'win' : 'loss';
      addResult(rows.get(tid)!, m, side, outcome, pts);
      h2h.set(`${tid}>${other}`, (h2h.get(`${tid}>${other}`) ?? 0) + pts[outcome]);
    }
  }
  return [...rows.values()].sort((a, b) => {
    const c = compareRows(a, b);
    if (c) return c;
    const d = (h2h.get(`${b.team.id}>${a.team.id}`) ?? 0) - (h2h.get(`${a.team.id}>${b.team.id}`) ?? 0);
    if (d) return d;
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
    if (f.stage === 'final' || !m || !fixtureDone(f)) continue;
    for (const [side, tid] of [['white', f.whiteTeamId], ['blue', f.blueTeamId]] as [Team, string | null][]) {
      for (const pid of t.teams.find((x) => x.id === tid)?.playerIds ?? []) {
        const r = rows.get(pid);
        if (r) addResult(r, m, side, f.draw ? 'draw' : f.winnerTeamId === tid ? 'win' : 'loss', DEFAULT_POINTS);
      }
    }
  }
  return [...rows.values()].sort((a, b) => compareRows(a, b) || a.team.name.localeCompare(b.team.name, 'es'));
}

function emptyRow(team: TournamentTeam): StandingRow {
  return { team, played: 0, wins: 0, draws: 0, losses: 0, points: 0, goalsFor: 0, goalsAgainst: 0, diff: 0 };
}

function addResult(r: StandingRow, m: StoredMatch, side: Team, outcome: 'win' | 'draw' | 'loss', pts: TournamentPoints): void {
  const opp: Team = side === 'white' ? 'blue' : 'white';
  r.played += 1;
  r.goalsFor += m.result.score[side];
  r.goalsAgainst += m.result.score[opp];
  r.diff = r.goalsFor - r.goalsAgainst;
  r.points += pts[outcome];
  if (outcome === 'win') r.wins += 1;
  else if (outcome === 'draw') r.draws += 1;
  else r.losses += 1;
}

function compareRows(a: StandingRow, b: StandingRow): number {
  return b.points - a.points || b.diff - a.diff || b.goalsFor - a.goalsFor;
}

/** Registra el resultado de un partido del torneo. Devuelve el torneo actualizado. */
export function recordFixtureResult(t: Tournament, fixtureId: string, match: StoredMatch, matches: StoredMatch[]): Tournament {
  const next: Tournament = { ...t, teams: [...t.teams], fixtures: t.fixtures.map((x) => ({ ...x })) };
  const f = next.fixtures.find((x) => x.id === fixtureId);
  if (!f || fixtureDone(f) || f.skipped || !f.whiteTeamId || !f.blueTeamId) return t;
  if (f.matchId === match.id || f.matchIds?.includes(match.id)) return t;
  const side = match.result.winner;
  // Un empate solo vale en la fase regular de una liga; en una eliminatoria o una final no cuenta.
  if (!side && (next.format !== 'league' || isFinalFixture(next, f) || (f.bestOf ?? 1) > 1)) return t;
  f.matchId = match.id;
  delete f.abandonedAt;
  if (!side) f.draw = true;
  else if ((f.bestOf ?? 1) > 1) {
    f.matchIds = [...(f.matchIds ?? []), match.id];
    const s = { white: f.series?.white ?? 0, blue: f.series?.blue ?? 0 };
    s[side] += 1;
    f.series = s;
    // La serie sigue hasta que uno llegue a las victorias necesarias.
    if (s[side] < Math.ceil(f.bestOf! / 2)) return next;
  }
  if (side) advanceWinner(next.fixtures, f, side === 'white' ? f.whiteTeamId : f.blueTeamId);
  const all = [...matches.filter((m) => m.id !== match.id), match];

  if (isFinalFixture(next, f)) {
    finish(next, f.winnerTeamId!, match);
    return next;
  }
  if (next.ladder) {
    ladderAfterResult(next, f.round, all);
    return next;
  }
  const regularDone = next.fixtures.every((x) => x.stage === 'final' || fixtureDone(x) || (!x.whiteTeamId && !x.blueTeamId));
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
  // En la escalera, la final es la pista 1 de una ronda que también tiene otras pistas.
  if (t.ladder) return `Ronda ${round}`;
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

/** Nivel de la fase: decide el color y la ornamentación del cartel. */
export type PhaseTier = 'final' | 'semi' | 'quarter' | 'eighth' | 'round';

export interface PhaseInfo {
  tier: PhaseTier;
  /** Título grande: «GRAN FINAL», «SEMIFINAL», «CUARTOS DE FINAL», «JORNADA 3»… */
  title: string;
  /** Debajo: serie («PARTIDO 2 · AL MEJOR DE 3», «¡PARTIDO DECISIVO!») o nada. */
  sub?: string;
}

/** Cartel de la fase de un partido de torneo. */
export function phaseInfo(t: Tournament, f: Fixture): PhaseInfo {
  let tier: PhaseTier = 'round';
  let title = roundLabel(t, f.round).toUpperCase();
  if (f.stage === 'final') {
    tier = 'final';
    title = 'GRAN FINAL';
  } else if (t.ladder && f.court) {
    title = `PISTA ${f.court} · ${title}`;
  } else if (t.format === 'bracket') {
    const fromEnd = Math.max(...t.fixtures.map((x) => x.round)) - f.round;
    const names: [PhaseTier, string][] = [
      ['final', 'GRAN FINAL'],
      ['semi', 'SEMIFINAL'],
      ['quarter', 'CUARTOS DE FINAL'],
      ['eighth', 'OCTAVOS DE FINAL'],
    ];
    if (fromEnd < names.length) [tier, title] = names[fromEnd];
  }
  let sub: string | undefined;
  const bestOf = f.bestOf ?? 1;
  if (bestOf > 1) {
    const w = f.series?.white ?? 0;
    const b = f.series?.blue ?? 0;
    const need = Math.ceil(bestOf / 2);
    sub = w === need - 1 && b === need - 1 ? '¡PARTIDO DECISIVO!' : `PARTIDO ${w + b + 1} · AL MEJOR DE ${bestOf}`;
  }
  return { tier, title, ...(sub ? { sub } : {}) };
}

/** Partidos ya apuntados en el torneo (los de las series incluidos), del más antiguo al último. */
export function recordedMatches(t: Tournament, matches: StoredMatch[]): StoredMatch[] {
  const ids = new Set(t.fixtures.flatMap((f) => f.matchIds ?? (f.matchId ? [f.matchId] : [])));
  return matches.filter((m) => ids.has(m.id) && m.tournament?.id === t.id).sort((a, b) => a.finishedAt - b.finishedAt);
}

/**
 * El torneo como si ese partido no se hubiera jugado: se vuelve al cuadro/calendario inicial y se
 * apuntan de nuevo, en orden, todos los demás resultados. Sirve para repetir un partido mal apuntado.
 */
export function undoMatch(t: Tournament, matchId: string, matches: StoredMatch[]): Tournament {
  if (t.ladder) {
    // Se vuelve a la ronda 1 y se apuntan los demás: las rondas siguientes salen iguales (sorteos con semilla).
    const { winnerTeamId: _w, finishedAt: _f, finalMatchId: _fm, ...base } = t;
    let next: Tournament = { ...base, status: 'active', teams: [...t.teams], fixtures: [] };
    next.fixtures = firstLadderRound(next);
    const played: StoredMatch[] = [];
    for (const m of recordedMatches(t, matches)) {
      if (m.id === matchId) continue;
      played.push(m);
      next = recordFixtureResult(next, m.tournament!.fixtureId, m, played);
    }
    return next;
  }
  const fixtures: Fixture[] = t.fixtures.map((f) => {
    if (f.bye) return { ...f };
    const { winnerTeamId: _w, draw: _d, matchId: _m, matchIds: _ids, series: _s, ...rest } = f;
    // Rondas siguientes del cuadro y final: sus equipos salen de los resultados.
    if (f.stage === 'final' || (t.format === 'bracket' && f.round > 1)) return { ...rest, whiteTeamId: null, blueTeamId: null };
    return rest;
  });
  for (const f of fixtures.filter((x) => x.bye)) advanceWinner(fixtures, f, f.winnerTeamId!, true);
  const { winnerTeamId: _w, finishedAt: _f, finalMatchId: _fm, ...base } = t;
  let next: Tournament = { ...base, status: 'active', fixtures };
  const played: StoredMatch[] = [];
  for (const m of recordedMatches(t, matches)) {
    if (m.id === matchId) continue;
    played.push(m);
    next = recordFixtureResult(next, m.tournament!.fixtureId, m, played);
  }
  return next;
}

/**
 * Cambia las reglas de lo que queda por jugar. Los partidos ya jugados no cambian.
 * La serie de la final solo cambia (1 partido / al mejor de 3) si aún no ha empezado.
 */
export function updateTournamentRules(t: Tournament, rules: MatchRules, finalRules: MatchRules | null, finalBestOf?: 1 | 3): Tournament {
  const config: MatchConfig = { ...t.config, ...rules };
  const finalConfig = finalRules && !sameRules(rules, finalRules) ? { ...config, ...finalRules } : undefined;
  const fixtures = t.fixtures.map((f) => {
    if (!finalBestOf || !isFinalFixture(t, f) || f.winnerTeamId || f.matchIds?.length) return f;
    const { bestOf: _b, ...rest } = f;
    return finalBestOf > 1 ? { ...rest, bestOf: finalBestOf } : rest;
  });
  const { finalConfig: _old, ...base } = t;
  // En la escalera la final aún no existe: se apunta para cuando toque.
  const ladder = t.ladder && finalBestOf && finalSeriesEditable(t) ? { ladder: { ...t.ladder, finalBestOf } } : {};
  return { ...base, config, fixtures, ...ladder, ...(finalConfig ? { finalConfig } : {}) };
}

/** ¿Se puede cambiar todavía si la final es a 1 partido o al mejor de 3? */
export function finalSeriesEditable(t: Tournament): boolean {
  return t.status === 'active' && t.fixtures.filter((f) => isFinalFixture(t, f)).every((f) => !f.winnerTeamId && !f.matchIds?.length);
}

/** Marca un partido del torneo como abandonado a medias (sigue pendiente). */
export function markAbandoned(t: Tournament, fixtureId: string, now: number): Tournament {
  return { ...t, fixtures: t.fixtures.map((f) => (f.id === fixtureId && !fixtureDone(f) ? { ...f, abandonedAt: now } : f)) };
}

export interface FinalPlace {
  /** Equipo (liguilla y cuadro) o jugador (Pool, su equipo de 1). */
  team: TournamentTeam;
  place: number;
  champion: boolean;
}

/**
 * Clasificación final del torneo, del campeón al último.
 * - Liguilla y Pool: la clasificación de la fase regular; si hubo final, sus finalistas van delante
 *   (primero los campeones y luego los que la perdieron).
 * - Cuadro: por la ronda a la que llegó cada uno; a igual ronda, por victorias y goles.
 */
export function finalRanking(t: Tournament, matches: StoredMatch[]): FinalPlace[] {
  const winner = t.teams.find((x) => x.id === t.winnerTeamId);
  if (t.ladder) return ladderPlaces(t).map((p) => ({ ...p, champion: p.team.id === t.winnerTeamId }));
  let order = (t.format === 'bracket' ? leagueStandings(t, matches) : standings(t, matches)).map((r) => r.team);
  if (t.format === 'bracket') {
    // Ronda más alta a la que llegó cada uno (el campeón, el primero); el orden estable deja el desempate.
    const reached = (id: string) =>
      id === t.winnerTeamId ? Infinity : Math.max(0, ...t.fixtures.filter((f) => !f.bye && (f.whiteTeamId === id || f.blueTeamId === id)).map((f) => f.round));
    order = [...order].sort((a, b) => reached(b.id) - reached(a.id));
  } else {
    const fin = t.fixtures.find((f) => f.stage === 'final' && f.winnerTeamId);
    if (fin && winner) {
      const loserId = fin.winnerTeamId === fin.whiteTeamId ? fin.blueTeamId : fin.whiteTeamId;
      const loser = t.teams.find((x) => x.id === loserId)?.playerIds ?? [];
      const within = (tt: TournamentTeam, ids: string[]) => tt.playerIds.every((p) => ids.includes(p));
      const rank = (tt: TournamentTeam) => (within(tt, winner.playerIds) ? 0 : within(tt, loser) ? 1 : 2);
      order = [...order].sort((a, b) => rank(a) - rank(b));
    }
  }
  return order.map((team, i) => ({
    team,
    place: i + 1,
    champion: !!winner && team.playerIds.every((p) => winner.playerIds.includes(p)),
  }));
}
