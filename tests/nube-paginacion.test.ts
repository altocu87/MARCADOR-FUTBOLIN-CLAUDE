/**
 * Bajar de la nube por páginas sin saltarse filas aunque otro aparato cambie algo entre página y página (auditoría
 * MD-05 / NC01). Servidor PostgREST simulado en memoria: nada contra la base real.
 */
import { describe, expect, it } from 'vitest';
import { restApi, type CloudRow } from '../src/services/cloud';

const conn = { url: 'https://nube.test', anonKey: 'anon' };

type Row = CloudRow;
const key = (r: Row) => [r.actualizado, r.tipo, r.id] as const;
function cmp(a: Row, b: Row): number {
  const x = key(a), y = key(b);
  for (let i = 0; i < 3; i++) if (x[i] !== y[i]) return x[i] < y[i] ? -1 : 1;
  return 0;
}

// --- Filtros lógicos de PostgREST: or=(a.gt.x,and(b.eq."y",c.gt.z)) -----------------------------------------
type Node = { op: 'and' | 'or'; items: Node[] } | { col: string; cmp: string; value: string };
function parseList(s: string, i: number): [Node[], number] {
  const items: Node[] = [];
  if (s[i] !== '(') throw new Error(`falta «(» en ${s}`);
  i++;
  for (;;) {
    let node: Node;
    if (s.startsWith('and(', i) || s.startsWith('or(', i)) {
      const op = s.startsWith('and(', i) ? 'and' : 'or';
      const [sub, j] = parseList(s, i + op.length);
      node = { op, items: sub };
      i = j;
    } else {
      const m = /^([a-z_]+)\.([a-z]+)\./.exec(s.slice(i));
      if (!m) throw new Error(`filtro raro en ${s.slice(i)}`);
      i += m[0].length;
      let value = '';
      if (s[i] === '"') {
        for (i++; s[i] !== '"'; i++) {
          if (i >= s.length) throw new Error('comillas sin cerrar');
          if (s[i] === '\\') i++;
          value += s[i];
        }
        i++;
      } else {
        while (s[i] !== ',' && s[i] !== ')') value += s[i++];
      }
      node = { col: m[1], cmp: m[2], value };
    }
    items.push(node);
    if (s[i] === ',') { i++; continue; }
    if (s[i] === ')') return [items, i + 1];
    throw new Error(`se esperaba «,» o «)» en ${s.slice(i)}`);
  }
}
function evalNode(n: Node, r: Row): boolean {
  if ('op' in n) return n.op === 'and' ? n.items.every((x) => evalNode(x, r)) : n.items.some((x) => evalNode(x, r));
  const v = String((r as unknown as Record<string, unknown>)[n.col]);
  if (n.cmp === 'eq') return v === n.value;
  if (n.cmp === 'gt') return v > n.value;
  throw new Error(`operador no simulado: ${n.cmp}`);
}

/** Lo que haría PostgREST con la consulta (solo lo que usa la sincronización). */
function serve(rows: Map<string, Row>, url: string): Row[] {
  const q = new URL(url).searchParams;
  expect(q.get('order')).toBe('actualizado.asc,tipo.asc,id.asc');
  let list = [...rows.values()];
  const since = q.get('actualizado');
  if (since) {
    expect(since.startsWith('gt.')).toBe(true);
    list = list.filter((r) => r.actualizado > since.slice(3));
  }
  const or = q.get('or');
  if (or) {
    const [tree, end] = parseList(or, 0);
    expect(end).toBe(or.length);
    list = list.filter((r) => tree.some((n) => evalNode(n, r)));
  }
  list.sort(cmp);
  const offset = Number(q.get('offset') ?? 0);
  return list.slice(offset, offset + Number(q.get('limit')));
}

function cloud() {
  const rows = new Map<string, Row>();
  let micro = 1;
  // La hora del servidor: todo lo de una misma subida (un upsert de hasta 50) lleva la misma.
  const stamp = () => `2026-11-07T10:00:00.${String(micro++).padStart(6, '0')}+00:00`;
  const put = (ids: string[], tipo: Row['tipo'] = 'match') => {
    const t = stamp();
    for (const id of ids) rows.set(`${tipo}:${id}`, { tipo, id, datos: { v: t }, borrado: false, actualizado: t });
  };
  return { rows, put };
}

describe('bajar por páginas con cambios a la vez', () => {
  it('una fila ya leída que cambia entre páginas no hace saltarse otra (600 filas, páginas de 500)', async () => {
    const c = cloud();
    const ids = Array.from({ length: 600 }, (_, i) => `m${String(i).padStart(3, '0')}`);
    for (let i = 0; i < ids.length; i += 50) c.put(ids.slice(i, i + 50));
    const urls: string[] = [];
    const http = (async (url: string) => {
      urls.push(String(url));
      const page = serve(c.rows, String(url));
      if (urls.length === 1) c.put(['m000']);  // otra mesa cambia una fila ya leída: pasa al final
      return new Response(JSON.stringify(page), { status: 200 });
    }) as unknown as typeof fetch;
    const got = await restApi(conn, async () => 'tok', http).pull(null);
    expect(new Set(got.map((r) => r.id)).size).toBe(600);
    expect(urls.every((u) => !u.includes('offset='))).toBe(true);
    // La nueva versión de la fila cambiada llega en esta misma bajada (es la última).
    expect(got[got.length - 1]).toMatchObject({ id: 'm000', actualizado: c.rows.get('match:m000')!.actualizado });
  });

  it('una fila aún no leída que cambia a la vez tampoco se pierde, y la siguiente bajada no deja nada atrás', async () => {
    const c = cloud();
    const ids = Array.from({ length: 1200 }, (_, i) => `p${String(i).padStart(4, '0')}`);
    for (let i = 0; i < ids.length; i += 50) c.put(ids.slice(i, i + 50), 'player');
    let calls = 0;
    const http = (async (url: string) => {
      const page = serve(c.rows, String(url));
      if (++calls === 1) { c.put(['p0001', 'p0700'], 'player'); c.put(['p0499'], 'player'); }  // leída, sin leer y la frontera
      return new Response(JSON.stringify(page), { status: 200 });
    }) as unknown as typeof fetch;
    const api = restApi(conn, async () => 'tok', http);
    const got = await api.pull(null);
    expect(new Set(got.map((r) => r.id)).size).toBe(1200);
    // Cada fila llega con su última versión (la última vez que aparece).
    const last = new Map(got.map((r) => [r.id, r.actualizado]));
    for (const r of c.rows.values()) expect(last.get(r.id)).toBe(r.actualizado);
    const lastPull = got.reduce((m, r) => (r.actualizado > m ? r.actualizado : m), '');
    c.put(['p1199'], 'player');
    const next = await api.pull(lastPull);
    expect(next.map((r) => r.id)).toEqual(['p1199']);
  });

  it('el cursor aguanta ids con comas, paréntesis, comillas y barras', async () => {
    const c = cloud();
    // Van al final del orden y la página de 500 se corta en 'ze"f' (el cursor lleva comillas).
    const raros = ['za,b', 'zc(d)', 'ze"f', 'zg\\h', 'zi.j:k', 'zl m'];
    const ids = Array.from({ length: 497 }, (_, i) => `x${String(i).padStart(3, '0')}`).concat(raros);
    for (let i = 0; i < ids.length; i += 50) c.put(ids.slice(i, i + 50));
    // Todos en la frontera de la página: la misma hora para muchas filas.
    const t = '2026-11-07T11:00:00+00:00';
    for (const r of c.rows.values()) r.actualizado = t;
    const http = (async (url: string) => new Response(JSON.stringify(serve(c.rows, String(url))), { status: 200 })) as unknown as typeof fetch;
    const got = await restApi(conn, async () => 'tok', http).pull(null);
    expect(got.map((r) => r.id).slice(-6)).toEqual(raros);
    expect(new Set(got.map((r) => r.id)).size).toBe(503);
  });
});
