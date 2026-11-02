import { describe, expect, it } from 'vitest';
import {
  DEFAULT_CONFIG,
  advance,
  createMatch,
  dispatch,
  type EngineCommand,
  type MatchConfig,
  type MatchEvent,
  type MatchState,
  type ParticipantRef,
} from '../src/match-engine';
import {
  AmbienceClock,
  CATALOG,
  CommentaryDeck,
  P,
  commentMoment,
  goalMask,
  previewDue,
  shootoutBeforeMask,
  type Mask,
} from '../src/services/sound/commentary';

const PEOPLE: ParticipantRef[] = [
  { playerId: 'a', team: 'white', slot: 1, nameSnapshot: 'Ana' },
  { playerId: 'b', team: 'blue', slot: 1, nameSnapshot: 'Beto' },
];
const cfg = (over: Partial<MatchConfig> = {}): MatchConfig => ({
  ...DEFAULT_CONFIG,
  testMode: false,
  mode: 'ranked',
  endCondition: 'time',
  minutesPerPeriod: 1,
  goalsPerPeriod: 20,
  overtimeSeconds: 30,
  penaltyRounds: 3,
  ...over,
});

/** Partido simulado: cada paso guarda el antes, el después y los eventos, como `commit` de la app. */
class Sim {
  state: MatchState;
  t = 0;
  prev!: MatchState;
  events: MatchEvent[] = [];
  constructor(config: MatchConfig) {
    this.state = createMatch('m', config, PEOPLE, 0);
    this.cmd({ type: 'SKIP_COUNTDOWN' });
  }
  private set(next: MatchState, events: MatchEvent[]) {
    this.prev = this.state;
    this.state = next;
    this.events = events;
  }
  cmd(c: EngineCommand) {
    const out = dispatch(this.state, c, this.t);
    this.set(out.state, out.events);
    return this;
  }
  wait(ms: number) {
    this.t += ms;
    const out = advance(this.state, this.t);
    this.set(out.state, out.events);
    return this;
  }
  goals(seq: string) {
    for (const ch of seq) this.wait(3100).cmd({ type: 'GOAL', team: ch === 'W' ? 'white' : 'blue' });
    return this;
  }
  get moment() {
    return commentMoment(this.prev, this.state, this.events);
  }
  get goalPools() {
    const team = this.events.find((e) => e.type === 'GOAL')!.team!;
    return names(goalMask(this.prev, this.state, team));
  }
}
const names = (m: Mask) => [...m].sort((a, b) => a - b).map((i) => Object.keys(P).find((k) => P[k as keyof typeof P] === i));

describe('comentarios del locutor en la app: las reglas de la pantalla', () => {
  it('catálogo: los 806 activos, en sus grupos, sin los reservados de torneo', () => {
    expect(CATALOG.flat().length).toBe(806);
    expect(CATALOG[P.Comodin].length).toBe(112);
    expect(CATALOG[P.Anulado].length).toBe(48);
    expect(CATALOG[P.Ambience].length).toBe(211);
    expect(CATALOG[P.ShootoutChampion].length).toBe(4);
    expect(new Set(CATALOG.flat()).size).toBe(806);
  });

  it('cada gol cae en sus grupos', () => {
    expect(new Sim(cfg()).goals('W').goalPools).toEqual(['Comodin', 'VaGanando']);
    expect(new Sim(cfg()).goals('WB').goalPools).toEqual(['Comodin', 'Empate']);
    expect(new Sim(cfg()).goals('WWW').goalPools).toEqual(['Comodin', 'Ventaja2', 'VaGanando', 'Lead3']);
    // Responde tras dos seguidos del rival y sigue perdiendo.
    expect(new Sim(cfg()).goals('BBW').goalPools).toEqual(['Comodin', 'Respuesta2', 'SiguePerdiendo']);
    // Remontada: se pone por delante tras haber ido perdiendo (aunque sea de uno).
    expect(new Sim(cfg()).goals('BWW').goalPools).toEqual(['Comodin', 'VaGanando', 'Remontada']);
    // Empata tras ir perdiendo por tres, y ya había encajado tres (y con dos goles en los 60 s de antes).
    expect(new Sim(cfg()).goals('BBBWWW').goalPools).toEqual(['Comodin', 'Encajados3', 'Empate', 'EmpateMovido', 'EmpateTras3']);
    // Pierde de cuatro o más (y responde tras cinco seguidos).
    expect(new Sim(cfg()).goals('BBBBBW').goalPools).toEqual(['Comodin', 'Encajados3', 'Respuesta2', 'SiguePerdiendo', 'Pierde4']);
  });

  it('empate con dos goles en los 60 s de antes; en el Partido Loco no hay remontadas', () => {
    expect(new Sim(cfg()).goals('WBWB').goalPools).toContain('EmpateMovido');
    expect(new Sim(cfg({ mode: 'chaos' })).goals('BWW').goalPools).toEqual(['Comodin', 'VaGanando']);
  });

  it('gol en los últimos 20 s de la 2.ª parte (nunca en la 1.ª ni a goles)', () => {
    const s = new Sim(cfg()).wait(61_000).cmd({ type: 'CONTINUE' }).wait(3000);
    expect(s.state.period).toBe('second');
    s.wait(42_000).goals('W');
    expect(s.goalPools).toContain('Ultimos20');
    const first = new Sim(cfg()).wait(45_000).goals('W');
    expect(first.goalPools).not.toContain('Ultimos20');
  });

  it('anulado, descanso, 2.ª parte, final por tiempo, prórroga y tanda', () => {
    const s = new Sim(cfg()).goals('W').cmd({ type: 'MINUS_ONE', team: 'white' });
    expect(s.moment?.kind).toBe('annulled');
    s.goals('W').wait(60_000);
    expect(s.moment).toMatchObject({ kind: 'phase', event: 'periodEnd' });
    expect(names((s.moment as { mask: Mask }).mask)).toEqual(['Descanso']);
    s.cmd({ type: 'CONTINUE' });
    if (s.state.phase === 'countdown') s.cmd({ type: 'SKIP_COUNTDOWN' });
    expect(names((s.moment as { mask: Mask }).mask)).toEqual(['SecondHalf', 'SecondHalfFew']);
    s.wait(61_000);
    expect(s.state.phase).toBe('finished');
    expect(names((s.moment as { mask: Mask }).mask)).toEqual(['FinalNormal']);

    // Empate al acabar la 2.ª: a la prórroga; empate en la prórroga: a penaltis.
    const t = new Sim(cfg()).wait(61_000).cmd({ type: 'CONTINUE' });
    if (t.state.phase === 'countdown') t.cmd({ type: 'SKIP_COUNTDOWN' });
    t.wait(61_000);
    expect(names((t.moment as { mask: Mask }).mask)).toEqual(['ExtraTimeAnnounce']);
    t.cmd({ type: 'CONTINUE' });
    if (t.state.phase === 'countdown') t.cmd({ type: 'SKIP_COUNTDOWN' });
    t.wait(31_000);
    expect(names((t.moment as { mask: Mask }).mask)).toEqual(['ShootoutAnnounce']);
    t.cmd({ type: 'CONTINUE' });
    expect(t.state.phase).toBe('penalties');
    expect(previewDue(t.state, t.events)).toBe(true);
    expect(names(shootoutBeforeMask(t.state))).toEqual(['ShootoutFirst']);
    t.cmd({ type: 'PENALTY', team: 'white', scored: true });
    expect(t.moment).toMatchObject({ kind: 'penalty', scored: true });
    expect(names((t.moment as { mask: Mask }).mask)).toEqual(['ShootoutGoal']);
    expect(names(shootoutBeforeMask(t.state))).toEqual(['ShootoutBefore']);
    t.cmd({ type: 'PENALTY', team: 'blue', scored: false });
    expect(names((t.moment as { mask: Mask }).mask)).toEqual(['ShootoutMiss']);
    t.cmd({ type: 'PENALTY', team: 'white', scored: true }).cmd({ type: 'PENALTY', team: 'blue', scored: true });
    // 2-1 tras dos rondas: si el blanco marca el tercero, gana.
    expect(names(shootoutBeforeMask(t.state))).toEqual(['ShootoutCanWin']);
    t.cmd({ type: 'PENALTY', team: 'white', scored: true });
    expect(t.state.phase).toBe('finished');
    expect(names((t.moment as { mask: Mask }).mask)).toEqual(['ShootoutWin']);
  });

  it('gol de oro en la prórroga', () => {
    const s = new Sim(cfg()).wait(61_000).cmd({ type: 'CONTINUE' });
    if (s.state.phase === 'countdown') s.cmd({ type: 'SKIP_COUNTDOWN' });
    s.wait(61_000).cmd({ type: 'CONTINUE' });
    if (s.state.phase === 'countdown') s.cmd({ type: 'SKIP_COUNTDOWN' });
    s.goals('B');
    expect(s.state.phase).toBe('finished');
    expect(s.goalPools).toContain('GoldenWinner');
  });
});

describe('sorteo de comentarios', () => {
  const seq = (values: number[]) => {
    let i = 0;
    return () => values[i++ % values.length] >>> 0;
  };
  const deckOf = (catalog: number[][]) => new CommentaryDeck(() => true, catalog);
  const pools = (spec: Record<number, number>) => {
    const out: number[][] = Array.from({ length: 39 }, () => []);
    let code = 1;
    for (const [p, n] of Object.entries(spec)) out[Number(p)] = Array.from({ length: n }, () => code++);
    return out;
  };

  it('no repite hasta agotar el grupo y la vuelta nueva no empieza por el último', () => {
    const d = deckOf(pools({ [P.Comodin]: 5 }));
    d.random = () => Math.floor(Math.random() * 2 ** 32);
    let last = '';
    for (let round = 0; round < 50; round++) {
      const got = Array.from({ length: 5 }, () => d.pick(new Set([P.Comodin]))!);
      expect(new Set(got).size).toBe(5);
      if (last) expect(got[0]).not.toBe(last);
      last = got[4];
    }
  });

  it('70 % específicos y 30 % comodín; la anulación nunca se mezcla', () => {
    const d = deckOf(pools({ [P.Comodin]: 10, [P.Empate]: 10, [P.Anulado]: 3 }));
    let specific = 0;
    d.random = seq(Array.from({ length: 1000 }, (_, i) => Math.floor((i / 1000) * 2 ** 32)));
    const codesEmpate = new Set(pools({ [P.Comodin]: 10, [P.Empate]: 10 })[P.Empate].map((c) => c.toString(16).padStart(8, '0')));
    for (let i = 0; i < 500; i++) if (codesEmpate.has(d.pick(new Set([P.Comodin, P.Empate]))!)) specific++;
    expect(specific / 500).toBeGreaterThan(0.6);
    expect(specific / 500).toBeLessThan(0.8);
    const an = pools({ [P.Comodin]: 10, [P.Empate]: 10, [P.Anulado]: 3 })[P.Anulado].map((c) => c.toString(16).padStart(8, '0'));
    expect(an).toContain(d.pick(new Set([P.Anulado, P.Comodin])));
  });

  it('solo los que hay en el aparato; sin ninguno, nada', () => {
    const cat = pools({ [P.Comodin]: 3 });
    const d = new CommentaryDeck((c) => c === '00000002', cat);
    expect(d.pick(new Set([P.Comodin]))).toBe('00000002');
    expect(new CommentaryDeck(() => false, cat).pick(new Set([P.Comodin]))).toBeNull();
    expect(new CommentaryDeck(() => false, cat).empty).toBe(true);
  });
});

describe('ambiente', () => {
  it('el primero en los 20 s del principio y luego cada 30–60 s, no justo tras un gol', () => {
    const a = new AmbienceClock();
    a.random = () => 0; // huecos de 30 s
    a.reset();
    expect(a.due(1000, true, false)).toBeNull(); // suena algo
    expect(a.due(2000, true, true)).toBe(P.Opening20);
    expect(a.due(20_000, true, true)).toBeNull();
    expect(a.due(32_000, true, true)).toBe(P.Ambience);
    a.score(40_000);
    expect(a.due(62_100, true, true)).toBe(P.Ambience);
    a.score(90_000);
    expect(a.due(95_000, true, true)).toBeNull(); // menos de 10 s tras el gol
    // Si la 1.ª parte pasa de 20 s sin hueco, ya no hay frase de principio.
    const b = new AmbienceClock();
    b.random = () => 0;
    b.reset();
    expect(b.due(25_000, true, true)).toBeNull();
    expect(b.due(30_000, true, true)).toBe(P.Ambience);
  });
});
