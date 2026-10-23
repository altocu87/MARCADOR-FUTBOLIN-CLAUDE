/**
 * Torneos guardados por la pantalla de 7" (hardware/esp32s3_pantalla7/s3_torneos_datos.h): los torneos de la app
 * que la placa abre y vuelve a guardar tienen que quedar igual, y sus tipos de torneo tienen que servirle a la app.
 * Los de tests/fixtures/torneos-placa salen de la prueba del PC hardware/test/torneos_datos_test.cpp (con los torneos
 * del modo prueba de la app que hay en tests/fixtures/progresion/torneos-app.json).
 */
import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { parseBackup } from '../src/services/persistence/backup';
import type { Tournament, TournamentTemplate } from '../src/services/persistence';
import { describeTemplate, draftFromTemplate, fixtureProgress, templateMatchConfig } from '../src/services/tournaments';

const DIR = join(__dirname, 'fixtures', 'torneos-placa');
const app = JSON.parse(readFileSync(join(__dirname, 'fixtures', 'progresion', 'torneos-app.json'), 'utf8')) as Tournament[];
const board: Tournament[] = readdirSync(DIR)
  .filter((f) => f.startsWith('t_') && f.endsWith('.json'))
  .map((f) => JSON.parse(readFileSync(join(DIR, f), 'utf8')) as Tournament);
const templates = JSON.parse(readFileSync(join(DIR, 'tipos.json'), 'utf8')) as TournamentTemplate[];

describe('torneos guardados por la placa', () => {
  it('hay torneos de cada formato, en juego y terminados', () => {
    expect(board.length).toBeGreaterThanOrEqual(5);
    expect(new Set(board.map((t) => t.format))).toEqual(new Set(['league', 'bracket', 'pool']));
    expect(board.some((t) => t.status === 'active')).toBe(true);
    expect(board.some((t) => t.status === 'finished')).toBe(true);
  });

  it('un torneo de la app que guarda la placa queda igual (mismos campos y valores)', () => {
    for (const t of board) {
      const original = app.find((x) => x.id === t.id);
      expect(original).toBeTruthy();
      expect(t).toEqual(original);
      expect(fixtureProgress(t)).toEqual(fixtureProgress(original!));
    }
  });

  it('entran en una copia de seguridad de la app', () => {
    const backup = { app: 'marcador-futbolin-v3', formatVersion: 1, exportedAt: '', players: [], matches: [], tournaments: board };
    expect(parseBackup(JSON.stringify(backup)).ok).toBe(true);
  });

  it('los tipos de torneo de la placa le sirven a la app', () => {
    expect(templates.length).toBe(2);
    for (const t of templates) {
      expect(describeTemplate(t)).toContain(t.format === 'pool' ? 'Pool' : 'Liga');
      expect(templateMatchConfig(t).endCondition).toBe(t.endCondition);
    }
    const pool = templates.find((t) => t.format === 'pool')!;
    expect(describeTemplate(pool)).toContain('6 rondas');
    const league = templates.find((t) => t.format === 'league')!;
    // Liga a tiempo con empates (sin poner: sí), final 1º contra 2º con sus propias reglas y a doble vuelta.
    expect(templateMatchConfig(league).allowDraw).toBe(true);
    const { draft } = draftFromTemplate(league, 'Prueba', ['a', 'b', 'c', 'd'], () => 1200);
    expect(draft.finalConfig).toMatchObject({ endCondition: 'goals', goalsPerPeriod: 7 });
    expect(draft.legs).toBe(2);
    expect(draft.points).toEqual({ win: 3, draw: 2, loss: 0 });
  });
});
