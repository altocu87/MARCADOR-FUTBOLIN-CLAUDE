import { STORAGE_FORMAT_VERSION, type Preferences, type ProgressionSettings } from './types';

/** Parámetros de progresión PROPUESTOS (pendientes de aprobación del propietario). */
export const DEFAULT_PROGRESSION: ProgressionSettings = {
  enabled: true,
  eloInitial: 1200,
  kProvisional: 40,
  kEstablished: 20,
  provisionalMatches: 10,
  goalDiffMultiplier: false,
};

export const DEFAULT_PREFERENCES: Preferences = {
  formatVersion: STORAGE_FORMAT_VERSION,
  // Ya no se usa: el modo prueba es global (Ajustes → General). Se conserva por compatibilidad.
  testModeDefault: false,
  volume: 0.7,
  muted: false,
  effects: 'full',
  sleepMinutes: 5,
  defaultEndCondition: 'goals',
  defaultGoalsPerPeriod: 5,
  defaultMinutesPerPeriod: 5,
  penaltyFirstTeam: 'white',
  progression: DEFAULT_PROGRESSION,
  voice: false,
  goalSound: 'arcade',
  chaosRules: { doubleLastMinute: false, jokers: false },
  seasonLength: 'month',
  challenges: true,
  keepAwake: true,
  hardware: { wsUrl: '', autoConnect: false },
  tournamentTemplates: [],
  clubs: [],
};

/** Completa preferencias guardadas con valores por defecto (migración suave). */
export function normalizePreferences(raw: unknown): Preferences {
  const base = { ...DEFAULT_PREFERENCES, progression: { ...DEFAULT_PROGRESSION } };
  if (!raw || typeof raw !== 'object') return base;
  const r = raw as Partial<Preferences>;
  return {
    ...base,
    ...r,
    formatVersion: STORAGE_FORMAT_VERSION,
    progression: { ...base.progression, ...(r.progression ?? {}) },
    chaosRules: { ...base.chaosRules, ...(r.chaosRules ?? {}) },
    hardware: { ...base.hardware, ...(r.hardware ?? {}) },
    tournamentTemplates: Array.isArray(r.tournamentTemplates) ? r.tournamentTemplates : [],
    clubs: Array.isArray(r.clubs) ? r.clubs : [],
  };
}
