/**
 * Logos (escudos) de torneos y equipos, y copas de los torneos.
 * - Los logos son imágenes `escudo-*` y las copas `copa-*` de src/assets/images: se detectan solas.
 * - Mientras no haya imágenes se usan unos provisionales: emojis en un escudo de neón y el trofeo
 *   de la app teñido de varios colores. Lo guardado es solo el id (`escudo-07`, `emoji:🦅`, `trofeo:plata`).
 */
import type { CSSProperties } from 'react';
import { Modal } from './common';
import { AssetImage, assetNames, assetUrl } from './assets';

const EMOJI_LOGOS = ['🦅', '🐺', '🦁', '🐯', '🦈', '🐉', '🔥', '⚡', '🚀', '👑', '💀', '🌪️', '🐍', '🦂', '🐻', '🦊', '🐝', '🐙', '🎯', '💎', '🛡️', '⚔️', '🌟', '☄️'];
const LOGO_COLORS = ['#62d6ff', '#ff5cd6', '#ffcc33', '#3dff9a', '#b46cff', '#ff8a2a', '#ff4d6d', '#8fd0ff'];

const CUP_TINTS: Record<string, { label: string; filter: string }> = {
  oro: { label: 'Oro', filter: 'none' },
  plata: { label: 'Plata', filter: 'grayscale(1) brightness(1.35)' },
  bronce: { label: 'Bronce', filter: 'sepia(1) saturate(1.6) hue-rotate(-18deg) brightness(0.85)' },
  cian: { label: 'Cian', filter: 'hue-rotate(150deg) saturate(1.4)' },
  morado: { label: 'Morado', filter: 'hue-rotate(230deg) saturate(1.3)' },
  verde: { label: 'Verde', filter: 'hue-rotate(80deg) saturate(1.3)' },
  rojo: { label: 'Rojo', filter: 'hue-rotate(-45deg) saturate(1.8)' },
};

/** Logos disponibles: las imágenes `escudo-*` o, si aún no hay, los provisionales. */
export function logoCatalog(): string[] {
  const imgs = assetNames('escudo-');
  return imgs.length ? imgs : EMOJI_LOGOS.map((e) => `emoji:${e}`);
}

/** Copas disponibles: las imágenes `copa-*` o, si aún no hay, el trofeo en varios colores. */
export function cupCatalog(): string[] {
  const imgs = assetNames('copa-');
  return imgs.length ? imgs : Object.keys(CUP_TINTS).map((k) => `trofeo:${k}`);
}

/** Un logo para empezar, siempre el mismo para la misma semilla (nombre del equipo o torneo). */
export function defaultLogo(seed: string): string {
  const list = logoCatalog();
  let h = 0;
  for (const c of seed) h = (h * 31 + c.charCodeAt(0)) >>> 0;
  return list[h % list.length];
}

export function defaultCup(): string {
  return cupCatalog()[0];
}

/** Escudo de un torneo o equipo. */
export function Crest({ id, size = 40, className = '', title }: { id?: string; size?: number; className?: string; title?: string }) {
  const style = { width: size, height: size, fontSize: size * 0.56 } as CSSProperties;
  if (id && assetUrl(id)) {
    return <img className={`crest crest-img ${className}`} src={assetUrl(id)} alt={title ?? ''} style={style} draggable={false} />;
  }
  const emoji = id?.startsWith('emoji:') ? id.slice(6) : '⚽';
  const i = Math.max(0, EMOJI_LOGOS.indexOf(emoji));
  return (
    <span className={`crest crest-emoji ${className}`} style={{ ...style, '--crest': LOGO_COLORS[i % LOGO_COLORS.length] } as CSSProperties} title={title} aria-hidden={!title}>
      {emoji}
    </span>
  );
}

/** Copa de un torneo. */
export function Cup({ id, size = 48, className = '' }: { id?: string; size?: number; className?: string }) {
  const style: CSSProperties = { width: size, height: size, objectFit: 'contain' };
  if (id && assetUrl(id)) return <img className={`cup ${className}`} src={assetUrl(id)} alt="" style={style} draggable={false} />;
  const tint = CUP_TINTS[id?.startsWith('trofeo:') ? id.slice(7) : 'oro'] ?? CUP_TINTS.oro;
  return (
    <AssetImage
      name="trofeo"
      className={`cup ${className}`}
      style={{ ...style, filter: tint.filter }}
      fallback={<span className={`cup ${className}`} style={{ fontSize: size * 0.8, filter: tint.filter }}>🏆</span>}
    />
  );
}

/** Muestrario para elegir un logo o una copa. */
export function IconPicker({
  kind,
  value,
  onPick,
  onClose,
  title,
}: {
  kind: 'logo' | 'cup';
  value?: string;
  onPick: (id: string) => void;
  onClose: () => void;
  title?: string;
}) {
  const list = kind === 'logo' ? logoCatalog() : cupCatalog();
  return (
    <Modal
      title={title ?? (kind === 'logo' ? 'Elige un logo' : 'Elige la copa')}
      onClose={onClose}
      actions={
        <button className="btn btn-ghost" onClick={onClose}>
          Cerrar
        </button>
      }
    >
      <div className={`icon-grid icon-grid-${kind}`}>
        {list.map((id) => (
          <button
            key={id}
            className={`icon-cell ${id === value ? 'is-on' : ''}`}
            aria-pressed={id === value}
            onClick={() => {
              onPick(id);
              onClose();
            }}
          >
            {kind === 'logo' ? <Crest id={id} size={56} /> : <Cup id={id} size={64} />}
          </button>
        ))}
      </div>
    </Modal>
  );
}
