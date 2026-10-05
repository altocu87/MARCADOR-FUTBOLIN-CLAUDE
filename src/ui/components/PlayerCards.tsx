/**
 * Fichas de jugador con estilo de neón (Pronóstico y estadísticas del partido): equipo en marco de
 * neón, foto con marco del rango, datos de progresión y racha de los últimos 10 clasificatorios.
 */
import { TeamName } from './TeamName';
import { useMemo, useRef, useState, type CSSProperties, type ReactNode } from 'react';
import { createPortal } from 'react-dom';
import { useApp } from '../../app/AppContext';
import type { ParticipantRef, Team } from '../../match-engine';
import type { Player, StoredMatch } from '../../services/persistence';
import { initials } from '../../services/players';
import { nextCategory, type PlayerProgress } from '../../services/progression';
import { computePlayerStats, type PlayerStats } from '../../services/statistics';
import { MODE_LABEL, Modal } from './common';
import { CategoryBadge } from './graphics';

/** ¿Se jugó entre estas mismas alineaciones (en cualquier lado de la mesa)? */
export function sameTeams(m: StoredMatch, whiteIds: string[], blueIds: string[]): boolean {
  const key = (ids: string[]) => [...ids].sort().join('|');
  const mw = key(m.participants.filter((p) => p.team === 'white').map((p) => p.playerId));
  const mb = key(m.participants.filter((p) => p.team === 'blue').map((p) => p.playerId));
  const kw = key(whiteIds);
  const kb = key(blueIds);
  return (mw === kw && mb === kb) || (mw === kb && mb === kw);
}

interface RecentGame {
  id: string;
  match: StoredMatch;
  won: boolean;
  /** Jugado entre estos mismos dos equipos. */
  same: boolean;
}

/**
 * Últimos 10 clasificatorios (G/P). El más reciente queda hacia el centro de la pantalla
 * (a la derecha en Blanco, a la izquierda en Azul) y bien nítido; los más antiguos se van
 * apagando hacia el borde. Los jugados contra este mismo equipo, resaltados en dorado.
 */
function RecentForm({ games, team, onOpen }: { games: RecentGame[]; team: Team; onOpen: (m: StoredMatch) => void }) {
  if (games.length === 0) return <span className="dim">Sin clasificatorios</span>;
  // games llega del más antiguo al más reciente.
  const ordered = team === 'white' ? games : [...games].reverse();
  const n = games.length;
  return (
    <span
      className={`pre-recent pre-recent-${team}`}
      aria-label={`Últimos clasificatorios, del más antiguo al más reciente: ${games.map((g) => (g.won ? 'G' : 'P')).join(' ')}`}
    >
      {ordered.map((g) => {
        // 0 = el más reciente.
        const age = n - 1 - games.indexOf(g);
        return (
          // Al tocarla se abre una ventana con los datos de ese partido.
          <button
            key={g.id}
            className={`form-chip ${g.won ? 'G' : 'P'} ${g.same ? 'same' : ''} ${age === 0 ? 'newest' : ''}`}
            style={{ opacity: Math.max(0.45, 1 - age * 0.065) }}
            title={`${age === 0 ? 'El más reciente' : `Hace ${age + 1} partidos`}${g.same ? ' · contra este mismo equipo' : ''}`}
            onClick={() => onOpen(g.match)}
          >
            {g.won ? 'G' : 'P'}
          </button>
        );
      })}
    </span>
  );
}

/** Foto del jugador con el marco de su rango (solo se ve en el Clasificatorio). */
export function RankPhoto({ name, photo, rank, size }: { name: string; photo?: string; rank: string; size: number }) {
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
export function PlayerCard({
  name,
  player,
  prog,
  stats,
  recent,
  team,
  size,
  onOpen,
}: {
  name: string;
  player?: Player;
  prog?: PlayerProgress;
  stats?: PlayerStats;
  recent: RecentGame[];
  team: Team;
  size: 'xl' | 'lg' | 'sm';
  onOpen: (m: StoredMatch) => void;
}) {
  const form = <RecentForm games={recent} team={team} onOpen={onOpen} />;
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
        {/* Barra y texto en la misma línea para ahorrar alto. */}
        <div className="pre-rankrow">
          <div className="pre-rankbar" style={{ '--cat': cat.color } as CSSProperties}>
            <span style={{ width: `${Math.round(pct * 100)}%` }} />
          </div>
          <div className="pre-rankbar-text">{next ? `Faltan ${next.min - prog.elo} para ${next.name}` : 'Rango máximo'}</div>
        </div>
        <div className={`pre-form pre-form-${team}`}>{form}</div>
      </div>
    </div>
  );
}

/** Ventana con los datos de un partido de la racha: fecha, tipo, resultado y equipos. */
export function MatchDetail({ match, playerId, onClose }: { match: StoredMatch; playerId: string; onClose: () => void }) {
  const r = match.result;
  const names = (t: Team) =>
    match.participants
      .filter((p) => p.team === t)
      .sort((a, b) => a.slot - b.slot)
      .map((p) => p.nameSnapshot);
  const mine = match.participants.find((p) => p.playerId === playerId);
  const won = mine?.team === r.winner;
  const side = (t: Team) => (
    <div className={`md-team md-${t} ${r.winner === t ? 'won' : ''}`}>
      <div className="md-team-label">
        <TeamName team={t} participants={match.participants} size={20} />
        {r.winner === t && ' 🏆'}
      </div>
      {names(t).map((n) => (
        <div key={n} className={`md-name ${mine?.team === t && n === mine.nameSnapshot ? 'me' : ''}`}>
          {n}
        </div>
      ))}
    </div>
  );
  return (
    <Modal
      title={MODE_LABEL[match.config.mode]}
      onClose={onClose}
      actions={
        <button className="btn btn-primary" onClick={onClose}>
          Cerrar
        </button>
      }
    >
      <div className="md">
        <div className="md-meta">
          {new Date(match.finishedAt).toLocaleString('es-ES', { day: '2-digit', month: '2-digit', year: '2-digit', hour: '2-digit', minute: '2-digit' })}
          {mine && <b className={won ? 'md-won' : 'md-lost'}>{won ? ' · Victoria' : ' · Derrota'} de {mine.nameSnapshot}</b>}
        </div>
        <div className="md-score">
          {side('white')}
          <div className="md-nums">
            <span className={r.winner === 'white' ? 'won' : ''}>{r.score.white}</span>
            <span className="md-sep">–</span>
            <span className={r.winner === 'blue' ? 'won' : ''}>{r.score.blue}</span>
          </div>
          {side('blue')}
        </div>
        {r.penaltyScore && (
          <div className="md-meta">
            Penaltis {r.penaltyScore.white}–{r.penaltyScore.blue}
          </div>
        )}
        {r.reason === 'golden_goal' && <div className="md-meta">Gol de oro en la prórroga</div>}
      </div>
    </Modal>
  );
}

/**
 * Equipo en marco de neón (blanco o celeste) con su nombre encajado arriba y las fichas de sus
 * jugadores, que cambian de tamaño según cuántos son. `extra` va justo bajo el nombre.
 * Al tocar una casilla de la racha se abre la ventana con los datos de ese partido.
 */
export function TeamFrame({
  team,
  participants,
  extra,
  compact,
  winner,
}: {
  team: Team;
  participants: ParticipantRef[];
  extra?: ReactNode;
  /** Con poco alto disponible: fichas un tamaño más pequeñas. */
  compact?: boolean;
  /** Equipo ganador: lleva una corona de neón sobre el nombre. */
  winner?: boolean;
}) {
  const { matches, progression, players } = useApp();
  const [detail, setDetail] = useState<{ match: StoredMatch; playerId: string } | null>(null);
  const frame = useRef<HTMLDivElement>(null);
  const list = participants.filter((p) => p.team === team).sort((a, b) => a.slot - b.slot);
  const size = list.length === 1 ? (compact ? 'lg' : 'xl') : list.length === 2 ? 'lg' : 'sm';
  const byId = new Map(players.map((p) => [p.id, p]));
  const ids = (t: Team) => participants.filter((p) => p.team === t).map((p) => p.playerId);
  // Estadísticas clasificatorias de cada jugador (balance) y sus últimos 10 clasificatorios;
  // se marcan los jugados entre estos mismos equipos.
  const { stats, recent } = useMemo(() => {
    const ranked = matches.filter((m) => m.config.mode === 'ranked').sort((a, b) => a.finishedAt - b.finishedAt);
    const same = (m: StoredMatch) => sameTeams(m, ids('white'), ids('blue'));
    return {
      stats: new Map(list.map((p) => [p.playerId, computePlayerStats(p.playerId, ranked)])),
      recent: new Map(
        list.map((p) => [
          p.playerId,
          ranked
            .filter((m) => m.participants.some((x) => x.playerId === p.playerId))
            .slice(-10)
            .map((m) => ({
              id: m.id,
              match: m,
              won: m.result.winner === m.participants.find((x) => x.playerId === p.playerId)!.team,
              same: same(m),
            })),
        ]),
      ),
    };
  }, [participants, matches]); // eslint-disable-line react-hooks/exhaustive-deps
  return (
    <div ref={frame} className={`pre-team pre-${team} pre-${size} ${compact ? 'pre-compact' : ''}`}>
      <div className="pre-team-name">
        {winner && <NeonCrown />}
        <TeamName team={team} participants={participants} size={26} />
      </div>
      {extra}
      {list.map((p) => (
        <PlayerCard
          key={p.playerId}
          name={p.nameSnapshot}
          player={byId.get(p.playerId)}
          prog={progression?.players.get(p.playerId)}
          stats={stats.get(p.playerId)}
          recent={recent.get(p.playerId) ?? []}
          team={team}
          size={size}
          onOpen={(match) => setDetail({ match, playerId: p.playerId })}
        />
      ))}
      {/* La ventana se dibuja sobre toda la pantalla, no dentro del marco del equipo. */}
      {detail &&
        createPortal(
          <MatchDetail match={detail.match} playerId={detail.playerId} onClose={() => setDetail(null)} />,
          frame.current?.closest('.screen') ?? document.body,
        )}
    </div>
  );
}

/** Corona dorada de neón que se posa sobre el nombre del equipo ganador. */
function NeonCrown() {
  return (
    <svg className="neon-crown" viewBox="0 0 64 40" aria-label="Ganador" role="img">
      <path
        d="M6 34 L4 10 L20 22 L32 4 L44 22 L60 10 L58 34 Z"
        fill="rgba(242, 201, 76, 0.18)"
        stroke="#ffd75a"
        strokeWidth="3"
        strokeLinejoin="round"
      />
      <line x1="8" y1="34" x2="56" y2="34" stroke="#ffd75a" strokeWidth="3" strokeLinecap="round" />
      <circle cx="4" cy="9" r="3" fill="#fff3b0" />
      <circle cx="32" cy="3.5" r="3.2" fill="#fff3b0" />
      <circle cx="60" cy="9" r="3" fill="#fff3b0" />
    </svg>
  );
}
