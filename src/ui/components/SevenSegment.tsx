/** Reloj de aspecto segmentado dibujado en SVG (sin fuentes externas). */
const SEGMENTS: Record<string, string> = {
  '0': 'abcdef',
  '1': 'bc',
  '2': 'abged',
  '3': 'abgcd',
  '4': 'fgbc',
  '5': 'afgcd',
  '6': 'afgedc',
  '7': 'abc',
  '8': 'abcdefg',
  '9': 'abcdfg',
  '-': 'g',
  ' ': '',
};

const h = (cy: number) => `12,${cy} 16,${cy - 4} 44,${cy - 4} 48,${cy} 44,${cy + 4} 16,${cy + 4}`;
const v = (cx: number, y1: number, y2: number) =>
  `${cx},${y1} ${cx + 4},${y1 + 4} ${cx + 4},${y2 - 4} ${cx},${y2} ${cx - 4},${y2 - 4} ${cx - 4},${y1 + 4}`;

const POLYS: Record<string, string> = {
  a: h(6),
  g: h(50),
  d: h(94),
  f: v(8, 10, 46),
  b: v(52, 10, 46),
  e: v(8, 54, 90),
  c: v(52, 54, 90),
};

interface Props {
  text: string;
  height: number;
  color?: string;
  className?: string;
  label?: string;
  /** Pintar tenues los segmentos apagados (aspecto de display real). */
  ghost?: boolean;
}

export function SevenSegment({ text, height, color = 'currentColor', className, label, ghost = true }: Props) {
  const chars = text.split('');
  let x = 0;
  const parts = chars.map((ch, i) => {
    if (ch === ':' || ch === '.') {
      const g = (
        <g key={i} className="seg-colon" transform={`translate(${x},0)`}>
          {ch === ':' && <circle cx={10} cy={32} r={5} fill={color} />}
          <circle cx={10} cy={ch === ':' ? 68 : 92} r={5} fill={color} />
        </g>
      );
      x += 20;
      return g;
    }
    const on = SEGMENTS[ch] ?? '';
    const g = (
      <g key={i} transform={`translate(${x},0)`}>
        {Object.entries(POLYS)
          .filter(([seg]) => ghost || on.includes(seg))
          .map(([seg, pts]) => (
            <polygon key={seg} points={pts} fill={color} opacity={on.includes(seg) ? 1 : 0.07} />
          ))}
      </g>
    );
    x += 66;
    return g;
  });
  const width = x - 6;
  return (
    <svg
      className={className}
      viewBox={`-4 0 ${width + 8} 100`}
      height={height}
      width={(height * (width + 8)) / 100}
      role="img"
      aria-label={label ?? text}
      style={{ transform: 'skewX(-6deg)', overflow: 'visible' }}
    >
      {parts}
    </svg>
  );
}
