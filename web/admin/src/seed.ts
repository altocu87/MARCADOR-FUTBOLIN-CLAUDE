/**
 * Datos de DEMOSTRACIÓN para probar el panel sin base de datos: un año ficticio de negocio.
 * Siempre los mismos para la misma fecha (generador pseudoaleatorio con semilla).
 */
import { DAY, VENUE_PRICE, generateLicense } from './logic';
import type { AdminData, Announcement, Coupon, License, Order, Product, Settings, ShipStatus, User, Venue } from './types';

function rng(seed: number) {
  let s = seed >>> 0;
  return () => {
    s = (s * 1664525 + 1013904223) >>> 0;
    return s / 4294967296;
  };
}

const NOMBRES = ['Ana', 'Beto', 'Carla', 'David', 'Elena', 'Fran', 'Gema', 'Hugo', 'Irene', 'Javi', 'Lucía', 'Marcos', 'Nuria', 'Óscar', 'Paula', 'Quique', 'Rosa', 'Sergio', 'Tania', 'Víctor'];
const APELLIDOS = ['García', 'López', 'Martín', 'Sánchez', 'Pérez', 'Gómez', 'Ruiz', 'Díaz', 'Moreno', 'Navarro'];
const CIUDADES = ['Valencia', 'Madrid', 'Barcelona', 'Sevilla', 'Bilbao', 'Zaragoza', 'Málaga', 'Alicante'];
const BARES = ['Bar El Gol', 'Cervecería La Mesa', 'Club Futbolín Norte', 'Taberna Los Delanteros', 'Pub La Portería', 'Bar Penalti', 'Casal Jove', 'Bar La Prórroga', 'Club Social Sur', 'Bar El Palo', 'La Bolera Vieja', 'Bar Gol de Oro'];
const PAISES = ['ES', 'ES', 'ES', 'ES', 'ES', 'PT', 'FR', 'MX', 'AR'];

export const DEFAULT_SETTINGS: Settings = {
  id: 'ajustes',
  nombreTienda: 'Marcador Futbolín',
  emailSoporte: 'soporte@ejemplo.com',
  moneda: 'EUR',
  ivaIncluido: true,
  proveedorPagos: 'ninguno',
  precioPro: 499,
  precioBarBasico: 900,
  precioBarPro: 1500,
  mantenimiento: false,
};

export const PRODUCTS: Product[] = [
  { id: 'p_pro', nombre: 'Marcador Pro', tipo: 'pro', precio: 499, activo: true, descripcion: 'Desbloqueo de por vida: torneos avanzados, estadísticas extendidas y temas.' },
  { id: 'p_tema_neon', nombre: 'Tema Neón', tipo: 'tema', precio: 199, activo: true, descripcion: 'Colores neón y efectos de luz.' },
  { id: 'p_tema_retro', nombre: 'Tema Retro', tipo: 'tema', precio: 199, activo: true, descripcion: 'Estilo marcador de estadio de los 80.' },
  { id: 'p_sonido_estadio', nombre: 'Pack de sonido Estadio', tipo: 'sonido', precio: 149, activo: true, descripcion: 'Afición, bocinas y locutor.' },
  { id: 'p_pack_completo', nombre: 'Pack completo', tipo: 'pack', precio: 699, activo: true, descripcion: 'Pro + todos los temas y sonidos.' },
  { id: 'p_bar_basico', nombre: 'Bar Básico (por mesa y mes)', tipo: 'suscripcion', precio: 900, activo: true, descripcion: 'Ranking del bar y ligas internas.' },
  { id: 'p_bar_pro', nombre: 'Bar Pro (por mesa y mes)', tipo: 'suscripcion', precio: 1500, activo: true, descripcion: 'Ligas entre bares, marcador en la tele y estadísticas.' },
  { id: 'p_kit_7c', nombre: 'Kit pantalla 7" + mando', tipo: 'kit', precio: 12900, activo: true, stock: 14, descripcion: 'Waveshare 7C grabada y probada + mando inalámbrico con 2 pulsadores.' },
  { id: 'p_kit_pulsadores', nombre: 'Kit pulsadores USB', tipo: 'kit', precio: 3900, activo: true, stock: 32, descripcion: 'Arduino grabado + 2 pulsadores arcade + cables.' },
];

export function seedData(now: number = Date.now(), seed = 7): AdminData {
  const r = rng(seed);
  const pick = <T,>(a: T[]) => a[Math.floor(r() * a.length)];
  const id = (p: string, i: number) => `${p}_${i.toString(36).padStart(3, '0')}`;
  const year = 365 * DAY;

  const users: User[] = [
    { id: 'u_admin', email: 'admin@ejemplo.com', nombre: 'Administrador', role: 'admin', plan: 'pro', pais: 'ES', createdAt: now - year, lastSeenAt: now, bloqueado: false, notas: '' },
    { id: 'u_soporte', email: 'soporte@ejemplo.com', nombre: 'Equipo de soporte', role: 'soporte', plan: 'pro', pais: 'ES', createdAt: now - 200 * DAY, lastSeenAt: now - DAY, bloqueado: false, notas: '' },
  ];
  for (let i = 0; i < 160; i++) {
    // Crecimiento: más altas en los últimos meses.
    const age = Math.floor(year * Math.pow(r(), 1.6));
    const nombre = `${pick(NOMBRES)} ${pick(APELLIDOS)}`;
    users.push({
      id: id('u', i),
      email: `${nombre.split(' ')[0].toLowerCase().normalize('NFD').replace(/[^a-z]/g, '')}${i}@correo.com`,
      nombre,
      role: 'jugador',
      plan: r() < 0.22 ? 'pro' : 'gratis',
      pais: pick(PAISES),
      createdAt: now - age,
      lastSeenAt: now - Math.floor(Math.min(age, 90 * DAY) * r()),
      bloqueado: r() < 0.01,
      notas: '',
    });
  }

  const venues: Venue[] = BARES.map((nombre, i) => {
    const owner: User = {
      id: id('ub', i),
      email: `bar${i}@negocio.com`,
      nombre: `Dueño de ${nombre}`,
      role: 'bar',
      plan: 'pro',
      pais: 'ES',
      createdAt: now - Math.floor(r() * 300 * DAY),
      lastSeenAt: now - Math.floor(r() * 10 * DAY),
      bloqueado: false,
      notas: '',
      venueId: id('v', i),
    };
    users.push(owner);
    const plan = r() < 0.4 ? 'bar-pro' : 'bar-basico';
    const mesas = 1 + Math.floor(r() * 3);
    const estado = i < 8 ? 'activa' : (['prueba', 'impagada', 'cancelada'] as const)[i % 3];
    const desde = owner.createdAt + 2 * DAY;
    return {
      id: id('v', i),
      nombre,
      ciudad: pick(CIUDADES),
      ownerId: owner.id,
      mesas,
      plan,
      estado,
      cuota: VENUE_PRICE(plan, mesas, DEFAULT_SETTINGS),
      desde,
      renovacion: now + Math.floor(r() * 30) * DAY,
    };
  });

  const orders: Order[] = [];
  const licenses: License[] = [];
  const buyers = users.filter((u) => u.role === 'jugador');
  const digital = PRODUCTS.filter((p) => ['pro', 'tema', 'sonido', 'pack'].includes(p.tipo));
  const kits = PRODUCTS.filter((p) => p.tipo === 'kit');
  for (let i = 0; i < 340; i++) {
    const u = pick(buyers);
    const isKit = r() < 0.12;
    const p = isKit ? pick(kits) : pick(digital);
    const fecha = Math.min(now, u.createdAt + Math.floor(r() * Math.max(DAY, now - u.createdAt)));
    const roll = r();
    const estado = roll < 0.9 ? 'pagado' : roll < 0.95 ? 'reembolsado' : roll < 0.98 ? 'fallido' : 'pendiente';
    const conCupon = r() < 0.12;
    const importe = conCupon ? Math.round(p.precio * 0.8) : p.precio;
    const order: Order = { id: id('o', i), userId: u.id, productId: p.id, importe, estado, fecha, cupon: conCupon ? 'BIENVENIDA20' : undefined, proveedorRef: `demo_${i}` };
    if (isKit) {
      const dias = (now - fecha) / DAY;
      const est: ShipStatus = dias > 12 ? 'entregado' : dias > 6 ? 'enviado' : dias > 3 ? 'probado' : dias > 1 ? 'grabando' : 'pendiente';
      order.envio = {
        estado: est,
        direccion: `C/ Mayor ${1 + Math.floor(r() * 90)}, ${pick(CIUDADES)}`,
        placa: p.id === 'p_kit_7c' ? 'waveshare-7c' : 'arduino-uno',
        grabada: est !== 'pendiente',
        probada: !['pendiente', 'grabando'].includes(est),
        seguimiento: ['enviado', 'entregado'].includes(est) ? `ES${100000 + i}` : undefined,
      };
    }
    orders.push(order);
    if ((p.tipo === 'pro' || p.tipo === 'pack') && estado !== 'fallido') {
      licenses.push({
        id: id('l', licenses.length),
        code: generateLicense(r),
        estado: estado === 'reembolsado' ? 'revocada' : 'activa',
        userId: u.id,
        orderId: order.id,
        creada: fecha,
        activaciones: estado === 'pagado' ? 1 + Math.floor(r() * 2) : 0,
        maxActivaciones: 3,
      });
    }
  }
  // Cuotas mensuales de los bares activos.
  for (const v of venues.filter((x) => x.estado === 'activa' || x.estado === 'cancelada')) {
    for (let t = v.desde; t < now; t += 30 * DAY) {
      orders.push({ id: id('ob', orders.length), userId: v.ownerId, productId: v.plan === 'bar-pro' ? 'p_bar_pro' : 'p_bar_basico', importe: v.cuota, estado: 'pagado', fecha: t, proveedorRef: `demo_sub_${orders.length}` });
      if (v.estado === 'cancelada' && t > v.desde + 90 * DAY) break;
    }
  }
  for (let i = 0; i < 10; i++)
    licenses.push({ id: id('lf', i), code: generateLicense(r), estado: 'libre', creada: now - Math.floor(r() * 20) * DAY, activaciones: 0, maxActivaciones: 3 });

  const coupons: Coupon[] = [
    { id: 'c_bienvenida', code: 'BIENVENIDA20', tipo: 'porcentaje', valor: 20, usos: orders.filter((o) => o.cupon).length, activo: true },
    { id: 'c_bares', code: 'BARES10', tipo: 'fijo', valor: 1000, usos: 3, maxUsos: 50, activo: true },
    { id: 'c_navidad', code: 'NAVIDAD', tipo: 'porcentaje', valor: 30, usos: 41, maxUsos: 41, caduca: now - 60 * DAY, activo: false },
  ];

  const announcements: Announcement[] = [
    { id: 'a_1', titulo: 'Nuevo tema Retro', texto: 'Ya disponible en la tienda.', nivel: 'info', activo: true, desde: now - 5 * DAY },
    { id: 'a_2', titulo: 'Mantenimiento programado', texto: 'El ranking online no estará disponible el domingo de 3:00 a 4:00.', nivel: 'aviso', activo: false, desde: now - 40 * DAY, hasta: now - 38 * DAY },
  ];

  return {
    users,
    venues,
    products: PRODUCTS.map((p) => ({ ...p })),
    orders: orders.sort((a, b) => b.fecha - a.fecha),
    licenses,
    coupons,
    announcements,
    ...seedCatalog(now),
    audit: [{ id: 'au_0', at: now, actor: 'sistema', accion: 'Datos de demostración', detalle: 'Se han creado los datos de ejemplo.' }],
    settings: { ...DEFAULT_SETTINGS },
  };
}

/** Catálogo de ejemplo: unos títulos, unos colores y una colección de Navidad (sin imágenes). */
function seedCatalog(now: number): Pick<AdminData, 'catalogCollections' | 'catalogItems'> {
  const catalogCollections: AdminData['catalogCollections'] = [
    {
      id: 'cc_titulos', tipo: 'titulo', nombre: 'Títulos de vestuario', descripcion: 'Para presumir bajo el nombre.', rareza: 'rara',
      estado: 'publicada', ambito: 'elemento', obtencion: { tipo: 'nivel', nivel: 5 }, disponibilidad: { modo: 'siempre' }, premioCompletar: 200, orden: 0, creada: now - 30 * DAY,
    },
    {
      id: 'cc_colores', tipo: 'color', nombre: 'Neón', descripcion: 'Colores de torneo.', rareza: 'comun',
      estado: 'publicada', ambito: 'coleccion', obtencion: { tipo: 'gratis' }, disponibilidad: { modo: 'siempre' }, orden: 0, creada: now - 30 * DAY,
    },
    {
      id: 'cc_navidad', tipo: 'copa', nombre: 'Navidad', descripcion: 'Copas que solo se consiguen jugando en Navidad.', rareza: 'epica',
      estado: 'borrador', ambito: 'coleccion', obtencion: { tipo: 'evento', partidos: 5 },
      disponibilidad: { modo: 'anual', desdeDia: '12-20', hastaDia: '01-06' }, orden: 0, creada: now - 2 * DAY,
    },
  ];
  const t = (id: string, nombre: string, orden: number, extra: Partial<AdminData['catalogItems'][number]> = {}) => ({
    id, coleccionId: 'cc_titulos', nombre, valor: nombre, orden, activo: true, ...extra,
  });
  const catalogItems: AdminData['catalogItems'] = [
    t('ci_t1', 'El Muro', 0),
    t('ci_t2', 'Pichichi', 1, { obtencion: { tipo: 'logro', logro: 'hat_trick' }, rareza: 'epica' }),
    t('ci_t3', 'Leyenda del bar', 2, { obtencion: { tipo: 'torneos', cantidad: 5 }, rareza: 'legendaria' }),
    { id: 'ci_c1', coleccionId: 'cc_colores', nombre: 'Cian', valor: '#62D6FF', orden: 0, activo: true },
    { id: 'ci_c2', coleccionId: 'cc_colores', nombre: 'Rosa', valor: '#FF5CD6 #B46CFF', orden: 1, activo: true },
    { id: 'ci_c3', coleccionId: 'cc_colores', nombre: 'Lima', valor: '#3DFF9A', orden: 2, activo: true },
  ];
  return { catalogCollections, catalogItems };
}
