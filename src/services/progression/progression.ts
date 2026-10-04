/**
 * Servicio de progresión: recalcula ELO, XP, niveles, logros, retos y premios de torneo
 * reprocesando el historial en orden cronológico. No hay totales guardados sin origen:
 * todo se deriva de partidos válidos, por lo que reprocesar no duplica premios.
 */
import { validGoalsFromEvents, type Team } from '../../match-engine';
import type { ProgressionSettings, StoredMatch, Tournament } from '../persistence';
import { dayKey } from '../statistics/calendar';
import { personalGoalsInMatch } from '../statistics/extras';
import { comebackSize, sortMatches } from '../statistics/statistics';
import { ACHIEVEMENTS, RARITY_XP, achievementById } from './achievements';
import { activeChallenges } from './challenges';
import {
  ELO_MODES,
  PROGRESSION_RULES_VERSION,
  XP_TABLE,
  categoryFor,
  eloExpected,
  goalDiffMultiplier,
  levelForXp,
  type CategoryDef,
} from './rules';

export interface UnlockedAchievement {
  id: string;
  matchId: string;
  at: number;
}

export interface PlayerProgress {
  playerId: string;
  elo: number;
  maxElo: number;
  rankedPlayed: number;
  xp: number;
  level: number;
  category: CategoryDef;
  achievements: UnlockedAchievement[];
  eloHistory: { matchId: string; at: number; elo: number }[];
  /** Claves de retos completados (premio concedido). */
  challengesDone: string[];
  tournamentsWon: number;
}

export interface XpLine {
  label: string;
  xp: number;
}

export interface MatchProgressEntry {
  playerId: string;
  eloBefore?: number;
  eloAfter?: number;
  eloDelta?: number;
  k?: number;
  xpGained: number;
  /** XP total del jugador justo después de este partido (antes = xpAfter − xpGained). */
  xpAfter: number;
  xpBreakdown: XpLine[];
  levelBefore: number;
  levelAfter: number;
  categoryBefore: CategoryDef;
  categoryAfter: CategoryDef;
  unlocked: string[];
  challenges: string[];
}

export interface ProgressionSnapshot {
  rulesVersion: string;
  players: Map<string, PlayerProgress>;
  byMatch: Map<string, Map<string, MatchProgressEntry>>;
}

interface Counters {
  played: number;
  wins: number;
  winStreak: number;
  lossStreak: number;
  rankedWins: number;
  chaosWins: number;
  personalGoals: number;
  teammates: Set<string>;
  rivals: Set<string>;
  teammateWins: Map<string, number>;
  byDay: Map<string, number>;
  challengeProgress: Map<string, number>;
}

export interface ProgressionOptions {
  tournaments?: Tournament[];
  challenges?: boolean;
}

export function computeProgression(
  playerIds: string[],
  matches: StoredMatch[],
  settings: ProgressionSettings,
  options: ProgressionOptions = {},
): ProgressionSnapshot {
  const players = new Map<string, PlayerProgress>();
  const counters = new Map<string, Counters>();
  const byMatch = new Map<string, Map<string, MatchProgressEntry>>();

  // Partido que cierra cada torneo → jugadores ganadores.
  const tournamentWinners = new Map<string, string[]>();
  for (const t of options.tournaments ?? []) {
    if (t.status === 'finished' && t.finalMatchId && t.winnerTeamId) {
      tournamentWinners.set(t.finalMatchId, t.teams.find((x) => x.id === t.winnerTeamId)?.playerIds ?? []);
    }
  }

  const ensure = (id: string): PlayerProgress => {
    let p = players.get(id);
    if (!p) {
      p = {
        playerId: id,
        elo: settings.eloInitial,
        maxElo: settings.eloInitial,
        rankedPlayed: 0,
        xp: 0,
        level: 0,
        category: categoryFor(settings.eloInitial),
        achievements: [],
        eloHistory: [],
        challengesDone: [],
        tournamentsWon: 0,
      };
      players.set(id, p);
      counters.set(id, {
        played: 0,
        wins: 0,
        winStreak: 0,
        lossStreak: 0,
        rankedWins: 0,
        chaosWins: 0,
        personalGoals: 0,
        teammates: new Set(),
        rivals: new Set(),
        teammateWins: new Map(),
        byDay: new Map(),
        challengeProgress: new Map(),
      });
    }
    return p;
  };
  playerIds.forEach(ensure);

  for (const match of sortMatches(matches)) {
    const entries = new Map<string, MatchProgressEntry>();
    const isRanked = ELO_MODES.includes(match.config.mode);
    const winner = match.result.winner;
    const comeback = comebackSize(match);
    const goals = validGoalsFromEvents(match.events);
    const totalGoals = match.result.score.white + match.result.score.blue;
    const firstGoal = goals[0];
    const challenges = options.challenges === false ? [] : activeChallenges(match.finishedAt);

    // ELO: media de cada equipo calculada con los valores previos al partido.
    const teamAvg = (team: Team) => {
      const ids = match.participants.filter((p) => p.team === team).map((p) => p.playerId);
      return ids.reduce((sum, id) => sum + ensure(id).elo, 0) / ids.length;
    };
    const avg = { white: teamAvg('white'), blue: teamAvg('blue') };
    const diff = match.result.score.white - match.result.score.blue;
    // Partido decidido por penaltis: diferencia ordinaria 0 → multiplicador 1,00 (propuesta).
    const mult = settings.goalDiffMultiplier ? goalDiffMultiplier(diff) : 1;

    const eloChanges = new Map<string, { before: number; after: number; delta: number; k: number }>();
    if (isRanked) {
      for (const part of match.participants) {
        const p = ensure(part.playerId);
        const opp: Team = part.team === 'white' ? 'blue' : 'white';
        const expected = eloExpected(avg[part.team], avg[opp]);
        const s = part.team === winner ? 1 : 0;
        const k = p.rankedPlayed < settings.provisionalMatches ? settings.kProvisional : settings.kEstablished;
        const delta = Math.round(k * mult * (s - expected));
        eloChanges.set(part.playerId, { before: p.elo, after: p.elo + delta, delta, k });
      }
    }

    for (const part of match.participants) {
      const p = ensure(part.playerId);
      const c = counters.get(part.playerId)!;
      const won = part.team === winner;
      const opp: Team = part.team === 'white' ? 'blue' : 'white';
      const levelBefore = p.level;
      const categoryBefore = p.category;
      const lossStreakBefore = c.lossStreak;

      c.played += 1;
      if (won) {
        c.wins += 1;
        c.winStreak += 1;
        c.lossStreak = 0;
        if (match.config.mode === 'ranked') c.rankedWins += 1;
        if (match.config.mode === 'chaos') c.chaosWins += 1;
      } else {
        c.winStreak = 0;
        c.lossStreak += 1;
      }
      const day = dayKey(match.finishedAt);
      c.byDay.set(day, (c.byDay.get(day) ?? 0) + 1);
      let winsWithSameTeammate = 0;
      for (const other of match.participants) {
        if (other.playerId === part.playerId) continue;
        if (other.team === part.team) {
          c.teammates.add(other.playerId);
          if (won) c.teammateWins.set(other.playerId, (c.teammateWins.get(other.playerId) ?? 0) + 1);
          winsWithSameTeammate = Math.max(winsWithSameTeammate, c.teammateWins.get(other.playerId) ?? 0);
        } else c.rivals.add(other.playerId);
      }
      const pgMatch = personalGoalsInMatch(match, part.playerId);
      c.personalGoals += pgMatch;

      const change = eloChanges.get(part.playerId);
      if (change) {
        p.elo = change.after;
        p.maxElo = Math.max(p.maxElo, p.elo);
        p.rankedPlayed += 1;
        p.category = categoryFor(p.elo);
        p.eloHistory.push({ matchId: match.id, at: match.finishedAt, elo: p.elo });
      }

      const lines: XpLine[] = [{ label: 'Partido completado', xp: XP_TABLE.complete }];
      lines.push(won ? { label: 'Victoria', xp: XP_TABLE.win } : { label: 'Derrota', xp: XP_TABLE.loss });
      if (won && isRanked) lines.push({ label: 'Victoria clasificatoria', xp: XP_TABLE.rankedWinBonus });
      if (won && match.result.reason === 'golden_goal') lines.push({ label: 'Victoria en prórroga', xp: XP_TABLE.overtimeWin });
      if (won && match.result.reason === 'penalties') lines.push({ label: 'Victoria en penaltis', xp: XP_TABLE.penaltiesWin });
      if (tournamentWinners.get(match.id)?.includes(part.playerId)) {
        p.tournamentsWon += 1;
        lines.push({ label: 'Ganar torneo', xp: XP_TABLE.tournamentWin });
      }

      // Retos activos en la fecha del partido.
      const completed: string[] = [];
      for (const ch of challenges) {
        if (p.challengesDone.includes(ch.key)) continue;
        const inc = ch.template.progress(match, part.team);
        if (!inc) continue;
        const value = (c.challengeProgress.get(ch.key) ?? 0) + inc;
        c.challengeProgress.set(ch.key, value);
        if (value >= ch.target) {
          p.challengesDone.push(ch.key);
          completed.push(ch.key);
          lines.push({ label: `Reto: ${ch.text}`, xp: ch.xp });
        }
      }

      const ownGoals = goals.filter((g) => g.team === part.team);
      let run = 0;
      let ownGoalRun = 0;
      for (const g of goals) {
        run = g.team === part.team ? run + 1 : 0;
        ownGoalRun = Math.max(ownGoalRun, run);
      }
      const ownKicks = match.penalties.filter((k) => k.team === part.team);
      const sdKicks = match.penalties.filter((k) => k.suddenDeath).length;

      const baseXp = lines.reduce((s, l) => s + l.xp, 0);
      const got = new Set(p.achievements.map((a) => a.id));
      const unlocked: string[] = [];
      const ctx = {
        match,
        won,
        goalsFor: match.result.score[part.team],
        goalsAgainst: match.result.score[opp],
        played: c.played,
        wins: c.wins,
        winStreak: c.winStreak,
        lossStreakBefore,
        rankedPlayed: p.rankedPlayed,
        rankedWins: c.rankedWins,
        chaosWins: c.chaosWins,
        elo: p.elo,
        eloGapBefore: isRanked ? avg[opp] - avg[part.team] : 0,
        level: levelForXp(p.xp + baseXp),
        comeback,
        earliestOwnGoalMs: ownGoals.length ? Math.min(...ownGoals.map((g) => g.periodTimeMs)) : null,
        ownGoalRun,
        totalGoals,
        personalGoalsInMatch: pgMatch,
        personalGoalsTotal: c.personalGoals,
        firstGoalOfMatch: !!firstGoal && match.scorers?.[firstGoal.id] === part.playerId,
        penaltiesPerfect: ownKicks.length >= match.config.penaltyRounds && ownKicks.every((k) => k.scored),
        suddenDeathRounds: Math.floor(sdKicks / 2),
        hour: new Date(match.finishedAt).getHours(),
        matchesToday: c.byDay.get(day) ?? 0,
        distinctTeammates: c.teammates.size,
        distinctRivals: c.rivals.size,
        winsWithSameTeammate,
        tournamentsWon: p.tournamentsWon,
        challengesDone: p.challengesDone.length,
      };
      for (const a of ACHIEVEMENTS) {
        if (got.has(a.id)) continue;
        if (a.check(ctx)) {
          unlocked.push(a.id);
          p.achievements.push({ id: a.id, matchId: match.id, at: match.finishedAt });
          lines.push({ label: `Logro: ${a.name}`, xp: RARITY_XP[a.rarity] });
        }
      }

      const xpGained = lines.reduce((s, l) => s + l.xp, 0);
      p.xp += xpGained;
      p.level = levelForXp(p.xp);

      entries.set(part.playerId, {
        playerId: part.playerId,
        eloBefore: change?.before,
        eloAfter: change?.after,
        eloDelta: change?.delta,
        k: change?.k,
        xpGained,
        xpAfter: p.xp,
        xpBreakdown: lines,
        levelBefore,
        levelAfter: p.level,
        categoryBefore,
        categoryAfter: p.category,
        unlocked,
        challenges: completed,
      });
    }

    byMatch.set(match.id, entries);
  }

  return { rulesVersion: PROGRESSION_RULES_VERSION, players, byMatch };
}

/** Títulos disponibles para un jugador (de sus logros) y el que se muestra. */
export function availableTitles(progress: PlayerProgress | undefined): { id: string; title: string }[] {
  if (!progress) return [];
  return progress.achievements
    .map((a) => achievementById(a.id))
    .filter((a): a is NonNullable<typeof a> => !!a?.title)
    .map((a) => ({ id: a.id, title: a.title! }));
}

const RANK = { common: 0, rare: 1, epic: 2 } as const;

export function displayTitle(progress: PlayerProgress | undefined, chosenId?: string): string | null {
  const titles = availableTitles(progress);
  if (titles.length === 0) return null;
  if (chosenId === 'none') return null;
  const chosen = titles.find((t) => t.id === chosenId);
  if (chosen) return chosen.title;
  // Por defecto: el de mayor rareza, el más reciente.
  const best = [...titles].reverse().sort((a, b) => RANK[achievementById(b.id)!.rarity] - RANK[achievementById(a.id)!.rarity])[0];
  return best.title;
}
