/**
 * Aviso de nombre repetido: no puede haber dos jugadores ni invitados con el mismo nombre.
 * Ofrece poner otro nombre o decir que es la misma persona (se usa o se fusiona con la que ya existe).
 */
import type { Player } from '../../services/persistence';
import { Avatar } from './common';

export function NameClashNotice({
  clash,
  onSame,
  onRename,
  sameLabel = 'Es la misma persona',
  busy = false,
  error,
  showActions = true,
}: {
  clash: Player;
  onSame: () => void;
  onRename: () => void;
  sameLabel?: string;
  busy?: boolean;
  error?: string | null;
  /** false: los botones van en el pie de la ventana (cuando no caben aquí). */
  showActions?: boolean;
}) {
  return (
    <div className="name-clash" role="alert">
      <div className="name-clash-head">
        <Avatar name={clash.name} photo={clash.photo} size={40} />
        <span>
          <b>
            Ya existe {clash.guest ? 'un invitado' : 'un jugador'} llamado «{clash.name}»
            {!clash.active && ' (dado de baja)'}.
          </b>
          <small>¿Es la misma persona? Si no, elige otro nombre.</small>
        </span>
      </div>
      {error && <div className="notice error">{error}</div>}
      {showActions && (
        <div className="name-clash-actions">
          <button className="btn btn-sm" onClick={onRename} disabled={busy}>
            Usar otro nombre
          </button>
          <button className="btn btn-sm btn-primary" onClick={onSame} disabled={busy}>
            {sameLabel}
          </button>
        </div>
      )}
    </div>
  );
}
