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

export type TournamentFormat = 'league' | 'bracket';

export interface TournamentTeam {
  id: string;
  name: string;
  playerIds: string[];
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
