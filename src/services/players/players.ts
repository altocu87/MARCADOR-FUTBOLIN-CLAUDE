/** Gestión de jugadores: validación, alta, edición y baja lógica. */
import { newId } from '../ids';
import type { Player } from '../persistence';

/** Máximo de caracteres del nombre: así cabe entero en marcador, fichas y pantalla final. */
export const NAME_MAX = 10;
export const ALIAS_MAX = 16;

export interface PlayerDraft {
  name: string;
  alias?: string;
  photo?: string;
}

export function validatePlayerDraft(draft: PlayerDraft, others: Player[], editingId?: string): string[] {
  const errors: string[] = [];
  const name = draft.name.trim();
  if (!name) errors.push('El nombre es obligatorio.');
  if (name.length > NAME_MAX) errors.push(`El nombre admite como máximo ${NAME_MAX} caracteres.`);
  if ((draft.alias ?? '').trim().length > ALIAS_MAX) errors.push(`El alias admite como máximo ${ALIAS_MAX} caracteres.`);
  const clash = others.some((p) => p.id !== editingId && p.name.trim().toLowerCase() === name.toLowerCase());
  if (clash) errors.push('Ya existe un jugador con ese nombre.');
  return errors;
}

export function createPlayer(draft: PlayerDraft, now: number): Player {
  return {
    id: newId('p'),
    name: draft.name.trim(),
    alias: draft.alias?.trim() || undefined,
    photo: draft.photo,
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

export function displayName(player: Pick<Player, 'name' | 'alias'>): string {
  return player.alias ? `${player.name} «${player.alias}»` : player.name;
}

export function sortPlayers(players: Player[]): Player[] {
  return [...players].sort((a, b) => a.name.localeCompare(b.name, 'es'));
}
