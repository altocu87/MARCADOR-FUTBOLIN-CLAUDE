import { useMemo, type CSSProperties } from 'react';
import { useApp } from '../../app/AppContext';
import type { MatchExtras } from '../../app/routes';
import { computePlayerStats, headToHead, lastMeetings, type PlayerStats } from '../../services/statistics';
import type { MatchConfig, ParticipantRef, Team } from '../../match-engine';
import type { Player } from '../../services/persistence';
import { initials } from '../../services/players';
import { nextCategory, predict, type PlayerProgress } from '../../services/progression';
import { FormChips, MODE_LABEL, ScreenFrame, TestModeBadge } from '../components/common';
import { CategoryBadge } from '../components/graphics';

export function PrematchScreen({
  config,
  participants,
  extras,
}: {
  config: MatchConfig;
  participants: ParticipantRef[];
  extras?: MatchExtras;
}) {
  const { navigate, matches, progression, players, demoMode } = useApp();
  // Estadísticas clasificatorias de cada jugador: forma (últimos 5) y balance de victorias.
  const stats = useMemo(() => {
    const ranked = matches.filter((m) => m.config.mode === 'ranked');
    return new Map(participants.map((p) => [p.playerId, computePlayerStats(p.playerId, ranked)]));
  }, [participants, matches]);
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
  const byId = new Map(players.map((p) => [p.id, p]));

  // Ficha según cuántos jugadores tenga el equipo: 1 = enorme, 2 = grande y completa, 3–4 = compacta.
  const teamCard = (team: Team) => {
    const list = participants.filter((p) => p.team === team).sort((a, b) => a.slot - b.slot);
    const size = list.length === 1 ? 'xl' : list.length === 2 ? 'lg' : 'sm';
    return (
      <div className={`pre-team pre-${team} pre-${size}`}>
        <div className="label">{team === 'white' ? 'BLANCO' : 'AZUL'}</div>
        {list.map((p) => (
          <PlayerCard
            key={p.playerId}
            name={p.nameSnapshot}
            player={byId.get(p.playerId)}
            prog={progression?.players.get(p.playerId)}
            stats={stats.get(p.playerId)}
            size={size}
          />
        ))}
      </div>
    );
  };

  return (
    <ScreenFrame
      title="Previsión"
      subtitle="Clasificatorio"
      onBack={() => (extras?.tournament ? navigate({ name: 'tournamentDetail', id: extras.tournament.id }) : navigate({ name: 'select', config, participants }))}
      right={demoMode ? <TestModeBadge /> : undefined}
    >
      <div className="pre-layout">
        {teamCard('white')}
        <div className="pre-center">
          {!prediction ? (
            <div className="notice warn">Clasificación pendiente: la progresión está desactivada en Ajustes.</div>
          ) : !prediction.available ? (
            <div className="notice">{prediction.reason ?? 'Datos insuficientes.'} Puedes jugar igualmente.</div>
          ) : (
            <>
              <div className="label">Previsión estadística</div>
              <div className="pre-pcts">
                <span>{prediction.whitePct} %</span>
                <span>{prediction.bluePct} %</span>
              </div>
              <div className="pre-bar" aria-hidden="true">
                <span style={{ width: `${prediction.whitePct}%` }} />
              </div>
              <div className="muted" style={{ fontSize: 13 }}>
                Blanco {prediction.whitePct} % · Azul {prediction.bluePct} % · Confianza {prediction.confidence}
              </div>
              <div className="dim" style={{ fontSize: 11 }}>
                {prediction.directMatches} enfrentamientos directos · pesos ELO {(prediction.weights.elo * 100).toFixed(0)} % ·
                directos {(prediction.weights.h2h * 100).toFixed(0)} % · forma {(prediction.weights.form * 100).toFixed(0)} %
              </div>
            </>
          )}
          {/* Últimos enfrentamientos entre estos mismos equipos. */}
          <div className="pre-meetings">
            <div className="label">Últimos enfrentamientos</div>
            {meetings.length === 0 ? (
              <div className="dim" style={{ fontSize: 13 }}>Primer enfrentamiento entre estos equipos.</div>
            ) : (
              meetings.map((m) => (
                <div key={m.match.id} className="pre-meeting">
                  <span className="pre-meeting-date">
                    {new Date(m.match.finishedAt).toLocaleDateString('es-ES', { day: '2-digit', month: '2-digit', year: '2-digit' })}
                  </span>
                  <span className="pre-meeting-mode">{MODE_LABEL[m.match.config.mode]}</span>
                  <span className="pre-meeting-score">
                    <b className={m.winner === 'white' ? 'won' : ''}>BLANCO {m.white}</b>
                    <span className="dim">–</span>
                    <b className={m.winner === 'blue' ? 'won' : ''}>{m.blue} AZUL</b>
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
          <div className="dim" style={{ fontSize: 10, marginTop: 'auto' }}>
            Estimación, nunca una certeza. Fórmula propuesta pendiente de aprobación.
          </div>
          {/* Empezar, abajo del centro: así las fichas de los equipos tienen todo el alto. */}
          <button className="btn btn-primary btn-lg pre-start" onClick={() => navigate({ name: 'match', config, participants, extras })}>
            Empezar partido
          </button>
        </div>
        {teamCard('blue')}
      </div>
    </ScreenFrame>
  );
}

/** Foto del jugador con el marco de su rango (solo se ve en el Clasificatorio). */
function RankPhoto({ name, photo, rank, size }: { name: string; photo?: string; rank: string; size: number }) {
  return (
    <span className={`rank-frame rank-${rank}`} style={{ width: size, height: size }}>
      <span className="rank-photo" style={{ fontSize: size * 0.34 }}>
        {photo ? <img src={photo} alt="" draggable={false} /> : initials(name)}
      </span>
    </span>
  );
}

const PHOTO_SIZE = { xl: 160, lg: 100, sm: 52 } as const;

/**
 * Ficha de jugador de la Previsión. Grande (1 o 2 por equipo): foto con marco, rango con su
 * escudo, ELO y nivel, lo que falta para el siguiente rango, balance en clasificatorios y forma.
 * Compacta (3 o 4): foto con marco, ELO, rango, nivel y forma.
 */
function PlayerCard({
  name,
  player,
  prog,
  stats,
  size,
}: {
  name: string;
  player?: Player;
  prog?: PlayerProgress;
  stats?: PlayerStats;
  size: 'xl' | 'lg' | 'sm';
}) {
  const form = <FormChips form={stats?.form ?? []} empty="Sin clasificatorios" />;
  if (!prog) {
    return (
      <div className="pre-player">
        <RankPhoto name={name} photo={player?.photo} rank="none" size={PHOTO_SIZE[size]} />
        <div className="pre-info">
          <div className="pre-name">{name}</div>
          <div className="muted" style={{ fontSize: 12 }}>Progresión pendiente</div>
        </div>
      </div>
    );
  }
  const cat = prog.category;
  if (size === 'sm') {
    return (
      <div className="pre-player">
        <RankPhoto name={name} photo={player?.photo} rank={cat.id} size={PHOTO_SIZE.sm} />
        <div className="pre-info">
          <div className="pre-name">{name}</div>
          <div className="muted" style={{ fontSize: 12 }}>
            ELO {prog.elo} · <span style={{ color: cat.color, fontWeight: 800 }}>{cat.name}</span> · Nv {prog.level}
          </div>
          <div className="pre-form">{form}</div>
        </div>
      </div>
    );
  }
  // Progreso dentro del rango, hacia el siguiente.
  const next = nextCategory(cat);
  const from = Number.isFinite(cat.min) ? cat.min : (next?.min ?? prog.elo) - 100;
  const pct = next ? Math.max(0, Math.min(1, (prog.elo - from) / (next.min - from))) : 1;
  const r = stats?.ranked;
  return (
    <div className="pre-player pre-player-big">
      <div className="pre-player-top">
        <RankPhoto name={name} photo={player?.photo} rank={cat.id} size={PHOTO_SIZE[size]} />
        <div className="pre-info">
          <div className="pre-name">{name}</div>
          {/* Rango con su escudo. */}
          <div className="pre-rank">
            <CategoryBadge category={cat} size={size === 'xl' ? 30 : 26} />
            <span style={{ color: cat.color }}>{cat.name}</span>
          </div>
          <div className="pre-elo">
            ELO <b>{prog.elo}</b> · Nv {prog.level}
          </div>
          {/* Balance en clasificatorios. */}
          {r && r.played > 0 && (
            <div className="pre-record">
              {r.wins} G · {r.losses} P{r.winPct !== null ? ` · ${Math.round(r.winPct)} %` : ''}
            </div>
          )}
        </div>
      </div>
      {/* Cuánto falta para el siguiente rango y forma reciente, a todo el ancho de la ficha. */}
      <div className="pre-player-bottom">
        <div className="pre-rankbar" style={{ '--cat': cat.color } as CSSProperties}>
          <span style={{ width: `${Math.round(pct * 100)}%` }} />
        </div>
        <div className="pre-rankbar-text">{next ? `Faltan ${next.min - prog.elo} para ${next.name}` : 'Rango máximo'}</div>
        <div className="pre-form">{form}</div>
      </div>
    </div>
  );
}
