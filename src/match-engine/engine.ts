/**
 * MatchEngine: reglas, estados, reloj, goles, bloqueo, correcciones, partes y penaltis.
 *
 * Funciones puras: (estado, comando, ahora) → nuevo estado. No usa React, DOM,
 * red, sonido ni hardware. El reloj se calcula con tiempo real transcurrido
 * (marcas `now` en ms), nunca contando renderizados.
 */
import {
  COUNTDOWN_MS,
  ENGINE_VERSION,
  GOAL_LOCK_MS,
  HANDICAP_END_PAUSE_MS,
  RULES_VERSION,
  type CommandOutcome,
  type EngineCommand,
  type MatchConfig,
  type MatchEvent,
  type MatchResult,
  type MatchState,
  type ParticipantRef,
  type PenaltyKick,
  type Period,
  type PeriodRecord,
  type RejectReason,
  type ResolutionReason,
  type Score,
  type Team,
} from './types';
import { validateConfig, validateParticipants } from './validation';

export class EngineError extends Error {
  constructor(public readonly errors: string[]) {
    super(errors.join(' '));
    this.name = 'EngineError';
  }
}

export const otherTeam = (team: Team): Team => (team === 'white' ? 'blue' : 'white');

// ---------------------------------------------------------------------------
// Creación
// ---------------------------------------------------------------------------

export function createMatch(
  id: string,
  config: MatchConfig,
  participants: ParticipantRef[],
  now: number,
  /** Tiempo extra antes de la cuenta atrás (p. ej. el cartel de la fase de un torneo). */
  introMs = 0,
): MatchState {
  const errors = [...validateConfig(config), ...validateParticipants(participants)];
  if (errors.length > 0) throw new EngineError(errors);

  const state: MatchState = {
    engineVersion: ENGINE_VERSION,
    rulesVersion: RULES_VERSION,
    id,
    config: { ...config },
    participants: participants.map((p) => ({ ...p })),
    phase: 'countdown',
    period: 'first',
    createdAt: now,
    countdownEndsAt: now + COUNTDOWN_MS + introMs,
    periodElapsedMs: 0,
    closedPeriodsMs: 0,
    events: [],
    periods: [],
    penalties: [],
    undoStack: [],
    jokers: chaosActive(config, 'jokers') ? { white: 'available', blue: 'available' } : undefined,
    seq: 0,
  };
  return pushEvent(state, [], now, { type: 'MATCH_START' });
}

// ---------------------------------------------------------------------------
// Selectores
// ---------------------------------------------------------------------------

/** Ids de goles anulados según la cronología (−1 y deshacer). */
export function annulledGoalIds(state: MatchState): Set<string> {
  return annulledGoalIdsFromEvents(state.events);
}

export function annulledGoalIdsFromEvents(events: MatchEvent[]): Set<string> {
  const annulled = new Set<string>();
  const byId = new Map(events.map((e) => [e.id, e]));
  for (const e of events) {
    if (e.type === 'CORRECTION' && e.refEventId) annulled.add(e.refEventId);
    if (e.type === 'UNDO' && e.refEventId) {
      const target = byId.get(e.refEventId);
      if (target?.type === 'GOAL') annulled.add(target.id);
      if (target?.type === 'CORRECTION' && target.refEventId) annulled.delete(target.refEventId);
    }
  }
  return annulled;
}

export function validGoals(state: MatchState): MatchEvent[] {
  return validGoalsFromEvents(state.events);
}

export function validGoalsFromEvents(events: MatchEvent[]): MatchEvent[] {
  const annulled = annulledGoalIdsFromEvents(events);
  return events.filter((e) => e.type === 'GOAL' && !annulled.has(e.id));
}

export const goalValue = (e: MatchEvent): number => e.value ?? 1;

/** Por goles se juega una sola parte (sin descanso): el primero que llega al objetivo gana. */
export function isSinglePeriod(config: MatchConfig): boolean {
  return config.endCondition === 'goals';
}

export function chaosActive(config: MatchConfig, rule: keyof NonNullable<MatchConfig['chaos']>): boolean {
  return config.mode === 'chaos' && !!config.chaos?.[rule];
}

/** Suma de goles válidos; un gol «robo» (Partido Loco) además quita uno al rival. Nunca baja de 0. */
function sumGoals(goals: MatchEvent[]): Score {
  const score: Score = { white: 0, blue: 0 };
  for (const g of goals) {
    if (!g.team) continue;
    score[g.team] += goalValue(g);
    if (g.steal) score[otherTeam(g.team)] -= 1;
  }
  return { white: Math.max(0, score.white), blue: Math.max(0, score.blue) };
}

/** Goles robados por la ardilla (Partido Loco): +1 al que recibe, −1 al robado. */
function applyVisits(score: Score, events: MatchEvent[], period?: Period): Score {
  for (const e of events) {
    if (e.type !== 'VISIT' || e.visitor?.animal !== 'squirrel' || !e.visitor.from) continue;
    if (period && e.period !== period) continue;
    score[e.visitor.from] -= 1;
    score[otherTeam(e.visitor.from)] += 1;
  }
  return { white: Math.max(0, score.white), blue: Math.max(0, score.blue) };
}

export function getScore(state: MatchState): Score {
  return applyVisits(sumGoals(validGoals(state)), state.events);
}

export function getPeriodScore(state: MatchState, period: Period = state.period): Score {
  return applyVisits(sumGoals(validGoals(state).filter((g) => g.period === period)), state.events, period);
}

/** Meta de goles para ganar (el caracol y el gato del Partido Loco la cambian). */
export function goalTarget(state: MatchState): number {
  return state.config.goalsPerPeriod + (state.goalTargetAdjust ?? 0);
}

/** Límite de tiempo de la parte actual, contando lo que añadan o quiten los animales. */
function stateTimeLimitMs(state: MatchState): number | null {
  const base = periodTimeLimitMs(state.config, state.period);
  return base === null ? null : base + (state.timeAdjustMs ?? 0);
}

/** Partido Loco: los hándicaps solo existen en el modo 'chaos'. */
export function handicapsEnabled(config: MatchConfig): boolean {
  return config.mode === 'chaos';
}

/** Hándicap activo (corriendo en juego), si lo hay. */
export function activeHandicap(state: MatchState) {
  return state.handicap?.stage === 'active' ? state.handicap.spec : undefined;
}

/** Tiempo de juego que le queda al hándicap activo (null si dura hasta el siguiente o hasta usarse). */
export function handicapRemaining(state: MatchState, now: number): number | null {
  const h = state.handicap;
  if (!h || h.stage !== 'active' || h.spec.durationMs === null || h.startTotalMs === undefined) return null;
  const total = state.closedPeriodsMs + periodElapsed(state, now);
  return Math.max(0, h.startTotalMs + h.spec.durationMs - total);
}

export function getPenaltyScore(state: MatchState): Score {
  const score: Score = { white: 0, blue: 0 };
  for (const k of state.penalties) if (k.scored) score[k.team] += 1;
  return score;
}

/** Límite de tiempo del periodo en ms, o null si el tiempo no termina el periodo. */
export function periodTimeLimitMs(config: MatchConfig, period: Period): number | null {
  if (period === 'overtime') return config.overtimeSeconds * 1000;
  if (period === 'shootout') return null;
  if (config.endCondition === 'goals') return null;
  return config.minutesPerPeriod * 60_000;
}

export function periodElapsed(state: MatchState, now: number): number {
  const running = state.runningSince !== undefined ? Math.max(0, now - state.runningSince) : 0;
  const elapsed = state.periodElapsedMs + running;
  const limit = stateTimeLimitMs(state);
  return limit !== null ? Math.min(elapsed, limit) : elapsed;
}

export interface ClockView {
  periodElapsedMs: number;
  /** Tiempo restante si el periodo termina por tiempo; null en POR GOLES. */
  remainingMs: number | null;
  totalElapsedMs: number;
  running: boolean;
}

export function getClock(state: MatchState, now: number): ClockView {
  const elapsed = periodElapsed(state, now);
  const limit = stateTimeLimitMs(state);
  return {
    periodElapsedMs: elapsed,
    remainingMs: limit !== null ? Math.max(0, limit - elapsed) : null,
    totalElapsedMs: state.closedPeriodsMs + (state.phase === 'periodEnd' || state.phase === 'finished' ? 0 : elapsed),
    running: state.runningSince !== undefined,
  };
}

export function goalLockRemaining(state: MatchState, now: number): number {
  if (state.lastGoalAt === undefined) return 0;
  return Math.max(0, GOAL_LOCK_MS - (now - state.lastGoalAt));
}

export function countdownRemaining(state: MatchState, now: number): number {
  if (state.phase !== 'countdown' || state.countdownEndsAt === undefined) return 0;
  return Math.max(0, state.countdownEndsAt - now);
}

export function nextPenaltyTeam(state: MatchState): Team {
  const first = state.config.penaltyFirstTeam;
  return state.penalties.length % 2 === 0 ? first : otherTeam(first);
}

export function penaltyAttempts(state: MatchState): Score {
  const n: Score = { white: 0, blue: 0 };
  for (const k of state.penalties) n[k.team] += 1;
  return n;
}

export function isSuddenDeath(state: MatchState): boolean {
  const a = penaltyAttempts(state);
  return a.white >= state.config.penaltyRounds && a.blue >= state.config.penaltyRounds;
}

/** Ganador de la tanda o null si aún no está decidida. */
export function shootoutWinner(kicks: PenaltyKick[], rounds: number): Team | null {
  const goals: Score = { white: 0, blue: 0 };
  const shots: Score = { white: 0, blue: 0 };
  for (const k of kicks) {
    shots[k.team] += 1;
    if (k.scored) goals[k.team] += 1;
  }
  if (shots.white <= rounds && shots.blue <= rounds) {
    const remW = rounds - shots.white;
    const remB = rounds - shots.blue;
    if (goals.white > goals.blue + remB) return 'white';
    if (goals.blue > goals.white + remW) return 'blue';
    return null;
  }
  // Muerte súbita: se decide tras el mismo número de lanzamientos.
  if (shots.white === shots.blue && goals.white !== goals.blue) {
    return goals.white > goals.blue ? 'white' : 'blue';
  }
  return null;
}

// ---------------------------------------------------------------------------
// Avance del reloj (cuenta atrás y límites de tiempo)
// ---------------------------------------------------------------------------

export function advance(state: MatchState, now: number): { state: MatchState; events: MatchEvent[] } {
  const events: MatchEvent[] = [];
  let s = state;
  // Bucle: tras una suspensión larga puede terminar la cuenta atrás y el periodo.
  for (let guard = 0; guard < 4; guard += 1) {
    if (s.phase === 'countdown' && s.countdownEndsAt !== undefined && now >= s.countdownEndsAt) {
      s = startPeriodClock(s, events, s.countdownEndsAt);
      continue;
    }
    // «Vuelta a la normalidad»: se sigue jugando solo tras la pausa breve.
    if (s.phase === 'handicap' && s.handicap?.stage === 'ending' && s.handicap.resumeAt !== undefined && now >= s.handicap.resumeAt) {
      s = { ...s, phase: 'playing', runningSince: s.handicap.resumeAt, handicap: undefined };
      continue;
    }
    if (s.phase === 'playing' && s.runningSince !== undefined) {
      const limit = stateTimeLimitMs(s);
      const periodEndWall = limit !== null ? s.runningSince + (limit - s.periodElapsedMs) : Infinity;
      // Fin de un hándicap con duración (si llega antes que el final de la parte).
      const h = s.handicap;
      if (h?.stage === 'active' && h.spec.durationMs !== null && h.startTotalMs !== undefined) {
        const endTotal = h.startTotalMs + h.spec.durationMs;
        const endWall = s.runningSince + (endTotal - s.closedPeriodsMs - s.periodElapsedMs);
        if (now >= endWall && endWall < periodEndWall) {
          s = handicapTimeUp(s, events, endWall);
          continue;
        }
      }
      if (now >= periodEndWall) {
        s = endPeriod(s, events, periodEndWall, 'time');
        continue;
      }
    }
    break;
  }
  return { state: s, events };
}

// ---------------------------------------------------------------------------
// Comandos
// ---------------------------------------------------------------------------

export function dispatch(state: MatchState, command: EngineCommand, now: number): CommandOutcome {
  const advanced = advance(state, now);
  const s = advanced.state;
  const events = [...advanced.events];
  const reject = (reason: RejectReason): CommandOutcome => ({ state: s, accepted: false, reason, events });
  const accept = (next: MatchState): CommandOutcome => ({ state: next, accepted: true, events });

  switch (command.type) {
    case 'SKIP_COUNTDOWN': {
      if (s.phase !== 'countdown') return reject('invalid_state');
      return accept(startPeriodClock(s, events, now));
    }

    case 'SKIP_INTRO': {
      // Saltar el cartel previo: queda solo la cuenta atrás normal.
      if (s.phase !== 'countdown' || s.countdownEndsAt === undefined) return reject('invalid_state');
      return accept({ ...s, countdownEndsAt: Math.min(s.countdownEndsAt, now + COUNTDOWN_MS) });
    }

    case 'GOAL': {
      if (s.phase !== 'playing') return reject('invalid_state');
      if (goalLockRemaining(s, now) > 0) return reject('goal_lock');
      const bonus: NonNullable<MatchEvent['bonus']> = [];
      let value = 1;
      let steal = false;
      // Comodín Caos antiguo (desactivado en partidos nuevos).
      if (s.jokers?.[command.team] === 'armed') {
        bonus.push('joker');
        value += 1;
      }
      // Partido Loco: el hándicap activo cambia el valor del gol.
      const h = activeHandicap(s);
      if (h?.kind === 'double_all' || (h?.kind === 'double_team' && h.team === command.team)) {
        bonus.push('double');
        value *= 2;
      } else if (h?.kind === 'triple_next') {
        bonus.push('triple');
        value = 3;
      } else if (h?.kind === 'steal' && h.team === command.team && getScore(s)[otherTeam(command.team)] > 0) {
        bonus.push('steal');
        steal = true;
      } else if (h?.kind === 'freeze_score') {
        bonus.push('frozen');
        value = 0;
      }
      let next = pushEvent(s, events, now, {
        type: 'GOAL',
        team: command.team,
        source: command.source,
        ...(bonus.length ? { value, bonus } : {}),
        ...(steal ? { steal: true } : {}),
      });
      rescoreLast(next);
      const goalId = next.events[next.events.length - 1].id;
      next = { ...next, lastGoalAt: now, undoStack: [...next.undoStack, goalId] };
      if (bonus.includes('joker') && next.jokers) next = { ...next, jokers: { ...next.jokers, [command.team]: 'used' } };
      // El gol triple y el robo se gastan con el gol.
      if (h && (h.kind === 'triple_next' || (h.kind === 'steal' && h.team === command.team))) {
        next = endHandicap(next, events, now, 'used');
      }
      return accept(checkGoalEnd(next, events, now));
    }

    case 'MINUS_ONE': {
      if (s.phase !== 'playing' && s.phase !== 'paused') return reject('invalid_state');
      // Solo goles del periodo actual: no se reabren partes cerradas.
      const target = [...validGoals(s)]
        .reverse()
        .find((g) => g.team === command.team && g.period === s.period);
      if (!target) return reject('no_goal_to_remove');
      const next = pushEvent(s, events, now, {
        type: 'CORRECTION',
        team: command.team,
        refEventId: target.id,
      });
      rescoreLast(next);
      const corrId = next.events[next.events.length - 1].id;
      return accept({ ...next, undoStack: [...next.undoStack, corrId] });
    }

    case 'UNDO': {
      if (s.phase !== 'playing' && s.phase !== 'paused') return reject('invalid_state');
      const lastId = s.undoStack[s.undoStack.length - 1];
      if (!lastId) return reject('nothing_to_undo');
      const target = s.events.find((e) => e.id === lastId);
      if (!target) return reject('nothing_to_undo');
      let next = pushEvent(s, events, now, {
        type: 'UNDO',
        team: target.team,
        refEventId: target.id,
      });
      rescoreLast(next);
      // El bloqueo de gol (lastGoalAt) y el reloj no se modifican.
      next = { ...next, undoStack: next.undoStack.slice(0, -1) };
      // Restaurar un gol puede alcanzar el objetivo del periodo.
      if (target.type === 'CORRECTION' && next.phase === 'playing') next = checkGoalEnd(next, events, now);
      return accept(next);
    }

    case 'PAUSE': {
      if (s.phase !== 'playing') return reject('invalid_state');
      const elapsed = periodElapsed(s, now);
      const next: MatchState = { ...s, phase: 'paused', periodElapsedMs: elapsed, runningSince: undefined };
      return accept(pushEvent(next, events, now, { type: 'PAUSE' }));
    }

    case 'RESUME': {
      if (s.phase !== 'paused') return reject('invalid_state');
      const next: MatchState = { ...s, phase: 'playing', runningSince: now };
      return accept(pushEvent(next, events, now, { type: 'RESUME' }));
    }

    case 'CONTINUE': {
      if (s.phase !== 'periodEnd') return reject('invalid_state');
      if (s.period === 'first') return accept(beginCountdown(s, now, 'second'));
      if (s.period === 'second') {
        const next = beginCountdown(s, now, 'overtime');
        return accept(pushEvent(next, events, now, { type: 'OVERTIME_START' }));
      }
      if (s.period === 'overtime') {
        const next: MatchState = {
          ...s,
          phase: 'penalties',
          period: 'shootout',
          periodElapsedMs: 0,
          runningSince: undefined,
          undoStack: [],
        };
        return accept(pushEvent(next, events, now, { type: 'SHOOTOUT_START' }));
      }
      return reject('invalid_state');
    }

    case 'PENALTY': {
      if (s.phase !== 'penalties') return reject('invalid_state');
      if (command.team !== nextPenaltyTeam(s)) return reject('wrong_turn');
      if (shootoutWinner(s.penalties, s.config.penaltyRounds)) return reject('shootout_decided');
      const attempts = penaltyAttempts(s);
      const suddenDeath = attempts[command.team] >= s.config.penaltyRounds;
      let next = pushEvent(s, events, now, {
        type: 'PENALTY',
        team: command.team,
        scored: command.scored,
        source: command.source,
      });
      const eventId = next.events[next.events.length - 1].id;
      const kick: PenaltyKick = {
        order: s.penalties.length + 1,
        team: command.team,
        scored: command.scored,
        attempt: attempts[command.team] + 1,
        suddenDeath,
        eventId,
      };
      next = { ...next, penalties: [...next.penalties, kick] };
      const winner = shootoutWinner(next.penalties, next.config.penaltyRounds);
      if (winner) next = finishMatch(next, events, now, winner, 'penalties');
      return accept(next);
    }

    case 'TOGGLE_JOKER': {
      // Propuesta Caos: armar/desarmar el comodín; se consume con el siguiente gol del equipo.
      if (!s.jokers) return reject('rule_disabled');
      if (s.phase !== 'playing' && s.phase !== 'paused') return reject('invalid_state');
      const current = s.jokers[command.team];
      if (current === 'used') return reject('invalid_state');
      const nextJoker = current === 'available' ? 'armed' : 'available';
      const next: MatchState = { ...s, jokers: { ...s.jokers, [command.team]: nextJoker } };
      return accept(pushEvent(next, events, now, { type: 'JOKER', team: command.team, reason: nextJoker }));
    }

    case 'HANDICAP_START': {
      // Partido Loco: se para el reloj y se anuncia; el anterior (si lo había) termina aquí.
      if (!handicapsEnabled(s.config)) return reject('rule_disabled');
      if (s.phase !== 'playing') return reject('invalid_state');
      const replaced = s.handicap?.spec;
      let base = s;
      if (s.handicap) base = endHandicap(s, events, now, 'replaced');
      const elapsed = periodElapsed(base, now);
      let next: MatchState = {
        ...base,
        phase: 'handicap',
        periodElapsedMs: elapsed,
        runningSince: undefined,
        nextHandicapAtMs: command.nextAtMs,
        nextVisitAtMs: command.visitAtMs,
        handicap: { spec: command.handicap, stage: 'announce', ...(replaced ? { replaced } : {}) },
      };
      const team = command.handicap.team === 'white' || command.handicap.team === 'blue' ? command.handicap.team : undefined;
      next = pushEvent(next, events, now, { type: 'HANDICAP_START', handicap: command.handicap, ...(team ? { team } : {}) });
      return accept(next);
    }

    case 'HANDICAP_GO': {
      if (s.phase !== 'handicap' || !s.handicap) return reject('invalid_state');
      if (s.handicap.stage === 'announce') {
        if (s.handicap.spec.kind === 'penalty') return reject('invalid_state');
        const startTotalMs = s.closedPeriodsMs + s.periodElapsedMs;
        return accept({
          ...s,
          phase: 'playing',
          runningSince: now,
          handicap: { spec: s.handicap.spec, stage: 'active', startTotalMs },
        });
      }
      // «Vuelta a la normalidad»: seguir antes de que termine la pausa breve.
      return accept({ ...s, phase: 'playing', runningSince: now, handicap: undefined });
    }

    case 'HANDICAP_PENALTY': {
      const h = s.handicap;
      if (s.phase !== 'handicap' || !h || h.stage !== 'announce' || h.spec.kind !== 'penalty') return reject('invalid_state');
      const team = h.spec.team === 'blue' ? 'blue' : 'white';
      let next: MatchState = { ...s, phase: 'playing', runningSince: now, handicap: undefined };
      next = pushEvent(next, events, now, { type: 'HANDICAP_PENALTY', team, scored: command.scored, source: command.source });
      if (!command.scored) return accept(next);
      next = pushEvent(next, events, now, { type: 'GOAL', team, source: command.source, value: 1, bonus: ['penalty'] });
      rescoreLast(next);
      const goalId = next.events[next.events.length - 1].id;
      next = { ...next, lastGoalAt: now, undoStack: [...next.undoStack, goalId] };
      return accept(checkGoalEnd(next, events, now));
    }

    case 'VISIT_START': {
      // Partido Loco: el reloj se para mientras el animal está en pantalla.
      if (!handicapsEnabled(s.config)) return reject('rule_disabled');
      if (s.phase !== 'playing') return reject('invalid_state');
      const elapsed = periodElapsed(s, now);
      return accept({
        ...s,
        phase: 'visit',
        periodElapsedMs: elapsed,
        runningSince: undefined,
        nextVisitAtMs: undefined,
        visit: { spec: command.visitor, applied: false },
      });
    }

    case 'VISIT_APPLY': {
      if (s.phase !== 'visit' || !s.visit || s.visit.applied) return reject('invalid_state');
      const v = s.visit.spec;
      let next: MatchState = { ...s, visit: { spec: v, applied: true } };
      if (v.timeMs) next = { ...next, timeAdjustMs: (next.timeAdjustMs ?? 0) + v.timeMs };
      if (v.goals) next = { ...next, goalTargetAdjust: (next.goalTargetAdjust ?? 0) + v.goals };
      next = pushEvent(next, events, now, { type: 'VISIT', visitor: v, ...(v.from ? { team: otherTeam(v.from) } : {}) });
      rescoreLast(next);
      return accept(next);
    }

    case 'VISIT_END': {
      if (s.phase !== 'visit') return reject('invalid_state');
      let next: MatchState = { ...s, phase: 'playing', runningSince: now, visit: undefined };
      // El gol robado o la meta nueva pueden decidir el partido (las reglas del sorteo lo evitan).
      next = checkGoalEnd(next, events, now);
      // Con el tiempo quitado, la parte puede haber terminado ya.
      const after = advance(next, now);
      events.push(...after.events);
      return accept(after.state);
    }

    case 'UNDO_PENALTY': {
      // Propuesta: corregir el último lanzamiento mientras la tanda no esté decidida.
      if (s.phase !== 'penalties') return reject('invalid_state');
      const last = s.penalties[s.penalties.length - 1];
      if (!last) return reject('nothing_to_undo');
      const next = pushEvent(s, events, now, {
        type: 'PENALTY_UNDO',
        team: last.team,
        refEventId: last.eventId,
      });
      return accept({ ...next, penalties: next.penalties.slice(0, -1) });
    }
  }
}

// ---------------------------------------------------------------------------
// Transiciones internas
// ---------------------------------------------------------------------------

function pushEvent(
  state: MatchState,
  sink: MatchEvent[],
  now: number,
  partial: Partial<MatchEvent> & Pick<MatchEvent, 'type'>,
): MatchState {
  const seq = state.seq + 1;
  const periodTime = periodElapsed(state, now);
  const event: MatchEvent = {
    id: `e${seq}`,
    seq,
    period: state.period,
    periodTimeMs: periodTime,
    totalTimeMs: state.closedPeriodsMs + periodTime,
    wallTime: now,
    scoreAfter: getScore(state),
    ...partial,
  };
  sink.push(event);
  return { ...state, seq, events: [...state.events, event] };
}

/** Recalcula el marcador del último evento con la cronología ya actualizada (−1, deshacer, robos…). */
function rescoreLast(state: MatchState): void {
  const last = state.events[state.events.length - 1];
  if (last) last.scoreAfter = getScore(state);
}

/** Termina el hándicap en curso sin parar el juego (sustituido, usado o fin de la parte). */
function endHandicap(state: MatchState, events: MatchEvent[], at: number, reason: 'replaced' | 'used' | 'period_end'): MatchState {
  if (!state.handicap) return state;
  const spec = state.handicap.spec;
  const next = pushEvent({ ...state, handicap: undefined }, events, at, { type: 'HANDICAP_END', handicap: spec, reason });
  return next;
}

/** Se acabó el tiempo de un hándicap: reloj parado y «vuelta a la normalidad» unos segundos. */
function handicapTimeUp(state: MatchState, events: MatchEvent[], at: number): MatchState {
  const spec = state.handicap!.spec;
  const elapsed = periodElapsed(state, at);
  const next: MatchState = {
    ...state,
    phase: 'handicap',
    periodElapsedMs: elapsed,
    runningSince: undefined,
    handicap: { spec, stage: 'ending', resumeAt: at + HANDICAP_END_PAUSE_MS },
  };
  return pushEvent(next, events, at, { type: 'HANDICAP_END', handicap: spec, reason: 'time' });
}

function beginCountdown(state: MatchState, now: number, period: Period): MatchState {
  return {
    ...state,
    phase: 'countdown',
    period,
    countdownEndsAt: now + COUNTDOWN_MS,
    periodElapsedMs: 0,
    runningSince: undefined,
    undoStack: [],
  };
}

function startPeriodClock(state: MatchState, events: MatchEvent[], at: number): MatchState {
  const next: MatchState = {
    ...state,
    // El tiempo que añadió o quitó un animal solo vale para su parte.
    timeAdjustMs: undefined,
    phase: 'playing',
    countdownEndsAt: undefined,
    periodElapsedMs: 0,
    runningSince: at,
    startedAt: state.startedAt ?? at,
  };
  return pushEvent(next, events, at, { type: 'PERIOD_START' });
}

function checkGoalEnd(state: MatchState, events: MatchEvent[], now: number): MatchState {
  if (state.phase !== 'playing') return state;
  if (state.period === 'overtime') {
    const ps = getPeriodScore(state, 'overtime');
    if (ps.white + ps.blue > 0) return endPeriod(state, events, now, 'golden_goal');
    return state;
  }
  if (state.config.endCondition === 'time') return state;
  // Por goles (y en «ambas»): gana el primer equipo que llega al objetivo, sin esperar al final de la parte.
  const score = getScore(state);
  if (Math.max(score.white, score.blue) >= goalTarget(state)) return endPeriod(state, events, now, 'goals');
  return state;
}

function endPeriod(
  state: MatchState,
  events: MatchEvent[],
  at: number,
  reason: PeriodRecord['endReason'],
): MatchState {
  // Un hándicap no pasa a la parte siguiente.
  state = endHandicap(state, events, at, 'period_end');
  const elapsed = periodElapsed(state, at);
  const record: PeriodRecord = {
    period: state.period,
    score: getPeriodScore(state),
    durationMs: elapsed,
    endReason: reason,
  };
  let next: MatchState = {
    ...state,
    phase: 'periodEnd',
    periodElapsedMs: elapsed,
    runningSince: undefined,
    undoStack: [],
  };
  next = pushEvent(next, events, at, { type: 'PERIOD_END', reason });
  next = { ...next, closedPeriodsMs: next.closedPeriodsMs + elapsed, periods: [...next.periods, record] };

  const score = getScore(next);
  if (reason === 'goals') {
    return finishMatch(next, events, at, score.white > score.blue ? 'white' : 'blue', 'regulation');
  }
  if (state.period === 'second' && score.white !== score.blue) {
    return finishMatch(next, events, at, score.white > score.blue ? 'white' : 'blue', 'regulation');
  }
  if (state.period === 'overtime' && reason === 'golden_goal') {
    return finishMatch(next, events, at, score.white > score.blue ? 'white' : 'blue', 'golden_goal');
  }
  return next;
}

function finishMatch(
  state: MatchState,
  events: MatchEvent[],
  at: number,
  winner: Team,
  reason: ResolutionReason,
): MatchState {
  const result: MatchResult = {
    winner,
    reason,
    score: getScore(state),
    penaltyScore: state.penalties.length > 0 ? getPenaltyScore(state) : undefined,
    totalTimeMs: state.closedPeriodsMs,
  };
  const next: MatchState = { ...state, phase: 'finished', finishedAt: at, result, undoStack: [] };
  return pushEvent(next, events, at, { type: 'MATCH_END', team: winner, reason });
}
