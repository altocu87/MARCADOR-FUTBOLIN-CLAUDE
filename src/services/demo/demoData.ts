/**
 * Datos de prueba: jugadores, partidos y torneos ficticios para probar la app.
 * Los partidos se juegan de verdad con el MatchEngine (reloj simulado), así que
 * eventos, partes, prórrogas y penaltis son coherentes con un partido real.
 * Todo vive en un espacio de almacenamiento aparte y se borra al desactivarlo.
 */
import {
  DEFAULT_CONFIG,
  advance,
  createMatch,
  dispatch,
  nextPenaltyTeam,
  validGoalsFromEvents,
  type MatchConfig,
  type MatchMode,
  type MatchState,
  type ParticipantRef,
  type Team,
} from '../../match-engine';
import { newId } from '../ids';
import { STORAGE_FORMAT_VERSION, type Player, type StoredMatch, type Tournament } from '../persistence';
import { autoScorers } from '../statistics/extras';
import { DEMO_PHOTOS } from './demoPhotos';
import {
  createTournament,
  editionName,
  effectivePoolGames,
  fixtureConfig,
  fixtureParticipants,
  playableFixtures,
  recordFixtureResult,
  type TournamentDraft,
} from '../tournaments';

const DAY = 86_400_000;

/** Nombres ficticios con alias y nivel de juego (0-1) para que haya favoritos. */
const DEMO_PLAYERS: { name: string; alias?: string; skill: number }[] = [
  { name: 'Alex', skill: 0.74 },
  { name: 'Vicky', skill: 0.7 },
  { name: 'Lucía', alias: 'La Muralla', skill: 0.85 },
  { name: 'Marcos', alias: 'Cañonero', skill: 0.8 },
  { name: 'Sara', skill: 0.72 },
  { name: 'Javi', alias: 'Molinillo', skill: 0.68 },
  { name: 'Elena', skill: 0.64 },
  { name: 'Rubén', alias: 'El Muro', skill: 0.6 },
  { name: 'Paula', skill: 0.55 },
  { name: 'Diego', alias: 'Flash', skill: 0.5 },
  { name: 'Carmen', skill: 0.45 },
  { name: 'Iván', skill: 0.4 },
  { name: 'Nerea', alias: 'Zurda', skill: 0.36 },
  { name: 'Óscar', skill: 0.3 },
];

/** Generador pseudoaleatorio con semilla: mismos datos en cada carga. */
function mulberry32(seed: number): () => number {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

export interface DemoData {
  players: Player[];
  matches: StoredMatch[];
  tournaments: Tournament[];
}

export function generateDemoData(now: number = Date.now(), seed = 20261003): DemoData {
  const rnd = mulberry32(seed);
  const pick = <T,>(list: T[]): T => list[Math.floor(rnd() * list.length)];
  const between = (min: number, max: number) => min + rnd() * (max - min);
  const shuffled = <T,>(list: T[]): T[] => {
    const copy = [...list];
    for (let i = copy.length - 1; i > 0; i -= 1) {
      const j = Math.floor(rnd() * (i + 1));
      [copy[i], copy[j]] = [copy[j], copy[i]];
    }
    return copy;
  };

  const startDay = now - 120 * DAY;
  const players: Player[] = DEMO_PLAYERS.map((d, i) => ({
    id: newId('p'),
    name: d.name,
    alias: d.alias,
    ...(DEMO_PHOTOS[d.name] ? { photo: DEMO_PHOTOS[d.name] } : {}),
    active: true,
    createdAt: startDay - (DEMO_PLAYERS.length - i) * 3_600_000,
    updatedAt: startDay,
  }));
  const skill = new Map(players.map((p, i) => [p.id, DEMO_PLAYERS[i].skill]));

  // ---- Simulación de un partido con el motor --------------------------------
  const play = (config: MatchConfig, participants: ParticipantRef[], startAt: number): StoredMatch => {
    const strength = (team: Team) => {
      const ids = participants.filter((p) => p.team === team).map((p) => skill.get(p.playerId) ?? 0.5);
      return ids.reduce((a, b) => a + b, 0) / ids.length;
    };
    const sw = strength('white');
    const sb = strength('blue');
    const pWhite = 0.5 + (sw - sb) * 0.9; // ventaja del favorito, nunca segura
    let t = startAt;
    let s: MatchState = createMatch(newId('m'), config, participants, t);
    for (let guard = 0; guard < 400 && s.phase !== 'finished'; guard += 1) {
      s = advance(s, t).state;
      switch (s.phase) {
        case 'countdown':
          t += 3000;
          break;
        case 'playing': {
          // En la prórroga (gol de oro) cuesta más marcar: así algunas acaban en penaltis.
          // Por tiempo hay menos goles (más empates); en la prórroga cuesta más marcar, así algunas acaban en penaltis.
          const gap =
            s.period === 'overtime' ? between(20_000, 130_000) : config.endCondition === 'goals' ? between(6_000, 45_000) : between(35_000, 120_000);
          t += Math.round(gap);
          s = advance(s, t).state;
          if (s.phase !== 'playing') break;
          const team: Team = rnd() < Math.min(0.85, Math.max(0.15, pWhite)) ? 'white' : 'blue';
          s = dispatch(s, { type: 'GOAL', team, source: pick(['sensor', 'sensor', 'touch', 'button'] as const) }, t).state;
          // De vez en cuando un gol mal contado que se anula con −1.
          if (s.phase === 'playing' && rnd() < 0.04) {
            t += 4000;
            s = dispatch(s, { type: 'MINUS_ONE', team }, t).state;
          }
          break;
        }
        case 'periodEnd':
          t += Math.round(between(20_000, 90_000));
          s = dispatch(s, { type: 'CONTINUE' }, t).state;
          break;
        case 'penalties': {
          t += Math.round(between(8_000, 20_000));
          const team = nextPenaltyTeam(s);
          const p = 0.68 + (team === 'white' ? sw - sb : sb - sw) * 0.3;
          s = dispatch(s, { type: 'PENALTY', team, scored: rnd() < p }, t).state;
          break;
        }
        default:
          t += 1000;
      }
    }
    if (!s.result || s.finishedAt === undefined) throw new Error('Partido de prueba sin terminar.');
    const match: StoredMatch = {
      formatVersion: STORAGE_FORMAT_VERSION,
      id: s.id,
      engineVersion: s.engineVersion,
      rulesVersion: s.rulesVersion,
      config: s.config,
      participants: s.participants,
      createdAt: s.createdAt,
      startedAt: s.startedAt ?? s.createdAt,
      finishedAt: s.finishedAt,
      result: s.result,
      periods: s.periods,
      events: s.events,
      penalties: s.penalties,
    };
    // Goleadores: en 2v2 se reparte entre la pareja según su nivel (en 1v1 es automático).
    if (participants.length === 4) {
      const scorers: Record<string, string> = {};
      for (const g of validGoalsFromEvents(match.events)) {
        const mates = participants.filter((p) => p.team === g.team);
        const [a, b] = mates;
        const wa = skill.get(a.playerId) ?? 0.5;
        const wb = skill.get(b.playerId) ?? 0.5;
        if (rnd() < 0.85) scorers[g.id] = (rnd() < wa / (wa + wb) ? a : b).playerId;
      }
      match.scorers = scorers;
    } else {
      match.scorers = autoScorers(match);
    }
    return match;
  };

  const makeParticipants = (ids: string[], teamSize: 1 | 2): ParticipantRef[] => {
    const name = (id: string) => players.find((p) => p.id === id)!.name;
    const white = ids.slice(0, teamSize);
    const blue = ids.slice(teamSize, teamSize * 2);
    return [
      ...white.map((id, i) => ({ playerId: id, team: 'white' as const, slot: (i + 1) as 1 | 2, nameSnapshot: name(id) })),
      ...blue.map((id, i) => ({ playerId: id, team: 'blue' as const, slot: (i + 1) as 1 | 2, nameSnapshot: name(id) })),
    ];
  };

  const randomConfig = (mode: MatchMode): MatchConfig => {
    const r = rnd();
    // Por goles (a 5–10), por tiempo (dos partes) o ambas.
    const endCondition = r < 0.55 ? 'goals' : r < 0.85 ? 'time' : 'both';
    return {
      ...DEFAULT_CONFIG,
      mode,
      testMode: false,
      endCondition,
      goalsPerPeriod: pick([5, 5, 6, 7, 8, 10]),
      minutesPerPeriod: pick([2, 3, 3, 4, 5]),
    };
  };

  /** Hora de juego plausible: almuerzo, tarde o noche (alguna madrugadora y alguna trasnochada). */
  const playTime = (dayStart: number) => {
    const slot = rnd();
    const hour = slot < 0.05 ? between(7.5, 8.8) : slot < 0.4 ? between(13.5, 15.5) : slot < 0.92 ? between(17, 21) : between(22.2, 23.5);
    return dayStart + Math.round(hour * 3_600_000);
  };

  const matches: StoredMatch[] = [];
  const dayStartOf = (ts: number) => {
    const d = new Date(ts);
    d.setHours(0, 0, 0, 0);
    return d.getTime();
  };

  // ---- Partidos sueltos repartidos en los últimos 4 meses --------------------
  // Una parte cae en el mes en curso para que la temporada actual del ranking no salga vacía.
  const monthStart = new Date(now);
  monthStart.setDate(1);
  monthStart.setHours(0, 0, 0, 0);
  const recentFrom = Math.min(Math.max(monthStart.getTime(), now - 7 * DAY), now - 2 * 3_600_000);
  const FREE_MATCHES = 110;
  const RECENT_MATCHES = 20;
  for (let i = 0; i < FREE_MATCHES; i += 1) {
    const recent = i >= FREE_MATCHES - RECENT_MATCHES;
    const startAt = recent
      ? Math.round(between(recentFrom, now - 30 * 60_000))
      : playTime(dayStartOf(startDay + Math.floor(between(0, 119)) * DAY));
    const r = rnd();
    const mode: MatchMode = r < (recent ? 0.75 : 0.5) ? 'ranked' : r < 0.85 ? 'quick' : 'chaos';
    const teamSize: 1 | 2 = rnd() < 0.6 ? 1 : 2;
    // Los mejores juegan algo más a menudo.
    const pool = players
      .map((p) => ({ id: p.id, key: rnd() + skill.get(p.id)! * 0.5 }))
      .sort((a, b) => b.key - a.key);
    const ids = pool.slice(0, teamSize * 2).map((p) => p.id);
    const participants = makeParticipants(shuffled(ids), teamSize);
    const match = play(randomConfig(mode), participants, startAt);
    matches.push(match);
  }

  // ---- Torneos ---------------------------------------------------------------
  const tournaments: Tournament[] = [];
  const runTournament = (
    name: string,
    format: 'league' | 'bracket' | 'pool',
    teamSize: 1 | 2,
    count: number,
    ranked: boolean,
    firstDay: number,
    maxMatches = Infinity,
    extra: Partial<TournamentDraft> = {},
  ) => {
    // En el Pool `count` son jugadores; en el resto, equipos.
    const ids = shuffled(players).slice(0, format === 'pool' ? count : count * teamSize).map((p) => p.id);
    const teams = format === 'pool' ? [] : Array.from({ length: count }, (_, i) => ({ playerIds: ids.slice(i * teamSize, (i + 1) * teamSize) }));
    let t = createTournament(
      {
        name,
        format,
        teamSize,
        ranked,
        config: { ...DEFAULT_CONFIG, goalsPerPeriod: 5, testMode: false },
        teams,
        seeding: 'random',
        ...(format === 'pool' ? { entrants: ids, gamesPerPlayer: effectivePoolGames(count, 4), pairing: 'random' as const } : {}),
        ...extra,
      },
      players,
      () => 1200,
      dayStartOf(firstDay) + 17 * 3_600_000,
      rnd,
    );
    let clock = t.createdAt + 10 * 60_000;
    let played = 0;
    for (let guard = 0; guard < 60 && played < maxMatches; guard += 1) {
      const fx = playableFixtures(t)[0];
      if (!fx) break;
      const match = play(fixtureConfig(t, fx), fixtureParticipants(t, fx, players), clock);
      match.tournament = { id: t.id, fixtureId: fx.id };
      matches.push(match);
      t = recordFixtureResult(t, fx.id, match, matches);
      played += 1;
      // Varios partidos por tarde; cada 4 se salta al día siguiente.
      clock = played % 4 === 0 ? dayStartOf(match.finishedAt + DAY) + 17 * 3_600_000 : match.finishedAt + Math.round(between(3, 12) * 60_000);
    }
    tournaments.push(t);
  };

  // Ediciones de competiciones de fábrica: así el palmarés tiene historia.
  const edition = (templateId: string, templateName: string, n: number, more: Partial<TournamentDraft> = {}): Partial<TournamentDraft> => ({
    templateId,
    templateName,
    edition: n,
    ...more,
  });
  runTournament(editionName('Liguilla Rápida', 1), 'league', 1, 5, true, startDay + 10 * DAY, Infinity, edition('builtin-league', 'Liguilla Rápida', 1));
  runTournament(editionName('Copa Parejas', 1), 'bracket', 2, 4, false, startDay + 45 * DAY, Infinity, edition('builtin-pairs', 'Copa Parejas', 1, { finalBestOf: 3 }));
  const rotativa = { final: 'top4' as const, finalBestOf: 3 };
  runTournament(editionName('Copa Rotativa', 1), 'pool', 2, 5, false, startDay + 80 * DAY, Infinity, edition('builtin-pool', 'Copa Rotativa', 1, rotativa));
  runTournament(editionName('Copa Rotativa', 2), 'pool', 2, 6, false, startDay + 100 * DAY, Infinity, edition('builtin-pool', 'Copa Rotativa', 2, rotativa));
  runTournament(editionName('Liguilla Rápida', 2), 'league', 1, 6, true, now - 6 * DAY, 8, edition('builtin-league', 'Liguilla Rápida', 2)); // en juego

  matches.sort((a, b) => a.finishedAt - b.finishedAt);
  return { players, matches, tournaments };
}
