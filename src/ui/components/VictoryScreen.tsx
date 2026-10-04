/**
 * Pantalla de final de partido (todo en una): equipo ganador y marcador en grande,
 * datos curiosos del partido, los dos equipos en columnas con el progreso de cada
 * jugador (barra de nivel animada, XP que cuenta sola y su desglose al tocar),
 * logros desbloqueados explicados y los botones Revancha · Nuevo partido ·
 * Ver estadísticas · Inicio.
 */
import { useEffect, useState, type CSSProperties } from 'react';
import { useApp } from '../../app/AppContext';
import type { SaveStatus } from '../../app/matchFinalizer';
import { validGoalsFromEvents, type MatchState, type ParticipantRef, type Team } from '../../match-engine';
import {
  ACHIEVEMENTS,
  MAX_LEVEL,
  RARITY_LABEL,
  levelForXp,
  levelProgress,
  xpForLevel,
  type MatchProgressEntry,
} from '../../services/progression';
import { sound } from '../../services/sound/sound';
import { formatDuration } from '../../services/statistics';
import { AssetImage } from './assets';
import { Avatar } from './common';
import { AchievementIcon, CategoryBadge, Confetti } from './graphics';

const TEAM_LABEL: Record<Team, string> = { white: 'BLANCO', blue: 'AZUL' };
const OTHER: Record<Team, Team> = { white: 'blue', blue: 'white' };

/** Cambio de lado para la revancha: Blanco ↔ Azul. */
export const swapSides = (parts: ParticipantRef[]): ParticipantRef[] =>
  parts.map((p) => ({ ...p, team: p.team === 'white' ? 'blue' : 'white' }));

/** Datos curiosos sacados del propio partido: duración, remontada, racha, portería a cero… */
function matchFacts(state: MatchState): string[] {
  const r = state.result!;
  const w = r.winner;
  const facts = [`Duración ${formatDuration(r.totalTimeMs)}`];
  const run = { white: 0, blue: 0 };
  let streakTeam: Team | null = null;
  let streak = 0;
  let best = 0;
  let maxDeficit = 0;
  for (const g of validGoalsFromEvents(state.events)) {
    const t = g.team!;
    run[t] += g.value ?? 1;
    streak = t === streakTeam ? streak + 1 : 1;
    streakTeam = t;
    best = Math.max(best, streak);
    maxDeficit = Math.max(maxDeficit, run[OTHER[w]] - run[w]);
  }
  if (maxDeficit >= 2) facts.push(`¡Remontada de ${maxDeficit} goles!`);
  else if (maxDeficit === 1) facts.push('Remontada');
  if (best >= 3) facts.push(`${best} goles seguidos`);
  if (r.score[OTHER[w]] === 0 && r.score[w] > 0) facts.push('Portería a cero');
  if (r.reason === 'golden_goal') facts.push('Gol de oro');
  if (r.penaltyScore) facts.push(`Penaltis ${r.penaltyScore.white}–${r.penaltyScore.blue}`);
  return facts;
}

export function VictoryScreen({
  state,
  matchId,
  save,
  inTournament,
  onStats,
  onRematch,
  onTournament,
  onHome,
}: {
  state: MatchState;
  matchId: string;
  save: SaveStatus | null;
  inTournament: boolean;
  onStats: () => void;
  onRematch: () => void;
  onTournament: () => void;
  onHome: () => void;
}) {
  const { players, progression, prefs } = useApp();
  const r = state.result!;
  const byTeam = (t: Team) => state.participants.filter((p) => p.team === t).sort((a, b) => a.slot - b.slot);
  const winners = byTeam(r.winner);
  const byId = new Map(players.map((p) => [p.id, p]));

  // Melodía del primer ganador (una vez, al aparecer la pantalla).
  const [anthem] = useState(() => byId.get(winners[0]?.playerId ?? '')?.anthem);
  useEffect(() => {
    const id = window.setTimeout(() => sound.playAnthem(anthem), 900);
    return () => window.clearTimeout(id);
  }, [anthem]);

  // El progreso solo existe si el partido se ha guardado.
  const entries = save?.kind === 'saved' ? progression?.byMatch.get(matchId) : undefined;
  const levelUps = entries ? [...entries.values()].some((e) => e.levelAfter > e.levelBefore) : false;
  useEffect(() => {
    if (!levelUps) return;
    const id = window.setTimeout(() => sound.play('levelUp'), 1700);
    return () => window.clearTimeout(id);
  }, [levelUps]);

  // Logros nuevos agrupados: cada logro una vez, con quién lo ha conseguido.
  const unlocked = new Map<string, string[]>();
  for (const e of entries?.values() ?? []) {
    const name = state.participants.find((p) => p.playerId === e.playerId)?.nameSnapshot ?? '?';
    for (const id of e.unlocked) unlocked.set(id, [...(unlocked.get(id) ?? []), name]);
  }
  const achievements = [...unlocked.entries()]
    .map(([id, who]) => ({ def: ACHIEVEMENTS.find((a) => a.id === id), who }))
    .filter((x) => x.def);

  const facts = matchFacts(state);

  const column = (team: Team, list: ParticipantRef[], winner: boolean, offset: number) => (
    <div className="vic-col">
      <div className={`vic-col-label vic-col-${team}`}>
        {TEAM_LABEL[team]}
        {winner && ' 🏆'}
      </div>
      {list.map((p, i) => (
        <PlayerProgressRow
          key={p.playerId}
          name={p.nameSnapshot}
          photo={byId.get(p.playerId)?.photo}
          entry={entries?.get(p.playerId)}
          delay={0.7 + (offset + i) * 0.2}
        />
      ))}
    </div>
  );

  return (
    <div className={`overlay overlay-victory victory-${r.winner}`} aria-live="assertive">
      {prefs.effects !== 'off' && <Confetti count={prefs.effects === 'full' ? 70 : 24} />}
      <AssetImage name="modo-clasificatorio" className="victory-bg" fallback={null} />
      <div className="victory-rays" aria-hidden="true" />

      {/* Barra de arriba: Inicio · título · Revancha (o Volver al torneo) · Estadísticas. */}
      <div className="vic-topbar">
        <button className="vic-icon-btn vic-home" onClick={onHome} aria-label="Inicio" title="Inicio">
          <svg width="30" height="30" viewBox="0 0 24 24" aria-hidden="true">
            <path d="M3 11.5 12 4l9 7.5" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round" />
            <path d="M5.5 10v9.5h4.5V14h4v5.5h4.5V10" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinejoin="round" />
          </svg>
        </button>
        <div className="vic-title-wrap">
          <div className="victory-title">¡VICTORIA {TEAM_LABEL[r.winner]}!</div>
        </div>
        {inTournament ? (
          <button className="btn btn-primary vic-main" onClick={onTournament} disabled={!save}>
            Volver al torneo
          </button>
        ) : (
          <button className="btn btn-primary vic-main" onClick={onRematch} disabled={!save} title="Mismos jugadores cambiando de lado">
            ⇄ REVANCHA
          </button>
        )}
        <button className="vic-icon-btn vic-stats" onClick={onStats} disabled={!save} aria-label="Ver estadísticas" title="Estadísticas">
          <svg width="30" height="30" viewBox="0 0 24 24" aria-hidden="true">
            <path d="M4 20h16" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" />
            <rect x="5.5" y="12" width="3" height="6" rx="1" fill="currentColor" />
            <rect x="10.5" y="7" width="3" height="11" rx="1" fill="currentColor" />
            <rect x="15.5" y="4" width="3" height="14" rx="1" fill="currentColor" />
          </svg>
        </button>
      </div>

      <div className="vic-head">
        {/* Marcador grande, estilo televisión. */}
        <div className="vic-score">
          <span className={`vic-score-team vic-col-white ${r.winner === 'white' ? 'won' : ''}`}>BLANCO</span>
          <span className={`vic-score-num ${r.winner === 'white' ? 'won' : ''}`}>{r.score.white}</span>
          <span className="vic-score-sep">–</span>
          <span className={`vic-score-num ${r.winner === 'blue' ? 'won' : ''}`}>{r.score.blue}</span>
          <span className={`vic-score-team vic-col-blue ${r.winner === 'blue' ? 'won' : ''}`}>AZUL</span>
        </div>
        <div className="vic-facts">{facts.join(' · ')}</div>
      </div>

      {!save ? (
        <div className="vic-note">Guardando el partido…</div>
      ) : save.kind === 'test' ? (
        <div className="vic-note">Modo prueba: el partido no se guarda y no da experiencia.</div>
      ) : save.kind === 'error' ? (
        <div className="vic-note warn">No se pudo guardar. Entra en Ver estadísticas para reintentarlo.</div>
      ) : null}

      <div className="vic-body scroll">
        <div className="vic-teams">
          {/* Siempre Blanco a la izquierda y Azul a la derecha; los ganadores aparecen primero. */}
          {column('white', byTeam('white'), r.winner === 'white', r.winner === 'white' ? 0 : winners.length)}
          {column('blue', byTeam('blue'), r.winner === 'blue', r.winner === 'blue' ? 0 : winners.length)}
        </div>
        {achievements.length > 0 && (
          <div className="vic-ach">
            <div className="vic-ach-title">🏅 LOGROS DESBLOQUEADOS</div>
            <div className="vic-ach-list">
              {achievements.map(({ def, who }, i) => (
                <div key={def!.id} className={`vic-ach-card rarity-${def!.rarity}`} style={{ '--d': `${1.4 + i * 0.2}s` } as CSSProperties}>
                  <AchievementIcon id={def!.id} glyph={def!.icon} rarity={def!.rarity} size={34} />
                  <div className="vic-ach-text">
                    <div className="vic-ach-name">
                      {def!.name} <span className="vic-ach-rarity">{RARITY_LABEL[def!.rarity]}</span>
                    </div>
                    <div className="vic-ach-desc">{def!.description}</div>
                    <div className="vic-ach-who">{who.join(' · ')}</div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

    </div>
  );
}

/** XP dentro del nivel: lo que lleva y lo que necesita para el siguiente. */
function levelSpan(xp: number) {
  const level = levelForXp(xp);
  if (level >= MAX_LEVEL) return { level, have: 0, need: 0 };
  const base = xpForLevel(level);
  return { level, have: xp - base, need: xpForLevel(level + 1) - base };
}

/**
 * Fila de un jugador: foto, nombre, XP que cuenta sola, ELO, iconos de sus logros y
 * barra de nivel que se llena. Al tocarla se ve de dónde sale la XP.
 */
function PlayerProgressRow({
  name,
  photo,
  entry,
  delay,
}: {
  name: string;
  photo?: string;
  entry?: MatchProgressEntry;
  delay: number;
}) {
  const before = entry ? entry.xpAfter - entry.xpGained : 0;
  const leveled = entry ? entry.levelAfter > entry.levelBefore : false;
  const from = entry ? levelProgress(before) : 0;
  const to = entry ? levelProgress(entry.xpAfter) : 0;
  const [open, setOpen] = useState(false);
  // La barra arranca donde estaba; si sube de nivel se llena, destella y sigue desde cero.
  const [fill, setFill] = useState(from);
  const [phase, setPhase] = useState<'start' | 'up' | 'reset' | 'done'>('start');
  // Contador de XP que sube de 0 a lo ganado mientras se llena la barra.
  const [shownXp, setShownXp] = useState(0);
  useEffect(() => {
    if (!entry) return;
    const at = (ms: number, fn: () => void) => window.setTimeout(fn, delay * 1000 + ms);
    const timers = leveled
      ? [
          at(0, () => {
            setFill(1);
            setPhase('up');
          }),
          at(1100, () => {
            setFill(0);
            setPhase('reset');
          }),
          at(1150, () => {
            setFill(to);
            setPhase('done');
          }),
        ]
      : [
          at(0, () => {
            setFill(to);
            setPhase('done');
          }),
        ];
    // Cuenta en ~1 s, con un clic suave cada pocos pasos.
    const steps = 20;
    for (let k = 1; k <= steps; k += 1) {
      timers.push(
        at((k * 1000) / steps, () => {
          setShownXp(Math.round((entry.xpGained * k) / steps));
          if (k % 4 === 0) sound.play('ui');
        }),
      );
    }
    return () => timers.forEach((t) => window.clearTimeout(t));
  }, [entry?.xpAfter, leveled, to, delay]); // eslint-disable-line react-hooks/exhaustive-deps

  const levelReached = leveled && (phase === 'reset' || phase === 'done');
  const span = entry ? levelSpan(phase === 'done' ? entry.xpAfter : before) : null;
  const rankUp = entry && entry.categoryAfter.id !== entry.categoryBefore.id && (entry.eloDelta ?? 0) > 0;
  const achs = (entry?.unlocked ?? []).map((id) => ACHIEVEMENTS.find((a) => a.id === id)).filter(Boolean);

  return (
    <button
      className={`vic-row ${open ? 'open' : ''}`}
      style={{ '--d': `${delay - 0.3}s` } as CSSProperties}
      onClick={() => entry && setOpen((o) => !o)}
      aria-expanded={open}
      aria-label={`${name}${entry ? `: +${entry.xpGained} XP. Toca para ver el desglose.` : ''}`}
    >
      <Avatar name={name} photo={photo} size={46} />
      <span className="vic-row-main">
        <span className="vic-row-top">
          <strong className="vic-name">{name}</strong>
          {/* XP en columna fija: queda alineada en todas las filas. */}
          {entry && <span className="vic-xp">+{shownXp} XP</span>}
          {entry?.eloDelta !== undefined && (
            <span className={`vic-elo ${entry.eloDelta >= 0 ? 'up' : 'down'}`}>
              ELO {entry.eloDelta >= 0 ? '+' : ''}
              {entry.eloDelta}
            </span>
          )}
          {rankUp && (
            <span className="vic-rank">
              <CategoryBadge category={entry!.categoryAfter} size={20} /> ¡{entry!.categoryAfter.name}!
            </span>
          )}
          {/* Iconos de los logros que ha conseguido este jugador. */}
          {achs.length > 0 && (
            <span className="vic-row-achs">
              {achs.map((a) => (
                <AchievementIcon key={a!.id} id={a!.id} glyph={a!.icon} rarity={a!.rarity} size={22} />
              ))}
            </span>
          )}
        </span>
        {entry && span && (
          <>
            <span className="vic-level">
              <span className={`vic-lv ${levelReached ? 'leveled' : ''}`}>NV {span.level}</span>
              <span className={`vic-bar ${phase === 'up' ? 'flash' : ''} ${levelReached ? 'leveled' : ''}`}>
                <span
                  className="vic-bar-fill"
                  style={{
                    width: `${Math.round(fill * 100)}%`,
                    // Al volver a cero tras subir de nivel, sin animación.
                    transition: phase === 'reset' ? 'none' : 'width 1s cubic-bezier(0.3, 0.8, 0.3, 1)',
                  }}
                />
                {/* XP dentro del nivel, sobre la propia barra. */}
                {levelReached && <span className="vic-bar-shine" aria-hidden="true" />}
                <span className="vic-bar-text">
                  {/* Al subir de nivel, el aviso va dentro de la propia barra. */}
                  {levelReached ? (
                    <>
                      <b className="vic-bar-up">⬆ ¡NIVEL {entry.levelAfter}!</b>
                      {span.need > 0 && <span className="vic-bar-xp">{span.have}/{span.need}</span>}
                    </>
                  ) : span.need > 0 ? (
                    `${span.have}/${span.need}`
                  ) : (
                    'MÁX'
                  )}
                </span>
              </span>
            </span>
          </>
        )}
        {/* Desglose de la XP (al tocar la fila). */}
        {open && entry && (
          <span className="vic-breakdown">
            {entry.xpBreakdown.map((l, i) => (
              <span key={i} className="vic-bd-item">
                {l.label} <b>+{l.xp}</b>
              </span>
            ))}
          </span>
        )}
      </span>
    </button>
  );
}
