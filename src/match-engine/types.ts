/**
 * Tipos del MatchEngine.
 * El motor es independiente de React, DOM, red, sonido y hardware:
 * recibe comandos + una marca de tiempo y devuelve el nuevo estado.
 */

export const ENGINE_VERSION = '1.0.0';
export const RULES_VERSION = 'reglas-partido-2';

export type Team = 'white' | 'blue';
export const TEAMS: readonly Team[] = ['white', 'blue'];

export type MatchMode = 'quick' | 'chaos' | 'ranked';

/**
 * Cómo termina el partido («reglas-partido-2»):
 * - goals: una sola parte con cronómetro ascendente; gana el primero que llega a `goalsPerPeriod`.
 * - time: dos partes de `minutesPerPeriod`; gana quien sume más goles; empate → prórroga → penaltis.
 * - both: dos partes con tiempo, pero llegar al objetivo de goles gana en ese momento.
 */
export type EndCondition = 'goals' | 'time' | 'both';

export type Period = 'first' | 'second' | 'overtime' | 'shootout';

export type Phase =
  | 'idle' // sin partido
  | 'countdown' // cuenta atrás 3-2-1
  | 'playing' // jugando
  | 'paused' // pausado
  | 'periodEnd' // final de periodo (espera acción del usuario)
  | 'penalties' // tanda de penaltis
  | 'finished'; // final de partido

export interface MatchConfig {
  mode: MatchMode;
  endCondition: EndCondition;
  /** Goles para ganar: el primer equipo que llega gana el partido (nombre histórico del campo). */
  goalsPerPeriod: number;
  /** Duración de cada parte en minutos. */
  minutesPerPeriod: number;
  /** Duración de la prórroga (gol de oro) en segundos. */
  overtimeSeconds: number;
  /** Lanzamientos iniciales por equipo en la tanda. */
  penaltyRounds: number;
  /** Equipo que lanza primero (propuesta configurable, por defecto Blanco). */
  penaltyFirstTeam: Team;
  /** Partido de prueba: no guarda historial ni progresión. Se fija al empezar. */
  testMode: boolean;
  /** Reglas especiales de Partido Caos (propuesta «caos-1»). Solo se aplican en modo 'chaos'. */
  chaos?: ChaosRules;
}

export interface ChaosRules {
  /** Los goles en el último minuto de una parte con límite de tiempo valen doble. */
  doubleLastMinute: boolean;
  /** Cada equipo tiene un comodín por partido: su siguiente gol vale doble. */
  jokers: boolean;
}

export const CHAOS_LAST_MINUTE_MS = 60_000;

export type JokerState = 'available' | 'armed' | 'used';

/** Máximo de jugadores por equipo. */
export const MAX_PER_TEAM = 4;
export type Slot = 1 | 2 | 3 | 4;

export interface ParticipantRef {
  playerId: string;
  team: Team;
  /** Plaza dentro del equipo: 1 a 4 (BLANCO 1…4, AZUL 1…4). */
  slot: Slot;
  /** Snapshot del nombre en el momento del partido. */
  nameSnapshot: string;
}

export type InputSource = 'touch' | 'mouse' | 'keyboard' | 'button' | 'sensor' | 'simulator' | 'test';

export type EngineCommand =
  | { type: 'SKIP_COUNTDOWN' }
  | { type: 'GOAL'; team: Team; source?: InputSource }
  | { type: 'MINUS_ONE'; team: Team }
  | { type: 'UNDO' }
  | { type: 'PAUSE' }
  | { type: 'RESUME' }
  | { type: 'CONTINUE' } // desde final de periodo: siguiente fase
  | { type: 'PENALTY'; team: Team; scored: boolean; source?: InputSource }
  | { type: 'UNDO_PENALTY' }
  | { type: 'TOGGLE_JOKER'; team: Team };

export type EventType =
  | 'MATCH_START'
  | 'PERIOD_START'
  | 'PERIOD_END'
  | 'GOAL'
  | 'CORRECTION' // −1: anula un gol concreto
  | 'UNDO' // revierte una acción corregible
  | 'PAUSE'
  | 'RESUME'
  | 'OVERTIME_START'
  | 'SHOOTOUT_START'
  | 'PENALTY'
  | 'PENALTY_UNDO'
  | 'JOKER'
  | 'MATCH_END';

export interface Score {
  white: number;
  blue: number;
}

export interface MatchEvent {
  /** Identificador estable dentro del partido (e1, e2…). */
  id: string;
  /** Secuencia inequívoca aunque dos eventos compartan segundo. */
  seq: number;
  type: EventType;
  team?: Team;
  period: Period;
  /** Tiempo de juego dentro del periodo (ms). */
  periodTimeMs: number;
  /** Tiempo de juego acumulado del partido (ms). */
  totalTimeMs: number;
  /** Hora de calendario (epoch ms). */
  wallTime: number;
  /** Marcador ordinario tras el evento. */
  scoreAfter: Score;
  /** Evento al que hace referencia (gol anulado, acción deshecha…). */
  refEventId?: string;
  /** Para PENALTY. */
  scored?: boolean;
  /** Valor del gol (1 normal; 2 o 3 con reglas Caos). */
  value?: number;
  /** Motivos del valor extra del gol. */
  bonus?: ('joker' | 'last_minute')[];
  source?: InputSource;
  reason?: string;
}

export interface PenaltyKick {
  order: number;
  team: Team;
  scored: boolean;
  /** Número de lanzamiento de ese equipo (1, 2, …). */
  attempt: number;
  suddenDeath: boolean;
  eventId: string;
}

export interface PeriodRecord {
  period: Period;
  /** Goles válidos marcados en este periodo (parcial real). */
  score: Score;
  durationMs: number;
  endReason?: 'goals' | 'time' | 'golden_goal';
}

export type ResolutionReason = 'regulation' | 'golden_goal' | 'penalties';

export interface MatchResult {
  winner: Team;
  reason: ResolutionReason;
  /** Resultado ordinario (sin penaltis). */
  score: Score;
  /** Resultado de la tanda, si la hubo (separado del ordinario). */
  penaltyScore?: Score;
  totalTimeMs: number;
}

export interface MatchState {
  engineVersion: string;
  rulesVersion: string;
  id: string;
  config: MatchConfig;
  participants: ParticipantRef[];
  phase: Phase;
  period: Period;
  createdAt: number;
  startedAt?: number;
  finishedAt?: number;
  /** Fin de la cuenta atrás actual (epoch ms). */
  countdownEndsAt?: number;
  /** Tiempo de juego acumulado del periodo, sin contar el tramo en marcha. */
  periodElapsedMs: number;
  /** Desde cuándo corre el reloj (epoch ms); undefined si está parado. */
  runningSince?: number;
  /** Tiempo de juego de periodos ya cerrados. */
  closedPeriodsMs: number;
  /** Hora del último gol aceptado (bloqueo de 3 s). */
  lastGoalAt?: number;
  events: MatchEvent[];
  periods: PeriodRecord[];
  penalties: PenaltyKick[];
  /** Pila de acciones corregibles del periodo actual (ids de evento). */
  undoStack: string[];
  /** Comodines Caos por equipo. */
  jokers?: Record<Team, JokerState>;
  result?: MatchResult;
  seq: number;
}

export type RejectReason =
  | 'invalid_state'
  | 'goal_lock'
  | 'no_goal_to_remove'
  | 'nothing_to_undo'
  | 'wrong_turn'
  | 'shootout_decided'
  | 'rule_disabled';

export interface CommandOutcome {
  state: MatchState;
  accepted: boolean;
  reason?: RejectReason;
  /** Eventos generados por este comando (incluye los del avance de reloj). */
  events: MatchEvent[];
}

export const GOAL_LOCK_MS = 3000;
export const COUNTDOWN_MS = 3000;
