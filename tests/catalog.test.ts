import { describe, expect, it } from 'vitest';
import { BOARDS, CAP_LABEL, DETECTION, PIN_FUNCTIONS, PIN_ORDER, findBoard, type PinFunction } from '../src/inputs/hardware/catalog';
import boardsHeader from '../hardware/common/mfv3_boards.h?raw';
import mandoIno from '../hardware/mando_pulsadores/mando_pulsadores.ino?raw';
import s3Ino from '../hardware/esp32s3_pantalla7/esp32s3_pantalla7.ino?raw';
import s3Ajustes from '../hardware/esp32s3_pantalla7/s3_ajustes.h?raw';

// Todos los sketches (texto) para comprobar que existe el de cada placa.
const SKETCHES = import.meta.glob('../hardware/*/*.ino', { query: '?raw', import: 'default', eager: true }) as Record<string, string>;

const MACRO: Record<PinFunction, string> = {
  btnBlanco: 'PIN_BTN_BLANCO',
  btnAzul: 'PIN_BTN_AZUL',
  pausa: 'PIN_BTN_PAUSA',
  sensorBlanco: 'PIN_SENSOR_BLANCO',
  sensorAzul: 'PIN_SENSOR_AZUL',
  ledBlanco: 'PIN_LED_BLANCO',
  ledAzul: 'PIN_LED_AZUL',
  zumbador: 'PIN_BUZZER',
};
const digits = (pin: string) => Number(pin.replace(/\D/g, ''));

/** Bloques «// @pines id id…» de mfv3_boards.h con sus #define PIN_…. */
function headerPins(): Map<string, Record<string, number>> {
  const text = boardsHeader;
  const out = new Map<string, Record<string, number>>();
  for (const block of text.split('// @pines ').slice(1)) {
    const [first, ...lines] = block.split('\n');
    const pins: Record<string, number> = {};
    for (const l of lines) {
      const m = /^#define (PIN_\w+) (\d+)/.exec(l);
      if (m) pins[m[1]] = Number(m[2]);
      if (l.startsWith('#elif') || l.startsWith('#else') || l.startsWith('#endif')) break;
    }
    for (const id of first.trim().split(/\s+/)) out.set(id, pins);
  }
  return out;
}

describe('Catálogo «Hazlo tú mismo»', () => {
  it('los pines del catálogo son los del firmware', () => {
    const header = headerPins();
    const sketchBoards = BOARDS.filter((b) => b.sketch === 'arduino_usb' || b.sketch === 'esp32_marcador');
    expect(sketchBoards.length).toBeGreaterThanOrEqual(9);
    for (const b of sketchBoards) {
      const pins = header.get(b.id);
      expect(pins, `bloque @pines de ${b.id}`).toBeDefined();
      for (const f of PIN_ORDER) expect(digits(b.pins[f] ?? ''), `${b.id} ${f}`).toBe(pins![MACRO[f]]);
    }
  });

  it('el mando usa los pines de su sketch', () => {
    const ino = mandoIno;
    const mando = findBoard('mando-c3')!;
    expect(ino).toContain(`PIN_BTN_BLANCO = ${digits(mando.pins.btnBlanco!)};`);
    expect(ino).toContain(`PIN_BTN_AZUL = ${digits(mando.pins.btnAzul!)};`);
  });

  it('cada programa de mando que descarga la pantalla se compila con el perfil de su mini', () => {
    const ids = [...s3Ajustes.matchAll(/"(mando-[\w-]+)"/g)].map((m) => m[1]);
    expect(ids.sort()).toEqual(['mando-c3', 'mando-c3zero']);
    expect(findBoard('mando-c3')!.build?.defines ?? '').not.toContain('MFV3_MANDO_RGB_PIN');
    expect(findBoard('mando-c3zero')!.build?.defines).toContain('-DMFV3_MANDO_RGB_PIN=10');
    for (const id of ids) expect(findBoard(id)!.sketch).toBe('mando_pulsadores');
  });

  it('cada modelo que el firmware puede anunciar está en el catálogo', () => {
    const ids = [
      ...boardsHeader.matchAll(/MFV3_BOARD_ID "([\w-]+)"/g),
      ...s3Ino.matchAll(/S3_BOARD_ID "([\w-]+)"/g),
    ].map((m) => m[1]);
    for (const id of ids.filter((i) => i !== 'generica' && i !== 's3-pantalla')) expect(findBoard(id), id).toBeDefined();
  });

  it('nombres antiguos y desconocidos', () => {
    expect(findBoard('ARDUINO-USB')?.id).toBe('arduino-uno');
    expect(findBoard('ESP32C3')?.id).toBe('esp32c3');
    expect(findBoard('otra-placa')).toBeUndefined();
  });

  it('textos completos: funciones, capacidades y detección', () => {
    for (const f of PIN_ORDER) expect(PIN_FUNCTIONS[f].label).toBeTruthy();
    for (const caps of ['goles', 'anular', 'pausa', 'sensores', 'leds', 'zumbador', 'wifi', 'bluetooth', 'servidor', 'pantalla', 'radio'])
      expect(CAP_LABEL[caps]).toBeTruthy();
    expect(DETECTION.filter((d) => d.recommended).length).toBeGreaterThanOrEqual(2);
  });

  it('existe la carpeta del programa de cada placa', () => {
    for (const b of BOARDS) expect(SKETCHES[`../hardware/${b.sketch}/${b.sketch}.ino`], b.sketch).toBeTruthy();
  });
});
