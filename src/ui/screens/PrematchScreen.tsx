import { useMemo, useState, type CSSProperties } from 'react';
import { useApp } from '../../app/AppContext';
import type { MatchExtras } from '../../app/routes';
import { computePlayerStats, headToHead, lastMeetings, type PlayerStats } from '../../services/statistics';
import type { MatchConfig, ParticipantRef, Team } from '../../match-engine';
import type { Player, StoredMatch } from '../../services/persistence';
import { initials } from '../../services/players';
import { nextCategory, predict, type PlayerProgress } from '../../services/progression';
import { MODE_LABEL, ScreenFrame, TestModeBadge } from '../components/common';
import { AssetImage } from '../components/assets';
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
  const [info, setInfo] = useState(false);
  const ids = (team: Team) => participants.filter((p) => p.team === team).map((p) => p.playerId);
  // Estadísticas clasificatorias de cada jugador: balance de victorias.
  const stats = useMemo(() => {
    const ranked = matches.filter((m) => m.config.mode === 'ranked');
    return new Map(participants.map((p) => [p.playerId, computePlayerStats(p.playerId, ranked)]));
  }, [participants, matches]);
  // Últimos 10 clasificatorios de cada jugador; se marcan los jugados entre estos mismos equipos.
  const recent = useMemo(() => {
    const ranked = matches.filter((m) => m.config.mode === 'ranked').sort((a, b) => a.finishedAt - b.finishedAt);
    const same = (m: StoredMatch) => sameTeams(m, ids('white'), ids('blue'));
    return new Map(
      participants.map((p) => [
        p.playerId,
        ranked
          .filter((m) => m.participants.some((x) => x.playerId === p.playerId))
          .slice(-10)
          .map((m) => ({
            id: m.id,
            won: m.result.winner === m.participants.find((x) => x.playerId === p.playerId)!.team,
            same: same(m),
          })),
      ]),
    );
  }, [participants, matches]); // eslint-disable-line react-hooks/exhaustive-deps
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
        {/* Nombre del equipo sobre el borde superior de la tarjeta. */}
        <div className="pre-team-name">{team === 'white' ? 'BLANCO' : 'AZUL'}</div>
        {/* Pronóstico de este equipo: porcentaje y barra de neón por segmentos. */}
        {prediction?.available && (
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
        )}
        {list.map((p) => (
          <PlayerCard
            key={p.playerId}
            name={p.nameSnapshot}
            player={byId.get(p.playerId)}
            prog={progression?.players.get(p.playerId)}
            stats={stats.get(p.playerId)}
            recent={recent.get(p.playerId) ?? []}
            size={size}
          />
        ))}
      </div>
    );
  };

  return (
    <ScreenFrame
      className="pre-screen"
      background={<AssetImage name="fondo-prevision" className="pre-bg" fallback={null} />}
      // Tipo de partido en grande y centrado, con el mismo cartel de neón que el marcador.
      title={<span className="match-mode mode-chip-ranked">{MODE_LABEL[config.mode]}</span>}
      onBack={() => (extras?.tournament ? navigate({ name: 'tournamentDetail', id: extras.tournament.id }) : navigate({ name: 'select', config, participants }))}
      right={demoMode ? <TestModeBadge /> : undefined}
    >
      <div className="pre-layout">
        {teamCard('white')}
        <div className="pre-center-col">
        <div className="pre-center">
          <div className="pre-center-head">
            <div className="label">Pronóstico</div>
            {/* Detalles del cálculo, a la vista solo si se pulsa la «i». */}
            {prediction?.available && (
              <button className={`pre-info-btn ${info ? 'on' : ''}`} onClick={() => setInfo((v) => !v)} aria-expanded={info} aria-label="Cómo se calcula">
                i
              </button>
            )}
          </div>
          {!prediction ? (
            <div className="notice warn">Clasificación pendiente: la progresión está desactivada en Ajustes.</div>
          ) : !prediction.available ? (
            <div className="notice">{prediction.reason ?? 'Datos insuficientes.'} Puedes jugar igualmente.</div>
          ) : null}
          {/* Últimos enfrentamientos entre estos mismos equipos. */}
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
        </div>
        {/* Empezar, bajo la tarjeta de la previsión: así las fichas de los equipos tienen todo el alto. */}
        <button className="btn btn-primary btn-lg pre-start" onClick={() => navigate({ name: 'match', config, participants, extras })}>
          Empezar partido
        </button>
        </div>
        {teamCard('blue')}
      </div>
    </ScreenFrame>
  );
}

/** ¿Se jugó entre estas mismas alineaciones (en cualquier lado de la mesa)? */
function sameTeams(m: StoredMatch, whiteIds: string[], blueIds: string[]): boolean {
  const key = (ids: string[]) => [...ids].sort().join('|');
  const mw = key(m.participants.filter((p) => p.team === 'white').map((p) => p.playerId));
  const mb = key(m.participants.filter((p) => p.team === 'blue').map((p) => p.playerId));
  const kw = key(whiteIds);
  const kb = key(blueIds);
  return (mw === kw && mb === kb) || (mw === kb && mb === kw);
}

interface RecentGame {
  id: string;
  won: boolean;
  /** Jugado entre estos mismos dos equipos. */
  same: boolean;
}

/** Últimos 10 clasificatorios (G/P), del más antiguo al más reciente; los de este mismo duelo, resaltados. */
function RecentForm({ games }: { games: RecentGame[] }) {
  if (games.length === 0) return <span className="dim">Sin clasificatorios</span>;
  return (
    <span className="pre-recent" aria-label={`Últimos clasificatorios: ${games.map((g) => (g.won ? 'G' : 'P')).join(' ')}`}>
      {games.map((g) => (
        <span
          key={g.id}
          className={`form-chip ${g.won ? 'G' : 'P'} ${g.same ? 'same' : ''}`}
          title={g.same ? 'Contra este mismo equipo' : undefined}
        >
          {g.won ? 'G' : 'P'}
        </span>
      ))}
    </span>
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

const PHOTO_SIZE = { xl: 112, lg: 80, sm: 44 } as const;

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
  recent,
  size,
}: {
  name: string;
  player?: Player;
  prog?: PlayerProgress;
  stats?: PlayerStats;
  recent: RecentGame[];
  size: 'xl' | 'lg' | 'sm';
}) {
  const form = <RecentForm games={recent} />;
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
            ELO {prog.elo} · <span className="pre-cat" style={{ color: cat.color, fontWeight: 800 }}>{cat.name}</span> · Nv {prog.level}
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
