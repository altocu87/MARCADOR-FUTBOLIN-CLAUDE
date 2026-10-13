/** Título visible bajo el nombre y melodía de victoria de un jugador (menú del jugador → «Título y melodía»). */
import { useApp } from '../../app/AppContext';
import type { Player } from '../../services/persistence';
import { availableTitles } from '../../services/progression';
import { ANTHEMS, sound } from '../../services/sound/sound';

export function PlayerStyle({ player }: { player: Player }) {
  const { progression, savePlayer, toast } = useApp();
  const prog = progression?.players.get(player.id);
  const titles = availableTitles(prog);
  const update = async (patch: Partial<Player>) => {
    try {
      await savePlayer({ ...player, ...patch, updatedAt: Date.now() });
    } catch {
      toast('No se pudo guardar');
    }
  };
  return (
    <div className="pstyle">
      <div>
        <div className="label">Título bajo el nombre</div>
        <div className="chip-wrap">
          <button className={`pick-chip ${!player.titleId ? 'pick-blue' : ''}`} onClick={() => void update({ titleId: undefined })}>
            Automático
          </button>
          <button className={`pick-chip ${player.titleId === 'none' ? 'pick-blue' : ''}`} onClick={() => void update({ titleId: 'none' })}>
            Sin título
          </button>
          {titles.map((t) => (
            <button key={t.id} className={`pick-chip ${player.titleId === t.id ? 'pick-blue' : ''}`} onClick={() => void update({ titleId: t.id })}>
              {t.title}
            </button>
          ))}
        </div>
      </div>
      <div>
        <div className="label">Melodía de victoria</div>
        <div className="chip-wrap">
          {Object.entries(ANTHEMS).map(([id, a]) => (
            <button
              key={id}
              className={`pick-chip ${(player.anthem ?? 'fanfare') === id ? 'pick-blue' : ''}`}
              onClick={() => {
                sound.unlock();
                sound.playAnthem(id);
                void update({ anthem: id });
              }}
            >
              ♪ {a.label}
            </button>
          ))}
        </div>
      </div>
    </div>
  );
}
