import { useEffect, useRef, useState, type CSSProperties } from 'react';
import { useApp } from '../../app/AppContext';
import { persistFinishedMatch, toStoredMatch, type SaveStatus } from '../../app/matchFinalizer';
import type { MatchExtras } from '../../app/routes';
import {
  COUNTDOWN_MS,
  GOAL_LOCK_MS,
  countdownRemaining,
  getClock,
  getPenaltyScore,
  getPeriodScore,
  getScore,
  goalLockRemaining,
  goalStreak,
  isSinglePeriod,
  isSuddenDeath,
  nextPenaltyTeam,
  type MatchConfig,
  type MatchState,
  type ParticipantRef,
  type Period,
  type Team,
} from '../../match-engine';
import type { Player } from '../../services/persistence';
import { initials } from '../../services/players';
import { displayTitle } from '../../services/progression';
import { sound } from '../../services/sound/sound';
import { formatDuration, headToHead } from '../../services/statistics';
import { MODE_LABEL, Modal, TestModeBadge } from '../components/common';
import { AssetImage } from '../components/assets';
import { AnnulShow, GoalShow } from '../components/GoalEffect';
import { VictoryScreen, swapSides } from '../components/VictoryScreen';
import { Banner, CountdownRing, NeonGoal } from '../components/graphics';
import { SevenSegment } from '../components/SevenSegment';
import { useMatchController, type MatchController } from './useMatchController';

export const PERIOD_LABEL: Record<Period, string> = {
  first: '1ª PARTE',
  second: '2ª PARTE',
  overtime: 'PRÓRROGA',
  shootout: 'PENALTIS',
};

const TEAM_LABEL: Record<Team, string> = { white: 'BLANCO', blue: 'AZUL' };
const CLOCK_GREEN = '#3DFF7A';
const CLOCK_YELLOW = '#FFD43B';
const CLOCK_RED = '#FF4D5E';
const CLOCK_MINT = '#7DFFD8';

/** Ilustración de cada modalidad, usada como fondo muy tenue del partido. */
const MODE_ART: Record<MatchConfig['mode'], string> = {
  quick: 'modo-rapido',
  chaos: 'modo-caos',
  ranked: 'modo-clasificatorio',
};

/** «1 gol», «5 goles». */
function goalsText(n: number): string {
  return `${n} ${n === 1 ? 'gol' : 'goles'}`;
}

function conditionText(config: MatchConfig): string {
  if (config.endCondition === 'goals') return `Gana quien llegue a ${goalsText(config.goalsPerPeriod)}`;
  if (config.endCondition === 'time') return `2 partes de ${config.minutesPerPeriod} min`;
  return `A ${goalsText(config.goalsPerPeriod)} o 2 partes de ${config.minutesPerPeriod} min`;
}

/** Rótulo del periodo: por goles no hay partes, es «PARTIDO». */
export function periodLabel(state: MatchState): string {
  return state.period === 'first' && isSinglePeriod(state.config) ? 'PARTIDO' : PERIOD_LABEL[state.period];
}

export function MatchScreen({
  config,
  participants,
  resume,
  extras,
}: {
  config: MatchConfig;
  participants: ParticipantRef[];
  resume?: MatchState;
  extras?: MatchExtras;
}) {
  const ctl = useMatchController(config, participants, resume, extras);
  const { state } = ctl;
  const { repos, navigate, refresh, prefs, players, matches } = useApp();
  const [confirmExit, setConfirmExit] = useState(false);
  const [save, setSave] = useState<SaveStatus | null>(null);
  const finishing = useRef(false);

  // Rivalidad: historial entre estas mismas alineaciones (cualquier modalidad).
  // Se calcula al empezar con las alineaciones del partido; no cambia durante el juego.
  const [rivalry] = useState(() => {
    const ids = (t: Team) => participants.filter((p) => p.team === t).map((p) => p.playerId);
    const h = headToHead(matches, ids('white'), ids('blue'), false);
    return h.played >= 5 ? h : null;
  });

  // Final: aplicación → resultado completo → repositorio local; después, pantalla de victoria.
  useEffect(() => {
    if (state.phase !== 'finished' || finishing.current) return;
    finishing.current = true;
    void (async () => {
      const s = await persistFinishedMatch(state, repos, extras);
      if (s.kind === 'saved') await refresh();
      setSave(s);
    })();
  }, [state, repos, refresh, extras]);

  const goSummary = () => {
    if (!save) return;
    navigate({ name: 'summary', match: toStoredMatch(state, extras), save, live: state, extras });
  };

  const abandon = async () => {
    await repos.activeMatch.clear();
    navigate({ name: 'home' });
  };

  const photos = new Map(players.map((p) => [p.id, p.photo]));
  const playersById = new Map(players.map((p) => [p.id, p]));

  return (
    <section className={`screen match mode-${state.config.mode}`} onPointerDown={() => sound.unlock()}>
      <AssetImage name={MODE_ART[state.config.mode]} className="match-bg" fallback={null} />
      {state.phase === 'penalties' || (state.phase === 'finished' && state.period === 'shootout') ? (
        <PenaltiesView ctl={ctl} />
      ) : (
        <ScoreboardView ctl={ctl} players={playersById} />
      )}

      {/* Al marcar: pantalla completa «¡GOL! · EQUIPO …» (varía en cada gol). */}
      {ctl.lastGoal && state.phase !== 'penalties' && state.phase !== 'finished' && (
        <GoalShow
          key={`show-${ctl.lastGoal.id}`}
          team={ctl.lastGoal.team!}
          level={prefs.effects}
          seed={ctl.lastGoal.id}
          label={ctl.goalLabel}
          people={state.participants
            .filter((p) => p.team === ctl.lastGoal!.team)
            .sort((a, b) => a.slot - b.slot)
            .map((p) => ({ id: p.playerId, name: p.nameSnapshot, photo: photos.get(p.playerId) }))}
        />
      )}
      {/* Al restar (−1) o deshacer un gol: «GOL ANULADO» en rojo. */}
      {ctl.annulled && state.phase !== 'penalties' && (
        <AnnulShow key={ctl.annulled.id} team={ctl.annulled.team} level={prefs.effects} seed={ctl.annulled.id} />
      )}

      {ctl.banner && state.phase !== 'finished' && (
        <Banner key={ctl.banner.id} text={ctl.banner.text} sub={ctl.banner.sub} tone={ctl.banner.tone} />
      )}

      {state.phase === 'countdown' && <CountdownOverlay ctl={ctl} rivalry={state.period === 'first' ? rivalry : null} />}
      {state.phase === 'paused' && (
        <div className="overlay overlay-pause">
          <div className="overlay-title">PAUSA</div>
          <div className="muted">El reloj está detenido · no se admiten goles</div>
          <button className="btn btn-primary btn-lg overlay-cta" onClick={() => ctl.send({ type: 'RESUME' })} autoFocus>
            ▶ CONTINUAR
          </button>
          <button className="btn btn-danger btn-sm" onClick={() => setConfirmExit(true)}>
            Abandonar partido
          </button>
        </div>
      )}
      {state.phase === 'periodEnd' && <PeriodEndOverlay ctl={ctl} />}
      {state.phase === 'finished' && (
        <VictoryScreen
          state={state}
          matchId={state.id}
          save={save}
          inTournament={!!extras?.tournament}
          onStats={goSummary}
          onRematch={() => navigate({ name: 'match', config: state.config, participants: swapSides(state.participants), extras: undefined })}
          onTournament={() => extras?.tournament && navigate({ name: 'tournamentDetail', id: extras.tournament.id })}
          onHome={() => navigate({ name: 'home' })}
        />
      )}

      {confirmExit && (
        <Modal
          title="¿Abandonar el partido?"
          onClose={() => setConfirmExit(false)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setConfirmExit(false)}>
                Seguir jugando
              </button>
              <button className="btn btn-danger" onClick={abandon}>
                Abandonar
              </button>
            </>
          }
        >
          <p className="muted" style={{ margin: 0 }}>
            El partido se descarta: no se guarda en el historial ni cuenta para estadísticas.
          </p>
        </Modal>
      )}
    </section>
  );
}

// ---------------------------------------------------------------------------

function ScoreboardView({ ctl, players }: { ctl: MatchController; players: Map<string, Player> }) {
  const { demoMode, progression } = useApp();
  const { state, now, send } = ctl;
  const score = getScore(state);
  const clock = getClock(state, now);
  const lock = goalLockRemaining(state, now);
  const playing = state.phase === 'playing';
  const flash = now - ctl.lockFlashAt < 700;
  const periodScore = getPeriodScore(state);
  const canCorrect = state.phase === 'playing' || state.phase === 'paused';
  const streak = goalStreak(state);
  // Color del reloj: menta en la prórroga; con cuenta atrás, verde → amarillo (último minuto) → rojo (últimos 20 s).
  const clockColor =
    state.period === 'overtime'
      ? CLOCK_MINT
      : clock.remainingMs !== null && clock.remainingMs <= 20_000
        ? CLOCK_RED
        : clock.remainingMs !== null && clock.remainingMs <= 60_000
          ? CLOCK_YELLOW
          : CLOCK_GREEN;
  const lastMinute =
    state.config.mode === 'chaos' &&
    state.config.chaos?.doubleLastMinute &&
    state.period !== 'overtime' &&
    clock.remainingMs !== null &&
    clock.remainingMs <= 60_000;
  // Bajo el tipo de partido: la parte en juego (si hay varias) y a cuántos goles se juega.
  const info = [
    isSinglePeriod(state.config) && state.period === 'first' ? null : PERIOD_LABEL[state.period],
    state.config.endCondition !== 'time' && state.period !== 'overtime' ? `A ${goalsText(state.config.goalsPerPeriod)}` : null,
  ]
    .filter(Boolean)
    .join(' · ');

  // Anular gol: botón rojo a cada lado de la Pausa.
  const minus = (t: Team) => (
    <button
      className={`annul-btn annul-${t}`}
      onClick={() => send({ type: 'MINUS_ONE', team: t })}
      disabled={!canCorrect || periodScore[t] === 0}
      aria-label={`Anular un gol de ${TEAM_LABEL[t]}`}
    >
      <svg width="34" height="34" viewBox="0 0 24 24" aria-hidden="true">
        <circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" strokeWidth="2.4" />
        <path d="M5.6 18.4 18.4 5.6" stroke="currentColor" strokeWidth="2.4" strokeLinecap="round" />
      </svg>
      <span>
        ANULAR
        <br />
        GOL
      </span>
    </button>
  );

  // Ficha de jugador: la misma que en la selección de jugadores (foto, nombre, nivel/ELO y título).
  const card = (p: ParticipantRef, t: Team) => {
    const player = players.get(p.playerId);
    const prog = progression?.players.get(p.playerId);
    const title = displayTitle(prog, player?.titleId);
    return (
      <div key={p.playerId} className={`team-card team-card-${t} match-card`}>
        <span className="team-card-photo">
          {player?.photo ? (
            <img src={player.photo} alt="" draggable={false} />
          ) : (
            <span className="pick-initials">{initials(p.nameSnapshot)}</span>
          )}
        </span>
        <span className="team-card-info">
          <span className="team-card-name">{p.nameSnapshot}</span>
          {prog && (
            <span className="pick-meta">
              Nv {prog.level}
              {prog.rankedPlayed ? ` · ELO ${prog.elo}` : ''}
            </span>
          )}
          {title && <span className="pick-title">{title}</span>}
        </span>
      </div>
    );
  };

  const team = (t: Team) => {
    const people = state.participants.filter((p) => p.team === t).sort((a, b) => a.slot - b.slot);
    return (
      <div className={`team-side side-${t}`}>
        <div className="score-wrap">
          <button
            className={`score-btn score-${t} ${lock > 0 ? 'locked' : ''} ${ctl.matchPoint.includes(t) ? 'match-point' : ''}`}
            onClick={() => send({ type: 'GOAL', team: t, source: 'touch' })}
            disabled={!playing}
            aria-label={`Gol ${TEAM_LABEL[t]}. Marcador ${score[t]}`}
          >
            <span className="score-sheen" aria-hidden="true" />
            <span className="score-team">{TEAM_LABEL[t]}</span>
            {streak && streak.team === t && streak.count >= 3 && <span className="streak-badge">🔥 x{streak.count}</span>}
            <span className={`score-num ${score[t] >= 10 ? 'two' : ''}`}>{score[t]}</span>
            {lock > 0 && (
              <span className="lock-bar" aria-hidden="true">
                <span style={{ width: `${(lock / GOAL_LOCK_MS) * 100}%` }} />
              </span>
            )}
          </button>
        </div>
        <div className={`team-cards ${people.length >= 3 ? 'many' : `count-${people.length}`}`}>{people.map((p) => card(p, t))}</div>
      </div>
    );
  };

  return (
    <div className="match-main">
      {team('white')}
      <div className="center-col">
        {/* Tipo de partido, en grande, y debajo a cuántos goles se juega. */}
        <span className={`match-mode mode-chip-${state.config.mode}`}>{MODE_LABEL[state.config.mode]}</span>
        <div className="period-goals">{info || '\u00a0'}</div>
        {/* Avisos del momento (gol de oro, bola de partido…): hueco fijo para que el reloj no salte. */}
        <div className="center-badges">
          {state.period === 'overtime' && <span className="badge badge-ranked">GOL DE ORO</span>}
          {ctl.matchPoint.length > 0 && state.period !== 'overtime' && (
            <span className="badge badge-danger match-point-badge">
              BOLA DE PARTIDO {ctl.matchPoint.length === 1 ? `· ${TEAM_LABEL[ctl.matchPoint[0]]}` : ''}
            </span>
          )}
          {lastMinute && <span className="badge badge-chaos">ÚLTIMO MINUTO · GOLES x2</span>}
        </div>
        <div className="clock-panel" style={{ '--clock': clockColor } as CSSProperties}>
          <SevenSegment
            className="clock"
            text={formatDuration(clock.remainingMs ?? clock.periodElapsedMs)}
            height={104}
            ghost={false}
            color={clockColor}
            label={`Reloj ${formatDuration(clock.remainingMs ?? clock.periodElapsedMs)}`}
          />
        </div>
        {/* Anular gol de Blanco · Pausa · Anular gol de Azul. */}
        <div className="center-actions">
          {minus('white')}
          <button
            className="pause-btn"
            onClick={() => send({ type: state.phase === 'paused' ? 'RESUME' : 'PAUSE' })}
            disabled={!canCorrect}
            aria-label={state.phase === 'paused' ? 'Continuar' : 'Pausa'}
          >
            <span className="pause-icon" aria-hidden="true">
              {state.phase === 'paused' ? '▶' : <><i /><i /></>}
            </span>
            <span className="pause-text">{state.phase === 'paused' ? 'SEGUIR' : 'PAUSA'}</span>
          </button>
          {minus('blue')}
        </div>
        {/* Cómo se gana el partido, legible bajo la Pausa. */}
        <div className="center-cond">{conditionText(state.config)}</div>
        <div className={`lock-msg ${lock > 0 ? 'on' : ''} ${flash ? 'flash' : ''}`} role="status">
          {lock > 0 ? `Bloqueo ${(lock / 1000).toFixed(1)} s` : ' '}
        </div>
        {(demoMode || state.config.testMode) && (
          <div className="center-test">
            <TestModeBadge />
          </div>
        )}
      </div>
      {team('blue')}
    </div>
  );
}

function CountdownOverlay({ ctl, rivalry }: { ctl: MatchController; rivalry: { played: number; whiteWins: number; blueWins: number } | null }) {
  const { state, now, send } = ctl;
  const remaining = countdownRemaining(state, now);
  const n = Math.max(1, Math.ceil(remaining / 1000));
  const lastN = useRef(0);
  useEffect(() => {
    if (n !== lastN.current) {
      lastN.current = n;
      sound.play('countdown');
    }
  }, [n]);
  return (
    <button className="overlay overlay-countdown" onClick={() => send({ type: 'SKIP_COUNTDOWN' })} autoFocus>
      <span className="overlay-sub">
        {periodLabel(state)}
        {state.period === 'overtime' ? ' · GOL DE ORO · 60 s' : ''}
      </span>
      <span className="countdown-stack">
        <CountdownRing progress={remaining / COUNTDOWN_MS} size={250} />
        <span key={n} className="countdown-num">
          <SevenSegment text={String(n)} height={150} color="#62D6FF" />
        </span>
      </span>
      {rivalry && (
        <span className="rivalry">
          ⚔ CLÁSICO · {rivalry.whiteWins}–{rivalry.blueWins} en {rivalry.played} enfrentamientos
        </span>
      )}
      <span className="overlay-hint">Toca para saltar</span>
    </button>
  );
}

function PeriodEndOverlay({ ctl }: { ctl: MatchController }) {
  const { state, send } = ctl;
  const score = getScore(state);
  const last = state.periods[state.periods.length - 1];
  let title = 'FINAL 1ª PARTE';
  let cta = 'CONTINUAR 2ª PARTE';
  let note = '';
  if (state.period === 'second') {
    title = 'FINAL 2ª PARTE';
    cta = 'IR A PRÓRROGA';
    note = 'Empate: prórroga de 60 segundos con gol de oro.';
  } else if (state.period === 'overtime') {
    title = 'FIN DE LA PRÓRROGA';
    cta = 'IR A PENALTIS';
    note = 'Sin gol en la prórroga: tanda de penaltis.';
  }
  const reason = last?.endReason === 'time' ? 'por tiempo' : last?.endReason === 'goals' ? 'por goles' : '';
  return (
    <div className="overlay overlay-period">
      <div className="overlay-sub">{title} {reason && <span className="dim">· {reason}</span>}</div>
      <div className="period-score">
        <span className="ps-white">{score.white}</span>
        <span className="ps-sep">–</span>
        <span className="ps-blue">{score.blue}</span>
      </div>
      {last && (
        <div className="muted">
          Parcial {PERIOD_LABEL[last.period].toLowerCase()}: {last.score.white}–{last.score.blue} · {formatDuration(last.durationMs)}
        </div>
      )}
      {note && <div className="muted">{note}</div>}
      <button className="btn btn-primary btn-lg overlay-cta" onClick={() => send({ type: 'CONTINUE' })} autoFocus>
        {cta}
      </button>
    </div>
  );
}

/** Pantalla de victoria: confeti, ganadores con foto y título, y melodía del jugador. */
function PenaltiesView({ ctl }: { ctl: MatchController }) {
  const { demoMode } = useApp();
  const { state, send } = ctl;
  const pen = getPenaltyScore(state);
  const score = getScore(state);
  const turn = nextPenaltyTeam(state);
  const sudden = isSuddenDeath(state);
  const active = state.phase === 'penalties';
  const lastKick = state.penalties[state.penalties.length - 1];

  const column = (t: Team) => {
    const kicks = state.penalties.filter((k) => k.team === t);
    const slots = Math.max(state.config.penaltyRounds, kicks.length + (active && turn === t ? 1 : 0));
    const isTurn = active && turn === t;
    return (
      <div className={`pen-col pen-${t} ${isTurn ? 'is-turn' : ''}`}>
        {isTurn && <span className="spotlight" aria-hidden="true" />}
        <div className="pen-team">{TEAM_LABEL[t]}</div>
        <div className="pen-score">{pen[t]}</div>
        <div className="pen-dots" aria-label={`Lanzamientos ${TEAM_LABEL[t]}: ${kicks.map((k) => (k.scored ? 'gol' : 'fallo')).join(', ') || 'ninguno'}`}>
          {Array.from({ length: slots }, (_, i) => {
            const k = kicks[i];
            return (
              <span key={i} className={`pen-dot ${k ? (k.scored ? 'hit' : 'miss') : ''} ${i >= state.config.penaltyRounds ? 'sd' : ''}`}>
                {k ? (k.scored ? '✓' : '✕') : ''}
              </span>
            );
          })}
        </div>
        <div className="pen-actions">
          <button
            className="btn btn-lg pen-goal"
            disabled={!isTurn}
            onClick={() => send({ type: 'PENALTY', team: t, scored: true, source: 'touch' })}
          >
            GOL
          </button>
          <button
            className="btn btn-lg pen-miss"
            disabled={!isTurn}
            onClick={() => send({ type: 'PENALTY', team: t, scored: false, source: 'touch' })}
          >
            FALLO
          </button>
        </div>
      </div>
    );
  };

  return (
    <div className="penalties">
      <header className="match-top">
        <span className="period-chip">TANDA DE PENALTIS</span>
        {sudden && <span className="badge badge-danger">MUERTE SÚBITA</span>}
        <span style={{ flex: 1 }} />
        <span className="muted" style={{ fontSize: 13 }}>
          Marcador ordinario {score.white}–{score.blue}
        </span>
        {(demoMode || state.config.testMode) && <TestModeBadge />}
      </header>
      <div className="pen-main">
        {column('white')}
        <div className="pen-center">
          <NeonGoal result={lastKick ? (lastKick.scored ? 'goal' : 'miss') : null} kickKey={lastKick?.eventId ?? 'none'} />
          {active ? (
            <>
              <div className="label">Lanza</div>
              <div className={`pen-turn team-${turn}`}>{TEAM_LABEL[turn]}</div>
              <div className="dim" style={{ fontSize: 12, textAlign: 'center' }}>
                Intento {state.penalties.filter((k) => k.team === turn).length + 1}
                {sudden ? ' · se decide por parejas' : ` de ${state.config.penaltyRounds}`}
              </div>
              <button
                className="btn btn-sm"
                style={{ marginTop: 'auto' }}
                disabled={state.penalties.length === 0}
                onClick={() => send({ type: 'UNDO_PENALTY' })}
              >
                ↶ Deshacer lanzamiento
              </button>
            </>
          ) : (
            <div className="pen-turn">DECIDIDA</div>
          )}
        </div>
        {column('blue')}
      </div>
    </div>
  );
}
