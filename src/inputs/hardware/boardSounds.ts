/**
 * Sonidos propios en la placa (pantalla de 7" con altavoces): órdenes del protocolo para pasarle sonidos, saber cuáles
 * tiene, borrarlos, tocarlos y decirle qué suena en cada momento cuando juega sola. Lo mismo que entiende
 * `hardware/common/mfv3_sound.h`. Cada sonido se conoce en la placa por un código de 8 cifras (FNV-1a de su id).
 */
import type { SoundLocation, StoredSound } from '../../services/persistence';
import { choiceFor, type SoundChoices, type SoundEventId } from '../../services/sound/soundEvents';

export type BoardPlace = Exclude<SoundLocation, 'device'>;

/** Código del sonido en la placa (el mismo cálculo que `soundCode()` del firmware). */
export function soundCode(id: string): string {
  let h = 2166136261;
  for (const b of new TextEncoder().encode(id)) {
    h ^= b;
    h = Math.imul(h, 16777619) >>> 0;
  }
  return h.toString(16).padStart(8, '0');
}

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n += 1) {
    let c = n;
    for (let k = 0; k < 8; k += 1) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();

export function crc32(bytes: Uint8Array): number {
  let c = 0xffffffff;
  for (const b of bytes) c = CRC_TABLE[(c ^ b) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

const hex8 = (n: number) => (n >>> 0).toString(16).padStart(8, '0');

/** Bytes por línea de datos (192 → 256 caracteres en base64; la placa admite líneas de hasta 383). */
export const CHUNK_BYTES = 192;

function base64(bytes: Uint8Array): string {
  let bin = '';
  for (const b of bytes) bin += String.fromCharCode(b);
  return btoa(bin);
}

/** Momentos que la placa detecta ella sola en sus partidos (los de torneos, niveles y retos son de la app). */
export const BOARD_EVENTS: SoundEventId[] = [
  'goal',
  'goalAnnulled',
  'streak3',
  'comeback',
  'equalizer',
  'matchPoint',
  'goldenGoal',
  'penaltyScored',
  'penaltyMissed',
  'matchStart',
  'periodEnd',
  'victory',
  'rout',
];

/**
 * Qué debe tocar la placa en cada momento cuando juega sola: los sonidos de ese momento que están guardados en ella
 * (los del aparato no los tiene: si no queda ninguno, el de siempre).
 */
export function momentLines(choices: SoundChoices | undefined, sounds: StoredSound[]): string[] {
  const where = new Map(sounds.map((s) => [s.id, s.location]));
  return BOARD_EVENTS.map((ev) => {
    const c = choiceFor(choices, ev);
    if (c.kind === 'none') return `MOMENTO ${ev} no`;
    const codes = c.kind === 'custom' ? c.ids.filter((id) => (where.get(id) ?? 'device') !== 'device').slice(0, 4).map(soundCode) : [];
    return `MOMENTO ${ev} ${codes.length ? codes.join(',') : 'def'}`;
  });
}

export interface BoardInventory {
  /** KB libres; null si la placa no tiene ese sitio (sin tarjeta, sin memoria). */
  chipFreeKB: number | null;
  sdFreeKB: number | null;
  /** Código → dónde está. */
  sounds: Map<string, BoardPlace>;
}

/** «SONIDOS chip=9216 sd=- 1a2b3c4d@c,0000beef@s» */
export function parseSoundsList(line: string): BoardInventory | null {
  const parts = line.trim().split(/\s+/);
  if (parts[0]?.toUpperCase() !== 'SONIDOS') return null;
  const inv: BoardInventory = { chipFreeKB: null, sdFreeKB: null, sounds: new Map() };
  for (const p of parts.slice(1)) {
    const kv = /^(chip|sd)=(\d+|-)$/.exec(p);
    if (kv) {
      const v = kv[2] === '-' ? null : Number(kv[2]);
      if (kv[1] === 'chip') inv.chipFreeKB = v;
      else inv.sdFreeKB = v;
      continue;
    }
    for (const item of p.split(',')) {
      const m = /^([0-9a-f]{8})@([cs])$/i.exec(item);
      if (m) inv.sounds.set(m[1].toLowerCase(), m[2].toLowerCase() === 's' ? 'sd' : 'chip');
    }
  }
  return inv;
}

/** Envío de líneas a la placa y espera de su respuesta. */
export interface BoardTransport {
  send(line: string): void;
  /** Espera la primera línea que cumpla `match` (rechaza si pasa `timeoutMs`). */
  waitFor(match: (line: string) => boolean, timeoutMs: number): Promise<string>;
}

const REASONS: Record<string, string> = {
  sin_sd: 'La placa no tiene tarjeta SD',
  sin_memoria: 'La memoria del chip no está preparada (revisa el esquema de particiones)',
  sin_espacio: 'No queda sitio en la placa',
  tamano: 'El sonido es demasiado grande',
  escritura: 'La placa no pudo guardarlo',
  crc: 'El sonido llegó dañado; inténtalo otra vez',
  tiempo: 'La placa dejó de recibir; inténtalo otra vez',
};

export class BoardSoundError extends Error {}

/**
 * Pasa un sonido a la placa: «SONIDO_SUBIR», trozos «SONIDO_DATOS» (cada uno espera su «SONIDO_OK») y «SONIDO_FIN»
 * con el CRC32. `onProgress` recibe de 0 a 1.
 */
export async function uploadSound(
  t: BoardTransport,
  sound: StoredSound,
  place: BoardPlace,
  onProgress: (fraction: number) => void = () => undefined,
  timeoutMs = 5000,
): Promise<void> {
  const code = soundCode(sound.id);
  const bytes = new Uint8Array(sound.wav);
  const isMine = (l: string) => {
    const [w, c] = l.trim().split(/\s+/);
    return /^SONIDO_(LISTO|OK|GUARDADO|ERROR)$/i.test(w) && (w.toUpperCase() === 'SONIDO_OK' || c === code || c === '-');
  };
  const expect = async (want: string, line: string): Promise<string> => {
    t.send(line);
    let reply: string;
    try {
      reply = await t.waitFor(isMine, timeoutMs);
    } catch {
      throw new BoardSoundError('La placa no contesta');
    }
    const parts = reply.trim().split(/\s+/);
    if (parts[0].toUpperCase() === 'SONIDO_ERROR') throw new BoardSoundError(REASONS[parts[2]] ?? 'La placa no pudo guardarlo');
    if (parts[0].toUpperCase() !== want) throw new BoardSoundError('Respuesta inesperada de la placa');
    return reply;
  };
  await expect('SONIDO_LISTO', `SONIDO_SUBIR ${place} ${code} ${bytes.length}`);
  onProgress(0);
  for (let i = 0; i < bytes.length; i += CHUNK_BYTES) {
    const chunk = bytes.subarray(i, Math.min(bytes.length, i + CHUNK_BYTES));
    await expect('SONIDO_OK', `SONIDO_DATOS ${base64(chunk)}`);
    onProgress(Math.min(1, (i + chunk.length) / bytes.length));
  }
  await expect('SONIDO_GUARDADO', `SONIDO_FIN ${hex8(crc32(bytes))}`);
}
