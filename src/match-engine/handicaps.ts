/**
 * Partido Loco: catálogo de hándicaps, sorteo y textos.
 *
 * Cada 30–60 s de juego sale un hándicap (al menos uno por minuto y con 30 s de separación
 * mínima). El sorteo recibe el generador de azar para que las pruebas sean reproducibles; lo
 * que se sortea queda guardado en el evento HANDICAP_START, así el partido se puede repasar.
 */
import { getClock, getScore, goalTarget, otherTeam, periodTimeLimitMs } from './engine';
import type { Bar, Handicap, HandicapKind, MatchState, ParticipantRef, Team, Visitor } from './types';

export const HANDICAP_MIN_GAP_MS = 30_000;
export const HANDICAP_MAX_GAP_MS = 60_000;

/** Duración de cada hándicap en tiempo de juego (null = hasta el siguiente o hasta usarse). */
export const HANDICAP_DURATION: Record<HandicapKind, number | null> = {
  double_all: 30_000,
  double_team: 30_000,
  triple_next: null,
  steal: null,
  freeze_score: 15_000,
  swap_positions: null,
  frozen_player: 15_000,
  bar_lock: 15_000,
  penalty: 0,
  transfer: 30_000,
  weak_hand: 20_000,
  no_spin: 30_000,
  long_shots: 30_000,
  one_hand: 20_000,
};

export const HANDICAP_ICON: Record<HandicapKind, string> = {
  double_all: '✖2',
  double_team: '✖2',
  triple_next: '✖3',
  steal: '🦹',
  freeze_score: '🧊',
  swap_positions: '🔄',
  frozen_player: '🥶',
  bar_lock: '🔒',
  penalty: '🎯',
  transfer: '🔀',
  weak_hand: '🫲',
  no_spin: '🚫',
  long_shots: '🎯',
  one_hand: '☝',
};

const ALL_KINDS = Object.keys(HANDICAP_DURATION) as HandicapKind[];
const BARS: Bar[] = ['portero', 'defensa', 'medio', 'delantero'];
const BAR_TEXT: Record<Bar, string> = {
  portero: 'la barra del portero',
  defensa: 'la barra de la defensa',
  medio: 'la barra del medio campo',
  delantero: 'la barra de la delantera',
};
const TEAM_NAME: Record<Team, string> = { white: 'Blanco', blue: 'Azul' };

/** Primer hándicap del partido: entre 30 y 45 s de juego (estable para un mismo partido). */
export function firstHandicapAtMs(matchId: string): number {
  let h = 0;
  for (const c of matchId) h = (h * 31 + c.charCodeAt(0)) >>> 0;
  return HANDICAP_MIN_GAP_MS + (h % 15_000);
}

/** Siguiente hándicap: entre 30 y 60 s de juego después de este. */
export function nextHandicapAtMs(currentTotalMs: number, rnd: () => number): number {
  return currentTotalMs + HANDICAP_MIN_GAP_MS + Math.floor(rnd() * (HANDICAP_MAX_GAP_MS - HANDICAP_MIN_GAP_MS));
}

const pick = <T,>(list: T[], rnd: () => number): T => list[Math.floor(rnd() * list.length) % list.length];

/** Equipo que va perdiendo; con empate, uno al azar. */
function trailing(state: MatchState, rnd: () => number): Team {
  const s = getScore(state);
  if (s.white === s.blue) return rnd() < 0.5 ? 'white' : 'blue';
  return s.white < s.blue ? 'white' : 'blue';
}

/** Sortea el siguiente hándicap teniendo en cuenta marcador, jugadores y el anterior. */
export function pickHandicap(state: MatchState, id: string, rnd: () => number = Math.random): Handicap {
  const byTeam = (t: Team) => state.participants.filter((p) => p.team === t);
  const sizes = { white: byTeam('white').length, blue: byTeam('blue').length };
  const score = getScore(state);
  const tied = score.white === score.blue;
  const previous = [...state.events].reverse().find((e) => e.type === 'HANDICAP_START')?.handicap?.kind;

  const allowed = ALL_KINDS.filter((k) => {
    if (k === previous) return false;
    // Cambiar de sitio solo tiene sentido con 2 o más jugadores en el equipo.
    if (k === 'swap_positions') return sizes.white >= 2 || sizes.blue >= 2;
    if (k === 'transfer') return sizes.white >= 2 && sizes.blue >= 2;
    // El robo necesita un equipo por detrás y un rival con goles.
    if (k === 'steal') return !tied;
    return true;
  });
  const kind = pick(allowed, rnd);
  const base = { id, kind, durationMs: HANDICAP_DURATION[kind] };
  const onePerTeam = (teams: Team[]) => teams.map((t) => pick(byTeam(t), rnd).playerId);

  switch (kind) {
    case 'double_team':
    case 'steal':
      // Nunca al que va ganando: al que va perdiendo (o al azar si hay empate).
      return { ...base, team: trailing(state, rnd) };
    case 'penalty':
      return { ...base, team: rnd() < 0.5 ? 'white' : 'blue' };
    case 'swap_positions': {
      const eligible = (['white', 'blue'] as Team[]).filter((t) => sizes[t] >= 2);
      const team = eligible.length === 2 && rnd() < 0.4 ? 'both' : pick(eligible, rnd);
      return { ...base, team };
    }
    case 'frozen_player':
    case 'bar_lock': {
      // Un jugador de un equipo, o uno de cada equipo.
      const teams: Team[] = rnd() < 0.5 ? ['white', 'blue'] : [rnd() < 0.5 ? 'white' : 'blue'];
      return {
        ...base,
        team: teams.length === 2 ? 'both' : teams[0],
        players: onePerTeam(teams),
        ...(kind === 'bar_lock' ? { bar: pick(BARS, rnd) } : {}),
      };
    }
    case 'transfer':
      return { ...base, team: 'both', players: onePerTeam(['white', 'blue']) };
    default:
      return { ...base, team: 'both' };
  }
}

/** Duración legible: «30 s», «hasta el siguiente hándicap»… */
export function handicapDurationText(h: Handicap): string {
  if (h.kind === 'penalty') return 'Un lanzamiento';
  if (h.durationMs === null) return h.kind === 'swap_positions' ? 'Hasta el siguiente hándicap' : 'Hasta que se use';
  return `${Math.round(h.durationMs / 1000)} s`;
}

/** Título corto (pantalla completa y tarjeta del partido) y explicación con nombres. */
export function handicapText(h: Handicap, participants: ParticipantRef[]): { title: string; short: string; detail: string } {
  const name = (id: string) => participants.find((p) => p.playerId === id)?.nameSnapshot ?? '?';
  const players = (h.players ?? []).map(name);
  const both = players.length === 2;
  const secs = h.durationMs ? `${Math.round(h.durationMs / 1000)} segundos` : '';
  const team = h.team === 'white' || h.team === 'blue' ? h.team : undefined;
  switch (h.kind) {
    case 'double_all':
      return { title: '¡GOLES DOBLES!', short: 'GOLES x2', detail: `Todos los goles valen doble durante ${secs}.` };
    case 'double_team':
      return {
        title: `GOLES DOBLES PARA ${TEAM_NAME[team!].toUpperCase()}`,
        short: `x2 ${TEAM_NAME[team!].toUpperCase()}`,
        detail: `Los goles del equipo ${TEAM_NAME[team!].toLowerCase()} valen doble durante ${secs}.`,
      };
    case 'triple_next':
      return { title: '¡GOL TRIPLE!', short: 'GOL x3', detail: 'El próximo gol, lo marque quien lo marque, vale 3.' };
    case 'steal':
      return {
        title: '¡ROBO!',
        short: `ROBO ${TEAM_NAME[team!].toUpperCase()}`,
        detail: `El próximo gol del equipo ${TEAM_NAME[team!].toLowerCase()} además le quita un gol al equipo ${TEAM_NAME[otherTeam(team!)].toLowerCase()}.`,
      };
    case 'freeze_score':
      return { title: 'MARCADOR CONGELADO', short: 'CONGELADO', detail: `Durante ${secs} los goles no cuentan.` };
    case 'swap_positions':
      return {
        title: 'CAMBIO DE POSICIONES',
        short: 'CAMBIO DE SITIO',
        detail:
          h.team === 'both'
            ? 'En los dos equipos: el de portero y defensa pasa al ataque y el de ataque pasa atrás.'
            : `Equipo ${TEAM_NAME[team!].toLowerCase()}: el de portero y defensa pasa al ataque y el de ataque pasa atrás.`,
      };
    case 'frozen_player':
      return {
        title: both ? 'JUGADORES CONGELADOS' : 'JUGADOR CONGELADO',
        short: 'CONGELADO',
        detail: `${players.join(' y ')} no se ${both ? 'pueden' : 'puede'} mover durante ${secs}.`,
      };
    case 'bar_lock':
      return {
        title: 'BARRA BLOQUEADA',
        short: `SIN ${h.bar!.toUpperCase()}`,
        detail: `${players.join(' y ')} no ${both ? 'pueden' : 'puede'} mover ${BAR_TEXT[h.bar!]} durante ${secs}.`,
      };
    case 'penalty':
      return {
        title: `¡PENALTI PARA ${TEAM_NAME[team!].toUpperCase()}!`,
        short: 'PENALTI',
        detail: `Penalti a favor del equipo ${TEAM_NAME[team!].toLowerCase()}. Un solo lanzamiento.`,
      };
    case 'transfer':
      return {
        title: '¡TRASPASO!',
        short: 'TRASPASO',
        detail: `${players[0]} se va al equipo azul y ${players[1]} al blanco durante ${secs}. El gol es para el equipo donde se juega.`,
      };
    case 'weak_hand':
      return { title: 'MANO MALA', short: 'MANO MALA', detail: `Todos juegan con la mano que no usan normalmente durante ${secs}.` };
    case 'no_spin':
      return {
        title: 'PROHIBIDO EL MOLINILLO',
        short: 'SIN MOLINILLO',
        detail: `Durante ${secs}, si alguien gira la barra entera, el gol no vale: anuladlo.`,
      };
    case 'long_shots':
      return {
        title: 'SOLO DESDE LEJOS',
        short: 'DESDE LEJOS',
        detail: `Durante ${secs} solo valen los goles marcados desde la defensa o el medio campo: anulad los demás.`,
      };
    case 'one_hand':
      return { title: 'UNA SOLA MANO', short: 'UNA MANO', detail: `Cada jugador juega con una sola mano durante ${secs}.` };
  }
}

/** Aviso al terminar un hándicap que cambiaba a la gente de sitio. */
export function handicapEndText(h: Handicap): string {
  if (h.kind === 'swap_positions') return 'Fin del cambio de posiciones: cada uno a su sitio.';
  if (h.kind === 'transfer') return 'Fin del traspaso: cada uno a su equipo.';
  const title = handicapText(h, []).title.replace(/[¡!]/g, '');
  return `Se acabó: ${title.charAt(0)}${title.slice(1).toLowerCase()}.`;
}

// ---------------------------------------------------------------------------
// Visitas de animales (agujero de gusano)
// ---------------------------------------------------------------------------

/** Como mucho dos visitas de animales por partido. */
export const MAX_VISITS = 2;
/** Probabilidad de que tras un hándicap se programe una visita. */
export const VISIT_CHANCE = 0.35;
const MINUTE = 60_000;

export const ANIMAL_NAME: Record<Visitor['animal'], string> = {
  squirrel: 'la ardilla ladrona',
  snail: 'el caracol',
  cat: 'el gato del futuro',
};

/**
 * Momento de la próxima visita: entre el final del hándicap recién anunciado y el siguiente,
 * dejando al menos 8 s a cada lado. Devuelve undefined si no cabe o si ya hubo dos.
 */
export function scheduleVisitAtMs(
  state: MatchState,
  handicapStartTotalMs: number,
  handicapDurationMs: number | null,
  nextHandicapAtMs: number,
  rnd: () => number,
): number | undefined {
  const visits = state.events.filter((e) => e.type === 'VISIT').length;
  if (visits >= MAX_VISITS || rnd() >= VISIT_CHANCE) return undefined;
  const from = handicapStartTotalMs + (handicapDurationMs ?? 0) + 8_000;
  const to = nextHandicapAtMs - 8_000;
  if (to <= from) return undefined;
  return from + Math.floor(rnd() * (to - from));
}

/** Elige el animal y su travesura sin decidir el partido; null si ninguno puede salir ahora. */
export function pickVisitor(state: MatchState, id: string, now: number, rnd: () => number = Math.random): Visitor | null {
  const score = getScore(state);
  const target = goalTarget(state);
  const timed = periodTimeLimitMs(state.config, state.period) !== null;
  const remaining = getClock(state, now).remainingMs;
  const options: Visitor[] = [];

  // Ardilla: roba a un equipo con goles (mejor al que va ganando), sin hacer ganar al otro.
  const victims = (['white', 'blue'] as Team[]).filter((t) => {
    if (score[t] === 0) return false;
    const thief = otherTeam(t);
    return timed && state.config.endCondition === 'time' ? true : score[thief] + 1 < target;
  });
  if (victims.length) {
    const leader = score.white === score.blue ? null : score.white > score.blue ? 'white' : 'blue';
    const from = leader && victims.includes(leader) && rnd() < 0.75 ? leader : victims[Math.floor(rnd() * victims.length)];
    options.push({ id, animal: 'squirrel', from });
  }
  // Caracol: alarga (tiempo o meta), como mucho dos veces.
  const snails = state.events.filter((e) => e.type === 'VISIT' && e.visitor?.animal === 'snail').length;
  if (snails < 2) options.push(timed ? { id, animal: 'snail', timeMs: MINUTE } : { id, animal: 'snail', goals: 1 });
  // Gato del futuro: acorta, pero nunca hasta decidir el partido.
  if (timed && remaining !== null && remaining > 90_000) options.push({ id, animal: 'cat', timeMs: -MINUTE });
  if (!timed && target - 1 > Math.max(score.white, score.blue) && target - 1 >= 2) options.push({ id, animal: 'cat', goals: -1 });

  if (!options.length) return null;
  return options[Math.floor(rnd() * options.length) % options.length];
}

/** Lo que dice la voz y el rótulo al hacer la travesura. */
export function visitorText(v: Visitor): { title: string; detail: string } {
  if (v.animal === 'squirrel') {
    const to = v.from === 'white' ? 'Azul' : 'Blanco';
    const from = v.from === 'white' ? 'Blanco' : 'Azul';
    return { title: '¡LA ARDILLA LADRONA!', detail: `Le roba un gol al equipo ${from.toLowerCase()} y se lo da al ${to.toLowerCase()}.` };
  }
  if (v.animal === 'snail') {
    return v.timeMs
      ? { title: '¡EL CARACOL!', detail: 'Atrasa el reloj: un minuto más de partido.' }
      : { title: '¡EL CARACOL!', detail: 'Sube la meta: ahora hace falta un gol más para ganar.' };
  }
  return v.timeMs
    ? { title: '¡EL GATO DEL FUTURO!', detail: 'Adelanta el reloj: un minuto menos de partido.' }
    : { title: '¡EL GATO DEL FUTURO!', detail: 'Baja la meta: ahora hace falta un gol menos para ganar.' };
}
