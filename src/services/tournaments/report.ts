/**
 * Ediciones, ficha y palmarés de los torneos.
 * - Cada predefinido es una «competición»; cada torneo creado con él es una edición (1ª, 2ª…).
 *   Las canceladas no cuentan, así que la numeración solo avanza con torneos que llegan a jugarse.
 * - La ficha resume un torneo: fechas, partidos, goles, estadísticas por jugador, MVP y destacados.
 * - El palmarés de una competición lista sus ediciones con campeón y MVP, y quién suma más títulos.
 */
import type { Team } from '../../match-engine';
import type { Player, StoredMatch, Tournament } from '../persistence';
import { personalGoalsInMatch } from '../statistics/extras';
import { FORMAT_LABEL } from './tournaments';

/** Clave de la competición: el predefinido; los torneos antiguos, por su nombre. */
export function competitionKey(t: Tournament): string {
  return t.templateId ?? `name:${t.templateName ?? t.name}`;
}

export function competitionName(t: Tournament): string {
  return t.templateName ?? t.name;
}

/** Número de la siguiente edición de una competición. */
export function nextEdition(templateId: string, tournaments: Tournament[]): number {
  return tournaments.filter((t) => t.templateId === templateId && t.status !== 'cancelled').length + 1;
}

/** «Copa Rotativa · 2ª edición». */
export function editionName(competition: string, edition: number): string {
  return `${competition} · ${edition}ª edición`;
}

export interface PlayerLine {
  playerId: string;
  name: string;
  played: number;
  wins: number;
  losses: number;
  /** Goles de su equipo a favor y en contra. */
  goalsFor: number;
  goalsAgainst: number;
  /** Goles marcados por él (solo si se apuntaron goleadores o jugó solo). */
  scored: number;
  winPct: number;
  champion: boolean;
  /** Puntuación MVP (ver `mvpScore`). */
  mvp: number;
}

export interface MatchHighlight {
  match: StoredMatch;
  label: string;
}

export interface TournamentReport {
  matches: StoredMatch[];
  startedAt?: number;
  finishedAt?: number;
  totalGoals: number;
  avgGoals: number;
  playTimeMs: number;
  /** Partidos decididos en prórroga o penaltis. */
  extraTime: number;
  players: PlayerLine[];
  mvp?: PlayerLine;
  /** Máximo goleador (solo si hay goles con autor). */
  topScorer?: PlayerLine;
  biggestWin?: MatchHighlight;
  longest?: MatchHighlight;
  highestScoring?: MatchHighlight;
  championIds: string[];
  /** Marcador de la final (o de la serie). */
  finalScore?: string;
}

/**
 * MVP: 3 por victoria + 1 por gol marcado + la mitad de su diferencia de goles + 2 si es campeón.
 * Premia ganar, marcar y dominar; el campeón tiene ventaja pero no lo gana por defecto.
 */
export function mvpScore(l: Pick<PlayerLine, 'wins' | 'scored' | 'goalsFor' | 'goalsAgainst' | 'champion'>): number {
  return l.wins * 3 + l.scored + (l.goalsFor - l.goalsAgainst) / 2 + (l.champion ? 2 : 0);
}

const scoreText = (m: StoredMatch) => `${m.result.score.white}–${m.result.score.blue}`;
const sideNames = (m: StoredMatch, team: Team) =>
  m.participants
    .filter((p) => p.team === team)
    .map((p) => p.nameSnapshot)
    .join(' + ');
const matchLabel = (m: StoredMatch) => `${sideNames(m, 'white')} ${scoreText(m)} ${sideNames(m, 'blue')}`;

export function tournamentReport(t: Tournament, allMatches: StoredMatch[], players: Player[]): TournamentReport {
  const ms = allMatches.filter((m) => m.tournament?.id === t.id).sort((a, b) => a.finishedAt - b.finishedAt);
  const championIds = t.teams.find((x) => x.id === t.winnerTeamId)?.playerIds ?? [];
  const nameOf = (id: string, fallback: string) => players.find((p) => p.id === id)?.name ?? fallback;
  const lines = new Map<string, Omit<PlayerLine, 'mvp' | 'winPct'>>();
  let totalGoals = 0;
  let playTimeMs = 0;
  let extraTime = 0;
  for (const m of ms) {
    totalGoals += m.result.score.white + m.result.score.blue;
    playTimeMs += m.result.totalTimeMs;
    if (m.result.reason !== 'regulation') extraTime += 1;
    for (const p of m.participants) {
      const opp: Team = p.team === 'white' ? 'blue' : 'white';
      const l = lines.get(p.playerId) ?? {
        playerId: p.playerId,
        name: nameOf(p.playerId, p.nameSnapshot),
        played: 0,
        wins: 0,
        losses: 0,
        goalsFor: 0,
        goalsAgainst: 0,
        scored: 0,
        champion: championIds.includes(p.playerId),
      };
      l.played += 1;
      if (m.result.winner === p.team) l.wins += 1;
      else l.losses += 1;
      l.goalsFor += m.result.score[p.team];
      l.goalsAgainst += m.result.score[opp];
      l.scored += personalGoalsInMatch(m, p.playerId);
      lines.set(p.playerId, l);
    }
  }
  // Inscritos que aún no han jugado también salen en la ficha.
  for (const id of t.entrants ?? t.teams.flatMap((x) => x.playerIds)) {
    if (!lines.has(id)) {
      lines.set(id, { playerId: id, name: nameOf(id, '?'), played: 0, wins: 0, losses: 0, goalsFor: 0, goalsAgainst: 0, scored: 0, champion: championIds.includes(id) });
    }
  }
  const rows: PlayerLine[] = [...lines.values()].map((l) => ({
    ...l,
    winPct: l.played ? (l.wins / l.played) * 100 : 0,
    mvp: Math.round(mvpScore(l) * 10) / 10,
  }));
  rows.sort((a, b) => b.mvp - a.mvp || b.winPct - a.winPct || b.scored - a.scored || a.name.localeCompare(b.name, 'es'));

  const pick = (score: (m: StoredMatch) => number) => ms.reduce<StoredMatch | undefined>((best, m) => (!best || score(m) > score(best) ? m : best), undefined);
  const biggest = pick((m) => Math.abs(m.result.score.white - m.result.score.blue));
  const longest = pick((m) => m.result.totalTimeMs);
  const highest = pick((m) => m.result.score.white + m.result.score.blue);
  const scorers = rows.filter((r) => r.scored > 0).sort((a, b) => b.scored - a.scored);

  const finalFx = t.fixtures.find((f) => f.stage === 'final') ?? (t.format === 'bracket' ? t.fixtures.reduce((a, b) => (b.round > a.round ? b : a)) : undefined);
  let finalScore: string | undefined;
  if (finalFx?.series) finalScore = `${finalFx.series.white}–${finalFx.series.blue} en la serie`;
  else if (finalFx?.matchId) {
    const fm = ms.find((m) => m.id === finalFx.matchId);
    if (fm) finalScore = scoreText(fm);
  }

  return {
    matches: ms,
    startedAt: ms[0]?.startedAt,
    finishedAt: t.finishedAt ?? ms[ms.length - 1]?.finishedAt,
    totalGoals,
    avgGoals: ms.length ? totalGoals / ms.length : 0,
    playTimeMs,
    extraTime,
    players: rows,
    mvp: ms.length && rows[0]?.played ? rows[0] : undefined,
    topScorer: scorers[0],
    biggestWin: biggest && { match: biggest, label: matchLabel(biggest) },
    longest: longest && { match: longest, label: matchLabel(longest) },
    highestScoring: highest && { match: highest, label: matchLabel(highest) },
    championIds,
    finalScore,
  };
}

export interface EditionRow {
  tournament: Tournament;
  edition: number;
  champion?: string;
  championIds: string[];
  mvp?: PlayerLine;
  date: number;
}

export interface HonourRow {
  playerId: string;
  name: string;
  titles: number;
  mvps: number;
  /** Ediciones jugadas. */
  editions: number;
}

export interface Competition {
  key: string;
  name: string;
  format: string;
  editions: EditionRow[];
  /** Jugadores con más títulos (luego MVP y ediciones jugadas). */
  honours: HonourRow[];
  lastChampion?: EditionRow;
}

/** Palmarés de todas las competiciones, la más reciente primero. */
export function competitions(tournaments: Tournament[], matches: StoredMatch[], players: Player[]): Competition[] {
  const groups = new Map<string, Tournament[]>();
  for (const t of tournaments) {
    if (t.status === 'cancelled') continue;
    const k = competitionKey(t);
    groups.set(k, [...(groups.get(k) ?? []), t]);
  }
  const out: Competition[] = [];
  for (const [key, list] of groups) {
    const sorted = [...list].sort((a, b) => a.createdAt - b.createdAt);
    const editions: EditionRow[] = sorted.map((t, i) => {
      const r = tournamentReport(t, matches, players);
      const champ = t.teams.find((x) => x.id === t.winnerTeamId);
      return {
        tournament: t,
        edition: t.edition ?? i + 1,
        champion: champ?.name,
        championIds: champ?.playerIds ?? [],
        mvp: t.status === 'finished' ? r.mvp : undefined,
        date: t.finishedAt ?? t.createdAt,
      };
    });
    const honours = new Map<string, HonourRow>();
    const row = (id: string, name: string) => {
      const h = honours.get(id) ?? { playerId: id, name: players.find((p) => p.id === id)?.name ?? name, titles: 0, mvps: 0, editions: 0 };
      honours.set(id, h);
      return h;
    };
    for (const e of editions) {
      const r = tournamentReport(e.tournament, matches, players);
      r.players.filter((p) => p.played > 0).forEach((p) => (row(p.playerId, p.name).editions += 1));
      e.championIds.forEach((id) => (row(id, '?').titles += 1));
      if (e.mvp) row(e.mvp.playerId, e.mvp.name).mvps += 1;
    }
    const finished = editions.filter((e) => e.tournament.status === 'finished');
    out.push({
      key,
      name: competitionName(sorted[sorted.length - 1]),
      format: FORMAT_LABEL[sorted[sorted.length - 1].format],
      editions: [...editions].reverse(),
      honours: [...honours.values()].sort((a, b) => b.titles - a.titles || b.mvps - a.mvps || b.editions - a.editions || a.name.localeCompare(b.name, 'es')),
      lastChampion: finished[finished.length - 1],
    });
  }
  return out.sort((a, b) => (b.editions[0]?.date ?? 0) - (a.editions[0]?.date ?? 0));
}
