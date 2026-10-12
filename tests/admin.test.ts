import { describe, expect, it } from 'vitest';
import { LocalBackend, hashPassword, type KeyValueStore } from '../web/admin/src/backend';
import {
  DAY,
  applyCoupon,
  can,
  canAdvanceShipment,
  canRefund,
  computeStats,
  generateLicense,
  isValidLicense,
  normalizeCouponCode,
  toCsv,
} from '../web/admin/src/logic';
import { seedData } from '../web/admin/src/seed';
import { SupabaseBackend, TABLES, fromDb, toDb } from '../web/admin/src/supabase';
import type { Coupon, Order } from '../web/admin/src/types';

const NOW = Date.UTC(2026, 9, 2, 12);
const mem = (): KeyValueStore => {
  const m = new Map<string, string>();
  return { getItem: (k) => m.get(k) ?? null, setItem: (k, v) => void m.set(k, v), removeItem: (k) => void m.delete(k) };
};

describe('Panel · estadísticas', () => {
  const d = seedData(NOW);
  const s = computeStats(d, NOW);
  it('ingresos de 12 meses, del más antiguo al actual', () => {
    expect(s.ingresos12).toHaveLength(12);
    expect(s.ingresos12[11].key).toBe('2026-10');
    expect(s.ingresos12[0].key).toBe('2025-11');
    expect(s.ingresos12[11].total).toBe(s.ingresosMes);
  });
  it('compara el mes en curso con los mismos días del mes anterior', () => {
    const prevSame = d.orders
      .filter((o) => o.estado === 'pagado' && new Date(o.fecha).getMonth() === 8 && new Date(o.fecha).getFullYear() === 2026 && new Date(o.fecha).getDate() <= 2)
      .reduce((a, o) => a + o.importe, 0);
    expect(s.ingresosMesAnterior).toBe(prevSame);
  });
  it('solo cuenta pedidos pagados', () => {
    const pagado = d.orders.filter((o) => o.estado === 'pagado').reduce((a, o) => a + o.importe, 0);
    expect(s.porTipo.reduce((a, t) => a + t.total, 0)).toBe(pagado);
  });
  it('MRR = cuotas de los bares activos; conversión entre 0 y 1', () => {
    expect(s.mrr).toBe(d.venues.filter((v) => v.estado === 'activa').reduce((a, v) => a + v.cuota, 0));
    expect(s.conversion).toBeGreaterThan(0);
    expect(s.conversion).toBeLessThan(1);
    expect(s.usuarios).toBe(d.users.filter((u) => u.role === 'jugador' || u.role === 'bar').length);
  });
  it('los datos de demostración son siempre los mismos para la misma fecha', () => {
    expect(JSON.stringify(seedData(NOW))).toBe(JSON.stringify(d));
  });
});

describe('Panel · licencias Pro', () => {
  it('genera códigos válidos y detecta errores de tecleo', () => {
    for (let i = 0; i < 200; i++) {
      const c = generateLicense();
      expect(c).toMatch(/^MFV3-[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}$/);
      expect(isValidLicense(c)).toBe(true);
      expect(isValidLicense(c.toLowerCase())).toBe(true);
    }
    const c = generateLicense(() => 0.42);
    const last = c[c.length - 1];
    const wrong = c.slice(0, -1) + (last === 'A' ? 'B' : 'A');
    expect(isValidLicense(wrong)).toBe(false);
    expect(isValidLicense('MFV3-0000-0000-0000')).toBe(false);
    expect(isValidLicense('hola')).toBe(false);
  });
});

describe('Panel · cupones', () => {
  const base: Coupon = { id: 'c', code: 'X', tipo: 'porcentaje', valor: 20, usos: 0, activo: true };
  it('porcentaje y fijo, sin bajar de 0', () => {
    expect(applyCoupon(1000, base, NOW)).toEqual({ ok: true, total: 800, descuento: 200 });
    expect(applyCoupon(500, { ...base, tipo: 'fijo', valor: 900 }, NOW)).toEqual({ ok: true, total: 0, descuento: 500 });
  });
  it('rechaza caducado, agotado, desactivado o inexistente', () => {
    expect(applyCoupon(1000, { ...base, caduca: NOW - 1 }, NOW).ok).toBe(false);
    expect(applyCoupon(1000, { ...base, usos: 5, maxUsos: 5 }, NOW).ok).toBe(false);
    expect(applyCoupon(1000, { ...base, activo: false }, NOW).ok).toBe(false);
    expect(applyCoupon(1000, undefined, NOW)).toMatchObject({ ok: false, total: 1000 });
    expect(normalizeCouponCode(' bienvenida-20 ')).toBe('BIENVENIDA20');
  });
});

describe('Panel · envíos y reembolsos', () => {
  const o: Order = {
    id: 'o',
    userId: 'u',
    productId: 'p_kit_7c',
    importe: 12900,
    estado: 'pagado',
    fecha: NOW,
    envio: { estado: 'pendiente', direccion: 'x', placa: 'waveshare-7c', grabada: false, probada: false },
  };
  it('no se envía sin grabar, probar y nº de seguimiento', () => {
    expect(canAdvanceShipment(o, 'enviado').ok).toBe(false);
    expect(canAdvanceShipment(o, 'grabando').ok).toBe(true);
    const grabando = { ...o, envio: { ...o.envio!, estado: 'grabando' as const } };
    expect(canAdvanceShipment(grabando, 'probado').motivo).toMatch(/grabada/);
    const probado = { ...o, envio: { ...o.envio!, estado: 'probado' as const, grabada: true, probada: true } };
    expect(canAdvanceShipment(probado, 'enviado').motivo).toMatch(/seguimiento/);
    expect(canAdvanceShipment({ ...probado, envio: { ...probado.envio, seguimiento: 'ES1' } }, 'enviado').ok).toBe(true);
    expect(canAdvanceShipment({ ...o, estado: 'pendiente' }, 'grabando').ok).toBe(false);
  });
  it('solo se reembolsa lo pagado', () => {
    expect(canRefund(o)).toBe(true);
    expect(canRefund({ ...o, estado: 'reembolsado' })).toBe(false);
  });
});

describe('Panel · permisos', () => {
  it('admin todo; soporte sin dinero ni ajustes; bar y jugador nada', () => {
    expect(can('admin', 'editar_ajustes')).toBe(true);
    expect(can('soporte', 'gestionar_envios')).toBe(true);
    expect(can('soporte', 'reembolsar')).toBe(false);
    expect(can('soporte', 'cambiar_roles')).toBe(false);
    expect(can('bar', 'ver_panel')).toBe(false);
    expect(can('jugador', 'ver_panel')).toBe(false);
  });
});

describe('Panel · exportar CSV', () => {
  it('separador ; y comillas cuando hace falta', () => {
    const csv = toCsv([{ a: 'x;y', b: 'di "hola"' }], [
      { label: 'A', value: (r) => r.a },
      { label: 'B', value: (r) => r.b },
    ]);
    expect(csv).toBe('A;B\n"x;y";"di ""hola"""');
  });
});

describe('Panel · modo demostración (LocalBackend)', () => {
  it('crea la cuenta, inicia sesión y rechaza contraseñas malas', async () => {
    const b = new LocalBackend(mem(), mem(), () => NOW);
    expect(b.needsSetup()).toBe(true);
    await expect(b.createAdmin('admin@x.com', 'corta')).rejects.toThrow(/8 caracteres/);
    await b.createAdmin('Admin@X.com', 'contraseña-larga');
    expect(b.needsSetup()).toBe(false);
    expect(b.session()?.email).toBe('admin@x.com');
    b.signOut();
    expect(b.session()).toBeNull();
    await expect(b.signIn('admin@x.com', 'otra-cosa-larga')).rejects.toThrow(/incorrectos/);
    expect((await b.signIn('admin@x.com', 'contraseña-larga')).role).toBe('admin');
  });
  it('no guarda la contraseña, solo su huella', async () => {
    const store = mem();
    const b = new LocalBackend(store, mem(), () => NOW);
    await b.createAdmin('a@b.co', 'secreto-largo');
    expect(store.getItem('mfv3-admin-demo-cuenta')).not.toContain('secreto-largo');
    expect(await hashPassword('x', 's')).toHaveLength(64);
  });
  it('guarda y borra datos', async () => {
    const store = mem();
    const b = new LocalBackend(store, mem(), () => NOW);
    const d = await b.load();
    const n = d.coupons.length;
    await b.upsert('coupons', { id: 'c_new', code: 'NUEVO', tipo: 'fijo', valor: 100, usos: 0, activo: true });
    const again = new LocalBackend(store, mem(), () => NOW);
    expect((await again.load()).coupons).toHaveLength(n + 1);
    await again.remove('coupons', 'c_new');
    expect((await new LocalBackend(store, mem(), () => NOW).load()).coupons).toHaveLength(n);
  });
});

describe('Panel · modo real (SupabaseBackend, con un servidor simulado)', () => {
  function fakeServer(role: string, opts: { factor?: boolean; bloqueado?: boolean } = {}) {
    const calls: { url: string; init?: RequestInit }[] = [];
    const json = (body: unknown, status = 200) => new Response(JSON.stringify(body), { status, headers: { 'Content-Type': 'application/json' } });
    const user = { id: 'uid-1', email: 'jefe@bar.com', factors: opts.factor ? [{ id: 'f-ok', factor_type: 'totp', status: 'verified' }] : [] };
    const http = (async (url: string, init?: RequestInit) => {
      calls.push({ url, init });
      if (url.includes('/auth/v1/token?grant_type=password')) return json({ access_token: 'AAL1', refresh_token: 'R1', expires_in: 3600, user });
      if (url.includes('/auth/v1/token?grant_type=refresh_token')) return json({ access_token: 'TOKEN2', refresh_token: 'R2', expires_in: 3600, user });
      if (url.endsWith('/auth/v1/factors')) return json({ id: 'f-new', totp: { qr_code: 'data:image/svg+xml;utf-8,<svg/>', secret: 'ABC123' } });
      if (url.endsWith('/challenge')) return json({ id: 'ch-1' });
      if (url.endsWith('/verify')) {
        const body = JSON.parse(String(init?.body));
        return body.code === '123456' ? json({ access_token: 'TOKEN', refresh_token: 'R1', expires_in: 3600, user }) : json({ msg: 'Invalid TOTP code entered' }, 422);
      }
      if (url.includes('/auth/v1/logout')) return new Response(null, { status: 204 });
      if (url.includes('/rest/v1/profiles?id=eq.')) return json([{ role, bloqueado: !!opts.bloqueado }]);
      if (init?.method === 'PATCH') return json(url.includes('id=eq.existe') ? [{ id: 'existe' }] : []);
      if (init?.method === 'POST' || init?.method === 'DELETE') return new Response(null, { status: 201 });
      if (url.includes('/rest/v1/settings')) return json([{ id: 'ajustes', nombre_tienda: 'Tienda', precio_pro: 499 }]);
      if (url.includes('/rest/v1/profiles')) return json([{ id: 'u1', email: 'a@b.c', created_at: 1, last_seen_at: 2, venue_id: null }]);
      return json([]);
    }) as typeof fetch;
    return { http, calls };
  }

  /** Entra con contraseña y código. */
  async function enter(b: SupabaseBackend) {
    const step = await b.signIn('jefe@bar.com', 'pw');
    if (!('kind' in step)) throw new Error('debería pedir el código');
    return b.verifyMfa(step, '123456');
  }

  it('solo entra con rol admin o soporte y cuenta sin bloquear (lo decide el servidor)', async () => {
    const no = fakeServer('jugador');
    await expect(new SupabaseBackend('https://x.supabase.co', 'ANON', mem(), no.http).signIn('j@x.com', 'pw')).rejects.toThrow(/permiso/);
    // Y se revoca la sesión a medias.
    expect(no.calls.some((c) => c.url.includes('/auth/v1/logout'))).toBe(true);
    const blocked = fakeServer('admin', { bloqueado: true });
    await expect(new SupabaseBackend('https://x.supabase.co', 'ANON', mem(), blocked.http).signIn('j@x.com', 'pw')).rejects.toThrow(/permiso/);
  });

  it('la contraseña sola no abre la sesión: la primera vez da de alta la app de verificación con un QR', async () => {
    const srv = fakeServer('admin');
    const store = mem();
    const b = new SupabaseBackend('https://x.supabase.co/', 'ANON', store, srv.http);
    const step = await b.signIn('jefe@bar.com', 'pw');
    expect(step).toMatchObject({ kind: 'mfa', factorId: 'f-new', enroll: { secret: 'ABC123' } });
    expect(b.session()).toBeNull();
    await expect(b.verifyMfa(step as never, '000000')).rejects.toThrow(/Invalid/);
    expect(b.session()).toBeNull();
  });

  it('con la app ya dada de alta pide el código y la sesión usa el token verificado', async () => {
    const srv = fakeServer('admin', { factor: true });
    const b = new SupabaseBackend('https://x.supabase.co', 'ANON', mem(), srv.http);
    const step = await b.signIn('jefe@bar.com', 'pw');
    expect(step).toEqual({ kind: 'mfa', factorId: 'f-ok' });
    expect(srv.calls.some((c) => c.url.endsWith('/auth/v1/factors'))).toBe(false);
    const s = await b.verifyMfa(step as never, '123456');
    expect(s).toMatchObject({ email: 'jefe@bar.com', role: 'admin', token: 'TOKEN', refreshToken: 'R1' });
    b.signOut();
    expect(b.session()).toBeNull();
    expect(srv.calls.at(-1)!.url).toContain('/auth/v1/logout');
  });

  it('renueva el token cuando va a caducar', async () => {
    const srv = fakeServer('admin', { factor: true });
    const store = mem();
    const b = new SupabaseBackend('https://x.supabase.co', 'ANON', store, srv.http);
    await enter(b);
    store.setItem('mfv3-admin-sesion-real', JSON.stringify({ ...b.session(), expiresAt: Date.now() + 10_000 }));
    await b.load();
    expect(b.session()!.token).toBe('TOKEN2');
    const read = srv.calls.find((c) => c.url.includes('/rest/v1/orders'))!;
    expect((read.init?.headers as Record<string, string>).Authorization).toBe('Bearer TOKEN2');
  });

  it('lee con el token del usuario y convierte nombres de columnas', async () => {
    const srv = fakeServer('admin', { factor: true });
    const b = new SupabaseBackend('https://x.supabase.co', 'ANON', mem(), srv.http);
    await enter(b);
    const d = await b.load();
    expect(d.users[0]).toEqual({ id: 'u1', email: 'a@b.c', createdAt: 1, lastSeenAt: 2 });
    expect(d.settings).toMatchObject({ nombreTienda: 'Tienda', precioPro: 499 });
    const read = srv.calls.find((c) => c.url.includes('/rest/v1/orders'))!;
    expect((read.init?.headers as Record<string, string>).Authorization).toBe('Bearer TOKEN');
    // Nuevo: intenta modificar (0 filas) y entonces crea.
    await b.upsert('coupons', { id: 'c1', code: 'X', tipo: 'fijo', valor: 1, usos: 0, activo: true, maxUsos: undefined });
    const [patch, post] = srv.calls.slice(-2);
    expect(patch.init?.method).toBe('PATCH');
    expect(post.init?.method).toBe('POST');
    expect(post.url).toBe('https://x.supabase.co/rest/v1/coupons');
    expect(JSON.parse(String(post.init?.body))).toMatchObject({ id: 'c1', max_usos: null });
    // Existente: solo modifica (así soporte puede actualizar envíos sin permiso de crear pedidos).
    const before = srv.calls.length;
    await b.upsert('orders', { id: 'existe', userId: 'u', productId: 'p', importe: 1, estado: 'pagado', fecha: 1 });
    expect(srv.calls.length - before).toBe(1);
    expect(srv.calls.at(-1)!.init?.method).toBe('PATCH');
  });

  it('en el servidor, el equipo solo tiene rol con verificación en dos pasos y cuenta sin bloquear', async () => {
    const { default: sql } = await import('../supabase/migrations/20261011000000_inicial.sql?raw');
    const fn = /function public\.staff_role\(\)[\s\S]*?\$\$([\s\S]*?)\$\$/.exec(sql)![1];
    expect(fn).toContain("'aal2'");
    expect(fn).toContain('not bloqueado');
  });

  it('las guardias frenan a quien entra por la API, pero no al dueño desde el SQL Editor', async () => {
    const { default: sql } = await import('../supabase/migrations/20261011130000_guardias_sql_editor.sql?raw');
    for (const fn of ['guard_profile', 'guard_order']) {
      const body = new RegExp(`function public\\.${fn}\\(\\)[\\s\\S]*?end \\$\\$`).exec(sql)![0];
      expect(body, fn).toContain("current_user not in ('anon', 'authenticated')");
    }
  });

  it('cada colección tiene su tabla y la conversión es reversible', () => {
    expect(Object.keys(TABLES)).toHaveLength(10);
    const o = { createdAt: 5, venueId: 'v', nombre: 'n' };
    expect(fromDb(toDb(o) as Record<string, unknown>)).toEqual(o);
  });
});

describe('Panel · rango de fechas de los datos de ejemplo', () => {
  it('ningún pedido en el futuro', () => {
    expect(seedData(NOW).orders.every((o) => o.fecha <= NOW + DAY)).toBe(true);
  });
});

describe('Panel · la base de datos preparada coincide con el panel', () => {
  it('cada tabla existe y tiene todas las columnas que el panel guarda', async () => {
    // Todas las migraciones juntas, en orden.
    const files = import.meta.glob('../supabase/migrations/*.sql', { query: '?raw', import: 'default', eager: true }) as Record<string, string>;
    const sql = Object.keys(files).sort().map((k) => files[k]).join('\n');
    const d = seedData(NOW);
    const samples: Record<string, object> = {
      users: { ...d.users.find((u) => u.venueId)! },
      venues: d.venues[0],
      products: d.products.find((p) => p.stock !== undefined)!,
      orders: { ...d.orders.find((o) => o.envio && o.cupon) ?? d.orders.find((o) => o.envio)!, cupon: 'X' },
      licenses: d.licenses[0],
      coupons: { ...d.coupons[2] },
      announcements: d.announcements[1],
      audit: d.audit[0],
      catalogCollections: { ...d.catalogCollections[0], premioCompletar: 1, torneo: 'league' },
      catalogItems: { ...d.catalogItems[1], archivo: 'x', ruta: 'x', rareza: 'epica' },
    };
    for (const [col, table] of Object.entries(TABLES)) {
      const m = new RegExp(`create table if not exists public\\.${table} \\(([\\s\\S]*?)\\n\\);`).exec(sql);
      expect(m, `tabla ${table}`).toBeTruthy();
      // La columna está en el «create table» o se añadió después con «add column if not exists».
      const added = (key: string) => new RegExp(`alter table public\\.${table}\\s+add column if not exists ${key}\\s`).test(sql);
      for (const key of Object.keys(toDb(samples[col]))) {
        if (!added(key)) expect(m![1], `${table}.${key}`).toMatch(new RegExp(`\\n\\s+${key}\\s`));
      }
      expect(sql, `RLS en ${table}`).toContain(`alter table public.${table}`);
    }
    const settingsCols = /create table if not exists public\.settings \(([\s\S]*?)\n\);/.exec(sql)![1];
    for (const key of Object.keys(toDb(d.settings))) expect(settingsCols).toMatch(new RegExp(`\\n\\s+${key}\\s`));
  });
});
