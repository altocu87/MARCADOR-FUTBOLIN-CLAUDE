import { describe, expect, it } from 'vitest';
import {
  DEFAULT_CONFIG,
  EngineError,
  createMatch,
  dispatch,
  advance,
  getScore,
  getClock,
  getPenaltyScore,
  nextPenaltyTeam,
  type EngineCommand,
  type MatchConfig,
  type MatchState,
  type ParticipantRef,
} from '../src/match-engine';

const P2: ParticipantRef[] = [
  { playerId: 'a', team: 'white', slot: 1, nameSnapshot: 'Ana' },
  { playerId: 'b', team: 'blue', slot: 1, nameSnapshot: 'Beto' },
];
const P4: ParticipantRef[] = [
  ...P2,
  { playerId: 'c', team: 'white', slot: 2, nameSnapshot: 'Carla' },
  { playerId: 'd', team: 'blue', slot: 2, nameSnapshot: 'Dani' },
];

function cfg(over: Partial<MatchConfig> = {}): MatchConfig {
  return { ...DEFAULT_CONFIG, testMode: false, ...over };
}

/** Pequeño arnés: mantiene el estado y el tiempo simulado. */
class Sim {
  state: MatchState;
  t = 0;
  constructor(config: MatchConfig, participants = P2) {
    this.state = createMatch('m1', config, participants, this.t);
  }
  at(t: number) {
    this.t = t;
    this.state = advance(this.state, t).state;
    return this;
  }
  wait(ms: number) {
    return this.at(this.t + ms);
  }
  cmd(c: EngineCommand) {
    const out = dispatch(this.state, c, this.t);
    this.state = out.state;
    return out;
  }
  goal(team: 'white' | 'blue') {
    return this.cmd({ type: 'GOAL', team });
  }
  /** Salta la cuenta atrás. */
  start() {
    expect(this.cmd({ type: 'SKIP_COUNTDOWN' }).accepted).toBe(true);
    return this;
  }
  /** Marca un gol esperando antes el bloqueo. */
  scoreAfterLock(team: 'white' | 'blue') {
    this.wait(3000);
    const out = this.goal(team);
    expect(out.accepted).toBe(true);
    return out;
  }
}

/** Configuración por tiempo (dos partes de 1 minuto). */
const TIME = { endCondition: 'time' as const, minutesPerPeriod: 1 };

/** Deja correr el reloj hasta el final de la parte en curso. */
function toPeriodEnd(s: Sim): Sim {
  const remaining = getClock(s.state, s.t).remainingMs;
  if (remaining === null) throw new Error('La parte no tiene límite de tiempo');
  s.wait(remaining);
  expect(['periodEnd', 'finished']).toContain(s.state.phase);
  return s;
}

describe('A01 · partido válido completo', () => {
  it('por goles: una sola parte y gana el primero que llega al objetivo', () => {
    const s = new Sim(cfg({ goalsPerPeriod: 3 }));
    expect(s.state.phase).toBe('countdown');
    s.wait(2999);
    expect(s.state.phase).toBe('countdown');
    s.wait(1);
    expect(s.state.phase).toBe('playing');
    expect(s.state.period).toBe('first');
    for (const team of ['white', 'blue', 'white', 'blue'] as const) s.scoreAfterLock(team);
    expect(s.state.phase).toBe('playing');
    s.scoreAfterLock('white');
    expect(s.state.phase).toBe('finished');
    expect(s.state.result?.winner).toBe('white');
    expect(s.state.result?.reason).toBe('regulation');
    expect(s.state.result?.score).toEqual({ white: 3, blue: 2 });
    expect(s.state.periods).toHaveLength(1);
    expect(s.state.periods[0].endReason).toBe('goals');
  });
  it('por tiempo: dos partes y gana quien suma más goles', () => {
    const s = new Sim(cfg(TIME)).start();
    s.scoreAfterLock('white');
    s.scoreAfterLock('white');
    toPeriodEnd(s);
    expect(s.cmd({ type: 'CONTINUE' }).accepted).toBe(true);
    expect(s.state.phase).toBe('countdown');
    expect(s.state.period).toBe('second');
    s.start();
    s.scoreAfterLock('blue');
    toPeriodEnd(s);
    expect(s.state.phase).toBe('finished');
    expect(s.state.result?.winner).toBe('white');
    expect(s.state.result?.reason).toBe('regulation');
    expect(s.state.result?.score).toEqual({ white: 2, blue: 1 });
    expect(s.state.periods.map((p) => p.score)).toEqual([
      { white: 2, blue: 0 },
      { white: 0, blue: 1 },
    ]);
  });
});

describe('A02 · configuraciones y selecciones inválidas', () => {
  it('rechaza configuraciones fuera de rango o no enteras', () => {
    expect(() => createMatch('x', cfg({ goalsPerPeriod: 0 }), P2, 0)).toThrow(EngineError);
    expect(() => createMatch('x', cfg({ goalsPerPeriod: 21 }), P2, 0)).toThrow(EngineError);
    expect(() => createMatch('x', cfg({ minutesPerPeriod: 31 }), P2, 0)).toThrow(EngineError);
    expect(() => createMatch('x', cfg({ minutesPerPeriod: 1.5 }), P2, 0)).toThrow(EngineError);
  });
  it('rechaza equipos vacíos, plazas repetidas y duplicados', () => {
    expect(() => createMatch('x', cfg(), P2.slice(0, 1), 0)).toThrow(EngineError);
    const five = [...P4, { playerId: 'e', team: 'white' as const, slot: 2 as const, nameSnapshot: 'E' }];
    expect(() => createMatch('x', cfg(), five, 0)).toThrow(EngineError);
    const dup = [P2[0], { ...P2[1], playerId: 'a' }];
    expect(() => createMatch('x', cfg(), dup, 0)).toThrow(EngineError);
    const unbalanced = [P2[0], { ...P2[1], team: 'white' as const, slot: 2 as const }];
    expect(() => createMatch('x', cfg(), unbalanced, 0)).toThrow(EngineError);
  });
  it('acepta 1v1 y 2v2', () => {
    expect(createMatch('x', cfg(), P2, 0).phase).toBe('countdown');
    expect(createMatch('x', cfg(), P4, 0).participants).toHaveLength(4);
  });
  it('acepta de 1 a 4 por equipo, también desiguales, y rechaza 5', () => {
    const team = (t: 'white' | 'blue', n: number) =>
      Array.from({ length: n }, (_, i) => ({ playerId: `${t}${i}`, team: t, slot: (i + 1) as 1 | 2 | 3 | 4, nameSnapshot: `${t}${i}` }));
    expect(createMatch('x', cfg(), [...team('white', 3), ...team('blue', 3)], 0).participants).toHaveLength(6);
    expect(createMatch('x', cfg(), [...team('white', 4), ...team('blue', 4)], 0).participants).toHaveLength(8);
    expect(createMatch('x', cfg(), [...team('white', 3), ...team('blue', 1)], 0).participants).toHaveLength(4);
    expect(createMatch('x', cfg(), P4.slice(0, 3), 0).participants).toHaveLength(3);
    expect(() => createMatch('x', cfg(), [...team('white', 5), ...team('blue', 5)], 0)).toThrow(EngineError);
  });
});

describe('A03 · POR GOLES', () => {
  it('cuentan los goles de cada equipo, no la suma de ambos', () => {
    const s = new Sim(cfg({ goalsPerPeriod: 5 })).start();
    for (let i = 0; i < 4; i += 1) {
      s.scoreAfterLock('white');
      s.scoreAfterLock('blue');
    }
    expect(s.state.phase).toBe('playing');
    expect(getScore(s.state)).toEqual({ white: 4, blue: 4 });
    s.scoreAfterLock('blue');
    expect(s.state.phase).toBe('finished');
    expect(s.state.result?.winner).toBe('blue');
    expect(s.state.events.some((e) => e.type === 'OVERTIME_START')).toBe(false);
  });
  it('un gol anulado no da la victoria', () => {
    const s = new Sim(cfg({ goalsPerPeriod: 2 })).start();
    s.scoreAfterLock('white');
    s.cmd({ type: 'MINUS_ONE', team: 'white' });
    s.scoreAfterLock('white');
    expect(s.state.phase).toBe('playing');
    s.scoreAfterLock('white');
    expect(s.state.phase).toBe('finished');
  });
});

describe('A04 · POR GOLES no termina por tiempo; POR TIEMPO sí', () => {
  it('POR GOLES sigue jugando tras mucho tiempo', () => {
    const s = new Sim(cfg({ endCondition: 'goals', minutesPerPeriod: 1 })).start();
    s.wait(60 * 60_000);
    expect(s.state.phase).toBe('playing');
    expect(getClock(s.state, s.t).remainingMs).toBeNull();
  });
  it('POR TIEMPO termina al agotarse el reloj, sin importar goles', () => {
    const s = new Sim(cfg({ endCondition: 'time', minutesPerPeriod: 1, goalsPerPeriod: 1 })).start();
    s.scoreAfterLock('white');
    s.scoreAfterLock('white');
    expect(s.state.phase).toBe('playing');
    s.at(60_000 - 1);
    expect(s.state.phase).toBe('playing');
    s.at(60_000);
    expect(s.state.phase).toBe('periodEnd');
    expect(s.state.periods[0].durationMs).toBe(60_000);
    expect(s.state.periods[0].endReason).toBe('time');
  });
});

describe('A05 · AMBAS', () => {
  it('llegar al objetivo de goles gana el partido en ese momento', () => {
    const s = new Sim(cfg({ endCondition: 'both', goalsPerPeriod: 2, minutesPerPeriod: 5 })).start();
    s.scoreAfterLock('white');
    s.scoreAfterLock('blue');
    expect(s.state.phase).toBe('playing');
    s.scoreAfterLock('blue');
    expect(s.state.phase).toBe('finished');
    expect(s.state.result?.winner).toBe('blue');
    expect(s.state.periods[0].endReason).toBe('goals');
  });
  it('termina por tiempo si llega primero', () => {
    const s = new Sim(cfg({ endCondition: 'both', goalsPerPeriod: 10, minutesPerPeriod: 1 })).start();
    s.at(60_000);
    expect(s.state.phase).toBe('periodEnd');
    expect(s.state.periods[0].endReason).toBe('time');
  });
  it('un gol en el instante límite se rechaza: el tiempo se procesa primero', () => {
    const s = new Sim(cfg({ endCondition: 'both', goalsPerPeriod: 10, minutesPerPeriod: 1 })).start();
    s.t = 60_000;
    const out = s.goal('white');
    expect(out.accepted).toBe(false);
    expect(out.reason).toBe('invalid_state');
    expect(s.state.phase).toBe('periodEnd');
    expect(getScore(s.state)).toEqual({ white: 0, blue: 0 });
    // Un milisegundo antes sí se acepta.
    const s2 = new Sim(cfg({ endCondition: 'both', goalsPerPeriod: 10, minutesPerPeriod: 1 })).start();
    s2.t = 59_999;
    expect(s2.goal('white').accepted).toBe(true);
  });
});

describe('A06 · goles fuera de estado de juego', () => {
  it('rechaza antes de iniciar, en pausa, en final de parte y con el partido terminado', () => {
    const s = new Sim(cfg(TIME));
    expect(s.goal('white').reason).toBe('invalid_state');
    s.start();
    s.cmd({ type: 'PAUSE' });
    expect(s.goal('white').reason).toBe('invalid_state');
    s.cmd({ type: 'RESUME' });
    toPeriodEnd(s);
    s.wait(5000);
    expect(s.goal('blue').reason).toBe('invalid_state');
    const f = new Sim(cfg({ goalsPerPeriod: 1 })).start();
    f.scoreAfterLock('white');
    expect(f.state.phase).toBe('finished');
    f.wait(5000);
    expect(f.goal('blue').reason).toBe('invalid_state');
  });
});

describe('A07 · bloqueo de 3.000 ms', () => {
  it('caso de aceptación del documento', () => {
    const s = new Sim(cfg({ goalsPerPeriod: 20 })).start();
    s.wait(10_000);
    const t0 = s.t;
    expect(s.goal('white').accepted).toBe(true);
    s.at(t0 + 500);
    expect(s.cmd({ type: 'GOAL', team: 'blue', source: 'button' }).reason).toBe('goal_lock');
    s.at(t0 + 1000);
    expect(s.cmd({ type: 'GOAL', team: 'white', source: 'sensor' }).reason).toBe('goal_lock');
    s.at(t0 + 2900);
    expect(s.cmd({ type: 'GOAL', team: 'blue', source: 'touch' }).reason).toBe('goal_lock');
    s.at(t0 + 3000);
    expect(s.goal('blue').accepted).toBe(true);
    expect(getScore(s.state)).toEqual({ white: 1, blue: 1 });
  });
  it('entradas simultáneas: solo la primera cuenta', () => {
    const s = new Sim(cfg({ goalsPerPeriod: 20 })).start();
    s.wait(5000);
    expect(s.goal('white').accepted).toBe(true);
    expect(s.goal('blue').accepted).toBe(false);
    expect(s.goal('white').accepted).toBe(false);
    expect(getScore(s.state)).toEqual({ white: 1, blue: 0 });
  });
});

describe('A08 · nada elude el bloqueo', () => {
  it('deshacer y −1 no abren una vía', () => {
    const s = new Sim(cfg({ goalsPerPeriod: 20 })).start();
    s.wait(5000);
    const t0 = s.t;
    s.goal('white');
    s.at(t0 + 100);
    expect(s.cmd({ type: 'UNDO' }).accepted).toBe(true);
    expect(s.goal('blue').reason).toBe('goal_lock');
    s.at(t0 + 3000);
    s.goal('white');
    s.at(t0 + 3100);
    expect(s.cmd({ type: 'MINUS_ONE', team: 'white' }).accepted).toBe(true);
    expect(s.goal('white').reason).toBe('goal_lock');
  });
  it('pausa/continuar no abren una vía', () => {
    const s = new Sim(cfg({ goalsPerPeriod: 20 })).start();
    s.wait(5000);
    const t0 = s.t;
    s.goal('white');
    s.at(t0 + 200);
    s.cmd({ type: 'PAUSE' });
    s.at(t0 + 400);
    s.cmd({ type: 'RESUME' });
    s.at(t0 + 600);
    expect(s.goal('blue').reason).toBe('goal_lock');
  });
  it('cambio de parte y salto de cuenta atrás no abren una vía', () => {
    const s = new Sim(cfg(TIME)).start();
    s.at(58_000);
    const t0 = s.t;
    expect(s.goal('white').accepted).toBe(true);
    s.at(60_000);
    expect(s.state.phase).toBe('periodEnd');
    s.at(t0 + 2200);
    s.cmd({ type: 'CONTINUE' });
    s.at(t0 + 2300);
    s.start();
    s.at(t0 + 2500);
    expect(s.goal('blue').reason).toBe('goal_lock');
    s.at(t0 + 3000);
    expect(s.goal('blue').accepted).toBe(true);
  });
});

describe('A09 · correcciones', () => {
  it('no generan resultado negativo', () => {
    const s = new Sim(cfg()).start();
    expect(s.cmd({ type: 'MINUS_ONE', team: 'white' }).reason).toBe('no_goal_to_remove');
    expect(getScore(s.state)).toEqual({ white: 0, blue: 0 });
  });
  it('no retroceden el reloj y dejan cronología trazable', () => {
    const s = new Sim(cfg({ endCondition: 'time', minutesPerPeriod: 5 })).start();
    s.scoreAfterLock('white');
    const goal = s.state.events.find((e) => e.type === 'GOAL')!;
    s.wait(1000);
    const before = getClock(s.state, s.t).periodElapsedMs;
    s.cmd({ type: 'MINUS_ONE', team: 'white' });
    expect(getClock(s.state, s.t).periodElapsedMs).toBe(before);
    expect(getScore(s.state)).toEqual({ white: 0, blue: 0 });
    const corr = s.state.events.find((e) => e.type === 'CORRECTION')!;
    expect(corr.refEventId).toBe(goal.id);
    // El gol original sigue en la cronología.
    expect(s.state.events.some((e) => e.id === goal.id)).toBe(true);
    // Deshacer la corrección restaura el gol.
    s.cmd({ type: 'UNDO' });
    expect(getScore(s.state)).toEqual({ white: 1, blue: 0 });
    const seqs = s.state.events.map((e) => e.seq);
    expect(seqs).toEqual([...seqs].sort((a, b) => a - b));
  });
  it('−1 solo actúa sobre el periodo actual', () => {
    const s = new Sim(cfg(TIME)).start();
    s.scoreAfterLock('white');
    toPeriodEnd(s);
    s.cmd({ type: 'CONTINUE' });
    s.start();
    expect(s.cmd({ type: 'MINUS_ONE', team: 'white' }).reason).toBe('no_goal_to_remove');
    expect(s.cmd({ type: 'UNDO' }).reason).toBe('nothing_to_undo');
  });
  it('correcciones no permitidas fuera de estado', () => {
    const s = new Sim(cfg(TIME)).start();
    s.scoreAfterLock('white');
    toPeriodEnd(s);
    expect(s.cmd({ type: 'MINUS_ONE', team: 'white' }).reason).toBe('invalid_state');
    expect(s.cmd({ type: 'UNDO' }).reason).toBe('invalid_state');
  });
});

/** Empate al final de la 2ª parte (por tiempo). */
function toOvertime(): Sim {
  const s = new Sim(cfg(TIME)).start();
  s.scoreAfterLock('white');
  toPeriodEnd(s);
  s.cmd({ type: 'CONTINUE' });
  s.start();
  s.scoreAfterLock('blue');
  toPeriodEnd(s);
  expect(s.state.period).toBe('second');
  s.cmd({ type: 'CONTINUE' });
  expect(s.state.period).toBe('overtime');
  expect(s.state.phase).toBe('countdown');
  s.start();
  return s;
}

describe('A10 · prórroga', () => {
  it('empate lleva a 60 s de prórroga y el primer gol gana', () => {
    const s = toOvertime();
    expect(getClock(s.state, s.t).remainingMs).toBe(60_000);
    s.scoreAfterLock('blue');
    expect(s.state.phase).toBe('finished');
    expect(s.state.result).toMatchObject({ winner: 'blue', reason: 'golden_goal', score: { white: 1, blue: 2 } });
  });
});

describe('A11 · prórroga sin gol → penaltis', () => {
  it('lleva a penaltis con turnos alternos validados', () => {
    const s = toOvertime();
    s.wait(60_000);
    expect(s.state.phase).toBe('periodEnd');
    s.cmd({ type: 'CONTINUE' });
    expect(s.state.phase).toBe('penalties');
    expect(nextPenaltyTeam(s.state)).toBe('white');
    expect(s.cmd({ type: 'PENALTY', team: 'blue', scored: true }).reason).toBe('wrong_turn');
    expect(s.cmd({ type: 'PENALTY', team: 'white', scored: true }).accepted).toBe(true);
    expect(nextPenaltyTeam(s.state)).toBe('blue');
    expect(s.cmd({ type: 'PENALTY', team: 'white', scored: true }).reason).toBe('wrong_turn');
    // No se pueden marcar goles ordinarios en la tanda.
    expect(s.goal('white').reason).toBe('invalid_state');
  });
});

function toShootout(over: Partial<MatchConfig> = {}): Sim {
  const s = new Sim(cfg({ ...TIME, ...over })).start();
  s.scoreAfterLock('white');
  toPeriodEnd(s);
  s.cmd({ type: 'CONTINUE' });
  s.start();
  s.scoreAfterLock('blue');
  toPeriodEnd(s);
  s.cmd({ type: 'CONTINUE' });
  s.start();
  s.wait(60_000);
  s.cmd({ type: 'CONTINUE' });
  expect(s.state.phase).toBe('penalties');
  return s;
}

function kicks(s: Sim, seq: string) {
  // 'G' = gol, 'F' = fallo, en orden de lanzamiento
  for (const ch of seq) {
    const out = s.cmd({ type: 'PENALTY', team: nextPenaltyTeam(s.state), scored: ch === 'G' });
    expect(out.accepted).toBe(true);
  }
}

describe('A12 · resolución de la tanda', () => {
  it('final anticipado cuando un equipo ya no puede alcanzar', () => {
    const s = toShootout();
    // Blanco G, Azul F ×3 → Blanco 3, Azul 0 tras 3 cada uno; a Azul le quedan 2 → decidida
    kicks(s, 'GFGFGF');
    expect(s.state.phase).toBe('finished');
    expect(s.state.result?.winner).toBe('white');
    expect(s.state.penalties).toHaveLength(6);
  });
  it('final anticipado incluso a mitad de ronda', () => {
    const s = toShootout();
    kicks(s, 'GFGFG');
    // W 3 (3 tiros), B 0 (2 tiros, quedan 3) → aún no
    expect(s.state.phase).toBe('penalties');
    kicks(s, 'F');
    // W 3 (3), B 0 (3, quedan 2) → 0+2 < 3: gana Blanco
    expect(s.state.phase).toBe('finished');
  });
  it('ganador al terminar la ronda inicial', () => {
    const s = toShootout();
    kicks(s, 'GGGGGGGGGF');
    expect(s.state.result?.winner).toBe('white');
    expect(s.state.result?.penaltyScore).toEqual({ white: 5, blue: 4 });
  });
  it('muerte súbita por parejas', () => {
    const s = toShootout();
    kicks(s, 'GGGGGGGGGG');
    expect(s.state.phase).toBe('penalties');
    kicks(s, 'G');
    expect(s.state.phase).toBe('penalties'); // no se decide hasta que tira Azul
    kicks(s, 'G');
    expect(s.state.phase).toBe('penalties');
    kicks(s, 'F');
    expect(s.state.phase).toBe('penalties');
    kicks(s, 'G');
    expect(s.state.phase).toBe('finished');
    expect(s.state.result?.winner).toBe('blue');
    expect(s.state.penalties.filter((k) => k.suddenDeath)).toHaveLength(4);
  });
  it('equipo inicial configurable', () => {
    const s = toShootout({ penaltyFirstTeam: 'blue' });
    expect(nextPenaltyTeam(s.state)).toBe('blue');
  });
  it('deshacer el último lanzamiento mientras no está decidida', () => {
    const s = toShootout();
    kicks(s, 'G');
    expect(s.cmd({ type: 'UNDO_PENALTY' }).accepted).toBe(true);
    expect(s.state.penalties).toHaveLength(0);
    expect(nextPenaltyTeam(s.state)).toBe('white');
  });
});

describe('A13 · resumen con ganador por penaltis', () => {
  it('el marcador ordinario sigue empatado y los penaltis van aparte', () => {
    const s = toShootout();
    kicks(s, 'GGGGGGGFG');
    expect(s.state.result).toMatchObject({
      winner: 'white',
      reason: 'penalties',
      score: { white: 1, blue: 1 },
      penaltyScore: { white: 5, blue: 3 },
    });
    expect(getScore(s.state)).toEqual({ white: 1, blue: 1 });
    expect(getPenaltyScore(s.state)).toEqual({ white: 5, blue: 3 });
  });
});

describe('Reloj y pausa', () => {
  it('la pausa detiene el tiempo y continuar conserva el restante', () => {
    const s = new Sim(cfg({ endCondition: 'time', minutesPerPeriod: 1 })).start();
    s.wait(10_000);
    s.cmd({ type: 'PAUSE' });
    s.wait(120_000);
    expect(s.state.phase).toBe('paused');
    expect(getClock(s.state, s.t).remainingMs).toBe(50_000);
    s.cmd({ type: 'RESUME' });
    s.wait(49_999);
    expect(s.state.phase).toBe('playing');
    s.wait(1);
    expect(s.state.phase).toBe('periodEnd');
  });
  it('la cuenta atrás termina sola a los 3 s y el reloj arranca en ese instante', () => {
    const s = new Sim(cfg({ endCondition: 'time', minutesPerPeriod: 1 }));
    s.at(4000);
    expect(s.state.phase).toBe('playing');
    expect(getClock(s.state, s.t).periodElapsedMs).toBe(1000);
  });
  it('distingue tiempo por periodo y tiempo acumulado', () => {
    const s = new Sim(cfg({ endCondition: 'time', minutesPerPeriod: 1 })).start();
    s.wait(60_000);
    s.cmd({ type: 'CONTINUE' });
    s.start();
    s.wait(15_000);
    const c = getClock(s.state, s.t);
    expect(c.periodElapsedMs).toBe(15_000);
    expect(c.totalElapsedMs).toBe(75_000);
  });
});

describe('Cartel de fase antes de la cuenta atrás', () => {
  it('la intro alarga la cuenta atrás y SKIP_INTRO la recorta a la cuenta normal', () => {
    const plain = createMatch('m0', cfg(), P2, 0);
    const s = createMatch('m1', cfg(), P2, 0, 3500);
    expect(s.countdownEndsAt).toBe(plain.countdownEndsAt! + 3500);
    const out = dispatch(s, { type: 'SKIP_INTRO' }, 1000);
    expect(out.accepted).toBe(true);
    expect(out.state.countdownEndsAt).toBe(1000 + plain.countdownEndsAt!);
    expect(out.state.phase).toBe('countdown');
  });
});
