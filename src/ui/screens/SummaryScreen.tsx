import { useState } from 'react';
import { useApp } from '../../app/AppContext';
import { persistFinishedMatch, type SaveStatus } from '../../app/matchFinalizer';
import type { MatchExtras } from '../../app/routes';
import type { MatchState } from '../../match-engine';
import { buildBackup, type StoredMatch } from '../../services/persistence';
import { ScreenFrame, TestModeBadge } from '../components/common';
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
  const { navigate, repos, refresh, matches, demoMode } = useApp();
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
      title="Estadísticas del partido"
      right={statusNode}
      footer={
        <>
          {status.kind === 'error' && (
            <span className="notice error" style={{ marginRight: 'auto', display: 'flex', gap: 8, alignItems: 'center' }}>
              No guardado.
              <button className="btn btn-sm" onClick={retry}>Reintentar</button>
              <button className="btn btn-sm" onClick={exportUnsaved}>Exportar</button>
            </span>
          )}
          {status.kind === 'test' && (
            <span className="notice" style={{ marginRight: 'auto' }}>
              Modo prueba: no se ha guardado.
            </span>
          )}
          <button className="btn" onClick={() => navigate({ name: 'home' })}>
            Inicio
          </button>
          {extras?.tournament ? (
            <button className="btn btn-primary" onClick={() => navigate({ name: 'tournamentDetail', id: extras.tournament!.id })}>
              Volver al torneo
            </button>
          ) : (
            <>
              <button
                className="btn"
                title="Mismos jugadores cambiando de lado"
                onClick={() => navigate({ name: 'match', config: match.config, participants: swapSides(match.participants) })}
              >
                ⇄ Revancha
              </button>
              <button className="btn btn-primary" onClick={() => navigate({ name: 'setup', mode: match.config.mode })}>
                Nuevo partido
              </button>
            </>
          )}
        </>
      }
    >
      <MatchReport match={match} />
    </ScreenFrame>
  );
}
