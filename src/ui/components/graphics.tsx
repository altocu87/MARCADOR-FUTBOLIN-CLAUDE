/** Elementos gráficos dibujados en código (SVG/CSS), con hueco para imágenes opcionales. */
import { useMemo, type CSSProperties } from 'react';
import type { CategoryDef, Rarity } from '../../services/progression';
import { AssetImage } from './assets';
import { Avatar } from './common';

// ---------------------------------------------------------------- Insignia de categoría

export function CategoryBadge({ category, size = 40 }: { category: CategoryDef; size?: number }) {
  const id = `cat-${category.id}-${size}`;
  return (
    <AssetImage
      name={`categoria-${category.id}`}
      alt={category.name}
      style={{ width: size, height: size, objectFit: 'contain' }}
      fallback={
        <svg width={size} height={size} viewBox="0 0 64 64" role="img" aria-label={category.name} className="cat-badge">
          <defs>
            <linearGradient id={id} x1="0" y1="0" x2="0" y2="1">
              <stop offset="0" stopColor="#ffffff" stopOpacity="0.9" />
              <stop offset="0.35" stopColor={category.color} />
              <stop offset="1" stopColor="#0b1626" />
            </linearGradient>
          </defs>
          <path d="M32 3 57 13v18c0 15-11 25-25 30C18 56 7 46 7 31V13Z" fill={`url(#${id})`} stroke={category.color} strokeWidth="2.5" />
          <path d="M32 10 51 17v14c0 11-8 19-19 23-11-4-19-12-19-23V17Z" fill="none" stroke="#ffffff" strokeOpacity="0.35" strokeWidth="1.5" />
          <text x="32" y="40" textAnchor="middle" fontSize="20" fontWeight="900" fill="#0b1626">
            {category.name[0]}
          </text>
        </svg>
      }
    />
  );
}

// ---------------------------------------------------------------- Icono de logro

export function AchievementIcon({ id, glyph, rarity, locked, size = 34 }: { id: string; glyph: string; rarity: Rarity; locked?: boolean; size?: number }) {
  return (
    <span className={`ach-icon rarity-${rarity} ${locked ? 'locked' : ''}`} style={{ width: size, height: size }} aria-hidden="true">
      {locked ? '?' : <AssetImage name={`logro-${id}`} style={{ width: '100%', height: '100%', objectFit: 'contain' }} fallback={glyph} />}
    </span>
  );
}

// ---------------------------------------------------------------- Confeti

const CONFETTI_COLORS = ['#62D6FF', '#F2C94C', '#EEF6FF', '#D65CFF', '#3DDC97'];

export function Confetti({ count = 70, colors = CONFETTI_COLORS }: { count?: number; colors?: string[] }) {
  const pieces = useMemo(
    () =>
      Array.from({ length: count }, (_, i) => ({
        left: Math.random() * 100,
        delay: Math.random() * 1.2,
        dur: 2.2 + Math.random() * 1.8,
        size: 6 + Math.random() * 8,
        rot: Math.random() * 360,
        color: colors[i % colors.length],
        drift: (Math.random() - 0.5) * 120,
      })),
    [count, colors],
  );
  return (
    <div className="confetti" aria-hidden="true">
      {pieces.map((p, i) => (
        <span
          key={i}
          style={
            {
              left: `${p.left}%`,
              width: p.size,
              height: p.size * 0.45,
              background: p.color,
              animationDelay: `${p.delay}s`,
              animationDuration: `${p.dur}s`,
              '--rot': `${p.rot}deg`,
              '--drift': `${p.drift}px`,
            } as CSSProperties
          }
        />
      ))}
    </div>
  );
}

// ---------------------------------------------------------------- Anillo de cuenta atrás

export function CountdownRing({ progress, size = 260 }: { progress: number; size?: number }) {
  const r = size / 2 - 10;
  const c = 2 * Math.PI * r;
  return (
    <svg className="countdown-ring" width={size} height={size} viewBox={`0 0 ${size} ${size}`} aria-hidden="true">
      <circle cx={size / 2} cy={size / 2} r={r} fill="none" stroke="rgba(98,214,255,0.12)" strokeWidth="8" />
      <circle
        cx={size / 2}
        cy={size / 2}
        r={r}
        fill="none"
        stroke="#62D6FF"
        strokeWidth="8"
        strokeLinecap="round"
        strokeDasharray={c}
        strokeDashoffset={c * (1 - Math.max(0, Math.min(1, progress)))}
        transform={`rotate(-90 ${size / 2} ${size / 2})`}
      />
    </svg>
  );
}

// ---------------------------------------------------------------- Rótulo animado

export function Banner({ text, sub, tone = 'accent' }: { text: string; sub?: string; tone?: 'accent' | 'gold' | 'danger' | 'chaos' }) {
  return (
    <div className={`banner banner-${tone}`} aria-live="polite">
      <div className="banner-strip">
        <span className="banner-text">{text}</span>
        {sub && <span className="banner-sub">{sub}</span>}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------- Portería de neón (penaltis)

export function NeonGoal({ result, kickKey }: { result: 'goal' | 'miss' | null; kickKey: string }) {
  return (
    <svg className="neon-goal" viewBox="0 0 200 120" aria-hidden="true">
      <defs>
        <pattern id="net" width="10" height="10" patternUnits="userSpaceOnUse">
          <path d="M10 0 0 10M0 0l10 10" stroke="rgba(98,214,255,0.25)" strokeWidth="0.8" />
        </pattern>
      </defs>
      <rect x="30" y="16" width="140" height="70" fill="url(#net)" />
      <path d="M30 86V16h140v70" fill="none" stroke="#62D6FF" strokeWidth="4" strokeLinecap="round" className="goal-frame" />
      <line x1="6" y1="104" x2="194" y2="104" stroke="rgba(98,214,255,0.35)" strokeWidth="2" />
      <circle key={kickKey} cx="100" cy="104" r="7" fill="#EEF6FF" className={`ball ${result ?? ''}`} />
    </svg>
  );
}

// ---------------------------------------------------------------- Gráfica de ELO

export function EloChart({ points, initial }: { points: { elo: number; at: number }[]; initial: number }) {
  const W = 360;
  const H = 120;
  const data = [initial, ...points.map((p) => p.elo)];
  if (data.length < 2) return <div className="dim" style={{ fontSize: 13 }}>Sin clasificatorios todavía.</div>;
  const min = Math.min(...data) - 10;
  const max = Math.max(...data) + 10;
  const x = (i: number) => 8 + (i / (data.length - 1)) * (W - 16);
  const y = (v: number) => H - 14 - ((v - min) / (max - min)) * (H - 28);
  const d = data.map((v, i) => `${i ? 'L' : 'M'}${x(i)},${y(v)}`).join(' ');
  const last = data[data.length - 1];
  return (
    <svg viewBox={`0 0 ${W} ${H}`} width="100%" role="img" aria-label={`Evolución de ELO: de ${initial} a ${last}`}>
      <line x1="8" x2={W - 8} y1={y(initial)} y2={y(initial)} stroke="#22385A" strokeDasharray="4 4" />
      <text x={W - 8} y={y(initial) - 4} textAnchor="end" fontSize="10" fill="#6F84A3">{initial}</text>
      <path d={`${d} L${x(data.length - 1)},${H - 14} L${x(0)},${H - 14} Z`} fill="rgba(98,214,255,0.12)" />
      <path d={d} fill="none" stroke="#62D6FF" strokeWidth="2" />
      <circle cx={x(data.length - 1)} cy={y(last)} r="4.5" fill="#62D6FF" stroke="#091324" strokeWidth="2" />
      <text x={x(data.length - 1) - 6} y={y(last) - 8} textAnchor="end" fontSize="12" fontWeight="700" fill="#F3F7FF">{last}</text>
    </svg>
  );
}

// ---------------------------------------------------------------- Podio

export function Podium({ leaders, onPick }: { leaders: { playerId: string; name: string; value: string; photo?: string }[]; onPick?: (id: string) => void }) {
  const order = [1, 0, 2].filter((i) => leaders[i]);
  return (
    <div className="podium">
      {order.map((i) => {
        const l = leaders[i];
        return (
          <button key={l.playerId} className={`podium-col place-${i + 1}`} onClick={() => onPick?.(l.playerId)}>
            <Avatar name={l.name} photo={l.photo} size={i === 0 ? 46 : 36} />
            <span className="podium-name ellipsis">{l.name}</span>
            <span className="podium-value">{l.value}</span>
            <span className="podium-step">{i + 1}</span>
          </button>
        );
      })}
    </div>
  );
}
