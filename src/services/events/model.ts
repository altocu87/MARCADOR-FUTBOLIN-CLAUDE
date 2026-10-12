/**
 * Modelo del catálogo de personalización compartido por el panel de administración y la app:
 * cómo se consigue una pieza (obtención), cuándo (disponibilidad) y a qué torneos aplica.
 * El panel lo edita y lo guarda en Supabase; la app lo descarga y decide quién ha conseguido qué.
 */
import type { TournamentFormat } from '../persistence';

export type CatalogKind = 'copa' | 'escudo' | 'avatar' | 'marco' | 'fondo' | 'podio' | 'titulo' | 'color' | 'sonido';
export type Rareza = 'comun' | 'rara' | 'epica' | 'legendaria' | 'mitica';

/**
 * Dónde cuenta un partido: cualquiera, uno suelto de una modalidad (rápido, loco o clasificatorio, fuera
 * de torneos) o uno de un torneo de liga, copa o pool.
 */
export type ModoPartido = 'cualquiera' | 'quick' | 'chaos' | 'ranked' | TournamentFormat;
/** Qué torneo hay que ganar: cualquiera o uno de un tipo. */
export type FormatoTorneo = 'cualquiera' | TournamentFormat;

export const MODOS_PARTIDO: { id: ModoPartido; label: string }[] = [
  { id: 'cualquiera', label: 'cualquier partido' },
  { id: 'quick', label: 'un partido rápido' },
  { id: 'chaos', label: 'un partido loco' },
  { id: 'ranked', label: 'un partido clasificatorio' },
  { id: 'league', label: 'un partido de liga' },
  { id: 'bracket', label: 'un partido de copa' },
  { id: 'pool', label: 'un partido de pool' },
];
export const FORMATOS_TORNEO: { id: FormatoTorneo; label: string }[] = [
  { id: 'cualquiera', label: 'un torneo' },
  { id: 'league', label: 'una liga' },
  { id: 'bracket', label: 'una copa' },
  { id: 'pool', label: 'un pool' },
];

/** Cómo se consigue una pieza o una colección. */
export type Obtencion =
  | { tipo: 'gratis' }
  | { tipo: 'nivel'; nivel: number }
  | { tipo: 'rango'; rango: string }
  | { tipo: 'logro'; logro: string }
  | { tipo: 'torneos'; cantidad: number }
  | { tipo: 'partidos'; cantidad: number }
  | { tipo: 'monedas'; precio: number }
  /** Céntimos. */
  | { tipo: 'pago'; precio: number }
  | { tipo: 'codigo'; codigo: string }
  | { tipo: 'regalo' }
  /** Jugar N partidos mientras dura el evento (la disponibilidad marca las fechas). */
  | { tipo: 'evento'; partidos: number }
  /** Sin requisitos: basta con abrir la app en las fechas del evento. */
  | { tipo: 'entrar' }
  /** Jugar un partido (de ese tipo) en las fechas. */
  | { tipo: 'jugarPartido'; modo: ModoPartido }
  /** Ganar un partido (de ese tipo) en las fechas. */
  | { tipo: 'ganarPartido'; modo: ModoPartido }
  /** Ganar un torneo (de ese tipo) en las fechas. */
  | { tipo: 'ganarTorneo'; formato: FormatoTorneo };

/** Cuándo se puede conseguir. */
export interface Disponibilidad {
  modo: 'siempre' | 'fechas' | 'anual';
  /** modo fechas: milisegundos. */
  desde?: number;
  hasta?: number;
  /** modo anual: «MM-DD» (puede cruzar el año: 12-20 → 01-06). */
  desdeDia?: string;
  hastaDia?: string;
  /** Además, solo se puede USAR en esas fechas (fuera de ellas sigue guardada pero no se puede elegir). */
  soloUsoEnFechas?: boolean;
}

/** A qué torneos aplica una colección de copas, escudos o fondos (banners). Sin poner: a todos. */
export type TorneoAmbito = 'todos' | TournamentFormat;

export interface CatalogCollection {
  id: string;
  tipo: CatalogKind;
  nombre: string;
  descripcion: string;
  rareza: Rareza;
  /** borrador: no se ve en la app; publicada: sí; retirada: ya no se consigue (quien la tenga la conserva). */
  estado: 'borrador' | 'publicada' | 'retirada';
  /** coleccion: se consigue entera de una vez; elemento: cada pieza por su cuenta (con su forma o la de la colección). */
  ambito: 'coleccion' | 'elemento';
  obtencion: Obtencion;
  disponibilidad: Disponibilidad;
  /** Copas, escudos y fondos: solo para ese tipo de torneo (los de la liga no salen en la copa ni en el pool). */
  torneo?: TorneoAmbito;
  premioCompletar?: number;
  orden: number;
  creada: number;
}

export interface CatalogItem {
  id: string;
  coleccionId: string;
  nombre: string;
  rareza?: Rareza;
  /** Dirección pública del archivo (imagen o sonido), o el nombre de una imagen de la app. */
  archivo?: string;
  ruta?: string;
  valor?: string;
  obtencion?: Obtencion;
  orden: number;
  activo: boolean;
}

/** Cómo se consigue una pieza (la suya si la colección va pieza a pieza y la tiene; si no, la de la colección). */
export const obtencionDe = (item: CatalogItem, col: CatalogCollection): Obtencion =>
  (col.ambito === 'elemento' && item.obtencion) || col.obtencion;

/** Formas de conseguir que solo tienen sentido en unas fechas (un evento). */
export const OBTENCION_DE_EVENTO: Obtencion['tipo'][] = ['evento', 'entrar', 'jugarPartido', 'ganarPartido', 'ganarTorneo'];

const mmdd = (t: number) => {
  const d = new Date(t);
  return `${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
};

/** ¿Se puede conseguir ahora? */
export function disponibleAhora(d: Disponibilidad, now: number): boolean {
  if (d.modo === 'siempre') return true;
  if (d.modo === 'fechas') return (d.desde === undefined || now >= d.desde) && (d.hasta === undefined || now <= d.hasta);
  if (!d.desdeDia || !d.hastaDia) return false;
  const hoy = mmdd(now);
  // Cruza el año (Navidad): vale desde el inicio hasta fin de año y desde enero hasta el final.
  return d.desdeDia <= d.hastaDia ? hoy >= d.desdeDia && hoy <= d.hastaDia : hoy >= d.desdeDia || hoy <= d.hastaDia;
}

/** «ganando un partido de liga», «entrando en la app», «ganando una copa»… (para el aviso y los candados). */
export function requisitoTexto(o: Obtencion): string {
  const modo = (m: ModoPartido) => MODOS_PARTIDO.find((x) => x.id === m)?.label ?? 'un partido';
  switch (o.tipo) {
    case 'entrar':
    case 'gratis':
      return 'entrando en la app';
    case 'jugarPartido':
      return `jugando ${modo(o.modo)}`;
    case 'ganarPartido':
      return `ganando ${modo(o.modo)}`;
    case 'ganarTorneo':
      return `ganando ${FORMATOS_TORNEO.find((x) => x.id === o.formato)?.label ?? 'un torneo'}`;
    case 'evento':
      return `jugando ${o.partidos} partido${o.partidos === 1 ? '' : 's'}`;
    case 'logro':
      return 'consiguiendo un logro';
    case 'nivel':
      return `llegando al nivel ${o.nivel}`;
    case 'rango':
      return 'alcanzando un rango';
    case 'torneos':
      return `ganando ${o.cantidad} torneo${o.cantidad === 1 ? '' : 's'}`;
    case 'partidos':
      return `jugando ${o.cantidad} partidos`;
    default:
      return 'de otra forma';
  }
}

const MESES_CORTOS = ['ene', 'feb', 'mar', 'abr', 'may', 'jun', 'jul', 'ago', 'sept', 'oct', 'nov', 'dic'];
const mdTexto = (md: string) => {
  const [m, d] = md.split('-').map(Number);
  return `${d} ${MESES_CORTOS[m - 1] ?? '?'}`;
};

/** «el 31 oct», «del 25 oct al 2 nov», «cada año el 31 oct», «siempre» (para el aviso y los candados). */
export function cuandoTexto(d: Disponibilidad): string {
  if (d.modo === 'siempre') return 'siempre';
  const rango = (a: string, b: string) => (a === b ? `el ${a}` : `del ${a} al ${b}`);
  if (d.modo === 'anual') return `cada año ${rango(mdTexto(d.desdeDia ?? '01-01'), mdTexto(d.hastaDia ?? '12-31'))}`;
  if (d.desde === undefined || d.hasta === undefined) return d.hasta !== undefined ? `hasta el ${mdTexto(mmdd(d.hasta))}` : 'desde ya';
  return rango(mdTexto(mmdd(d.desde)), mdTexto(mmdd(d.hasta)));
}
