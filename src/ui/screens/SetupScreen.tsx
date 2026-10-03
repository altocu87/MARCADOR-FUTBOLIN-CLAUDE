import { useEffect, useState } from 'react';
import { useApp } from '../../app/AppContext';
import {
  CONFIG_LIMITS,
  DEFAULT_CONFIG,
  validateConfig,
  type EndCondition,
  type MatchConfig,
  type MatchMode,
} from '../../match-engine';
import { AssetImage } from '../components/assets';
import { MODE_LABEL, ScreenFrame, TestModeBadge } from '../components/common';
import { SevenSegment } from '../components/SevenSegment';

/** Las dos tarjetas activas a la vez equivalen a «ambas». */
function conditionFrom(goals: boolean, time: boolean): EndCondition {
  return goals && time ? 'both' : time ? 'time' : 'goals';
}

/** Flecha de los botones de sumar/restar (dibujada en SVG para que se vea nítida a cualquier tamaño). */
function Chevron({ dir }: { dir: 'up' | 'down' }) {
  return (
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d={dir === 'up' ? 'M5 15.5 12 8.5l7 7' : 'M5 8.5l7 7 7-7'} />
    </svg>
  );
}

/** Reloj pequeño de la esquina, con el mismo estilo de segmentos que el del salvapantallas. */
function MiniClock() {
  const [now, setNow] = useState(() => new Date());
  useEffect(() => {
    const id = window.setInterval(() => setNow(new Date()), 1000);
    return () => window.clearInterval(id);
  }, []);
  const hh = String(now.getHours()).padStart(2, '0');
  const mm = String(now.getMinutes()).padStart(2, '0');
  return (
    <div className={`setup-clock${now.getSeconds() % 2 ? ' sec-odd' : ''}`}>
      <SevenSegment text={`${hh}:${mm}`} height={28} color="#4fd8ff" label={`Hora: ${hh}:${mm}`} />
    </div>
  );
}

/**
 * Botón Continuar con forma de panel neón (dibujado en SVG): en reposo marco cian con detalles verdes;
 * al pulsarlo se ilumina por dentro.
 */
function ContinueButton({ disabled, onClick }: { disabled: boolean; onClick: () => void }) {
  const frame = 'M14 2H286L298 13L280 56L268 62H32L20 56L2 13Z';
  return (
    <button className="continue-btn" disabled={disabled} onClick={onClick}>
      <svg viewBox="0 0 300 64" preserveAspectRatio="none" aria-hidden="true">
        <defs>
          <radialGradient id="continue-glow" cx="50%" cy="50%" r="60%">
            <stop offset="0%" stopColor="#3fdcff" stopOpacity="0.95" />
            <stop offset="100%" stopColor="#0b4a6e" stopOpacity="1" />
          </radialGradient>
        </defs>
        <path className="cb-fill" d={frame} />
        <path className="cb-fill-on" d={frame} fill="url(#continue-glow)" />
        <path className="cb-frame" d={frame} />
        <path className="cb-inner" d="M18 7H282L291 14L275 51L265 56H35L25 51L9 14Z" />
        <path className="cb-bar cb-top" d="M95 4.5H205" />
        <path className="cb-bar" d="M115 59.5H185" />
      </svg>
      <span>Continuar</span>
    </button>
  );
}

/**
 * Tarjeta de condición: botón ▲ pegado encima, recuadro con el número y botón ▼ pegado debajo.
 * Tocar el número activa o desactiva la tarjeta (decide si el partido es por goles, por tiempo o ambos).
 */
function ConditionCard({
  title,
  label,
  unit,
  value,
  min,
  max,
  active,
  onToggle,
  onChange,
}: {
  title: string;
  label: string;
  unit: string;
  value: number;
  min: number;
  max: number;
  active: boolean;
  onToggle: () => void;
  onChange: (v: number) => void;
}) {
  return (
    <div className={`cond${active ? ' on' : ''}`} role="group" aria-label={title}>
      <button
        className="cond-btn up"
        aria-label={`Sumar ${label}`}
        disabled={!active || value >= max}
        onClick={() => onChange(Math.min(max, value + 1))}
      >
        <span className="cond-tab">
          <Chevron dir="up" />
        </span>
      </button>
      <button
        className="cond-box"
        aria-pressed={active}
        aria-label={`${title}: ${value} ${unit}. ${active ? 'Toca para desactivar' : 'Toca para activar'}`}
        onClick={onToggle}
      >
        <span className="num" aria-live="polite">{value}</span>
        <span className="cond-title">{title}</span>
      </button>
      <button
        className="cond-btn down"
        aria-label={`Restar ${label}`}
        disabled={!active || value <= min}
        onClick={() => onChange(Math.max(min, value - 1))}
      >
        <span className="cond-tab">
          <Chevron dir="down" />
        </span>
      </button>
    </div>
  );
}

export function SetupScreen({ mode, initial }: { mode: MatchMode; initial?: MatchConfig }) {
  const { navigate, prefs, demoMode, toast } = useApp();
  const [config, setConfig] = useState<MatchConfig>(
    () =>
      initial ?? {
        ...DEFAULT_CONFIG,
        mode,
        endCondition: prefs.defaultEndCondition,
        goalsPerPeriod: prefs.defaultGoalsPerPeriod,
        minutesPerPeriod: prefs.defaultMinutesPerPeriod,
        penaltyFirstTeam: prefs.penaltyFirstTeam,
        // Todos los partidos se guardan; en modo prueba van a los datos de prueba.
        testMode: false,
        // Caos lleva siempre sus reglas especiales: no se eligen en esta pantalla.
        ...(mode === 'chaos' ? { chaos: { jokers: true, doubleLastMinute: true } } : {}),
      },
  );
  const set = (patch: Partial<MatchConfig>) => setConfig((c) => ({ ...c, ...patch }));
  const errors = validateConfig(config);
  const usesGoals = config.endCondition !== 'time';
  const usesTime = config.endCondition !== 'goals';
  // Siempre debe quedar al menos una de las dos tarjetas activa.
  const toggle = (which: 'goals' | 'time') => {
    const goals = which === 'goals' ? !usesGoals : usesGoals;
    const time = which === 'time' ? !usesTime : usesTime;
    if (!goals && !time) {
      toast('Deja activa al menos una: goles o tiempo');
      return;
    }
    set({ endCondition: conditionFrom(goals, time) });
  };

  return (
    <ScreenFrame
      title="Configuración"
      className={`setup-screen setup-${mode}`}
      background={<AssetImage name="fondo-configuracion" className="setup-bg" fallback={null} />}
      onBack={() => navigate({ name: 'home' })}
      right={
        <>
          {/* Modo de partido, grande y centrado en la cabecera. */}
          <div className="setup-mode">{MODE_LABEL[mode]}</div>
          <div className="setup-right">
            {demoMode && <TestModeBadge />}
            <MiniClock />
          </div>
        </>
      }
      footer={
        <>
          {errors.length > 0 && <span className="notice error setup-error">{errors[0]}</span>}
          <ContinueButton disabled={errors.length > 0} onClick={() => navigate({ name: 'select', config })} />
        </>
      }
    >
      <div className="grid-2 setup-conditions">
        <ConditionCard
          title="Goles para ganar"
          label="goles para ganar"
          unit="goles"
          value={config.goalsPerPeriod}
          min={CONFIG_LIMITS.goalsPerPeriod.min}
          max={CONFIG_LIMITS.goalsPerPeriod.max}
          active={usesGoals}
          onToggle={() => toggle('goals')}
          onChange={(v) => set({ goalsPerPeriod: v })}
        />
        <ConditionCard
          title="Minutos por parte"
          label="minutos por parte"
          unit="min"
          value={config.minutesPerPeriod}
          min={CONFIG_LIMITS.minutesPerPeriod.min}
          max={CONFIG_LIMITS.minutesPerPeriod.max}
          active={usesTime}
          onToggle={() => toggle('time')}
          onChange={(v) => set({ minutesPerPeriod: v })}
        />
      </div>
    </ScreenFrame>
  );
}
