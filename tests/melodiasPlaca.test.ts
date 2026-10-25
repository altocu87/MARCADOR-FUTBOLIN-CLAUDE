/**
 * Melodías de victoria en la pantalla de 7" (ficha → OPCIONES → TÍTULO Y MELODÍA): son copias de ANTHEMS
 * (src/services/sound/sound.ts) en hardware/esp32s3_pantalla7/s3_melodias.h. Si se cambia una melodía en la app,
 * hay que cambiarla también allí: mismo id, nombre, orden y notas (frecuencia y duración de cada una en ms).
 */
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';
import { ANTHEMS } from '../src/services/sound/sound';

const SRC = readFileSync(join(__dirname, '..', 'hardware', 'esp32s3_pantalla7', 's3_melodias.h'), 'utf8');

/** Notas de la app como las toca la placa: frecuencia y tiempo hasta la siguiente (la última, su duración entera). */
function appNotes(id: string): [number, number][] {
  const notes = ANTHEMS[id].notes;
  return notes.map((n, i) => [n.freq, Math.round(((i + 1 < notes.length ? notes[i + 1].start - n.start : n.dur / 0.95)) * 1000)]);
}

describe('melodías de la placa', () => {
  const list = [...SRC.matchAll(/\{"(\w+)", "([^"]+)", \{(\w+), MFV3_N\(\w+\)\}\}/g)];

  it('mismas melodías, en el mismo orden y con el mismo nombre', () => {
    expect(list.map((m) => m[1])).toEqual(Object.keys(ANTHEMS));
    expect(list.map((m) => m[2])).toEqual(Object.values(ANTHEMS).map((a) => a.label));
  });

  it('mismas notas', () => {
    for (const [, id, , table] of list) {
      const start = SRC.indexOf(`Note ${table}[] = {`);
      expect(start).toBeGreaterThan(0);
      const body = SRC.slice(start, SRC.indexOf('};', start));
      const notes = [...body.matchAll(/\{(\d+), (\d+)\}/g)].map((m) => [Number(m[1]), Number(m[2])]);
      expect(notes, id).toEqual(appNotes(id));
    }
  });
});
