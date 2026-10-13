/**
 * Fotos de jugadores para listas, tablas y resúmenes: la norma es que donde sale un jugador sale su
 * cara (o sus iniciales si no tiene foto).
 */
import type { ReactNode } from 'react';
import { useApp } from '../../app/AppContext';
import type { StoredMatch } from '../../services/persistence';
import { Avatar } from './common';

/** Foto de un jugador por su id (undefined si no tiene). */
export function usePhotoOf(): (id: string) => string | undefined {
  const { players } = useApp();
  return (id) => players.find((p) => p.id === id)?.photo;
}

/** Varias caras en fila (sin nombres). */
export function Faces({ ids, names, size = 24 }: { ids: string[]; names?: (id: string) => string; size?: number }) {
  const { players } = useApp();
  return (
    <span className="faces" aria-hidden="true">
      {ids.map((id) => {
        const p = players.find((x) => x.id === id);
        return <Avatar key={id} name={names?.(id) ?? p?.name ?? '?'} photo={p?.photo} size={size} />;
      })}
    </span>
  );
}

/** Cara y nombre de un jugador. */
export function PlayerTag({ id, name, size = 28, children }: { id: string; name?: string; size?: number; children?: ReactNode }) {
  const { players } = useApp();
  const p = players.find((x) => x.id === id);
  const shown = name ?? p?.name ?? '?';
  return (
    <span className="ptag">
      <Avatar name={shown} photo={p?.photo} size={size} />
      <span className="ptag-text">
        <b className="ptag-name">{shown}</b>
        {children}
      </span>
    </span>
  );
}

/** Un lado de un partido: sus caras y «Ana + Luis». */
export function SideTag({
  match,
  team,
  size = 24,
  right = false,
  facesOnly = false,
}: {
  match: StoredMatch;
  team: 'white' | 'blue';
  size?: number;
  right?: boolean;
  /** Solo las caras (donde los nombres no caben). */
  facesOnly?: boolean;
}) {
  const people = match.participants.filter((p) => p.team === team).sort((a, b) => a.slot - b.slot);
  const faces = <Faces ids={people.map((p) => p.playerId)} names={(id) => people.find((p) => p.playerId === id)?.nameSnapshot ?? '?'} size={size} />;
  const names = facesOnly ? null : <span className="side-names">{people.map((p) => p.nameSnapshot).join(' + ')}</span>;
  return <span className={`side-tag${right ? ' is-right' : ''}`}>{right ? <>{names}{faces}</> : <>{faces}{names}</>}</span>;
}

/** Partido en una línea: un lado, el marcador y el otro (con `facesOnly`, sin nombres). */
export function MatchLine({ match, size = 24, facesOnly = false }: { match: StoredMatch; size?: number; facesOnly?: boolean }) {
  return (
    <span className={`match-line${facesOnly ? ' is-faces' : ''}`}>
      <SideTag match={match} team="white" size={size} facesOnly={facesOnly} />
      <b className="match-line-score">
        {match.result.score.white}–{match.result.score.blue}
      </b>
      <SideTag match={match} team="blue" size={size} right facesOnly={facesOnly} />
    </span>
  );
}
