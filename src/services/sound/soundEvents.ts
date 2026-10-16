/**
 * Momentos del partido que pueden llevar un sonido propio de la mesa y cómo se detectan.
 * Solo interpreta lo que el motor ya aceptó: no cambia reglas.
 */
import {
  getScore,
  goalStreak,
  goalValue,
  otherTeam,
  validGoals,
  type MatchEvent,
  type MatchState,
  type Team,
} from '../../match-engine';

export type SoundEventId =
  | 'goal'
  | 'goalAnnulled'
  | 'streak3'
  | 'comeback'
  | 'equalizer'
  | 'matchPoint'
  | 'goldenGoal'
  | 'penaltyScored'
  | 'penaltyMissed'
  | 'matchStart'
  | 'periodEnd'
  | 'victory'
  | 'rout'
  | 'tournamentWin'
  | 'levelUp'
  | 'challenge';

export interface SoundEventDef {
  id: SoundEventId;
  label: string;
  /** Explicación detrás de la «?» (solo si el nombre no se entiende solo). */
  help?: string;
}

export const SOUND_EVENTS: SoundEventDef[] = [
  { id: 'goal', label: 'Gol' },
  { id: 'goalAnnulled', label: 'Gol anulado' },
  { id: 'streak3', label: '3 seguidos', help: 'El mismo equipo marca 3 goles seguidos sin que el rival responda. Suena en el tercero.' },
  { id: 'comeback', label: 'Remontada', help: 'Un equipo que iba perdiendo por 2 goles o más se pone por delante.' },
  { id: 'equalizer', label: 'Empate', help: 'El gol que iguala el marcador (el 0-0 del principio no cuenta).' },
  { id: 'matchPoint', label: 'Bola de partido', help: 'Un equipo gana si marca el siguiente gol. Suena justo después del gol.' },
  { id: 'goldenGoal', label: 'Gol de oro', help: 'El gol que gana el partido en la prórroga.' },
  { id: 'penaltyScored', label: 'Penalti marcado' },
  { id: 'penaltyMissed', label: 'Penalti fallado' },
  { id: 'matchStart', label: 'Empieza el partido' },
  { id: 'periodEnd', label: 'Fin de la parte' },
  { id: 'victory', label: 'Victoria' },
  { id: 'rout', label: 'Goleada', help: 'Ganar dejando al rival a 0, o ganar por 4 goles o más. Suena en lugar de «Victoria».' },
  { id: 'tournamentWin', label: 'Ganar un torneo' },
  { id: 'levelUp', label: 'Subir de nivel' },
  { id: 'challenge', label: 'Reto conseguido' },
];

export const SOUND_EVENT_IDS = SOUND_EVENTS.map((e) => e.id);

/** «default»: el de siempre de la app; «none»: nada; «custom»: uno de `ids` al azar. */
export interface SoundChoice {
  kind: 'default' | 'none' | 'custom';
  /** Sonidos propios elegidos. Se conservan aunque se vuelva a «el de siempre» (botón «Propio» del panel rápido). */
  ids: string[];
}

export type SoundChoices = Partial<Record<SoundEventId, SoundChoice>>;

export const DEFAULT_CHOICE: SoundChoice = { kind: 'default', ids: [] };

export function choiceFor(choices: SoundChoices | undefined, id: SoundEventId): SoundChoice {
  return choices?.[id] ?? DEFAULT_CHOICE;
}

/** Limpia lo guardado: solo momentos conocidos y listas de texto. */
export function normalizeSoundChoices(raw: unknown): SoundChoices {
  const out: SoundChoices = {};
  if (!raw || typeof raw !== 'object') return out;
  for (const id of SOUND_EVENT_IDS) {
    const c = (raw as Record<string, unknown>)[id] as Partial<SoundChoice> | undefined;
    if (!c || typeof c !== 'object') continue;
    const kind = c.kind === 'none' || c.kind === 'custom' ? c.kind : 'default';
    const ids = Array.isArray(c.ids) ? [...new Set(c.ids.filter((x): x is string => typeof x === 'string'))] : [];
    out[id] = { kind: kind === 'custom' && !ids.length ? 'default' : kind, ids };
  }
  return out;
}

/** Quita un sonido borrado de todos los momentos (si un momento se queda sin ninguno, vuelve a «el de siempre»). */
export function withoutSound(choices: SoundChoices, soundId: string): SoundChoices {
  const out: SoundChoices = {};
  for (const [k, c] of Object.entries(choices) as [SoundEventId, SoundChoice][]) {
    const ids = c.ids.filter((x) => x !== soundId);
    out[k] = { kind: c.kind === 'custom' && !ids.length ? 'default' : c.kind, ids };
  }
  return out;
}

/** Goleada: ganar dejando al rival a 0 o por 4 o más (en el marcador ordinario; los penaltis no cuentan). */
export function isRout(state: MatchState, winner: Team): boolean {
  const s = getScore(state);
  const w = s[winner];
  const l = s[otherTeam(winner)];
  return w > l && (l === 0 || w - l >= 4);
}

/** Remontada: este gol pone por delante a un equipo que llegó a ir perdiendo por 2 o más. */
export function isComebackGoal(state: MatchState, goal: MatchEvent): boolean {
  if (!goal.team) return false;
  const team = goal.team;
  const opp = otherTeam(team);
  const after = goal.scoreAfter;
  const before = { ...after, [team]: after[team] - goalValue(goal) };
  if (before[team] > before[opp] || after[team] <= after[opp]) return false;
  let worst = 0;
  for (const g of validGoals(state)) {
    if (g.seq >= goal.seq) break;
    worst = Math.max(worst, g.scoreAfter[opp] - g.scoreAfter[team]);
  }
  return worst >= 2;
}

/** ¿Un equipo ha bajado en el marcador sin que sea un robo ni la travesura de un animal? */
function annulled(prev: MatchState, next: MatchState, events: MatchEvent[]): boolean {
  if (prev.period !== next.period) return false;
  if (events.some((e) => (e.type === 'GOAL' && e.steal) || e.type === 'VISIT')) return false;
  const before = getScore(prev);
  const after = getScore(next);
  return after.white < before.white || after.blue < before.blue;
}

/** Sonido de un gol que no termina el partido: el momento más especial gana. */
function goalSound(state: MatchState, goal: MatchEvent): SoundEventId {
  if (goal.bonus?.includes('penalty')) return 'penaltyScored';
  if (isComebackGoal(state, goal)) return 'comeback';
  const streak = goalStreak(state);
  if (streak && streak.team === goal.team && streak.count === 3) return 'streak3';
  const s = goal.scoreAfter;
  if (s.white === s.blue && s.white > 0) return 'equalizer';
  return 'goal';
}

/**
 * Sonidos de un cambio aceptado por el motor, en orden: el primero suena ya y los siguientes, en cola, al
 * terminar el anterior. Si el partido termina, suena «Victoria» o «Goleada» en lugar del gol (salvo el gol de oro,
 * que suena antes). `matchPointNew`: acaba de aparecer la bola de partido.
 */
export function soundsForCommit(prev: MatchState, next: MatchState, events: MatchEvent[], matchPointNew = false): SoundEventId[] {
  const out: SoundEventId[] = [];
  if (annulled(prev, next, events)) out.push('goalAnnulled');
  const end = events.find((e) => e.type === 'MATCH_END');
  const goals = events.filter((e) => e.type === 'GOAL' && e.team);
  const lastGoal = goals[goals.length - 1];
  for (const e of events) {
    if (e.type === 'PERIOD_START' && e.period === 'first') out.push('matchStart');
    else if (e.type === 'PERIOD_END' && !end) out.push('periodEnd');
    else if (e.type === 'PENALTY') out.push(e.scored ? 'penaltyScored' : 'penaltyMissed');
    else if (e.type === 'HANDICAP_PENALTY' && !e.scored) out.push('penaltyMissed');
  }
  if (lastGoal) {
    if (!end) out.push(goalSound(next, lastGoal));
    else if (lastGoal.period === 'overtime') out.push('goldenGoal');
  }
  if (end?.team) out.push(isRout(next, end.team) ? 'rout' : 'victory');
  if (matchPointNew && !end) out.push('matchPoint');
  return out;
}
