/**
 * Predefinidos de torneo: los de fábrica y los guardados desde el creador.
 * Un predefinido guarda las reglas; los jugadores se eligen al crear cada torneo.
 */
import { DEFAULT_CONFIG, type MatchConfig } from '../../match-engine';
import type { TournamentTemplate } from '../persistence';
import { newId } from '../ids';
import { POOL_MAX_PLAYERS, POOL_MIN_PLAYERS, effectivePoolGames } from './pool';
import { FORMAT_LABEL, MAX_TEAMS, MIN_TEAMS, formTeams, rulesShort, sameRules, type MatchRules, type TournamentDraft } from './tournaments';

const base: Omit<TournamentTemplate, 'id' | 'name' | 'format'> = {
  builtIn: true,
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

export const BUILT_IN_TEMPLATES: TournamentTemplate[] = [
  { ...base, id: 'builtin-pool', name: 'Copa Rotativa', format: 'pool', teamSize: 2, final: 'top4', finalBestOf: 3 },
  { ...base, id: 'builtin-league', name: 'Liguilla Rápida', format: 'league', goalsPerPeriod: 3 },
  { ...base, id: 'builtin-league-final', name: 'Liga + Final', format: 'league', final: 'top2', finalBestOf: 3 },
  { ...base, id: 'builtin-bracket', name: 'Eliminatoria', format: 'bracket', finalBestOf: 3, finalGoals: 7 },
  { ...base, id: 'builtin-pairs', name: 'Copa Parejas', format: 'bracket', teamSize: 2, finalBestOf: 3, finalGoals: 7 },
  { ...base, id: 'builtin-pairs-league', name: 'Liga Parejas', format: 'league', teamSize: 2, final: 'top2', finalBestOf: 3 },
];

export function allTemplates(saved: TournamentTemplate[]): TournamentTemplate[] {
  return [...BUILT_IN_TEMPLATES, ...saved];
}

export function newTemplate(from?: TournamentTemplate): TournamentTemplate {
  const src = from ?? BUILT_IN_TEMPLATES[0];
  return { ...src, id: newId('tpl'), name: from ? `${from.name} (mío)` : 'Mi torneo', builtIn: false };
}

const FINAL_LABEL: Record<TournamentTemplate['final'], string> = {
  none: 'sin final',
  top2: 'final 1º vs 2º',
  top4: 'final 1º+4º vs 2º+3º',
};

/** Resumen en una línea: «Pool rotativo · 4 partidos por jugador · final 1º+4º vs 2º+3º al mejor de 3 · a 5 goles». */
export function describeTemplate(t: TournamentTemplate): string {
  const parts: string[] = [FORMAT_LABEL[t.format]];
  if (t.format === 'pool') parts.push(`${t.gamesPerPlayer} partidos por jugador`);
  else parts.push(t.teamSize === 1 ? '1 contra 1' : `parejas ${t.pairing === 'elo' ? 'equilibradas' : 'al azar'}`);
  const bo = t.finalBestOf > 1 ? ' al mejor de 3' : '';
  if (t.format === 'bracket') {
    if (bo) parts.push(`final${bo}`);
  } else parts.push(FINAL_LABEL[t.final] + (t.final !== 'none' ? bo : ''));
  parts.push(rulesShort(t) + (hasTemplateFinal(t) && !sameRules(t, finalRulesOf(t)) ? ` (final ${rulesShort(finalRulesOf(t))})` : ''));
  if (t.ranked) parts.push('cuenta para ELO');
  return parts.join(' · ');
}

/** Opciones de final que tienen sentido para cada formato. */
export function finalOptions(format: TournamentTemplate['format']): TournamentTemplate['final'][] {
  if (format === 'bracket') return [];
  return format === 'pool' ? ['none', 'top2', 'top4'] : ['none', 'top2'];
}

/** ¿Este tipo de torneo acaba en final? (el cuadro siempre; liguilla y Pool si la tienen). */
export function hasTemplateFinal(t: TournamentTemplate): boolean {
  return t.format === 'bracket' || (t.final !== 'none' && finalOptions(t.format).includes(t.final));
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
  const finalConfig = hasFinal && !sameRules(t, fr) ? { ...config, ...fr } : undefined;
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
  };
  if (t.format === 'pool') {
    return {
      draft: {
        ...common,
        teamSize: 2,
        teams: [],
        entrants: [...selected],
        gamesPerPlayer: effectivePoolGames(selected.length, t.gamesPerPlayer),
        pairing: t.pairing,
      },
      leftover: [],
    };
  }
  // Los equipos guardados se respetan; el resto se empareja como diga el tipo de torneo.
  const fixed = t.teamSize === 2 ? keepPairs(fixedPairs, selected) : [];
  const rest = selected.filter((id) => !fixed.some((p) => p.includes(id)));
  const formed = formTeams(rest, t.teamSize, t.pairing, eloOf, rnd);
  return { draft: { ...common, teamSize: t.teamSize, teams: [...fixed.map((p) => ({ playerIds: [...p] })), ...formed.teams] }, leftover: formed.leftover };
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
    if (n < POOL_MIN_PLAYERS) return { ok: false, reason: `Mínimo ${POOL_MIN_PLAYERS} jugadores` };
    if (n > POOL_MAX_PLAYERS) return { ok: false, reason: `Máximo ${POOL_MAX_PLAYERS} jugadores` };
    const g = effectivePoolGames(n, t.gamesPerPlayer);
    const rest = n % 4;
    const resting = rest === 0 ? 'sin descansos' : `descansa${rest > 1 ? 'n' : ''} ${rest} por ronda`;
    return { ok: true, summary: `${(n * g) / 4} partidos${finalExtra} · cada uno juega ${g} · ${resting}` };
  }
  const size = t.teamSize;
  const unit = size === 1 ? 'jugadores' : 'parejas';
  if (size === 2 && n % 2 === 1) return { ok: false, reason: 'Necesita número par de jugadores' };
  const teams = Math.floor(n / size);
  if (teams < MIN_TEAMS) return { ok: false, reason: `Mínimo ${MIN_TEAMS * size} jugadores${size === 2 ? ` (${MIN_TEAMS} parejas)` : ''}` };
  if (teams > MAX_TEAMS) return { ok: false, reason: `Máximo ${MAX_TEAMS} ${unit}` };
  if (t.format === 'league') {
    return { ok: true, summary: `${(teams * (teams - 1)) / 2} partidos${finalExtra} · ${teams} ${unit}, todos contra todos` };
  }
  const byes = (teams <= 4 ? 4 : 8) - teams;
  const path = teams <= 4 ? 'semifinales y final' : 'cuartos, semis y final';
  return {
    ok: true,
    summary: `${teams} ${unit} · ${path}${t.finalBestOf > 1 ? ' al mejor de 3' : ''}${byes ? ` · ${byes} pase${byes > 1 ? 's' : ''} directo${byes > 1 ? 's' : ''}` : ''}`,
  };
}

/**
 * Tipos de fábrica recomendados para `n` jugadores, del más al menos aconsejable. Pensado sobre todo
 * para grupos de 4 a 6: torneos de una tarde, con pocos partidos y sin nadie mucho rato parado.
 */
const RECOMMENDED: Record<number, string[]> = {
  3: ['builtin-league', 'builtin-league-final'],
  4: ['builtin-pool', 'builtin-league', 'builtin-league-final'],
  5: ['builtin-pool', 'builtin-league'],
  6: ['builtin-pool', 'builtin-pairs-league', 'builtin-bracket'],
  7: ['builtin-pool', 'builtin-bracket'],
  8: ['builtin-pairs', 'builtin-pool', 'builtin-pairs-league'],
};

export function recommendedTemplateIds(n: number): string[] {
  const ids = RECOMMENDED[n] ?? (n > 8 ? (n % 2 === 0 ? ['builtin-pool', 'builtin-pairs'] : ['builtin-pool']) : []);
  return ids.filter((id) => templateFit(BUILT_IN_TEMPLATES.find((x) => x.id === id)!, n).ok);
}

/** El más recomendado para `n` jugadores. */
export function recommendedTemplateId(n: number): string | null {
  return recommendedTemplateIds(n)[0] ?? null;
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
