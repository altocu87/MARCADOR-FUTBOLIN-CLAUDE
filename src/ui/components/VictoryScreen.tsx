/**
 * Pantalla de final de partido (todo en una): equipo ganador en grande, progreso de
 * cada jugador con su barra de nivel animada, logros desbloqueados explicados y los
 * botones Ver estadísticas · Revancha · Nuevo partido.
 */
import { useEffect, useState, type CSSProperties } from 'react';
import { useApp } from '../../app/AppContext';
import type { SaveStatus } from '../../app/matchFinalizer';
import type { MatchState, ParticipantRef, Team } from '../../match-engine';
import { ACHIEVEMENTS, RARITY_LABEL, levelProgress, type MatchProgressEntry } from '../../services/progression';
import { sound } from '../../services/sound/sound';
import { AssetImage } from './assets';
import { Avatar } from './common';
import { AchievementIcon, CategoryBadge, Confetti } from './graphics';

const TEAM_LABEL: Record<Team, string> = { white: 'BLANCO', blue: 'AZUL' };

/** Cambio de lado para la revancha: Blanco ↔ Azul. */
export const swapSides = (parts: ParticipantRef[]): ParticipantRef[] =>
  parts.map((p) => ({ ...p, team: p.team === 'white' ? 'blue' : 'white' }));

export function VictoryScreen({
  state,
  matchId,
  save,
  inTournament,
  onStats,
  onRematch,
  onNewMatch,
  onTournament,
  onHome,
}: {
  state: MatchState;
  matchId: string;
  save: SaveStatus | null;
  inTournament: boolean;
  onStats: () => void;
  onRematch: () => void;
  onNewMatch: () => void;
  onTournament: () => void;
  onHome: () => void;
}) {
  const { players, progression, prefs } = useApp();
  const r = state.result!;
  const winners = state.participants.filter((p) => p.team === r.winner).sort((a, b) => a.slot - b.slot);
  const losers = state.participants.filter((p) => p.team !== r.winner).sort((a, b) => a.slot - b.slot);
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

  const line = r.penaltyScore
    ? `${r.score.white}–${r.score.blue} · Penaltis ${r.penaltyScore.white}–${r.penaltyScore.blue}`
    : r.reason === 'golden_goal'
      ? `${r.score.white}–${r.score.blue} · Gol de oro`
      : `${r.score.white}–${r.score.blue}`;

  const row = (p: ParticipantRef, i: number, winner: boolean) => (
    <PlayerProgressRow
      key={p.playerId}
      name={p.nameSnapshot}
      photo={byId.get(p.playerId)?.photo}
      entry={entries?.get(p.playerId)}
      winner={winner}
      delay={0.6 + i * 0.18}
    />
  );

  return (
    <div className={`overlay overlay-victory victory-${r.winner}`} aria-live="assertive">
      {prefs.effects !== 'off' && <Confetti count={prefs.effects === 'full' ? 70 : 24} />}
      <AssetImage name="modo-clasificatorio" className="victory-bg" fallback={null} />
      <div className="victory-rays" aria-hidden="true" />

      <div className="vic-head">
        <div className="victory-kicker">FINAL DEL PARTIDO · {line}</div>
        <div className="victory-title">¡VICTORIA {TEAM_LABEL[r.winner]}!</div>
      </div>

      <div className={`vic-body ${achievements.length ? 'with-ach' : ''}`}>
        <div className="vic-players scroll">
          {!save ? (
            <div className="vic-note">Guardando el partido…</div>
          ) : save.kind === 'test' ? (
            <div className="vic-note">Modo prueba: el partido no se guarda y no da experiencia.</div>
          ) : save.kind === 'error' ? (
            <div className="vic-note warn">No se pudo guardar. Entra en Ver estadísticas para reintentarlo.</div>
          ) : null}
          {winners.map((p, i) => row(p, i, true))}
          {losers.map((p, i) => row(p, winners.length + i, false))}
        </div>
        {achievements.length > 0 && (
          <div className="vic-ach scroll">
            <div className="vic-ach-title">🏅 LOGROS DESBLOQUEADOS</div>
            {achievements.map(({ def, who }, i) => (
              <div key={def!.id} className={`vic-ach-card rarity-${def!.rarity}`} style={{ '--d': `${1.2 + i * 0.2}s` } as CSSProperties}>
                <AchievementIcon id={def!.id} glyph={def!.icon} rarity={def!.rarity} size={42} />
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
        )}
      </div>

      <div className="vic-actions">
        <button className="btn vic-btn" onClick={onHome} title="Volver al inicio">
          ⌂
        </button>
        <button className="btn vic-btn" onClick={onStats} disabled={!save}>
          📊 Ver estadísticas
        </button>
        {inTournament ? (
          <button className="btn btn-primary vic-btn vic-main" onClick={onTournament} disabled={!save}>
            Volver al torneo
          </button>
        ) : (
          <>
            <button className="btn vic-btn" onClick={onRematch} disabled={!save} title="Mismos jugadores cambiando de lado">
              ⇄ Revancha
            </button>
            <button className="btn btn-primary vic-btn vic-main" onClick={onNewMatch} disabled={!save}>
              Nuevo partido
            </button>
          </>
        )}
      </div>
    </div>
  );
}

/** Fila de un jugador: foto, nombre, XP ganada, ELO y barra de nivel que se llena. */
function PlayerProgressRow({
  name,
  photo,
  entry,
  winner,
  delay,
}: {
  name: string;
  photo?: string;
  entry?: MatchProgressEntry;
  winner: boolean;
  delay: number;
}) {
  const before = entry ? entry.xpAfter - entry.xpGained : 0;
  const leveled = entry ? entry.levelAfter > entry.levelBefore : false;
  const from = entry ? levelProgress(before) : 0;
  const to = entry ? levelProgress(entry.xpAfter) : 0;
  // La barra arranca donde estaba; si sube de nivel se llena, destella y sigue desde cero.
  const [fill, setFill] = useState(from);
  const [phase, setPhase] = useState<'start' | 'up' | 'reset' | 'done'>('start');
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
    return () => timers.forEach((t) => window.clearTimeout(t));
  }, [entry?.xpAfter, leveled, to, delay]); // eslint-disable-line react-hooks/exhaustive-deps

  const levelReached = leveled && (phase === 'reset' || phase === 'done');
  const level = entry ? (levelReached ? entry.levelAfter : entry.levelBefore) : null;
  const rankUp = entry && entry.categoryAfter.id !== entry.categoryBefore.id && (entry.eloDelta ?? 0) > 0;

  return (
    <div className={`vic-row ${winner ? 'is-winner' : ''}`} style={{ '--d': `${delay - 0.3}s` } as CSSProperties}>
      <Avatar name={name} photo={photo} size={winner ? 52 : 42} />
      <div className="vic-row-main">
        <div className="vic-row-top">
          <strong className="vic-name">
            {winner && '👑 '}
            {name}
          </strong>
          {entry && <span className="vic-xp">+{entry.xpGained} XP</span>}
          {entry?.eloDelta !== undefined && (
            <span className={`vic-elo ${entry.eloDelta >= 0 ? 'up' : 'down'}`}>
              ELO {entry.eloDelta >= 0 ? '+' : ''}
              {entry.eloDelta}
            </span>
          )}
          {rankUp && (
            <span className="vic-rank">
              <CategoryBadge category={entry!.categoryAfter} size={22} /> ¡{entry!.categoryAfter.name}!
            </span>
          )}
        </div>
        {entry && (
          <div className="vic-level">
            <span className={`vic-lv ${levelReached ? 'leveled' : ''}`}>NV {level}</span>
            <div className={`vic-bar ${phase === 'up' ? 'flash' : ''}`}>
              <span
                style={{
                  width: `${Math.round(fill * 100)}%`,
                  // Al volver a cero tras subir de nivel, sin animación.
                  transition: phase === 'reset' ? 'none' : 'width 1s cubic-bezier(0.3, 0.8, 0.3, 1)',
                }}
              />
            </div>
            {levelReached && <span className="vic-levelup">⬆ ¡NIVEL {entry.levelAfter}!</span>}
          </div>
        )}
      </div>
    </div>
  );
}
