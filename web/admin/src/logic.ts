/**
 * Reglas del negocio del panel, sin pantalla ni base de datos: se prueban en tests/admin.test.ts.
 */
import type { AdminData, Coupon, Order, ProductKind, Role, ShipStatus, User, Venue } from './types';

export const DAY = 86_400_000;

// ---------------------------------------------------------------- dinero y fechas
export function money(cents: number): string {
  return (cents / 100).toLocaleString('es-ES', { style: 'currency', currency: 'EUR' });
}

/** Cifra compacta para las tarjetas: 1.284 · 12,9 mil · 4,2 M. */
export function compact(n: number): string {
  return n.toLocaleString('es-ES', { notation: n >= 10_000 ? 'compact' : 'standard', maximumFractionDigits: 1 });
}

export function fecha(ts: number): string {
  return new Date(ts).toLocaleDateString('es-ES', { day: '2-digit', month: 'short', year: 'numeric' });
}

export function monthKey(ts: number): string {
  const d = new Date(ts);
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}`;
}

const MESES = ['ene', 'feb', 'mar', 'abr', 'may', 'jun', 'jul', 'ago', 'sep', 'oct', 'nov', 'dic'];
export const monthLabel = (key: string) => {
  const [y, m] = key.split('-').map(Number);
  return `${MESES[m - 1]} ${String(y).slice(2)}`;
};

// ---------------------------------------------------------------- estadísticas
export interface Stats {
  ingresosMes: number;
  /** Mes anterior hasta el mismo día (comparación justa con el mes en curso). */
  ingresosMesAnterior: number;
  ingresos12: { key: string; total: number }[];
  mrr: number;
  usuarios: number;
  nuevos30: number;
  activos30: number;
  pro: number;
  conversion: number;
  baresActivos: number;
  envioPendientes: number;
  reembolsos30: number;
  porTipo: { tipo: ProductKind; total: number }[];
}

const isPaid = (o: Order) => o.estado === 'pagado';

export function computeStats(d: AdminData, now: number): Stats {
  const thisMonth = monthKey(now);
  const prev = new Date(now);
  prev.setDate(1);
  prev.setMonth(prev.getMonth() - 1);
  const prevMonth = monthKey(prev.getTime());

  const months: string[] = [];
  const cursor = new Date(now);
  cursor.setDate(1);
  for (let i = 11; i >= 0; i--) {
    const c = new Date(cursor);
    c.setMonth(cursor.getMonth() - i);
    months.push(monthKey(c.getTime()));
  }
  const byMonth = new Map(months.map((m) => [m, 0]));
  const byType = new Map<ProductKind, number>();
  const productType = new Map(d.products.map((p) => [p.id, p.tipo]));
  const today = new Date(now).getDate();
  let prevSamePeriod = 0;
  for (const o of d.orders.filter(isPaid)) {
    const k = monthKey(o.fecha);
    if (k === prevMonth && new Date(o.fecha).getDate() <= today) prevSamePeriod += o.importe;
    if (byMonth.has(k)) byMonth.set(k, byMonth.get(k)! + o.importe);
    const t = productType.get(o.productId);
    if (t) byType.set(t, (byType.get(t) ?? 0) + o.importe);
  }
  const users = d.users.filter((u) => u.role !== 'admin' && u.role !== 'soporte');
  const pro = users.filter((u) => u.plan === 'pro').length;
  return {
    ingresosMes: byMonth.get(thisMonth) ?? 0,
    ingresosMesAnterior: prevSamePeriod,
    ingresos12: months.map((key) => ({ key, total: byMonth.get(key) ?? 0 })),
    mrr: d.venues.filter((v) => v.estado === 'activa').reduce((s, v) => s + v.cuota, 0),
    usuarios: users.length,
    nuevos30: users.filter((u) => now - u.createdAt <= 30 * DAY).length,
    activos30: users.filter((u) => now - u.lastSeenAt <= 30 * DAY).length,
    pro,
    conversion: users.length ? pro / users.length : 0,
    baresActivos: d.venues.filter((v) => v.estado === 'activa').length,
    envioPendientes: d.orders.filter((o) => o.envio && isPaid(o) && !['enviado', 'entregado', 'devuelto'].includes(o.envio.estado)).length,
    reembolsos30: d.orders.filter((o) => o.estado === 'reembolsado' && now - o.fecha <= 30 * DAY).length,
    porTipo: [...byType.entries()].map(([tipo, total]) => ({ tipo, total })).sort((a, b) => b.total - a.total),
  };
}

/** Variación porcentual frente a un periodo anterior (null si no hay base). */
export function percent(f: number, digits = 1): string {
  return `${(f * 100).toLocaleString('es-ES', { minimumFractionDigits: digits, maximumFractionDigits: digits })} %`;
}

export function delta(now: number, before: number): number | null {
  return before ? (now - before) / before : null;
}

// ---------------------------------------------------------------- licencias Pro
// Formato MFV3-XXXX-XXXX-XXXX: 12 caracteres sin letras confusas (sin 0/O, 1/I/L) y el último es control.
const ALPHABET = '23456789ABCDEFGHJKMNPQRSTUVWXYZ';

function checkChar(body: string): string {
  let sum = 0;
  for (let i = 0; i < body.length; i++) sum = (sum * 31 + ALPHABET.indexOf(body[i]) * (i + 7)) % 1_000_003;
  return ALPHABET[sum % ALPHABET.length];
}

export function generateLicense(random: () => number = Math.random): string {
  let body = '';
  for (let i = 0; i < 11; i++) body += ALPHABET[Math.floor(random() * ALPHABET.length)];
  const full = body + checkChar(body);
  return `MFV3-${full.slice(0, 4)}-${full.slice(4, 8)}-${full.slice(8, 12)}`;
}

/** Comprueba el formato y el carácter de control (no consulta la base de datos). */
export function isValidLicense(code: string): boolean {
  const m = /^MFV3-([A-Z0-9]{4})-([A-Z0-9]{4})-([A-Z0-9]{4})$/.exec(code.trim().toUpperCase());
  if (!m) return false;
  const full = m[1] + m[2] + m[3];
  if ([...full].some((c) => !ALPHABET.includes(c))) return false;
  return checkChar(full.slice(0, 11)) === full[11];
}

// ---------------------------------------------------------------- cupones
export type CouponResult = { ok: true; total: number; descuento: number } | { ok: false; motivo: string; total: number };

export function applyCoupon(precio: number, c: Coupon | undefined, now: number): CouponResult {
  if (!c) return { ok: false, motivo: 'El cupón no existe', total: precio };
  if (!c.activo) return { ok: false, motivo: 'El cupón está desactivado', total: precio };
  if (c.caduca && now > c.caduca) return { ok: false, motivo: 'El cupón ha caducado', total: precio };
  if (c.maxUsos !== undefined && c.usos >= c.maxUsos) return { ok: false, motivo: 'El cupón ya no tiene usos', total: precio };
  const descuento = Math.min(precio, c.tipo === 'porcentaje' ? Math.round((precio * c.valor) / 100) : c.valor);
  return { ok: true, total: precio - descuento, descuento };
}

export function normalizeCouponCode(code: string): string {
  return code.trim().toUpperCase().replace(/[^A-Z0-9]/g, '');
}

// ---------------------------------------------------------------- pedidos y envíos
export const SHIP_FLOW: Record<ShipStatus, ShipStatus[]> = {
  pendiente: ['grabando'],
  grabando: ['probado'],
  probado: ['enviado'],
  enviado: ['entregado', 'devuelto'],
  entregado: ['devuelto'],
  devuelto: [],
};

export const SHIP_LABEL: Record<ShipStatus, string> = {
  pendiente: 'Pendiente',
  grabando: 'Grabando programa',
  probado: 'Probado',
  enviado: 'Enviado',
  entregado: 'Entregado',
  devuelto: 'Devuelto',
};

/** Siguiente paso del envío; para pasar a «enviado» hace falta haber grabado y probado la placa y un nº de seguimiento. */
export function canAdvanceShipment(o: Order, to: ShipStatus): { ok: boolean; motivo?: string } {
  if (!o.envio) return { ok: false, motivo: 'El pedido no lleva envío' };
  if (o.estado !== 'pagado') return { ok: false, motivo: 'El pedido no está pagado' };
  if (!SHIP_FLOW[o.envio.estado].includes(to)) return { ok: false, motivo: 'Paso no permitido' };
  if (to === 'probado' && !o.envio.grabada) return { ok: false, motivo: 'Marca antes la placa como grabada' };
  if (to === 'enviado' && !o.envio.probada) return { ok: false, motivo: 'Marca antes la placa como probada' };
  if (to === 'enviado' && !o.envio.seguimiento) return { ok: false, motivo: 'Falta el número de seguimiento' };
  return { ok: true };
}

export function canRefund(o: Order): boolean {
  return o.estado === 'pagado';
}

// ---------------------------------------------------------------- bares
export const VENUE_PRICE = (plan: Venue['plan'], mesas: number, s: { precioBarBasico: number; precioBarPro: number }) =>
  (plan === 'bar-pro' ? s.precioBarPro : s.precioBarBasico) * Math.max(1, mesas);

// ---------------------------------------------------------------- permisos
export type Permission =
  | 'ver_panel'
  | 'editar_usuarios'
  | 'cambiar_roles'
  | 'gestionar_envios'
  | 'reembolsar'
  | 'editar_tienda'
  | 'gestionar_licencias'
  | 'editar_avisos'
  | 'editar_ajustes'
  | 'editar_catalogo';

const ROLE_PERMS: Record<Role, Permission[]> = {
  admin: ['ver_panel', 'editar_usuarios', 'cambiar_roles', 'gestionar_envios', 'reembolsar', 'editar_tienda', 'gestionar_licencias', 'editar_avisos', 'editar_ajustes', 'editar_catalogo'],
  soporte: ['ver_panel', 'editar_usuarios', 'gestionar_envios', 'gestionar_licencias'],
  bar: [],
  jugador: [],
};

export const can = (role: Role, p: Permission) => ROLE_PERMS[role].includes(p);

export const ROLE_LABEL: Record<Role, string> = { admin: 'Administrador', soporte: 'Soporte', bar: 'Dueño de bar', jugador: 'Jugador' };

// ---------------------------------------------------------------- búsqueda y exportación
export function matches(text: string, ...fields: (string | undefined)[]): boolean {
  const q = text.trim().toLowerCase();
  return !q || fields.some((f) => f?.toLowerCase().includes(q));
}

/** CSV con «;» (lo que abre bien Excel en español) y comillas cuando hace falta. */
export function toCsv<T>(rows: T[], cols: { label: string; value: (r: T) => string | number }[]): string {
  const esc = (v: string | number) => {
    const s = String(v);
    return /[;"\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
  };
  return [cols.map((c) => esc(c.label)).join(';'), ...rows.map((r) => cols.map((c) => esc(c.value(r))).join(';'))].join('\n');
}

export function userName(users: User[], id: string | undefined): string {
  return users.find((u) => u.id === id)?.nombre ?? '—';
}

let counter = 0;
export function newId(prefix: string): string {
  counter = (counter + 1) % 1_000_000;
  return `${prefix}_${Date.now().toString(36)}${counter.toString(36)}${Math.floor(Math.random() * 1e6).toString(36)}`;
}
