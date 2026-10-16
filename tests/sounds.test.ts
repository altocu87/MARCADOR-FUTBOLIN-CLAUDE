import { describe, expect, it } from 'vitest';
import {
  DEFAULT_CONFIG,
  advance,
  createMatch,
  dispatch,
  getClock,
  getScore,
  matchPointTeams,
  type EngineCommand,
  type MatchConfig,
  type MatchState,
  type ParticipantRef,
  type Team,
} from '../src/match-engine';
import {
  applyImport,
  buildBackup,
  createLocalRepositories,
  normalizePreferences,
  parseBackup,
  previewImport,
  type StoredSound,
} from '../src/services/persistence';
import { memorySoundRepository } from '../src/services/sound/library';
import { decodeWav, encodeWav, limitLength, normalize, prepareSamples, SOUND_RATE, trimSilence } from '../src/services/sound/prepare';
import { normalizeSoundChoices, soundsForCommit, withoutSound, type SoundEventId } from '../src/services/sound/soundEvents';
import { SoundPlayer } from '../src/services/sound/soundPlayer';

const P2: ParticipantRef[] = [
  { playerId: 'a', team: 'white', slot: 1, nameSnapshot: 'Ana' },
  { playerId: 'b', team: 'blue', slot: 1, nameSnapshot: 'Beto' },
];

/** Partido simulado que apunta los sonidos de cada cambio, como el controlador del partido. */
class Sim {
  state: MatchState;
  t = 0;
  heard: SoundEventId[][] = [];
  private mpKey = '';
  constructor(over: Partial<MatchConfig> = {}) {
    this.state = createMatch('m', { ...DEFAULT_CONFIG, testMode: false, ...over }, P2, 0);
  }
  private apply(next: MatchState, events: Parameters<typeof soundsForCommit>[2]) {
    if (next === this.state) return;
    const sc = getScore(next);
    const mp = matchPointTeams(next).length ? `${next.period}:${sc.white}-${sc.blue}` : '';
    const isNew = !!mp && mp !== this.mpKey && next.period !== 'overtime';
    this.mpKey = mp;
    this.heard.push(soundsForCommit(this.state, next, events, isNew));
    this.state = next;
  }
  cmd(c: EngineCommand) {
    const out = dispatch(this.state, c, this.t);
    this.apply(out.state, out.events);
    return out;
  }
  wait(ms: number) {
    this.t += ms;
    const out = advance(this.state, this.t);
    this.apply(out.state, out.events);
    return this;
  }
  start() {
    this.cmd({ type: 'SKIP_COUNTDOWN' });
    return this;
  }
  /** Goles en orden («W» blanco, «B» azul), esperando el bloqueo; devuelve lo que sonó en cada uno. */
  goals(seq: string): SoundEventId[][] {
    return [...seq].map((ch) => {
      this.wait(3000);
      const team: Team = ch === 'W' ? 'white' : 'blue';
      this.heard = [];
      this.cmd({ type: 'GOAL', team });
      return this.heard.flat();
    });
  }
  last(): SoundEventId[] {
    return this.heard[this.heard.length - 1] ?? [];
  }
}

describe('Sonidos · momentos del partido', () => {
  it('empieza el partido al acabar la cuenta atrás', () => {
    const s = new Sim().start();
    expect(s.last()).toEqual(['matchStart']);
  });

  it('gol normal, empate y 3 seguidos', () => {
    const s = new Sim({ goalsPerPeriod: 10 }).start();
    const r = s.goals('WBWWW');
    expect(r[0]).toEqual(['goal']);
    expect(r[1]).toEqual(['equalizer']);
    expect(r[2]).toEqual(['goal']);
    expect(r[3]).toEqual(['goal']);
    expect(r[4]).toEqual(['streak3']);
  });

  it('el 4º seguido ya no es «3 seguidos»', () => {
    const s = new Sim({ goalsPerPeriod: 10 }).start();
    expect(s.goals('WWWW').map((x) => x[0])).toEqual(['goal', 'goal', 'streak3', 'goal']);
  });

  it('remontada solo al ponerse por delante tras ir 2 abajo (igualar es «empate»)', () => {
    const s = new Sim({ goalsPerPeriod: 10 }).start();
    const r = s.goals('BBWWW');
    expect(r[3]).toEqual(['equalizer']);
    expect(r[4]).toEqual(['comeback']);
  });

  it('la remontada gana a los 3 seguidos', () => {
    const s = new Sim({ goalsPerPeriod: 10 }).start();
    const r = s.goals('BBWW' + 'W');
    expect(r[4]).toEqual(['comeback']);
  });

  it('bola de partido suena en cola tras el gol', () => {
    const s = new Sim({ goalsPerPeriod: 3 }).start();
    const r = s.goals('WW');
    expect(r[1]).toEqual(['goal', 'matchPoint']);
  });

  it('victoria en lugar del gol que termina; goleada si el rival se queda a 0 o por 4', () => {
    const a = new Sim({ goalsPerPeriod: 5 }).start();
    expect(a.goals('BBWWWWW')[6]).toEqual(['victory']); // 5-2
    const b = new Sim({ goalsPerPeriod: 3 }).start();
    expect(b.goals('WWW')[2]).toEqual(['rout']); // 3-0
    const c = new Sim({ goalsPerPeriod: 5 }).start();
    expect(c.goals('BWWWWW')[5]).toEqual(['rout']); // 5-1
  });

  it('gol anulado (−1)', () => {
    const s = new Sim({ goalsPerPeriod: 10 }).start();
    s.goals('W');
    s.heard = [];
    s.cmd({ type: 'MINUS_ONE', team: 'white' });
    expect(s.heard.flat()).toEqual(['goalAnnulled']);
  });

  it('fin de la parte, gol de oro y victoria (por tiempo)', () => {
    const s = new Sim({ endCondition: 'time', minutesPerPeriod: 1 }).start();
    s.goals('W');
    s.heard = [];
    s.wait(getClock(s.state, s.t).remainingMs ?? 0);
    expect(s.heard.flat()).toContain('periodEnd');
    s.cmd({ type: 'CONTINUE' });
    s.start();
    s.goals('B');
    s.wait(getClock(s.state, s.t).remainingMs ?? 0);
    s.cmd({ type: 'CONTINUE' });
    expect(s.state.period).toBe('overtime');
    s.start();
    expect(s.goals('B')[0]).toEqual(['goldenGoal', 'victory']);
  });

  it('penaltis: marcado y fallado', () => {
    const s = new Sim({ endCondition: 'time', minutesPerPeriod: 1 }).start();
    s.wait(60_000).cmd({ type: 'CONTINUE' });
    s.start().wait(60_000).cmd({ type: 'CONTINUE' });
    s.start().wait(60_000).cmd({ type: 'CONTINUE' });
    expect(s.state.phase).toBe('penalties');
    s.heard = [];
    s.cmd({ type: 'PENALTY', team: 'white', scored: true });
    s.cmd({ type: 'PENALTY', team: 'blue', scored: false });
    expect(s.heard).toEqual([['penaltyScored'], ['penaltyMissed']]);
  });
});

const wav = (seconds: number, amp = 0.5) => {
  const n = Math.round(SOUND_RATE * seconds);
  const x = new Float32Array(n);
  for (let i = 0; i < n; i += 1) x[i] = amp * Math.sin((2 * Math.PI * 440 * i) / SOUND_RATE);
  return x;
};

describe('Sonidos · preparar al subir', () => {
  it('recorta silencios, iguala volumen y limita a 10 s', () => {
    const quiet = new Float32Array(SOUND_RATE); // 1 s de silencio
    const tone = wav(1, 0.05);
    const raw = new Float32Array(quiet.length * 2 + tone.length);
    raw.set(quiet, 0);
    raw.set(tone, quiet.length);
    const trimmed = trimSilence(raw, SOUND_RATE);
    expect(trimmed.length).toBeLessThan(tone.length * 1.05);
    const peak = Math.max(...normalize(tone).map(Math.abs));
    expect(peak).toBeGreaterThan(0.2);
    expect(peak).toBeLessThanOrEqual(0.95);
    expect(limitLength(wav(14), SOUND_RATE).length).toBe(SOUND_RATE * 10);
  });

  it('mezcla en una pista y cambia a 22 050 Hz', () => {
    const l = new Float32Array(44_100).fill(0.3);
    const r = new Float32Array(44_100).fill(0.1);
    const out = prepareSamples([l, r], 44_100);
    expect(Math.abs(out.length - SOUND_RATE)).toBeLessThan(500);
  });

  it('WAV de ida y vuelta', () => {
    const x = wav(0.5);
    const d = decodeWav(encodeWav(x))!;
    expect(d.rate).toBe(SOUND_RATE);
    expect(d.samples.length).toBe(x.length);
    expect(Math.abs(d.samples[100] - x[100])).toBeLessThan(0.001);
    expect(decodeWav(new ArrayBuffer(10))).toBeNull();
  });
});

const clip = (id: string, location: StoredSound['location'] = 'device'): StoredSound => ({
  id,
  name: id,
  location,
  durationMs: 500,
  createdAt: 1,
  wav: encodeWav(wav(0.5)),
});

describe('Sonidos · de dónde sale cada uno', () => {
  it('el de siempre, ninguno, propio del aparato o de la placa', () => {
    const p = new SoundPlayer();
    p.setLibrary([clip('a'), clip('b', 'sd'), clip('c', 'chip')]);
    p.setChoices({ goal: { kind: 'none', ids: [] }, victory: { kind: 'custom', ids: ['a'] }, rout: { kind: 'custom', ids: ['b'] }, levelUp: { kind: 'custom', ids: ['c'] } });
    expect(p.plan('equalizer').kind).toBe('default');
    expect(p.plan('goal').kind).toBe('none');
    expect(p.plan('victory')).toMatchObject({ kind: 'device', sound: { id: 'a' } });
    expect(p.plan('rout')).toMatchObject({ kind: 'board', sound: { id: 'b', location: 'sd' } });
    expect(p.plan('levelUp')).toMatchObject({ kind: 'board', sound: { id: 'c', location: 'chip' } });
  });

  it('varios al azar; si se borró, el de siempre', () => {
    const p = new SoundPlayer();
    p.setLibrary([clip('a'), clip('b')]);
    p.setChoices({ goal: { kind: 'custom', ids: ['a', 'b', 'x'] }, comeback: { kind: 'custom', ids: ['x'] } });
    p.random = () => 0.99;
    expect(p.plan('goal')).toMatchObject({ sound: { id: 'b' } });
    p.random = () => 0;
    expect(p.plan('goal')).toMatchObject({ sound: { id: 'a' } });
    expect(p.plan('comeback').kind).toBe('default');
  });

  it('borrar un sonido lo quita de los momentos', () => {
    const c = withoutSound({ goal: { kind: 'custom', ids: ['a'] }, rout: { kind: 'custom', ids: ['a', 'b'] } }, 'a');
    expect(c.goal).toEqual({ kind: 'default', ids: [] });
    expect(c.rout).toEqual({ kind: 'custom', ids: ['b'] });
  });

  it('las preferencias guardadas se limpian', () => {
    expect(normalizeSoundChoices({ goal: { kind: 'custom', ids: [] }, nope: { kind: 'none' }, rout: { kind: 'none', ids: [3, 'a'] } })).toEqual({
      goal: { kind: 'default', ids: [] },
      rout: { kind: 'none', ids: ['a'] },
    });
    expect(normalizePreferences({}).soundEvents).toEqual({});
  });
});

const memStore = () => {
  const m = new Map<string, string>();
  return { getItem: (k: string) => m.get(k) ?? null, setItem: (k: string, v: string) => void m.set(k, v), removeItem: (k: string) => void m.delete(k) };
};

describe('Sonidos · copia de seguridad', () => {
  it('se exportan y se importan (combinar y reemplazar)', async () => {
    const src = createLocalRepositories(memStore());
    const srcSounds = memorySoundRepository([clip('a'), clip('b', 'sd')]);
    const text = JSON.stringify(await buildBackup(src, new Date(), srcSounds));
    const parsed = parseBackup(text);
    expect(parsed.ok).toBe(true);
    if (!parsed.ok) return;
    const dst = createLocalRepositories(memStore());
    const dstSounds = memorySoundRepository([clip('a')]);
    expect((await previewImport(dst, parsed.backup, dstSounds)).sounds).toEqual({ total: 2, new: 1, existing: 1 });
    await applyImport(dst, parsed.backup, 'merge', dstSounds);
    const got = await dstSounds.list();
    expect(got.map((x) => x.id).sort()).toEqual(['a', 'b']);
    const b = got.find((x) => x.id === 'b')!;
    expect(b.location).toBe('sd');
    expect(decodeWav(b.wav)?.samples.length).toBe(Math.round(SOUND_RATE * 0.5));

    const other = memorySoundRepository([clip('z')]);
    await applyImport(dst, parsed.backup, 'replace', other);
    expect((await other.list()).map((x) => x.id).sort()).toEqual(['a', 'b']);
  });

  it('una copia antigua sin sonidos no borra los de la mesa', async () => {
    const parsed = parseBackup(JSON.stringify({ app: 'marcador-futbolin-v3', formatVersion: 1, players: [], matches: [] }));
    expect(parsed.ok).toBe(true);
    if (!parsed.ok) return;
    const sounds = memorySoundRepository([clip('a')]);
    await applyImport(createLocalRepositories(memStore()), parsed.backup, 'replace', sounds);
    expect((await sounds.list()).length).toBe(1);
  });
});
