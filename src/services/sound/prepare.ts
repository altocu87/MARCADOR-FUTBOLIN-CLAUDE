/**
 * Preparar un sonido subido: una sola pista, sin silencios al principio ni al final, volumen igualado, como mucho
 * 10 s y en WAV ligero (22 050 Hz, 16 bits) que también puede tocar la placa. Todo son cálculos sobre las muestras,
 * sin depender del navegador; solo `decodeFile` usa Web Audio para leer el archivo subido.
 */

export const SOUND_RATE = 22_050;
export const MAX_SOUND_MS = 10_000;
/** Por debajo de esto se considera silencio (−46 dB). */
const SILENCE = 0.005;
/** Volumen medio buscado (−16 dB) sin que los picos pasen de −0,5 dB. */
const TARGET_RMS = 0.158;
const MAX_PEAK = 0.944;
const FADE_MS = 40;

/** Mezcla las pistas en una sola. */
export function toMono(channels: Float32Array[]): Float32Array {
  if (channels.length === 1) return channels[0];
  const len = Math.min(...channels.map((c) => c.length));
  const out = new Float32Array(len);
  for (const c of channels) for (let i = 0; i < len; i += 1) out[i] += c[i] / channels.length;
  return out;
}

/** Cambia la frecuencia de muestreo (interpolación lineal; suficiente para avisos de unos segundos). */
export function resample(samples: Float32Array, from: number, to: number): Float32Array {
  if (from === to) return samples;
  const len = Math.max(1, Math.round((samples.length * to) / from));
  const out = new Float32Array(len);
  const step = from / to;
  for (let i = 0; i < len; i += 1) {
    const x = i * step;
    const a = Math.floor(x);
    const b = Math.min(samples.length - 1, a + 1);
    const f = x - a;
    out[i] = (samples[Math.min(a, samples.length - 1)] ?? 0) * (1 - f) + (samples[b] ?? 0) * f;
  }
  return out;
}

/** Quita el silencio del principio y del final (deja unos milisegundos de margen). */
export function trimSilence(samples: Float32Array, rate: number): Float32Array {
  let start = 0;
  let end = samples.length - 1;
  while (start <= end && Math.abs(samples[start]) < SILENCE) start += 1;
  while (end >= start && Math.abs(samples[end]) < SILENCE) end -= 1;
  if (start > end) return new Float32Array(0);
  const pad = Math.round(rate * 0.01);
  return samples.slice(Math.max(0, start - pad), Math.min(samples.length, end + 1 + pad));
}

/** Iguala el volumen: todos los sonidos suenan parecido de fuertes. */
export function normalize(samples: Float32Array): Float32Array {
  let sum = 0;
  let peak = 0;
  for (const v of samples) {
    sum += v * v;
    peak = Math.max(peak, Math.abs(v));
  }
  if (!peak) return samples;
  const rms = Math.sqrt(sum / samples.length);
  const gain = Math.min(TARGET_RMS / rms, MAX_PEAK / peak);
  const out = new Float32Array(samples.length);
  for (let i = 0; i < samples.length; i += 1) out[i] = samples[i] * gain;
  return out;
}

/** Corta a la duración máxima con un final suave (sin chasquido). */
export function limitLength(samples: Float32Array, rate: number, maxMs = MAX_SOUND_MS): Float32Array {
  const max = Math.round((rate * maxMs) / 1000);
  const out = samples.length > max ? samples.slice(0, max) : samples.slice();
  const fade = Math.min(out.length, Math.round((rate * FADE_MS) / 1000));
  for (let i = 0; i < fade; i += 1) out[out.length - 1 - i] *= i / fade;
  return out;
}

/** Todo el proceso: de las pistas originales a una pista lista para guardar. */
export function prepareSamples(channels: Float32Array[], rate: number): Float32Array {
  const mono = resample(toMono(channels), rate, SOUND_RATE);
  return limitLength(normalize(trimSilence(mono, SOUND_RATE)), SOUND_RATE);
}

/** WAV de 16 bits y una pista (el formato más sencillo de tocar en la placa). */
export function encodeWav(samples: Float32Array, rate = SOUND_RATE): ArrayBuffer {
  const buf = new ArrayBuffer(44 + samples.length * 2);
  const v = new DataView(buf);
  const text = (at: number, s: string) => [...s].forEach((ch, i) => v.setUint8(at + i, ch.charCodeAt(0)));
  text(0, 'RIFF');
  v.setUint32(4, 36 + samples.length * 2, true);
  text(8, 'WAVE');
  text(12, 'fmt ');
  v.setUint32(16, 16, true);
  v.setUint16(20, 1, true); // PCM
  v.setUint16(22, 1, true); // una pista
  v.setUint32(24, rate, true);
  v.setUint32(28, rate * 2, true);
  v.setUint16(32, 2, true);
  v.setUint16(34, 16, true);
  text(36, 'data');
  v.setUint32(40, samples.length * 2, true);
  for (let i = 0; i < samples.length; i += 1) {
    const s = Math.max(-1, Math.min(1, samples[i]));
    v.setInt16(44 + i * 2, Math.round(s < 0 ? s * 0x8000 : s * 0x7fff), true);
  }
  return buf;
}

/** Lee un WAV guardado por `encodeWav` (para tocarlo sin esperas: no hace falta descodificar con el navegador). */
export function decodeWav(buf: ArrayBuffer): { samples: Float32Array; rate: number } | null {
  if (buf.byteLength < 44) return null;
  const v = new DataView(buf);
  const tag = (at: number) => String.fromCharCode(v.getUint8(at), v.getUint8(at + 1), v.getUint8(at + 2), v.getUint8(at + 3));
  if (tag(0) !== 'RIFF' || tag(8) !== 'WAVE' || v.getUint16(20, true) !== 1 || v.getUint16(22, true) !== 1 || v.getUint16(34, true) !== 16) return null;
  const rate = v.getUint32(24, true);
  const bytes = Math.min(v.getUint32(40, true), buf.byteLength - 44);
  const samples = new Float32Array(Math.floor(bytes / 2));
  for (let i = 0; i < samples.length; i += 1) samples[i] = v.getInt16(44 + i * 2, true) / 0x8000;
  return { samples, rate };
}

/** Lee el archivo subido (mp3, wav, ogg, m4a… lo que sepa el navegador) y lo prepara. */
export async function decodeFile(file: Blob): Promise<{ wav: ArrayBuffer; durationMs: number }> {
  const Ctor = window.OfflineAudioContext ?? (window as unknown as { webkitOfflineAudioContext?: typeof OfflineAudioContext }).webkitOfflineAudioContext;
  if (!Ctor) throw new Error('Este navegador no sabe leer sonidos');
  const ctx = new Ctor(1, 1, 44_100);
  const audio = await ctx.decodeAudioData(await file.arrayBuffer());
  const channels = Array.from({ length: audio.numberOfChannels }, (_, i) => audio.getChannelData(i));
  const samples = prepareSamples(channels, audio.sampleRate);
  if (!samples.length) throw new Error('El sonido está en silencio');
  return { wav: encodeWav(samples), durationMs: Math.round((samples.length / SOUND_RATE) * 1000) };
}
