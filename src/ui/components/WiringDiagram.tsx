/** Esquema de conexión de una placa: la placa a la izquierda, cada componente a la derecha y la línea común. Lo usa la web («Placa»). */
import { PIN_FUNCTIONS, type BoardInfo, type PinFunction } from '../../inputs/hardware/catalog';
import '../../styles/wiring.css';

const KIND_CLASS: Record<string, string> = { button: 'w-btn', sensor: 'w-sensor', output: 'w-out' };

/** Esquema simple: placa a la izquierda, cada componente a la derecha, y la línea común de GND. */
export function WiringDiagram({ board, used }: { board: BoardInfo; used: PinFunction[] }) {
  const row = 40;
  const top = 34;
  const h = top + used.length * row + 40;
  const gndY = h - 22;
  const ground = board.role === 'pantalla' ? 'COM' : 'GND';
  return (
    <svg className="diy-wiring" viewBox={`0 0 560 ${h}`} role="img" aria-label={`Esquema de conexión de ${board.name}`}>
      <rect className="w-board" x="10" y="10" width="150" height={h - 20} rx="10" />
      <text className="w-board-name" x="85" y="28" textAnchor="middle">{board.chip}</text>
      {used.map((f, i) => {
        const y = top + i * row + row / 2;
        const info = PIN_FUNCTIONS[f];
        const team = f.includes('Blanco') ? 'white' : f.includes('Azul') ? 'blue' : 'neutral';
        return (
          <g key={f} className={KIND_CLASS[info.kind]}>
            <text className="w-pin" x="150" y={y + 4} textAnchor="end">{board.pins[f]}</text>
            <circle className="w-dot" cx="160" cy={y} r="3.5" />
            <line className="w-wire" x1="160" y1={y} x2="330" y2={y} />
            {info.kind === 'button' ? (
              <circle className={`w-part ${team}`} cx="345" cy={y} r="13" />
            ) : info.kind === 'sensor' ? (
              <rect className={`w-part ${team}`} x="332" y={y - 11} width="26" height="22" rx="4" />
            ) : (
              <polygon className={`w-part ${team}`} points={`333,${y - 11} 333,${y + 11} 357,${y}`} />
            )}
            <text className="w-label" x="368" y={y + 4}>{info.label}</text>
            <path className="w-gnd" d={`M345 ${y + (info.kind === 'button' ? 13 : 11)} V ${y + row / 2 - 2} H 540 V ${gndY}`} />
          </g>
        );
      })}
      <path className="w-gnd strong" d={`M540 ${gndY} H 160`} />
      <circle className="w-dot gnd" cx="160" cy={gndY} r="3.5" />
      <text className="w-pin" x="150" y={gndY + 4} textAnchor="end">{ground}</text>
    </svg>
  );
}
