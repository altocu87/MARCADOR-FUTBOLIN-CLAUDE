/** Cuenta del dueño de la mesa y sincronización con la nube (Supabase simulado: nada contra la base real). */
import { describe, expect, it, vi } from 'vitest';
import {
  emptyState,
  fingerprint,
  mergeAfterSync,
  freshSession,
  readSession,
  readRememberSession,
  REMEMBER_KEY,
  signOut,
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
import { DEFAULT_PREFERENCES, createLocalRepositories, createMemoryStore, type Player, type Repositories, type StoredMatch } from '../src/services/persistence';

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

/** Lo que hay guardado en el aparato (como hace la app antes y después de sincronizar). */
async function readRepos(repos: Repositories): Promise<MesaData> {
  return { players: await repos.players.list(), matches: await repos.matches.list(), tournaments: await repos.tournaments.list(), prefs: await repos.preferences.load() };
}

describe('aplicar lo bajado sin perder lo de aquí', () => {
  it('un partido guardado durante la sincronización no se borra y se sube en la siguiente', async () => {
    const c = fakeCloud();
    // Otro aparato ya ha subido un partido (m2).
    await syncOnce(c.api, data({ players: [player('p1', 'Alex')], matches: [match('m2')] }), emptyState());

    const repos = createLocalRepositories(createMemoryStore());
    await repos.players.save(player('p1', 'Alex'));
    await repos.matches.save(match('m1'));
    const first = await syncOnce(c.api, await readRepos(repos), emptyState());
    await repos.matches.saveAll(first.data!.matches);

    // Llega otro partido de la nube (m4) y, MIENTRAS se sincroniza, aquí se termina un partido (m3).
    const other = await syncOnce(c.api, data(), emptyState());
    await syncOnce(c.api, { ...other.data!, matches: [...other.data!.matches, match('m4')] }, other.state);
    const start = await readRepos(repos);
    const slowApi: typeof c.api = {
      pull: async (since) => {
        const rows = await c.api.pull(since);
        await repos.matches.save(match('m3'));  // matchFinalizer guarda el partido justo ahora
        return rows;
      },
      push: c.api.push,
    };
    const r = await syncOnce(slowApi, start, first.state);
    expect(r.data?.matches.map((m) => m.id).sort()).toEqual(['m1', 'm2', 'm4']);  // la copia de antes no lo tiene

    const merged = mergeAfterSync(start, r.data!, await readRepos(repos));
    await repos.matches.saveAll(merged.matches);
    expect((await repos.matches.list()).map((m) => m.id).sort()).toEqual(['m1', 'm2', 'm3', 'm4']);

    // Queda pendiente y se sube en la siguiente sincronización.
    expect(c.rows.has('match:m3')).toBe(false);
    const again = await syncOnce(c.api, await readRepos(repos), r.state);
    expect(again.pushed).toBe(1);
    expect(c.rows.has('match:m3')).toBe(true);
  });

  it('lo cambiado o borrado aquí durante la sincronización gana; lo que no se tocó se actualiza', () => {
    const start = data({ players: [player('p1', 'Alex'), player('p2', 'José'), player('p3', 'Ana')] });
    // La nube cambió a p1 y p2 y borró a p3.
    const synced = data({ players: [player('p1', 'Álex', 2), player('p2', 'Jose', 2)] });
    // Aquí, mientras tanto, se cambió a p2 (con otro objeto pero igual contenido para p1 y p3).
    const current = data({ players: [player('p1', 'Alex'), player('p2', 'Pepe', 3), player('p3', 'Ana')] });
    const merged = mergeAfterSync(start, synced, current);
    expect(merged.players.map((p) => `${p.id}:${p.name}`)).toEqual(['p1:Álex', 'p2:Pepe']);
  });

  it('preferencias: se toma lo que cambió en la nube sin pisar lo cambiado aquí', () => {
    const start = data({ prefs: { ...DEFAULT_PREFERENCES, volume: 0.5, muted: false } });
    const synced = data({ prefs: { ...DEFAULT_PREFERENCES, volume: 0.2, muted: false } });
    const current = data({ prefs: { ...DEFAULT_PREFERENCES, volume: 0.5, muted: true } });
    const merged = mergeAfterSync(start, synced, current);
    expect(merged.prefs.volume).toBe(0.2);
    expect(merged.prefs.muted).toBe(true);
  });
});

describe('cancelar y tiempo máximo', () => {
  /** Una nube que no contesta nunca (hasta que se corta la petición). */
  const hanging = (async (_url: string, init?: RequestInit) =>
    new Promise<Response>((_, reject) => init?.signal?.addEventListener('abort', () => reject(new DOMException('cortada', 'AbortError'))))) as unknown as typeof fetch;

  it('al cancelar se corta la petición y sale «aplazada»', async () => {
    const ctl = new AbortController();
    const api = restApi(conn, async () => 'tok', hanging, { signal: ctl.signal });
    const p = api.pull(null);
    ctl.abort();
    await expect(p).rejects.toMatchObject({ code: 'aborted' });
  });

  it('si la nube no contesta, se corta a los 15 s', async () => {
    vi.useFakeTimers();
    try {
      const api = restApi(conn, async () => 'tok', hanging);
      const p = api.pull(null);
      const check = expect(p).rejects.toMatchObject({ code: 'timeout' });
      await vi.advanceTimersByTimeAsync(15_000);
      await check;
    } finally {
      vi.useRealTimers();
    }
  });

  it('syncOnce no sigue si ya está cancelada', async () => {
    const c = fakeCloud();
    const ctl = new AbortController();
    ctl.abort();
    await expect(syncOnce(c.api, data({ players: [player('p1', 'Alex')] }), emptyState(), { signal: ctl.signal })).rejects.toMatchObject({ code: 'aborted' });
    expect(c.rows.size).toBe(0);
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

  it('recuerda las sesiones antiguas y permite elegir no recordar en este aparato', () => {
    const store = createMemoryStore();
    expect(readRememberSession(store)).toBe(true);
    store.setItem(REMEMBER_KEY, '0');
    expect(readRememberSession(store)).toBe(false);
    store.setItem(REMEMBER_KEY, '1');
    expect(readRememberSession(store)).toBe(true);
  });

  it('renueva tras meses sin abrir y guarda el nuevo permiso, sin ampliar la vida del JWT', async () => {
    const store = createMemoryStore();
    const old = { accessToken: 'viejo', refreshToken: 'permiso', expiresAt: 1000, userId: 'u1', email: 'a@b.es' };
    writeSession(store, old);
    const now = 180 * 24 * 60 * 60 * 1000;
    const http = (async (_url: string, init: RequestInit) => {
      expect(JSON.parse(String(init.body))).toEqual({ refresh_token: 'permiso' });
      return reply(200, { access_token: 'nuevo', refresh_token: 'rotado', expires_in: 3600 });
    }) as unknown as typeof fetch;
    const next = await freshSession(conn, readSession(store)!, http, now);
    writeSession(store, next);
    expect(readSession(store)).toMatchObject({ refreshToken: 'rotado', userId: 'u1', expiresAt: now + 3600_000 });
  });

  it('solo cierra la sesión de este aparato y borra el acceso local también sin red', async () => {
    const s = { accessToken: 't', refreshToken: 'r', expiresAt: 1000, userId: 'u', email: 'a@b.es' };
    const http = (async (url: string) => {
      expect(url).toBe('https://nube.test/auth/v1/logout?scope=local');
      throw new TypeError('sin red');
    }) as unknown as typeof fetch;
    await expect(signOut(conn, s, http)).resolves.toBeUndefined();
    const store = createMemoryStore(); writeSession(store, s); writeSession(store, null);
    expect(readSession(store)).toBeNull();
  });

  it('un permiso revocado pide entrar de nuevo; un fallo de red conserva el permiso', async () => {
    const s = { accessToken: 't', refreshToken: 'r', expiresAt: 0, userId: 'u', email: 'a@b.es' };
    const rejected = (async () => reply(400, { error_code: 'refresh_token_not_found' })) as unknown as typeof fetch;
    await expect(freshSession(conn, s, rejected)).rejects.toMatchObject({ code: 'session_expired' });
    const offline = (async () => { throw new TypeError('sin red'); }) as unknown as typeof fetch;
    await expect(freshSession(conn, s, offline)).rejects.toMatchObject({ code: 'offline' });
    expect(s.refreshToken).toBe('r');
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
    expect(boardCodeFrom(`#vincular=${CODE}!alterado`)).toBeNull();
    expect(boardCodeFrom(`#vincular=${CODE}%2Fextra`)).toBeNull();
    expect(boardCodeFrom(`#vincular=${'A'.repeat(65)}`)).toBeNull();
    expect(boardCodeFrom(`?vincular=${CODE}&otra=1`)).toBe(CODE);
    expect(boardCodeFrom(`  ${CODE}  `)).toBe(CODE);
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
