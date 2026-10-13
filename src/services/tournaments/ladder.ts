/**
 * Pool en escalera de pistas.
 * - Una pista por cada dos parejas; la 1 es la mejor y cada una hacia abajo es peor. Con número impar de
 *   parejas, la que sobra espera en el banquillo, debajo de la última pista.
 * - Cada ronda se juega un partido en cada pista, de la última a la 1 (la 1 cierra la ronda).
 * - Al acabar la ronda, el que gana sube una pista y el que pierde baja una. Los que ganan en la 1 se
 *   quedan arriba; los que pierden en la última se quedan abajo (o salen al banquillo, y los que
 *   esperaban entran en la última pista).
 * - Con cambio de pareja, en cada pista las dos parejas se deshacen: cada jugador va con uno de la otra
 *   pareja, por sorteo. Sin cambio, las parejas son las mismas todo el Pool.
 * - Por rondas, la pista 1 de la última ronda es la final. Por tiempo, cuando ya solo da para un partido
 *   (lo que queda es menos que dos partidos de media), el siguiente es la final en la pista 1 y los de las
 *   otras pistas de esa ronda ya no se juegan.
 * Todo sale del orden de salida y de los resultados (con sorteos de semilla fija), así que borrar el último
 * resultado y volver a apuntar el resto da exactamente las mismas rondas.
 */
import type { Fixture, LadderSettings, StoredMatch, Tournament, TournamentTeam } from '../persistence';
import { newId } from '../ids';
import { seededRandom } from '../statistics/calendar';

export const LADDER_MIN_PAIRS = 2;
export const LADDER_MAX_PAIRS = 8;
/** Duraciones y rondas que se ofrecen en el creador. */
export const LADDER_MINUTES = [20, 30, 45, 60] as const;
export const LADDER_ROUNDS = [3, 4, 5, 6, 8, 10] as const;
export const LADDER_DEFAULTS = { swap: true, limit: 'time' as const, minutes: 30, rounds: 5 };

/** Una pista: las dos parejas que juegan en ella (la primera, de Blanco). */
export type Court = [string[], string[]];

export interface Lineup {
  courts: Court[];
  /** Pareja que espera (solo con número impar de parejas). */
  bench: string[] | null;
}

const done = (f: Fixture) => !!(f.winnerTeamId || f.draw);
const pairKey = (ids: string[]) => [...ids].sort().join('|');

/** Pistas que salen con `pairs` parejas, y si hay banquillo. */
export function ladderShape(pairs: number): { courts: number; bench: boolean } {
  return { courts: Math.floor(pairs / 2), bench: pairs % 2 === 1 };
}

/** Pistas de la primera ronda: las parejas de salida en su orden. */
export function startLineup(start: string[][]): Lineup {
  const { courts, bench } = ladderShape(start.length);
  return {
    courts: Array.from({ length: courts }, (_, i) => [[...start[i * 2]], [...start[i * 2 + 1]]] as Court),
    bench: bench ? [...start[start.length - 1]] : null,
  };
}

/** Equipo de esa pareja en el torneo (lo crea si aún no existe; el nombre, con los de cada jugador). */
export function ensurePairTeam(t: Tournament, ids: string[]): TournamentTeam {
  const key = pairKey(ids);
  const found = t.teams.find((x) => x.playerIds.length === ids.length && pairKey(x.playerIds) === key);
  if (found) return found;
  const nameOf = (id: string) => t.teams.find((x) => x.playerIds.length === 1 && x.playerIds[0] === id)?.name ?? '?';
  const tt: TournamentTeam = { id: newId('tt'), name: ids.map(nameOf).join(' + '), playerIds: [...ids] };
  t.teams.push(tt);
  return tt;
}

/**
 * Partidos de una ronda, de la última pista a la 1. Con `final`, el de la pista 1 es la final. Crea en
 * `t.teams` las parejas que aún no existan (también la del banquillo, que sale en la clasificación final).
 */
export function ladderRoundFixtures(t: Tournament, round: number, lineup: Lineup, final = false): Fixture[] {
  const settings = t.ladder!;
  const resting = lineup.bench ? { resting: [...lineup.bench] } : {};
  if (lineup.bench) ensurePairTeam(t, lineup.bench);
  const out: Fixture[] = [];
  for (let c = lineup.courts.length; c >= 1; c -= 1) {
    const [w, b] = lineup.courts[c - 1];
    out.push({
      id: `${t.id}-r${round}-p${c}`,
      round,
      court: c,
      whiteTeamId: ensurePairTeam(t, w).id,
      blueTeamId: ensurePairTeam(t, b).id,
      ...resting,
      ...(final && c === 1 ? { stage: 'final' as const, ...(settings.finalBestOf > 1 ? { bestOf: settings.finalBestOf } : {}) } : {}),
    });
  }
  return out;
}

/** Pistas de una ronda ya creada, en orden (pista 1 primero). */
export function lineupOfRound(t: Tournament, round: number): Lineup {
  const ids = (tid: string | null) => t.teams.find((x) => x.id === tid)?.playerIds ?? [];
  const fx = t.fixtures.filter((f) => f.round === round).sort((a, b) => (a.court ?? 0) - (b.court ?? 0));
  return {
    courts: fx.map((f) => [ids(f.whiteTeamId), ids(f.blueTeamId)] as Court),
    bench: fx[0]?.resting ? [...fx[0].resting] : null,
  };
}

/** Las dos parejas que llegan a una pista se deshacen: cada uno va con uno de la otra, por sorteo. */
function mix([top, bottom]: Court, rnd: () => number): Court {
  const x = rnd() < 0.5 ? 0 : 1;
  return [
    [top[0], bottom[x]],
    [top[1], bottom[1 - x]],
  ];
}

/**
 * Cómo llegan las parejas a cada pista tras una ronda jugada entera, antes de mezclarse: arriba la que
 * baja (o la que se queda en la 1) y abajo la que sube (o la que se queda abajo, o la del banquillo).
 */
export function arrivalLineup(t: Tournament, round: number): Lineup {
  const ids = (tid: string | null | undefined) => t.teams.find((x) => x.id === tid)?.playerIds ?? [];
  const fx = t.fixtures.filter((f) => f.round === round).sort((a, b) => (a.court ?? 0) - (b.court ?? 0));
  const { bench } = lineupOfRound(t, round);
  const won = fx.map((f) => ids(f.winnerTeamId));
  const lost = fx.map((f) => ids(f.winnerTeamId === f.whiteTeamId ? f.blueTeamId : f.whiteTeamId));
  const k = fx.length;
  const courts: Court[] = fx.map((_, i) => {
    const top = i === 0 ? won[0] : lost[i - 1];
    const bottom = i === k - 1 ? (bench ?? lost[k - 1]) : won[i + 1];
    return [top, bottom];
  });
  return { courts, bench: bench ? lost[k - 1] : null };
}

/** Pistas de la ronda siguiente: las de llegada y, con cambio de pareja, mezcladas por sorteo. */
export function nextLineup(t: Tournament, round: number): Lineup {
  const arrival = arrivalLineup(t, round);
  if (!t.ladder!.swap) return arrival;
  const rnd = seededRandom(`${t.id}-r${round + 1}`);
  return { ...arrival, courts: arrival.courts.map((c) => mix(c, rnd)) };
}

/** Partidos del torneo ya apuntados, del primero al último. */
function playedMatches(t: Tournament, matches: StoredMatch[]): StoredMatch[] {
  const ids = new Set(t.fixtures.flatMap((f) => f.matchIds ?? (f.matchId ? [f.matchId] : [])));
  return matches.filter((m) => ids.has(m.id)).sort((a, b) => a.finishedAt - b.finishedAt);
}

export interface LadderClock {
  /** El reloj arranca con el primer partido. */
  started: boolean;
  remainingMs: number;
  /** Lo que tarda de media cada partido (con el tiempo entre uno y otro). */
  avgMs: number | null;
}

/** Reloj de un Pool por tiempo: lo que queda hasta `now`. */
export function ladderClock(t: Tournament, matches: StoredMatch[], now: number): LadderClock {
  const total = (t.ladder?.minutes ?? 0) * 60_000;
  const played = playedMatches(t, matches);
  if (!played.length) return { started: false, remainingMs: total, avgMs: null };
  const start = played[0].startedAt;
  const last = played[played.length - 1].finishedAt;
  return { started: true, remainingMs: total - (now - start), avgMs: (last - start) / played.length };
}

/** ¿Ya solo da tiempo a un partido? Lo que queda (al acabar el último) es menos que dos partidos de media. */
export function timeForFinal(t: Tournament, matches: StoredMatch[]): boolean {
  const played = playedMatches(t, matches);
  if (!played.length) return false;
  const clock = ladderClock(t, played, played[played.length - 1].finishedAt);
  return clock.remainingMs < 2 * clock.avgMs!;
}

/** La siguiente pista 1 pasa a ser la final; lo que quede de esa ronda en las otras pistas ya no se juega. */
function callFinal(t: Tournament): void {
  const pending = t.fixtures.filter((f) => !done(f) && !f.skipped);
  if (!pending.length) return;
  const round = Math.min(...pending.map((f) => f.round));
  for (const f of t.fixtures) {
    if (f.round !== round || done(f) || f.skipped) continue;
    if (f.court === 1) {
      f.stage = 'final';
      if (t.ladder!.finalBestOf > 1) f.bestOf = t.ladder!.finalBestOf;
    } else f.skipped = true;
  }
}

/**
 * Tras apuntar un partido (que no es la final): si la ronda está completa se crea la siguiente, y por
 * tiempo, si ya solo da para un partido, se llama a la final. Modifica `t`.
 */
export function ladderAfterResult(t: Tournament, round: number, all: StoredMatch[]): void {
  const s = t.ladder!;
  const roundFx = t.fixtures.filter((f) => f.round === round && !f.skipped);
  if (roundFx.every(done)) {
    const lastRound = s.limit === 'rounds' && round + 1 >= s.rounds;
    t.fixtures.push(...ladderRoundFixtures(t, round + 1, nextLineup(t, round), lastRound));
  }
  if (s.limit === 'time' && !t.fixtures.some((f) => f.stage === 'final') && timeForFinal(t, all)) callFinal(t);
}

export interface LadderPlace {
  team: TournamentTeam;
  place: number;
}

/**
 * Clasificación final: la escalera de la última ronda. En cada pista, el que gana delante del que
 * pierde (pista 1: campeón y subcampeón); si esa pista ya no se jugó, sus dos parejas comparten puesto.
 * El banquillo, al final.
 */
export function ladderPlaces(t: Tournament): LadderPlace[] {
  const round = Math.max(...t.fixtures.map((f) => f.round));
  const fx = t.fixtures.filter((f) => f.round === round).sort((a, b) => (a.court ?? 0) - (b.court ?? 0));
  const team = (tid: string | null | undefined) => t.teams.find((x) => x.id === tid);
  const out: LadderPlace[] = [];
  fx.forEach((f, i) => {
    const w = team(f.whiteTeamId);
    const b = team(f.blueTeamId);
    if (!w || !b) return;
    const base = i * 2 + 1;
    if (f.winnerTeamId) {
      const [win, lose] = f.winnerTeamId === w.id ? [w, b] : [b, w];
      out.push({ team: win, place: base }, { team: lose, place: base + 1 });
    } else out.push({ team: w, place: base }, { team: b, place: base });
  });
  const bench = fx[0]?.resting;
  if (bench) {
    const tt = t.teams.find((x) => pairKey(x.playerIds) === pairKey(bench));
    if (tt) out.push({ team: tt, place: fx.length * 2 + 1 });
  }
  return out;
}

/** Ajustes de un Pool en escalera nuevo (las parejas de salida se ponen al crearlo). */
export function ladderSettings(p: Partial<Omit<LadderSettings, 'start'>> = {}): Omit<LadderSettings, 'start'> {
  return { ...LADDER_DEFAULTS, finalBestOf: 1, ...p };
}
