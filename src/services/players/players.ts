/** Gestión de jugadores: validación, alta, edición y baja lógica. */
import { newId } from '../ids';
import type { Player, PlayerGender } from '../persistence';

/** Máximo de caracteres del nombre: así cabe entero en marcador, fichas y pantalla final. */
export const NAME_MAX = 10;
export const ALIAS_MAX = 16;

export interface PlayerDraft {
  name: string;
  alias?: string;
  photo?: string;
  gender?: PlayerGender;
}

/** Nombre comparable: sin mayúsculas, tildes ni espacios de más («José» = «jose »). */
export function nameKey(name: string): string {
  return name
    .normalize('NFD')
    .replace(/[\u0300-\u036f]/g, '')
    .trim()
    .replace(/\s+/g, ' ')
    .toLowerCase();
}

/** Jugador que ya usa ese nombre (no puede haber dos iguales). */
export function findNameClash(name: string, players: Player[], exceptId?: string): Player | undefined {
  const key = nameKey(name);
  return key ? players.find((p) => p.id !== exceptId && nameKey(p.name) === key) : undefined;
}

export function validatePlayerDraft(draft: PlayerDraft, others: Player[], editingId?: string): string[] {
  const errors: string[] = [];
  const name = draft.name.trim();
  if (!name) errors.push('El nombre es obligatorio.');
  if (name.length > NAME_MAX) errors.push(`El nombre admite como máximo ${NAME_MAX} caracteres.`);
  if ((draft.alias ?? '').trim().length > ALIAS_MAX) errors.push(`El alias admite como máximo ${ALIAS_MAX} caracteres.`);
  if (findNameClash(name, others, editingId)) errors.push('Ya existe un jugador con ese nombre.');
  return errors;
}

export function createPlayer(draft: PlayerDraft, now: number): Player {
  return {
    id: newId('p'),
    name: draft.name.trim(),
    alias: draft.alias?.trim() || undefined,
    photo: draft.photo,
    ...(draft.gender ? { gender: draft.gender } : {}),
    active: true,
    createdAt: now,
    updatedAt: now,
  };
}

export function updatePlayer(player: Player, draft: PlayerDraft, now: number): Player {
  return {
    ...player,
    name: draft.name.trim(),
    alias: draft.alias?.trim() || undefined,
    photo: draft.photo,
    gender: draft.gender,
    updatedAt: now,
  };
}

export function setPlayerActive(player: Player, active: boolean, now: number): Player {
  return { ...player, active, updatedAt: now };
}

export function initials(name: string): string {
  const parts = name.trim().split(/\s+/).filter(Boolean);
  if (parts.length === 0) return '?';
  if (parts.length === 1) return parts[0].slice(0, 2).toUpperCase();
  return (parts[0][0] + parts[1][0]).toUpperCase();
}

export function sortPlayers(players: Player[]): Player[] {
  return [...players].sort((a, b) => a.name.localeCompare(b.name, 'es'));
}

/**
 * Chico o chica de los jugadores que ya había, puesto de una vez a petición del dueño (01/11/2026).
 * Solo a los que aún no lo tienen y solo una vez por jugador (`done`): si luego se cambia en la ficha, se respeta.
 */
const INITIAL_GENDERS: Record<string, PlayerGender> = {
  vicky: 'f',
  victoria: 'f',
  lucia: 'f',
  maria: 'f',
  alex: 'm',
  jose: 'm',
};
export function initialGenders(players: Player[], done: Set<string>, now: number): Player[] {
  return players
    .filter((p) => !p.gender && !done.has(p.id) && INITIAL_GENDERS[nameKey(p.name)])
    .map((p) => ({ ...p, gender: INITIAL_GENDERS[nameKey(p.name)], updatedAt: now }));
}
