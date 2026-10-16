/**
 * Copia de seguridad local: exportar/importar jugadores, preferencias, historial y sonidos propios
 * en un archivo JSON versionado (los sonidos, en base64). La progresión no se exporta porque se recalcula
 * siempre a partir del historial.
 */
import type { SoundRepository } from '../sound/library';
import { normalizePreferences } from './defaults';
import {
  STORAGE_FORMAT_VERSION,
  type Player,
  type Preferences,
  type Repositories,
  type StoredMatch,
  type StoredSound,
  type Tournament,
} from './types';

export const BACKUP_APP_ID = 'marcador-futbolin-v3';

export interface BackupFile {
  app: typeof BACKUP_APP_ID;
  formatVersion: number;
  exportedAt: string;
  players: Player[];
  matches: StoredMatch[];
  tournaments: Tournament[];
  preferences: Preferences;
  /** Sonidos propios de la mesa (las copias antiguas no los tienen). */
  sounds?: BackupSound[];
}

/** Un sonido dentro de la copia: el audio en base64. */
export type BackupSound = Omit<StoredSound, 'wav'> & { wav: string };

function toBase64(buf: ArrayBuffer): string {
  const bytes = new Uint8Array(buf);
  let bin = '';
  for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  return btoa(bin);
}

function fromBase64(text: string): ArrayBuffer {
  const bin = atob(text);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i += 1) bytes[i] = bin.charCodeAt(i);
  return bytes.buffer;
}

export const soundToBackup = (s: StoredSound): BackupSound => ({ ...s, wav: toBase64(s.wav) });
export const soundFromBackup = (s: BackupSound): StoredSound => ({ ...s, wav: fromBase64(s.wav) });

export type ImportStrategy = 'merge' | 'replace';

export interface ImportPreview {
  players: { total: number; new: number; existing: number };
  matches: { total: number; new: number; existing: number };
  sounds: { total: number; new: number; existing: number };
}

export async function buildBackup(repos: Repositories, now = new Date(), sounds?: SoundRepository): Promise<BackupFile> {
  return {
    app: BACKUP_APP_ID,
    formatVersion: STORAGE_FORMAT_VERSION,
    exportedAt: now.toISOString(),
    players: await repos.players.list(),
    matches: await repos.matches.list(),
    tournaments: await repos.tournaments.list(),
    preferences: await repos.preferences.load(),
    sounds: sounds ? (await sounds.list()).map(soundToBackup) : [],
  };
}

function isBackupSound(v: unknown): v is BackupSound {
  return (
    isObj(v) &&
    typeof v.id === 'string' &&
    typeof v.name === 'string' &&
    (v.location === 'device' || v.location === 'chip' || v.location === 'sd') &&
    typeof v.durationMs === 'number' &&
    typeof v.wav === 'string'
  );
}

const isObj = (v: unknown): v is Record<string, unknown> => !!v && typeof v === 'object' && !Array.isArray(v);

function isPlayer(v: unknown): v is Player {
  return isObj(v) && typeof v.id === 'string' && typeof v.name === 'string' && typeof v.active === 'boolean';
}

function isMatch(v: unknown): v is StoredMatch {
  return (
    isObj(v) &&
    typeof v.id === 'string' &&
    isObj(v.config) &&
    Array.isArray(v.participants) &&
    Array.isArray(v.events) &&
    isObj(v.result) &&
    typeof v.finishedAt === 'number'
  );
}

/** Valida el archivo. Devuelve la copia o la lista de errores. */
export function parseBackup(text: string): { ok: true; backup: BackupFile } | { ok: false; errors: string[] } {
  let data: unknown;
  try {
    data = JSON.parse(text);
  } catch {
    return { ok: false, errors: ['El archivo no es un JSON válido.'] };
  }
  if (!isObj(data) || data.app !== BACKUP_APP_ID) {
    return { ok: false, errors: ['El archivo no es una copia de MARCADOR FUTBOLÍN V3.'] };
  }
  if (typeof data.formatVersion !== 'number' || data.formatVersion > STORAGE_FORMAT_VERSION) {
    return { ok: false, errors: ['Versión de formato no compatible.'] };
  }
  const errors: string[] = [];
  if (!Array.isArray(data.players) || !data.players.every(isPlayer)) errors.push('Lista de jugadores no válida.');
  if (!Array.isArray(data.matches) || !data.matches.every(isMatch)) errors.push('Historial de partidos no válido.');
  if (errors.length) return { ok: false, errors };
  return {
    ok: true,
    backup: {
      app: BACKUP_APP_ID,
      formatVersion: data.formatVersion,
      exportedAt: String(data.exportedAt ?? ''),
      players: data.players as Player[],
      matches: data.matches as StoredMatch[],
      tournaments: Array.isArray(data.tournaments) ? (data.tournaments as Tournament[]) : [],
      preferences: normalizePreferences(data.preferences),
      sounds: Array.isArray(data.sounds)
        ? data.sounds.filter(isBackupSound).map((x) => ({ ...x, createdAt: typeof x.createdAt === 'number' ? x.createdAt : 0 }))
        : undefined,
    },
  };
}

export async function previewImport(repos: Repositories, backup: BackupFile, sounds?: SoundRepository): Promise<ImportPreview> {
  const players = new Set((await repos.players.list()).map((p) => p.id));
  const matches = new Set((await repos.matches.list()).map((m) => m.id));
  const pNew = backup.players.filter((p) => !players.has(p.id)).length;
  const mNew = backup.matches.filter((m) => !matches.has(m.id)).length;
  const sIds = new Set(sounds ? (await sounds.list()).map((x) => x.id) : []);
  const bSounds = backup.sounds ?? [];
  const sNew = bSounds.filter((x) => !sIds.has(x.id)).length;
  return {
    players: { total: backup.players.length, new: pNew, existing: backup.players.length - pNew },
    matches: { total: backup.matches.length, new: mNew, existing: backup.matches.length - mNew },
    sounds: { total: bSounds.length, new: sNew, existing: bSounds.length - sNew },
  };
}

/**
 * merge: añade lo nuevo y conserva los datos locales existentes (no sobrescribe).
 * replace: sustituye jugadores, historial, preferencias y sonidos por los del archivo (si la copia no trae
 * sonidos, porque es antigua, se quedan los de la mesa).
 */
export async function applyImport(repos: Repositories, backup: BackupFile, strategy: ImportStrategy, sounds?: SoundRepository): Promise<void> {
  const incoming = (backup.sounds ?? []).map(soundFromBackup);
  if (strategy === 'replace') {
    await repos.players.saveAll(backup.players);
    await repos.matches.saveAll(backup.matches);
    await repos.tournaments.saveAll(backup.tournaments);
    await repos.preferences.save(backup.preferences);
    if (sounds && backup.sounds) await sounds.saveAll(incoming);
    return;
  }
  if (sounds) {
    const have = new Set((await sounds.list()).map((x) => x.id));
    for (const x of incoming) if (!have.has(x.id)) await sounds.save(x);
  }
  const players = await repos.players.list();
  const pIds = new Set(players.map((p) => p.id));
  await repos.players.saveAll([...players, ...backup.players.filter((p) => !pIds.has(p.id))]);
  const matches = await repos.matches.list();
  const mIds = new Set(matches.map((m) => m.id));
  await repos.matches.saveAll([...matches, ...backup.matches.filter((m) => !mIds.has(m.id))]);
  const tournaments = await repos.tournaments.list();
  const tIds = new Set(tournaments.map((t) => t.id));
  await repos.tournaments.saveAll([...tournaments, ...backup.tournaments.filter((t) => !tIds.has(t.id))]);
}
