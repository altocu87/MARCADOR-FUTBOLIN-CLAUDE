/**
 * Tipos de torneo guardados desde el creador (no hay de fábrica: cada uno se crea a su gusto).
 * Un tipo guarda las reglas; los jugadores se eligen al crear cada torneo.
 */
import { DEFAULT_CONFIG, type MatchConfig } from '../../match-engine';
import type { TournamentTemplate } from '../persistence';
import { newId } from '../ids';
import { LADDER_DEFAULTS, LADDER_MAX_PAIRS, LADDER_MIN_PAIRS, ladderShape } from './ladder';
import { DEFAULT_POINTS, FORMAT_LABEL, MAX_TEAMS, MIN_TEAMS, formTeams, rulesShort, sameRules, type MatchRules, type TournamentDraft } from './tournaments';

/** Punto de partida de un tipo nuevo en el creador: 1 contra 1, a 5 goles, sin final. */
export const TEMPLATE_DEFAULTS: Omit<TournamentTemplate, 'id' | 'name' | 'format'> = {
  teamSize: 1,
  pairing: 'elo',
  seeding: 'elo',
  gamesPerPlayer: 4,
  final: 'none',
  finalBestOf: 1,
  ranked: false,
  endCondition: 'goals',
  goalsPerPeriod: 5,
  minutesPerPeriod: 5,
  finalGoals: null,
};

/** Los tipos de torneo guardados. Los de fábrica que hubo se retiraron: ya no se ofrecen. */
export function allTemplates(saved: TournamentTemplate[]): TournamentTemplate[] {
  return saved.filter((t) => !t.builtIn);
}

/** Tipo nuevo: copia de otro o, sin él, los valores por defecto del formato dado. */
export function newTemplate(from?: TournamentTemplate, format: TournamentTemplate['format'] = 'league'): TournamentTemplate {
  if (from) return { ...from, id: newId('tpl'), name: `${from.name} (copia)`, builtIn: false };
  return { ...TEMPLATE_DEFAULTS, id: newId('tpl'), name: '', format, teamSize: format === 'pool' ? 2 : 1, ...(format === 'pool' ? { pairing: 'random' as const } : {}) };
}

/** Liga: ¿se puede empatar? Solo a tiempo (o «ambas»); a goles siempre hay ganador. */
export function templateAllowsDraw(t: TournamentTemplate): boolean {
  return t.format === 'league' && t.endCondition !== 'goals' && t.allowDraw !== false;
}

/** Puntos de la liga (los del tipo o los de por defecto). */
export function templatePoints(t: TournamentTemplate): NonNullable<TournamentTemplate['points']> {
  return t.points ?? DEFAULT_POINTS;
}

const FINAL_LABEL: Record<TournamentTemplate['final'], string> = {
  none: 'sin final',
  top2: 'final 1º vs 2º',
  top4: 'final 1º+4º vs 2º+3º',
};

/** Ajustes del Pool de un tipo de torneo (con los de por defecto donde no diga nada). */
export function poolSettings(t: TournamentTemplate): { swap: boolean; limit: 'time' | 'rounds'; minutes: number; rounds: number } {
  return {
    swap: t.poolSwap ?? LADDER_DEFAULTS.swap,
    limit: t.poolLimit ?? LADDER_DEFAULTS.limit,
    minutes: t.poolMinutes ?? LADDER_DEFAULTS.minutes,
    rounds: t.poolRounds ?? LADDER_DEFAULTS.rounds,
  };
}

/** «30 min» / «5 rondas». */
export function poolLimitText(t: TournamentTemplate): string {
  const p = poolSettings(t);
  return p.limit === 'time' ? `${p.minutes} min` : `${p.rounds} rondas`;
}

/** Resumen en una línea: «Pool · con cambio de pareja · 30 min · final en la pista 1 · a 5 goles». */
export function describeTemplate(t: TournamentTemplate): string {
  const parts: string[] = [FORMAT_LABEL[t.format]];
  if (t.format === 'pool') parts.push(poolSettings(t).swap ? 'con cambio de pareja' : 'sin cambio de pareja', poolLimitText(t));
  else parts.push(t.teamSize === 1 ? '1 vs 1' : `parejas ${t.pairing === 'elo' ? 'equilibradas' : t.pairing === 'manual' ? 'elegidas' : 'al azar'}`);
  if (t.format === 'league') {
    if ((t.legs ?? 1) === 2) parts.push('ida y vuelta');
    const p = templatePoints(t);
    parts.push(templateAllowsDraw(t) ? `${p.win}-${p.draw}-${p.loss} pts` : `${p.win}-${p.loss} pts`);
  }
  const bo = t.finalBestOf > 1 ? ' al mejor de 3' : '';
  if (t.format === 'bracket') {
    if (bo) parts.push(`final${bo}`);
  } else if (t.format === 'pool') parts.push(`final en la pista 1${bo}`);
  else parts.push(FINAL_LABEL[t.final] + (t.final !== 'none' ? bo : ''));
  parts.push(rulesShort(t) + (hasTemplateFinal(t) && !sameRules(t, finalRulesOf(t)) ? ` (final ${rulesShort(finalRulesOf(t))})` : ''));
  if (t.ranked) parts.push('cuenta para ELO');
  return parts.join(' · ');
}

/** Opciones de final que tienen sentido para cada formato. */
export function finalOptions(format: TournamentTemplate['format']): TournamentTemplate['final'][] {
  // La copa y el Pool siempre acaban en final (la copa con los dos que llegan; el Pool, en la pista 1).
  if (format === 'bracket' || format === 'pool') return [];
  return ['none', 'top2'];
}

/** ¿Este tipo de torneo acaba en final? (copa y Pool siempre; la liga si la tiene). */
export function hasTemplateFinal(t: TournamentTemplate): boolean {
  return t.format === 'bracket' || t.format === 'pool' || (t.final !== 'none' && finalOptions(t.format).includes(t.final));
}

/** Reglas de la final: las suyas propias o, si no tiene, las del resto. */
export function finalRulesOf(t: TournamentTemplate): MatchRules {
  return {
    endCondition: t.finalEndCondition ?? t.endCondition,
    goalsPerPeriod: t.finalGoals ?? t.goalsPerPeriod,
    minutesPerPeriod: t.finalMinutes ?? t.minutesPerPeriod,
  };
}

/** El tipo de torneo con la final jugada con otras reglas. */
export function withFinalRules(t: TournamentTemplate, r: MatchRules): TournamentTemplate {
  return { ...t, finalEndCondition: r.endCondition, finalGoals: r.goalsPerPeriod, finalMinutes: r.minutesPerPeriod };
}

export function templateMatchConfig(t: TournamentTemplate, defaults: Partial<MatchConfig> = {}): MatchConfig {
  return {
    ...DEFAULT_CONFIG,
    ...defaults,
    endCondition: t.endCondition,
    goalsPerPeriod: t.goalsPerPeriod,
    minutesPerPeriod: t.minutesPerPeriod,
    ...(templateAllowsDraw(t) ? { allowDraw: true } : {}),
  };
}

/** Convierte un predefinido + jugadores elegidos en un borrador de torneo. */
export function draftFromTemplate(
  t: TournamentTemplate,
  name: string,
  selected: string[],
  eloOf: (id: string) => number,
  defaults: Partial<MatchConfig> = {},
  rnd: () => number = Math.random,
  /** Parejas que ya son un equipo guardado: juegan juntas (solo con parejas fijas). */
  fixedPairs: string[][] = [],
): { draft: TournamentDraft; leftover: string[] } {
  const config = templateMatchConfig(t, defaults);
  const hasFinal = hasTemplateFinal(t);
  const fr = finalRulesOf(t);
  // La final nunca empata: si tiene reglas propias, sin `allowDraw` (y si no, fixtureConfig se lo quita).
  const finalConfig = hasFinal && !sameRules(t, fr) ? { ...config, ...fr, allowDraw: undefined } : undefined;
  const common = {
    name,
    format: t.format,
    ranked: t.ranked,
    config,
    seeding: t.seeding,
    final: t.format === 'bracket' || !finalOptions(t.format).includes(t.final) ? ('none' as const) : t.final,
    finalBestOf: hasFinal ? t.finalBestOf : 1,
    finalConfig,
    templateName: t.name,
    templateId: t.id,
    ...(t.lastTitle?.trim() ? { lastTitle: t.lastTitle.trim() } : {}),
    ...(t.format === 'league' ? { legs: t.legs ?? 1, points: { ...templatePoints(t) } } : {}),
  };
  // El Pool se juega por parejas (las de salida, al azar o elegidas), como las parejas fijas.
  const size = t.format === 'pool' ? 2 : t.teamSize;
  const pairing = t.format === 'pool' && t.pairing === 'elo' ? 'random' : t.pairing;
  const ladder = t.format === 'pool' ? { ladder: poolSettings(t) } : {};
  // Los equipos guardados (o las parejas elegidas a mano) se respetan; el resto se empareja como diga
  // el tipo de torneo. Con parejas elegidas por los jugadores no se forma ninguna sola: lo que quede sin
  // pareja vuelve en `leftover`.
  const fixed = size === 2 ? keepPairs(fixedPairs, selected) : [];
  const rest = selected.filter((id) => !fixed.some((p) => p.includes(id)));
  const formed = size === 2 && pairing === 'manual' ? { teams: [], leftover: rest } : formTeams(rest, size, pairing === 'elo' ? 'elo' : 'random', eloOf, rnd);
  return { draft: { ...common, teamSize: size, ...ladder, teams: [...fixed.map((p) => ({ playerIds: [...p] })), ...formed.teams] }, leftover: formed.leftover };
}

export interface TemplateFit {
  ok: boolean;
  /** Por qué no se puede jugar con este número de jugadores. */
  reason?: string;
  /** Lo que saldría: «5 partidos · cada uno juega 4 y descansa 1». */
  summary?: string;
}

/** ¿Se puede jugar este tipo de torneo con `n` jugadores? Y, si se puede, cómo saldría. */
export function templateFit(t: TournamentTemplate, n: number): TemplateFit {
  const finalExtra = t.format !== 'bracket' && t.final !== 'none' ? ' + final' + (t.finalBestOf > 1 ? ' al mejor de 3' : '') : '';
  if (t.format === 'pool') {
    if (n % 2 === 1) return { ok: false, reason: 'Necesita número par de jugadores' };
    if (n < LADDER_MIN_PAIRS * 2) return { ok: false, reason: `Mínimo ${LADDER_MIN_PAIRS * 2} jugadores` };
    if (n > LADDER_MAX_PAIRS * 2) return { ok: false, reason: `Máximo ${LADDER_MAX_PAIRS * 2} jugadores` };
    const shape = ladderShape(n / 2);
    const courts = `${shape.courts} pista${shape.courts > 1 ? 's' : ''}${shape.bench ? ' + banquillo' : ''}`;
    return { ok: true, summary: `${n / 2} parejas · ${courts} · ${poolLimitText(t)} · final en la pista 1${t.finalBestOf > 1 ? ' al mejor de 3' : ''}` };
  }
  const size = t.teamSize;
  const unit = size === 1 ? 'jugadores' : 'parejas';
  if (size === 2 && n % 2 === 1) return { ok: false, reason: 'Necesita número par de jugadores' };
  const teams = Math.floor(n / size);
  if (teams < MIN_TEAMS) return { ok: false, reason: `Mínimo ${MIN_TEAMS * size} jugadores${size === 2 ? ` (${MIN_TEAMS} parejas)` : ''}` };
  if (teams > MAX_TEAMS) return { ok: false, reason: `Máximo ${MAX_TEAMS} ${unit}` };
  if (t.format === 'league') {
    const legs = t.legs ?? 1;
    return {
      ok: true,
      summary: `${((teams * (teams - 1)) / 2) * legs} partidos${finalExtra} · ${teams} ${unit}, todos contra todos${legs === 2 ? ' (ida y vuelta)' : ''}`,
    };
  }
  const byes = (teams <= 4 ? 4 : 8) - teams;
  const path = teams <= 4 ? 'semifinales y final' : 'cuartos, semis y final';
  return {
    ok: true,
    summary: `${teams} ${unit} · ${path}${t.finalBestOf > 1 ? ' al mejor de 3' : ''}${byes ? ` · ${byes} pase${byes > 1 ? 's' : ''} directo${byes > 1 ? 's' : ''}` : ''}`,
  };
}

/** Parejas cuyos dos jugadores están apuntados, sin repetir jugador. */
function keepPairs(pairs: string[][], selected: string[]): string[][] {
  const used = new Set<string>();
  const out: string[][] = [];
  for (const p of pairs) {
    if (p.length !== 2 || !p.every((id) => selected.includes(id) && !used.has(id))) continue;
    p.forEach((id) => used.add(id));
    out.push(p);
  }
  return out;
}
