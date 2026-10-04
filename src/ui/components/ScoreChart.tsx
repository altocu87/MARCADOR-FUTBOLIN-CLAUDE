/**
 * Evolución del resultado, estilo neón: dos líneas escalonadas (Blanco / Azul) sobre el tiempo
 * de juego acumulado, en un panel oscuro de cristal. El eje del tiempo marca cada minuto (o cada
 * 15/30 s en partidos muy cortos, o cada 2/5 min en los largos); los goles llevan un punto que
 * brilla y los anulados una ✕ roja sobre el eje. Tocar un gol muestra su detalle.
 */
import { useLayoutEffect, useRef, useState } from 'react';
import { annulledGoalIdsFromEvents, validGoalsFromEvents, type MatchEvent } from '../../match-engine';
import { formatDuration } from '../../services/statistics';

const COLORS = { white: '#ffffff', blue: '#62d6ff' };
const PAD = { l: 40, r: 34, t: 18, b: 34 };

/** Paso del eje del tiempo: el menor de la lista que deja como mucho ~10 marcas. */
function tickStep(totalMs: number): number {
  const steps = [10, 15, 30, 60, 120, 300, 600].map((s) => s * 1000);
  return steps.find((s) => totalMs / s <= 10) ?? 600_000;
}

/** Rótulo de una marca: «1'», «2'»… por minutos; «0:30» si el paso es de segundos. */
function tickLabel(ms: number, step: number): string {
  if (ms === 0) return '0';
  if (step >= 60_000) return `${Math.round(ms / 60_000)}'`;
  return formatDuration(ms).replace(/^0/, '');
}

export function ScoreChart({ events, totalTimeMs }: { events: MatchEvent[]; totalTimeMs: number }) {
  const goals = validGoalsFromEvents(events);
  const annulledIds = annulledGoalIdsFromEvents(events);
  const annulled = events.filter((e) => e.type === 'GOAL' && annulledIds.has(e.id));
  // Cambios de parte (2ª parte, prórroga): línea vertical discontinua.
  const periodStarts = events.filter((e) => e.type === 'PERIOD_START' && e.period !== 'first' && e.period !== 'shootout');
  const [hover, setHover] = useState<number | null>(null);

  // El dibujo se adapta al tamaño real del panel (sin deformar textos).
  const box = useRef<HTMLDivElement>(null);
  const [size, setSize] = useState({ w: 720, h: 300 });
  useLayoutEffect(() => {
    const el = box.current;
    if (!el) return;
    const update = () => setSize({ w: Math.max(240, el.clientWidth), h: Math.max(160, el.clientHeight) });
    update();
    const ro = new ResizeObserver(update);
    ro.observe(el);
    return () => ro.disconnect();
  }, []);
  const { w: W, h: H } = size;

  const maxT = Math.max(totalTimeMs, ...goals.map((g) => g.totalTimeMs), 1);
  const final = goals.length ? goals[goals.length - 1].scoreAfter : { white: 0, blue: 0 };
  const maxY = Math.max(1, final.white, final.blue, ...goals.map((g) => Math.max(g.scoreAfter.white, g.scoreAfter.blue)));
  const x = (t: number) => PAD.l + (t / maxT) * (W - PAD.l - PAD.r);
  const y = (v: number) => H - PAD.b - (v / maxY) * (H - PAD.t - PAD.b);
  // Las dos líneas no se tapan cuando coinciden: Blanco va un pelo por encima.
  const off = { white: -2, blue: 2 };

  const path = (team: 'white' | 'blue') => {
    let d = `M${x(0)},${y(0) + off[team]}`;
    let prev = 0;
    for (const g of goals) {
      const v = g.scoreAfter[team];
      d += ` H${x(g.totalTimeMs)}`;
      if (v !== prev) d += ` V${y(v) + off[team]}`;
      prev = v;
    }
    return d + ` H${x(maxT)}`;
  };
  // Zona rellena bajo cada línea (degradado suave).
  const area = (team: 'white' | 'blue') => `${path(team)} V${y(0)} H${x(0)} Z`;

  const step = tickStep(maxT);
  const xTicks: number[] = [];
  for (let t = 0; t <= maxT + 1; t += step) xTicks.push(t);
  const yTicks = Array.from({ length: maxY + 1 }, (_, i) => i).filter((v) => maxY <= 10 || v % 2 === 0);
  const hovered = hover !== null ? goals[hover] : null;

  return (
    <div className="chart-wrap">
      <div className="chart-legend">
        <span className="chart-key white">
          <i /> BLANCO <b>{final.white}</b>
        </span>
        <span className="chart-key blue">
          <i /> AZUL <b>{final.blue}</b>
        </span>
        {annulled.length > 0 && (
          <span className="chart-key off">
            <i /> ANULADOS <b>{annulled.length}</b>
          </span>
        )}
        <span className="chart-tip" aria-live="polite">
          {hovered
            ? `⚽ Gol ${hovered.team === 'white' ? 'Blanco' : 'Azul'} · minuto ${formatDuration(hovered.totalTimeMs)} · ${hovered.scoreAfter.white}–${hovered.scoreAfter.blue}`
            : goals.length
              ? 'Toca un gol para ver el detalle'
              : 'Sin goles ordinarios'}
        </span>
      </div>
      <div className="chart-panel" ref={box}>
        <svg width={W} height={H} viewBox={`0 0 ${W} ${H}`} role="img" aria-label="Evolución del resultado">
          <defs>
            <filter id="neon-glow" x="-20%" y="-20%" width="140%" height="140%">
              <feGaussianBlur stdDeviation="3" result="b" />
              <feMerge>
                <feMergeNode in="b" />
                <feMergeNode in="SourceGraphic" />
              </feMerge>
            </filter>
            <linearGradient id="fill-white" x1="0" y1="0" x2="0" y2="1">
              <stop offset="0" stopColor={COLORS.white} stopOpacity="0.22" />
              <stop offset="1" stopColor={COLORS.white} stopOpacity="0" />
            </linearGradient>
            <linearGradient id="fill-blue" x1="0" y1="0" x2="0" y2="1">
              <stop offset="0" stopColor={COLORS.blue} stopOpacity="0.28" />
              <stop offset="1" stopColor={COLORS.blue} stopOpacity="0" />
            </linearGradient>
          </defs>

          {/* Rejilla: líneas horizontales por gol y verticales por minuto. */}
          {yTicks.map((v) => (
            <g key={`y${v}`}>
              <line x1={PAD.l} x2={W - PAD.r} y1={y(v)} y2={y(v)} className="chart-grid" />
              <text x={PAD.l - 10} y={y(v) + 4} textAnchor="end" className="chart-axis">
                {v}
              </text>
            </g>
          ))}
          {xTicks.map((t) => (
            <g key={`x${t}`}>
              <line x1={x(t)} x2={x(t)} y1={PAD.t} y2={H - PAD.b} className="chart-grid v" />
              <line x1={x(t)} x2={x(t)} y1={H - PAD.b} y2={H - PAD.b + 5} className="chart-tick" />
              <text x={x(t)} y={H - PAD.b + 18} textAnchor="middle" className="chart-axis">
                {tickLabel(t, step)}
              </text>
            </g>
          ))}
          {/* Eje del tiempo, con el final del partido. */}
          <line x1={PAD.l} x2={W - PAD.r} y1={H - PAD.b} y2={H - PAD.b} className="chart-base" />
          <text x={W - PAD.r} y={H - 4} textAnchor="end" className="chart-axis end">
            Final {formatDuration(maxT)}
          </text>
          <text x={8} y={12} className="chart-axis unit">
            Goles
          </text>

          {periodStarts.map((e) => (
            <g key={e.id}>
              <line x1={x(e.totalTimeMs)} x2={x(e.totalTimeMs)} y1={PAD.t} y2={H - PAD.b} className="chart-period" />
              <text x={x(e.totalTimeMs) + 4} y={PAD.t + 10} className="chart-axis period">
                {e.period === 'second' ? '2ª PARTE' : 'PRÓRROGA'}
              </text>
            </g>
          ))}

          <path d={area('white')} fill="url(#fill-white)" />
          <path d={area('blue')} fill="url(#fill-blue)" />
          <path d={path('white')} fill="none" stroke={COLORS.white} strokeWidth={3} filter="url(#neon-glow)" />
          <path d={path('blue')} fill="none" stroke={COLORS.blue} strokeWidth={3} filter="url(#neon-glow)" />

          {/* Goles anulados: ✕ roja sobre el eje del tiempo. */}
          {annulled.map((g) => (
            <g key={g.id} className="chart-off" transform={`translate(${x(g.totalTimeMs)},${H - PAD.b})`}>
              <circle r={7} />
              <path d="M-3.5,-3.5 L3.5,3.5 M3.5,-3.5 L-3.5,3.5" />
            </g>
          ))}

          {goals.map((g, i) => {
            const cx = x(g.totalTimeMs);
            const cy = y(g.scoreAfter[g.team!]) + off[g.team!];
            return (
              <g
                key={g.id}
                className="chart-goal"
                onPointerEnter={() => setHover(i)}
                onPointerDown={() => setHover(i)}
                onPointerLeave={() => setHover(null)}
              >
                <circle cx={cx} cy={cy} r={16} fill="transparent" />
                <circle cx={cx} cy={cy} r={hover === i ? 8 : 5.5} fill={COLORS[g.team!]} stroke="#061020" strokeWidth={2} filter="url(#neon-glow)" />
                {hover === i && <line x1={cx} x2={cx} y1={cy + 8} y2={H - PAD.b} className="chart-guide" />}
              </g>
            );
          })}

          {/* Resultado final al extremo de cada línea. */}
          <text x={W - PAD.r + 8} y={y(final.white) + off.white + (final.white === final.blue ? -6 : 5)} className="chart-end white">
            {final.white}
          </text>
          <text x={W - PAD.r + 8} y={y(final.blue) + off.blue + (final.white === final.blue ? 14 : 5)} className="chart-end blue">
            {final.blue}
          </text>
        </svg>
      </div>
    </div>
  );
}
