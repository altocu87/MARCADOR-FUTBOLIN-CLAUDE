import { useState } from 'react';
import { useApp } from '../../app/AppContext';
import { persistFinishedMatch, type SaveStatus } from '../../app/matchFinalizer';
import type { MatchExtras } from '../../app/routes';
import type { MatchState } from '../../match-engine';
import { buildBackup, type StoredMatch } from '../../services/persistence';
import { MODE_LABEL, ScreenFrame, TestModeBadge } from '../components/common';
import { AssetImage } from '../components/assets';
import { downloadJson } from '../components/download';
import { MatchReport } from '../components/MatchReport';
import { swapSides } from '../components/VictoryScreen';

export function SummaryScreen({
  match,
  save,
  live,
  extras,
}: {
  match: StoredMatch;
  save: SaveStatus;
  live: MatchState;
  extras?: MatchExtras;
}) {
  const { navigate, repos, refresh, matches, demoMode, tournaments } = useApp();
  const tournamentDone = !!extras?.tournament && tournaments.find((x) => x.id === extras.tournament!.id)?.status === 'finished';
  const [status, setStatus] = useState<SaveStatus>(save);

  const retry = async () => {
    const s = await persistFinishedMatch(live, repos, extras);
    setStatus(s);
    if (s.kind === 'saved') await refresh();
  };

  const exportUnsaved = async () => {
    const backup = await buildBackup(repos);
    downloadJson(`marcador-partido-${match.id}.json`, { ...backup, matches: [...matches, match] });
  };

  const statusNode =
    status.kind === 'test' ? (
      <TestModeBadge />
    ) : status.kind === 'saved' ? (
      <span className="badge badge-ok">{demoMode ? '✓ Guardado en modo prueba' : '✓ Guardado'}</span>
    ) : (
      <span className="badge badge-danger">No guardado</span>
    );

  return (
    <ScreenFrame
      className="rep-screen"
      background={<AssetImage name="fondo-estadisticas" className="rep-bg" fallback={null} />}
      // Tipo de partido grande y centrado; los botones, arriba a la izquierda (abajo no queda franja).
      title={<span className={`match-mode mode-chip-${match.config.mode}`}>{MODE_LABEL[match.config.mode]}</span>}
      left={
        <div className="rep-actions">
          <button className="btn" onClick={() => navigate({ name: 'home' })}>
            Inicio
          </button>
          {extras?.tournament ? (
            <button className="btn btn-primary" onClick={() => navigate({ name: 'tournamentDetail', id: extras.tournament!.id, ...(tournamentDone ? { ceremony: true } : {}) })}>
              {tournamentDone ? '🏆 Ver campeón' : 'Volver al torneo'}
            </button>
          ) : (
            <button
              className="btn"
              title="Mismos jugadores cambiando de lado"
              onClick={() => navigate({ name: 'match', config: match.config, participants: swapSides(match.participants) })}
            >
              ⇄ Revancha
            </button>
          )}
        </div>
      }
      right={statusNode}
      footer={
        // Solo si no se pudo guardar: aviso con Reintentar y Exportar.
        status.kind === 'error' && (
          <span className="notice error" style={{ marginRight: 'auto', display: 'flex', gap: 8, alignItems: 'center' }}>
            No guardado.
            <button className="btn btn-sm" onClick={retry}>Reintentar</button>
            <button className="btn btn-sm" onClick={exportUnsaved}>Exportar</button>
          </span>
        )
      }
    >
      <MatchReport match={match} />
    </ScreenFrame>
  );
}
