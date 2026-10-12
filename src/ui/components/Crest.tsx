/**
 * Logos (escudos) de torneos y equipos, y copas de los torneos.
 * - Los logos son imágenes `escudo-*` y las copas `copa-*` de src/assets/images: se detectan solas.
 * - Mientras no haya imágenes se usan unos provisionales: emojis en un escudo de neón y el trofeo
 *   de la app teñido de varios colores. Lo guardado es solo el id (`escudo-07`, `emoji:🦅`, `trofeo:plata`).
 */
import { useState, type CSSProperties } from 'react';
import { useApp } from '../../app/AppContext';
import {
  ICON_PREFIX,
  allIconCategories,
  catalogCategoryItems,
  catalogIconInfo,
  iconCategory,
  isUnlocked,
  unlockText,
  type IconCategory,
  type IconKind,
} from '../../services/icons';
import { ownedBy } from '../../services/events';
import type { TournamentFormat } from '../../services/persistence';
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

/** Imágenes de una categoría; los genéricos, si aún no hay imágenes, son los provisionales. */
export function categoryItems(kind: IconKind, category: string): string[] {
  const fromCatalog = catalogCategoryItems(kind, category);
  if (fromCatalog) return fromCatalog;
  const imgs = assetNames(ICON_PREFIX[kind]).filter((n) => iconCategory(n, kind) === category);
  // Los banners no tienen provisionales: sin imágenes no hay banner.
  if (imgs.length || category !== 'generico' || kind === 'banner') return imgs;
  return kind === 'logo' ? EMOJI_LOGOS.map((e) => `emoji:${e}`) : Object.keys(CUP_TINTS).map((k) => `trofeo:${k}`);
}

/**
 * Categorías que ya tienen algo que enseñar. Con un tipo de torneo que tenga las suyas propias, solo
 * esas (los iconos de la liga no se mezclan con los de la copa o el pool); si no, las comunes.
 */
export function iconCategories(kind: IconKind, format?: TournamentFormat): IconCategory[] {
  const shown = allIconCategories().filter((c) => c.kind === kind && categoryItems(kind, c.id).length > 0);
  // Los del catálogo para todos los torneos salen en cualquiera; los de un tipo, solo en ese.
  const forAll = shown.filter((c) => c.catalog && !c.format);
  const own = format ? shown.filter((c) => c.format === format) : [];
  const base = own.length ? own : shown.filter((c) => !c.format && !c.catalog);
  return [...[...forAll, ...base].filter((c) => c.catalog), ...base.filter((c) => !c.catalog)];
}

/** Logos de por defecto: los del tipo de torneo, si tiene propios, o los genéricos. */
export function logoCatalog(format?: TournamentFormat): string[] {
  // Las de eventos van con candado: nunca son las de por defecto.
  const cat = iconCategories('logo', format).find((c) => !c.catalog);
  return cat ? categoryItems('logo', cat.id) : categoryItems('logo', 'generico');
}

/** Copas genéricas. */
export function cupCatalog(): string[] {
  return categoryItems('cup', 'generico');
}

/** Un logo para empezar, siempre el mismo para la misma semilla (nombre del equipo o torneo). */
export function defaultLogo(seed: string, format?: TournamentFormat): string {
  const list = logoCatalog(format);
  let h = 0;
  for (const c of seed) h = (h * 31 + c.charCodeAt(0)) >>> 0;
  return list[h % list.length];
}

export function defaultCup(): string {
  return cupCatalog()[0];
}

/** Banners que se pueden elegir en este tipo de torneo (puede que ninguno). */
export function bannerCatalog(format?: TournamentFormat): string[] {
  return iconCategories('banner', format)
    .filter((c) => !c.catalog)
    .flatMap((c) => categoryItems('banner', c.id));
}

/** Un banner para empezar (siempre el mismo para la misma semilla), o ninguno si no hay. */
export function defaultBanner(seed: string, format?: TournamentFormat): string | undefined {
  const list = bannerCatalog(format);
  if (!list.length) return undefined;
  let h = 7;
  for (const c of seed) h = (h * 31 + c.charCodeAt(0)) >>> 0;
  return list[h % list.length];
}

/** Banner de un torneo: la franja de imagen, a todo el ancho de quien la contiene. */
export function BannerArt({ id, className = '' }: { id?: string; className?: string }) {
  const url = id ? assetUrl(id) : undefined;
  return url ? <img className={`banner-art ${className}`} src={url} alt="" draggable={false} /> : null;
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

/**
 * Muestrario con pestañas por categoría. Las bloqueadas se ven apagadas con su candado y lo que
 * hace falta; se desbloquean si lo cumple alguno de los jugadores (`playerIds`).
 */
export function IconGrid({
  kind,
  value,
  onPick,
  playerIds,
  size = 56,
  format,
}: {
  kind: IconKind;
  value?: string;
  onPick: (id: string) => void;
  playerIds: string[];
  size?: number;
  /** Tipo de torneo: con iconos propios, solo esos. Sin él (equipos), los comunes. */
  format?: TournamentFormat;
}) {
  const { progression, rewards } = useApp();
  const cats = iconCategories(kind, format);
  const progs = playerIds.map((id) => progression?.players.get(id));
  // Pieza de un evento: la tiene alguno de estos jugadores (o este aparato) y se puede usar hoy.
  const now = Date.now();
  const lockOf = (id: string): { short: string; full: string } | null => {
    const info = catalogIconInfo(id);
    if (!info) return null;
    if (!info.usable(now)) return { short: `Solo ${info.cuando}`, full: `Solo se puede usar ${info.cuando}` };
    if (ownedBy(rewards, info.itemId, playerIds)) return null;
    const short = info.requisito.charAt(0).toUpperCase() + info.requisito.slice(1);
    return { short, full: `Se consigue ${info.requisito} · ${info.cuando}` };
  };
  const startCat = (value && cats.find((c) => categoryItems(kind, c.id).includes(value))?.id) || cats[0]?.id;
  const [tab, setTab] = useState(startCat);
  const cat = cats.find((c) => c.id === tab) ?? cats[0];
  if (!cat) return null;
  const open = isUnlocked(cat.unlock, progs);
  return (
    <div className="icon-picker">
      {cats.length > 1 && (
        <div className="icon-tabs" role="tablist">
          {cats.map((c) => {
            const locked = !isUnlocked(c.unlock, progs);
            return (
              <button key={c.id} role="tab" className={`icon-tab ${c.id === cat.id ? 'is-on' : ''} ${locked ? 'is-locked' : ''}`} aria-selected={c.id === cat.id} onClick={() => setTab(c.id)}>
                {locked && '🔒 '}
                {c.label}
              </button>
            );
          })}
        </div>
      )}
      {!open && cat.unlock && (
        <div className="icon-lock-note">
          🔒 Se desbloquea con: <b>{unlockText(cat.unlock)}</b> (basta con uno de los jugadores)
        </div>
      )}
      <div className={`icon-grid icon-grid-${kind} ${open ? '' : 'is-locked'}`}>
        {categoryItems(kind, cat.id).map((id) => {
          const lock = lockOf(id);
          return (
            <button
              key={id}
              className={`icon-cell ${id === value ? 'is-on' : ''} ${lock ? 'is-item-locked' : ''}`}
              aria-pressed={id === value}
              disabled={!open || !!lock}
              title={lock?.full}
              onClick={() => onPick(id)}
            >
              {kind === 'logo' ? <Crest id={id} size={size} /> : kind === 'banner' ? <BannerArt id={id} /> : <Cup id={id} size={size + 8} />}
              {lock && (
                <span className="icon-cell-lock">
                  🔒 <small>{lock.short}</small>
                </span>
              )}
            </button>
          );
        })}
      </div>
    </div>
  );
}

/** Ventana para elegir un logo o una copa. */
export function IconPicker({
  kind,
  value,
  onPick,
  onClose,
  playerIds,
  title,
  format,
}: {
  kind: IconKind;
  value?: string;
  onPick: (id: string) => void;
  onClose: () => void;
  playerIds: string[];
  title?: string;
  /** Tipo de torneo (sus iconos propios, si los tiene). */
  format?: TournamentFormat;
}) {
  return (
    <Modal
      title={title ?? (kind === 'logo' ? 'Elige un logo' : kind === 'banner' ? 'Elige el banner' : 'Elige la copa')}
      onClose={onClose}
      actions={
        <button className="btn btn-ghost" onClick={onClose}>
          Cerrar
        </button>
      }
    >
      <IconGrid
        kind={kind}
        value={value}
        playerIds={playerIds}
        format={format}
        onPick={(id) => {
          onPick(id);
          onClose();
        }}
      />
    </Modal>
  );
}
