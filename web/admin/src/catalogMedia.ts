/**
 * Preparar archivos del catálogo en el navegador antes de subirlos:
 * - Imágenes: se ajustan al tamaño de su tipo (con márgenes transparentes o recortando) y se pasan a WebP,
 *   bajando la calidad hasta que quepan en el peso máximo. Así da igual cómo venga la imagen original.
 * - Sonidos: se comprueba la duración.
 */
import { KINDS, avisosImagen, revisarArchivo, type CatalogKind } from './catalog';

export interface Preparado {
  blob: Blob;
  extension: string;
  avisos: string[];
}

function cargarImagen(file: File): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const url = URL.createObjectURL(file);
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error(`${file.name}: no se puede abrir la imagen.`));
    img.src = url;
  });
}

const aBlob = (c: HTMLCanvasElement, calidad: number) =>
  new Promise<Blob | null>((resolve) => c.toBlob(resolve, 'image/webp', calidad));

function duracion(file: File): Promise<number> {
  return new Promise((resolve) => {
    const a = document.createElement('audio');
    a.preload = 'metadata';
    a.onloadedmetadata = () => resolve(a.duration);
    a.onerror = () => resolve(NaN);
    a.src = URL.createObjectURL(file);
  });
}

export async function preparar(tipo: CatalogKind, file: File): Promise<Preparado> {
  const error = revisarArchivo(tipo, file);
  if (error) throw new Error(error);
  const spec = KINDS[tipo];
  if (spec.archivo === 'audio') {
    const s = await duracion(file);
    const avisos = s > 12 ? [`${file.name}: dura ${Math.round(s)} s; mejor 10 s o menos.`] : [];
    const ext = file.type === 'audio/ogg' ? 'ogg' : file.type.includes('wav') ? 'wav' : 'mp3';
    return { blob: file, extension: ext, avisos };
  }

  const img = await cargarImagen(file);
  const w = spec.ancho!;
  const h = spec.alto!;
  const canvas = document.createElement('canvas');
  canvas.width = w;
  canvas.height = h;
  const ctx = canvas.getContext('2d')!;
  const escala = spec.ajuste === 'cover' ? Math.max(w / img.naturalWidth, h / img.naturalHeight) : Math.min(w / img.naturalWidth, h / img.naturalHeight);
  const dw = img.naturalWidth * escala;
  const dh = img.naturalHeight * escala;
  ctx.imageSmoothingQuality = 'high';
  ctx.drawImage(img, (w - dw) / 2, (h - dh) / 2, dw, dh);
  URL.revokeObjectURL(img.src);

  let blob: Blob | null = null;
  for (const calidad of [0.92, 0.85, 0.75, 0.65, 0.5]) {
    blob = await aBlob(canvas, calidad);
    if (blob && blob.size <= spec.maxKB * 1024) break;
  }
  if (!blob) throw new Error(`${file.name}: el navegador no pudo convertir la imagen.`);
  if (blob.size > spec.maxKB * 1024) throw new Error(`${file.name}: aun comprimida pesa más de ${spec.maxKB} KB; simplifica la imagen.`);
  return { blob, extension: 'webp', avisos: avisosImagen(tipo, file, img.naturalWidth, img.naturalHeight) };
}
