import { describe, expect, it } from 'vitest';
import type { Handicap } from '../src/match-engine';
import { CloudVoices, memoryClipRepository, syncCloudSounds } from '../src/services/sound/cloudSounds';
import {
  affected,
  avisoVoice,
  countVoice,
  endingVoice,
  hcCode,
  nameOnly,
  nameVoice,
  pitidoCode,
  pitidoDue,
  revealVoices,
  turnVoice,
  type Who,
} from '../src/services/sound/handicapVoices';

const ALL = ['A1', 'A2', 'A3', 'Q1', 'Q2', 'Q3', 'Q4', 'F01', 'F02', 'F03', 'F04', 'F05', 'F06', 'F07a', 'F07b', 'F07c',
  'F07d', 'F08a', 'F08b', 'F08c', 'F08d', 'F08e', 'F08f', 'F08g', 'F08h', 'F09a', 'F09b', 'F10a', 'F10b', 'F11', 'F12',
  'F13', 'R1a', 'R1b', 'R1c', 'R1d', 'R4a', 'R4b', 'R4c', 'R4d', 'R5a', 'R5b', 'R5c', 'R5d', 'C1', 'C2', 'C3', 'N_vicky',
  'N_alex', 'N_lucia', 'N_alvaro'];
const present = (list = ALL) => {
  const set = new Set(list.map(hcCode));
  return (c: string) => set.has(c);
};
const C = hcCode;
const hc = (kind: Handicap['kind'], team: Handicap['team'], extra: Partial<Handicap> = {}): Handicap => ({
  id: 'h',
  kind,
  team,
  durationMs: 15000,
  ...extra,
});
const vicky: Who = { name: 'Vicky', gender: 'f' };
const alex: Who = { name: 'Alex', gender: 'm' };
const lucia: Who = { name: 'Lucía', gender: 'f' };

// Los mismos casos que hardware/test/s3_voces_hc_test.cpp: la app dice lo mismo que la pantalla.
describe('voces grabadas: iguales que la pantalla', () => {
  it('códigos y nombres', () => {
    expect(C('A1')).toBe('5280fb10');
    expect(C('F01')).toBe('86a50327');
    expect(C('N_alex')).toBe('53d8fa09');
    expect(C('Q4')).toBe('d75929ff');
    expect(C('F08h')).toBe('5ead04fc');
    expect(C('R5d')).toBe('45ab4449');
    expect(C('C1')).toBe('e67bd3de');
    expect(pitidoCode()).toBe('194809f5');
    expect(nameVoice('Álvaro')).toBe('N_alvaro');
    expect(C(nameVoice('Álvaro'))).toBe('3c7067f6');
    expect(nameVoice('Lucía')).toBe('N_lucia');
    expect(nameVoice(' María José ')).toBe('N_maria_jose');
    expect(nameVoice('NÚÑEZ')).toBe('N_nunez');
  });

  it('quién + la frase de cada hándicap', () => {
    const has = present();
    const r = (h: Handicap, w?: Who, b?: Who) => revealVoices(h, w, b, has);
    expect(r(hc('double_all', 'both'))).toEqual([C('F01')]);
    expect(r(hc('double_team', 'blue'))).toEqual([C('Q2'), C('F02')]);
    expect(r(hc('steal', 'white', { durationMs: null }))).toEqual([C('Q1'), C('F04')]);
    expect(r(hc('swap_positions', 'both', { durationMs: null }))).toEqual([C('Q3'), C('F06')]);
    expect(r(hc('frozen_player', 'white'), vicky)).toEqual([C('N_vicky'), C('F07b')]);
    expect(r(hc('frozen_player', 'blue'), undefined, alex)).toEqual([C('N_alex'), C('F07a')]);
    expect(r(hc('frozen_player', 'both'), vicky, alex)).toEqual([C('N_vicky'), C('Q4'), C('N_alex'), C('F07c')]);
    expect(r(hc('frozen_player', 'both'), vicky, lucia)).toEqual([C('N_vicky'), C('Q4'), C('N_lucia'), C('F07d')]);
    expect(r(hc('bar_lock', 'both', { bar: 'defensa' }), vicky, alex)).toEqual([C('N_vicky'), C('Q4'), C('N_alex'), C('F08f')]);
    expect(r(hc('bar_lock', 'blue', { bar: 'delantero' }), undefined, alex)).toEqual([C('N_alex'), C('F08d')]);
    expect(r(hc('penalty', 'blue', { durationMs: null }))).toEqual([C('F09b')]);
    expect(r(hc('transfer', 'both', { durationMs: 30000 }), vicky, alex)).toEqual([C('N_vicky'), C('F10a'), C('N_alex'), C('F10b')]);
    expect(r(hc('one_hand', 'both', { durationMs: 20000 }))).toEqual([C('F13')]);
    // Sin su nombre grabado dice su equipo; sin la frase, nada.
    expect(r(hc('frozen_player', 'blue'), undefined, { name: 'Sergio', gender: 'm' })).toEqual([C('Q2'), C('F07a')]);
    expect(revealVoices(hc('frozen_player', 'blue'), undefined, alex, present(ALL.filter((v) => v !== 'F07a')))).toEqual([]);
    expect(r(hc('weak_hand', 'both'))).toEqual([]);
  });

  it('finales, cuenta atrás y aviso', () => {
    const has = present();
    for (const rnd of [0, 0.3, 0.6, 0.99]) {
      expect(['R1a', 'R1b', 'R1c', 'R1d'].map(C)).toContain(endingVoice(hc('double_all', 'both', { durationMs: 30000 }), has, rnd));
      expect(['R4a', 'R4b', 'R4c', 'R4d'].map(C)).toContain(endingVoice(hc('swap_positions', 'both', { durationMs: null }), has, rnd));
      expect(['R5a', 'R5b', 'R5c', 'R5d'].map(C)).toContain(endingVoice(hc('transfer', 'both', { durationMs: 30000 }), has, rnd));
      expect(['A1', 'A2', 'A3'].map(C)).toContain(avisoVoice(has, rnd));
    }
    expect(endingVoice(hc('triple_next', 'both', { durationMs: null }), has)).toBeNull();
    expect(countVoice(3, has)).toBe(C('C3'));
    expect(countVoice(1, has)).toBe(C('C1'));
    expect(countVoice(0, has)).toBeNull();
    const none = present([]);
    expect(avisoVoice(none)).toBeNull();
    expect(countVoice(2, none)).toBeNull();
  });

  it('elegir jugadores: equipo y nombre (sin nombre grabado, nada)', () => {
    const has = present();
    expect(turnVoice('white', has)).toBe(C('Q1'));
    expect(turnVoice('blue', has)).toBe(C('Q2'));
    expect(nameOnly('Álvaro', has)).toBe(C('N_alvaro'));
    expect(nameOnly('  Lucía', has)).toBe(C('N_lucia'));
    expect(nameOnly('José', has)).toBeNull();
    expect(nameOnly('', has)).toBeNull();
    expect(turnVoice('white', present([]))).toBeNull();
  });

  it('afectados por equipo, con chico o chica de su ficha', () => {
    const participants = [
      { playerId: 'v', team: 'white' as const, slot: 1 as const, nameSnapshot: 'Vicky' },
      { playerId: 'a', team: 'blue' as const, slot: 1 as const, nameSnapshot: 'Alex' },
    ];
    const got = affected(hc('frozen_player', 'both', { players: ['v', 'a'] }), { participants }, (id) => (id === 'v' ? 'f' : 'm'));
    expect(got).toEqual({ white: { name: 'Vicky', gender: 'f' }, blue: { name: 'Alex', gender: 'm' } });
  });

  it('pitido: fin de cada parte, prórroga y partido; no al acabar los penaltis', () => {
    expect(pitidoDue({ phase: 'playing' }, { phase: 'periodEnd', period: 'first' })).toBe(true);
    expect(pitidoDue({ phase: 'playing' }, { phase: 'finished', period: 'second' })).toBe(true);
    expect(pitidoDue({ phase: 'playing' }, { phase: 'finished', period: 'overtime' })).toBe(true);
    expect(pitidoDue({ phase: 'penalties' }, { phase: 'finished', period: 'shootout' })).toBe(false);
    expect(pitidoDue({ phase: 'playing' }, { phase: 'playing', period: 'first' })).toBe(false);
  });
});

describe('sonidos de la nube en el aparato', () => {
  const mp3 = (n: number) => new Uint8Array(n).fill(7).buffer;
  const cloud = (files: Record<string, { bytes: number; ms: number; mp3Sha256: string }>, broken = new Set<string>()) => {
    const calls: string[] = [];
    const http = (async (url: string, init?: RequestInit) => {
      calls.push(url);
      expect((init?.headers as Record<string, string>).Authorization).toBe('Bearer tok');
      if (url.endsWith('/indice.json')) return new Response(JSON.stringify({ version: 1, updatedAt: 1, files }));
      const code = url.split('/').pop()!.replace('.mp3', '');
      if (!files[code]) return new Response('', { status: 404 });
      return new Response(mp3(broken.has(code) ? 3 : files[code].bytes));
    }) as typeof fetch;
    return { http, calls };
  };
  const opts = { url: 'https://x.supabase.co/', anonKey: 'anon', token: 'tok' };

  it('baja lo nuevo, comprueba el tamaño, no repite y borra lo que ya no está', async () => {
    const repo = memoryClipRepository();
    const files = { aaaaaaaa: { bytes: 10, ms: 500, mp3Sha256: 's1' }, bbbbbbbb: { bytes: 20, ms: 900, mp3Sha256: 's2' } };
    const a = cloud(files, new Set(['bbbbbbbb']));
    expect(await syncCloudSounds({ ...opts, repo, http: a.http })).toEqual({ total: 2, downloaded: 1, removed: 0, failed: 1 });
    expect(a.calls[0]).toBe('https://x.supabase.co/storage/v1/object/authenticated/sonidos/indice.json');
    expect([...(await repo.codes()).keys()]).toEqual(['aaaaaaaa']);
    // La segunda vez solo lo que faltaba; luego, con uno quitado de la nube, se borra aquí.
    const b = cloud(files);
    expect(await syncCloudSounds({ ...opts, repo, http: b.http })).toEqual({ total: 2, downloaded: 1, removed: 0, failed: 0 });
    expect(b.calls.filter((u) => u.endsWith('.mp3'))).toEqual(['https://x.supabase.co/storage/v1/object/authenticated/sonidos/bbbbbbbb.mp3']);
    const c = cloud({ bbbbbbbb: files.bbbbbbbb });
    expect(await syncCloudSounds({ ...opts, repo, http: c.http })).toEqual({ total: 1, downloaded: 0, removed: 1, failed: 0 });
    expect([...(await repo.codes()).keys()]).toEqual(['bbbbbbbb']);
  });

  it('sin nada subido todavía, no es un error', async () => {
    const http = (async () => new Response('{}', { status: 400 })) as unknown as typeof fetch;
    expect(await syncCloudSounds({ ...opts, repo: memoryClipRepository(), http })).toEqual({ total: 0, downloaded: 0, removed: 0, failed: 0 });
  });

  it('las voces cortas quedan preparadas y se sabe cuánto duran', async () => {
    const repo = memoryClipRepository();
    await repo.put({ code: pitidoCode(), sha: 'p', data: mp3(4) });
    await repo.put({ code: 'cccccccc', sha: 'c', data: mp3(4) });
    const v = new CloudVoices();
    v.decode = async () => ({ samples: new Float32Array(24000), rate: 16000 });
    await v.load(repo, [pitidoCode(), C('Q1')]);
    expect(v.count).toBe(2);
    expect(v.has(pitidoCode())).toBe(true);
    expect(v.has(C('Q1'))).toBe(false);
    expect(v.durationMs(pitidoCode())).toBe(1500);
    expect(v.durationMs('cccccccc')).toBe(0); // no se prepara hasta que se pide
    await v.prepare('cccccccc');
    expect(v.durationMs('cccccccc')).toBe(1500);
  });
});
