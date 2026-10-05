/**
 * Nombre de un lado del partido: si esa pareja es un equipo guardado, su logo y su nombre;
 * si no, «BLANCO» o «AZUL».
 */
import { useApp } from '../../app/AppContext';
import type { ParticipantRef, Team } from '../../match-engine';
import { clubOfSide, type Club } from '../../services/clubs';
import { Crest } from './Crest';

export const SIDE_LABEL: Record<Team, string> = { white: 'BLANCO', blue: 'AZUL' };

/** Equipos guardados de cada lado (si los hay). */
export function useSideClubs(participants: { playerId: string; team: Team }[]): Record<Team, Club | undefined> {
  const { prefs } = useApp();
  return { white: clubOfSide(prefs.clubs, participants, 'white'), blue: clubOfSide(prefs.clubs, participants, 'blue') };
}

/** Texto del lado: nombre del equipo o BLANCO/AZUL. */
export function sideText(clubs: Record<Team, Club | undefined>, team: Team, upper = true): string {
  const c = clubs[team];
  return c ? (upper ? c.name.toLocaleUpperCase('es') : c.name) : SIDE_LABEL[team];
}

export function TeamName({
  team,
  participants,
  size = 22,
  upper = true,
  className = '',
}: {
  team: Team;
  participants: ParticipantRef[];
  size?: number;
  upper?: boolean;
  className?: string;
}) {
  const clubs = useSideClubs(participants);
  const c = clubs[team];
  if (!c) return <>{SIDE_LABEL[team]}</>;
  return (
    <span className={`team-name-club ${className}`}>
      <Crest id={c.logo} size={size} />
      <span>{upper ? c.name.toLocaleUpperCase('es') : c.name}</span>
    </span>
  );
}
