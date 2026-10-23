/**
 * La pantalla de 7" juega los torneos con una copia en C++ de las reglas de la app
 * (hardware/esp32s3_pantalla7/s3_torneos_reglas.h y s3_torneos_ficha.h). Aquí la APP crea varios torneos con
 * semillas fijas, juega resultados fijos, deshace, cambia reglas y abandona, y guarda en tests/fixtures/torneos lo
 * que pasa en cada paso (cruces por nombre, clasificación con flechas, ranking final, etiquetas y carteles de fase,
 * reglas de cada cruce, reloj del Pool, ficha y MVP, palmarés, templateFit y describeTemplate). La prueba del PC de la
 * placa (hardware/test/torneos_test.cpp) repite lo mismo y comprueba que sale exactamente igual.
 *
 * Si se cambia una regla de la app, esta prueba falla: hay que regenerar lo esperado con
 *   ACTUALIZAR_TORNEOS=1 npx vitest run tests/torneosPlaca.test.ts
 * y cambiar también la copia de la placa hasta que su prueba vuelva a pasar.
 * Los ids salen de un contador por prefijo («t_1», «tt_3», «fx_7»), igual que en la prueba de la placa.
 */
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it, vi } from 'vitest';

const ids = vi.hoisted(() => ({ n: {} as Record<string, number> }));
vi.mock('../src/services/ids', () => ({
  newId: (prefix: string) => {
    ids.n[prefix] = (ids.n[prefix] ?? 0) + 1;
    return `${prefix}_${ids.n[prefix]}`;
  },
}));

import type { MatchConfig, MatchEvent, Team } from '../src/match-engine';
import type { Fixture, Player, StoredMatch, Tournament, TournamentTemplate } from '../src/services/persistence';
import { personalGoalsInMatch, scorerCoverage } from '../src/services/statistics/extras';
import { seededRandom } from '../src/services/statistics/calendar';
import {
  FORMAT_LABEL,
  activeEdition,
  arrivalLineup,
  competitionKey,
  competitions,
  createTournament,
  describeTemplate,
  draftFromTemplate,
  finalRanking,
  finalSeriesEditable,
  fixtureConfig,
  fixtureProgress,
  ladderClock,
  lineupOfRound,
  markAbandoned,
  nextEdition,
  nextFixture,
  phaseInfo,
  recordFixtureResult,
  recordedMatches,
  roundLabel,
  standings,
  standingsMovement,
  templateFit,
  timeForFinal,
  tournamentReport,
  undoMatch,
  updateTournamentRules,
  validateDraft,
  type Lineup,
  type MatchRules,
  type TournamentDraft,
} from '../src/services/tournaments';

const DIR = join(__dirname, 'fixtures', 'torneos');
const UPDATE = !!process.env.ACTUALIZAR_TORNEOS;

// ---------------------------------------------------------------- jugadores (con tildes, ñ y ELO empatados)
const NAMES = ['Álvaro', 'Ana', 'beto', 'Carla', 'Ñete', 'Nico', 'Óscar', 'Olga', 'Pablo', 'Rosa', 'Sara', 'Tomás', 'Úrsula', 'Vera', 'Wences', 'Ximena'];
const ELOS = [1210, 1180, 1305, 1180, 990, 1250, 1100, 1420, 1050, 1180, 1333, 960, 1275, 1120, 1005, 1190];
const PLAYERS = NAMES.map((name, i) => ({ id: `p${String(i + 1).padStart(2, '0')}`, name, elo: ELOS[i] }));
const players: Player[] = PLAYERS.map((p) => ({ id: p.id, name: p.name, active: true, createdAt: 0, updatedAt: 0 }));
const eloOf = (id: string) => PLAYERS.find((p) => p.id === id)?.elo ?? 1000;
const nameOf = (id: string) => PLAYERS.find((p) => p.id === id)?.name ?? '?';
const pid = (i: number) => PLAYERS[i - 1].id;

const BASE: Omit<TournamentTemplate, 'id' | 'name' | 'format'> = {
  teamSize: 1,
  pairing: 'elo',
  seeding: 'elo',
  final: 'none',
  finalBestOf: 1,
  ranked: false,
  endCondition: 'goals',
  goalsPerPeriod: 5,
  minutesPerPeriod: 5,
  finalGoals: null,
};

/** p juega el siguiente · x lo juega empatado (a goles, empate: solo vale en la fase regular de una liga; a tiempo,
 * igualados al acabar: empate en liga y, si no, prórroga o penaltis) · u deshace · a abandona. */
type Op = 'p' | 'x' | 'u' | 'a' | 'rest' | { rules: MatchRules; final: MatchRules | null; bestOf?: 1 | 3 };

interface Scenario {
  name: string;
  template: TournamentTemplate;
  selected: string[];
  fixedPairs?: string[][];
  tournamentName: string;
  competition?: string;
  edition?: number;
  logo?: string;
  cup?: string;
  banner?: string;
  now: number;
  /** Probabilidad de apuntar el autor de cada gol en parejas (en 1 contra 1 siempre lo tiene). */
  scorerChance: number;
  /** Minutos que dura cada partido (más o menos). */
  matchMinutes: number;
  ops: Op[];
  cancel?: boolean;
}

const tpl = (id: string, name: string, format: TournamentTemplate['format'], extra: Partial<TournamentTemplate>): TournamentTemplate => ({
  ...BASE,
  id,
  name,
  format,
  ...extra,
});

const LIGA = tpl('tpl_liga', 'Liga del jueves', 'league', {});
const SCENARIOS: Scenario[] = [
  {
    // Liga 1 contra 1 con 5 (impar: cada jornada descansa uno). Deshacer y cambiar reglas a mitad.
    name: 'liga5',
    template: LIGA,
    selected: [1, 2, 3, 5, 6].map(pid),
    tournamentName: '  Liga del jueves ',
    now: Date.UTC(2026, 9, 1, 17, 0, 0),
    scorerChance: 1,
    matchMinutes: 6,
    ops: ['p', 'p', 'p', 'u', 'p', { rules: { endCondition: 'goals', goalsPerPeriod: 4, minutesPerPeriod: 5 }, final: null }, 'a', 'p', 'p', 'u', 'rest'],
  },
  {
    // Otra edición de la misma liga (el palmarés las junta por el tipo de torneo).
    name: 'liga5b',
    template: LIGA,
    selected: [1, 2, 3, 4, 6, 8].map(pid),
    tournamentName: 'Liga del jueves',
    now: Date.UTC(2026, 9, 8, 17, 0, 0),
    scorerChance: 1,
    matchMinutes: 5,
    ops: ['rest'],
  },
  {
    // Liga por parejas equilibradas (7 elegidos: queda fuera el del medio) con un equipo guardado.
    name: 'liga_parejas',
    template: tpl('tpl_lp', 'Parejas', 'league', { teamSize: 2, pairing: 'elo', seeding: 'random', points: { win: 2, draw: 1, loss: 0 } }),
    selected: [1, 2, 3, 4, 5, 6, 7].map(pid),
    fixedPairs: [[pid(4), pid(2)], [pid(9), pid(1)]],
    tournamentName: 'Parejas de otoño',
    competition: 'Trofeo Otoño',
    now: Date.UTC(2026, 9, 2, 18, 0, 0),
    scorerChance: 0.5,
    matchMinutes: 7,
    ops: ['rest'],
  },
  {
    // Liga ida y vuelta a tiempo con empates, puntos propios y final 1º-2º al mejor de 3 con sus reglas.
    name: 'liga_ida_vuelta',
    template: tpl('tpl_iv', 'Ida y vuelta', 'league', {
      endCondition: 'time',
      minutesPerPeriod: 4,
      legs: 2,
      points: { win: 3, draw: 1, loss: 0 },
      final: 'top2',
      finalBestOf: 3,
      finalGoals: 7,
      finalEndCondition: 'both',
      lastTitle: '  Farolillo rojo ',
    }),
    selected: [3, 8, 11, 13].map(pid),
    tournamentName: 'Ida y vuelta',
    logo: 'escudo-04',
    cup: 'copa-2',
    now: Date.UTC(2026, 9, 3, 16, 0, 0),
    scorerChance: 1,
    matchMinutes: 8,
    ops: ['p', 'x', 'p', 'p', 'p', 'u', 'p', 'rest'],
  },
  {
    // Copa de 5 por ELO (3 pases directos) con la final al mejor de 3.
    name: 'copa5_elo',
    template: tpl('tpl_c5', 'Copa', 'bracket', { finalBestOf: 3, seeding: 'elo' }),
    selected: [2, 4, 7, 10, 16].map(pid),
    tournamentName: 'Copa de octubre',
    competition: 'Copa del Almacén',
    edition: 1,
    logo: 'escudo-09',
    banner: 'banner-1',
    now: Date.UTC(2026, 9, 4, 19, 0, 0),
    scorerChance: 1,
    matchMinutes: 5,
    ops: ['x', 'p', 'p', 'a', 'p', 'u', 'p', { rules: { endCondition: 'goals', goalsPerPeriod: 6, minutesPerPeriod: 5 }, final: { endCondition: 'goals', goalsPerPeriod: 8, minutesPerPeriod: 5 }, bestOf: 3 }, 'rest'],
  },
  {
    // Copa de 8 parejas al azar, por sorteo, con un equipo guardado (misma competición por su nombre).
    name: 'copa8_azar',
    template: tpl('tpl_c8', 'Copa parejas', 'bracket', { teamSize: 2, pairing: 'random', seeding: 'random' }),
    selected: PLAYERS.map((p) => p.id),
    fixedPairs: [[pid(5), pid(12)]],
    tournamentName: 'Copa grande',
    competition: '  copa del   ALMACÉN ',
    edition: 3,
    cup: 'copa-1',
    now: Date.UTC(2026, 9, 10, 19, 0, 0),
    scorerChance: 0.9,
    matchMinutes: 6,
    ops: ['p', 'p', 'p', 'u', { rules: { endCondition: 'goals', goalsPerPeriod: 5, minutesPerPeriod: 5 }, final: null, bestOf: 3 }, 'rest'],
  },
  {
    // Pool de 5 parejas por rondas, con cambio de pareja y banquillo; final al mejor de 3.
    name: 'pool_rondas_cambio',
    template: tpl('tpl_p1', 'Pool rondas', 'pool', { teamSize: 2, pairing: 'random', poolSwap: true, poolLimit: 'rounds', poolRounds: 4, finalBestOf: 3 }),
    selected: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10].map(pid),
    tournamentName: 'Pool del viernes',
    now: Date.UTC(2026, 9, 5, 18, 0, 0),
    scorerChance: 0.85,
    matchMinutes: 5,
    ops: ['p', 'p', 'x', 'p', 'p', 'u', 'p', 'rest'],
  },
  {
    // Pool de 4 parejas fijas por rondas (sin cambio); se descarta al final (no cuenta en el palmarés).
    name: 'pool_rondas_fijas',
    template: tpl('tpl_p2', 'Pool fijo', 'pool', { teamSize: 2, pairing: 'manual', poolSwap: false, poolLimit: 'rounds', poolRounds: 3 }),
    selected: [3, 6, 9, 12, 13, 14, 15, 16].map(pid),
    fixedPairs: [[pid(3), pid(6)], [pid(9), pid(12)], [pid(13), pid(14)], [pid(15), pid(16)]],
    tournamentName: 'Pool fijo',
    now: Date.UTC(2026, 9, 6, 18, 0, 0),
    scorerChance: 1,
    matchMinutes: 5,
    ops: ['rest'],
    cancel: true,
  },
  {
    // Pool de 6 parejas por tiempo (30 min): cuando ya solo da para un partido se llama a la final.
    name: 'pool_tiempo',
    template: tpl('tpl_p3', 'Pool 30', 'pool', { teamSize: 2, pairing: 'elo', poolSwap: true, poolLimit: 'time', poolMinutes: 30, endCondition: 'time', minutesPerPeriod: 3, finalGoals: 3, finalEndCondition: 'goals' }),
    selected: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].map(pid),
    tournamentName: 'Pool exprés',
    competition: 'Pool exprés',
    now: Date.UTC(2026, 9, 7, 18, 0, 0),
    scorerChance: 0.6,
    matchMinutes: 3,
    ops: ['p', 'x', 'p', 'p', 'u', 'rest'],
  },
];

// ---------------------------------------------------------------- partidos fijos
interface MatchLine {
  id: string;
  fixture: string;
  winner: 'w' | 'b' | '-';
  w: number;
  b: number;
  reason: 'r' | 'g' | 'p';
  totalMs: number;
  startedAt: number;
  finishedAt: number;
  white: string[];
  blue: string[];
  names: { white: string[]; blue: string[] };
  scored: { white: number[]; blue: number[] };
  goalsValid: number;
  goalsAssigned: number;
}

function makeMatch(t: Tournament, f: Fixture, k: number, rnd: () => number, s: Scenario, clock: { at: number }, forceDraw = false): StoredMatch {
  const cfg: MatchConfig = fixtureConfig(t, f);
  const draws = !!cfg.allowDraw && t.format === 'league' && (f.bestOf ?? 1) === 1;
  let w: number;
  let b: number;
  let reason: 'regulation' | 'golden_goal' | 'penalties' = 'regulation';
  if (cfg.endCondition === 'goals' && forceDraw) {
    w = 2;
    b = 2;
  } else if (cfg.endCondition === 'goals') {
    const loser = Math.floor(rnd() * cfg.goalsPerPeriod);
    [w, b] = rnd() < 0.5 ? [cfg.goalsPerPeriod, loser] : [loser, cfg.goalsPerPeriod];
  } else {
    w = Math.floor(rnd() * 4);
    b = Math.floor(rnd() * 4);
    if (cfg.endCondition === 'both') {
      w = Math.min(w, cfg.goalsPerPeriod);
      b = Math.min(b, cfg.goalsPerPeriod);
    }
    if (forceDraw) b = w;
    if (w === b && !draws) {
      if (rnd() < 0.5) reason = 'golden_goal';
      else reason = 'penalties';
      if (reason === 'golden_goal') (rnd() < 0.5 ? (w += 1) : (b += 1));
    }
  }
  const pens = reason === 'penalties' ? (rnd() < 0.5 ? { white: 4, blue: 3 } : { white: 2, blue: 3 }) : undefined;
  const winner: Team | null = w > b ? 'white' : b > w ? 'blue' : pens ? (pens.white > pens.blue ? 'white' : 'blue') : null;
  const side = (team: Team) => t.teams.find((x) => x.id === (team === 'white' ? f.whiteTeamId : f.blueTeamId))!.playerIds;
  const participants = (['white', 'blue'] as Team[]).flatMap((team) =>
    side(team).map((p, i) => ({ playerId: p, team, slot: (i + 1) as 1 | 2, nameSnapshot: nameOf(p) })),
  );
  const events: MatchEvent[] = [];
  const scorers: Record<string, string> = {};
  let seq = 0;
  const goal = (team: Team) => {
    seq += 1;
    const id = `e${seq}`;
    events.push({ id, seq, type: 'GOAL', team, period: 'first', periodTimeMs: seq * 1000, totalTimeMs: seq * 1000, wallTime: clock.at + seq * 1000, scoreAfter: { white: 0, blue: 0 } } as MatchEvent);
    const members = side(team);
    if (members.length === 1 || rnd() < s.scorerChance) scorers[id] = members[members.length === 1 ? 0 : Math.floor(rnd() * members.length)];
  };
  for (let i = 0; i < w; i += 1) goal('white');
  for (let i = 0; i < b; i += 1) goal('blue');
  const duration = Math.round((s.matchMinutes + rnd() * 2) * 60_000);
  const startedAt = clock.at;
  const finishedAt = startedAt + duration;
  clock.at = finishedAt + 30_000 + Math.floor(rnd() * 60_000);
  return {
    formatVersion: 1,
    id: `m_${s.name}_${k}`,
    engineVersion: 'x',
    rulesVersion: 'x',
    config: cfg,
    participants,
    createdAt: startedAt,
    startedAt,
    finishedAt,
    result: { winner, reason, score: { white: w, blue: b }, ...(pens ? { penaltyScore: pens } : {}), totalTimeMs: duration },
    periods: [],
    events,
    penalties: [],
    scorers,
    tournament: { id: t.id, fixtureId: f.id },
  };
}

function lineOf(m: StoredMatch): MatchLine {
  const ids = (team: Team) => m.participants.filter((p) => p.team === team);
  const c = scorerCoverage(m);
  return {
    id: m.id,
    fixture: m.tournament!.fixtureId,
    winner: m.result.winner === 'white' ? 'w' : m.result.winner === 'blue' ? 'b' : '-',
    w: m.result.score.white,
    b: m.result.score.blue,
    reason: m.result.reason === 'regulation' ? 'r' : m.result.reason === 'golden_goal' ? 'g' : 'p',
    totalMs: m.result.totalTimeMs,
    startedAt: m.startedAt,
    finishedAt: m.finishedAt,
    white: ids('white').map((p) => p.playerId),
    blue: ids('blue').map((p) => p.playerId),
    names: { white: ids('white').map((p) => p.nameSnapshot), blue: ids('blue').map((p) => p.nameSnapshot) },
    scored: { white: ids('white').map((p) => personalGoalsInMatch(m, p.playerId)), blue: ids('blue').map((p) => personalGoalsInMatch(m, p.playerId)) },
    goalsValid: c.total,
    goalsAssigned: c.assigned,
  };
}

// ---------------------------------------------------------------- lo que se compara (texto, una cosa por línea)
const TIER = { final: 0, semi: 1, quarter: 2, eighth: 3, round: 4 } as const;
const rulesText = (c: { endCondition: string; goalsPerPeriod: number; minutesPerPeriod: number; allowDraw?: boolean }) =>
  `${c.endCondition} ${c.goalsPerPeriod} ${c.minutesPerPeriod} ${c.allowDraw ? 1 : 0}`;
const lineupText = (lu: Lineup) =>
  lu.courts.map(([w, b]) => `${w.join('+')} v ${b.join('+')}`).join(';') + (lu.bench ? ` | ${lu.bench.join('+')}` : '');

function snapshot(t: Tournament, matches: StoredMatch[], clockAt: number): string[] {
  const name = (id: string | null | undefined) => (id ? t.teams.find((x) => x.id === id)?.name ?? '?' : '-');
  const out: string[] = [];
  out.push(`T|${t.id}|${t.name}|${t.status}|${name(t.winnerTeamId)}|${t.finalMatchId ?? '-'}|${t.finishedAt ?? 0}|${t.createdAt}|${t.teamSize}|${t.lastTitle ?? '-'}|${t.final ?? 'none'}|${t.edition ?? 0}|${t.competition ?? '-'}`);
  out.push(`C|${rulesText(t.config)}|${t.finalConfig ? rulesText(t.finalConfig) : '-'}|${t.points ? `${t.points.win}-${t.points.draw}-${t.points.loss}` : '-'}`);
  if (t.ladder) out.push(`G|${t.ladder.swap ? 1 : 0}|${t.ladder.limit}|${t.ladder.minutes}|${t.ladder.rounds}|${t.ladder.finalBestOf}|${t.ladder.start.map((p) => p.join('+')).join(';')}|${(t.entrants ?? []).join(',')}`);
  for (const x of t.teams) out.push(`E|${x.id}|${x.name}|${x.playerIds.join(',')}|${x.logo ?? '-'}|${x.clubId ?? '-'}`);
  for (const f of t.fixtures) {
    const next = t.fixtures.find((x) => x.id === f.nextFixtureId);
    out.push(
      [
        'F',
        f.id,
        f.round,
        f.court ?? 0,
        name(f.whiteTeamId),
        name(f.blueTeamId),
        name(f.winnerTeamId),
        f.draw ? 1 : 0,
        f.bye ? 1 : 0,
        f.stage === 'final' ? 1 : 0,
        f.skipped ? 1 : 0,
        f.bestOf ?? 1,
        f.series ? `${f.series.white}-${f.series.blue}` : '-',
        f.matchId ?? '-',
        f.matchIds?.join(',') || '-',
        f.abandonedAt ?? 0,
        f.resting?.join(',') || '-',
        next ? `${next.id}/${f.nextSlot}` : '-',
      ].join('|'),
    );
    const ph = phaseInfo(t, f);
    out.push(`H|${f.id}|${TIER[ph.tier]}|${ph.title}|${ph.sub ?? '-'}|${rulesText(fixtureConfig(t, f))}`);
  }
  out.push(`N|${nextFixture(t)?.id ?? '-'}`);
  const pr = fixtureProgress(t);
  out.push(`P|${pr.done}|${pr.total}|${finalSeriesEditable(t) ? 1 : 0}`);
  const rounds = [...new Set(t.fixtures.map((f) => f.round))].sort((a, b) => a - b);
  for (const r of rounds) out.push(`L|${r}|${roundLabel(t, r)}`);
  const mv = standingsMovement(t, matches);
  for (const r of standings(t, matches)) {
    const m = mv.get(r.team.id);
    out.push(`S|${r.team.name}|${r.played}|${r.wins}|${r.draws}|${r.losses}|${r.points}|${r.goalsFor}|${r.goalsAgainst}|${r.diff}|${m === 'up' ? 1 : m === 'down' ? -1 : 0}`);
  }
  for (const p of finalRanking(t, matches)) out.push(`R|${p.place}|${p.team.name}|${p.champion ? 1 : 0}`);
  if (t.ladder) {
    const c = ladderClock(t, matches, clockAt);
    out.push(`K|${c.started ? 1 : 0}|${c.remainingMs}|${c.avgMs === null ? 0 : Math.trunc(c.avgMs)}|${timeForFinal(t, matches) ? 1 : 0}`);
    for (const r of rounds) {
      out.push(`U|${r}|${lineupText(lineupOfRound(t, r))}`);
      const fx = t.fixtures.filter((f) => f.round === r);
      if (fx.every((f) => f.winnerTeamId)) out.push(`A|${r}|${lineupText(arrivalLineup(t, r))}`);
    }
    // Flechas de la ronda que se enseña (como el panel de pistas de TournamentScreens): pista de antes y de ahora.
    const round = rounds[rounds.length - 1];
    const courtOf = (r: number) => {
      const lu = lineupOfRound(t, r);
      const m = new Map<string, number>();
      lu.courts.forEach((c, i) => c.flat().forEach((p) => m.set(p, i + 1)));
      lu.bench?.forEach((p) => m.set(p, lu.courts.length + 1));
      return m;
    };
    const now = courtOf(round);
    const before = round > 1 ? courtOf(round - 1) : new Map<string, number>();
    for (const p of t.entrants ?? []) {
      const c = now.get(p);
      const was = before.get(p);
      out.push(`M|${round}|${p}|${c && was && was !== c ? (was > c ? 1 : -1) : 0}`);
    }
  }
  return out;
}

function reportLines(t: Tournament, matches: StoredMatch[]): string[] {
  const r = tournamentReport(t, matches, players);
  const out = [
    `r|${r.matches.length}|${r.totalGoals}|${r.playTimeMs}|${r.startedAt ?? 0}|${r.finishedAt ?? 0}|${r.mvp?.playerId ?? '-'}|${r.topScorer?.playerId ?? '-'}|${r.scorersReliable ? 1 : 0}|${r.biggestWin?.match.id ?? '-'}|${r.highestScoring?.match.id ?? '-'}|${r.finalScore ?? '-'}|${r.longest?.match.id ?? '-'}|${r.extraTime}`,
  ];
  for (const l of r.players) out.push(`l|${l.playerId}|${l.played}|${l.wins}|${l.draws}|${l.losses}|${l.goalsFor}|${l.goalsAgainst}|${l.scored}|${l.champion ? 1 : 0}|${l.mvp.toFixed(1)}`);
  return out;
}

function draftLines(d: TournamentDraft, leftover: string[]): string[] {
  return [`D|${leftover.join(',') || '-'}|${d.teams.map((x) => x.playerIds.join('+')).join(';')}`];
}

// ---------------------------------------------------------------- jugar cada torneo
interface Played {
  json: Record<string, unknown>;
  t: Tournament;
  matches: StoredMatch[];
}

function play(s: Scenario): Played {
  const fit: string[] = [];
  for (let n = 2; n <= 17; n += 1) {
    const f = templateFit(s.template, n);
    fit.push(`fit|${n}|${f.ok ? 1 : 0}|${f.reason ?? '-'}|${f.summary ?? '-'}`);
  }
  const draftSeed = `${s.name}-borrador`;
  const createSeed = `${s.name}-crear`;
  const { draft, leftover } = draftFromTemplate(s.template, s.tournamentName, s.selected, eloOf, {}, seededRandom(draftSeed), s.fixedPairs ?? []);
  const full: TournamentDraft = {
    ...draft,
    ...(s.edition ? { edition: s.edition } : {}),
    ...(s.competition ? { competition: s.competition } : {}),
    ...(s.logo ? { logo: s.logo } : {}),
    ...(s.cup ? { cup: s.cup } : {}),
    ...(s.banner ? { banner: s.banner } : {}),
  };
  const validate = [
    `V|${validateDraft(full)[0] ?? '-'}`,
    `V|${validateDraft({ ...full, name: '   ' })[0] ?? '-'}`,
    `V|${validateDraft({ ...full, teams: full.teams.slice(0, 1) })[0] ?? '-'}`,
  ];
  ids.n = {};
  let t = createTournament(full, players, eloOf, s.now, seededRandom(createSeed));
  const matches: StoredMatch[] = [];
  const rnd = seededRandom(`${s.name}-resultados`);
  const clock = { at: s.now + 60_000 };
  const created = snapshot(t, matches, clock.at);
  const steps: Record<string, unknown>[] = [];
  let k = 0;
  const playOne = (draw = false) => {
    const f = nextFixture(t);
    if (!f) return false;
    k += 1;
    const m = makeMatch(t, f, k, rnd, s, clock, draw);
    matches.push(m);
    const before = t;
    t = recordFixtureResult(t, f.id, m, matches);
    const accepted = t !== before;
    if (!accepted) matches.splice(matches.indexOf(m), 1);
    steps.push({ op: 'play', fixture: f.id, accepted, match: lineOf(m), clockAt: clock.at, lines: snapshot(t, matches, clock.at) });
    return true;
  };
  for (const op of s.ops) {
    if (op === 'p') playOne();
    else if (op === 'x') playOne(true);
    else if (op === 'rest') {
      let guard = 0;
      while (t.status === 'active' && playOne() && guard++ < 300);
    } else if (op === 'u') {
      const rec = recordedMatches(t, matches);
      const last = rec[rec.length - 1];
      t = undoMatch(t, last.id, matches);
      matches.splice(matches.indexOf(last), 1);
      steps.push({ op: 'undo', removed: last.id, clockAt: clock.at, lines: snapshot(t, matches, clock.at) });
    } else if (op === 'a') {
      const f = nextFixture(t)!;
      t = markAbandoned(t, f.id, clock.at);
      steps.push({ op: 'abandon', fixture: f.id, at: clock.at, clockAt: clock.at, lines: snapshot(t, matches, clock.at) });
    } else {
      t = updateTournamentRules(t, op.rules, op.final, op.bestOf);
      steps.push({ op: 'rules', rules: op.rules, final: op.final, bestOf: op.bestOf ?? 0, clockAt: clock.at, lines: snapshot(t, matches, clock.at) });
    }
  }
  const report = reportLines(t, matches);
  if (s.cancel) t = { ...t, status: 'cancelled' };
  return {
    t,
    matches,
    json: {
      name: s.name,
      players: PLAYERS,
      template: s.template,
      selected: s.selected,
      fixedPairs: s.fixedPairs ?? [],
      tournamentName: s.tournamentName,
      draftSeed,
      createSeed,
      now: s.now,
      edition: s.edition ?? 0,
      competition: s.competition ?? '',
      logo: s.logo ?? '',
      cup: s.cup ?? '',
      banner: s.banner ?? '',
      cancel: !!s.cancel,
      describe: describeTemplate(s.template),
      fit,
      draft: draftLines(draft, leftover),
      validate,
      created,
      steps,
      report,
      key: competitionKey(t),
    },
  };
}

function palmares(all: Played[]): string[] {
  const ts = all.map((p) => p.t);
  const ms = all.flatMap((p) => p.matches);
  const out: string[] = [];
  for (const c of competitions(ts, ms, players)) {
    out.push(`Q|${c.key}|${c.name}|${c.format}|${c.logo ?? '-'}|${c.cup ?? '-'}|${c.banner ?? '-'}|${c.editions.map((e) => `${e.tournament.id}:${e.edition}:${e.champion ?? '-'}:${e.mvp?.playerId ?? '-'}`).join(',')}|${c.lastChampion?.tournament.id ?? '-'}`);
    for (const h of c.honours) out.push(`h|${c.key}|${h.playerId}|${h.titles}|${h.mvps}|${h.editions}`);
  }
  for (const key of [...new Set(ts.map(competitionKey))]) out.push(`X|${key}|${nextEdition(key, ts)}|${activeEdition(key, ts)?.id ?? '-'}`);
  return out;
}

describe('torneos de la placa', () => {
  // Los ids de los torneos se repiten entre escenarios (cada uno empieza su contador): para el palmarés se les da
  // uno propio, como si fueran torneos distintos guardados.
  const all = SCENARIOS.map((s) => {
    const p = play(s);
    const id = `t_${s.name}`;
    p.t = { ...p.t, id };
    p.matches = p.matches.map((m) => ({ ...m, tournament: { id, fixtureId: m.tournament!.fixtureId } }));
    return p;
  });

  it('los escenarios cubren lo que hay que comparar', () => {
    const steps = all.flatMap((p) => p.json.steps as { op: string; accepted?: boolean; match?: MatchLine }[]);
    expect(steps.some((x) => x.op === 'undo')).toBe(true);
    expect(steps.some((x) => x.op === 'rules')).toBe(true);
    expect(steps.some((x) => x.op === 'abandon')).toBe(true);
    expect(steps.some((x) => x.match?.winner === '-' && x.accepted)).toBe(true);  // empates en liga
    expect(steps.some((x) => x.match?.winner === '-' && !x.accepted)).toBe(true);  // y fuera de la fase regular, no
    expect(all.every((p) => p.t.status !== 'active')).toBe(true);
    expect(steps.some((x) => x.accepted && x.match && x.match.reason !== 'r')).toBe(true);  // prórroga o penaltis
    expect(all.find((p) => p.json.name === 'pool_tiempo')!.t.fixtures.some((f) => f.skipped)).toBe(true);
    expect(all.find((p) => p.json.name === 'liga_ida_vuelta')!.t.fixtures.some((f) => f.stage === 'final' && (f.matchIds?.length ?? 0) >= 2)).toBe(true);
  });

  it('lo esperado coincide con lo que calcula hoy la app', () => {
    mkdirSync(DIR, { recursive: true });
    for (const p of all) {
      const file = join(DIR, `${p.json.name}.json`);
      const text = JSON.stringify(p.json, null, 1) + '\n';
      if (UPDATE || !existsSync(file)) writeFileSync(file, text);
      expect(readFileSync(file, 'utf8')).toBe(text);
    }
    const file = join(DIR, 'palmares.json');
    // seededRandom con letras de fuera de ASCII (y de más de 16 bits): la placa tiene que sacar los mismos números.
    const seeds = ['t_1-r2', 'Ñandú-r3', '🦅 águila', ''].map((seed) => {
      const r = seededRandom(seed);
      return { seed, values: Array.from({ length: 6 }, () => r()) };
    });
    const text = JSON.stringify({ order: SCENARIOS.map((s) => s.name), labels: FORMAT_LABEL, seeds, lines: palmares(all) }, null, 1) + '\n';
    if (UPDATE || !existsSync(file)) writeFileSync(file, text);
    expect(readFileSync(file, 'utf8')).toBe(text);
  });
});
