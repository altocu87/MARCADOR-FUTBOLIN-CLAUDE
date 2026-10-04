import { describe, expect, it } from 'vitest';
import { DEFAULT_PROGRESSION } from '../src/services/persistence';
import {
  categoryFor,
  computeProgression,
  eloExpected,
  levelForXp,
  predict,
  xpForLevel,
} from '../src/services/progression';
import { comebackSize, computePlayerStats } from '../src/services/statistics';
import { makeMatch } from './helpers';

describe('ELO', () => {
  it('expectativa 0,5 entre iguales; ±10 con K=20', () => {
    expect(eloExpected(1200, 1200)).toBe(0.5);
    const m = makeMatch({ white: ['a'], blue: ['b'], goals: 'WWWWWB' });
    const prog = computeProgression(['a', 'b'], [m], { ...DEFAULT_PROGRESSION, kProvisional: 20 });
    expect(prog.players.get('a')!.elo).toBe(1210);
    expect(prog.players.get('b')!.elo).toBe(1190);
  });
  it('K provisional 40 durante los 10 primeros clasificatorios y 20 después', () => {
    const matches = Array.from({ length: 11 }, (_, i) =>
      makeMatch({ white: ['a'], blue: [`x${i}`], goals: 'W', at: 1_000_000 + i }),
    );
    const prog = computeProgression(['a'], matches, DEFAULT_PROGRESSION);
    const entries = matches.map((m) => prog.byMatch.get(m.id)!.get('a')!);
    expect(entries[0].k).toBe(40);
    expect(entries[0].eloDelta).toBe(20);
    expect(entries[9].k).toBe(40);
    expect(entries[10].k).toBe(20);
  });
  it('solo Clasificatorio modifica ELO', () => {
    const q = makeMatch({ white: ['a'], blue: ['b'], goals: 'W', mode: 'quick' });
    const c = makeMatch({ white: ['a'], blue: ['b'], goals: 'W', mode: 'chaos' });
    const prog = computeProgression(['a', 'b'], [q, c], DEFAULT_PROGRESSION);
    expect(prog.players.get('a')!.elo).toBe(1200);
    expect(prog.players.get('a')!.xp).toBeGreaterThan(0);
  });
  it('2v2 usa la media de cada equipo', () => {
    const warm = makeMatch({ white: ['a'], blue: ['z'], goals: 'W', at: 1 });
    const m = makeMatch({ white: ['a', 'b'], blue: ['c', 'd'], goals: 'B', at: 2 });
    const prog = computeProgression(['a', 'b', 'c', 'd', 'z'], [warm, m], DEFAULT_PROGRESSION);
    const e = prog.byMatch.get(m.id)!;
    // a=1220, b=1200 → media 1210 vs 1200
    const exp = eloExpected(1210, 1200);
    expect(e.get('a')!.eloDelta).toBe(Math.round(40 * (0 - exp)));
    expect(e.get('c')!.eloDelta).toBe(Math.round(40 * (1 - (1 - exp))));
  });
  it('reprocesar no duplica (resultado determinista)', () => {
    const ms = [makeMatch({ white: ['a'], blue: ['b'], goals: 'WBW' }), makeMatch({ white: ['b'], blue: ['a'], goals: 'W' })];
    const p1 = computeProgression(['a', 'b'], ms, DEFAULT_PROGRESSION);
    const p2 = computeProgression(['a', 'b'], [...ms].reverse(), DEFAULT_PROGRESSION);
    expect(p2.players.get('a')).toEqual(p1.players.get('a'));
  });
  it('categorías por umbral', () => {
    expect(categoryFor(1200).name).toBe('Chatarra');
    expect(categoryFor(1225).name).toBe('Madera');
    expect(categoryFor(1275).name).toBe('Bronce');
    expect(categoryFor(1350).name).toBe('Plata');
    expect(categoryFor(1425).name).toBe('Oro');
    expect(categoryFor(1500).name).toBe('Platino');
    expect(categoryFor(1800).name).toBe('Diamante');
  });
});

describe('XP y niveles', () => {
  it('fórmula 100 × N^1,35 acumulada', () => {
    expect(xpForLevel(0)).toBe(0);
    expect(xpForLevel(1)).toBe(100);
    expect(xpForLevel(2)).toBe(Math.round(100 * 2 ** 1.35));
    expect(levelForXp(99)).toBe(0);
    expect(levelForXp(100)).toBe(1);
    expect(levelForXp(10 ** 9)).toBe(100);
  });
  it('victoria clasificatoria: 50 + 100 + 50 + logros únicos una sola vez', () => {
    const m1 = makeMatch({ white: ['a'], blue: ['b'], goals: 'W', at: 1 });
    const m2 = makeMatch({ white: ['a'], blue: ['b'], goals: 'W', at: 2 });
    const prog = computeProgression(['a', 'b'], [m1, m2], DEFAULT_PROGRESSION, { challenges: false });
    const e1 = prog.byMatch.get(m1.id)!.get('a')!;
    const e2 = prog.byMatch.get(m2.id)!.get('a')!;
    expect(e1.unlocked).toEqual(expect.arrayContaining(['debut', 'first_win', 'ranked_debut', 'shutout']));
    expect(e2.unlocked).not.toContain('debut');
    const base = e2.xpBreakdown.filter((l) => !l.label.startsWith('Logro')).reduce((a, l) => a + l.xp, 0);
    expect(base).toBe(200); // 50 + 100 + 50
    expect(e2.unlocked.filter((id) => e1.unlocked.includes(id))).toEqual([]);
  });
});

describe('Estadísticas', () => {
  it('sin partidos: estado vacío sin división por cero', () => {
    const s = computePlayerStats('a', []);
    expect(s.general.winPct).toBeNull();
    expect(s.favoriteRival).toBeNull();
  });
  it('goles del equipo mientras participaba y rachas', () => {
    const ms = [
      makeMatch({ white: ['a', 'b'], blue: ['c', 'd'], goals: 'WWB', at: 1 }),
      makeMatch({ white: ['a'], blue: ['c'], goals: 'BWB', at: 2 }),
      makeMatch({ white: ['c'], blue: ['a'], goals: 'B', at: 3 }),
    ];
    const s = computePlayerStats('a', ms);
    expect(s.general.played).toBe(3);
    expect(s.general.wins).toBe(2);
    expect(s.general.goalsFor).toBe(2 + 1 + 1);
    expect(s.general.goalsAgainst).toBe(1 + 2 + 0);
    expect(s.currentStreak).toEqual({ type: 'G', count: 1 });
    expect(s.bestWinStreak).toBe(1);
  });
  it('rival favorito y némesis solo con 5+ enfrentamientos', () => {
    const ms = Array.from({ length: 4 }, (_, i) => makeMatch({ white: ['a'], blue: ['b'], goals: 'W', at: i }));
    expect(computePlayerStats('a', ms).favoriteRival).toBeNull();
    ms.push(makeMatch({ white: ['a'], blue: ['b'], goals: 'B', at: 10 }));
    const s = computePlayerStats('a', ms);
    expect(s.favoriteRival?.playerId).toBe('b');
    expect(s.favoriteRival?.winPct).toBe(80);
  });
  it('remontada', () => {
    expect(comebackSize(makeMatch({ white: ['a'], blue: ['b'], goals: 'BBWWW' }))).toBe(2);
  });
});

describe('Previsión', () => {
  it('sin datos clasificatorios: no disponible', () => {
    const prog = computeProgression(['a', 'b'], [], DEFAULT_PROGRESSION);
    expect(predict(['a'], ['b'], [], prog).available).toBe(false);
  });
  it('favorece al de más ELO y suma 100', () => {
    const ms = Array.from({ length: 6 }, (_, i) => makeMatch({ white: ['a'], blue: ['b'], goals: 'W', at: i }));
    const prog = computeProgression(['a', 'b'], ms, DEFAULT_PROGRESSION);
    const p = predict(['a'], ['b'], ms, prog);
    expect(p.available).toBe(true);
    expect(p.whitePct).toBeGreaterThan(50);
    expect(p.whitePct + p.bluePct).toBe(100);
    expect(p.confidence).toBe('media');
    expect(p.weights.elo + p.weights.h2h + p.weights.form).toBeCloseTo(1);
  });
});
