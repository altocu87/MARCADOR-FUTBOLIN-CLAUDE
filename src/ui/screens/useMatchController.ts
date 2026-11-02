/**
 * Controlador del partido en la interfaz: mantiene el estado del motor, avanza
 * el reloj con tiempo real, traduce entradas a comandos y dispara sonido, voz y
 * efectos SOLO tras la validación del motor. Guarda snapshots de recuperación.
 */
import { useCallback, useEffect, useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import { cloudVoices } from '../../services/sound/cloudSounds';
import { AmbienceClock, CommentaryDeck, P, commentMoment, previewDue, shootoutBeforeMask } from '../../services/sound/commentary';
import { affected, avisoVoice, endingVoice, pitidoCode, pitidoDue, revealVoices } from '../../services/sound/handicapVoices';
import { makeSnapshot } from '../../app/recovery';
import type { MatchExtras } from '../../app/routes';
import { hardwareHub } from '../../inputs/hardware/hub';
import { annulTeamFromCommand, teamFromCommand, inputBus } from '../../inputs/inputBus';
import {
  advance,
  COUNTDOWN_MS,
  countdownRemaining,
  createMatch,
  dispatch,
  firstHandicapAtMs,
  getClock,
  getScore,
  goalLockRemaining,
  validGoalsFromEvents,
  goalMoment,
  handicapEndText,
  handicapsEnabled,
  handicapText,
  nextHandicapAtMs,
  pickHandicap,
  pickVisitor,
  scheduleVisitAtMs,
  visitorText,
  VISIT_TIMING,
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
  type Visitor,
  isSinglePeriod,
} from '../../match-engine';
import { newId } from '../../services/ids';
import { sound } from '../../services/sound/sound';
import { soundsForCommit } from '../../services/sound/soundEvents';
import { soundPlayer } from '../../services/sound/soundPlayer';
import { voice } from '../../services/sound/voice';

const SNAPSHOT_EVERY_MS = 5000;

/** Solo en desarrollo: el animal pedido, si puede salir ahora (si no, el que toque). */
function forcedVisitor(s: MatchState, animal: Visitor['animal'], id: string, t: number): Visitor | null {
  for (let i = 0; i < 40; i += 1) {
    const v = pickVisitor(s, id, t);
    if (v?.animal === animal) return v;
  }
  return pickVisitor(s, id, t);
}

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

/** Cartel de la fase de un torneo antes de la cuenta atrás. */
export const PHASE_POSTER_MS = 3500;

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

/**
 * ¿Esta pulsación elige goleador? Sí si acaba de marcar ese equipo (seguimos en el bloqueo de 3 s del
 * gol, con el partido en juego) y tiene más de un jugador. Devuelve el gol y el siguiente goleador de la
 * vuelta: ninguno → 1.º → 2.º → … → ninguno.
 */
export function scorerPickTarget(s: MatchState, team: Team, now: number): { goalId: string; next: string | undefined } | null {
  if (s.phase !== 'playing' || goalLockRemaining(s, now) <= 0) return null;
  const goal = validGoalsFromEvents(s.events).at(-1);
  if (!goal || goal.team !== team) return null;
  const ids = s.participants.filter((p) => p.team === team).sort((a, b) => a.slot - b.slot).map((p) => p.playerId);
  if (ids.length < 2) return null;
  const cycle: (string | undefined)[] = [...ids, undefined];
  const current = s.scorers?.[goal.id];
  const i = current === undefined ? cycle.length - 1 : cycle.indexOf(current);
  return { goalId: goal.id, next: cycle[(i + 1) % cycle.length] };
}

export function useMatchController(
  config: MatchConfig,
  participants: ParticipantRef[],
  resume?: MatchState,
  extras?: MatchExtras,
): MatchController {
  const { repos, players } = useApp();
  const playersRef = useRef(players);
  playersRef.current = players;
  const [state, setState] = useState<MatchState>(
    // En un torneo, antes del 3·2·1 se ve el cartel de la fase (semifinal, final…).
    () => resume ?? createMatch(newId('m'), config, participants, Date.now(), extras?.tournament ? PHASE_POSTER_MS : 0),
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
  const visitTimers = useRef<number[]>([]);
  // Solo en desarrollo: forzar un animal desde la consola (window.marcadorLoco.animal('squirrel')).
  const forcedVisit = useRef<Visitor['animal'] | null>(null);
  useEffect(() => {
    if (!import.meta.env.DEV) return;
    (window as unknown as { marcadorLoco: unknown }).marcadorLoco = {
      animal: (a: Visitor['animal']) => {
        forcedVisit.current = a;
      },
    };
  }, []);
  const stateRef = useRef(state);
  // Comentarios del locutor (commentary.ts): la baraja de los que hay en el aparato, el reloj del ambiente y si lo
  // que suena es un comentario de ambiente o la intro (lo corta cualquier cosa que pase).
  const deckRef = useRef<{ version: number; deck: CommentaryDeck | null }>({ version: -1, deck: null });
  const ambienceRef = useRef<AmbienceClock | null>(null);
  ambienceRef.current ??= (() => {
    const a = new AmbienceClock();
    a.reset();
    return a;
  })();
  const ambientOn = useRef(false);
  const introOn = useRef(false);
  const quietSince = useRef(0);
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
      visitTimers.current.forEach((id) => window.clearTimeout(id));
    },
    [],
  );

  /** La baraja de comentarios (null si en este aparato no hay ninguno o no suena nada). */
  const deck = useCallback((): CommentaryDeck | null => {
    if (!sound.audible) return null;
    if (deckRef.current.version !== cloudVoices.version) {
      const d = new CommentaryDeck((c) => cloudVoices.has(c));
      deckRef.current = { version: cloudVoices.version, deck: d.empty ? null : d };
    }
    return deckRef.current.deck;
  }, []);

  // Intro del locutor durante la primera cuenta atrás (sigue sonando al empezar a jugar; un gol la corta).
  const introTried = useRef(false);
  useEffect(() => {
    const s = stateRef.current;
    if (introTried.current || resume || s.phase !== 'countdown' || s.events.some((e) => e.type === 'GOAL')) return;
    introTried.current = true;
    const code = soundPlayer.isDefault('matchStart') ? deck()?.pick(new Set([P.Intro])) : null;
    if (code && cloudVoices.say([code])) introOn.current = true;
    // Solo al abrir el partido (una vez, aunque React monte la pantalla dos veces en desarrollo).
  }, []);

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
      // Con las voces grabadas (nube): quién + la frase, como la pantalla; si no, la voz del navegador.
      const who = affected(cur.spec, stateRef.current, (id) => playersRef.current.find((p) => p.id === id)?.gender);
      const has = (c: string) => cloudVoices.has(c);
      if (!cloudVoices.say(revealVoices(cur.spec, who.white, who.blue, has))) voice.say(`${t.title.replace(/[¡!]/g, '')}. ${t.detail}`);
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
      // Pitido del árbitro (si está en este aparato): primero el pitido y después lo demás.
      const whistleMs = pitidoDue(prev, next) && cloudVoices.playNow(pitidoCode()) ? cloudVoices.durationMs(pitidoCode()) : 0;
      // Bola de partido: aviso al aparecer.
      const mp = matchPointTeams(next);
      const key = mp.length ? `${next.period}:${getScore(next).white}-${getScore(next).blue}` : '';
      const matchPointNew = !!key && key !== matchPointKey.current && next.period !== 'overtime';
      matchPointKey.current = key;
      const soundIds = soundsForCommit(prev, next, events, matchPointNew);
      // Comentario del locutor (como la placa): anulado, penalti, gol o el de la fase. Si un momento tiene un sonido
      // elegido a mano, no habla. Si habla, sustituye a los sonidos y a la voz del navegador de este cambio.
      const scoreMoved = getScore(prev).white !== getScore(next).white || getScore(prev).blue !== getScore(next).blue;
      const happened = scoreMoved || prev.phase !== next.phase || events.some((e) => e.type === 'GOAL' || e.type === 'PENALTY' || e.type === 'HANDICAP_PENALTY');
      if (scoreMoved) ambienceRef.current?.score(getClock(next, t).totalElapsedMs);
      if (ambientOn.current && happened) {
        sound.stopClip();
        ambientOn.current = false;
      }
      const d = deck();
      let spoken = false;
      if (d && soundIds.every((id) => id === 'matchPoint' || soundPlayer.isDefault(id))) {
        const moment = commentMoment(prev, next, events);
        const code = moment
          ? moment.kind === 'annulled'
            ? d.pick(new Set([P.Anulado]))
            : d.pick(moment.mask, moment.kind === 'goal' ? P.Comodin : moment.generic)
          : null;
        if (code) {
          if (moment?.kind === 'annulled' || moment?.kind === 'goal' || moment?.kind === 'penalty') introOn.current = false;
          spoken = cloudVoices.say([code], { queue: whistleMs > 0 });
        }
        // La intro sigue al empezar a jugar: no suena encima el «empieza el partido» de siempre.
        if (!spoken && introOn.current && soundIds.includes('matchStart')) spoken = true;
        // Tanda: después de cada tiro (y al empezarla), lo que se juega el siguiente.
        if (previewDue(next, events)) {
          const pre = d.pick(shootoutBeforeMask(next));
          if (pre) spoken = cloudVoices.say([pre], { queue: spoken || whistleMs > 0 }) || spoken;
        }
      }
      const say = (text: string) => {
        if (!spoken) voice.say(text);
      };
      const sayAfterWhistle = (text: string) => (whistleMs ? window.setTimeout(() => say(text), whistleMs) : say(text));
      // Gol anulado: el marcador de un equipo baja en la misma parte (−1 o deshacer un gol).
      // Un gol «robo» o la ardilla del Partido Loco también bajan al rival, pero no es una anulación.
      if (prev.period === next.period && !events.some((e) => (e.type === 'GOAL' && e.steal) || e.type === 'VISIT')) {
        const before = getScore(prev);
        const after = getScore(next);
        const down = (['white', 'blue'] as Team[]).find((tm) => after[tm] < before[tm]);
        if (down) {
          setLastGoal(null);
          setAnnulled({ id: newId('a'), team: down });
        }
      }
      for (const e of events) {
        if (e.type === 'GOAL' && e.team) {
          setLastGoal(e);
          const moment = goalMoment(next, e);
          setGoalLabel(bonusLabel(e) ?? (moment ? MOMENT_LABEL[moment] : null));
          if (next.phase !== 'finished') {
            const sc = e.scoreAfter;
            say(`¡Gol del ${TEAM_VOICE[e.team]}! ${moment ? MOMENT_LABEL[moment].replace(/[¡!]/g, '') + '. ' : ''}${sc.white} a ${sc.blue}.`);
          }
        } else if (e.type === 'MATCH_END' && e.team) {
          sayAfterWhistle(`¡Final del partido! Gana el ${TEAM_VOICE[e.team]}.`);
        } else if (e.type === 'PERIOD_END' && next.phase !== 'finished') {
          sayAfterWhistle(`Final de la ${PERIOD_VOICE[e.period]}.`);
        } else if (e.type === 'PERIOD_START') {
          // El del arranque del partido es un momento con sonido propio (soundsForCommit).
          if (e.period !== 'first') sound.play('countdownGo');
          showBanner(
            e.period === 'overtime'
              ? { text: 'PRÓRROGA', sub: 'GOL DE ORO', tone: 'gold' }
              : { text: e.period === 'first' && isSinglePeriod(next.config) ? 'PARTIDO' : PERIOD_BANNER[e.period], sub: '¡A JUGAR!', tone: next.config.mode === 'chaos' ? 'chaos' : 'accent' },
            1500,
          );
        } else if (e.type === 'HANDICAP_START' && e.handicap) {
          // Partido Loco: cuenta atrás y, al terminar, el hándicap a pantalla completa.
          // Con las voces grabadas: el final del que sustituye y «¡Atentos! Hándicap en 3 segundos»; la cuenta
          // atrás empieza al acabar el aviso (HandicapCountdown dice «¡Tres!», «¡Dos!», «¡Uno!»).
          const replaced = next.handicap?.replaced;
          const has = (c: string) => cloudVoices.has(c);
          const end = replaced ? endingVoice(replaced, has) : null;
          const aviso = avisoVoice(has);
          if (replaced && !end) voice.say(handicapEndText(replaced));
          if (end || aviso) cloudVoices.say([end, aviso].filter((x): x is string => !!x));
          else sound.play('countdown');
          const avisoMs = (end ? cloudVoices.durationMs(end) : 0) + (aviso ? cloudVoices.durationMs(aviso) : 0);
          reveal(t + avisoMs + HANDICAP_INTRO_MS);
        } else if (e.type === 'VISIT' && e.visitor) {
          sound.play(e.visitor.animal === 'squirrel' ? 'error' : 'whoosh');
          voice.say(visitorText(e.visitor).detail, { interrupt: false });
        } else if (e.type === 'HANDICAP_END' && e.handicap && e.reason === 'time') {
          sound.play('countdownGo');
          const end = endingVoice(e.handicap, (c) => cloudVoices.has(c));
          if (!end || !cloudVoices.say([end], { queue: true })) voice.say('¡Vuelta a la normalidad!');
        } else if (e.type === 'HANDICAP_END' && e.handicap && e.reason === 'used') {
          showBanner({ text: 'FIN DEL HÁNDICAP', sub: handicapText(e.handicap, next.participants).short, tone: 'chaos' }, 1500);
        } else if (e.type === 'HANDICAP_PENALTY') {
          if (!e.scored) say('¡Falla el penalti!');
        } else if (e.type === 'SHOOTOUT_START') {
          showBanner({ text: 'PENALTIS', sub: 'TANDA', tone: 'gold' });
          say('Tanda de penaltis.');
        } else if (e.type === 'PENALTY') {
          say(e.scored ? '¡Gol!' : '¡Falla!');
          if (!isSuddenDeath(prev) && isSuddenDeath(next) && next.phase === 'penalties') {
            showBanner({ text: 'MUERTE SÚBITA', tone: 'danger' });
          }
        }
      }
      if (matchPointNew && !spoken) voice.say('¡Bola de partido!', { interrupt: false });
      // Sonidos de los momentos (el de la mesa o el de siempre): el primero ya, los demás en cola. Si ha hablado el
      // locutor, no (como en la placa).
      if (!spoken) soundIds.forEach((id, i) => soundPlayer.play(id, { queue: i > 0 || whistleMs > 0 }));
      hardwareHub.notifyMatch(next, events);
      saveSnapshot(next, t);
    },
    [saveSnapshot, showBanner, reveal, deck],
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
      // Ambiente del locutor: con el partido en juego y sin sonar nada desde hace 250 ms.
      {
        const s = stateRef.current;
        const busy = sound.busy || (typeof window !== 'undefined' && !!window.speechSynthesis?.speaking);
        if (busy || s.phase !== 'playing') quietSince.current = 0;
        else if (!quietSince.current) quietSince.current = t;
        else if (t - quietSince.current >= 250) {
          ambientOn.current = false;
          introOn.current = false;
          const d = deck();
          const pool = d ? ambienceRef.current!.due(getClock(s, t).totalElapsedMs, s.period === 'first', true) : null;
          const code = pool === null || (pool === P.Opening20 && !soundPlayer.isDefault('matchStart')) ? null : d!.pick(new Set([pool]));
          if (code && cloudVoices.say([code])) {
            ambientOn.current = true;
            quietSince.current = 0;
          }
        }
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
          const handicap = pickHandicap(s, newId('h'));
          const nextAtMs = nextHandicapAtMs(total, Math.random);
          const visitAtMs = scheduleVisitAtMs(s, total, handicap.durationMs, nextAtMs, Math.random);
          const out = dispatch(s, { type: 'HANDICAP_START', handicap, nextAtMs, visitAtMs }, t);
          if (out.accepted) commit(out.state, out.events, t);
        } else if ((s.nextVisitAtMs !== undefined && total >= s.nextVisitAtMs) || forcedVisit.current) {
          // Un animal sale del agujero de gusano entre dos hándicaps.
          const forced = forcedVisit.current;
          forcedVisit.current = null;
          const visitor = forced ? forcedVisitor(s, forced, newId('v'), t) : pickVisitor(s, newId('v'), t);
          if (visitor) {
            const out = dispatch(s, { type: 'VISIT_START', visitor }, t);
            if (out.accepted) {
              commit(out.state, out.events, t);
              sound.play(visitor.animal === 'squirrel' ? 'siren' : 'whoosh');
              voice.say(visitor.animal === 'squirrel' ? '¡Robo en marcha! ¡Cuidado con la ardilla!' : visitorText(visitor).title.replace(/[¡!]/g, ''));
              const timing = VISIT_TIMING[visitor.animal];
              const step = (cmd: 'VISIT_APPLY' | 'VISIT_END') => {
                const now2 = Date.now();
                const o = dispatch(stateRef.current, { type: cmd }, now2);
                if (o.accepted) commit(o.state, o.events, now2);
              };
              visitTimers.current = [
                window.setTimeout(() => step('VISIT_APPLY'), timing.apply),
                window.setTimeout(() => step('VISIT_END'), timing.end),
              ];
            }
          }
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
          // La pulsación se consume como salto y no registra además un gol. Con el cartel de la
          // fase en pantalla, primero salta el cartel (luego viene el 3·2·1).
          send({ type: countdownRemaining(s, Date.now()) > COUNTDOWN_MS ? 'SKIP_INTRO' : 'SKIP_COUNTDOWN' });
          return;
        }
        if (team && s.phase === 'penalties') {
          send({ type: 'PENALTY', team, scored: true, source });
          return;
        }
        if (team) {
          // Tras un gol de su equipo, mientras sale la celebración (bloqueo de 3 s), cada pulsación más
          // elige al goleador: el 1.º jugador, el 2.º… y vuelta a ninguno. Se ve en la pantalla.
          const pick = scorerPickTarget(s, team, Date.now());
          if (pick && source !== 'sensor') {
            send({ type: 'ASSIGN_SCORER', eventId: pick.goalId, scorer: pick.next });
            sound.play('ui');
            return;
          }
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
