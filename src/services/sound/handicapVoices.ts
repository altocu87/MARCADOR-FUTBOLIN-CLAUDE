/**
 * Qué voces grabadas suenan en el Partido Loco, al elegir jugadores y con el pitido: lo mismo que la pantalla
 * (hardware/esp32s3_pantalla7/s3_voces_hc.h y s3_pitido.h; guion en docs/GUION_VOCES_HANDICAPS.md).
 * Aquí solo se decide qué códigos y en qué orden; si falta una voz, se devuelve [] y la app usa la voz del navegador.
 */
import { soundCode } from '../../inputs/hardware/boardSounds';
import type { Handicap, MatchState, Team } from '../../match-engine';
import { PITIDO_ID } from './cloudSounds';

export type Has = (code: string) => boolean;

/** Código de una voz de los hándicaps: FNV-1a de «handicap/<voz>». */
export const hcCode = (voice: string) => soundCode(`handicap/${voice}`);
export const pitidoCode = () => soundCode(PITIDO_ID);

/** «Álvaro» → «N_alvaro» (minúsculas, sin tildes ni eñes, espacios como «_»), como `nameVoice` de la placa. */
export function nameVoice(name: string): string {
  const base = name
    .replace(/^ +/, '')
    .normalize('NFD')
    .replace(/[̀-ͯ]/g, '')
    .toLowerCase()
    .replace(/ /g, '_')
    .replace(/[^a-z0-9_]/g, '')
    .replace(/_+$/, '');
  return `N_${base}`;
}

/** Afectado por el hándicap: su nombre ('' si se juega sin jugadores) y si es chico ('m') o chica ('f'). */
export interface Who {
  name: string;
  gender?: 'm' | 'f';
}

const pick = (voices: string[], has: Has, rnd: number): string | null => {
  const ok = voices.map(hcCode).filter(has).slice(0, 4);
  return ok.length ? ok[Math.floor(rnd * ok.length) % ok.length] : null;
};

/** «¡Atentos! Hándicap en 3 segundos» (una de tres). */
export const avisoVoice = (has: Has, rnd = Math.random()) => pick(['A1', 'A2', 'A3'], has, rnd);

/** «¡Tres!», «¡Dos!», «¡Uno!». */
export function countVoice(n: number, has: Has): string | null {
  if (n < 1 || n > 3) return null;
  const c = hcCode(`C${n}`);
  return has(c) ? c : null;
}

/** «¡Todo vuelve a la normalidad!» (o el final del cambio de posiciones o del traspaso); los que se gastan, nada. */
export function endingVoice(h: Handicap, has: Has, rnd = Math.random()): string | null {
  const v = (p: string) => ['a', 'b', 'c', 'd'].map((x) => p + x);
  if (h.kind === 'swap_positions') return pick(v('R4'), has, rnd);
  if (h.kind === 'transfer') return pick(v('R5'), has, rnd);
  return h.durationMs ? pick(v('R1'), has, rnd) : null;
}

const TEAM_INDEX: Record<Team | 'both', number> = { white: 0, blue: 1, both: 2 };

export function teamVoice(team: number, has: Has): string | null {
  const c = hcCode(team === 0 ? 'Q1' : team === 1 ? 'Q2' : 'Q3');
  return has(c) ? c : null;
}

/** Elegir jugadores: «¡Equipo blanco!»/«¡Equipo azul!» al tocarle a un equipo. */
export const turnVoice = (team: Team, has: Has) => teamVoice(TEAM_INDEX[team], has);

/** Elegir jugadores: su nombre; si no está grabado, nada (el equipo ya se ha oído). */
export function nameOnly(name: string | undefined, has: Has): string | null {
  if (!name?.trim()) return null;
  const c = hcCode(nameVoice(name));
  return has(c) ? c : null;
}

/** Su nombre o, si no está grabado, su equipo. */
function personVoice(w: Who | undefined, team: number, has: Has): string | null {
  if (w?.name) {
    const c = hcCode(nameVoice(w.name));
    if (has(c)) return c;
  }
  return teamVoice(team, has);
}

const BAR_LETTER = { portero: 0, defensa: 1, medio: 2, delantero: 3 } as const;

/**
 * Al enseñar el hándicap: quién + la frase («¡Vicky!» + «y» + «¡Alex!» + «¡Congelados! 15 segundos sin moveros.»).
 * `white`/`blue`: el afectado de cada equipo (si lo hay).
 */
export function revealVoices(h: Handicap, white: Who | undefined, blue: Who | undefined, has: Has): string[] {
  const team = TEAM_INDEX[h.team ?? 'both'];
  const both = !!white && !!blue;
  let phrase: string | null = null;
  switch (h.kind) {
    case 'double_all': phrase = 'F01'; break;
    case 'double_team': phrase = 'F02'; break;
    case 'triple_next': phrase = 'F03'; break;
    case 'steal': phrase = 'F04'; break;
    case 'freeze_score': phrase = 'F05'; break;
    case 'swap_positions': phrase = 'F06'; break;
    case 'frozen_player': {
      const girl = both ? white!.gender === 'f' && blue!.gender === 'f' : (white ?? blue)?.gender === 'f';
      phrase = both ? (girl ? 'F07d' : 'F07c') : girl ? 'F07b' : 'F07a';
      break;
    }
    case 'bar_lock':
      phrase = `F08${String.fromCharCode((both ? 101 : 97) + BAR_LETTER[h.bar ?? 'portero'])}`;
      break;
    case 'penalty': phrase = team === 1 ? 'F09b' : 'F09a'; break;
    case 'no_spin': phrase = 'F11'; break;
    case 'long_shots': phrase = 'F12'; break;
    case 'one_hand': phrase = 'F13'; break;
    case 'transfer': {
      const a = hcCode('F10a');
      const b = hcCode('F10b');
      if (!has(a) || !has(b)) return [];
      return [personVoice(white, 0, has), a, personVoice(blue, 1, has), b].filter((x): x is string => !!x);
    }
    default:
      return []; // «Mano mala» ya no sale
  }
  const p = hcCode(phrase);
  if (!has(p)) return [];
  const out: (string | null)[] = [];
  if (h.kind === 'double_team' || h.kind === 'steal' || h.kind === 'swap_positions') out.push(teamVoice(team, has));
  else if (h.kind === 'frozen_player' || h.kind === 'bar_lock') {
    if (both) {
      out.push(personVoice(white, 0, has));
      if (has(hcCode('Q4'))) out.push(hcCode('Q4'));
      out.push(personVoice(blue, 1, has));
    } else out.push(white ? personVoice(white, 0, has) : personVoice(blue, 1, has));
  }
  out.push(p);
  return out.filter((x): x is string => !!x).slice(0, 4);
}

/** El afectado de cada equipo (por los participantes del partido). */
export function affected(h: Handicap, state: Pick<MatchState, 'participants'>, genderOf: (playerId: string) => 'm' | 'f' | undefined) {
  const out: { white?: Who; blue?: Who } = {};
  for (const id of h.players ?? []) {
    const p = state.participants.find((x) => x.playerId === id);
    if (!p || out[p.team]) continue;
    out[p.team] = { name: p.nameSnapshot, gender: genderOf(id) };
  }
  return out;
}

/** Todas las voces cortas que conviene tener preparadas (las de arriba, más los nombres que se pasen). */
export function shortVoiceCodes(names: string[] = []): string[] {
  const fixed = ['A1', 'A2', 'A3', 'C1', 'C2', 'C3', 'Q1', 'Q2', 'Q3', 'Q4', 'F01', 'F02', 'F03', 'F04', 'F05', 'F06',
    'F07a', 'F07b', 'F07c', 'F07d', 'F08a', 'F08b', 'F08c', 'F08d', 'F08e', 'F08f', 'F08g', 'F08h', 'F09a', 'F09b',
    'F10a', 'F10b', 'F11', 'F12', 'F13', 'R1a', 'R1b', 'R1c', 'R1d', 'R4a', 'R4b', 'R4c', 'R4d', 'R5a', 'R5b', 'R5c', 'R5d'];
  return [...fixed.map(hcCode), ...names.filter(Boolean).map((n) => hcCode(nameVoice(n))), pitidoCode()];
}

/** El pitido: al acabar la 1.ª parte, la 2.ª, la prórroga y el partido; no al acabar los penaltis (como s3_pitido.h). */
export function pitidoDue(prev: Pick<MatchState, 'phase'>, next: Pick<MatchState, 'phase' | 'period'>): boolean {
  if (next.phase === prev.phase) return false;
  if (next.phase === 'periodEnd') return true;
  return next.phase === 'finished' && next.period !== 'shootout';
}
