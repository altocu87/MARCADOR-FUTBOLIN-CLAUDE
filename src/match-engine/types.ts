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
  | 'handicap' // Partido Loco: anuncio o fin de un hándicap (reloj parado)
  | 'visit' // Partido Loco: un animal sale de un agujero de gusano y hace una travesura (reloj parado)
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
  /**
   * Reglas antiguas de Partido Caos (ya no se aplican: el modo 'chaos' es ahora Partido Loco,
   * con hándicaps). Se conservan para leer partidos guardados.
   */
  chaos?: ChaosRules;
}

export interface ChaosRules {
  /** Obsoleta: los goles del último minuto valían doble. Ya no se aplica. */
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
  | { type: 'TOGGLE_JOKER'; team: Team }
  /** Partido Loco: para el reloj y anuncia un hándicap (sustituye al que hubiera). */
  | { type: 'HANDICAP_START'; handicap: Handicap; nextAtMs: number; visitAtMs?: number }
  /** Partido Loco: se sigue jugando tras el anuncio o tras «vuelta a la normalidad». */
  | { type: 'HANDICAP_GO' }
  /** Partido Loco: resultado del penalti pitado por un hándicap. */
  | { type: 'HANDICAP_PENALTY'; scored: boolean; source?: InputSource }
  /** Partido Loco: aparece un animal (reloj parado); aún no hace nada. */
  | { type: 'VISIT_START'; visitor: Visitor }
  /** Partido Loco: el animal hace su travesura (roba el gol, cambia el reloj o la meta). */
  | { type: 'VISIT_APPLY' }
  /** Partido Loco: el animal se va y se sigue jugando. */
  | { type: 'VISIT_END' };

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
  | 'HANDICAP_START'
  | 'HANDICAP_END'
  | 'HANDICAP_PENALTY'
  | 'VISIT'
  | 'MATCH_END';

/** Animales del Partido Loco que salen de un agujero de gusano. */
export type Animal = 'squirrel' | 'snail' | 'cat';

/**
 * Travesura de un animal:
 * - ardilla: roba un gol al equipo `from` y se lo da al otro;
 * - caracol: +1 min al reloj (por tiempo) o sube la meta un gol (por goles);
 * - gato del futuro: −1 min al reloj (por tiempo) o baja la meta un gol (por goles).
 */
export interface Visitor {
  id: string;
  animal: Animal;
  /** Ardilla: equipo al que le roba el gol. */
  from?: Team;
  /** Caracol / gato: cambio de tiempo de la parte en ms (±60 000). */
  timeMs?: number;
  /** Caracol / gato: cambio de la meta de goles (±1). */
  goals?: number;
}

export interface VisitState {
  spec: Visitor;
  applied: boolean;
}

/** Hándicaps del Partido Loco. */
export type HandicapKind =
  | 'double_all' // todos los goles valen doble
  | 'double_team' // los goles de un equipo (el que va perdiendo) valen doble
  | 'triple_next' // el próximo gol, de quien sea, vale 3
  | 'steal' // el próximo gol del que va perdiendo además le quita uno al rival
  | 'freeze_score' // marcador congelado: los goles no cuentan
  | 'swap_positions' // portero/defensa ↔ ataque en un equipo o en los dos
  | 'frozen_player' // un jugador (o uno de cada equipo) no se puede mover
  | 'bar_lock' // un jugador (o uno de cada) no puede mover una barra
  | 'penalty' // penalti a favor de un equipo
  | 'transfer' // un jugador de cada equipo se cambia al rival
  | 'weak_hand' // todos con la mano no dominante
  | 'no_spin' // prohibido el molinillo
  | 'long_shots' // solo valen los goles desde defensa o medio
  | 'one_hand'; // cada jugador con una sola mano

export type Bar = 'portero' | 'defensa' | 'medio' | 'delantero';

export interface Handicap {
  id: string;
  kind: HandicapKind;
  /** Equipo afectado (o a favor, en el penalti); 'both' = los dos. */
  team?: Team | 'both';
  /** Jugadores afectados (ids). */
  players?: string[];
  /** Barra bloqueada (bar_lock). */
  bar?: Bar;
  /** Duración en tiempo de juego; null = hasta el siguiente hándicap o hasta que se use. */
  durationMs: number | null;
}

/** Hándicap en curso: anunciándose, activo o terminando («vuelta a la normalidad»). */
export interface HandicapState {
  spec: Handicap;
  stage: 'announce' | 'active' | 'ending';
  /** Tiempo de juego acumulado al empezar a correr (ms). */
  startTotalMs?: number;
  /** En «ending»: hora (epoch ms) a la que se sigue jugando sola. */
  resumeAt?: number;
  /** Hándicap anterior que terminó al anunciar este («volved a vuestro sitio»). */
  replaced?: Handicap;
}

export const HANDICAP_END_PAUSE_MS = 3000;

/** Tiempos de la escena de cada animal (ms desde que aparece): cuándo hace la travesura y cuándo se va. */
export const VISIT_TIMING: Record<Animal, { apply: number; end: number }> = {
  squirrel: { apply: 4700, end: 7800 },
  snail: { apply: 3800, end: 6600 },
  cat: { apply: 2800, end: 5600 },
};

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
  /** Motivos del valor del gol (Partido Loco: double, triple, frozen, penalty; antiguos: joker, last_minute). */
  bonus?: ('joker' | 'last_minute' | 'double' | 'triple' | 'steal' | 'frozen' | 'penalty')[];
  /** Partido Loco: este gol quitó además un gol al rival. */
  steal?: boolean;
  /** Partido Loco: hándicap anunciado (HANDICAP_START) o terminado (HANDICAP_END). */
  handicap?: Handicap;
  /** Partido Loco: travesura de un animal (VISIT). */
  visitor?: Visitor;
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
  /** Partido Loco: hándicap en curso. */
  handicap?: HandicapState;
  /** Partido Loco: tiempo de juego acumulado al que toca el siguiente hándicap. */
  nextHandicapAtMs?: number;
  /** Partido Loco: animal en pantalla. */
  visit?: VisitState;
  /** Partido Loco: tiempo de juego al que puede salir el próximo animal (sin fijar = ninguno). */
  nextVisitAtMs?: number;
  /** Partido Loco: tiempo añadido (+) o quitado (−) a la parte actual por un animal. */
  timeAdjustMs?: number;
  /** Partido Loco: cambio de la meta de goles por un animal (por goles). */
  goalTargetAdjust?: number;
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
