/**
 * La ficha de un jugador en la pantalla de 7" (ui_ficha.h) enseña los logros con su nombre, título, descripción,
 * apartado, rareza y si son secretos. Son copias de ACHIEVEMENTS en hardware/esp32s3_pantalla7/s3_progresion.h
 * (tablas ACH y ACH_INFO): si se cambia un logro en la app, hay que cambiarlo también allí.
 */
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { ACHIEVEMENTS, CATEGORY_LABEL, RARITY_LABEL } from '../src/services/progression';

const SRC = readFileSync(join(__dirname, '..', 'hardware', 'esp32s3_pantalla7', 's3_progresion.h'), 'utf8');

function table(name: string): string {
  const start = SRC.indexOf(`static const ${name}[] = {`);
  expect(start).toBeGreaterThan(0);
  return SRC.slice(start, SRC.indexOf('\n};', start));
}

describe('logros de la placa', () => {
  const ach = [...table('Ach ACH').matchAll(/\{"([^"]+)", "([^"]+)", (nullptr|"[^"]*"), (COMMON|RARE|EPIC)\}/g)];
  const info = [...table('AchInfo ACH_INFO').matchAll(/\{"((?:[^"\\]|\\.)*)", (\d), (true|false)\},/g)];

  it('mismos logros, en el mismo orden, con nombre, título y rareza', () => {
    expect(ach.map((m) => m[1])).toEqual(ACHIEVEMENTS.map((a) => a.id));
    expect(ach.map((m) => m[2])).toEqual(ACHIEVEMENTS.map((a) => a.name));
    expect(ach.map((m) => (m[3] === 'nullptr' ? undefined : JSON.parse(m[3])))).toEqual(ACHIEVEMENTS.map((a) => a.title));
    expect(ach.map((m) => m[4].toLowerCase())).toEqual(
      ACHIEVEMENTS.map((a) => ({ common: 'common', rare: 'rare', epic: 'epic' })[a.rarity]),
    );
  });

  it('misma descripción, apartado y secreto', () => {
    const groups = Object.keys(CATEGORY_LABEL);
    expect(info.map((m) => JSON.parse(`"${m[1]}"`))).toEqual(ACHIEVEMENTS.map((a) => a.description));
    expect(info.map((m) => groups[Number(m[2])])).toEqual(ACHIEVEMENTS.map((a) => a.category));
    expect(info.map((m) => m[3] === 'true')).toEqual(ACHIEVEMENTS.map((a) => !!a.secret));
  });

  it('nombres de apartados y rarezas', () => {
    expect(SRC).toContain(`ACH_GROUP[6] = {${Object.values(CATEGORY_LABEL).map((l) => `"${l}"`).join(', ')}}`);
    expect(SRC).toContain(`RARITY_NAME[3] = {${Object.values(RARITY_LABEL).map((l) => `"${l}"`).join(', ')}}`);
  });
});
