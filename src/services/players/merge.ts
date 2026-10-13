/**
 * Fusionar un jugador con otro: todo lo que jugó el primero pasa al
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

/** ¿Está apuntado a este torneo (en un equipo o como inscrito)? */
const inTournament = (t: Tournament, id: string) => t.teams.some((x) => x.playerIds.includes(id)) || !!t.entrants?.includes(id);

/** Motivo por el que no se puede fusionar, o null si se puede. */
export function mergeBlocker(fromId: string, intoId: string, matches: StoredMatch[], tournaments: Tournament[] = []): string | null {
  if (fromId === intoId) return 'Elige otro jugador.';
  const together = matches.some((m) => m.participants.some((p) => p.playerId === fromId) && m.participants.some((p) => p.playerId === intoId));
  if (together) return 'Jugaron juntos o uno contra otro en algún partido: no pueden ser la misma persona.';
  // Si los dos están en el mismo torneo, al fusionar saldría la misma persona dos veces.
  const both = tournaments.find((t) => t.status !== 'cancelled' && inTournament(t, fromId) && inTournament(t, intoId));
  return both ? `Los dos están apuntados a «${both.name}»: no pueden ser la misma persona.` : null;
}

export function mergePlayers(fromId: string, intoId: string, players: Player[], matches: StoredMatch[], tournaments: Tournament[], now: number): MergeResult {
  const blocker = mergeBlocker(fromId, intoId, matches, tournaments);
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
  const nameOf = (list: Player[], id: string) => list.find((p) => p.id === id)?.name ?? '?';
  // Solo se rehace el nombre automático («Pepe + Ana»); el de un equipo con nombre propio se respeta.
  const autoName = (ids: string[]) => ids.map((id) => nameOf(players, id)).join(' + ');
  let movedTournaments = 0;
  const nextTournaments = tournaments.map((t) => {
    if (!inTournament(t, fromId)) return t;
    movedTournaments += 1;
    return {
      ...t,
      teams: t.teams.map((x) => {
        if (!x.playerIds.includes(fromId)) return x;
        const ids = x.playerIds.map(swap);
        return { ...x, playerIds: ids, name: x.name === autoName(x.playerIds) ? ids.map((id) => nameOf(remaining, id)).join(' + ') : x.name };
      }),
      ...(t.entrants ? { entrants: t.entrants.map(swap) } : {}),
      ...(t.ladder ? { ladder: { ...t.ladder, start: t.ladder.start.map((pair) => pair.map(swap)) } } : {}),
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
