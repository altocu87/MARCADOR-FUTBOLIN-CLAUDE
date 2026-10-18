/**
 * Partidos guardados por la pantalla de 7" (hardware/esp32s3_pantalla7/s3_partidos.h) en la tarjeta SD: tienen que
 * servirle a la app tal cual (copia de seguridad, estadísticas y progresión). Los de tests/fixtures/partidos-placa
 * salen de la simulación del PC (hardware/test/s3_capturas.cpp).
 */
import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { validGoalsFromEvents } from '../src/match-engine';
import { parseBackup } from '../src/services/persistence/backup';
import { DEFAULT_PROGRESSION, type StoredMatch } from '../src/services/persistence';
import { computeProgression } from '../src/services/progression';

const DIR = join(__dirname, 'fixtures', 'partidos-placa');
const matches: StoredMatch[] = readdirSync(DIR)
  .filter((f) => f.endsWith('.json'))
  .map((f) => JSON.parse(readFileSync(join(DIR, f), 'utf8')) as StoredMatch);

describe('partidos guardados por la placa', () => {
  it('hay partidos de ejemplo', () => {
    expect(matches.length).toBeGreaterThanOrEqual(2);
  });

  it('entran en una copia de seguridad de la app', () => {
    const backup = { app: 'marcador-futbolin-v3', formatVersion: 1, exportedAt: '', players: [], matches, tournaments: [] };
    const r = parseBackup(JSON.stringify(backup));
    expect(r.ok).toBe(true);
  });

  it('los goles válidos dan el resultado guardado y el ganador es el que más marcó', () => {
    for (const m of matches) {
      const goals = validGoalsFromEvents(m.events);
      const sum = { white: 0, blue: 0 };
      for (const g of goals) sum[g.team!] += g.value ?? 1;
      expect(sum).toEqual(m.result.score);
      if (!m.result.penaltyScore) expect(m.result.winner).toBe(sum.white > sum.blue ? 'white' : 'blue');
      expect(m.events[0].type).toBe('MATCH_START');
      expect(m.events[m.events.length - 1].type).toBe('MATCH_END');
    }
  });

  it('un gol anulado se marca con CORRECTION y deja de contar', () => {
    const m = matches.find((x) => x.events.some((e) => e.type === 'CORRECTION'));
    expect(m).toBeTruthy();
    const c = m!.events.find((e) => e.type === 'CORRECTION')!;
    expect(m!.events.some((e) => e.id === c.refEventId && e.type === 'GOAL')).toBe(true);
  });

  it('la progresión de la app los procesa sin fallar', () => {
    const ids = [...new Set(matches.flatMap((m) => m.participants.map((p) => p.playerId)))];
    const snap = computeProgression(ids, matches, DEFAULT_PROGRESSION);
    for (const id of ids) expect(snap.players.get(id)).toBeTruthy();
  });
});
