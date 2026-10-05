/**
 * Equipos fijos guardados: una pareja con nombre y logo propios. Se reconocen solos en cualquier
 * partido o torneo donde esos dos jueguen juntos en el mismo lado.
 */
import type { Club, ParticipantRefLike } from './types';
import { newId } from '../ids';

export type { Club } from './types';

const key = (ids: string[]) => [...ids].sort().join('|');

/** Equipo guardado de estos jugadores (solo parejas). */
export function findClub(clubs: Club[], playerIds: string[]): Club | undefined {
  if (playerIds.length !== 2) return undefined;
  const k = key(playerIds);
  return clubs.find((c) => key(c.playerIds) === k);
}

/** Equipo guardado de un lado del partido. */
export function clubOfSide(clubs: Club[], participants: ParticipantRefLike[], team: 'white' | 'blue'): Club | undefined {
  return findClub(
    clubs,
    participants.filter((p) => p.team === team).map((p) => p.playerId),
  );
}

/** Crea o actualiza el equipo de esta pareja. Devuelve la lista nueva y el equipo. */
export function saveClub(clubs: Club[], playerIds: string[], name: string, logo: string, now: number): { clubs: Club[]; club: Club } {
  const found = findClub(clubs, playerIds);
  const club: Club = found
    ? { ...found, name: name.trim(), logo }
    : { id: newId('club'), name: name.trim(), logo, playerIds: [...playerIds].sort(), createdAt: now };
  return { clubs: found ? clubs.map((c) => (c.id === found.id ? club : c)) : [...clubs, club], club };
}

/** Al fusionar un jugador en otro, sus equipos pasan al nuevo (si no choca con uno que ya exista). */
export function mergeClubPlayer(clubs: Club[], fromId: string, intoId: string): Club[] {
  const out: Club[] = [];
  for (const c of clubs) {
    if (!c.playerIds.includes(fromId)) {
      out.push(c);
      continue;
    }
    const ids = c.playerIds.map((id) => (id === fromId ? intoId : id)).sort();
    if (new Set(ids).size < 2 || out.some((o) => key(o.playerIds) === key(ids)) || clubs.some((o) => o !== c && key(o.playerIds) === key(ids))) continue;
    out.push({ ...c, playerIds: ids });
  }
  return out;
}

/** El nombre ya lo usa otro equipo. */
export function clubNameTaken(clubs: Club[], name: string, playerIds: string[]): boolean {
  const n = name.trim().toLocaleLowerCase('es');
  const mine = findClub(clubs, playerIds);
  return clubs.some((c) => c.id !== mine?.id && c.name.trim().toLocaleLowerCase('es') === n);
}
