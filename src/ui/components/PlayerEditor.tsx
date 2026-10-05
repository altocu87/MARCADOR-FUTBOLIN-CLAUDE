import { useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import type { Player } from '../../services/persistence';
import { ALIAS_MAX, NAME_MAX, createPlayer, findNameClash, mergeBlocker, updatePlayer, validatePlayerDraft } from '../../services/players';
import { NameClashNotice } from './NameClash';
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
  onMerged,
}: {
  player?: Player;
  onClose: () => void;
  onSaved?: (p: Player) => void;
  /** Convertir un invitado en jugador definitivo (deja de ser invitado al guardar). */
  promote?: boolean;
  /** Se fusionó con otro jugador por tener el mismo nombre: el que queda. */
  onMerged?: (intoId: string) => void;
}) {
  const { players, matches, savePlayer, mergePlayers } = useApp();
  // Jugador o invitado que ya tiene ese nombre.
  const [clash, setClash] = useState<Player | null>(null);
  const [clashError, setClashError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const nameRef = useRef<HTMLInputElement>(null);
  const [name, setName] = useState(player?.name ?? '');
  const [alias, setAlias] = useState(player?.alias ?? '');
  const [photo, setPhoto] = useState<string | undefined>(player?.photo);
  const [errors, setErrors] = useState<string[]>([]);
  const fileRef = useRef<HTMLInputElement>(null);

  const save = async () => {
    const draft = { name, alias, photo };
    const same = findNameClash(name, players, player?.id);
    if (same) {
      setErrors([]);
      setClashError(null);
      setClash(same);
      return;
    }
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

  // «Es la misma persona»: si se está creando, se usa (o se hace definitivo) el que ya existe;
  // si se edita uno que ya existía, se fusionan (el invitado pasa al jugador, nunca al revés).
  const useSame = async () => {
    if (!clash) return;
    setBusy(true);
    const now = Date.now();
    try {
      if (!player) {
        const kept: Player = {
          ...clash,
          alias: alias.trim() || clash.alias,
          photo: photo ?? clash.photo,
          active: true,
          // Se estaba creando un jugador: si el que existía era invitado, pasa a definitivo.
          guest: undefined,
          updatedAt: now,
        };
        await savePlayer(kept);
        onSaved?.(kept);
        onClose();
        return;
      }
      const [from, into] = player.guest && !clash.guest ? [player, clash] : !player.guest && clash.guest ? [clash, player] : [player, clash];
      const blocker = mergeBlocker(from.id, into.id, matches);
      if (blocker) {
        setClashError(blocker);
        return;
      }
      await mergePlayers(from.id, into.id);
      // Lo que se haya puesto (foto, alias) se queda en el que permanece.
      const keptBase = into.id === player.id ? { ...player, name: clash.name } : into;
      const kept: Player = {
        ...keptBase,
        alias: alias.trim() || keptBase.alias,
        photo: photo ?? keptBase.photo,
        active: true,
        guest: undefined,
        updatedAt: now,
      };
      await savePlayer(kept);
      onMerged?.(kept.id);
      onSaved?.(kept);
      onClose();
    } catch (err) {
      setClashError(err instanceof Error ? err.message : 'No se pudo completar.');
    } finally {
      setBusy(false);
    }
  };

  const rename = () => {
    setClash(null);
    setClashError(null);
    nameRef.current?.focus();
    nameRef.current?.select();
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
        clash ? (
          <>
            <button className="btn btn-ghost" onClick={rename} disabled={busy}>
              Usar otro nombre
            </button>
            <button className="btn btn-primary" onClick={() => void useSame()} disabled={busy}>
              {player ? 'Es la misma persona: fusionar' : clash.guest ? 'Es la misma persona: hacerlo jugador' : 'Es la misma persona'}
            </button>
          </>
        ) : (
          <>
            <button className="btn btn-ghost" onClick={onClose}>
              Cancelar
            </button>
            <button className="btn btn-primary" onClick={save}>
              {promote ? 'Hacer definitivo' : 'Guardar'}
            </button>
          </>
        )
      }
    >
      {clash && (
        <div style={{ marginBottom: 10 }}>
          <NameClashNotice clash={clash} busy={busy} error={clashError} showActions={false} onSame={() => void useSame()} onRename={rename} />
        </div>
      )}
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
              ref={nameRef}
              className="input"
              value={name}
              maxLength={NAME_MAX}
              autoFocus
              onChange={(e) => {
                setName(e.target.value);
                setClash(null);
              }}
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
