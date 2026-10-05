import { describe, expect, it } from 'vitest';
import { DEFAULT_CONFIG } from '../src/match-engine';
import { DEFAULT_PROGRESSION, normalizePreferences, type Player, type StoredMatch, type Tournament } from '../src/services/persistence';
import { findNameClash, mergeBlocker, mergePlayers, nameKey, validatePlayerDraft } from '../src/services/players';
import { computeProgression } from '../src/services/progression';
import { seededRandom } from '../src/services/statistics/calendar';
import {
  BUILT_IN_TEMPLATES,
  competitions,
  editionName,
  nextEdition,
  tournamentReport,
  createTournament,
  describeTemplate,
  draftFromTemplate,
  effectivePoolGames,
  fixtureConfig,
  fixtureProgress,
  nextFixture,
  undoMatch,
  updateTournamentRules,
  recordedMatches,
  markAbandoned,
  withFinalRules,
  phaseInfo,
  standingsMovement,
  formTeams,
  playableFixtures,
  poolGameOptions,
  poolStandings,
  recommendedTemplateId,
  recommendedTemplateIds,
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
  it('de 4 a 6 jugadores propone pocos torneos y todos se pueden jugar', () => {
    expect(recommendedTemplateIds(4)).toEqual(['builtin-pool', 'builtin-league', 'builtin-league-final']);
    expect(recommendedTemplateIds(5)).toEqual(['builtin-pool', 'builtin-league']);
    expect(recommendedTemplateIds(6)).toEqual(['builtin-pool', 'builtin-pairs-league', 'builtin-bracket']);
    for (const n of [4, 5, 6]) {
      for (const id of recommendedTemplateIds(n)) expect(templateFit(tpl(id), n).ok).toBe(true);
    }
  });
});

describe('Ediciones, ficha y palmarés', () => {
  const ps = players(['a', 'b', 'c', 'd', 'e']);
  const tpl = BUILT_IN_TEMPLATES.find((x) => x.id === 'builtin-pool')!;
  const make = (edition: number, seed: string) => {
    const { draft } = draftFromTemplate(tpl, editionName(tpl.name, edition), ps.map((p) => p.id), () => 1200, {}, seededRandom(seed));
    return createTournament({ ...draft, edition, final: 'none' }, ps, () => 1200, edition * 1000, seededRandom(seed));
  };

  it('numera las ediciones y no cuenta las canceladas', () => {
    expect(editionName('Copa Rotativa', 2)).toBe('Copa Rotativa · 2ª edición');
    const t1 = make(1, 'e1');
    const cancelled = { ...make(2, 'e2'), status: 'cancelled' as const };
    expect(nextEdition(tpl.id, [t1, cancelled])).toBe(2);
  });

  it('la ficha calcula partidos, goles y MVP; el palmarés suma títulos', () => {
    const all: StoredMatch[] = [];
    const tA = playAll(make(1, 'p1'), (_w, b) => (b.includes('a') ? 'B' : 'W'), all).t;
    const tB = playAll(make(2, 'p2'), (_w, b) => (b.includes('a') ? 'B' : 'W'), all).t;
    const r = tournamentReport(tA, all, ps);
    expect(r.matches).toHaveLength(5);
    expect(r.totalGoals).toBe(25);
    expect(r.mvp?.playerId).toBe('a');
    expect(r.championIds).toEqual(['a']);
    const [c] = competitions([tA, tB], all, ps);
    expect(c.name).toBe('Copa Rotativa');
    expect(c.editions.map((e) => e.edition)).toEqual([2, 1]);
    expect(c.honours[0]).toMatchObject({ playerId: 'a', titles: 2, mvps: 2, editions: 2 });
  });
});

describe('Fusionar un invitado con un jugador', () => {
  it('pasa partidos, goleadores y torneos al jugador y el invitado desaparece', () => {
    const ps: Player[] = [...players(['a', 'b', 'c', 'd']), { id: 'g', name: 'Pepe', guest: true, active: true, createdAt: 0, updatedAt: 0 }];
    const t0 = createTournament(poolDraft(['a', 'b', 'c', 'g']), ps, () => 1200, 0, seededRandom('m'));
    const { t, matches } = playAll(t0, (_w, b) => (b.includes('g') ? 'B' : 'W'));
    const withScorer = { ...matches[0], scorers: { e1: 'g' } };
    const all = [withScorer, ...matches.slice(1)];
    const r = mergePlayers('g', 'd', ps, all, [t], 5);
    expect(r.players.some((p) => p.id === 'g')).toBe(false);
    expect(r.matches.every((m) => m.participants.every((p) => p.playerId !== 'g'))).toBe(true);
    expect(r.matches[0].scorers?.e1).toBe('d');
    expect(r.tournaments[0].entrants).toContain('d');
    expect(r.tournaments[0].teams.some((x) => x.playerIds.includes('g'))).toBe(false);
    expect(r.movedTournaments).toBe(1);
    // El campeón (era el invitado) pasa a ser «d».
    const champ = r.tournaments[0].teams.find((x) => x.id === r.tournaments[0].winnerTeamId)!;
    expect(champ.playerIds).toEqual(['d']);
    expect(champ.name).toBe('D');
  });

  it('no deja fusionar a dos que jugaron en el mismo partido', () => {
    const m = makeMatch({ white: ['a'], blue: ['g'], goals: 'WWWWW' });
    expect(mergeBlocker('g', 'a', [m])).not.toBeNull();
    expect(mergeBlocker('g', 'b', [m])).toBeNull();
  });
});

describe('Siguiente partido', () => {
  it('va por orden de jornada y cuenta el progreso', () => {
    const ps = players(['a', 'b', 'c', 'd', 'e']);
    let t = createTournament(
      { name: 'Liga', format: 'league', teamSize: 1, ranked: false, config: DEFAULT_CONFIG, teams: ps.map((p) => ({ playerIds: [p.id] })), seeding: 'elo' },
      ps,
      () => 1200,
      0,
    );
    expect(fixtureProgress(t)).toEqual({ done: 0, total: 10 });
    const seen: number[] = [];
    const all: StoredMatch[] = [];
    for (let i = 0; i < 10; i += 1) {
      const f = nextFixture(t)!;
      seen.push(f.round);
      const white = t.teams.find((x) => x.id === f.whiteTeamId)!.playerIds;
      const blue = t.teams.find((x) => x.id === f.blueTeamId)!.playerIds;
      const m = { ...makeMatch({ white, blue, goals: 'WWWWW', at: 1000 + i }), tournament: { id: t.id, fixtureId: f.id } };
      all.push(m);
      t = recordFixtureResult(t, f.id, m, all);
    }
    expect(seen).toEqual([...seen].sort((a, b) => a - b));
    expect(nextFixture(t)).toBeUndefined();
    expect(fixtureProgress(t)).toEqual({ done: 10, total: 10 });
  });
});

describe('Nombres repetidos', () => {
  const ps: Player[] = [
    { id: 'j', name: 'José', active: true, createdAt: 0, updatedAt: 0 },
    { id: 'g', name: 'Pepe', guest: true, active: true, createdAt: 0, updatedAt: 0 },
  ];
  it('detecta el mismo nombre sin tildes, mayúsculas ni espacios', () => {
    expect(nameKey('  José  ')).toBe('jose');
    expect(findNameClash('jose', ps)?.id).toBe('j');
    expect(findNameClash('PEPE', ps)?.id).toBe('g');
    expect(findNameClash('Pepa', ps)).toBeUndefined();
    // Al editar a uno, su propio nombre no cuenta.
    expect(findNameClash('José', ps, 'j')).toBeUndefined();
  });
  it('no deja guardar un jugador con un nombre que ya existe', () => {
    expect(validatePlayerDraft({ name: 'pepe' }, ps)).toContain('Ya existe un jugador con ese nombre.');
  });
});

describe('Subidas y bajadas en la clasificación', () => {
  it('compara con la clasificación de antes del último partido', () => {
    const ps = players(['a', 'b', 'c']);
    let t = createTournament(
      { name: 'Liga', format: 'league', teamSize: 1, ranked: false, config: DEFAULT_CONFIG, teams: ps.map((p) => ({ playerIds: [p.id] })), seeding: 'elo' },
      ps,
      () => 1200,
      0,
    );
    expect(standingsMovement(t, []).size).toBe(0);
    const all: StoredMatch[] = [];
    const play = (winnerLetter: string) => {
      const f = nextFixture(t)!;
      const white = t.teams.find((x) => x.id === f.whiteTeamId)!.playerIds;
      const blue = t.teams.find((x) => x.id === f.blueTeamId)!.playerIds;
      const m = { ...makeMatch({ white, blue, goals: white.includes(winnerLetter) ? 'WWWWW' : 'BBBBB', at: 1000 + all.length }), tournament: { id: t.id, fixtureId: f.id } };
      all.push(m);
      t = recordFixtureResult(t, f.id, m, all);
      return [...white, ...blue];
    };
    // Antes de jugar: A, B, C (por nombre). Primer partido: B contra C y gana C.
    expect(play('c').sort()).toEqual(['b', 'c']);
    const id = (pid: string) => t.teams.find((tt) => tt.playerIds[0] === pid)!.id;
    const mv = standingsMovement(t, all);
    // Ahora: C, A, B → C sube; A y B bajan un puesto.
    expect(mv.get(id('c'))).toBe('up');
    expect(mv.get(id('a'))).toBe('down');
    expect(mv.get(id('b'))).toBe('down');
    expect([...mv.values()].every((v) => ['up', 'down', 'same'].includes(v))).toBe(true);
  });
});

describe('Cartel de fase del torneo', () => {
  const tpl = BUILT_IN_TEMPLATES.find((x) => x.format === 'bracket')!;
  const bracket = (n: number) => {
    const ps = players('abcdefgh'.slice(0, n).split(''));
    const { draft } = draftFromTemplate(tpl, 'Copa', ps.map((p) => p.id), () => 1200);
    return createTournament(draft, ps, () => 1200, 0, seededRandom('z'));
  };
  it('la eliminatoria de 4 anuncia semifinal y final, con el partido de la serie', () => {
    const t = bracket(4);
    expect(phaseInfo(t, t.fixtures.find((f) => f.round === 1)!)).toMatchObject({ tier: 'semi', title: 'SEMIFINAL' });
    const final = phaseInfo(t, t.fixtures.find((f) => f.round === 2)!);
    expect(final).toMatchObject({ tier: 'final', title: 'GRAN FINAL', sub: 'PARTIDO 1 · AL MEJOR DE 3' });
  });
  it('la eliminatoria de 8 empieza en cuartos de final', () => {
    const t = bracket(8);
    expect(phaseInfo(t, t.fixtures.find((f) => f.round === 1)!)).toMatchObject({ tier: 'quarter', title: 'CUARTOS DE FINAL' });
  });
});

describe('Reglas propias de la final', () => {
  const ps = players(['a', 'b', 'c', 'd']);
  const tpl = BUILT_IN_TEMPLATES.find((x) => x.id === 'builtin-league-final')!;
  it('la final puede jugarse a tiempo aunque la liguilla sea a goles, y a partido único', () => {
    const custom = { ...withFinalRules(tpl, { endCondition: 'time', goalsPerPeriod: 5, minutesPerPeriod: 8 }), finalBestOf: 1 as const };
    const { draft } = draftFromTemplate(custom, 'Liga', ps.map((p) => p.id), () => 1200);
    const t = createTournament(draft, ps, () => 1200, 0, seededRandom('z'));
    const final = t.fixtures.find((f) => f.stage === 'final')!;
    expect(final.bestOf).toBeUndefined();
    expect(fixtureConfig(t, final)).toMatchObject({ endCondition: 'time', minutesPerPeriod: 8 });
    expect(fixtureConfig(t, t.fixtures[0])).toMatchObject({ endCondition: 'goals', goalsPerPeriod: 5 });
    expect(describeTemplate(custom)).toMatch(/final 8 min/);
  });
  it('sin reglas propias la final se juega como el resto', () => {
    const { draft } = draftFromTemplate(tpl, 'Liga', ps.map((p) => p.id), () => 1200);
    expect(draft.finalConfig).toBeUndefined();
  });
});

describe('Corregir un torneo ya empezado', () => {
  const tplB = BUILT_IN_TEMPLATES.find((x) => x.id === 'builtin-bracket')!;
  const tplL = BUILT_IN_TEMPLATES.find((x) => x.id === 'builtin-league-final')!;
  const ps = players(['a', 'b', 'c', 'd']);
  const make = (tpl: typeof tplB) => {
    const { draft } = draftFromTemplate(tpl, 'T', ps.map((p) => p.id), () => 1200);
    return createTournament(draft, ps, () => 1200, 0, seededRandom('z'));
  };
  it('repetir el último partido deja el torneo como antes de jugarlo', () => {
    const t0 = make(tplB);
    const { t, matches } = playAll(t0, (w, b) => (w[0] < b[0] ? 'W' : 'B'));
    expect(t.status).toBe('finished');
    const last = recordedMatches(t, matches).at(-1)!;
    const back = undoMatch(t, last.id, matches);
    expect(back.status).toBe('active');
    expect(back.winnerTeamId).toBeUndefined();
    const final = back.fixtures.find((f) => f.round === 2)!;
    expect(final.whiteTeamId && final.blueTeamId).toBeTruthy();
    expect((final.series?.white ?? 0) + (final.series?.blue ?? 0)).toBe(1);
    expect(final.winnerTeamId).toBeUndefined();
  });
  it('en la liguilla, deshacer el último de la fase regular quita los equipos de la final', () => {
    let t = make(tplL);
    const matches: StoredMatch[] = [];
    let at = 1000;
    while (playableFixtures(t).some((f) => f.stage !== 'final')) {
      const f = playableFixtures(t)[0];
      const white = t.teams.find((x) => x.id === f.whiteTeamId)!.playerIds;
      const blue = t.teams.find((x) => x.id === f.blueTeamId)!.playerIds;
      const m = { ...makeMatch({ white, blue, goals: 'WWWWB', at: (at += 1000), mode: 'quick' }), tournament: { id: t.id, fixtureId: f.id } };
      matches.push(m);
      t = recordFixtureResult(t, f.id, m, matches);
    }
    expect(t.fixtures.find((f) => f.stage === 'final')!.whiteTeamId).toBeTruthy();
    const back = undoMatch(t, matches.at(-1)!.id, matches);
    expect(back.fixtures.find((f) => f.stage === 'final')!.whiteTeamId).toBeNull();
    expect(back.fixtures.filter((f) => f.winnerTeamId).length).toBe(matches.length - 1);
  });
  it('cambiar las reglas a mitad de torneo afecta a lo pendiente y a la final', () => {
    const t = make(tplL);
    const next = updateTournamentRules(t, { endCondition: 'time', goalsPerPeriod: 5, minutesPerPeriod: 3 }, { endCondition: 'goals', goalsPerPeriod: 10, minutesPerPeriod: 3 }, 1);
    const final = next.fixtures.find((f) => f.stage === 'final')!;
    expect(fixtureConfig(next, next.fixtures[0])).toMatchObject({ endCondition: 'time', minutesPerPeriod: 3 });
    expect(fixtureConfig(next, final)).toMatchObject({ endCondition: 'goals', goalsPerPeriod: 10 });
    expect(final.bestOf).toBeUndefined();
  });
  it('un partido abandonado queda marcado hasta que se apunta su resultado', () => {
    const t = make(tplB);
    const f = playableFixtures(t)[0];
    const marked = markAbandoned(t, f.id, 5);
    expect(marked.fixtures.find((x) => x.id === f.id)!.abandonedAt).toBe(5);
    const white = t.teams.find((x) => x.id === f.whiteTeamId)!.playerIds;
    const blue = t.teams.find((x) => x.id === f.blueTeamId)!.playerIds;
    const m = { ...makeMatch({ white, blue, goals: 'WWWWW', at: 9, mode: 'quick' }), tournament: { id: t.id, fixtureId: f.id } };
    expect(recordFixtureResult(marked, f.id, m, [m]).fixtures.find((x) => x.id === f.id)!.abandonedAt).toBeUndefined();
  });
});

describe('Equipos guardados en torneos de parejas', () => {
  it('si están los dos de un equipo, juegan juntos; el resto se empareja como siempre', () => {
    const tpl = BUILT_IN_TEMPLATES.find((x) => x.id === 'builtin-pairs')!;
    const ids = ['a', 'b', 'c', 'd', 'e', 'f'];
    const elo = (id: string) => ({ a: 1500, b: 1490, c: 1200, d: 1100, e: 1000, f: 900 })[id]!;
    // Por ELO, a y b irían separados; como son equipo, van juntos.
    const { draft, leftover } = draftFromTemplate(tpl, 'Copa', ids, elo, {}, () => 0.3, [['a', 'b'], ['x', 'y']]);
    expect(leftover).toEqual([]);
    expect(draft.teams.map((t) => [...t.playerIds].sort().join(''))).toContain('ab');
    expect(draft.teams).toHaveLength(3);
  });
});
