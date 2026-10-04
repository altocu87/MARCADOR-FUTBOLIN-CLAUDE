/**
 * Controlador del partido en la interfaz: mantiene el estado del motor, avanza
 * el reloj con tiempo real, traduce entradas a comandos y dispara sonido, voz y
 * efectos SOLO tras la validación del motor. Guarda snapshots de recuperación.
 */
import { useCallback, useEffect, useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import { makeSnapshot } from '../../app/recovery';
import type { MatchExtras } from '../../app/routes';
import { hardwareHub } from '../../inputs/hardware/hub';
import { annulTeamFromCommand, teamFromCommand, inputBus } from '../../inputs/inputBus';
import {
  advance,
  createMatch,
  dispatch,
  firstHandicapAtMs,
  getClock,
  getScore,
  goalLockRemaining,
  goalMoment,
  handicapEndText,
  handicapsEnabled,
  handicapText,
  nextHandicapAtMs,
  pickHandicap,
  isSuddenDeath,
  matchPointTeams,
  type CommandOutcome,
  type EngineCommand,
  type MatchConfig,
  type MatchEvent,
  type MatchState,
  type ParticipantRef,
  type Period,
  type Team,
  isSinglePeriod,
} from '../../match-engine';
import { newId } from '../../services/ids';
import { sound } from '../../services/sound/sound';
import { voice } from '../../services/sound/voice';

const SNAPSHOT_EVERY_MS = 5000;

const TEAM_VOICE: Record<Team, string> = { white: 'equipo blanco', blue: 'equipo azul' };
const PERIOD_VOICE: Record<Period, string> = {
  first: 'primera parte',
  second: 'segunda parte',
  overtime: 'prórroga',
  shootout: 'tanda de penaltis',
};
const PERIOD_BANNER: Record<Period, string> = {
  first: '1ª PARTE',
  second: '2ª PARTE',
  overtime: 'PRÓRROGA',
  shootout: 'PENALTIS',
};

export interface Celebration {
  id: string;
  text: string;
  sub?: string;
  tone: 'accent' | 'gold' | 'danger' | 'chaos';
}

export interface MatchController {
  state: MatchState;
  now: number;
  send(command: EngineCommand): CommandOutcome;
  /** Último gol aceptado (para efectos). */
  lastGoal: MatchEvent | null;
  /** Último gol anulado (−1 o deshacer), para su animación en rojo. */
  annulled: { id: string; team: Team } | null;
  /** Texto especial del último gol (empate, remontada…). */
  goalLabel: string | null;
  /** Rótulo animado en curso (inicio de parte, muerte súbita…). */
  banner: Celebration | null;
  /** Marca de tiempo del último rechazo por bloqueo (feedback visual). */
  lockFlashAt: number;
  matchPoint: Team[];
  /** Partido Loco: ms que quedan de la cuenta atrás antes de revelar el hándicap (0 = revelado). */
  handicapIntroLeft: number;
  /** Partido Loco: saltar la cuenta atrás y enseñar ya el hándicap. */
  skipHandicapIntro(): void;
}

/** Duración de la cuenta atrás que precede a cada hándicap (3 · 2 · 1). */
export const HANDICAP_INTRO_MS = 3000;
/** Tras un gol, se espera a que acabe la celebración antes de sacar un hándicap. */
const HANDICAP_AFTER_GOAL_MS = 4500;

const MOMENT_LABEL = { equalizer: '¡EMPATE!', lead: '¡POR DELANTE!', comeback: '¡REMONTADA!', double: '¡GOL DOBLE!' } as const;

/** Rótulo del gol según el hándicap del Partido Loco que lo afectó. */
function bonusLabel(e: MatchEvent): string | null {
  const b = e.bonus ?? [];
  if (b.includes('frozen')) return 'NO CUENTA · CONGELADO';
  if (b.includes('triple')) return '¡GOL TRIPLE!';
  if (b.includes('steal')) return '¡ROBO!';
  if (b.includes('penalty')) return '¡PENALTI!';
  if (b.includes('double')) return '¡GOL DOBLE!';
  return null;
}

export function useMatchController(
  config: MatchConfig,
  participants: ParticipantRef[],
  resume?: MatchState,
  extras?: MatchExtras,
): MatchController {
  const { repos } = useApp();
  const [state, setState] = useState<MatchState>(
    () => resume ?? createMatch(newId('m'), config, participants, Date.now()),
  );
  const [now, setNow] = useState(() => Date.now());
  const [lastGoal, setLastGoal] = useState<MatchEvent | null>(null);
  const [goalLabel, setGoalLabel] = useState<string | null>(null);
  const [annulled, setAnnulled] = useState<{ id: string; team: Team } | null>(null);
  const [banner, setBanner] = useState<Celebration | null>(null);
  const [lockFlashAt, setLockFlashAt] = useState(0);
  // Partido Loco: fin de la cuenta atrás del hándicap anunciado (epoch ms).
  const [introUntil, setIntroUntil] = useState(0);
  const introUntilRef = useRef(0);
  const revealTimer = useRef<number | undefined>(undefined);
  const stateRef = useRef(state);
  const lastSnapshotAt = useRef(0);
  const matchPointKey = useRef('');
  const bannerTimer = useRef<number | undefined>(undefined);

  const showBanner = useCallback((b: Omit<Celebration, 'id'>, ms = 1800) => {
    window.clearTimeout(bannerTimer.current);
    setBanner({ ...b, id: newId('b') });
    sound.play('whoosh');
    bannerTimer.current = window.setTimeout(() => setBanner(null), ms);
  }, []);

  useEffect(
    () => () => {
      window.clearTimeout(bannerTimer.current);
      window.clearTimeout(revealTimer.current);
    },
    [],
  );

  // Revelar el hándicap: fin de la cuenta atrás (sola o al tocar) y anuncio por voz.
  const reveal = useCallback((at: number) => {
    window.clearTimeout(revealTimer.current);
    introUntilRef.current = at;
    setIntroUntil(at);
    const h = stateRef.current.handicap;
    if (!h || h.stage !== 'announce') return;
    const say = () => {
      const cur = stateRef.current.handicap;
      if (!cur || cur.spec.id !== h.spec.id) return;
      const t = handicapText(cur.spec, stateRef.current.participants);
      sound.play('whoosh');
      voice.say(`${t.title.replace(/[¡!]/g, '')}. ${t.detail}`);
    };
    const wait = at - Date.now();
    if (wait > 0) revealTimer.current = window.setTimeout(say, wait);
    else say();
  }, []);
  const skipHandicapIntro = useCallback(() => reveal(Date.now()), [reveal]);

  // Las placas conectadas reciben el estado inicial (p. ej. «STATE countdown»).
  useEffect(() => {
    hardwareHub.notifyMatch(stateRef.current, []);
  }, []);

  const saveSnapshot = useCallback(
    (s: MatchState, t: number) => {
      if (s.config.testMode || s.phase === 'finished') return;
      lastSnapshotAt.current = t;
      // Un fallo de snapshot no detiene el partido.
      void repos.activeMatch.save(makeSnapshot(s, t, extras)).catch(() => undefined);
    },
    [repos, extras],
  );

  const commit = useCallback(
    (next: MatchState, events: MatchEvent[], t: number) => {
      if (next === stateRef.current) return;
      const prev = stateRef.current;
      stateRef.current = next;
      setState(next);
      // Gol anulado: el marcador de un equipo baja en la misma parte (−1 o deshacer un gol).
      // Un gol «robo» del Partido Loco también baja al rival, pero no es una anulación.
      if (prev.period === next.period && !events.some((e) => e.type === 'GOAL' && e.steal)) {
        const before = getScore(prev);
        const after = getScore(next);
        const down = (['white', 'blue'] as Team[]).find((tm) => after[tm] < before[tm]);
        if (down) {
          setLastGoal(null);
          setAnnulled({ id: newId('a'), team: down });
          sound.play('error');
        }
      }
      for (const e of events) {
        if (e.type === 'GOAL' && e.team) {
          setLastGoal(e);
          const moment = goalMoment(next, e);
          setGoalLabel(bonusLabel(e) ?? (moment ? MOMENT_LABEL[moment] : null));
          sound.play('goal');
          if (next.phase !== 'finished') {
            const sc = e.scoreAfter;
            voice.say(`¡Gol del ${TEAM_VOICE[e.team]}! ${moment ? MOMENT_LABEL[moment].replace(/[¡!]/g, '') + '. ' : ''}${sc.white} a ${sc.blue}.`);
          }
        } else if (e.type === 'MATCH_END' && e.team) {
          sound.play('matchEnd');
          voice.say(`¡Final del partido! Gana el ${TEAM_VOICE[e.team]}.`);
        } else if (e.type === 'PERIOD_END' && next.phase !== 'finished') {
          sound.play('periodEnd');
          voice.say(`Final de la ${PERIOD_VOICE[e.period]}.`);
        } else if (e.type === 'PERIOD_START') {
          sound.play('countdownGo');
          showBanner(
            e.period === 'overtime'
              ? { text: 'PRÓRROGA', sub: 'GOL DE ORO', tone: 'gold' }
              : { text: e.period === 'first' && isSinglePeriod(next.config) ? 'PARTIDO' : PERIOD_BANNER[e.period], sub: '¡A JUGAR!', tone: next.config.mode === 'chaos' ? 'chaos' : 'accent' },
            1500,
          );
        } else if (e.type === 'HANDICAP_START' && e.handicap) {
          // Partido Loco: cuenta atrás y, al terminar, el hándicap a pantalla completa.
          const replaced = next.handicap?.replaced;
          if (replaced) voice.say(handicapEndText(replaced));
          sound.play('countdown');
          reveal(t + HANDICAP_INTRO_MS);
        } else if (e.type === 'HANDICAP_END' && e.handicap && e.reason === 'time') {
          sound.play('countdownGo');
          voice.say('¡Vuelta a la normalidad!');
        } else if (e.type === 'HANDICAP_END' && e.handicap && e.reason === 'used') {
          showBanner({ text: 'FIN DEL HÁNDICAP', sub: handicapText(e.handicap, next.participants).short, tone: 'chaos' }, 1500);
        } else if (e.type === 'HANDICAP_PENALTY') {
          if (!e.scored) {
            sound.play('error');
            voice.say('¡Falla el penalti!');
          }
        } else if (e.type === 'SHOOTOUT_START') {
          showBanner({ text: 'PENALTIS', sub: 'TANDA', tone: 'gold' });
          voice.say('Tanda de penaltis.');
        } else if (e.type === 'PENALTY') {
          sound.play(e.scored ? 'goal' : 'error');
          voice.say(e.scored ? '¡Gol!' : '¡Falla!');
          if (!isSuddenDeath(prev) && isSuddenDeath(next) && next.phase === 'penalties') {
            showBanner({ text: 'MUERTE SÚBITA', tone: 'danger' });
          }
        }
      }
      // Bola de partido: aviso al aparecer.
      const mp = matchPointTeams(next);
      const key = mp.length ? `${next.period}:${getScore(next).white}-${getScore(next).blue}` : '';
      if (key && key !== matchPointKey.current && next.period !== 'overtime') {
        sound.play('matchPoint');
        voice.say('¡Bola de partido!', { interrupt: false });
      }
      matchPointKey.current = key;
      hardwareHub.notifyMatch(next, events);
      saveSnapshot(next, t);
    },
    [saveSnapshot, showBanner, reveal],
  );

  const send = useCallback(
    (command: EngineCommand): CommandOutcome => {
      const t = Date.now();
      const out = dispatch(stateRef.current, command, t);
      commit(out.state, out.events, t);
      if (!out.accepted && out.reason === 'goal_lock') {
        setLockFlashAt(t);
        sound.play('error');
      }
      setNow(t);
      return out;
    },
    [commit],
  );

  // Reloj: avanza con tiempo real (no cuenta renderizados).
  useEffect(() => {
    const id = window.setInterval(() => {
      const t = Date.now();
      const r = advance(stateRef.current, t);
      // También sin eventos nuevos (p. ej. se sigue jugando tras «vuelta a la normalidad»).
      if (r.events.length || r.state !== stateRef.current) commit(r.state, r.events, t);
      else if (stateRef.current.runningSince !== undefined && t - lastSnapshotAt.current > SNAPSHOT_EVERY_MS) {
        saveSnapshot(stateRef.current, t);
      }
      // Partido Loco: ¿toca hándicap? Cada 30–60 s de juego, nunca en la prórroga ni en plena
      // celebración de un gol.
      const s = stateRef.current;
      if (
        handicapsEnabled(s.config) &&
        s.phase === 'playing' &&
        s.period !== 'overtime' &&
        goalLockRemaining(s, t) === 0 &&
        t - (s.lastGoalAt ?? -Infinity) > HANDICAP_AFTER_GOAL_MS
      ) {
        const total = getClock(s, t).totalElapsedMs;
        if (total >= (s.nextHandicapAtMs ?? firstHandicapAtMs(s.id))) {
          const out = dispatch(s, { type: 'HANDICAP_START', handicap: pickHandicap(s, newId('h')), nextAtMs: nextHandicapAtMs(total, Math.random) }, t);
          if (out.accepted) commit(out.state, out.events, t);
        }
      }
      setNow(t);
    }, 100);
    return () => window.clearInterval(id);
  }, [commit, saveSnapshot]);

  // Guardar al ocultar/cerrar la pestaña.
  useEffect(() => {
    const onHide = () => saveSnapshot(stateRef.current, Date.now());
    window.addEventListener('pagehide', onHide);
    document.addEventListener('visibilitychange', onHide);
    return () => {
      window.removeEventListener('pagehide', onHide);
      document.removeEventListener('visibilitychange', onHide);
    };
  }, [saveSnapshot]);

  // Entradas comunes (teclado, simulador, futuros pulsadores/sensores).
  useEffect(
    () =>
      inputBus.subscribe(({ command, source }) => {
        const s = stateRef.current;
        const team = teamFromCommand(command);
        const annul = annulTeamFromCommand(command);
        if (annul) {
          // Pulsación larga: −1 al equipo en juego o en pausa; en penaltis deshace el último lanzamiento.
          if (s.phase === 'playing' || s.phase === 'paused') send({ type: 'MINUS_ONE', team: annul });
          else if (s.phase === 'penalties') send({ type: 'UNDO_PENALTY' });
          return;
        }
        // Partido Loco: con el hándicap en pantalla, un pulsador (no un sensor de gol) sigue.
        if (s.phase === 'handicap' && s.handicap && source !== 'sensor') {
          const h = s.handicap;
          if (h.stage === 'announce' && Date.now() < introUntilRef.current) {
            reveal(Date.now());
          } else if (h.stage === 'announce' && h.spec.kind === 'penalty') {
            // Penalti: el botón del equipo que tira = gol; el del rival = fallo.
            if (team) send({ type: 'HANDICAP_PENALTY', scored: team === h.spec.team, source });
          } else if (team || command === 'SALTAR' || command === 'PAUSA') {
            send({ type: 'HANDICAP_GO' });
          }
          return;
        }
        if (s.phase === 'countdown' && (team || command === 'SALTAR')) {
          // La pulsación se consume como salto y no registra además un gol.
          send({ type: 'SKIP_COUNTDOWN' });
          return;
        }
        if (team && s.phase === 'penalties') {
          send({ type: 'PENALTY', team, scored: true, source });
          return;
        }
        if (team) {
          send({ type: 'GOAL', team, source });
          return;
        }
        if (command === 'PAUSA') {
          if (s.phase === 'playing') send({ type: 'PAUSE' });
          else if (s.phase === 'paused') send({ type: 'RESUME' });
        }
      }),
    [send, reveal],
  );

  return {
    state,
    now,
    send,
    lastGoal,
    goalLabel,
    annulled,
    banner,
    lockFlashAt,
    matchPoint: matchPointTeams(state),
    handicapIntroLeft: state.phase === 'handicap' && state.handicap?.stage === 'announce' ? Math.max(0, introUntil - now) : 0,
    skipHandicapIntro,
  };
}
