/** Cuenta del dueño de la mesa y sincronización con la nube (Supabase simulado: nada contra la base real). */
import { describe, expect, it } from 'vitest';
import {
  emptyState,
  fingerprint,
  freshSession,
  readSession,
  restApi,
  signIn,
  signUp,
  stableStringify,
  syncOnce,
  writeSession,
  type CloudApi,
  type CloudRow,
  type MesaData,
} from '../src/services/cloud';
import { DEFAULT_PREFERENCES, createMemoryStore, type Player, type StoredMatch } from '../src/services/persistence';

const conn = { url: 'https://nube.test', anonKey: 'anon' };

/** Nube en memoria: guarda las filas de un dueño y les pone la hora del servidor (que siempre avanza). */
function fakeCloud() {
  const rows = new Map<string, CloudRow>();
  let clock = 0;
  const stamp = () => `2026-10-19T10:00:${String(clock++).padStart(4, '0')}Z`;
  const api: CloudApi = {
    async pull(since) {
      return [...rows.values()].filter((r) => !since || r.actualizado > since).sort((a, b) => (a.actualizado < b.actualizado ? -1 : 1));
    },
    async push(list) {
      for (const r of list) rows.set(`${r.tipo}:${r.id}`, { ...JSON.parse(JSON.stringify(r)), actualizado: stamp() });
    },
  };
  return { rows, api };
}

const player = (id: string, name: string, updatedAt = 1): Player => ({ id, name, active: true, createdAt: 1, updatedAt });
const match = (id: string): StoredMatch =>
  ({ id, formatVersion: 1, config: { mode: 'quick' }, participants: [], events: [], result: { score: { white: 1, blue: 0 } }, finishedAt: 5 }) as unknown as StoredMatch;
const data = (over: Partial<MesaData> = {}): MesaData => ({ players: [], matches: [], tournaments: [], prefs: DEFAULT_PREFERENCES, ...over });

describe('huellas', () => {
  it('no dependen del orden de las claves (la nube lo cambia)', () => {
    expect(stableStringify({ b: 1, a: { d: 2, c: 3 } })).toBe(stableStringify({ a: { c: 3, d: 2 }, b: 1 }));
    expect(fingerprint({ b: 1, a: 2 })).toBe(fingerprint({ a: 2, b: 1 }));
    expect(fingerprint({ a: 1 })).not.toBe(fingerprint({ a: 2 }));
  });
});

describe('sincronizar', () => {
  it('la primera vez sube todo lo de aquí y la siguiente no sube nada', async () => {
    const c = fakeCloud();
    const local = data({ players: [player('p1', 'Alex')], matches: [match('m1')] });
    const r1 = await syncOnce(c.api, local, emptyState());
    expect(r1.pushed).toBe(3);  // jugador, partido y preferencias
    expect(c.rows.size).toBe(3);
    const r2 = await syncOnce(c.api, local, r1.state);
    expect(r2.pushed).toBe(0);
    expect(r2.data).toBeNull();
  });

  it('otro aparato de la misma cuenta baja lo de la nube', async () => {
    const c = fakeCloud();
    await syncOnce(c.api, data({ players: [player('p1', 'Alex')], matches: [match('m1')] }), emptyState());
    const other = await syncOnce(c.api, data(), emptyState());
    expect(other.data?.players.map((p) => p.name)).toEqual(['Alex']);
    expect(other.data?.matches.map((m) => m.id)).toEqual(['m1']);
  });

  it('un cambio aquí se sube; uno en la nube se baja; un borrado se borra en los dos', async () => {
    const c = fakeCloud();
    const a0 = data({ players: [player('p1', 'Alex'), player('p2', 'José')] });
    const ra = await syncOnce(c.api, a0, emptyState());
    const rb = await syncOnce(c.api, data(), emptyState());
    // A cambia el nombre de Alex y borra a José.
    const a1 = data({ players: [player('p1', 'Álex', 2)] });
    const ra2 = await syncOnce(c.api, a1, ra.state);
    expect(ra2.pushed).toBe(2);
    expect(c.rows.get('player:p2')?.borrado).toBe(true);
    // B se entera de las dos cosas.
    const rb2 = await syncOnce(c.api, rb.data!, rb.state);
    expect(rb2.data?.players.map((p) => p.name)).toEqual(['Álex']);
  });

  it('si se cambia aquí y en la nube a la vez, gana lo de aquí (es lo último)', async () => {
    const c = fakeCloud();
    const ra = await syncOnce(c.api, data({ players: [player('p1', 'Alex')] }), emptyState());
    const rb = await syncOnce(c.api, data(), emptyState());
    await syncOnce(c.api, data({ players: [player('p1', 'Desde A', 2)] }), ra.state);
    const rb2 = await syncOnce(c.api, data({ players: [player('p1', 'Desde B', 3)] }), rb.state);
    expect(rb2.data).toBeNull();  // no se pisa lo de B
    expect((c.rows.get('player:p1')?.datos as Player).name).toBe('Desde B');
  });

  it('las preferencias de este aparato (conexión con la placa) no se suben ni se pisan', async () => {
    const c = fakeCloud();
    const prefsA = { ...DEFAULT_PREFERENCES, volume: 0.3, hardware: { wsUrl: 'ws://a', autoConnect: true } };
    await syncOnce(c.api, data({ prefs: prefsA }), emptyState());
    expect((c.rows.get('prefs:mesa')?.datos as Record<string, unknown>).hardware).toBeUndefined();
    const prefsB = { ...DEFAULT_PREFERENCES, hardware: { wsUrl: 'ws://b', autoConnect: false } };
    const rb = await syncOnce(c.api, data({ prefs: prefsB }), emptyState());
    expect(rb.data?.prefs.volume).toBe(0.3);
    expect(rb.data?.prefs.hardware.wsUrl).toBe('ws://b');
  });
});

describe('cuenta', () => {
  const reply = (status: number, body: object) => new Response(JSON.stringify(body), { status });

  it('entrar guarda la sesión y la renueva cuando va a caducar', async () => {
    const calls: string[] = [];
    const http = (async (url: string) => {
      calls.push(String(url));
      return reply(200, { access_token: `t${calls.length}`, refresh_token: 'r', expires_in: 3600, user: { id: 'u1', email: 'a@b.es' } });
    }) as unknown as typeof fetch;
    const s = await signIn(conn, ' a@b.es ', 'secreto', http, 1000);
    expect(s).toMatchObject({ userId: 'u1', email: 'a@b.es', accessToken: 't1' });
    expect(calls[0]).toContain('/auth/v1/token?grant_type=password');
    const store = createMemoryStore();
    writeSession(store, s);
    expect(readSession(store)?.userId).toBe('u1');
    expect(await freshSession(conn, s, http, 1000 + 10_000)).toBe(s);  // aún vale
    const renewed = await freshSession(conn, s, http, s.expiresAt - 30_000);
    expect(renewed.accessToken).toBe('t2');
    expect(calls[1]).toContain('grant_type=refresh_token');
  });

  it('los errores salen en español', async () => {
    const http = (async () => reply(400, { error_code: 'invalid_credentials', msg: 'Invalid login credentials' })) as unknown as typeof fetch;
    await expect(signIn(conn, 'a@b.es', 'x', http)).rejects.toThrow('Correo o contraseña incorrectos');
    const http2 = (async () => {
      throw new TypeError('fetch failed');
    }) as unknown as typeof fetch;
    await expect(signIn(conn, 'a@b.es', 'x', http2)).rejects.toThrow('Sin conexión con la nube');
  });

  it('crear cuenta con confirmación por correo no deja sesión todavía', async () => {
    const http = (async () => reply(200, { id: 'u1', email: 'a@b.es' })) as unknown as typeof fetch;
    expect(await signUp(conn, 'a@b.es', 'secreto', http)).toEqual({ session: null, needsConfirm: true });
  });

  it('la API REST pide lo cambiado desde una fecha y sube con «merge»', async () => {
    const seen: { url: string; init?: RequestInit }[] = [];
    const http = (async (url: string, init?: RequestInit) => {
      seen.push({ url: String(url), init });
      return init?.method === 'POST' ? new Response(null, { status: 201 }) : reply(200, []);
    }) as unknown as typeof fetch;
    const api = restApi(conn, async () => 'tok', http);
    await api.pull('2026-10-19T10:00:00Z');
    await api.push([{ tipo: 'match', id: 'm1', datos: {}, borrado: false }]);
    expect(decodeURIComponent(seen[0].url)).toContain('actualizado=gt.2026-10-19T10:00:00Z');
    expect((seen[0].init?.headers as Record<string, string>).Authorization).toBe('Bearer tok');
    expect(seen[1].url).toContain('on_conflict=owner_id,tipo,id');
    expect((seen[1].init?.headers as Record<string, string>).Prefer).toContain('resolution=merge-duplicates');
  });
});

describe('entrar en la placa con un QR', () => {
  const CODE = 'Ab3dEf6hIj9kLm2nOp5qRs8t';
  it('saca el código de la placa de la dirección del QR', async () => {
    const { boardCodeFrom } = await import('../src/services/cloud');
    expect(boardCodeFrom(`https://x.github.io/app/#vincular=${CODE}`)).toBe(CODE);
    expect(boardCodeFrom(`?vincular=${CODE}`)).toBe(CODE);
    expect(boardCodeFrom('https://otra.web/')).toBeNull();
    expect(boardCodeFrom('#vincular=corto')).toBeNull();
  });

  it('pide a la función vincular-placa con la sesión del dueño', async () => {
    const { linkBoard } = await import('../src/services/cloud');
    const seen: { url: string; init?: RequestInit }[] = [];
    const http = (async (url: string, init?: RequestInit) => {
      seen.push({ url: String(url), init });
      return new Response('{"ok":true}', { status: 200 });
    }) as unknown as typeof fetch;
    const s = { accessToken: 'tok', refreshToken: 'r', expiresAt: 0, userId: 'u', email: 'a@b.es' };
    await linkBoard(conn, s, CODE, http);
    expect(seen[0].url).toBe('https://nube.test/functions/v1/vincular-placa');
    expect((seen[0].init?.headers as Record<string, string>).Authorization).toBe('Bearer tok');
    expect(JSON.parse(String(seen[0].init?.body))).toEqual({ codigo: CODE });
    const bad = (async () => new Response('{"error":"Entra primero con tu cuenta"}', { status: 401 })) as unknown as typeof fetch;
    await expect(linkBoard(conn, s, CODE, bad)).rejects.toThrow('Entra primero con tu cuenta');
  });
});
