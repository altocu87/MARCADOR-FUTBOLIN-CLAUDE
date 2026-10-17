import { describe, expect, it } from 'vitest';
import {
  PHOTO_BYTES,
  parsePhotosList,
  parsePlayersHash,
  playerLine,
  playersHash,
  rgb565Image,
  syncPlayers,
  type BoardPlayer,
  type PlayersTransport,
} from '../src/inputs/hardware/boardPlayers';
import { crc32 } from '../src/inputs/hardware/boardSounds';

const photo = rgb565Image(new Uint8ClampedArray(120 * 120 * 4).fill(200));
const LIST: BoardPlayer[] = [
  { code: '0a1b2c3d', name: 'Alex', meta: 'Nv 23 · ELO 1299', title: 'Tricampeón', photo },
  { code: 'deadbeef', name: 'Lucía | la «Pichichi»', meta: 'Nv 25', title: '', photo: null },
];

describe('Jugadores en la placa', () => {
  it('cada jugador va en una línea «código crc nombre|detalle|título» sin barras de más', () => {
    expect(playerLine(LIST[0])).toBe(`0a1b2c3d ${crc32(photo).toString(16).padStart(8, '0')} Alex|Nv 23 · ELO 1299|Tricampeón`);
    expect(playerLine(LIST[1])).toBe('deadbeef - Lucía   la «Pichichi»|Nv 25|');
  });

  it('recorta por bytes (lo que cabe en la placa) sin partir las letras con tilde', () => {
    const long = playerLine({ ...LIST[1], name: 'Ñandú '.repeat(10) });
    const name = long.split(' - ')[1].split('|')[0];
    expect(new TextEncoder().encode(name).length).toBeLessThanOrEqual(27);
    expect(name.endsWith('�')).toBe(false);
  });

  it('la huella es la de la placa (FNV-1a de las líneas con salto) y cambia con cualquier dato', () => {
    // Mismo valor que comprueba hardware/test/jugadores_test.cpp con s3_jugadores.h.
    expect(playersHash([{ code: '0a1b2c3d', name: 'Alex', meta: 'Nv 2', title: '', photo: null }])).toBe('4b387986');
    expect(playersHash(LIST)).not.toBe(playersHash([LIST[0]]));
  });

  it('la foto es una imagen de LVGL RGB565 de 120 × 120', () => {
    expect(photo.length).toBe(PHOTO_BYTES);
    expect([photo[0], photo[1], photo[4], photo[6], photo[8]]).toEqual([0x19, 0x12, 120, 120, 240]);
  });

  it('entiende las respuestas de la placa', () => {
    expect(parsePlayersHash('JUGADORES 3 0A1B2C3D')).toBe('0a1b2c3d');
    expect(parsePhotosList('FOTOS 0a1b2c3d:11112222,deadbeef:33334444')?.get('deadbeef')).toBe('33334444');
    expect(parsePhotosList('FOTOS ')?.size).toBe(0);
  });

  it('pasa la lista y solo las fotos que faltan, en trozos con su confirmación', async () => {
    // Placa simulada: guarda lo que le llega y contesta como s3_jugadores.h.
    const board = { hash: '00000000', photos: new Map<string, string>(), lines: [] as string[] };
    let pending: string[] = [];
    let upload: { code: string; bytes: number[] } | null = null;
    const replies: string[] = [];
    const t: PlayersTransport = {
      send(line) {
        board.lines.push(line);
        const [w, a, b] = line.split(' ');
        if (w === 'JUGADORES?') replies.push(`JUGADORES ${pending.length} ${board.hash}`);
        else if (w === 'JUGADORES_INICIO') {
          pending = [];
          replies.push('JUGADORES_LISTO');
        } else if (w === 'JUGADOR') pending.push(line.slice(8));
        else if (w === 'JUGADORES_FIN') {
          board.hash = a;
          replies.push(`JUGADORES_GUARDADOS ${pending.length}`);
        } else if (w === 'FOTOS?') replies.push(`FOTOS ${[...board.photos].map(([c, k]) => `${c}:${k}`).join(',')}`);
        else if (w === 'FOTO_SUBIR') {
          upload = { code: a, bytes: [] };
          replies.push(`FOTO_LISTO ${a}`);
        } else if (w === 'FOTO_DATOS') {
          upload!.bytes.push(...Uint8Array.from(atob(a), (c) => c.charCodeAt(0)));
          replies.push(`FOTO_OK ${upload!.bytes.length}`);
        } else if (w === 'FOTO_FIN') {
          board.photos.set(upload!.code, a);
          replies.push(`FOTO_GUARDADA ${upload!.code}`);
        }
        void b;
      },
      async waitFor(match) {
        const i = replies.findIndex(match);
        if (i < 0) throw new Error(`sin respuesta (${replies.join(' / ')})`);
        return replies.splice(i, 1)[0];
      },
    };
    expect(await syncPlayers(t, LIST)).toBe(true);
    expect(board.hash).toBe(playersHash(LIST));
    expect(board.photos.get('0a1b2c3d')).toBe(crc32(photo).toString(16).padStart(8, '0'));
    expect(board.lines.filter((l) => l.startsWith('FOTO_DATOS')).length).toBe(Math.ceil(PHOTO_BYTES / 192));
    // Segunda vez: todo al día, no se manda nada más que las preguntas.
    board.lines = [];
    expect(await syncPlayers(t, LIST)).toBe(false);
    expect(board.lines).toEqual(['JUGADORES?', 'FOTOS?']);
  });
});
