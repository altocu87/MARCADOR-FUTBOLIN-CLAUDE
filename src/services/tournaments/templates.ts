/**
 * Predefinidos de torneo: los de fábrica y los guardados desde el creador.
 * Un predefinido guarda las reglas; los jugadores se eligen al crear cada torneo.
 */
import { DEFAULT_CONFIG, type MatchConfig } from '../../match-engine';
import type { TournamentTemplate } from '../persistence';
import { newId } from '../ids';
import { effectivePoolGames } from './pool';
import { FORMAT_LABEL, formTeams, type TournamentDraft } from './tournaments';

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
  { ...base, id: 'builtin-pool', name: 'Pool rotativo', format: 'pool', teamSize: 2, final: 'top4', finalBestOf: 3 },
  { ...base, id: 'builtin-league', name: 'Liguilla rápida', format: 'league', goalsPerPeriod: 3 },
  { ...base, id: 'builtin-league-final', name: 'Liga + Final', format: 'league', final: 'top2', finalBestOf: 3 },
  { ...base, id: 'builtin-bracket', name: 'Eliminatoria', format: 'bracket', finalBestOf: 3, finalGoals: 7 },
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
  const goals = t.endCondition === 'time' ? `${t.minutesPerPeriod} min` : `a ${t.goalsPerPeriod} goles`;
  parts.push(goals + (t.finalGoals && t.endCondition !== 'time' ? ` (final a ${t.finalGoals})` : ''));
  if (t.ranked) parts.push('cuenta para ELO');
  return parts.join(' · ');
}

/** Opciones de final que tienen sentido para cada formato. */
export function finalOptions(format: TournamentTemplate['format']): TournamentTemplate['final'][] {
  if (format === 'bracket') return [];
  return format === 'pool' ? ['none', 'top2', 'top4'] : ['none', 'top2'];
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
): { draft: TournamentDraft; leftover: string[] } {
  const config = templateMatchConfig(t, defaults);
  const hasFinal = t.format === 'bracket' || (t.final !== 'none' && finalOptions(t.format).includes(t.final));
  const finalConfig = hasFinal && t.finalGoals && t.endCondition !== 'time' ? { ...config, goalsPerPeriod: t.finalGoals } : undefined;
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
  const { teams, leftover } = formTeams(selected, t.teamSize, t.pairing, eloOf, rnd);
  return { draft: { ...common, teamSize: t.teamSize, teams }, leftover };
}
