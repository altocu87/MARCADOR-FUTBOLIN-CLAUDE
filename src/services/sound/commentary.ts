/**
 * Comentarios del locutor (los 806 de la SD, bajados de la nube): en qué grupo cae cada momento del partido y cuál
 * suena. Copia de hardware/esp32s3_pantalla7/s3_comentarios.h para que la app y la pantalla digan lo mismo; si cambia
 * una regla allí, cambiarla aquí (y en tests/comentariosApp.test.ts). Reglas de los grupos: docs/HISTORIAL_ENTREGAS.md.
 */
import {
  getPeriodScore,
  getScore,
  goalValue,
  nextPenaltyTeam,
  otherTeam,
  shootoutWinner,
  validGoals,
  type MatchEvent,
  type MatchState,
  type Score,
  type Team,
} from '../../match-engine';
import data from './commentaryCatalog.json';

/** Los grupos, en el mismo orden (y número) que `comentarios::Pool` de la placa y el campo `pool` del catálogo. */
export const POOLS = [
  'Comodin', 'Encajados3', 'Empate', 'Respuesta2', 'Anulado', 'EmpateMovido', 'SiguePerdiendo', 'Ventaja2', 'MasDe8',
  'VaGanando', 'Ultimos20', 'Intro', 'Descanso', 'FinalNormal', 'Remontada', 'EmpateTras3', 'Pierde4Ultimo60', 'Pierde4',
  'PenaltyMiss', 'PenaltyGoal', 'PenaltyEqualizer', 'PenaltyEqualizer20', 'Ambience', 'Opening20', 'SecondHalf',
  'SecondHalfMany', 'SecondHalfFew', 'GoldenWinner', 'ExtraTimeAnnounce', 'ShootoutAnnounce', 'Lead3', 'ShootoutCanWin',
  'ShootoutMustScore', 'ShootoutBefore', 'ShootoutMiss', 'ShootoutGoal', 'ShootoutFirst', 'ShootoutWin',
  'ShootoutChampion',
] as const;
export type Pool = (typeof POOLS)[number];
export const P = Object.fromEntries(POOLS.map((p, i) => [p, i])) as Record<Pool, number>;

/** Grupos de un momento (un conjunto de números de grupo). */
export type Mask = Set<number>;
const mask = (...pools: Pool[]): Mask => new Set(pools.map((p) => P[p]));

/** Códigos de cada grupo según el catálogo de la SD (solo los activos; los de torneo siguen reservados). */
export const CATALOG: number[][] = POOLS.map((_, i) => (data.pools as Record<string, string[]>)[String(i)] ?? []).map(
  (codes) => codes.map((c) => parseInt(c, 16)),
);
const hex = (n: number) => n.toString(16).padStart(8, '0');

/** `random()` de 32 bits → posición en [0, bound) sin divisiones, como `Catalog::index`. */
const index = (random: number, bound: number) => Math.floor(((random >>> 0) * bound) / 2 ** 32);
const rnd32 = () => Math.floor(Math.random() * 2 ** 32) >>> 0;

/**
 * Baraja de cada grupo: sale uno al azar sin repetir hasta agotarlos (Fisher-Yates paso a paso) y, al empezar otra
 * vuelta, nunca repite el último que sonó. Solo con los que hay en el aparato.
 */
export class CommentaryDeck {
  private codes: number[][];
  private remaining: number[];
  private started: boolean[];
  random: () => number = rnd32;

  constructor(has: (code: string) => boolean, catalog: number[][] = CATALOG) {
    this.codes = catalog.map((list) => list.filter((c) => has(hex(c))));
    this.remaining = this.codes.map(() => 0);
    this.started = this.codes.map(() => false);
  }

  count(pool: number): number {
    return this.codes[pool]?.length ?? 0;
  }

  get empty(): boolean {
    return this.codes.every((c) => !c.length);
  }

  private draw(p: number): string {
    const list = this.codes[p];
    const count = list.length;
    let at: number;
    if (!this.remaining[p]) {
      this.remaining[p] = count;
      // Al acabar una vuelta, el último que sonó está en la posición 0: el primero de la nueva no puede ser él.
      at = this.started[p] && count > 1 ? 1 + index(this.random(), count - 1) : index(this.random(), count);
      this.started[p] = true;
    } else at = index(this.random(), this.remaining[p]);
    const last = --this.remaining[p];
    const code = list[at];
    list[at] = list[last];
    list[last] = code;
    return hex(code);
  }

  /**
   * Uno de los grupos del momento. Con el comodín entre ellos: 70 % de los específicos y 30 % del comodín. La
   * anulación nunca se mezcla con voces de gol. null si no hay ninguno.
   */
  pick(m: Mask, generic: number = P.Comodin): string | null {
    if (m.has(P.Anulado)) return this.count(P.Anulado) ? this.draw(P.Anulado) : null;
    const pools = [...m].sort((a, b) => a - b).filter((p) => p !== generic && this.count(p));
    const hasGeneric = m.has(generic) && this.count(generic) > 0;
    if (!pools.length) return hasGeneric ? this.draw(generic) : null;
    let p: number;
    if (hasGeneric) {
      const k = pools.length;
      const ticket = index(this.random(), 10 * k);
      p = ticket < 7 * k ? pools[ticket % k] : generic;
    } else p = pools[index(this.random(), pools.length)];
    return this.draw(p);
  }
}

// ---------------------------------------------------------------- qué grupos tocan

const goals = (s: MatchState) => s.events.filter((e) => e.type === 'GOAL');
const limitMs = (s: MatchState): number | null => {
  if (s.period === 'overtime') return s.config.overtimeSeconds * 1000 + (s.timeAdjustMs ?? 0);
  if (s.period === 'shootout' || s.config.endCondition === 'goals') return null;
  return s.config.minutesPerPeriod * 60_000 + (s.timeAdjustMs ?? 0);
};

/** El último gol (si sigue valiendo) en los últimos `ms` de la 2.ª parte o de la prórroga; nunca a goles. */
function lastGoalWithin(s: MatchState, ms: number): boolean {
  if (s.config.endCondition === 'goals') return false;
  const all = goals(s);
  const g = all[all.length - 1];
  if (!g || !validGoals(s).includes(g) || !goalValue(g) || g.period !== s.period) return false;
  if (g.period !== 'second' && g.period !== 'overtime') return false;
  const limit = limitMs(s);
  return !!limit && g.periodTimeMs <= limit && limit - g.periodTimeMs <= ms;
}
export const lastTwentySeconds = (s: MatchState) => lastGoalWithin(s, 20_000);
const lastMinute = (s: MatchState) => lastGoalWithin(s, 60_000);

/** Empate con al menos dos goles válidos de antes en los 60 s de juego previos (ventana móvil). */
export function twoRecentGoals(s: MatchState): boolean {
  const list = validGoals(s);
  const cur = list[list.length - 1];
  if (!cur) return false;
  let count = 0;
  for (let i = list.length - 2; i >= 0; i--) {
    const g = list[i];
    if (!goalValue(g)) continue;
    if (g.totalTimeMs <= cur.totalTimeMs && cur.totalTimeMs - g.totalTimeMs <= 60_000 && ++count >= 2) return true;
  }
  return false;
}

/** La peor desventaja que llegó a tener un equipo, solo con goles válidos (anular deshace el contexto). */
export function worstDeficit(s: MatchState, team: Team): number {
  let own = 0;
  let other = 0;
  let worst = 0;
  for (const g of validGoals(s)) {
    const v = goalValue(g);
    if (!v) continue;
    if (g.team === team) {
      own += v;
      if (g.steal && other > 0) other--;
    } else {
      other += v;
      if (g.steal && own > 0) own--;
    }
    worst = Math.max(worst, other - own);
  }
  return worst;
}

/** Grupos de un gol de `team` (el marcador de antes es el de `prev`). Vacío en penaltis o si no subió. */
export function goalMask(prev: MatchState, next: MatchState, team: Team): Mask {
  const before = getScore(prev);
  const s = getScore(next);
  if (prev.phase === 'penalties' || next.phase === 'penalties' || s[team] <= before[team]) return new Set();
  const m = mask('Comodin');
  const opp = otherTeam(team);
  const own = s[team];
  const other = s[opp];
  if (own > other) m.add(P.VaGanando);
  if (own === other + 3) m.add(P.Lead3); // exactamente tres
  if (next.phase === 'finished' && next.period === 'overtime' && next.result?.reason === 'golden_goal') m.add(P.GoldenWinner);
  if (lastTwentySeconds(next)) m.add(P.Ultimos20);
  if (next.config.mode !== 'chaos') {
    // En el Partido Loco no se deduce una remontada de las travesuras de los animales.
    const worst = worstDeficit(next, team);
    if (own > other && before[team] <= before[opp] && worst) m.add(P.Remontada);
    if (own === other && worst >= 3) m.add(P.EmpateTras3);
  }
  if (other >= own + 4) {
    m.add(P.Pierde4);
    if (lastMinute(next)) m.add(P.Pierde4Ultimo60);
  }
  if (own < other) m.add(P.SiguePerdiendo);
  if (own > 8) m.add(P.MasDe8);
  if (own >= other + 2) m.add(P.Ventaja2);
  if (before[opp] >= 3) m.add(P.Encajados3); // marca quien ya había encajado al menos tres
  if (s.white === s.blue) {
    m.add(P.Empate);
    if (twoRecentGoals(next)) m.add(P.EmpateMovido);
  }
  // Responde tras dos o más goles seguidos del rival (sin contar el de ahora).
  const list = validGoals(next).filter((g) => goalValue(g));
  let streak = 0;
  for (let i = list.length - 2; i >= 0; i--) {
    if (list[i].team === team) break;
    if (++streak >= 2) {
      m.add(P.Respuesta2);
      break;
    }
  }
  return m;
}

/** Al empezar la 2.ª parte, según los goles de la 1.ª. */
export function secondHalfMask(s: MatchState): Mask {
  const first = getPeriodScore(s, 'first');
  const n = first.white + first.blue;
  const m = mask('SecondHalf');
  if (n > 6) m.add(P.SecondHalfMany);
  else if (n < 2) m.add(P.SecondHalfFew);
  return m;
}

/** Penalti del Partido Loco (no de la tanda): marcado, empate del partido o fallado. */
export function penaltyMask(s: MatchState, scored: boolean): Mask {
  if (!scored) return mask('PenaltyMiss');
  const m = mask('PenaltyGoal');
  const sc = getScore(s);
  if (s.period !== 'shootout' && sc.white === sc.blue) {
    m.add(P.PenaltyEqualizer);
    if (lastTwentySeconds(s)) m.add(P.PenaltyEqualizer20);
  }
  return m;
}

/** ¿Ganaría alguien la tanda con el siguiente tiro (si entra o si no)? */
function winsAfterKick(s: MatchState, scored: boolean): Team | null {
  const kicks = [...s.penalties, { order: 0, team: nextPenaltyTeam(s), scored, attempt: 0, suddenDeath: false, eventId: '' }];
  return shootoutWinner(kicks, s.config.penaltyRounds);
}

/** Antes de cada tiro de la tanda. */
export function shootoutBeforeMask(s: MatchState): Mask {
  if (s.phase !== 'penalties') return new Set();
  if (!s.penalties.length) return mask('ShootoutFirst');
  const kicker = nextPenaltyTeam(s);
  if (winsAfterKick(s, true) === kicker) return mask('ShootoutCanWin');
  const w = winsAfterKick(s, false);
  if (w && w !== kicker) return mask('ShootoutMustScore');
  return mask('ShootoutBefore');
}

/** Después de un tiro de la tanda. `champion`: ese tiro gana la final de un torneo. */
export function shootoutResultMask(s: MatchState, scored: boolean, champion = false): Mask {
  if (!scored) return mask('ShootoutMiss');
  if (s.phase === 'finished' && s.result?.reason === 'penalties') return mask(champion ? 'ShootoutChampion' : 'ShootoutWin');
  return mask('ShootoutGoal');
}

/** ¿Ha bajado el marcador de un equipo sin que sea un robo ni un animal? (anulación o deshacer) */
function annulled(prev: MatchState, next: MatchState, events: MatchEvent[]): boolean {
  if (prev.period !== next.period) return false;
  if (events.some((e) => (e.type === 'GOAL' && e.steal) || e.type === 'VISIT')) return false;
  const b = getScore(prev);
  const a = getScore(next);
  return a.white < b.white || a.blue < b.blue;
}
const up = (a: Score, b: Score) => a.white > b.white || a.blue > b.blue;

/** Qué momento ha pasado, para elegir comentario (el orden de prioridad es el de la placa). */
export type CommentMoment =
  | { kind: 'annulled' }
  | { kind: 'penalty'; scored: boolean; mask: Mask; generic: number }
  | { kind: 'goal'; mask: Mask }
  | { kind: 'phase'; mask: Mask; generic: number; event: 'periodEnd' | 'matchStart' | 'victory' }
  | null;

/**
 * El momento de un cambio aceptado por el motor (como `afterChange` de la placa): anulación, penalti (de la tanda o
 * del Partido Loco), gol, o el de una fase (descanso, a la prórroga, a penaltis, 2.ª parte, final por tiempo).
 */
export function commentMoment(prev: MatchState, next: MatchState, events: MatchEvent[], champion = false): CommentMoment {
  const goal = events.filter((e) => e.type === 'GOAL' && e.team).pop();
  if (!goal && annulled(prev, next, events)) return { kind: 'annulled' };
  const kick = events.filter((e) => e.type === 'PENALTY').pop();
  const hcPen = events.find((e) => e.type === 'HANDICAP_PENALTY');
  if (kick) {
    const scored = !!kick.scored;
    return { kind: 'penalty', scored, mask: shootoutResultMask(next, scored, champion), generic: P.PenaltyGoal };
  }
  if (hcPen) {
    const scored = !!hcPen.scored;
    return { kind: 'penalty', scored, mask: penaltyMask(next, scored), generic: P.PenaltyGoal };
  }
  const visitUp = !goal && events.some((e) => e.type === 'VISIT') && up(getScore(next), getScore(prev));
  if (visitUp) return { kind: 'goal', mask: mask('Comodin') }; // animales: como un gol cualquiera
  if (goal?.team) {
    const m = goalMask(prev, next, goal.team);
    return m.size ? { kind: 'goal', mask: m } : null;
  }
  if (getScore(next).white !== getScore(prev).white || getScore(next).blue !== getScore(prev).blue) return null;
  const ended = next.phase === 'periodEnd' && prev.phase !== 'periodEnd';
  const sc = getScore(next);
  if (ended && next.period === 'first') return { kind: 'phase', mask: mask('Descanso'), generic: P.Comodin, event: 'periodEnd' };
  if (ended && next.config.endCondition !== 'goals' && sc.white === sc.blue) {
    if (next.period === 'second') return { kind: 'phase', mask: mask('ExtraTimeAnnounce'), generic: P.Comodin, event: 'periodEnd' };
    if (next.period === 'overtime') return { kind: 'phase', mask: mask('ShootoutAnnounce'), generic: P.Comodin, event: 'periodEnd' };
  }
  if (events.some((e) => e.type === 'PERIOD_START' && e.period === 'second'))
    return { kind: 'phase', mask: secondHalfMask(next), generic: P.SecondHalf, event: 'matchStart' };
  if (next.phase === 'finished' && prev.phase !== 'finished' && next.result?.winner && next.period === 'second' &&
      next.config.endCondition !== 'goals' && next.result.reason === 'regulation')
    return { kind: 'phase', mask: mask('FinalNormal'), generic: P.Comodin, event: 'victory' };
  return null;
}

/** Antes de cada tiro de la tanda (al empezarla y tras cada tiro) va la vista previa del siguiente. */
export const previewDue = (next: MatchState, events: MatchEvent[]) =>
  next.phase === 'penalties' && events.some((e) => e.type === 'PENALTY' || e.type === 'SHOOTOUT_START');

/**
 * Comentarios de ambiente con el reloj de juego: el primero en los 20 s del principio y luego uno cada 30–60 s,
 * solo si no suena nada y no ha habido un gol hace menos de 10 s. No se acumulan ni corren en pausa.
 */
export class AmbienceClock {
  private nextAt = 0;
  private lastScoreAt = 0;
  private openingDone = false;
  private hasScore = false;
  random: () => number = rnd32;

  private gap() {
    return 30_000 + index(this.random(), 30_001);
  }
  reset(): void {
    this.nextAt = this.gap();
    this.lastScoreAt = 0;
    this.openingDone = this.hasScore = false;
  }
  score(play: number): void {
    this.lastScoreAt = play;
    this.hasScore = true;
  }
  /** Grupo que toca ahora (o null). `first`: es la 1.ª parte. */
  due(play: number, first: boolean, quiet: boolean): number | null {
    if (!first || play > 20_000) this.openingDone = true;
    if (!quiet || (this.hasScore && play - this.lastScoreAt < 10_000)) return null;
    if (!this.openingDone) {
      this.openingDone = true;
      this.nextAt = play + this.gap();
      return P.Opening20;
    }
    if (play < this.nextAt) return null;
    this.nextAt = play + this.gap();
    return P.Ambience;
  }
}
