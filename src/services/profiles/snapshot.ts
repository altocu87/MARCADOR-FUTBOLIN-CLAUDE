/**
 * Perfil público de un jugador: una «foto» de sus datos que se sube a internet para verla desde el móvil
 * (leyendo su QR). Se calcula aquí, en el aparato, con las mismas cuentas que el perfil de la app; la página
 * del móvil solo la pinta. Es de solo lectura: desde el móvil no se cambia nada.
 */
import type { Player, StoredMatch, Tournament } from '../persistence';
import { achievementById, displayTitle, type PlayerProgress } from '../progression';
import { computePlayerStats, habits, personalGoals, personalGoalsInMatch, sideStats, sortMatches, teamOf, type ResultLetter, type StatBlock } from '../statistics';
import type { WinRecord } from '../statistics/extras';

export const SNAPSHOT_VERSION = 1;
/** Partidos del historial que viajan (los más recientes). */
export const SNAPSHOT_MATCHES = 60;
const TOP_PEOPLE = 12;

export type MatchModeId = 'quick' | 'chaos' | 'ranked';
export type FormatId = 'league' | 'bracket' | 'pool';

export interface SnapshotPerson {
  name: string;
  photo?: string;
}

export interface SnapshotRival {
  id: string;
  played: number;
  wins: number;
  draws: number;
  losses: number;
  winPct: number;
}

export interface SnapshotMatch {
  at: number;
  mode: MatchModeId;
  /** Torneo al que pertenece (nombre y tipo). */
  tournament?: { name: string; format: FormatId };
  /** «G», «P» o «E». */
  result: ResultLetter;
  /** Marcador visto desde el jugador: los suyos primero. */
  mine: number;
  theirs: number;
  /** Penaltis, oro… («Penaltis 4–3»). */
  note?: string;
  mates: string[];
  rivals: string[];
  /** Goles que se le asignaron a él. */
  goals: number;
}

export interface SnapshotTitle {
  name: string;
  format: FormatId;
  status: 'active' | 'finished' | 'cancelled';
  champion: boolean;
  at: number;
  cup?: string;
  logo?: string;
  banner?: string;
}

export interface ProfileSnapshot {
  v: number;
  generatedAt: number;
  player: {
    name: string;
    alias?: string;
    photo?: string;
    title?: string;
    level?: number;
    xp?: number;
    elo?: number;
    maxElo?: number;
    category?: { name: string; color: string };
    since: number;
  };
  stats: {
    general: StatBlock;
    ranked: StatBlock;
    byMode: Record<MatchModeId, StatBlock>;
    streak: { type: 'G' | 'P' | null; count: number };
    bestWinStreak: number;
    recent: ResultLetter[];
    playTimeMs: number;
    personalGoals: number;
  };
  sides: Record<'white' | 'blue', WinRecord>;
  habits: {
    byWeekday: { label: string; rec: WinRecord }[];
    byDayPart: { label: string; rec: WinRecord }[];
    favoriteDay: string | null;
    bestDayPart: string | null;
  };
  /** Gente que sale en el perfil (rivales, compañeros, partidos): nombre y foto. */
  people: Record<string, SnapshotPerson>;
  rivals: SnapshotRival[];
  teammates: SnapshotRival[];
  favoriteRival: string | null;
  nemesis: string | null;
  palmares: SnapshotTitle[];
  matches: SnapshotMatch[];
  achievements: { id: string; name: string; icon: string; rarity: string; at: number }[];
  /** Premios de eventos (Halloween…): nombre, tipo y dibujo. */
  rewards: { name: string; tipo: string; archivo?: string; at: number }[];
}

export interface SnapshotInputs {
  player: Player;
  players: Player[];
  matches: StoredMatch[];
  tournaments: Tournament[];
  progress?: PlayerProgress;
  rewards?: { name: string; tipo: string; archivo?: string; at: number }[];
  now: number;
}

const rivalOut = (r: { playerId: string; played: number; wins: number; draws: number; losses: number; winPct: number }): SnapshotRival => ({
  id: r.playerId,
  played: r.played,
  wins: r.wins,
  draws: r.draws,
  losses: r.losses,
  winPct: Math.round(r.winPct),
});

function matchNote(m: StoredMatch): string | undefined {
  const r = m.result;
  if (r.penaltyScore) return `Penaltis ${r.penaltyScore.white}–${r.penaltyScore.blue}`;
  if (r.reason === 'golden_goal') return 'Gol de oro';
  return undefined;
}

export function buildProfileSnapshot(i: SnapshotInputs): ProfileSnapshot {
  const { player, matches, tournaments, progress: prog } = i;
  const pid = player.id;
  const byId = new Map(i.players.map((p) => [p.id, p]));
  const tById = new Map(tournaments.map((t) => [t.id, t]));
  const stats = computePlayerStats(pid, matches);
  const hab = habits(pid, matches);
  const people: Record<string, SnapshotPerson> = {};
  const person = (id: string) => {
    if (people[id]) return;
    const p = byId.get(id);
    people[id] = { name: p?.name ?? 'Jugador borrado', photo: p?.photo };
  };

  const rivals = stats.rivals.slice(0, TOP_PEOPLE).map(rivalOut);
  const teammates = stats.teammates.slice(0, TOP_PEOPLE).map(rivalOut);
  [...rivals, ...teammates].forEach((r) => person(r.id));
  if (stats.favoriteRival) person(stats.favoriteRival.playerId);
  if (stats.nemesis) person(stats.nemesis.playerId);

  const mine = sortMatches(matches.filter((m) => teamOf(m, pid))).reverse().slice(0, SNAPSHOT_MATCHES);
  const history: SnapshotMatch[] = mine.map((m) => {
    const team = teamOf(m, pid)!;
    const other = team === 'white' ? 'blue' : 'white';
    const t = m.tournament ? tById.get(m.tournament.id) : undefined;
    const mates = m.participants.filter((p) => p.team === team && p.playerId !== pid).map((p) => p.playerId);
    const rivalIds = m.participants.filter((p) => p.team === other).map((p) => p.playerId);
    [...mates, ...rivalIds].forEach(person);
    return {
      at: m.finishedAt,
      mode: m.config.mode as MatchModeId,
      tournament: t ? { name: t.name, format: t.format } : undefined,
      result: !m.result.winner ? 'E' : m.result.winner === team ? 'G' : 'P',
      mine: m.result.score[team],
      theirs: m.result.score[other],
      note: matchNote(m),
      mates,
      rivals: rivalIds,
      goals: personalGoalsInMatch(m, pid),
    };
  });

  const palmares: SnapshotTitle[] = tournaments
    .filter((t) => t.status !== 'cancelled' && (t.teams.some((x) => x.playerIds.includes(pid)) || (t.entrants ?? []).includes(pid)))
    .map((t) => {
      const champion = t.status === 'finished' && !!t.teams.find((x) => x.id === t.winnerTeamId)?.playerIds.includes(pid);
      return {
        name: t.name,
        format: t.format,
        status: t.status,
        champion,
        at: t.finishedAt ?? t.createdAt,
        cup: t.cup,
        logo: t.logo,
        banner: t.banner,
      };
    })
    .sort((a, b) => Number(b.champion) - Number(a.champion) || b.at - a.at);

  return {
    v: SNAPSHOT_VERSION,
    generatedAt: i.now,
    player: {
      name: player.name,
      alias: player.alias,
      photo: player.photo,
      title: displayTitle(prog, player.titleId) ?? undefined,
      level: prog?.level,
      xp: prog?.xp,
      elo: prog?.elo,
      maxElo: prog?.maxElo,
      category: prog ? { name: prog.category.name, color: prog.category.color } : undefined,
      since: player.createdAt,
    },
    stats: {
      general: stats.general,
      ranked: stats.ranked,
      byMode: stats.byMode as Record<MatchModeId, StatBlock>,
      streak: stats.currentStreak,
      bestWinStreak: stats.bestWinStreak,
      recent: stats.recent,
      playTimeMs: stats.totalPlayTimeMs,
      personalGoals: personalGoals(matches).get(pid) ?? 0,
    },
    sides: sideStats(pid, matches),
    habits: hab,
    people,
    rivals,
    teammates,
    favoriteRival: stats.favoriteRival?.playerId ?? null,
    nemesis: stats.nemesis?.playerId ?? null,
    palmares,
    matches: history,
    achievements: (prog?.achievements ?? [])
      .map((a) => {
        const def = achievementById(a.id);
        return def ? { id: a.id, name: def.name, icon: def.icon, rarity: def.rarity, at: a.at } : null;
      })
      .filter((a): a is NonNullable<typeof a> => !!a)
      .sort((a, b) => b.at - a.at),
    rewards: [...(i.rewards ?? [])].sort((a, b) => b.at - a.at),
  };
}

/** Huella corta del contenido (para no volver a subir lo que no ha cambiado). */
export function snapshotHash(s: ProfileSnapshot): string {
  const text = JSON.stringify({ ...s, generatedAt: 0 });
  let h1 = 0x811c9dc5;
  let h2 = 0;
  for (let k = 0; k < text.length; k++) {
    const c = text.charCodeAt(k);
    h1 = Math.imul(h1 ^ c, 0x01000193) >>> 0;
    h2 = (h2 * 31 + c) >>> 0;
  }
  return `${h1.toString(36)}${h2.toString(36)}${text.length.toString(36)}`;
}

/** ¿Tiene perfil público? Los invitados no. */
export const hasPublicProfile = (p: Player) => !p.guest;
