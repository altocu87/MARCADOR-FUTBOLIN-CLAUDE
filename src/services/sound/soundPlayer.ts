/**
 * Toca el sonido de cada momento del partido según lo elegido en la mesa, y lo saca de donde esté guardado:
 * - «Este aparato»: de la copia ya cargada en memoria (sin esperas).
 * - «Chip» o «Tarjeta SD»: se le pide a la placa; si no está conectada o no lo puede tocar, suena el de siempre.
 * Si un sonido propio falta o falla, también suena el de siempre: un gol nunca se queda en silencio
 * (salvo que la mesa haya elegido «Ninguno»).
 */
import type { StoredSound } from '../persistence';
import { decodeWav } from './prepare';
import { sound, type PlayOptions, type SoundName } from './sound';
import { choiceFor, type SoundChoices, type SoundEventId } from './soundEvents';

/** Lo que suena si la mesa no ha elegido nada (los sonidos de la app de siempre). */
export const DEFAULT_SOUNDS: Record<SoundEventId, SoundName | 'anthem'> = {
  goal: 'goal',
  goalAnnulled: 'error',
  streak3: 'goal',
  comeback: 'goal',
  equalizer: 'goal',
  matchPoint: 'matchPoint',
  goldenGoal: 'goal',
  penaltyScored: 'goal',
  penaltyMissed: 'error',
  matchStart: 'countdownGo',
  periodEnd: 'periodEnd',
  victory: 'matchEnd',
  rout: 'matchEnd',
  tournamentWin: 'anthem',
  levelUp: 'levelUp',
  challenge: 'special',
};

/** La placa: toca un sonido guardado en ella. Devuelve false si no puede (no conectada, no lo tiene…). */
export interface BoardSoundTarget {
  play(sound: StoredSound): boolean;
}

/** Sin placa con altavoz todavía (fase 3): nunca puede, así que suena el de siempre. */
export const NO_BOARD: BoardSoundTarget = { play: () => false };

export type SoundPlan =
  | { kind: 'none' }
  | { kind: 'default' }
  | { kind: 'device'; sound: StoredSound }
  | { kind: 'board'; sound: StoredSound };

interface Loaded {
  meta: StoredSound;
  samples: Float32Array;
  rate: number;
}

export class SoundPlayer {
  private clips = new Map<string, Loaded>();
  private choices: SoundChoices = {};
  board: BoardSoundTarget = NO_BOARD;
  random: () => number = Math.random;

  /** Carga en memoria los sonidos de la mesa (al abrir la app y al subir o borrar uno). */
  setLibrary(list: StoredSound[]): void {
    const next = new Map<string, Loaded>();
    for (const s of list) {
      const prev = this.clips.get(s.id);
      if (prev && prev.meta.wav === s.wav) {
        next.set(s.id, { ...prev, meta: s });
        continue;
      }
      const d = decodeWav(s.wav);
      if (d) next.set(s.id, { meta: s, samples: d.samples, rate: d.rate });
    }
    this.clips = next;
  }

  setChoices(choices: SoundChoices | undefined): void {
    this.choices = choices ?? {};
  }

  /** Qué sonaría en ese momento y de dónde (elige al azar entre los propios que existan). */
  plan(id: SoundEventId): SoundPlan {
    const c = choiceFor(this.choices, id);
    if (c.kind === 'none') return { kind: 'none' };
    if (c.kind === 'default') return { kind: 'default' };
    const found = c.ids.map((x) => this.clips.get(x)).filter((x): x is Loaded => !!x);
    if (!found.length) return { kind: 'default' };
    const pick = found[Math.min(found.length - 1, Math.floor(this.random() * found.length))].meta;
    return pick.location === 'device' ? { kind: 'device', sound: pick } : { kind: 'board', sound: pick };
  }

  /** Toca el sonido de un momento del partido. */
  play(id: SoundEventId, opts: PlayOptions = {}): void {
    if (!sound.audible) return;
    const p = this.plan(id);
    if (p.kind === 'none') return;
    if (!opts.queue) sound.stopClip();
    if (p.kind === 'device') return this.playLocal(p.sound.id, opts);
    if (p.kind === 'board' && this.board.play(p.sound)) return;
    this.playDefault(id, opts);
  }

  playDefault(id: SoundEventId, opts: PlayOptions = {}): void {
    const d = DEFAULT_SOUNDS[id];
    if (d === 'anthem') sound.playAnthem('champion', opts);
    else sound.play(d, opts);
  }

  /** Escuchar un sonido de la biblioteca (siempre desde la copia del aparato). */
  preview(soundId: string): void {
    sound.unlock();
    this.playLocal(soundId);
  }

  private playLocal(soundId: string, opts: PlayOptions = {}): void {
    const c = this.clips.get(soundId);
    if (c) sound.playClip(c.samples, c.rate, opts);
  }
}

export const soundPlayer = new SoundPlayer();
