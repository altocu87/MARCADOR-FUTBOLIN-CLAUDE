import { describe, expect, it } from 'vitest';
import {
  DEFAULT_CONFIG,
  createMatch,
  dispatch,
  getScore,
  goalMoment,
  goalStreak,
  matchPointTeams,
  advance,
  getClock,
  handicapRemaining,
  handicapText,
  pickHandicap,
  type EngineCommand,
  type Handicap,
  type MatchConfig,
  type MatchState,
  type ParticipantRef,
  type Visitor,
  goalTarget,
  pickVisitor,
} from '../src/match-engine';

const P2 = [
  { playerId: 'a', team: 'white' as const, slot: 1 as const, nameSnapshot: 'A' },
  { playerId: 'b', team: 'blue' as const, slot: 1 as const, nameSnapshot: 'B' },
];

function run(config: Partial<MatchConfig>) {
  let s: MatchState = createMatch('m', { ...DEFAULT_CONFIG, testMode: false, ...config }, P2, 0);
  let t = 0;
  const api = {
    get s() {
      return s;
    },
    at(ms: number) {
      t = ms;
      return api;
    },
    cmd(c: EngineCommand) {
      const o = dispatch(s, c, t);
      s = o.state;
      return o;
    },
  };
  api.cmd({ type: 'SKIP_COUNTDOWN' });
  return api;
}

describe('Caos: comodín', () => {
  it('el siguiente gol del equipo vale doble y se consume', () => {
    const m = run({ mode: 'chaos', goalsPerPeriod: 10, chaos: { jokers: true, doubleLastMinute: false } });
    expect(m.cmd({ type: 'TOGGLE_JOKER', team: 'white' }).accepted).toBe(true);
    m.at(1000).cmd({ type: 'GOAL', team: 'white' });
    expect(getScore(m.s)).toEqual({ white: 2, blue: 0 });
    expect(m.s.jokers?.white).toBe('used');
    m.at(5000).cmd({ type: 'GOAL', team: 'white' });
    expect(getScore(m.s).white).toBe(3);
    expect(m.cmd({ type: 'TOGGLE_JOKER', team: 'white' }).accepted).toBe(false);
  });
  it('−1 sobre un gol doble resta 2 y deshacer lo restaura', () => {
    const m = run({ mode: 'chaos', goalsPerPeriod: 10, chaos: { jokers: true, doubleLastMinute: false } });
    m.cmd({ type: 'TOGGLE_JOKER', team: 'blue' });
    m.at(1000).cmd({ type: 'GOAL', team: 'blue' });
    m.at(1500).cmd({ type: 'MINUS_ONE', team: 'blue' });
    expect(getScore(m.s).blue).toBe(0);
    m.cmd({ type: 'UNDO' });
    expect(getScore(m.s).blue).toBe(2);
  });
  it('desactivado fuera de Caos', () => {
    const m = run({ mode: 'quick', chaos: { jokers: true, doubleLastMinute: true } });
    expect(m.cmd({ type: 'TOGGLE_JOKER', team: 'white' }).reason).toBe('rule_disabled');
  });
});

describe('Partido Loco: hándicaps', () => {
  const P4 = [
    { playerId: 'a', team: 'white' as const, slot: 1 as const, nameSnapshot: 'Ana' },
    { playerId: 'c', team: 'white' as const, slot: 2 as const, nameSnapshot: 'Carlos' },
    { playerId: 'b', team: 'blue' as const, slot: 1 as const, nameSnapshot: 'Bea' },
    { playerId: 'd', team: 'blue' as const, slot: 2 as const, nameSnapshot: 'Dani' },
  ];
  const loco = (parts: ParticipantRef[] = P2) => {
    let s: MatchState = createMatch('m', { ...DEFAULT_CONFIG, testMode: false, mode: 'chaos', goalsPerPeriod: 20 }, parts, 0);
    s = dispatch(s, { type: 'SKIP_COUNTDOWN' }, 0).state;
    return s;
  };
  const start = (s: MatchState, h: Handicap, t: number) => dispatch(s, { type: 'HANDICAP_START', handicap: h, nextAtMs: t + 45_000 }, t);

  it('solo en Partido Loco', () => {
    const s = run({ mode: 'quick' }).s;
    expect(dispatch(s, { type: 'HANDICAP_START', handicap: { id: 'h', kind: 'double_all', durationMs: 30_000 }, nextAtMs: 0 }, 0).reason).toBe('rule_disabled');
  });

  it('el anuncio para el reloj y los goles dobles duran 30 s de juego', () => {
    let s = loco();
    s = start(s, { id: 'h1', kind: 'double_all', team: 'both', durationMs: 30_000 }, 10_000).state;
    expect(s.phase).toBe('handicap');
    // Tiempo parado durante el anuncio: 5 s después sigue en 10 s de juego.
    expect(getClock(s, 15_000).totalElapsedMs).toBe(10_000);
    s = dispatch(s, { type: 'HANDICAP_GO' }, 15_000).state;
    s = dispatch(s, { type: 'GOAL', team: 'white' }, 16_000).state;
    expect(getScore(s)).toEqual({ white: 2, blue: 0 });
    // A los 30 s de juego termina: «vuelta a la normalidad» con el reloj parado.
    s = advance(s, 45_000).state;
    expect(s.phase).toBe('handicap');
    expect(s.handicap?.stage).toBe('ending');
    expect(s.events.some((e) => e.type === 'HANDICAP_END')).toBe(true);
    // Sigue solo a los 3 s (o al tocar).
    s = advance(s, 48_000).state;
    expect(s.phase).toBe('playing');
    s = dispatch(s, { type: 'GOAL', team: 'white' }, 50_000).state;
    expect(getScore(s).white).toBe(3);
  });

  it('la pausa también para el hándicap', () => {
    let s = loco();
    s = start(s, { id: 'h1', kind: 'freeze_score', team: 'both', durationMs: 15_000 }, 0).state;
    s = dispatch(s, { type: 'HANDICAP_GO' }, 0).state;
    s = dispatch(s, { type: 'PAUSE' }, 5_000).state;
    s = dispatch(s, { type: 'RESUME' }, 60_000).state;
    expect(handicapRemaining(s, 60_000)).toBe(10_000);
  });

  it('marcador congelado: el gol se registra pero vale 0', () => {
    let s = loco();
    s = start(s, { id: 'h1', kind: 'freeze_score', team: 'both', durationMs: 15_000 }, 0).state;
    s = dispatch(s, { type: 'HANDICAP_GO' }, 0).state;
    s = dispatch(s, { type: 'GOAL', team: 'blue' }, 1_000).state;
    expect(getScore(s)).toEqual({ white: 0, blue: 0 });
    expect(s.events.find((e) => e.type === 'GOAL')?.bonus).toEqual(['frozen']);
  });

  it('gol doble solo para un equipo', () => {
    let s = loco();
    s = start(s, { id: 'h1', kind: 'double_team', team: 'blue', durationMs: 30_000 }, 0).state;
    s = dispatch(s, { type: 'HANDICAP_GO' }, 0).state;
    s = dispatch(s, { type: 'GOAL', team: 'white' }, 1_000).state;
    s = dispatch(s, { type: 'GOAL', team: 'blue' }, 5_000).state;
    expect(getScore(s)).toEqual({ white: 1, blue: 2 });
  });

  it('gol triple: vale 3, se gasta y −1 lo resta entero', () => {
    let s = loco();
    s = start(s, { id: 'h1', kind: 'triple_next', team: 'both', durationMs: null }, 0).state;
    s = dispatch(s, { type: 'HANDICAP_GO' }, 0).state;
    s = dispatch(s, { type: 'GOAL', team: 'white' }, 1_000).state;
    expect(getScore(s).white).toBe(3);
    expect(s.handicap).toBeUndefined();
    s = dispatch(s, { type: 'GOAL', team: 'white' }, 5_000).state;
    expect(getScore(s).white).toBe(4);
    s = dispatch(s, { type: 'MINUS_ONE', team: 'white' }, 6_000).state;
    s = dispatch(s, { type: 'MINUS_ONE', team: 'white' }, 7_000).state;
    expect(getScore(s).white).toBe(0);
  });

  it('robo: el gol del que pierde quita uno al rival; anularlo lo devuelve', () => {
    let s = loco();
    s = dispatch(s, { type: 'GOAL', team: 'white' }, 1_000).state;
    s = dispatch(s, { type: 'GOAL', team: 'white' }, 5_000).state;
    s = start(s, { id: 'h1', kind: 'steal', team: 'blue', durationMs: null }, 10_000).state;
    s = dispatch(s, { type: 'HANDICAP_GO' }, 10_000).state;
    s = dispatch(s, { type: 'GOAL', team: 'blue' }, 11_000).state;
    expect(getScore(s)).toEqual({ white: 1, blue: 1 });
    expect(s.events.filter((e) => e.type === 'GOAL').pop()?.scoreAfter).toEqual({ white: 1, blue: 1 });
    s = dispatch(s, { type: 'MINUS_ONE', team: 'blue' }, 12_000).state;
    expect(getScore(s)).toEqual({ white: 2, blue: 0 });
  });

  it('penalti: gol o fallo y se sigue jugando', () => {
    let s = loco();
    s = start(s, { id: 'h1', kind: 'penalty', team: 'blue', durationMs: 0 }, 0).state;
    expect(dispatch(s, { type: 'HANDICAP_GO' }, 0).accepted).toBe(false);
    s = dispatch(s, { type: 'HANDICAP_PENALTY', scored: true }, 2_000).state;
    expect(s.phase).toBe('playing');
    expect(getScore(s)).toEqual({ white: 0, blue: 1 });
    s = start(s, { id: 'h2', kind: 'penalty', team: 'white', durationMs: 0 }, 40_000).state;
    s = dispatch(s, { type: 'HANDICAP_PENALTY', scored: false }, 41_000).state;
    expect(getScore(s)).toEqual({ white: 0, blue: 1 });
  });

  it('un hándicap «hasta el siguiente» termina al anunciar el nuevo', () => {
    let s = loco(P4);
    s = start(s, { id: 'h1', kind: 'swap_positions', team: 'both', durationMs: null }, 0).state;
    s = dispatch(s, { type: 'HANDICAP_GO' }, 0).state;
    s = start(s, { id: 'h2', kind: 'weak_hand', team: 'both', durationMs: 20_000 }, 40_000).state;
    expect(s.handicap?.replaced?.kind).toBe('swap_positions');
    const end = s.events.find((e) => e.type === 'HANDICAP_END');
    expect(end?.reason).toBe('replaced');
  });

  it('sorteo: nunca dobles para el que gana y sin cambios de sitio en 1 contra 1', () => {
    let s = loco();
    s = dispatch(s, { type: 'GOAL', team: 'white' }, 1_000).state;
    let seed = 1;
    const rnd = () => ((seed = (seed * 16807) % 2147483647) - 1) / 2147483646;
    for (let i = 0; i < 300; i += 1) {
      const h = pickHandicap(s, `h${i}`, rnd);
      expect(['swap_positions', 'transfer']).not.toContain(h.kind);
      if (h.kind === 'double_team' || h.kind === 'steal') expect(h.team).toBe('blue');
    }
    const kinds = new Set(Array.from({ length: 300 }, (_, i) => pickHandicap(loco(P4), `x${i}`, rnd).kind));
    expect(kinds.has('swap_positions')).toBe(true);
    expect(kinds.has('transfer')).toBe(true);
  });

  it('los textos llevan los nombres de los jugadores', () => {
    const t = handicapText({ id: 'h', kind: 'frozen_player', team: 'both', players: ['a', 'b'], durationMs: 15_000 }, P4);
    expect(t.detail).toBe('Ana y Bea no se pueden mover durante 15 segundos.');
  });

  it('el hándicap termina con la parte', () => {
    let s = createMatch('m', { ...DEFAULT_CONFIG, testMode: false, mode: 'chaos', endCondition: 'time', minutesPerPeriod: 1 }, P2, 0);
    s = dispatch(s, { type: 'SKIP_COUNTDOWN' }, 0).state;
    s = start(s, { id: 'h1', kind: 'swap_positions', team: 'both', durationMs: null }, 50_000).state;
    s = dispatch(s, { type: 'HANDICAP_GO' }, 50_000).state;
    s = advance(s, 70_000).state;
    expect(s.phase).toBe('periodEnd');
    expect(s.handicap).toBeUndefined();
  });
});

describe('Partido Loco: animales', () => {
  const loco = (cfg: Partial<MatchConfig> = {}) => {
    let s: MatchState = createMatch('m', { ...DEFAULT_CONFIG, testMode: false, mode: 'chaos', goalsPerPeriod: 5, ...cfg }, P2, 0);
    s = dispatch(s, { type: 'SKIP_COUNTDOWN' }, 0).state;
    return s;
  };
  const visit = (s: MatchState, v: Visitor, t: number) => {
    s = dispatch(s, { type: 'VISIT_START', visitor: v }, t).state;
    s = dispatch(s, { type: 'VISIT_APPLY' }, t + 2000).state;
    return dispatch(s, { type: 'VISIT_END' }, t + 5000).state;
  };

  it('ardilla: roba un gol y para el reloj mientras está', () => {
    let s = loco();
    s = dispatch(s, { type: 'GOAL', team: 'white' }, 1000).state;
    s = dispatch(s, { type: 'VISIT_START', visitor: { id: 'v', animal: 'squirrel', from: 'white' } }, 10_000).state;
    expect(s.phase).toBe('visit');
    expect(getClock(s, 14_000).totalElapsedMs).toBe(10_000);
    expect(getScore(s)).toEqual({ white: 1, blue: 0 });
    s = dispatch(s, { type: 'VISIT_APPLY' }, 12_000).state;
    expect(getScore(s)).toEqual({ white: 0, blue: 1 });
    s = dispatch(s, { type: 'VISIT_END' }, 15_000).state;
    expect(s.phase).toBe('playing');
    // Un gol posterior lleva el marcador correcto.
    s = dispatch(s, { type: 'GOAL', team: 'blue' }, 20_000).state;
    expect(s.events.filter((e) => e.type === 'GOAL').pop()?.scoreAfter).toEqual({ white: 0, blue: 2 });
  });

  it('caracol y gato cambian la meta en partidos por goles', () => {
    let s = loco();
    s = visit(s, { id: 'v1', animal: 'snail', goals: 1 }, 10_000);
    expect(goalTarget(s)).toBe(6);
    s = visit(s, { id: 'v2', animal: 'cat', goals: -1 }, 30_000);
    expect(goalTarget(s)).toBe(5);
  });

  it('caracol y gato cambian el reloj en partidos por tiempo', () => {
    let s = loco({ endCondition: 'time', minutesPerPeriod: 3 });
    s = visit(s, { id: 'v1', animal: 'snail', timeMs: 60_000 }, 10_000);
    // 3 min + 1 min; llevamos 10 s jugados.
    expect(getClock(s, 15_000).remainingMs).toBe(230_000);
    s = visit(s, { id: 'v2', animal: 'cat', timeMs: -60_000 }, 30_000);
    // 4 min − 1 min; llevamos 25 s jugados (los 5 s de cada visita no cuentan).
    expect(getClock(s, 35_000).remainingMs).toBe(155_000);
  });

  it('el sorteo nunca decide el partido', () => {
    let s = loco({ goalsPerPeriod: 3 });
    s = dispatch(s, { type: 'GOAL', team: 'white' }, 1000).state;
    s = dispatch(s, { type: 'GOAL', team: 'white' }, 5000).state;
    s = dispatch(s, { type: 'GOAL', team: 'blue' }, 9000).state;
    s = dispatch(s, { type: 'GOAL', team: 'blue' }, 13000).state;
    let seed = 7;
    const rnd = () => ((seed = (seed * 16807) % 2147483647) - 1) / 2147483646;
    for (let i = 0; i < 200; i += 1) {
      const v = pickVisitor(s, `v${i}`, 20_000, rnd);
      // 2–2 a 3 goles: ni ardilla (daría la victoria) ni gato (bajaría la meta a 2).
      expect(v?.animal).toBe('snail');
    }
  });
});

describe('Avisos', () => {
  it('bola de partido a un gol del objetivo', () => {
    const m = run({ goalsPerPeriod: 3 });
    m.at(1000).cmd({ type: 'GOAL', team: 'white' });
    expect(matchPointTeams(m.s)).toEqual([]);
    m.at(5000).cmd({ type: 'GOAL', team: 'white' });
    // 2–0 a 3 goles: Blanco gana si marca.
    expect(matchPointTeams(m.s)).toEqual(['white']);
    m.at(9000).cmd({ type: 'GOAL', team: 'blue' });
    m.at(13000).cmd({ type: 'GOAL', team: 'blue' });
    expect(matchPointTeams(m.s)).toEqual(['white', 'blue']);
  });
  it('racha y remontada', () => {
    const m = run({ goalsPerPeriod: 10 });
    let t = 0;
    for (const team of ['blue', 'blue', 'white', 'white'] as const) {
      t += 4000;
      m.at(t).cmd({ type: 'GOAL', team });
    }
    expect(goalStreak(m.s)).toEqual({ team: 'white', count: 2 });
    const last = m.s.events.filter((e) => e.type === 'GOAL').pop()!;
    expect(goalMoment(m.s, last)).toBe('comeback');
  });
  it('empate y ponerse por delante', () => {
    const m = run({ goalsPerPeriod: 10 });
    m.at(4000).cmd({ type: 'GOAL', team: 'blue' });
    m.at(8000).cmd({ type: 'GOAL', team: 'white' });
    let last = m.s.events.filter((e) => e.type === 'GOAL').pop()!;
    expect(goalMoment(m.s, last)).toBe('equalizer');
    m.at(12000).cmd({ type: 'GOAL', team: 'white' });
    last = m.s.events.filter((e) => e.type === 'GOAL').pop()!;
    expect(goalMoment(m.s, last)).toBe('lead');
  });
});
