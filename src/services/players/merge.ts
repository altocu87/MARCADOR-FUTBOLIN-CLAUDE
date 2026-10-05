/**
 * Fusionar un jugador (normalmente un invitado) con otro: todo lo que jugó el primero pasa al
 * segundo —partidos, goles, torneos y palmarés— y el primero desaparece. Como la progresión,
 * el ranking y los logros se calculan a partir de los partidos, se recalculan solos.
 */
import type { Player, StoredMatch, Tournament } from '../persistence';

export interface MergeResult {
  matches: StoredMatch[];
  tournaments: Tournament[];
  players: Player[];
  /** Partidos y torneos que han cambiado (para avisar). */
  movedMatches: number;
  movedTournaments: number;
}

/** Motivo por el que no se puede fusionar, o null si se puede. */
export function mergeBlocker(fromId: string, intoId: string, matches: StoredMatch[]): string | null {
  if (fromId === intoId) return 'Elige otro jugador.';
  const together = matches.some((m) => m.participants.some((p) => p.playerId === fromId) && m.participants.some((p) => p.playerId === intoId));
  return together ? 'Jugaron juntos o uno contra otro en algún partido: no pueden ser la misma persona.' : null;
}

export function mergePlayers(fromId: string, intoId: string, players: Player[], matches: StoredMatch[], tournaments: Tournament[], now: number): MergeResult {
  const blocker = mergeBlocker(fromId, intoId, matches);
  if (blocker) throw new Error(blocker);
  const into = players.find((p) => p.id === intoId);
  if (!into || !players.some((p) => p.id === fromId)) throw new Error('Jugador no encontrado.');
  const swap = (id: string) => (id === fromId ? intoId : id);

  let movedMatches = 0;
  const nextMatches = matches.map((m) => {
    const hit = m.participants.some((p) => p.playerId === fromId) || Object.values(m.scorers ?? {}).includes(fromId);
    if (!hit) return m;
    movedMatches += 1;
    return {
      ...m,
      participants: m.participants.map((p) => (p.playerId === fromId ? { ...p, playerId: intoId, nameSnapshot: into.name } : p)),
      ...(m.scorers ? { scorers: Object.fromEntries(Object.entries(m.scorers).map(([ev, pid]) => [ev, swap(pid)])) } : {}),
    };
  });

  const remaining = players.filter((p) => p.id !== fromId);
  const nameOf = (id: string) => remaining.find((p) => p.id === id)?.name ?? '?';
  let movedTournaments = 0;
  const nextTournaments = tournaments.map((t) => {
    const hit = t.teams.some((x) => x.playerIds.includes(fromId)) || t.entrants?.includes(fromId);
    if (!hit) return t;
    movedTournaments += 1;
    return {
      ...t,
      teams: t.teams.map((x) =>
        x.playerIds.includes(fromId) ? { ...x, playerIds: x.playerIds.map(swap), name: x.playerIds.map(swap).map(nameOf).join(' + ') } : x,
      ),
      ...(t.entrants ? { entrants: t.entrants.map(swap) } : {}),
      fixtures: t.fixtures.map((f) => (f.resting?.includes(fromId) ? { ...f, resting: f.resting.map(swap) } : f)),
    };
  });

  return {
    matches: nextMatches,
    tournaments: nextTournaments,
    players: remaining.map((p) => (p.id === intoId ? { ...p, active: true, updatedAt: now } : p)),
    movedMatches,
    movedTournaments,
  };
}
