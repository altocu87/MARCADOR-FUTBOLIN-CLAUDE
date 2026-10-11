import { DEFAULT_CONFIG, type MatchEvent, type MatchMode, type Team } from '../src/match-engine';
import { STORAGE_FORMAT_VERSION, type StoredMatch, type TournamentTemplate } from '../src/services/persistence';
import { TEMPLATE_DEFAULTS } from '../src/services/tournaments';

let counter = 0;

/** Construye un partido guardado sintético. `goals`: 'W' gol Blanco, 'B' gol Azul, en orden. */
export function makeMatch(opts: {
  white: string[];
  blue: string[];
  goals: string;
  mode?: MatchMode;
  at?: number;
  reason?: 'regulation' | 'golden_goal' | 'penalties';
  penaltyWinner?: Team;
  totalTimeMs?: number;
}): StoredMatch {
  counter += 1;
  const id = `m${counter}`;
  const at = opts.at ?? counter * 1000;
  const score = { white: 0, blue: 0 };
  const events: MatchEvent[] = [];
  let seq = 0;
  for (const ch of opts.goals) {
    const team: Team = ch === 'W' ? 'white' : 'blue';
    score[team] += 1;
    seq += 1;
    events.push({
      id: `e${seq}`,
      seq,
      type: 'GOAL',
      team,
      period: 'first',
      periodTimeMs: seq * 10_000,
      totalTimeMs: seq * 10_000,
      wallTime: at + seq,
      scoreAfter: { ...score },
    });
  }
  const winner: Team =
    opts.penaltyWinner ?? (score.white > score.blue ? 'white' : 'blue');
  return {
    formatVersion: STORAGE_FORMAT_VERSION,
    id,
    engineVersion: 'test',
    rulesVersion: 'test',
    config: { ...DEFAULT_CONFIG, mode: opts.mode ?? 'ranked', testMode: false },
    participants: [
      ...opts.white.map((pid, i) => ({ playerId: pid, team: 'white' as const, slot: (i + 1) as 1 | 2, nameSnapshot: pid.toUpperCase() })),
      ...opts.blue.map((pid, i) => ({ playerId: pid, team: 'blue' as const, slot: (i + 1) as 1 | 2, nameSnapshot: pid.toUpperCase() })),
    ],
    createdAt: at - 100,
    startedAt: at - 90,
    finishedAt: at,
    result: {
      winner,
      reason: opts.reason ?? (opts.penaltyWinner ? 'penalties' : 'regulation'),
      score: { ...score },
      penaltyScore: opts.penaltyWinner ? { white: winner === 'white' ? 3 : 2, blue: winner === 'blue' ? 3 : 2 } : undefined,
      totalTimeMs: opts.totalTimeMs ?? 300_000,
    },
    periods: [],
    events,
    penalties: [],
  };
}

/** Tipos de torneo de ejemplo para las pruebas (la app ya no trae ninguno de fábrica). */
const tplBase = { ...TEMPLATE_DEFAULTS };
export const SAMPLE_TEMPLATES: TournamentTemplate[] = [
  { ...tplBase, id: 'sample-pool', name: 'Copa Rotativa', format: 'pool', teamSize: 2, final: 'top4', finalBestOf: 3 },
  { ...tplBase, id: 'sample-league', name: 'Liguilla Rápida', format: 'league', goalsPerPeriod: 3 },
  { ...tplBase, id: 'sample-league-final', name: 'Liga + Final', format: 'league', final: 'top2', finalBestOf: 3 },
  { ...tplBase, id: 'sample-bracket', name: 'Eliminatoria', format: 'bracket', finalBestOf: 3, finalGoals: 7 },
  { ...tplBase, id: 'sample-pairs', name: 'Copa Parejas', format: 'bracket', teamSize: 2, finalBestOf: 3, finalGoals: 7 },
  { ...tplBase, id: 'sample-pairs-league', name: 'Liga Parejas', format: 'league', teamSize: 2, final: 'top2', finalBestOf: 3 },
];
