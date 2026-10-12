/**
 * Catálogo de personalización: copas, escudos, avatares, marcos, fondos, podios, títulos, colores y sonidos.
 * Se organiza como en los juegos con tienda de aspectos (Fortnite, Clash Royale, Rocket League…):
 *   - Cada pieza pertenece a una COLECCIÓN de un solo tipo («Navidad 2026», «Retro 90»…).
 *   - Rareza: común, rara, épica, legendaria o mítica (color del marco y orden en la app).
 *   - Cómo se consigue: gratis, por nivel, rango, logro, torneos ganados, partidos jugados, con monedas,
 *     con dinero real, con un código, como regalo del administrador o jugando durante un evento.
 *     O la colección entera de una vez, o cada pieza con su propia forma (las que no la tengan, la heredan).
 *   - Cuándo: siempre, entre dos fechas o cada año en las mismas fechas (Navidad, Halloween…). Puede ser
 *     «solo se consigue en esas fechas» (se queda para siempre) o además «solo se puede usar en esas fechas».
 *   - Premio por completar la colección (monedas).
 * Aquí solo hay reglas y utilidades sin pantalla; la pantalla está en sections/Catalogo.tsx.
 */

import {
  FORMATOS_TORNEO,
  MODOS_PARTIDO,
  disponibleAhora,
  obtencionDe,
  type CatalogCollection,
  type CatalogItem,
  type CatalogKind,
  type Disponibilidad,
  type Obtencion,
  type Rareza,
  type TorneoAmbito,
} from '../../../src/services/events/model';

// El modelo (tipos, obtención, disponibilidad) es el mismo que usa la app: vive en src/services/events.
export { disponibleAhora, obtencionDe, FORMATOS_TORNEO, MODOS_PARTIDO };
export type { CatalogCollection, CatalogItem, CatalogKind, Disponibilidad, Obtencion, Rareza, TorneoAmbito };

export interface KindSpec {
  label: string;
  plural: string;
  icono: string;
  /** Para qué se usa en la app. */
  uso: string;
  /** Sin archivo: títulos (texto) y colores (códigos). */
  archivo: 'imagen' | 'audio' | null;
  /** Lo que se acepta al subir (las imágenes se convierten solas a WebP del tamaño correcto). */
  acepta: string[];
  ancho?: number;
  alto?: number;
  /** Necesita fondo transparente. */
  transparente?: boolean;
  /** contain: cabe entera con márgenes transparentes; cover: llena el hueco recortando lo que sobre. */
  ajuste?: 'contain' | 'cover';
  /** Peso máximo del archivo final. */
  maxKB: number;
  /** Cómo preparar el archivo, en una frase. */
  consejo: string;
}

const IMG = ['image/png', 'image/webp', 'image/jpeg'];

/** Tipos que se usan en un torneo y pueden ser exclusivos de liga, copa o pool. */
export const KINDS_DE_TORNEO: CatalogKind[] = ['copa', 'escudo', 'fondo'];
export const TORNEOS_AMBITO: { id: TorneoAmbito; label: string }[] = [
  { id: 'todos', label: 'Todos los torneos' },
  { id: 'league', label: 'Solo ligas' },
  { id: 'bracket', label: 'Solo copas' },
  { id: 'pool', label: 'Solo pools' },
];

export const KINDS: Record<CatalogKind, KindSpec> = {
  copa: {
    label: 'Copa', plural: 'Copas', icono: '🏆', uso: 'Trofeo que se lleva el campeón de un torneo y sale en el palmarés.',
    archivo: 'imagen', acepta: IMG, ancho: 512, alto: 512, transparente: true, ajuste: 'contain', maxKB: 300,
    consejo: 'PNG con fondo transparente, cuadrada, la copa centrada y ocupando casi todo el alto.',
  },
  escudo: {
    label: 'Escudo', plural: 'Escudos', icono: '🛡️', uso: 'Logo de un torneo o de un equipo.',
    archivo: 'imagen', acepta: IMG, ancho: 512, alto: 512, transparente: true, ajuste: 'contain', maxKB: 300,
    consejo: 'PNG con fondo transparente, cuadrado, el escudo centrado con un poco de margen.',
  },
  avatar: {
    label: 'Avatar', plural: 'Avatares', icono: '🙂', uso: 'Imagen de perfil para quien no quiera usar su foto.',
    archivo: 'imagen', acepta: IMG, ancho: 512, alto: 512, ajuste: 'cover', maxKB: 250,
    consejo: 'Cuadrado, con fondo de color o ilustración; la cara centrada (se recorta en círculo).',
  },
  marco: {
    label: 'Marco', plural: 'Marcos', icono: '🖼️', uso: 'Borde decorativo alrededor de la foto del jugador.',
    archivo: 'imagen', acepta: IMG, ancho: 512, alto: 512, transparente: true, ajuste: 'contain', maxKB: 300,
    consejo: 'PNG transparente cuadrado con el CENTRO VACÍO (la foto va debajo): el hueco ocupa el 70 % central.',
  },
  fondo: {
    label: 'Fondo', plural: 'Fondos', icono: '🌄', uso: 'Imagen del banner del torneo en el palmarés y en su ficha.',
    archivo: 'imagen', acepta: IMG, ancho: 1600, alto: 400, ajuste: 'cover', maxKB: 450,
    consejo: 'Horizontal 4:1 (1600×400). Lo importante en el centro: los lados pueden recortarse en pantallas estrechas.',
  },
  podio: {
    label: 'Podio', plural: 'Podios', icono: '🥇', uso: 'Escena del podio 1º-2º-3º en la ceremonia del final del torneo.',
    archivo: 'imagen', acepta: IMG, ancho: 1200, alto: 600, transparente: true, ajuste: 'contain', maxKB: 450,
    consejo: 'PNG transparente 2:1 (1200×600): el 1º en el centro y más alto, el 2º a la izquierda y el 3º a la derecha.',
  },
  titulo: {
    label: 'Título', plural: 'Títulos', icono: '🏷️', uso: 'Texto que el jugador muestra bajo su nombre («El Muro», «Pichichi»…).',
    archivo: null, acepta: [], maxKB: 0,
    consejo: 'Sin archivo: escribe un título por línea, de 24 caracteres como mucho.',
  },
  color: {
    label: 'Color', plural: 'Colores', icono: '🎨', uso: 'Color principal de un torneo (banner, carteles y ceremonia).',
    archivo: null, acepta: [], maxKB: 0,
    consejo: 'Sin archivo: una línea por color, «Nombre #RRGGBB» y, si quieres, un segundo color «#RRGGBB» para el degradado.',
  },
  sonido: {
    label: 'Sonido', plural: 'Sonidos', icono: '🎵', uso: 'Himno del campeón o sonido de gol.',
    archivo: 'audio', acepta: ['audio/mpeg', 'audio/ogg', 'audio/wav', 'audio/x-wav'], maxKB: 400,
    consejo: 'MP3 u OGG de 10 segundos como mucho, sin silencio al principio.',
  },
};

export const KIND_ORDER: CatalogKind[] = ['copa', 'escudo', 'avatar', 'marco', 'fondo', 'podio', 'titulo', 'color', 'sonido'];

export const RAREZAS: { id: Rareza; label: string; color: string }[] = [
  { id: 'comun', label: 'Común', color: '#9aa4b2' },
  { id: 'rara', label: 'Rara', color: '#4aa8ff' },
  { id: 'epica', label: 'Épica', color: '#b46cff' },
  { id: 'legendaria', label: 'Legendaria', color: '#f2c94c' },
  { id: 'mitica', label: 'Mítica', color: '#ff5c8a' },
];
export const rareza = (id: Rareza | undefined) => RAREZAS.find((r) => r.id === id) ?? RAREZAS[0];

export type ObtencionTipo = Obtencion['tipo'];

export const OBTENCION: { tipo: ObtencionTipo; label: string; ayuda: string; proximamente?: boolean }[] = [
  { tipo: 'gratis', label: 'Gratis', ayuda: 'Todos la tienen desde el principio.' },
  { tipo: 'nivel', label: 'Llegar a un nivel', ayuda: 'Se desbloquea al alcanzar ese nivel de experiencia.' },
  { tipo: 'rango', label: 'Alcanzar un rango', ayuda: 'Por el ELO máximo alcanzado (Bronce, Plata, Oro…).' },
  { tipo: 'logro', label: 'Conseguir un logro', ayuda: 'Premio por un logro concreto.' },
  { tipo: 'torneos', label: 'Ganar torneos', ayuda: 'Al ganar ese número de torneos.' },
  { tipo: 'partidos', label: 'Jugar partidos', ayuda: 'Al jugar ese número de partidos (ganados o no).' },
  { tipo: 'entrar', label: 'Entrar en la app (evento)', ayuda: 'Sin requisitos: se consigue solo con abrir la app en las fechas del evento.' },
  { tipo: 'jugarPartido', label: 'Jugar un partido (evento)', ayuda: 'Jugando un partido del tipo elegido en las fechas del evento.' },
  { tipo: 'ganarPartido', label: 'Ganar un partido (evento)', ayuda: 'Ganando un partido del tipo elegido en las fechas del evento.' },
  { tipo: 'ganarTorneo', label: 'Ganar un torneo (evento)', ayuda: 'Ganando una liga, una copa o un pool (o cualquiera) en las fechas.' },
  { tipo: 'evento', label: 'Jugar N partidos (evento)', ayuda: 'Jugando ese número de partidos en las fechas del evento.' },
  { tipo: 'monedas', label: 'Comprar con monedas', ayuda: 'Con la moneda del juego, en la tienda.', proximamente: true },
  { tipo: 'pago', label: 'Comprar con dinero', ayuda: 'Compra con dinero real.', proximamente: true },
  { tipo: 'codigo', label: 'Código promocional', ayuda: 'Se canjea escribiendo un código (sorteos, redes, kits…).', proximamente: true },
  { tipo: 'regalo', label: 'Solo como regalo', ayuda: 'No se puede conseguir: solo la regala el administrador a quien quiera.', proximamente: true },
];

export function obtencionPorDefecto(tipo: ObtencionTipo): Obtencion {
  switch (tipo) {
    case 'nivel': return { tipo, nivel: 10 };
    case 'rango': return { tipo, rango: 'gold' };
    case 'logro': return { tipo, logro: '' };
    case 'torneos': return { tipo, cantidad: 1 };
    case 'partidos': return { tipo, cantidad: 50 };
    case 'monedas': return { tipo, precio: 500 };
    case 'pago': return { tipo, precio: 199 };
    case 'codigo': return { tipo, codigo: '' };
    case 'evento': return { tipo, partidos: 3 };
    case 'jugarPartido': return { tipo, modo: 'cualquiera' };
    case 'ganarPartido': return { tipo, modo: 'cualquiera' };
    case 'ganarTorneo': return { tipo, formato: 'cualquiera' };
    default: return { tipo } as Obtencion;
  }
}

export function obtencionTexto(o: Obtencion, nombres: { rango?: (id: string) => string; logro?: (id: string) => string } = {}): string {
  switch (o.tipo) {
    case 'gratis': return 'Gratis';
    case 'nivel': return `Nivel ${o.nivel}`;
    case 'rango': return `Rango ${nombres.rango?.(o.rango) ?? o.rango}`;
    case 'logro': return o.logro ? `Logro «${nombres.logro?.(o.logro) ?? o.logro}»` : 'Logro (sin elegir)';
    case 'torneos': return `Ganar ${o.cantidad} torneo${o.cantidad === 1 ? '' : 's'}`;
    case 'partidos': return `Jugar ${o.cantidad} partidos`;
    case 'monedas': return `🪙 ${o.precio}`;
    case 'pago': return `${(o.precio / 100).toFixed(2).replace('.', ',')} €`;
    case 'codigo': return o.codigo ? `Código ${o.codigo}` : 'Código (sin poner)';
    case 'regalo': return 'Regalo';
    case 'evento': return `Evento: ${o.partidos} partido${o.partidos === 1 ? '' : 's'}`;
    case 'entrar': return 'Evento: entrar en la app';
    case 'jugarPartido': return `Evento: jugar ${MODOS_PARTIDO.find((m) => m.id === o.modo)?.label ?? 'un partido'}`;
    case 'ganarPartido': return `Evento: ganar ${MODOS_PARTIDO.find((m) => m.id === o.modo)?.label ?? 'un partido'}`;
    case 'ganarTorneo': return `Evento: ganar ${FORMATOS_TORNEO.find((f) => f.id === o.formato)?.label ?? 'un torneo'}`;
  }
}

/** Errores que impiden publicar. */
export function obtencionErrores(o: Obtencion, d: Disponibilidad): string[] {
  const e: string[] = [];
  if (o.tipo === 'nivel' && !(o.nivel >= 1)) e.push('El nivel debe ser 1 o más.');
  if ((o.tipo === 'torneos' || o.tipo === 'partidos') && !(o.cantidad >= 1)) e.push('La cantidad debe ser 1 o más.');
  if (o.tipo === 'evento' && !(o.partidos >= 1)) e.push('El evento debe pedir al menos 1 partido.');
  if ((o.tipo === 'monedas' || o.tipo === 'pago') && !(o.precio > 0)) e.push('El precio debe ser mayor que 0.');
  if (o.tipo === 'logro' && !o.logro) e.push('Elige el logro.');
  if (o.tipo === 'codigo' && !/^[A-Z0-9-]{4,20}$/.test(o.codigo)) e.push('El código: 4 a 20 letras, números o guiones.');
  if (['evento', 'entrar', 'jugarPartido', 'ganarPartido', 'ganarTorneo'].includes(o.tipo) && d.modo === 'siempre')
    e.push('Un evento necesita fechas: elige «Entre dos fechas» o «Cada año».');
  return e;
}

export const TEMPORADAS: { nombre: string; desdeDia: string; hastaDia: string }[] = [
  { nombre: '🎄 Navidad', desdeDia: '12-20', hastaDia: '01-06' },
  { nombre: '❤️ San Valentín', desdeDia: '02-10', hastaDia: '02-15' },
  { nombre: '🎭 Carnaval', desdeDia: '02-15', hastaDia: '03-05' },
  { nombre: '☀️ Verano', desdeDia: '06-21', hastaDia: '08-31' },
  { nombre: '🎃 Halloween', desdeDia: '10-25', hastaDia: '11-02' },
];


const MESES = ['ene', 'feb', 'mar', 'abr', 'may', 'jun', 'jul', 'ago', 'sept', 'oct', 'nov', 'dic'];
const diaTexto = (md: string) => {
  const [m, d] = md.split('-').map(Number);
  return `${d} ${MESES[m - 1] ?? '?'}`;
};

export function disponibilidadTexto(d: Disponibilidad): string {
  if (d.modo === 'siempre') return 'Siempre';
  const fin = d.soloUsoEnFechas ? ' · solo se usa en esas fechas' : '';
  if (d.modo === 'anual') return `Cada año, ${diaTexto(d.desdeDia ?? '01-01')} – ${diaTexto(d.hastaDia ?? '12-31')}${fin}`;
  const f = (t?: number) => (t === undefined ? '…' : new Date(t).toLocaleDateString('es-ES', { day: 'numeric', month: 'short', year: 'numeric' }));
  return `Del ${f(d.desde)} al ${f(d.hasta)}${fin}`;
}

export function nuevaColeccion(tipo: CatalogKind, id: string, now: number, orden: number): CatalogCollection {
  return {
    id, tipo, nombre: '', descripcion: '', rareza: 'comun', estado: 'borrador', ambito: 'coleccion',
    obtencion: { tipo: 'gratis' }, disponibilidad: { modo: 'siempre' }, torneo: 'todos', orden, creada: now,
  };
}

/** «copa-dorada_02.png» → «Copa dorada 02». */
export function nombreDesdeArchivo(file: string): string {
  const base = file.replace(/\.[a-z0-9]+$/i, '').replace(/[-_]+/g, ' ').replace(/\s+/g, ' ').trim();
  return base ? base[0].toUpperCase() + base.slice(1) : 'Sin nombre';
}

/** Líneas de títulos: quita vacías y repetidas; avisa de las largas. */
export function leerTitulos(texto: string): { valores: string[]; errores: string[] } {
  const valores: string[] = [];
  const errores: string[] = [];
  for (const raw of texto.split('\n')) {
    const t = raw.trim();
    if (!t) continue;
    if (t.length > 24) errores.push(`«${t}» tiene más de 24 caracteres.`);
    else if (!valores.includes(t)) valores.push(t);
  }
  return { valores, errores };
}

/** Líneas de colores: «Nombre #RRGGBB [#RRGGBB]». */
export function leerColores(texto: string): { colores: { nombre: string; valor: string }[]; errores: string[] } {
  const colores: { nombre: string; valor: string }[] = [];
  const errores: string[] = [];
  for (const raw of texto.split('\n')) {
    const t = raw.trim();
    if (!t) continue;
    const hex = t.match(/#[0-9a-f]{6}\b/gi) ?? [];
    const nombre = t.replace(/#[0-9a-f]{6}\b/gi, '').trim();
    if (!hex.length || hex.length > 2) errores.push(`«${t}»: pon uno o dos colores como #1E90FF.`);
    else colores.push({ nombre: nombre || hex[0]!.toUpperCase(), valor: hex.map((h) => h.toUpperCase()).join(' ') });
  }
  return { colores, errores };
}

/** Comprueba un archivo antes de prepararlo. */
export function revisarArchivo(tipo: CatalogKind, f: { name: string; type: string; size: number }): string | null {
  const spec = KINDS[tipo];
  if (!spec.archivo) return 'Este tipo no lleva archivo.';
  if (!spec.acepta.includes(f.type)) return `${f.name}: formato no válido (${spec.archivo === 'audio' ? 'MP3, OGG o WAV' : 'PNG, WebP o JPG'}).`;
  // Las imágenes se reducen solas; aun así, nada de archivos enormes.
  const limite = spec.archivo === 'imagen' ? 15 * 1024 * 1024 : spec.maxKB * 1024;
  if (f.size > limite) return `${f.name}: pesa demasiado (máximo ${spec.archivo === 'imagen' ? '15 MB' : `${spec.maxKB} KB`}).`;
  return null;
}

/** Avisos de una imagen según su tamaño original (no impiden subirla). */
export function avisosImagen(tipo: CatalogKind, f: { name: string; type: string }, ancho: number, alto: number): string[] {
  const spec = KINDS[tipo];
  const out: string[] = [];
  if (!spec.ancho || !spec.alto) return out;
  if (ancho < spec.ancho / 2 || alto < spec.alto / 2) out.push(`${f.name}: es pequeña (${ancho}×${alto}); puede verse borrosa.`);
  const prop = ancho / alto / (spec.ancho / spec.alto);
  if (prop > 1.1 || prop < 0.9)
    out.push(`${f.name}: no tiene la proporción ${spec.ancho}×${spec.alto}; se ha ${spec.ajuste === 'cover' ? 'recortado' : 'ajustado con márgenes'}.`);
  if (spec.transparente && f.type === 'image/jpeg') out.push(`${f.name}: JPG no tiene fondo transparente; se verá con fondo.`);
  return out;
}
