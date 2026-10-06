/**
 * Imágenes opcionales: se detectan en src/assets/images al compilar (sin peticiones
 * fallidas). Si una imagen no existe, se pinta el respaldo en SVG/CSS.
 */
import type { CSSProperties, ReactNode } from 'react';

const files = import.meta.glob('../../assets/images/*.{png,webp,jpg,jpeg,svg}', {
  eager: true,
  query: '?url',
  import: 'default',
}) as Record<string, string>;

const byName = new Map<string, string>();
for (const [path, url] of Object.entries(files)) {
  const base = path.split('/').pop()!.replace(/\.(png|webp|jpe?g|svg)$/i, '');
  byName.set(base, url);
}

/** Nombres de las imágenes que empiezan por `prefix`, en orden (`escudo-01`, `escudo-02`…). */
export function assetNames(prefix: string): string[] {
  return [...byName.keys()].filter((n) => n.startsWith(prefix)).sort((a, b) => a.localeCompare(b, 'es', { numeric: true }));
}

export function assetUrl(name: string): string | undefined {
  return byName.get(name);
}

export function AssetImage({
  name,
  alt = '',
  className,
  fallback,
  style,
}: {
  name: string;
  alt?: string;
  className?: string;
  fallback: ReactNode;
  style?: CSSProperties;
}) {
  const url = assetUrl(name);
  if (!url) return <>{fallback}</>;
  return <img src={url} alt={alt} className={className} style={style} draggable={false} />;
}
