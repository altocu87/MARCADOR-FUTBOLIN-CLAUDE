import { describe, expect, it } from 'vitest';
import {
  BOARD_EVENTS,
  CHUNK_BYTES,
  crc32,
  momentLines,
  parseSoundsList,
  soundCode,
  uploadSound,
  type BoardTransport,
} from '../src/inputs/hardware/boardSounds';
import type { StoredSound } from '../src/services/persistence';
import { encodeWav } from '../src/services/sound/prepare';
import { SoundPlayer } from '../src/services/sound/soundPlayer';

const snd = (id: string, location: StoredSound['location'], samples = 2000): StoredSound => ({
  id,
  name: id,
  location,
  durationMs: 100,
  createdAt: 1,
  wav: encodeWav(new Float32Array(samples).fill(0.25)),
});

/** Placa simulada: contesta como `mfv3snd::Commands` del firmware (hardware/common/mfv3_sound.h). */
class FakeBoard implements BoardTransport {
  files = new Map<string, Uint8Array>();
  sent: string[] = [];
  private pending: string[] = [];
  private cur: { code: string; size: number; data: number[] } | null = null;
  sd = false;
  breakCrc = false;
  silent = false;
  send(line: string): void {
    this.sent.push(line);
    if (this.silent) return;
    const [w, ...a] = line.split(' ');
    if (w === 'SONIDO_SUBIR') {
      const [place, code, size] = a;
      if (place === 'sd' && !this.sd) return void this.pending.push(`SONIDO_ERROR ${code} sin_sd`);
      this.cur = { code, size: Number(size), data: [] };
      this.pending.push(`SONIDO_LISTO ${code}`);
    } else if (w === 'SONIDO_DATOS' && this.cur) {
      const bin = atob(a[0]);
      for (let i = 0; i < bin.length; i += 1) this.cur.data.push(bin.charCodeAt(i));
      this.pending.push(`SONIDO_OK ${this.cur.data.length}`);
    } else if (w === 'SONIDO_FIN' && this.cur) {
      const bytes = new Uint8Array(this.cur.data);
      const ok = !this.breakCrc && a[0] === crc32(bytes).toString(16).padStart(8, '0') && bytes.length === this.cur.size;
      this.pending.push(ok ? `SONIDO_GUARDADO ${this.cur.code}` : `SONIDO_ERROR ${this.cur.code} crc`);
      if (ok) this.files.set(this.cur.code, bytes);
      this.cur = null;
    }
  }
  async waitFor(match: (l: string) => boolean): Promise<string> {
    const i = this.pending.findIndex(match);
    if (i < 0) throw new Error('sin respuesta');
    return this.pending.splice(i, 1)[0];
  }
}

describe('Sonidos en la placa · protocolo', () => {
  it('el código y el CRC coinciden con los del firmware', () => {
    expect(soundCode('snd_abc')).toBe('a9f86d43'); // hardware/test/sound_test.cpp
    expect(soundCode('')).toBe('811c9dc5');
    expect(crc32(new TextEncoder().encode('123456789'))).toBe(0xcbf43926);
  });

  it('pasa un sonido a la memoria del chip en trozos y llega igual', async () => {
    const board = new FakeBoard();
    const s = snd('snd_x', 'chip');
    const progress: number[] = [];
    await uploadSound(board, s, 'chip', (f) => progress.push(f));
    const code = soundCode('snd_x');
    expect(board.sent[0]).toBe(`SONIDO_SUBIR chip ${code} ${s.wav.byteLength}`);
    expect(board.sent.filter((l) => l.startsWith('SONIDO_DATOS')).length).toBe(Math.ceil(s.wav.byteLength / CHUNK_BYTES));
    expect(board.sent.every((l) => l.length < 383)).toBe(true);
    expect([...board.files.get(code)!]).toEqual([...new Uint8Array(s.wav)]);
    expect(progress[progress.length - 1]).toBe(1);
  });

  it('errores claros: sin tarjeta, dañado o sin respuesta', async () => {
    const board = new FakeBoard();
    await expect(uploadSound(board, snd('a', 'sd'), 'sd')).rejects.toThrow('La placa no tiene tarjeta SD');
    board.sd = true;
    board.breakCrc = true;
    await expect(uploadSound(board, snd('a', 'sd'), 'sd')).rejects.toThrow('El sonido llegó dañado');
    board.silent = true;
    await expect(uploadSound(board, snd('a', 'sd'), 'sd')).rejects.toThrow('La placa no contesta');
  });

  it('lee la lista de sonidos de la placa', () => {
    const inv = parseSoundsList('SONIDOS chip=9216 sd=- 1a2b3c4d@c,0000BEEF@s')!;
    expect(inv.chipFreeKB).toBe(9216);
    expect(inv.sdFreeKB).toBeNull();
    expect(inv.sounds.get('1a2b3c4d')).toBe('chip');
    expect(inv.sounds.get('0000beef')).toBe('sd');
    expect(parseSoundsList('SONIDOS chip=- sd=100')!.sounds.size).toBe(0);
    expect(parseSoundsList('HELLO x')).toBeNull();
  });

  it('qué suena en cada momento cuando la placa juega sola (solo con lo que tiene ella)', () => {
    const sounds = [snd('a', 'chip'), snd('b', 'sd'), snd('c', 'device')];
    const lines = momentLines(
      {
        goal: { kind: 'custom', ids: ['a', 'b', 'c'] },
        victory: { kind: 'custom', ids: ['c'] },
        rout: { kind: 'none', ids: [] },
        levelUp: { kind: 'custom', ids: ['a'] },
      },
      sounds,
    );
    expect(lines.length).toBe(BOARD_EVENTS.length);
    expect(lines).toContain(`MOMENTO goal ${soundCode('a')},${soundCode('b')}`);
    expect(lines).toContain('MOMENTO victory def'); // el del aparato no está en la placa
    expect(lines).toContain('MOMENTO rout no');
    expect(lines).toContain('MOMENTO equalizer def');
    expect(lines.some((l) => l.includes('levelUp'))).toBe(false); // subir de nivel es cosa de la app
  });

  it('el reproductor pide a la placa los suyos (en cola si toca) y si no puede suena el de siempre', () => {
    const p = new SoundPlayer();
    const asked: [string, boolean][] = [];
    let can = true;
    p.board = { play: (s, q) => (asked.push([s.id, q]), can) };
    p.setLibrary([snd('a', 'sd')]);
    p.setChoices({ goal: { kind: 'custom', ids: ['a'] } });
    expect(p.plan('goal')).toMatchObject({ kind: 'board', sound: { id: 'a', location: 'sd' } });
    p.play('goal', { queue: true });
    expect(asked).toEqual([['a', true]]);
    can = false;
    p.play('goal'); // sin placa: el de siempre (sin audio en el PC, no falla)
    expect(asked).toEqual([['a', true], ['a', false]]);
  });
});
