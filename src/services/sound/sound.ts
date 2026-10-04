/**
 * Sonidos sintetizados en local con Web Audio (sin archivos ni licencias externas).
 * Solo reproduce eventos ya aceptados por el motor. Si el navegador no permite
 * audio, falla en silencio: el partido no depende del sonido.
 */
import type { GoalSoundId } from '../persistence';

export type SoundName =
  | 'goal'
  | 'periodEnd'
  | 'matchEnd'
  | 'ui'
  | 'countdown'
  | 'countdownGo'
  | 'error'
  | 'special'
  | 'whoosh'
  | 'levelUp'
  | 'matchPoint'
  | 'siren';

interface Note {
  freq: number;
  start: number;
  dur: number;
  type?: OscillatorType;
  gain?: number;
  slideTo?: number;
}

/** Variantes de sonido de gol seleccionables en Ajustes. */
export const GOAL_SOUNDS: Record<GoalSoundId, { label: string; notes: Note[] }> = {
  arcade: {
    label: 'Arcade',
    notes: [
      { freq: 523, start: 0, dur: 0.12, type: 'square', gain: 0.5 },
      { freq: 659, start: 0.1, dur: 0.12, type: 'square', gain: 0.5 },
      { freq: 784, start: 0.2, dur: 0.12, type: 'square', gain: 0.5 },
      { freq: 1047, start: 0.3, dur: 0.35, type: 'sawtooth', gain: 0.4 },
    ],
  },
  laser: {
    label: 'Láser',
    notes: [
      { freq: 1800, start: 0, dur: 0.25, type: 'sawtooth', gain: 0.4, slideTo: 200 },
      { freq: 1800, start: 0.22, dur: 0.25, type: 'sawtooth', gain: 0.35, slideTo: 300 },
      { freq: 880, start: 0.45, dur: 0.3, type: 'square', gain: 0.3 },
    ],
  },
  stadium: {
    label: 'Estadio',
    notes: [
      { freq: 233, start: 0, dur: 0.6, type: 'sawtooth', gain: 0.45 },
      { freq: 294, start: 0, dur: 0.6, type: 'sawtooth', gain: 0.35 },
      { freq: 349, start: 0, dur: 0.6, type: 'sawtooth', gain: 0.3 },
      { freq: 466, start: 0.6, dur: 0.5, type: 'sawtooth', gain: 0.45 },
    ],
  },
  retro: {
    label: 'Retro 8 bits',
    notes: [
      { freq: 392, start: 0, dur: 0.07, type: 'square', gain: 0.4 },
      { freq: 523, start: 0.07, dur: 0.07, type: 'square', gain: 0.4 },
      { freq: 659, start: 0.14, dur: 0.07, type: 'square', gain: 0.4 },
      { freq: 784, start: 0.21, dur: 0.07, type: 'square', gain: 0.4 },
      { freq: 1047, start: 0.28, dur: 0.07, type: 'square', gain: 0.4 },
      { freq: 1319, start: 0.35, dur: 0.25, type: 'square', gain: 0.4 },
    ],
  },
};

const melody = (notes: [number, number][], step = 0.16, type: OscillatorType = 'triangle'): Note[] => {
  let t = 0;
  return notes.map(([freq, beats]) => {
    const n = { freq, start: t, dur: beats * step * 0.95, type, gain: 0.55 };
    t += beats * step;
    return n;
  });
};

/** Melodías de celebración por jugador. */
export const ANTHEMS: Record<string, { label: string; notes: Note[] }> = {
  fanfare: { label: 'Fanfarria', notes: melody([[523, 1], [523, 1], [523, 1], [698, 3], [880, 1], [784, 1], [1047, 4]]) },
  heroic: { label: 'Épica', notes: melody([[392, 2], [523, 1], [587, 1], [659, 2], [587, 1], [659, 1], [784, 4]], 0.15, 'sawtooth') },
  funky: { label: 'Funky', notes: melody([[330, 1], [0, 1], [392, 1], [440, 1], [0, 1], [494, 1], [587, 2], [494, 1], [587, 3]], 0.13, 'square') },
  champion: { label: 'Campeón', notes: melody([[523, 2], [659, 2], [784, 2], [1047, 3], [988, 1], [1047, 4]]) },
  arcade: { label: 'Arcade', notes: melody([[659, 1], [784, 1], [988, 1], [1319, 1], [988, 1], [1319, 3]], 0.1, 'square') },
};

const PATTERNS: Record<Exclude<SoundName, 'goal'>, Note[]> = {
  periodEnd: [
    { freq: 880, start: 0, dur: 0.25, type: 'triangle' },
    { freq: 660, start: 0.3, dur: 0.45, type: 'triangle' },
  ],
  matchEnd: [
    { freq: 523, start: 0, dur: 0.18, type: 'triangle' },
    { freq: 659, start: 0.18, dur: 0.18, type: 'triangle' },
    { freq: 784, start: 0.36, dur: 0.18, type: 'triangle' },
    { freq: 1047, start: 0.54, dur: 0.6, type: 'triangle' },
  ],
  ui: [{ freq: 1200, start: 0, dur: 0.04, type: 'sine', gain: 0.25 }],
  countdown: [{ freq: 660, start: 0, dur: 0.12, type: 'sine', gain: 0.5 }],
  countdownGo: [{ freq: 1320, start: 0, dur: 0.3, type: 'sine', gain: 0.5 }],
  error: [{ freq: 200, start: 0, dur: 0.15, type: 'square', gain: 0.25, slideTo: 140 }],
  special: [{ freq: 300, start: 0, dur: 0.5, type: 'sawtooth', gain: 0.35, slideTo: 1200 }],
  whoosh: [{ freq: 120, start: 0, dur: 0.35, type: 'sawtooth', gain: 0.2, slideTo: 900 }],
  levelUp: melody([[523, 1], [659, 1], [784, 1], [1047, 1], [1319, 3]], 0.09, 'square'),
  // Sirena de alarma (sube y baja) durante el robo de la ardilla, unos 4,5 s.
  siren: Array.from({ length: 12 }, (_, i) => ({
    freq: i % 2 ? 1150 : 620,
    start: i * 0.38,
    dur: 0.4,
    type: 'sawtooth' as OscillatorType,
    gain: 0.22,
    slideTo: i % 2 ? 620 : 1150,
  })),
  matchPoint: [
    { freq: 880, start: 0, dur: 0.12, type: 'square', gain: 0.35 },
    { freq: 880, start: 0.18, dur: 0.12, type: 'square', gain: 0.35 },
  ],
};

class SoundService {
  private ctx: AudioContext | null = null;
  private volume = 0.7;
  private muted = false;
  private goalSound: GoalSoundId = 'arcade';

  configure(volume: number, muted: boolean, goalSound: GoalSoundId = 'arcade'): void {
    this.volume = Math.min(1, Math.max(0, volume));
    this.muted = muted;
    this.goalSound = goalSound;
  }

  /** Debe llamarse tras una interacción del usuario (política de autoplay). */
  unlock(): void {
    try {
      if (!this.ctx) {
        const Ctor = window.AudioContext ?? (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
        if (!Ctor) return;
        this.ctx = new Ctor();
      }
      if (this.ctx.state === 'suspended') void this.ctx.resume();
    } catch {
      this.ctx = null;
    }
  }

  play(name: SoundName): void {
    this.playNotes(name === 'goal' ? GOAL_SOUNDS[this.goalSound].notes : PATTERNS[name]);
  }

  playGoalVariant(id: GoalSoundId): void {
    this.playNotes(GOAL_SOUNDS[id].notes);
  }

  playAnthem(id: string | undefined): void {
    this.playNotes((ANTHEMS[id ?? ''] ?? ANTHEMS.fanfare).notes);
  }

  private playNotes(notes: Note[]): void {
    if (this.muted || this.volume <= 0) return;
    this.unlock();
    const ctx = this.ctx;
    if (!ctx) return;
    try {
      const t0 = ctx.currentTime + 0.01;
      const master = ctx.createGain();
      master.gain.value = this.volume * 0.35;
      master.connect(ctx.destination);
      for (const n of notes) {
        if (n.freq <= 0) continue; // silencio
        const osc = ctx.createOscillator();
        const g = ctx.createGain();
        osc.type = n.type ?? 'sine';
        osc.frequency.setValueAtTime(n.freq, t0 + n.start);
        if (n.slideTo) osc.frequency.exponentialRampToValueAtTime(n.slideTo, t0 + n.start + n.dur);
        const peak = n.gain ?? 0.6;
        g.gain.setValueAtTime(0.0001, t0 + n.start);
        g.gain.exponentialRampToValueAtTime(peak, t0 + n.start + 0.01);
        g.gain.exponentialRampToValueAtTime(0.0001, t0 + n.start + n.dur);
        osc.connect(g).connect(master);
        osc.start(t0 + n.start);
        osc.stop(t0 + n.start + n.dur + 0.02);
      }
    } catch {
      // El audio es un servicio independiente: un fallo nunca afecta al partido.
    }
  }
}

export const sound = new SoundService();
