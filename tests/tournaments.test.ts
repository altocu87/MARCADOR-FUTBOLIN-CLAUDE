import { describe, expect, it } from 'vitest';
import { DEFAULT_CONFIG } from '../src/match-engine';
import { DEFAULT_PROGRESSION, normalizePreferences, type Player, type StoredMatch, type Tournament } from '../src/services/persistence';
import { computeProgression } from '../src/services/progression';
import { seededRandom } from '../src/services/statistics/calendar';
import {
  BUILT_IN_TEMPLATES,
  createTournament,
  describeTemplate,
  draftFromTemplate,
  effectivePoolGames,
  fixtureConfig,
  formTeams,
  playableFixtures,
  poolGameOptions,
  poolStandings,
  recommendedTemplateId,
  recordFixtureResult,
  schedulePool,
  templateFit,
  validateDraft,
  type TournamentDraft,
} from '../src/services/tournaments';
import { makeMatch } from './helpers';

const players = (ids: string[]): Player[] =>
  ids.map((id) => ({ id, name: id.toUpperCase(), active: true, createdAt: 0, updatedAt: 0 }));

/** Juega todo lo pendiente; `winnerOf` decide qué lado gana cada partido. */
function playAll(t: Tournament, winnerOf: (white: string[], blue: string[]) => 'W' | 'B', matches: StoredMatch[] = []) {
  let at = 1000;
  for (let guard = 0; guard < 100 && playableFixtures(t).length; guard += 1) {
    const f = playableFixtures(t)[0];
    const white = t.teams.find((x) => x.id === f.whiteTeamId)!.playerIds;
    const blue = t.teams.find((x) => x.id === f.blueTeamId)!.playerIds;
    const w = winnerOf(white, blue);
    const goals = w === 'W' ? 'WWWWB' : 'BBBBW';
    const m = { ...makeMatch({ white, blue, goals, at: (at += 1000), mode: 'quick' }), tournament: { id: t.id, fixtureId: f.id } };
    matches.push(m);
    t = recordFixtureResult(t, f.id, m, matches);
  }
  return { t, matches };
}

const poolDraft = (ids: string[], extra: Partial<TournamentDraft> = {}): TournamentDraft => ({
  name: 'Pool',
  format: 'pool',
  teamSize: 2,
  ranked: false,
  config: DEFAULT_CONFIG,
  teams: [],
  seeding: 'random',
  entrants: ids,
  gamesPerPlayer: effectivePoolGames(ids.length, 4),
  pairing: 'random',
  ...extra,
});

describe('Pool rotativo', () => {
  it('ajusta los partidos por jugador para que todos jueguen lo mismo', () => {
    expect(poolGameOptions(5)).toEqual([4, 8]);
    expect(effectivePoolGames(5, 3)).toBe(4);
    expect(effectivePoolGames(6, 3)).toBe(4);
    expect(effectivePoolGames(8, 3)).toBe(3);
    expect(effectivePoolGames(7, 1)).toBe(4);
  });

  it.each([4, 5, 6, 7, 8, 9, 10])('con %i jugadores todos juegan lo mismo y nadie repite en su ronda', (n) => {
    const ids = Array.from({ length: n }, (_, i) => `p${i}`);
    const g = effectivePoolGames(n, 4);
    const rounds = schedulePool(ids, g, 'random', () => 1200, seededRandom(`pool${n}`), 60);
    const count = new Map(ids.map((id) => [id, 0]));
    for (const r of rounds) {
      const inRound = r.matches.flatMap(([w, b]) => [...w, ...b]);
      expect(new Set(inRound).size).toBe(inRound.length);
      expect(r.resting.length + inRound.length).toBe(n);
      inRound.forEach((id) => count.set(id, count.get(id)! + 1));
    }
    expect([...count.values()].every((c) => c === g)).toBe(true);
  });

  it('con 5 jugadores: 5 partidos, cada uno descansa una vez y no repite pareja', () => {
    const ids = ['a', 'b', 'c', 'd', 'e'];
    const rounds = schedulePool(ids, 4, 'random', () => 1200, seededRandom('cinco'));
    expect(rounds).toHaveLength(5);
    expect(rounds.map((r) => r.resting[0]).sort()).toEqual(ids);
    const pairs = rounds.flatMap((r) => r.matches.flatMap(([w, b]) => [w, b].map((p) => [...p].sort().join('|'))));
    expect(new Set(pairs).size).toBe(pairs.length);
  });

  it('sin final gana el líder individual y recibe el premio en un partido suyo', () => {
    const ps = players(['a', 'b', 'c', 'd', 'e']);
    const t0 = createTournament(poolDraft(ps.map((p) => p.id)), ps, () => 1200, 0, seededRandom('x'));
    expect(t0.fixtures).toHaveLength(5);
    expect(t0.fixtures.every((f) => f.resting?.length === 1)).toBe(true);
    // Gana siempre el lado de «a»; si no está, el blanco.
    const { t, matches } = playAll(t0, (_w, b) => (b.includes('a') ? 'B' : 'W'));
    expect(t.status).toBe('finished');
    const table = poolStandings(t, matches);
    expect(table[0].team.playerIds).toEqual(['a']);
    expect(table[0].wins).toBe(4);
    expect(t.teams.find((x) => x.id === t.winnerTeamId)!.playerIds).toEqual(['a']);
    const prize = matches.find((m) => m.id === t.finalMatchId)!;
    expect(prize.participants.some((p) => p.playerId === 'a')).toBe(true);
    const prog = computeProgression(ps.map((p) => p.id), matches, DEFAULT_PROGRESSION, { tournaments: [t], challenges: false });
    expect(prog.players.get('a')!.tournamentsWon).toBe(1);
  });

  it('final 1º+4º contra 2º+3º al mejor de 3', () => {
    const ps = players(['a', 'b', 'c', 'd', 'e', 'f']);
    const draft = poolDraft(ps.map((p) => p.id), { final: 'top4', finalBestOf: 3, finalConfig: { ...DEFAULT_CONFIG, goalsPerPeriod: 7 } });
    const t0 = createTournament(draft, ps, () => 1200, 0, seededRandom('y'));
    const final = t0.fixtures.find((f) => f.stage === 'final')!;
    expect(final.bestOf).toBe(3);
    expect(fixtureConfig(t0, final).goalsPerPeriod).toBe(7);
    expect(fixtureConfig(t0, t0.fixtures[0]).goalsPerPeriod).toBe(DEFAULT_CONFIG.goalsPerPeriod);
    // Gana el lado que tenga al jugador de menor letra.
    const rank = (ids: string[]) => Math.min(...ids.map((id) => id.charCodeAt(0)));
    const { t, matches } = playAll(t0, (w, b) => (rank(w) < rank(b) ? 'W' : 'B'));
    const f = t.fixtures.find((x) => x.stage === 'final')!;
    expect(t.status).toBe('finished');
    expect(f.matchIds).toHaveLength(2);
    expect(Math.max(f.series!.white, f.series!.blue)).toBe(2);
    const champs = t.teams.find((x) => x.id === t.winnerTeamId)!.playerIds;
    expect(champs).toHaveLength(2);
    expect(t.finalMatchId).toBe(f.matchId);
    const table = poolStandings(t, matches);
    const white = t.teams.find((x) => x.id === f.whiteTeamId)!.playerIds;
    expect(white.sort()).toEqual([table[0].team.playerIds[0], table[3].team.playerIds[0]].sort());
  });

  it('valida el número de jugadores', () => {
    expect(validateDraft(poolDraft(['a', 'b', 'c']))).not.toHaveLength(0);
    expect(validateDraft(poolDraft(['a', 'b', 'c', 'd', 'e']))).toHaveLength(0);
  });
});

describe('Liguilla con final', () => {
  it('los dos primeros juegan la final y el campeón sale de ella', () => {
    const ps = players(['a', 'b', 'c', 'd']);
    const t0 = createTournament(
      { name: 'Liga', format: 'league', teamSize: 1, ranked: false, config: DEFAULT_CONFIG, teams: ps.map((p) => ({ playerIds: [p.id] })), seeding: 'elo', final: 'top2' },
      ps,
      () => 1200,
      0,
    );
    expect(t0.fixtures).toHaveLength(7);
    // Gana siempre el de menor letra: a 1º, b 2º.
    const { t } = playAll(t0, (w, b) => (w[0] < b[0] ? 'W' : 'B'));
    const final = t.fixtures.find((f) => f.stage === 'final')!;
    expect(t.status).toBe('finished');
    expect([final.whiteTeamId, final.blueTeamId].map((id) => t.teams.find((x) => x.id === id)!.playerIds[0])).toEqual(['a', 'b']);
    expect(t.winnerTeamId).toBe(final.winnerTeamId);
  });
});

describe('Parejas fijas y predefinidos', () => {
  it('con número impar sobra un jugador', () => {
    const elo: Record<string, number> = { a: 1500, b: 1400, c: 1300, d: 1200, e: 1100 };
    const { teams, leftover } = formTeams(Object.keys(elo), 2, 'elo', (id) => elo[id]);
    expect(teams).toHaveLength(2);
    expect(leftover).toEqual(['c']);
    expect(teams[0].playerIds).toEqual(['a', 'e']);
  });

  it('los predefinidos de fábrica producen torneos válidos', () => {
    const ids = ['a', 'b', 'c', 'd', 'e', 'f'];
    for (const tpl of BUILT_IN_TEMPLATES) {
      const { draft, leftover } = draftFromTemplate(tpl, tpl.name, ids, () => 1200, {}, seededRandom(tpl.id));
      expect(leftover).toEqual([]);
      expect(validateDraft(draft)).toEqual([]);
      expect(describeTemplate(tpl).length).toBeGreaterThan(5);
    }
  });

  it('la eliminatoria de fábrica juega la final al mejor de 3 y a 7 goles', () => {
    const tpl = BUILT_IN_TEMPLATES.find((x) => x.format === 'bracket')!;
    const ps = players(['a', 'b', 'c', 'd']);
    const { draft } = draftFromTemplate(tpl, 'Copa', ps.map((p) => p.id), () => 1200);
    const t0 = createTournament(draft, ps, () => 1200, 0, seededRandom('z'));
    const final = t0.fixtures.find((f) => f.round === 2)!;
    expect(final.bestOf).toBe(3);
    expect(fixtureConfig(t0, final).goalsPerPeriod).toBe(7);
    const { t } = playAll(t0, (w, b) => (w[0] < b[0] ? 'W' : 'B'));
    expect(t.status).toBe('finished');
    expect(t.teams.find((x) => x.id === t.winnerTeamId)!.playerIds).toEqual(['a']);
  });

  it('las preferencias antiguas se completan con la lista de predefinidos vacía', () => {
    expect(normalizePreferences({ volume: 0.2 }).tournamentTemplates).toEqual([]);
  });
});

describe('Tipos de torneo según el número de jugadores', () => {
  const tpl = (id: string) => BUILT_IN_TEMPLATES.find((x) => x.id === id)!;
  it('las parejas fijas no encajan con número impar y dicen por qué', () => {
    const fit = templateFit(tpl('builtin-pairs'), 7);
    expect(fit.ok).toBe(false);
    expect(fit.reason).toMatch(/par/);
    expect(templateFit(tpl('builtin-pairs'), 8).ok).toBe(true);
  });
  it('el Pool encaja con pares e impares y resume los descansos', () => {
    expect(templateFit(tpl('builtin-pool'), 5).summary).toMatch(/descansa 1/);
    expect(templateFit(tpl('builtin-pool'), 8).summary).toMatch(/sin descansos/);
    expect(templateFit(tpl('builtin-pool'), 3).ok).toBe(false);
  });
  it('la liguilla individual tiene tope de 8', () => {
    expect(templateFit(tpl('builtin-league'), 9).ok).toBe(false);
  });
  it('recomienda según cuántos son', () => {
    expect(recommendedTemplateId(3)).toBe('builtin-league');
    expect(recommendedTemplateId(5)).toBe('builtin-pool');
    expect(recommendedTemplateId(8)).toBe('builtin-pairs');
    expect(recommendedTemplateId(9)).toBe('builtin-pool');
    expect(recommendedTemplateId(2)).toBeNull();
  });
});
