import { useMemo, useState, type CSSProperties } from 'react';
import { useApp } from '../../app/AppContext';
import type { MatchExtras } from '../../app/routes';
import { headToHead, lastMeetings } from '../../services/statistics';
import type { MatchConfig, ParticipantRef, Team } from '../../match-engine';
import { predict } from '../../services/progression';
import { roundLabel } from '../../services/tournaments';
import { MODE_LABEL, ScreenFrame, TestModeBadge } from '../components/common';
import { AssetImage } from '../components/assets';
import { TeamFrame } from '../components/PlayerCards';

export function PrematchScreen({
  config,
  participants,
  extras,
}: {
  config: MatchConfig;
  participants: ParticipantRef[];
  extras?: MatchExtras;
}) {
  const { navigate, matches, progression, demoMode, tournaments } = useApp();
  // En un torneo, el cartel dice la jornada o la ronda («JORNADA 2», «FINAL»…).
  const tournamentLabel = useMemo(() => {
    const t = extras?.tournament && tournaments.find((x) => x.id === extras.tournament!.id);
    const f = t && t.fixtures.find((x) => x.id === extras!.tournament!.fixtureId);
    return t && f ? roundLabel(t, f.round) : null;
  }, [extras, tournaments]);
  const [info, setInfo] = useState(false);
  // Últimos enfrentamientos entre estas mismas alineaciones (cualquier modalidad).
  const meetings = useMemo(() => {
    const ids = (team: Team) => participants.filter((p) => p.team === team).map((p) => p.playerId);
    return lastMeetings(matches, ids('white'), ids('blue'), 3);
  }, [participants, matches]);
  const rivalry = useMemo(() => {
    const ids = (team: Team) => participants.filter((p) => p.team === team).map((p) => p.playerId);
    const h = headToHead(matches, ids('white'), ids('blue'), false);
    return h.played >= 5 ? h : null;
  }, [participants, matches]);
  const prediction = useMemo(() => {
    if (!progression) return null;
    const ids = (team: Team) => participants.filter((p) => p.team === team).map((p) => p.playerId);
    return predict(ids('white'), ids('blue'), matches, progression);
  }, [participants, matches, progression]);
  
  // Equipo en marco de neón con su porcentaje del pronóstico bajo el nombre.
  const teamCard = (team: Team) => (
    <TeamFrame
      team={team}
      participants={participants}
      extra={
        prediction?.available && (
          <div className="pre-team-pct">
            <span className="pre-pct">{team === 'white' ? prediction.whitePct : prediction.bluePct} %</span>
            <span
              className="pre-tbar"
              style={{ '--p': `${team === 'white' ? prediction.whitePct : prediction.bluePct}%` } as CSSProperties}
              aria-hidden="true"
            >
              <span className="pre-tbar-track">
                <span className="pre-tbar-fill" />
              </span>
            </span>
          </div>
        )
      }
    />
  );

  return (
    <ScreenFrame
      className="pre-screen"
      background={<AssetImage name="fondo-prevision" className="pre-bg" fallback={null} />}
      // Tipo de partido en grande y centrado, con el mismo cartel de neón que el marcador.
      title={<span className="match-mode mode-chip-ranked">{tournamentLabel ?? MODE_LABEL[config.mode]}</span>}
      onBack={() => (extras?.tournament ? navigate({ name: 'tournamentDetail', id: extras.tournament.id }) : navigate({ name: 'select', config, participants }))}
      right={demoMode ? <TestModeBadge /> : undefined}
    >
      <div className="pre-layout">
        {teamCard('white')}
        <div className="pre-center-col">
          {/* «Pronóstico» enlazado con una línea de neón a los porcentajes de los dos equipos. */}
          <div className="pre-link">
            <span className="pre-link-line" aria-hidden="true" />
            <span className="pre-link-pill">
              PRONÓSTICO
              {prediction?.available && (
                <button className={`pre-info-btn ${info ? 'on' : ''}`} onClick={() => setInfo((v) => !v)} aria-expanded={info} aria-label="Cómo se calcula">
                  i
                </button>
              )}
            </span>
          </div>
          {!prediction ? (
            <div className="notice warn">Clasificación pendiente: la progresión está desactivada en Ajustes.</div>
          ) : !prediction.available ? (
            <div className="notice">{prediction.reason ?? 'Datos insuficientes.'} Puedes jugar igualmente.</div>
          ) : null}
          {/* Tarjeta propia: últimos enfrentamientos entre estos mismos equipos. */}
          <div className="pre-center">
            <div className="pre-meetings">
              <div className="label">Últimos enfrentamientos</div>
              {meetings.length === 0 ? (
                <div className="dim" style={{ fontSize: 13 }}>Primer enfrentamiento entre estos equipos.</div>
              ) : (
                meetings.map((m) => (
                  <div key={m.match.id} className="pre-meeting" title={MODE_LABEL[m.match.config.mode]}>
                    <span className="pre-meeting-date">
                      {new Date(m.match.finishedAt).toLocaleDateString('es-ES', { day: '2-digit', month: '2-digit', year: '2-digit' })}
                    </span>
                    <span className="pre-meeting-score">
                      <i className="pre-dot white" aria-label="Blanco" />
                      <b className={m.winner === 'white' ? 'won' : ''}>{m.white}</b>
                      <span className="dim">–</span>
                      <b className={m.winner === 'blue' ? 'won' : ''}>{m.blue}</b>
                      <i className="pre-dot blue" aria-label="Azul" />
                    </span>
                  </div>
                ))
              )}
            </div>
            {rivalry && (
              <div className="rivalry" style={{ fontSize: 12, alignSelf: 'center' }}>
                ⚔ CLÁSICO · {rivalry.whiteWins}–{rivalry.blueWins}
              </div>
            )}
          </div>
          {/* Panel de la «i»: confianza, cómo se calcula y aviso. */}
          {info && prediction?.available && (
            <button className="pre-info-panel" onClick={() => setInfo(false)}>
              <b>Confianza {prediction.confidence}</b>
              <span>
                {prediction.directMatches} enfrentamientos directos. Pesos del cálculo: ELO {(prediction.weights.elo * 100).toFixed(0)} % ·
                enfrentamientos directos {(prediction.weights.h2h * 100).toFixed(0)} % · forma {(prediction.weights.form * 100).toFixed(0)} %.
              </span>
              <span className="dim">Estimación, nunca una certeza. Fórmula propuesta pendiente de aprobación.</span>
              <span className="dim">Toca para cerrar</span>
            </button>
          )}
          {/* Empezar: neón verde, «EMPEZAR» arriba y «PARTIDO» abajo ocupando todo el botón. */}
          <button className="pre-start" onClick={() => navigate({ name: 'match', config, participants, extras })}>
            <span>EMPEZAR</span>
            <span>PARTIDO</span>
          </button>
        </div>
        {teamCard('blue')}
      </div>
    </ScreenFrame>
  );
}

