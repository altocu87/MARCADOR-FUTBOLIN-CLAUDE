import { describe, expect, it } from 'vitest';
import type { Player, Tournament } from '../src/services/persistence';
import {
  buildProfileSnapshot,
  credentialsFor,
  fetchProfile,
  profileUrl,
  publishProfile,
  randomCode,
  readCredentials,
  snapshotHash,
  tokenFromLocation,
} from '../src/services/profiles';
import { makeMatch } from './helpers';

const player = (id: string, extra: Partial<Player> = {}): Player => ({ id, name: id.toUpperCase(), active: true, createdAt: 1, updatedAt: 1, ...extra });
const players = [player('ana', { photo: 'data:image/jpeg;base64,AAA' }), player('beto'), player('caro'), player('dani')];

function memoryStore() {
  const m = new Map<string, string>();
  return { getItem: (k: string) => m.get(k) ?? null, setItem: (k: string, v: string) => void m.set(k, v) };
}

describe('Perfil público del jugador', () => {
  const matches = [
    makeMatch({ white: ['ana'], blue: ['beto'], goals: 'WWB', at: 1000 }),
    makeMatch({ white: ['ana', 'caro'], blue: ['beto', 'dani'], goals: 'BBW', at: 2000 }),
    makeMatch({ white: ['beto'], blue: ['caro'], goals: 'WW', at: 3000 }),
  ];
  const torneo = {
    id: 't1', name: 'Liga del Salón', format: 'league', status: 'finished', finishedAt: 5000, createdAt: 10, winnerTeamId: 'x',
    teams: [{ id: 'x', name: 'Ana', playerIds: ['ana'] }, { id: 'y', name: 'Beto', playerIds: ['beto'] }], fixtures: [], cup: 'copa-generico-01',
  } as unknown as Tournament;

  const snap = buildProfileSnapshot({ player: players[0], players, matches, tournaments: [torneo], now: 9000 });

  it('cuenta sus partidos, los más recientes primero y vistos desde él', () => {
    expect(snap.stats.general.played).toBe(2);
    expect(snap.matches.map((m) => [m.result, m.mine, m.theirs])).toEqual([
      ['P', 1, 2],
      ['G', 2, 1],
    ]);
    expect(snap.matches[0].mates).toEqual(['caro']);
    expect(snap.matches[0].rivals).toEqual(['beto', 'dani']);
  });
  it('lleva nombre y foto de la gente que sale (para enseñar sus fotos)', () => {
    expect(snap.player.photo).toBe('data:image/jpeg;base64,AAA');
    expect(snap.people.beto.name).toBe('BETO');
    expect(snap.people.caro).toBeDefined();
  });
  it('palmarés: campeón de la liga con su copa', () => {
    expect(snap.palmares).toEqual([expect.objectContaining({ name: 'Liga del Salón', champion: true, cup: 'copa-generico-01' })]);
    const beto = buildProfileSnapshot({ player: players[1], players, matches, tournaments: [torneo], now: 9000 });
    expect(beto.palmares[0].champion).toBe(false);
  });
  it('la huella no cambia por la hora de cálculo, sí por un partido nuevo', () => {
    const again = buildProfileSnapshot({ player: players[0], players, matches, tournaments: [torneo], now: 99999 });
    expect(snapshotHash(again)).toBe(snapshotHash(snap));
    const more = buildProfileSnapshot({ player: players[0], players, matches: [...matches, makeMatch({ white: ['ana'], blue: ['dani'], goals: 'W', at: 4000 })], tournaments: [torneo], now: 9000 });
    expect(snapshotHash(more)).not.toBe(snapshotHash(snap));
  });
});

describe('Código, clave y enlace del perfil', () => {
  it('cada jugador tiene su código y su clave, y se conservan', () => {
    const store = memoryStore();
    const a = credentialsFor(store, 'ana');
    expect(a.token).toMatch(/^[A-Za-z0-9]{22}$/);
    expect(a.clave).toHaveLength(32);
    expect(credentialsFor(store, 'ana')).toEqual(a);
    expect(credentialsFor(store, 'beto').token).not.toBe(a.token);
    expect(Object.keys(readCredentials(store))).toEqual(['ana', 'beto']);
  });
  it('el enlace lleva el código tras «#» y la página lo sabe leer', () => {
    const url = profileUrl('AbCdEfGhJkMnPqRsTuVwXy', 'https://x.es/');
    expect(url).toBe('https://x.es/perfil/#AbCdEfGhJkMnPqRsTuVwXy');
    expect(tokenFromLocation('#AbCdEfGhJkMnPqRsTuVwXy')).toBe('AbCdEfGhJkMnPqRsTuVwXy');
    expect(tokenFromLocation('#corto')).toBeNull();
    expect(tokenFromLocation('', '?t=AbCdEfGhJkMnPqRsTuVwXy')).toBe('AbCdEfGhJkMnPqRsTuVwXy');
  });
  it('el código no usa letras que se confunden', () => {
    expect(randomCode(200)).not.toMatch(/[0O1lI]/);
  });
  it('publica y lee por las funciones del servidor', async () => {
    const calls: { url: string; body: Record<string, unknown> }[] = [];
    let saved: unknown = null;
    const http = (async (url: string, init?: RequestInit) => {
      const body = JSON.parse(String(init?.body));
      calls.push({ url, body });
      if (url.endsWith('publicar_perfil_jugador')) {
        saved = body.p_datos;
        return new Response(null, { status: 204 });
      }
      return new Response(JSON.stringify(saved), { status: 200 });
    }) as typeof fetch;
    const conn = { url: 'https://s.supabase.co', anonKey: 'k' };
    const snap = buildProfileSnapshot({ player: players[0], players, matches: [], tournaments: [], now: 1 });
    await publishProfile(conn, { token: 'T'.repeat(22), clave: 'C'.repeat(32) }, snap, http);
    expect(calls[0].url).toBe('https://s.supabase.co/rest/v1/rpc/publicar_perfil_jugador');
    expect(calls[0].body.p_clave).toBe('C'.repeat(32));
    expect((await fetchProfile(conn, 'T'.repeat(22), http))?.player.name).toBe('ANA');
    const none = (async () => new Response('null', { status: 200 })) as typeof fetch;
    expect(await fetchProfile(conn, 'T'.repeat(22), none)).toBeNull();
  });
});
