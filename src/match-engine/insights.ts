/**
 * Lecturas del estado para avisos de interfaz (bola de partido, rachas, remontadas).
 * No cambian reglas: solo interpretan el estado que ya validó el motor.
 */
import { getScore, goalTarget, goalValue, otherTeam, validGoals } from './engine';
import type { MatchEvent, MatchState, Score, Team } from './types';

/** Equipos que ganarían el partido marcando el siguiente gol. */
export function matchPointTeams(state: MatchState): Team[] {
  if (state.phase !== 'playing' && state.phase !== 'paused') return [];
  if (state.period === 'overtime') return ['white', 'blue'];
  if (state.config.endCondition === 'time') return [];
  // A un gol del objetivo de goles para ganar.
  const score = getScore(state);
  return (['white', 'blue'] as Team[]).filter((t) => score[t] + 1 >= goalTarget(state));
}

/** Equipo que encadena goles sin respuesta y cuántos (racha del partido). */
export function goalStreak(state: MatchState): { team: Team; count: number } | null {
  const goals = validGoals(state);
  const last = goals[goals.length - 1];
  if (!last?.team) return null;
  let count = 0;
  for (let i = goals.length - 1; i >= 0 && goals[i].team === last.team; i -= 1) count += 1;
  return { team: last.team, count };
}

export type GoalMoment = 'equalizer' | 'lead' | 'comeback' | 'double' | null;

/** Interpreta un gol recién aceptado para la celebración. */
export function goalMoment(state: MatchState, goal: MatchEvent): GoalMoment {
  if (!goal.team) return null;
  const team = goal.team;
  const opp = otherTeam(team);
  const after = goal.scoreAfter;
  const before: Score = { ...after, [team]: after[team] - goalValue(goal) };
  // Peor desventaja previa de este equipo en el partido.
  let worst = 0;
  for (const g of validGoals(state)) {
    if (g.seq >= goal.seq) break;
    worst = Math.max(worst, g.scoreAfter[opp] - g.scoreAfter[team]);
  }
  if (worst >= 2 && after[team] >= after[opp]) return 'comeback';
  if (after[team] === after[opp]) return 'equalizer';
  if (before[team] <= before[opp] && after[team] > after[opp]) return 'lead';
  if (goalValue(goal) > 1) return 'double';
  return null;
}
