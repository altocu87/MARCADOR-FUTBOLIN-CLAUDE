/**
 * Contratos de datos y repositorios.
 * Durante las entregas locales se implementan con almacenamiento del navegador;
 * en la última fase se podrá añadir un adaptador remoto detrás de los mismos contratos.
 */
import type {
  ChaosRules,
  MatchConfig,
  MatchEvent,
  MatchResult,
  MatchState,
  ParticipantRef,
  PenaltyKick,
  PeriodRecord,
  Team,
} from '../../match-engine';

export const STORAGE_FORMAT_VERSION = 1;

export interface Player {
  id: string;
  name: string;
  alias?: string;
  /** Foto opcional como data URL (redimensionada en local). */
  photo?: string;
  active: boolean;
  createdAt: number;
  updatedAt: number;
  /** Título elegido entre los logros desbloqueados (id de logro). */
  titleId?: string;
  /** Melodía de celebración al ganar. */
  anthem?: string;
  /** Invitado: se apunta con solo el nombre y no sale en el ranking. */
  guest?: boolean;
}

/** Partido terminado y guardado. Permite reconstruir el encuentro completo. */
export interface StoredMatch {
  formatVersion: number;
  id: string;
  engineVersion: string;
  rulesVersion: string;
  config: MatchConfig;
  participants: ParticipantRef[];
  createdAt: number;
  startedAt: number;
  finishedAt: number;
  result: MatchResult;
  periods: PeriodRecord[];
  events: MatchEvent[];
  penalties: PenaltyKick[];
  /** Goleador opcional por gol: id de evento GOAL → id de jugador. */
  scorers?: Record<string, string>;
  tournament?: { id: string; fixtureId: string };
}

/** Liguilla, cuadro eliminatorio o Pool (parejas rotativas con puntos individuales). */
export type TournamentFormat = 'league' | 'bracket' | 'pool';
/** Final tras la fase regular: sin final, 1º contra 2º o (Pool) 1º+4º contra 2º+3º. */
export type TournamentFinal = 'none' | 'top2' | 'top4';

export interface TournamentTeam {
  id: string;
  name: string;
  playerIds: string[];
  /** Logo del equipo (id del muestrario), si es un equipo guardado con nombre propio. */
  logo?: string;
  /** Equipo guardado del que sale (nombre y logo). */
  clubId?: string;
}

/** Equipo fijo guardado (una pareja con nombre y logo): se reconoce en cualquier partido que jueguen juntos. */
export interface Club {
  id: string;
  name: string;
  /** Id del logo en el muestrario (`escudo-07`, `emoji:🦅`…). */
  logo: string;
  /** Jugadores, ordenados. */
  playerIds: string[];
  createdAt: number;
}

export interface Fixture {
  id: string;
  round: number;
  whiteTeamId: string | null;
  blueTeamId: string | null;
  matchId?: string;
  winnerTeamId?: string;
  /** Pase directo (cuadro con huecos). */
  bye?: boolean;
  nextFixtureId?: string;
  nextSlot?: 'white' | 'blue';
  /** Final añadida tras la fase regular (liguilla y Pool); sus equipos se rellenan al acabar esa fase. */
  stage?: 'final';
  /** Pool: jugadores que descansan en esta ronda. */
  resting?: string[];
  /** Serie al mejor de N partidos (por defecto 1). */
  bestOf?: number;
  /** Victorias de cada lado en la serie. */
  series?: { white: number; blue: number };
  /** Partidos de la serie, en orden (`matchId` es siempre el último). */
  matchIds?: string[];
  /** Se empezó y se abandonó a medias (sigue pendiente; se borra al apuntar el resultado). */
  abandonedAt?: number;
}

export interface Tournament {
  formatVersion: number;
  id: string;
  name: string;
  format: TournamentFormat;
  teamSize: 1 | 2;
  /** Si cuenta para ELO los partidos se juegan como Clasificatorio. */
  ranked: boolean;
  config: MatchConfig;
  teams: TournamentTeam[];
  fixtures: Fixture[];
  status: 'active' | 'finished' | 'cancelled';
  createdAt: number;
  finishedAt?: number;
  winnerTeamId?: string;
  /** Partido que cerró el torneo (para conceder el premio una vez). */
  finalMatchId?: string;
  /** Pool: jugadores inscritos (las parejas cambian en cada partido). */
  entrants?: string[];
  /** Final tras la fase regular (liguilla y Pool). */
  final?: TournamentFinal;
  /** Reglas de la final si difieren de las del resto (p. ej. a más goles). */
  finalConfig?: MatchConfig;
  /** Predefinido con el que se creó. */
  templateName?: string;
  /** Competición a la que pertenece (id del predefinido): sus ediciones forman el palmarés. */
  templateId?: string;
  /** Número de edición dentro de su competición (1ª, 2ª…). */
  edition?: number;
  /** Nombre propio de la competición (p. ej. «Copa del Almacén»); sin él, la del tipo de torneo. */
  competition?: string;
  /** Logo del torneo y copa que se lleva el campeón (ids del muestrario). */
  logo?: string;
  cup?: string;
}

/** Tipo de torneo guardado como predefinido en el creador. */
export interface TournamentTemplate {
  id: string;
  name: string;
  /** Predefinido de fábrica (no se puede borrar). */
  builtIn?: boolean;
  format: TournamentFormat;
  /** Liguilla y cuadro: 1 jugador o parejas fijas. El Pool siempre es por parejas rotativas. */
  teamSize: 1 | 2;
  /** Cómo se forman las parejas: equilibradas por ELO o al azar. */
  pairing: 'elo' | 'random';
  /** Cuadro: orden de siembra. */
  seeding: 'elo' | 'random';
  /** Pool: partidos que juega cada jugador (se ajusta para que todos jueguen los mismos). */
  gamesPerPlayer: number;
  final: TournamentFinal;
  /** Final al mejor de 1 o de 3. */
  finalBestOf: 1 | 3;
  ranked: boolean;
  endCondition: MatchConfig['endCondition'];
  goalsPerPeriod: number;
  minutesPerPeriod: number;
  /** Goles de la final (null = los mismos que el resto). */
  finalGoals: number | null;
  /** Cómo se acaba la final (sin poner = como el resto). */
  finalEndCondition?: MatchConfig['endCondition'];
  /** Minutos por parte de la final (sin poner = los mismos que el resto). */
  finalMinutes?: number;
}

export type EffectsLevel = 'full' | 'reduced' | 'off';
export type GoalSoundId = 'arcade' | 'laser' | 'stadium' | 'retro';
export type SeasonLength = 'month' | 'quarter';

export interface ProgressionSettings {
  /** Si está desactivada se muestra «Clasificación pendiente». */
  enabled: boolean;
  eloInitial: number;
  kProvisional: number;
  kEstablished: number;
  /** Partidos clasificatorios con K provisional. */
  provisionalMatches: number;
  /** Multiplicador por diferencia de goles (propuesta, desactivado por defecto). */
  goalDiffMultiplier: boolean;
}

export interface Preferences {
  formatVersion: number;
  /** Valor inicial del interruptor de modo prueba al preparar un partido. */
  testModeDefault: boolean;
  volume: number;
  muted: boolean;
  effects: EffectsLevel;
  /** Minutos de inactividad antes del reposo (0 = desactivado). */
  sleepMinutes: number;
  defaultEndCondition: MatchConfig['endCondition'];
  defaultGoalsPerPeriod: number;
  defaultMinutesPerPeriod: number;
  penaltyFirstTeam: Team;
  progression: ProgressionSettings;
  /** Locutor con la voz del navegador. */
  voice: boolean;
  goalSound: GoalSoundId;
  /** Reglas Caos propuestas («caos-1»). */
  chaosRules: ChaosRules;
  seasonLength: SeasonLength;
  /** Retos diarios y semanales. */
  challenges: boolean;
  /** Mantener la pantalla encendida mientras la app está abierta. */
  keepAwake: boolean;
  /** Conexión con placas (Arduino/ESP32). */
  hardware: HardwarePrefs;
  /** Tipos de torneo guardados en el creador (los de fábrica van aparte). */
  tournamentTemplates: TournamentTemplate[];
  /** Equipos fijos guardados con nombre y logo. */
  clubs: Club[];
}

export interface HardwarePrefs {
  /** Dirección WebSocket de la placa (p. ej. ws://192.168.4.1:81/). Vacío = automática. */
  wsUrl: string;
  /** Reconectar al abrir la app (Wi-Fi y puertos USB ya autorizados). */
  autoConnect: boolean;
}

/** Snapshot versionado de la partida en curso para ofrecer reanudar o descartar. */
export interface ActiveMatchSnapshot {
  formatVersion: number;
  savedAt: number;
  state: MatchState;
  /** Torneo asociado al partido en curso. */
  extras?: { tournament?: { id: string; fixtureId: string } };
}

export interface PlayerRepository {
  list(): Promise<Player[]>;
  save(player: Player): Promise<void>;
  saveAll(players: Player[]): Promise<void>;
}

export interface MatchRepository {
  list(): Promise<StoredMatch[]>;
  save(match: StoredMatch): Promise<void>;
  saveAll(matches: StoredMatch[]): Promise<void>;
}

export interface PreferencesRepository {
  load(): Promise<Preferences>;
  save(prefs: Preferences): Promise<void>;
}

export interface ActiveMatchRepository {
  load(): Promise<ActiveMatchSnapshot | null>;
  save(snapshot: ActiveMatchSnapshot): Promise<void>;
  clear(): Promise<void>;
}

export interface TournamentRepository {
  list(): Promise<Tournament[]>;
  save(tournament: Tournament): Promise<void>;
  saveAll(tournaments: Tournament[]): Promise<void>;
}

export interface Repositories {
  players: PlayerRepository;
  tournaments: TournamentRepository;
  matches: MatchRepository;
  preferences: PreferencesRepository;
  activeMatch: ActiveMatchRepository;
  /** Borra todos los datos locales de la aplicación. */
  wipe(): Promise<void>;
}
