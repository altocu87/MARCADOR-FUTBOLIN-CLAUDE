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
 */
process.env.TZ = 'Europe/Madrid';

import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { generateDemoData } from '../src/services/demo/demoData';
import { DEFAULT_PROGRESSION, type StoredMatch } from '../src/services/persistence';
import { ACHIEVEMENTS, categoryFor, computeProgression } from '../src/services/progression';

const DIR = join(__dirname, 'fixtures', 'progresion');
const MATCHES = join(DIR, 'partidos.jsonl');
const EXPECTED = join(DIR, 'esperado.txt');

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

/** Una línea por jugador (por código): elo maxElo clasificatorios xp nivel rango logros(ids, en el orden del catálogo). */
function expectedText(matches: StoredMatch[]): string {
  const ids = [...new Set(matches.flatMap((m) => m.participants.map((p) => p.playerId)))];
  const snap = computeProgression(ids, matches, DEFAULT_PROGRESSION);
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
      lines.push(`P ${m.id} ${code(part.playerId)} ${x.eloBefore ?? '-'} ${x.eloAfter ?? '-'} ${x.xpGained} ${x.levelAfter}`);
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
