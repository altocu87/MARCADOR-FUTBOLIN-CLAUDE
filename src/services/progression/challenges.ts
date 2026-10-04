/**
 * Retos diarios y semanales (propuesta «retos-1»). Se eligen de forma determinista
 * a partir de la fecha, así todos los dispositivos verían los mismos. El progreso se
 * calcula desde el historial; el premio se concede una vez por jugador y reto.
 */
import type { Team } from '../../match-engine';
import type { StoredMatch } from '../persistence';
import { dayKey, seededRandom, weekKey } from '../statistics/calendar';
import { comebackSize } from '../statistics/statistics';

export type ChallengeScope = 'daily' | 'weekly';

export interface ChallengeTemplate {
  id: string;
  /** Texto con {n}. */
  text: string;
  daily: number;
  weekly: number;
  /** Cuánto avanza este partido para el jugador (0 si nada). */
  progress(m: StoredMatch, team: Team): number;
}

const won = (m: StoredMatch, t: Team) => m.result.winner === t;
const opp = (t: Team): Team => (t === 'white' ? 'blue' : 'white');

export const CHALLENGE_TEMPLATES: ChallengeTemplate[] = [
  { id: 'play', text: 'Juega {n} partidos', daily: 3, weekly: 10, progress: () => 1 },
  { id: 'win', text: 'Gana {n} partidos', daily: 2, weekly: 6, progress: (m, t) => (won(m, t) ? 1 : 0) },
  { id: 'win_ranked', text: 'Gana {n} clasificatorios', daily: 1, weekly: 3, progress: (m, t) => (won(m, t) && m.config.mode === 'ranked' ? 1 : 0) },
  {
    id: 'clean_win',
    text: 'Gana {n} partidos encajando 3 goles o menos',
    daily: 1,
    weekly: 3,
    progress: (m, t) => (won(m, t) && m.result.score[opp(t)] <= 3 ? 1 : 0),
  },
  { id: 'team_goals', text: 'Tu equipo marca {n} goles', daily: 10, weekly: 40, progress: (m, t) => m.result.score[t] },
  { id: 'shutout', text: 'Gana {n} partido(s) sin encajar', daily: 1, weekly: 2, progress: (m, t) => (won(m, t) && m.result.score[opp(t)] === 0 ? 1 : 0) },
  { id: 'comeback', text: 'Remonta {n} partido(s) tras ir 2 abajo', daily: 1, weekly: 1, progress: (m, t) => (won(m, t) && comebackSize(m) >= 2 ? 1 : 0) },
  { id: 'big_win', text: 'Gana {n} partido(s) por 3 o más', daily: 1, weekly: 3, progress: (m, t) => (won(m, t) && m.result.score[t] - m.result.score[opp(t)] >= 3 ? 1 : 0) },
  { id: 'chaos', text: 'Juega {n} Partidos Locos', daily: 2, weekly: 5, progress: (m) => (m.config.mode === 'chaos' ? 1 : 0) },
  { id: 'duo', text: 'Gana {n} partidos de 2 contra 2', daily: 1, weekly: 4, progress: (m, t) => (won(m, t) && m.participants.filter((p) => p.team === 'white').length === 2 && m.participants.filter((p) => p.team === 'blue').length === 2 ? 1 : 0) },
];

export const CHALLENGE_XP: Record<ChallengeScope, number> = { daily: 40, weekly: 100 };

export interface ActiveChallenge {
  key: string; // p. ej. «daily:2026-10-02:win»
  scope: ChallengeScope;
  periodKey: string;
  template: ChallengeTemplate;
  target: number;
  text: string;
  xp: number;
}

function pick(seed: string, count: number): ChallengeTemplate[] {
  const rnd = seededRandom(seed);
  const pool = [...CHALLENGE_TEMPLATES];
  const out: ChallengeTemplate[] = [];
  while (out.length < count && pool.length) out.push(pool.splice(Math.floor(rnd() * pool.length), 1)[0]);
  return out;
}

/** Retos activos en el instante ts: 1 diario y 3 semanales. */
export function activeChallenges(ts: number): ActiveChallenge[] {
  const dk = dayKey(ts);
  const wk = weekKey(ts);
  const make = (scope: ChallengeScope, periodKey: string, tpl: ChallengeTemplate): ActiveChallenge => {
    const target = scope === 'daily' ? tpl.daily : tpl.weekly;
    return {
      key: `${scope}:${periodKey}:${tpl.id}`,
      scope,
      periodKey,
      template: tpl,
      target,
      text: tpl.text.replace('{n}', String(target)),
      xp: CHALLENGE_XP[scope],
    };
  };
  return [
    ...pick(`d${dk}`, 1).map((t) => make('daily', dk, t)),
    ...pick(`w${wk}`, 3).map((t) => make('weekly', wk, t)),
  ];
}

/** Progreso de un jugador en los retos activos en `now`. */
export function challengeProgress(playerId: string, matches: StoredMatch[], now: number) {
  return activeChallenges(now).map((c) => {
    let value = 0;
    for (const m of matches) {
      const key = c.scope === 'daily' ? dayKey(m.finishedAt) : weekKey(m.finishedAt);
      if (key !== c.periodKey) continue;
      const p = m.participants.find((x) => x.playerId === playerId);
      if (p) value += c.template.progress(m, p.team);
    }
    return { challenge: c, value: Math.min(value, c.target), done: value >= c.target };
  });
}
