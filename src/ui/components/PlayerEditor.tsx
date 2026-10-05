import { useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import type { Player } from '../../services/persistence';
import { ALIAS_MAX, NAME_MAX, createPlayer, updatePlayer, validatePlayerDraft } from '../../services/players';
import { Avatar, Modal } from './common';

/** Redimensiona la foto en local a 160 × 160 (recorte centrado) para no llenar el almacenamiento. */
function resizePhoto(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const url = URL.createObjectURL(file);
    const img = new Image();
    img.onload = () => {
      const size = 160;
      const canvas = document.createElement('canvas');
      canvas.width = size;
      canvas.height = size;
      const ctx = canvas.getContext('2d');
      if (!ctx) return reject(new Error('Canvas no disponible'));
      const side = Math.min(img.width, img.height);
      ctx.drawImage(img, (img.width - side) / 2, (img.height - side) / 2, side, side, 0, 0, size, size);
      URL.revokeObjectURL(url);
      resolve(canvas.toDataURL('image/jpeg', 0.82));
    };
    img.onerror = () => {
      URL.revokeObjectURL(url);
      reject(new Error('Foto no disponible'));
    };
    img.src = url;
  });
}

export function PlayerEditor({
  player,
  onClose,
  onSaved,
  promote = false,
}: {
  player?: Player;
  onClose: () => void;
  onSaved?: (p: Player) => void;
  /** Convertir un invitado en jugador definitivo (deja de ser invitado al guardar). */
  promote?: boolean;
}) {
  const { players, savePlayer } = useApp();
  const [name, setName] = useState(player?.name ?? '');
  const [alias, setAlias] = useState(player?.alias ?? '');
  const [photo, setPhoto] = useState<string | undefined>(player?.photo);
  const [errors, setErrors] = useState<string[]>([]);
  const fileRef = useRef<HTMLInputElement>(null);

  const save = async () => {
    const draft = { name, alias, photo };
    const errs = validatePlayerDraft(draft, players, player?.id);
    setErrors(errs);
    if (errs.length) return;
    const now = Date.now();
    const updated = player ? updatePlayer(player, draft, now) : createPlayer(draft, now);
    const next = promote ? { ...updated, guest: undefined, active: true } : updated;
    try {
      await savePlayer(next);
      onSaved?.(next);
      onClose();
    } catch (err) {
      setErrors([err instanceof Error ? err.message : 'No se pudo guardar.']);
    }
  };

  const onFile = async (file?: File) => {
    if (!file) return;
    try {
      setPhoto(await resizePhoto(file));
    } catch {
      setErrors(['Foto no disponible: se mostrarán las iniciales.']);
    }
  };

  return (
    <Modal
      title={promote ? 'Hacer jugador definitivo' : player ? 'Editar jugador' : 'Nuevo jugador'}
      onClose={onClose}
      actions={
        <>
          <button className="btn btn-ghost" onClick={onClose}>
            Cancelar
          </button>
          <button className="btn btn-primary" onClick={save}>
            {promote ? 'Hacer definitivo' : 'Guardar'}
          </button>
        </>
      }
    >
      <div style={{ display: 'flex', gap: 16, alignItems: 'flex-start' }}>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 6, alignItems: 'center' }}>
          <Avatar name={name || '?'} photo={photo} size={84} />
          <button className="btn btn-sm" onClick={() => fileRef.current?.click()}>
            Foto
          </button>
          {photo && (
            <button className="btn btn-sm btn-ghost" onClick={() => setPhoto(undefined)}>
              Quitar
            </button>
          )}
          <input
            ref={fileRef}
            type="file"
            accept="image/*"
            hidden
            onChange={(e) => void onFile(e.target.files?.[0])}
          />
        </div>
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: 10 }}>
          <label className="field">
            <span className="label">Nombre</span>
            <input
              className="input"
              value={name}
              maxLength={NAME_MAX}
              autoFocus
              onChange={(e) => setName(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && void save()}
            />
          </label>
          <label className="field">
            <span className="label">Alias (opcional)</span>
            <input className="input" value={alias} maxLength={ALIAS_MAX} onChange={(e) => setAlias(e.target.value)} />
          </label>
        </div>
      </div>
      {promote && (
        <p className="muted" style={{ margin: '10px 0 0', fontSize: 12 }}>
          Conserva todos sus partidos, torneos y palmarés. Pasará a salir en el ranking.
        </p>
      )}
      {errors.length > 0 && (
        <div className="notice error" style={{ marginTop: 10 }}>
          {errors.join(' ')}
        </div>
      )}
    </Modal>
  );
}
