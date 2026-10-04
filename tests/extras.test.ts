import { describe, expect, it } from 'vitest';
import { DEFAULT_CONFIG } from '../src/match-engine';
import { DEFAULT_PROGRESSION, type Player, type StoredMatch } from '../src/services/persistence';
import { balancedTeams, randomTeams } from '../src/services/players';
import {
  ACHIEVEMENTS,
  activeChallenges,
  computeProgression,
  displayTitle,
  seasonChampions,
} from '../src/services/progression';
import {
  autoScorers,
  duel,
  pairStats,
  periodDigest,
  personalGoals,
  seasonKey,
  sideStats,
  weekKey,
} from '../src/services/statistics';
import {
  buildBracket,
  createTournament,
  leagueStandings,
  playableFixtures,
  recordFixtureResult,
  roundRobin,
} from '../src/services/tournaments';
import { makeMatch } from './helpers';

const players = (ids: string[]): Player[] =>
  ids.map((id) => ({ id, name: id.toUpperCase(), active: true, createdAt: 0, updatedAt: 0 }));

describe('Sorteo de equipos', () => {
  it('equilibra por ELO', () => {
    const elo: Record<string, number> = { a: 1500, b: 1400, c: 1000, d: 900 };
    const s = balancedTeams(['a', 'b', 'c', 'd'], (id) => elo[id]);
    expect(s.white).toEqual(['a', 'd']);
    expect(s.blue).toEqual(['b', 'c']);
    expect(s.diff).toBe(0);
  });
  it('aleatorio reparte los 4', () => {
    const s = randomTeams(['a', 'b', 'c', 'd'], () => 0.3);
    expect(new Set([...s.white, ...s.blue]).size).toBe(4);
  });
  it('equilibra también 4 contra 4', () => {
    const elo: Record<string, number> = { a: 1800, b: 1700, c: 1300, d: 1200, e: 1100, f: 1000, g: 900, h: 800 };
    const s = balancedTeams(Object.keys(elo), (id) => elo[id]);
    expect(s.white).toHaveLength(4);
    expect(s.blue).toHaveLength(4);
    expect(new Set([...s.white, ...s.blue]).size).toBe(8);
    expect(s.diff).toBe(0);
  });
});

describe('Torneos', () => {
  it('liguilla de 4: 6 partidos, cada uno juega 3', () => {
    const f = roundRobin(['A', 'B', 'C', 'D']);
    expect(f).toHaveLength(6);
    for (const id of ['A', 'B', 'C', 'D']) expect(f.filter((x) => x.whiteTeamId === id || x.blueTeamId === id)).toHaveLength(3);
  });
  it('liguilla impar: 5 equipos → 10 partidos', () => {
    expect(roundRobin(['A', 'B', 'C', 'D', 'E'])).toHaveLength(10);
  });
  it('cuadro de 3 con pase directo y final', () => {
    const f = buildBracket(['A', 'B', 'C']);
    expect(f).toHaveLength(3);
    const final = f.find((x) => x.round === 2)!;
    expect(final.whiteTeamId).toBe('A'); // cabeza de serie pasa directo
    expect(f.filter((x) => x.bye)).toHaveLength(1);
  });
  it('flujo completo de liguilla con clasificación y campeón', () => {
    const ps = players(['a', 'b', 'c']);
    let t = createTournament(
      { name: 'Copa', format: 'league', teamSize: 1, ranked: false, config: DEFAULT_CONFIG, teams: ps.map((p) => ({ playerIds: [p.id] })), seeding: 'elo' },
      ps,
      () => 1200,
      0,
    );
    const matches: StoredMatch[] = [];
    let at = 1000;
    while (playableFixtures(t).length) {
      const f = playableFixtures(t)[0];
      const white = t.teams.find((x) => x.id === f.whiteTeamId)!.playerIds;
      const blue = t.teams.find((x) => x.id === f.blueTeamId)!.playerIds;
      // 'a' gana siempre
      const goals = white.includes('a') ? 'W' : blue.includes('a') ? 'B' : 'W';
      const m = { ...makeMatch({ white, blue, goals, at: (at += 1000), mode: 'quick' }), tournament: { id: t.id, fixtureId: f.id } };
      matches.push(m);
      t = recordFixtureResult(t, f.id, m, matches);
    }
    expect(t.status).toBe('finished');
    const standings = leagueStandings(t, matches);
    expect(standings[0].team.playerIds).toEqual(['a']);
    expect(standings[0].points).toBe(6);
    expect(t.teams.find((x) => x.id === t.winnerTeamId)!.playerIds).toEqual(['a']);
    // El premio de torneo se concede una vez al ganador.
    const prog = computeProgression(['a', 'b', 'c'], matches, DEFAULT_PROGRESSION, { tournaments: [t], challenges: false });
    expect(prog.players.get('a')!.tournamentsWon).toBe(1);
    expect(prog.byMatch.get(t.finalMatchId!)!.get('a')?.xpBreakdown.some((l) => l.label === 'Ganar torneo')).toBe(true);
  });
});

describe('Retos', () => {
  it('son deterministas por fecha: 1 diario y 3 semanales distintos', () => {
    const a = activeChallenges(Date.UTC(2026, 9, 2, 12));
    const b = activeChallenges(Date.UTC(2026, 9, 2, 12));
    expect(a.map((c) => c.key)).toEqual(b.map((c) => c.key));
    expect(a.filter((c) => c.scope === 'daily')).toHaveLength(1);
    const weekly = a.filter((c) => c.scope === 'weekly');
    expect(new Set(weekly.map((c) => c.template.id)).size).toBe(3);
  });
  it('el premio de un reto se concede una sola vez', () => {
    const at = new Date(2026, 9, 2, 12).getTime();
    const matches = Array.from({ length: 15 }, (_, i) => makeMatch({ white: ['a'], blue: ['b'], goals: 'WWW', at: at + i * 1000, mode: 'ranked' }));
    const prog = computeProgression(['a', 'b'], matches, DEFAULT_PROGRESSION);
    const done = prog.players.get('a')!.challengesDone;
    expect(new Set(done).size).toBe(done.length);
    expect(done.length).toBeGreaterThan(0);
  });
});

describe('Estadísticas ampliadas', () => {
  it('lados, parejas y duelos', () => {
    const ms = [
      makeMatch({ white: ['a', 'b'], blue: ['c', 'd'], goals: 'W' }),
      makeMatch({ white: ['c', 'd'], blue: ['a', 'b'], goals: 'B' }),
      makeMatch({ white: ['a'], blue: ['c'], goals: 'B' }),
    ];
    expect(sideStats('a', ms).white).toMatchObject({ played: 2, wins: 1 });
    expect(sideStats('a', ms).blue).toMatchObject({ played: 1, wins: 1 });
    const pairs = pairStats(ms);
    expect(pairs[0].playerIds).toEqual(['a', 'b']);
    expect(pairs[0].rec.wins).toBe(2);
    expect(duel('a', 'c', ms)).toMatchObject({ played: 3, aWins: 2, bWins: 1 });
  });
  it('goleadores: 1v1 automático, solo goles válidos', () => {
    const m = makeMatch({ white: ['a'], blue: ['b'], goals: 'WWB' });
    const withScorers = { ...m, scorers: autoScorers(m) };
    expect(personalGoals([withScorers]).get('a')).toBe(2);
  });
  it('1 contra 2: los goles del jugador solo se le asignan; los de la pareja, no', () => {
    const m = makeMatch({ white: ['a'], blue: ['b', 'c'], goals: 'WWB' });
    const scorers = autoScorers(m)!;
    expect(Object.values(scorers)).toEqual(['a', 'a']);
  });
  it('resumen del periodo', () => {
    const ms = [makeMatch({ white: ['a'], blue: ['b'], goals: 'WWWWB', at: 10 }), makeMatch({ white: ['a'], blue: ['b'], goals: 'WB B', at: 20 })];
    const d = periodDigest(ms, players(['a', 'b']), 0, 100);
    expect(d.matches).toBe(2);
    expect(d.playerOfPeriod?.playerId).toBe('a');
    expect(d.biggest?.id).toBe(ms[0].id);
  });
  it('claves de calendario', () => {
    expect(weekKey(new Date(2026, 9, 2).getTime())).toBe('2026-W40');
    expect(seasonKey(new Date(2026, 9, 2).getTime(), 'month')).toBe('2026-10');
    expect(seasonKey(new Date(2026, 9, 2).getTime(), 'quarter')).toBe('2026-T4');
  });
});

describe('Temporadas, logros y títulos', () => {
  it('campeón de cada temporada terminada', () => {
    const sep = new Date(2026, 8, 10).getTime();
    const ms = [makeMatch({ white: ['a'], blue: ['b'], goals: 'W', at: sep }), makeMatch({ white: ['b'], blue: ['a'], goals: 'W', at: new Date(2026, 9, 1).getTime() })];
    const champs = seasonChampions(players(['a', 'b']), ms, DEFAULT_PROGRESSION, 'month', new Date(2026, 9, 2).getTime());
    expect(champs).toEqual([expect.objectContaining({ key: '2026-09', playerId: 'a' })]);
  });
  it('catálogo de ~50 logros con ~10 secretos e ids únicos', () => {
    expect(ACHIEVEMENTS.length).toBeGreaterThanOrEqual(50);
    expect(ACHIEVEMENTS.filter((a) => a.secret).length).toBeGreaterThanOrEqual(10);
    expect(new Set(ACHIEVEMENTS.map((a) => a.id)).size).toBe(ACHIEVEMENTS.length);
  });
  it('título por defecto y elegido', () => {
    const m = makeMatch({ white: ['a'], blue: ['b'], goals: 'WWW' });
    const prog = computeProgression(['a', 'b'], [m], DEFAULT_PROGRESSION, { challenges: false });
    const pa = prog.players.get('a');
    expect(displayTitle(pa)).toBe('El Muro');
    expect(displayTitle(pa, 'none')).toBeNull();
  });
});
