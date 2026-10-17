/**
 * Jugadores en la placa (pantalla de 7"): la app le pasa la lista (nombre, «Nv 23 · ELO 1299» y título) y la foto de
 * cada uno, ya preparada como imagen RGB565 de LVGL de 120 × 120 (lo que mide en la lista de la app), para que pueda elegir jugadores y enseñar sus fichas
 * jugando sola. Lo mismo que entiende `hardware/esp32s3_pantalla7/s3_jugadores.h`.
 */
import type { Player } from '../../services/persistence';
import { crc32, soundCode } from './boardSounds';

export const PHOTO_SIZE = 120;
/** Cabecera de imagen de LVGL 9 (12 bytes) + RGB565. */
export const PHOTO_BYTES = 12 + PHOTO_SIZE * PHOTO_SIZE * 2;
const CHUNK = 192;

export interface BoardPlayer {
  code: string;
  name: string;
  meta: string;
  title: string;
  /** Foto ya convertida para la placa (o null si no tiene). */
  photo: Uint8Array | null;
}

const hex8 = (n: number) => (n >>> 0).toString(16).padStart(8, '0');

/**
 * Quita el separador de campos y los saltos de línea (la placa lee «nombre|detalle|título» por línea) y recorta a
 * `max` bytes en UTF-8 sin partir letras (lo que cabe en la placa: 27, 39 y 31).
 */
function clean(s: string, max: number): string {
  let out = '';
  let bytes = 0;
  for (const ch of s.replace(/[|\r\n]/g, ' ').trim()) {
    const n = new TextEncoder().encode(ch).length;
    if (bytes + n > max) break;
    out += ch;
    bytes += n;
  }
  return out;
}

/** Línea canónica de un jugador: la misma que escribe la placa (para la huella). */
export function playerLine(p: BoardPlayer): string {
  const crc = p.photo ? hex8(crc32(p.photo)) : '-';
  return `${p.code} ${crc} ${clean(p.name, 27)}|${clean(p.meta, 39)}|${clean(p.title, 31)}`;
}

/** Huella de la lista (FNV-1a de las líneas canónicas con su salto de línea), como `hashOf()` de la placa. */
export function playersHash(list: BoardPlayer[]): string {
  let h = 2166136261;
  for (const p of list) {
    for (const b of new TextEncoder().encode(`${playerLine(p)}\n`)) {
      h ^= b;
      h = Math.imul(h, 16777619) >>> 0;
    }
  }
  return hex8(h);
}

export function boardPlayer(p: Player, meta: string, title: string | null, photo: Uint8Array | null): BoardPlayer {
  return { code: soundCode(p.id), name: p.name, meta, title: title ?? '', photo };
}

/**
 * Píxeles RGBA de 120 × 120 → imagen de LVGL RGB565 (cabecera: magia 0x19, formato 0x12, tamaño y paso), con un
 * tramado ordenado para que los degradados de la piel no hagan escalones.
 */
export function rgb565Image(rgba: Uint8ClampedArray | Uint8Array, w = PHOTO_SIZE, h = PHOTO_SIZE): Uint8Array {
  const out = new Uint8Array(12 + w * h * 2);
  const dv = new DataView(out.buffer);
  out[0] = 0x19;
  out[1] = 0x12;
  dv.setUint16(2, 0, true);
  dv.setUint16(4, w, true);
  dv.setUint16(6, h, true);
  dv.setUint16(8, w * 2, true);
  dv.setUint16(10, 0, true);
  const bayer = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];
  for (let y = 0; y < h; y += 1) {
    for (let x = 0; x < w; x += 1) {
      const i = (y * w + x) * 4;
      const t = bayer[(y & 3) * 4 + (x & 3)];
      const r = Math.min(255, rgba[i] + (t >> 1));
      const g = Math.min(255, rgba[i + 1] + (t >> 2));
      const b = Math.min(255, rgba[i + 2] + (t >> 1));
      dv.setUint16(12 + (y * w + x) * 2, ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3), true);
    }
  }
  return out;
}

/** Foto del jugador (data URL) → imagen para la placa: recortada al centro, cuadrada, a 120 × 120. Solo en el navegador. */
export async function photoForBoard(dataUrl: string): Promise<Uint8Array | null> {
  if (typeof document === 'undefined') return null;
  const img = new Image();
  img.src = dataUrl;
  try {
    await img.decode();
  } catch {
    return null;
  }
  const c = document.createElement('canvas');
  c.width = PHOTO_SIZE;
  c.height = PHOTO_SIZE;
  const g = c.getContext('2d');
  if (!g) return null;
  const side = Math.min(img.naturalWidth, img.naturalHeight);
  g.imageSmoothingQuality = 'high';
  g.drawImage(img, (img.naturalWidth - side) / 2, (img.naturalHeight - side) / 2, side, side, 0, 0, PHOTO_SIZE, PHOTO_SIZE);
  return rgb565Image(g.getImageData(0, 0, PHOTO_SIZE, PHOTO_SIZE).data);
}

function base64(bytes: Uint8Array): string {
  let bin = '';
  for (const b of bytes) bin += String.fromCharCode(b);
  return btoa(bin);
}

/** «FOTOS 1a2b3c4d:9f8e7d6c,…» → código → crc de las fotos que ya tiene la placa. */
export function parsePhotosList(line: string): Map<string, string> | null {
  const [w, rest = ''] = line.trim().split(/\s+/, 2);
  if (w?.toUpperCase() !== 'FOTOS') return null;
  const out = new Map<string, string>();
  for (const item of rest.split(',')) {
    const [code, crc] = item.split(':');
    if (code?.length === 8 && crc?.length === 8) out.set(code.toLowerCase(), crc.toLowerCase());
  }
  return out;
}

/** «JUGADORES 12 1a2b3c4d» → huella de la lista de la placa. */
export function parsePlayersHash(line: string): string | null {
  const m = /^JUGADORES\s+\d+\s+([0-9a-f]{8})$/i.exec(line.trim());
  return m ? m[1].toLowerCase() : null;
}

export interface PlayersTransport {
  send(line: string): void;
  /** Espera la primera línea que cumpla `match` (o falla al pasar `ms`). */
  waitFor(match: (line: string) => boolean, ms: number): Promise<string>;
}

export class BoardPlayersError extends Error {}

/**
 * Deja en la placa la misma lista y fotos que la app: si la huella coincide, no hace nada; si no, manda la lista y
 * después las fotos que le falten («FOTO_SUBIR», trozos «FOTO_DATOS» con su «FOTO_OK» y «FOTO_FIN» con el CRC).
 */
export async function syncPlayers(t: PlayersTransport, list: BoardPlayer[], onProgress?: (fraction: number) => void): Promise<boolean> {
  const want = playersHash(list);
  t.send('JUGADORES?');
  const have = parsePlayersHash(await t.waitFor((l) => /^JUGADORES\s/i.test(l), 3000));
  if (have !== want) {
    t.send(`JUGADORES_INICIO ${list.length}`);
    await t.waitFor((l) => /^JUGADORES_LISTO/i.test(l), 3000);
    for (const p of list) t.send(`JUGADOR ${playerLine(p)}`);
    t.send(`JUGADORES_FIN ${want}`);
    const r = await t.waitFor((l) => /^JUGADORES_(GUARDADOS|ERROR)/i.test(l), 5000);
    if (/ERROR/i.test(r)) throw new BoardPlayersError(`La placa no guardó los jugadores (${r.split(/\s+/)[1] ?? '?'})`);
  }
  t.send('FOTOS?');
  const photos = parsePhotosList(await t.waitFor((l) => /^FOTOS(\s|$)/i.test(l), 3000)) ?? new Map();
  const missing = list.filter((p) => p.photo && photos.get(p.code) !== hex8(crc32(p.photo)));
  let done = 0;
  const total = missing.reduce((n, p) => n + p.photo!.length, 0);
  for (const p of missing) {
    const bytes = p.photo!;
    t.send(`FOTO_SUBIR ${p.code} ${bytes.length}`);
    await expectOk(t, `FOTO_LISTO ${p.code}`, p.code);
    for (let i = 0; i < bytes.length; i += CHUNK) {
      const chunk = bytes.subarray(i, i + CHUNK);
      t.send(`FOTO_DATOS ${base64(chunk)}`);
      await expectOk(t, `FOTO_OK ${i + chunk.length}`, p.code);
      done += chunk.length;
      onProgress?.(done / total);
    }
    t.send(`FOTO_FIN ${hex8(crc32(bytes))}`);
    await expectOk(t, `FOTO_GUARDADA ${p.code}`, p.code);
  }
  return have !== want || missing.length > 0;
}

async function expectOk(t: PlayersTransport, ok: string, code: string): Promise<void> {
  const line = await t.waitFor((l) => l.trim().toLowerCase() === ok.toLowerCase() || /^FOTO_ERROR/i.test(l), 3000);
  if (/^FOTO_ERROR/i.test(line)) throw new BoardPlayersError(`La placa no guardó la foto de ${code} (${line.split(/\s+/)[2] ?? '?'})`);
}
