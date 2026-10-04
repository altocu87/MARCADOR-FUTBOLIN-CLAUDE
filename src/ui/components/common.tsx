/** Componentes de interfaz reutilizables. */
import { useEffect, type ReactNode } from 'react';
import type { MatchMode } from '../../match-engine';
import { initials } from '../../services/players';
import { assetUrl } from './assets';
import type { ResultLetter } from '../../services/statistics';

export function ScreenFrame({
  title,
  subtitle,
  onBack,
  left,
  right,
  children,
  footer,
  className,
  background,
}: {
  title: ReactNode;
  subtitle?: ReactNode;
  onBack?: () => void;
  /** Botones a la izquierda de la cabecera, tras el de volver. */
  left?: ReactNode;
  right?: ReactNode;
  children: ReactNode;
  footer?: ReactNode;
  className?: string;
  /** Imagen de fondo a pantalla completa, detrás de la cabecera y el contenido. */
  background?: ReactNode;
}) {
  return (
    <section className={`screen ${className ?? ''}`}>
      {background}
      <header className="screen-header">
        {onBack && (
          <button className="btn btn-ghost btn-icon" onClick={onBack} aria-label="Volver">
            <svg width="22" height="22" viewBox="0 0 24 24" aria-hidden="true">
              <path d="M15 4 7 12l8 8" fill="none" stroke="currentColor" strokeWidth="2.6" strokeLinecap="round" />
            </svg>
          </button>
        )}
        {left}
        <h1 className="screen-title" style={{ margin: 0, paddingLeft: onBack ? 0 : 6 }}>
          {title}
          {subtitle && <small>{subtitle}</small>}
        </h1>
        {right}
      </header>
      <div className="screen-body">{children}</div>
      {footer && <footer className="screen-footer">{footer}</footer>}
    </section>
  );
}

export function Avatar({ name, photo, size = 40 }: { name: string; photo?: string; size?: number }) {
  return (
    <span className="avatar" style={{ width: size, height: size, fontSize: size * 0.38 }} aria-hidden="true">
      {photo ? <img src={photo} alt="" draggable={false} /> : initials(name)}
    </span>
  );
}

export function Tabs<T extends string>({
  tabs,
  value,
  onChange,
  label,
}: {
  tabs: { id: T; label: string }[];
  value: T;
  onChange: (id: T) => void;
  label: string;
}) {
  return (
    <div className="tabs" role="tablist" aria-label={label}>
      {tabs.map((t) => (
        <button key={t.id} role="tab" className="tab" aria-selected={t.id === value} onClick={() => onChange(t.id)}>
          {t.label}
        </button>
      ))}
    </div>
  );
}

export function Toggle({
  checked,
  onChange,
  label,
  description,
}: {
  checked: boolean;
  onChange: (v: boolean) => void;
  label: string;
  description?: string;
}) {
  return (
    <button className="toggle" role="switch" aria-checked={checked} onClick={() => onChange(!checked)}>
      <span className="track" />
      <span>
        <strong style={{ display: 'block', fontSize: 15 }}>{label}</strong>
        {description && <span className="muted" style={{ fontSize: 12 }}>{description}</span>}
      </span>
    </button>
  );
}

/** Signo − / + con las imágenes neón (reposo y pulsado); sin imágenes, el texto de respaldo. */
export function NeonSign({ kind, fallback }: { kind: 'restar' | 'sumar'; fallback: string }) {
  const off = assetUrl(`boton-${kind}-off`);
  const on = assetUrl(`boton-${kind}-on`);
  if (!off || !on) return <>{fallback}</>;
  return (
    <span className="neon-sign" aria-hidden="true">
      <img className="off" src={off} alt="" draggable={false} />
      <img className="on" src={on} alt="" draggable={false} />
    </span>
  );
}

export function Stepper({
  value,
  min,
  max,
  step = 1,
  unit,
  onChange,
  label,
  disabled,
}: {
  value: number;
  min: number;
  max: number;
  step?: number;
  unit?: string;
  onChange: (v: number) => void;
  label: string;
  disabled?: boolean;
}) {
  return (
    <div className="stepper" role="group" aria-label={label} style={{ opacity: disabled ? 0.4 : 1 }}>
      <button
        className="btn btn-icon btn-lg"
        aria-label={`Restar ${label}`}
        disabled={disabled || value <= min}
        onClick={() => onChange(Math.max(min, value - step))}
      >
        −
      </button>
      <span className="value" aria-live="polite">
        {value}
        {unit && <span className="unit"> {unit}</span>}
      </span>
      <button
        className="btn btn-icon btn-lg"
        aria-label={`Sumar ${label}`}
        disabled={disabled || value >= max}
        onClick={() => onChange(Math.min(max, value + step))}
      >
        +
      </button>
    </div>
  );
}

export function Modal({
  title,
  children,
  actions,
  onClose,
}: {
  title: string;
  children: ReactNode;
  actions: ReactNode;
  onClose?: () => void;
}) {
  useEffect(() => {
    if (!onClose) return;
    const onKey = (e: KeyboardEvent) => e.key === 'Escape' && onClose();
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [onClose]);
  return (
    <div className="modal-backdrop" onPointerDown={(e) => e.target === e.currentTarget && onClose?.()}>
      <div className="modal" role="dialog" aria-modal="true" aria-label={title}>
        <h2>{title}</h2>
        <div className="scroll" style={{ minHeight: 0 }}>
          {children}
        </div>
        <div className="modal-actions">{actions}</div>
      </div>
    </div>
  );
}

export const MODE_LABEL: Record<MatchMode, string> = {
  quick: 'Partido rápido',
  chaos: 'Partido Caos',
  ranked: 'Clasificatorio',
};

export function ModeBadge({ mode }: { mode: MatchMode }) {
  const cls = mode === 'chaos' ? 'badge-chaos' : mode === 'ranked' ? 'badge-ranked' : 'badge-accent';
  return <span className={`badge ${cls}`}>{MODE_LABEL[mode]}</span>;
}

export function FormChips({ form, empty = '—' }: { form: ResultLetter[]; empty?: string }) {
  if (form.length === 0) return <span className="dim">{empty}</span>;
  return (
    <span style={{ display: 'inline-flex', gap: 3 }} aria-label={`Forma: ${form.join(' ')}`}>
      {form.map((f, i) => (
        <span key={i} className={`form-chip ${f}`}>
          {f}
        </span>
      ))}
    </span>
  );
}

export function TestModeBadge() {
  return (
    <span className="badge badge-danger" title="Modo prueba: se guarda aparte y se borra al desactivarlo en Ajustes">
      <span className="dot" /> Modo prueba
    </span>
  );
}

export function StatTile({ k, v }: { k: string; v: ReactNode }) {
  return (
    <div className="stat">
      <div className="v">{v}</div>
      <div className="k">{k}</div>
    </div>
  );
}

export function formatDate(ts: number): string {
  return new Date(ts).toLocaleString('es-ES', {
    day: '2-digit',
    month: '2-digit',
    year: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
  });
}

export function pct(v: number | null): string {
  return v === null ? '—' : `${v.toFixed(0)} %`;
}
