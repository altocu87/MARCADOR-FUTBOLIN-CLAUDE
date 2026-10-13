import { describe, expect, it } from 'vitest';
import { DEFAULT_CONFIG } from '../src/match-engine';
import type { Player, StoredMatch, Tournament } from '../src/services/persistence';
import { seededRandom } from '../src/services/statistics/calendar';
import {
  createTournament,
  finalRanking,
  lineupOfRound,
  nextFixture,
  phaseInfo,
  playableFixtures,
  recordFixtureResult,
  undoMatch,
  validateDraft,
  type TournamentDraft,
} from '../src/services/tournaments';
import { makeMatch } from './helpers';

const players = (ids: string[]): Player[] => ids.map((id) => ({ id, name: id.toUpperCase(), active: true, createdAt: 0, updatedAt: 0 }));

const draft = (pairs: string[][], ladder: Partial<NonNullable<TournamentDraft['ladder']>> = {}, extra: Partial<TournamentDraft> = {}): TournamentDraft => ({
  name: 'Pool',
  format: 'pool',
  teamSize: 2,
  ranked: false,
  config: DEFAULT_CONFIG,
  teams: pairs.map((p) => ({ playerIds: p })),
  seeding: 'random',
  ladder: { swap: false, limit: 'rounds', minutes: 30, rounds: 3, ...ladder },
  ...extra,
});

/** Siempre el mismo orden de salida (sin barajar): pareja 0 y 1 en la pista 1, 2 y 3 en la 2… */
const noShuffle = () => 0.999999;
const make = (pairs: string[][], ladder: Partial<NonNullable<TournamentDraft['ladder']>> = {}, extra: Partial<TournamentDraft> = {}) => {
  const ids = pairs.flat();
  const t = createTournament(draft(pairs, ladder, extra), players(ids), () => 1200, 0, noShuffle);
  return t;
};

const idsOf = (t: Tournament, tid: string | null | undefined) => t.teams.find((x) => x.id === tid)?.playerIds ?? [];

/** Juega el siguiente partido; `wins` dice si gana la pareja que tiene a ese jugador (o Blanco si nadie). */
function playNext(t: Tournament, matches: StoredMatch[], winner: (white: string[], blue: string[]) => 'W' | 'B', at: number, durationMs = 90) {
  const f = nextFixture(t)!;
  const white = idsOf(t, f.whiteTeamId);
  const blue = idsOf(t, f.blueTeamId);
  const base = makeMatch({ white, blue, goals: winner(white, blue) === 'W' ? 'WWWB' : 'BBBW', at, mode: 'quick' });
  const m: StoredMatch = { ...base, startedAt: at - durationMs, tournament: { id: t.id, fixtureId: f.id } };
  matches.push(m);
  return recordFixtureResult(t, f.id, m, matches);
}

const whiteWins = () => 'W' as const;

describe('Pool en escalera', () => {
  it('cada ronda va de la última pista a la 1 y el que gana sube, el que pierde baja', () => {
    // A+B y C+D en la pista 1; E+F y G+H en la 2.
    let t = make([['a', 'b'], ['c', 'd'], ['e', 'f'], ['g', 'h']]);
    expect(nextFixture(t)!.court).toBe(2);
    const matches: StoredMatch[] = [];
    t = playNext(t, matches, whiteWins, 1000); // pista 2: gana E+F
    t = playNext(t, matches, whiteWins, 2000); // pista 1: gana A+B
    const r2 = lineupOfRound(t, 2);
    expect(r2.courts[0]).toEqual([['a', 'b'], ['e', 'f']]); // A+B se quedan, E+F suben
    expect(r2.courts[1]).toEqual([['c', 'd'], ['g', 'h']]); // C+D bajan, G+H se quedan
    expect(r2.bench).toBeNull();
  });

  it('con cambio de pareja, en cada pista cada uno va con uno de la otra pareja', () => {
    let t = make([['a', 'b'], ['c', 'd'], ['e', 'f'], ['g', 'h']], { swap: true });
    const matches: StoredMatch[] = [];
    t = playNext(t, matches, whiteWins, 1000);
    t = playNext(t, matches, whiteWins, 2000);
    const [c1, c2] = lineupOfRound(t, 2).courts;
    // Pista 1: A y B (se quedan) repartidos con E y F (suben): nunca A con B ni E con F.
    for (const pair of c1) expect(pair.filter((p) => 'ab'.includes(p))).toHaveLength(1);
    expect(c1.flat().sort()).toEqual(['a', 'b', 'e', 'f']);
    for (const pair of c2) expect(pair.filter((p) => 'cd'.includes(p))).toHaveLength(1);
    expect(c2.flat().sort()).toEqual(['c', 'd', 'g', 'h']);
  });

  it('con número impar de parejas, el que pierde en la última sale al banquillo y entra el que esperaba', () => {
    // Tu ejemplo: A+B contra C+D, E y F esperan. Pierden A+B: salen y entran E y F repartidos con C y D.
    let t = make([['a', 'b'], ['c', 'd'], ['e', 'f']], { swap: true });
    expect(t.fixtures[0].resting).toEqual(['e', 'f']);
    const matches: StoredMatch[] = [];
    t = playNext(t, matches, () => 'B', 1000);
    const r2 = lineupOfRound(t, 2);
    expect(r2.bench).toEqual(['a', 'b']);
    expect(r2.courts[0].flat().sort()).toEqual(['c', 'd', 'e', 'f']);
    for (const pair of r2.courts[0]) expect(pair.filter((p) => 'cd'.includes(p))).toHaveLength(1);
  });

  it('por rondas, la pista 1 de la última ronda es la final y gana el torneo', () => {
    let t = make([['a', 'b'], ['c', 'd'], ['e', 'f'], ['g', 'h']], { rounds: 2 });
    const matches: StoredMatch[] = [];
    t = playNext(t, matches, whiteWins, 1000);
    t = playNext(t, matches, whiteWins, 2000);
    const fin = t.fixtures.find((f) => f.stage === 'final')!;
    expect(fin).toMatchObject({ round: 2, court: 1 });
    expect(phaseInfo(t, fin).title).toBe('GRAN FINAL');
    expect(phaseInfo(t, nextFixture(t)!).title).toBe('PISTA 2 · RONDA 2');
    t = playNext(t, matches, whiteWins, 3000); // pista 2
    t = playNext(t, matches, () => 'B', 4000); // final: gana E+F
    expect(t.status).toBe('finished');
    expect(idsOf(t, t.winnerTeamId)).toEqual(['e', 'f']);
    const ranking = finalRanking(t, matches);
    expect(ranking.map((r) => [r.place, r.team.playerIds.join('')])).toEqual([
      [1, 'ef'],
      [2, 'ab'],
      [3, 'cd'],
      [4, 'gh'],
    ]);
    expect(ranking[0].champion).toBe(true);
  });

  it('por tiempo, cuando ya solo da para un partido, la pista 1 juega la final y el resto de la ronda no se juega', () => {
    // 10 minutos y partidos de 3: tras el segundo (6 min) quedan 4, menos que dos partidos.
    let t = make([['a', 'b'], ['c', 'd'], ['e', 'f'], ['g', 'h'], ['i', 'j'], ['k', 'l']], { limit: 'time', minutes: 10 });
    const matches: StoredMatch[] = [];
    const min = 60_000;
    t = playNext(t, matches, whiteWins, 3 * min, 3 * min); // pista 3
    expect(t.fixtures.some((f) => f.stage === 'final')).toBe(false);
    t = playNext(t, matches, whiteWins, 6 * min, 3 * min); // pista 2
    const fin = nextFixture(t)!;
    expect(fin).toMatchObject({ stage: 'final', court: 1, round: 1 });
    expect(playableFixtures(t)).toHaveLength(1);
    t = playNext(t, matches, whiteWins, 9 * min, 3 * min);
    expect(t.status).toBe('finished');
  });

  it('si el tiempo se acaba al cerrar una ronda, la final es la pista 1 de la siguiente', () => {
    let t = make([['a', 'b'], ['c', 'd'], ['e', 'f'], ['g', 'h']], { limit: 'time', minutes: 10 });
    const matches: StoredMatch[] = [];
    const min = 60_000;
    t = playNext(t, matches, whiteWins, 3 * min, 3 * min);
    t = playNext(t, matches, whiteWins, 6 * min, 3 * min);
    const fin = nextFixture(t)!;
    expect(fin).toMatchObject({ stage: 'final', court: 1, round: 2 });
    expect(t.fixtures.find((f) => f.round === 2 && f.court === 2)?.skipped).toBe(true);
    t = playNext(t, matches, whiteWins, 9 * min, 3 * min);
    // La pista 2 no se jugó: sus dos parejas comparten puesto.
    expect(finalRanking(t, matches).map((r) => r.place)).toEqual([1, 2, 3, 3]);
  });

  it('repetir el último partido deja las rondas exactamente como estaban', () => {
    let t = make([['a', 'b'], ['c', 'd'], ['e', 'f'], ['g', 'h'], ['i', 'j']], { swap: true, rounds: 4 });
    const matches: StoredMatch[] = [];
    for (let i = 1; i <= 5; i += 1) t = playNext(t, matches, (w) => (w.includes('a') || w.includes('e') ? 'W' : 'B'), i * 1000);
    const before = t;
    t = playNext(t, matches, whiteWins, 9000);
    const undone = undoMatch(t, matches[matches.length - 1].id, matches.slice(0, -1));
    expect(undone.fixtures).toEqual(before.fixtures);
  });

  it('pide parejas completas y entre 2 y 8', () => {
    expect(validateDraft(draft([['a', 'b'], ['c', 'd']]))).toHaveLength(0);
    expect(validateDraft(draft([['a', 'b']]))).not.toHaveLength(0);
    expect(validateDraft(draft([['a', 'b'], ['c']]))).not.toHaveLength(0);
    expect(validateDraft(draft([['a', 'b'], ['a', 'd']]))).not.toHaveLength(0);
  });

  it('las parejas de salida se colocan en las pistas al azar', () => {
    const pairs = [['a', 'b'], ['c', 'd'], ['e', 'f'], ['g', 'h']];
    const orders = new Set(
      ['x', 'y', 'z', 'w'].map((seed) => {
        const t = createTournament(draft(pairs), players(pairs.flat()), () => 1200, 0, seededRandom(seed));
        return t.ladder!.start.map((p) => p.join('')).join(',');
      }),
    );
    expect(orders.size).toBeGreaterThan(1);
  });
});
