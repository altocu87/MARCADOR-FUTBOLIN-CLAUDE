/**
 * Acciones de un invitado: hacerlo jugador definitivo (con foto, alias…) o fusionarlo con un
 * jugador que ya existe. En los dos casos se conservan sus partidos, torneos y palmarés.
 */
import { useState } from 'react';
import { useApp } from '../../app/AppContext';
import type { Player } from '../../services/persistence';
import { mergeBlocker, sortPlayers } from '../../services/players';
import { Avatar, Modal } from './common';
import { PlayerEditor } from './PlayerEditor';

export function GuestActions({ player, onMerged }: { player: Player; onMerged?: (intoId: string) => void }) {
  const { players, matches, mergePlayers, toast } = useApp();
  const [promoting, setPromoting] = useState(false);
  const [merging, setMerging] = useState(false);
  const [target, setTarget] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  if (!player.guest) return null;

  const candidates = sortPlayers(players.filter((p) => !p.guest && p.id !== player.id));
  const blocker = target ? mergeBlocker(player.id, target, matches) : null;
  const targetName = players.find((p) => p.id === target)?.name ?? '';

  const merge = async () => {
    if (!target || blocker) return;
    setBusy(true);
    try {
      const r = await mergePlayers(player.id, target);
      toast(`${player.name} fusionado con ${targetName} · ${r.movedMatches} partidos y ${r.movedTournaments} torneos`);
      setMerging(false);
      onMerged?.(target);
    } catch (err) {
      toast(err instanceof Error ? err.message : 'No se pudo fusionar');
    } finally {
      setBusy(false);
    }
  };

  return (
    <>
      <button className="btn btn-sm btn-primary" onClick={() => setPromoting(true)}>
        Hacer jugador
      </button>
      <button className="btn btn-sm" onClick={() => setMerging(true)} disabled={candidates.length === 0}>
        Fusionar con…
      </button>
      {promoting && <PlayerEditor player={player} promote onClose={() => setPromoting(false)} />}
      {merging && (
        <Modal
          title={`Fusionar a ${player.name} con…`}
          onClose={() => setMerging(false)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setMerging(false)}>Cancelar</button>
              <button className="btn btn-primary" disabled={!target || !!blocker || busy} onClick={merge}>
                Fusionar
              </button>
            </>
          }
        >
          <p className="muted" style={{ margin: '0 0 8px', fontSize: 12 }}>
            Elige el jugador que es en realidad. Sus partidos, torneos y palmarés pasan a ese jugador y el invitado desaparece.
          </p>
          <div className="merge-list scroll">
            {candidates.map((p) => (
              <button key={p.id} className="merge-item" aria-pressed={target === p.id} onClick={() => setTarget(p.id)}>
                <Avatar name={p.name} photo={p.photo} size={34} />
                <span>{p.name}</span>
              </button>
            ))}
          </div>
          {target && (
            <div className={`notice ${blocker ? 'error' : ''}`} style={{ marginTop: 8 }}>
              {blocker ?? `${player.name} pasará a ser ${targetName}. No se puede deshacer.`}
            </div>
          )}
        </Modal>
      )}
    </>
  );
}
