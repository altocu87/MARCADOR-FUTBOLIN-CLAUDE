import { useState } from 'react';
import { useApp } from '../../app/AppContext';
import { AssetImage } from '../components/assets';
import { MatchReport } from '../components/MatchReport';
import { MODE_LABEL, Modal, ScreenFrame } from '../components/common';

export function MatchDetailScreen({ matchId, fromTournament }: { matchId: string; fromTournament?: boolean }) {
  const { matches, navigate, deleteMatch, toast } = useApp();
  const [confirm, setConfirm] = useState(false);
  const match = matches.find((m) => m.id === matchId);
  const remove = async () => {
    try {
      await deleteMatch(matchId);
      toast('Partido eliminado · estadísticas recalculadas');
      navigate({ name: 'ranking', tab: 'history' });
    } catch {
      toast('No se pudo eliminar');
    }
  };
  return (
    <ScreenFrame
      className="rep-screen"
      background={<AssetImage name="fondo-estadisticas" className="rep-bg" fallback={null} />}
      title={match ? <span className={`match-mode mode-chip-${match.config.mode}`}>{MODE_LABEL[match.config.mode]}</span> : 'Detalle del partido'}
      onBack={() =>
        fromTournament && match?.tournament
          ? navigate({ name: 'tournamentDetail', id: match.tournament.id, view: 'report' })
          : navigate({ name: 'ranking', tab: 'history' })
      }
      right={
        match &&
        !match.tournament && (
          <button className="btn btn-danger btn-sm" onClick={() => setConfirm(true)}>
            Eliminar
          </button>
        )
      }
    >
      {match ? <MatchReport match={match} /> : <div className="empty">Partido no encontrado.</div>}
      {confirm && (
        <Modal
          title="¿Eliminar este partido?"
          onClose={() => setConfirm(false)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setConfirm(false)}>Cancelar</button>
              <button className="btn btn-danger" onClick={remove}>Eliminar</button>
            </>
          }
        >
          <p style={{ margin: 0 }}>
            Se borra del historial y se recalculan automáticamente estadísticas, ELO, XP, logros y récords de todos los
            jugadores. No se puede deshacer (salvo con una copia de seguridad).
          </p>
        </Modal>
      )}
    </ScreenFrame>
  );
}
