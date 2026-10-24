/**
 * La pantalla de 7" calcula ELO, experiencia, niveles, rangos y logros con una copia en C++ de la progresión de la app
 * (hardware/esp32s3_pantalla7/s3_progresion.h). Aquí se guarda lo que calcula la APP con unos partidos fijos
 * (tests/fixtures/progresion/partidos.jsonl, sacados una vez del modo prueba) y la prueba del PC de la placa
 * (hardware/test/progresion_test.cpp) comprueba que la placa da exactamente lo mismo.
 *
 * Si se cambia una regla de la app, esta prueba falla: hay que regenerar lo esperado con
 *   ACTUALIZAR_PROGRESION=1 npx vitest run tests/progresionPlaca.test.ts
 * y cambiar también la copia de la placa hasta que su prueba vuelva a pasar.
 * (GENERAR_PARTIDOS=1 vuelve a sacar los partidos del modo prueba.)
 *
 * Con torneos: partidos-torneos.jsonl son los mismos partidos del modo prueba con su «tournament» y torneos.txt dice
 * de cada torneo si sigue en juego y, si terminó, qué partido lo cerró y quiénes ganaron (lo que la placa saca de sus
 * resúmenes). torneos-app.json son esos torneos completos, como los guarda la app (la placa los lee en
 * hardware/test/torneos_datos_test.cpp). Los partidos de un torneo en juego solo mueven el ELO; el que cierra uno da
 * «Ganar torneo».
 */
process.env.TZ = 'Europe/Madrid';

import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { generateDemoData } from '../src/services/demo/demoData';
import { DEFAULT_PROGRESSION, type StoredMatch, type Tournament } from '../src/services/persistence';
import { ACHIEVEMENTS, activeChallenges, categoryFor, challengeProgress, computeProgression } from '../src/services/progression';

const DIR = join(__dirname, 'fixtures', 'progresion');
const MATCHES = join(DIR, 'partidos.jsonl');
const EXPECTED = join(DIR, 'esperado.txt');
const T_MATCHES = join(DIR, 'partidos-torneos.jsonl');
const T_INFO = join(DIR, 'torneos.txt');
const T_FULL = join(DIR, 'torneos-app.json');
const T_EXPECTED = join(DIR, 'esperado-torneos.txt');
const R_EXPECTED = join(DIR, 'esperado-retos.txt');

/** Código de un jugador en la placa: FNV-1a de su id (como soundCode). */
function code(id: string): string {
  let h = 2166136261;
  for (const b of new TextEncoder().encode(id)) {
    h ^= b;
    h = Math.imul(h, 16777619) >>> 0;
  }
  return h.toString(16).padStart(8, '0');
}

function loadMatches(): StoredMatch[] {
  if (process.env.GENERAR_PARTIDOS || !existsSync(MATCHES)) {
    const demo = generateDemoData(Date.UTC(2026, 9, 18, 18, 0, 0), 20261003);
    // Sin fotos ni torneos (la placa aún no juega torneos): solo los partidos, con los nombres en la ficha.
    const lines = demo.matches.map((m) => {
      const { tournament: _t, ...rest } = m;
      return JSON.stringify(rest);
    });
    mkdirSync(DIR, { recursive: true });
    writeFileSync(MATCHES, lines.join('\n') + '\n');
  }
  return readFileSync(MATCHES, 'utf8')
    .split('\n')
    .filter(Boolean)
    .map((l) => JSON.parse(l) as StoredMatch);
}

/** Los partidos del modo prueba con sus torneos (y una línea por torneo para la placa). */
function loadTournamentData(): { matches: StoredMatch[]; tournaments: Tournament[] } {
  if (process.env.GENERAR_PARTIDOS || !existsSync(T_MATCHES) || !existsSync(T_INFO) || !existsSync(T_FULL)) {
    const demo = generateDemoData(Date.UTC(2026, 9, 18, 18, 0, 0), 20261003);
    mkdirSync(DIR, { recursive: true });
    writeFileSync(T_MATCHES, demo.matches.map((m) => JSON.stringify(m)).join('\n') + '\n');
    writeFileSync(T_FULL, JSON.stringify(demo.tournaments, null, 1) + '\n');
    // id estado(a/f/c) partidoFinal campeones (códigos, con comas; «-» si no hay)
    const lines = demo.tournaments.map((t) => {
      const champ = t.teams.find((x) => x.id === t.winnerTeamId)?.playerIds ?? [];
      return `${t.id} ${t.status[0]} ${t.finalMatchId ?? '-'} ${champ.map(code).join(',') || '-'}`;
    });
    writeFileSync(T_INFO, lines.join('\n') + '\n');
  }
  const matches = readFileSync(T_MATCHES, 'utf8')
    .split('\n')
    .filter(Boolean)
    .map((l) => JSON.parse(l) as StoredMatch);
  // Lo justo de cada torneo para la progresión (lo mismo que lee la placa de torneos.txt).
  const tournaments = readFileSync(T_INFO, 'utf8')
    .split('\n')
    .filter(Boolean)
    .map((l) => {
      const [id, st, fin] = l.split(' ');
      const winners = matches.find((m) => m.id === fin);
      const champ = l.split(' ')[3];
      const playerIds = champ === '-' || !winners ? [] : winners.participants.filter((p) => champ.split(',').includes(code(p.playerId))).map((p) => p.playerId);
      return {
        id,
        status: st === 'a' ? 'active' : st === 'f' ? 'finished' : 'cancelled',
        ...(fin !== '-' ? { finalMatchId: fin } : {}),
        ...(playerIds.length ? { winnerTeamId: 'campeon' } : {}),
        teams: [{ id: 'campeon', name: '', playerIds }],
      } as unknown as Tournament;
    });
  return { matches, tournaments };
}

/** Una línea por jugador (por código): elo maxElo clasificatorios xp nivel rango logros(ids, en el orden del catálogo). */
function expectedText(matches: StoredMatch[], tournaments?: Tournament[]): string {
  const ids = [...new Set(matches.flatMap((m) => m.participants.map((p) => p.playerId)))];
  const snap = computeProgression(ids, matches, DEFAULT_PROGRESSION, tournaments ? { tournaments } : {});
  const cats = ['scrap', 'wood', 'bronze', 'silver', 'gold', 'platinum', 'diamond'];
  const lines = ids
    .map((id) => {
      const p = snap.players.get(id)!;
      const got = new Set(p.achievements.map((a) => a.id));
      const ach = ACHIEVEMENTS.filter((a) => got.has(a.id)).map((a) => a.id);
      return `${code(id)} ${p.elo} ${p.maxElo} ${p.rankedPlayed} ${p.xp} ${p.level} ${cats.indexOf(categoryFor(p.elo).id)} ${ach.join(',') || '-'}`;
    })
    .sort();
  // Y por partido: código, ELO antes y después y XP ganada de cada jugador.
  for (const m of matches) {
    const e = snap.byMatch.get(m.id);
    for (const part of m.participants) {
      const x = e?.get(part.playerId);
      if (!x) continue;
      lines.push(`P ${m.id} ${code(part.playerId)} ${x.eloBefore ?? '-'} ${x.eloAfter ?? '-'} ${x.xpGained} ${x.levelAfter}${x.deferred ? ' d' : ''}`);
    }
  }
  return lines.join('\n') + '\n';
}

describe('progresión de la placa', () => {
  const matches = loadMatches();

  it('hay partidos de todo tipo para comparar', () => {
    expect(matches.length).toBeGreaterThan(80);
    expect(matches.some((m) => m.config.mode === 'ranked')).toBe(true);
    expect(matches.some((m) => m.result.reason === 'penalties')).toBe(true);
  });

  it('lo esperado coincide con lo que calcula hoy la app', () => {
    const text = expectedText(matches);
    if (process.env.ACTUALIZAR_PROGRESION || !existsSync(EXPECTED)) writeFileSync(EXPECTED, text);
    expect(readFileSync(EXPECTED, 'utf8')).toBe(text);
  });
});

describe('progresión de la placa con torneos', () => {
  const { matches, tournaments } = loadTournamentData();

  it('hay un torneo en juego y otros terminados con campeón', () => {
    expect(tournaments.some((t) => t.status === 'active')).toBe(true);
    expect(tournaments.some((t) => t.status === 'finished' && t.finalMatchId && t.winnerTeamId)).toBe(true);
    const live = new Set(tournaments.filter((t) => t.status === 'active').map((t) => t.id));
    expect(matches.some((m) => m.tournament && live.has(m.tournament.id))).toBe(true);
  });

  it('los resúmenes de la placa dan lo mismo que los torneos completos de la app', () => {
    const full = JSON.parse(readFileSync(T_FULL, 'utf8')) as Tournament[];
    expect(full.map((t) => t.id)).toEqual(tournaments.map((t) => t.id));
    expect(expectedText(matches, tournaments)).toBe(expectedText(matches, full));
  });

  it('lo esperado coincide con lo que calcula hoy la app', () => {
    const text = expectedText(matches, tournaments);
    if (process.env.ACTUALIZAR_PROGRESION || !existsSync(T_EXPECTED)) writeFileSync(T_EXPECTED, text);
    expect(readFileSync(T_EXPECTED, 'utf8')).toBe(text);
    // Hay XP en espera y campeones con su premio.
    expect(text).toMatch(/ d\n/);
    expect(text).toMatch(/champion/);
  });
});

/**
 * Pantalla RETOS de la placa (ui_retos.h): en varios momentos (segundos UTC; al terminar algunos partidos y un rato
 * después), los retos activos («R momento clave meta xp texto») y el progreso de cada jugador
 * («momento código valor/meta o ok» por reto).
 */
function challengesText(matches: StoredMatch[]): string {
  const ids = [...new Set(matches.flatMap((m) => m.participants.map((p) => p.playerId)))].sort((a, b) =>
    code(a).localeCompare(code(b)),
  );
  const ends = [...new Set(matches.map((m) => Math.floor(m.finishedAt / 1000)))].sort((a, b) => a - b);
  const moments = [...ends.slice(-4), ends[Math.floor(ends.length / 2)], ends[ends.length - 1] + 3 * 86400];
  const lines: string[] = [];
  for (const s of moments) {
    for (const c of activeChallenges(s * 1000)) lines.push(`R ${s} ${c.key} ${c.target} ${c.xp} ${c.text}`);
    for (const id of ids) {
      const prog = challengeProgress(id, matches, s * 1000).map((x) => (x.done ? 'ok' : `${x.value}/${x.challenge.target}`));
      lines.push(`${s} ${code(id)} ${prog.join(' ')}`);
    }
  }
  return lines.join('\n') + '\n';
}

describe('retos de la placa', () => {
  it('lo esperado coincide con lo que calcula hoy la app', () => {
    const text = challengesText(loadMatches());
    if (process.env.ACTUALIZAR_PROGRESION || !existsSync(R_EXPECTED)) writeFileSync(R_EXPECTED, text);
    expect(readFileSync(R_EXPECTED, 'utf8')).toBe(text);
    // Hay retos a medias y cumplidos.
    expect(text).toMatch(/ ok/);
    expect(text).toMatch(/ [1-9]\d*\/\d+/);
  });
});
