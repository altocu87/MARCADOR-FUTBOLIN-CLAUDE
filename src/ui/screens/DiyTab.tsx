/**
 * Ajustes → Hazlo tú mismo: guía para montar el marcador con una placa propia.
 * Todo sale del catálogo (diy-catalog.json): niveles, placas, pines, esquemas y detección de goles.
 */
import { useState } from 'react';
import {
  BOARDS,
  DETECTION,
  DIY_LEVELS,
  LINK_LABEL,
  PIN_FUNCTIONS,
  PIN_ORDER,
  ROLE_LABEL,
  SAFETY,
  installSteps,
  type BoardInfo,
  type BoardRole,
  type PinFunction,
} from '../../inputs/hardware/catalog';
import '../../styles/wiring.css';

type Section = 'levels' | 'boards' | 'detection' | 'safety';

const SECTIONS: { id: Section; label: string }[] = [
  { id: 'levels', label: 'Niveles' },
  { id: 'boards', label: 'Placas y conexión' },
  { id: 'detection', label: 'Detectar goles' },
  { id: 'safety', label: 'Seguridad' },
];

const ROLES: BoardRole[] = ['entradas', 'mando', 'pantalla'];

function Difficulty({ value }: { value: number }) {
  return (
    <span className="diy-diff" aria-label={`Dificultad ${value} de 3`}>
      {[1, 2, 3].map((i) => (
        <i key={i} className={i <= value ? 'on' : ''} />
      ))}
    </span>
  );
}

export function DiyTab({ initialBoard }: { initialBoard?: string }) {
  const [section, setSection] = useState<Section>(initialBoard ? 'boards' : 'levels');
  const [boardId, setBoardId] = useState(initialBoard && BOARDS.some((b) => b.id === initialBoard) ? initialBoard : BOARDS[0].id);
  const board = BOARDS.find((b) => b.id === boardId) ?? BOARDS[0];

  return (
    <div className="diy">
      <div className="segmented diy-sections" role="group" aria-label="Secciones de la guía">
        {SECTIONS.map((s) => (
          <button key={s.id} className="seg seg-compact" aria-pressed={section === s.id} onClick={() => setSection(s.id)}>
            {s.label}
          </button>
        ))}
      </div>

      {section === 'levels' && (
        <div className="diy-levels">
          {DIY_LEVELS.map((l) => (
            <div key={l.id} className="card diy-level">
              <div className="diy-level-head">
                <span className="diy-level-n">{l.id}</span>
                <strong>{l.title}</strong>
              </div>
              <div className="muted" style={{ fontSize: 13 }}>{l.summary}</div>
              <ul className="diy-list">
                {l.needs.map((n) => (
                  <li key={n}>{n}</li>
                ))}
              </ul>
              <div className="diy-level-foot">
                <span className="badge">{l.cost}</span>
                <Difficulty value={l.difficulty} />
              </div>
              {l.id > 0 && (
                <button
                  className="btn btn-sm"
                  onClick={() => {
                    const first = BOARDS.find((b) => b.level === l.id);
                    if (first) setBoardId(first.id);
                    setSection('boards');
                  }}
                >
                  Ver placas
                </button>
              )}
            </div>
          ))}
        </div>
      )}

      {section === 'boards' && (
        <div className="diy-boards">
          <div className="diy-board-list" role="listbox" aria-label="Placas compatibles">
            {ROLES.map((role) => (
              <div key={role}>
                <div className="label">{ROLE_LABEL[role]}</div>
                {BOARDS.filter((b) => b.role === role).map((b) => (
                  <button
                    key={b.id}
                    role="option"
                    aria-selected={b.id === board.id}
                    className={`diy-board-item ${b.id === board.id ? 'on' : ''}`}
                    ref={b.id === board.id ? (el) => el?.scrollIntoView({ block: 'nearest' }) : undefined}
                    onClick={() => setBoardId(b.id)}
                  >
                    {b.name}
                  </button>
                ))}
              </div>
            ))}
          </div>
          <BoardDetail board={board} />
        </div>
      )}

      {section === 'detection' && (
        <div className="diy-detect">
          <div className="notice" style={{ fontSize: 13 }}>
            Cualquier sensor con salida de «contacto» vale: el programa aprende al encender cómo está «sin balón» y cuenta gol
            cuando cambia. El mejor sitio es el <strong>canal interior por el que cae la bola</strong> tras el gol.
          </div>
          {DETECTION.map((d) => (
            <div key={d.id} className={`card diy-detect-card ${d.supported ? '' : 'off'}`}>
              <div className="diy-level-head">
                <strong style={{ flex: 1 }}>{d.name}</strong>
                {d.recommended && <span className="badge badge-ok">Recomendado</span>}
                {!d.supported && <span className="badge badge-danger">No recomendado</span>}
                <span className="badge">{d.cost}</span>
                <Difficulty value={d.difficulty} />
              </div>
              <div style={{ fontSize: 13 }}>{d.how}</div>
              <div className="diy-two">
                <div>
                  <div className="label">Dónde</div>
                  <div className="muted" style={{ fontSize: 13 }}>{d.where}</div>
                  <div className="label" style={{ marginTop: 6 }}>Conexión</div>
                  <div className="muted" style={{ fontSize: 13 }}>{d.wiring}</div>
                </div>
                <div>
                  <ul className="diy-list pros">
                    {d.pros.map((p) => (
                      <li key={p}>{p}</li>
                    ))}
                  </ul>
                  <ul className="diy-list cons">
                    {d.cons.map((c) => (
                      <li key={c}>{c}</li>
                    ))}
                  </ul>
                </div>
              </div>
            </div>
          ))}
        </div>
      )}

      {section === 'safety' && (
        <div className="diy-safety">
          {SAFETY.map((s) => (
            <div key={s} className="notice warn" style={{ fontSize: 13 }}>
              {s}
            </div>
          ))}
          <div className="card" style={{ fontSize: 13 }}>
            <div className="label">¿Tu placa no está en la lista?</div>
            Si habla el protocolo MFV3 (una orden por línea: <code>GB</code>, <code>GA</code>, <code>AB</code>, <code>AA</code>,{' '}
            <code>PAUSA</code>) por USB, Wi-Fi o Bluetooth, la app la acepta. Todos los detalles están en docs/MANUAL_DIY.md y
            docs/HARDWARE.md.
          </div>
        </div>
      )}
    </div>
  );
}

function BoardDetail({ board }: { board: BoardInfo }) {
  const used = PIN_ORDER.filter((f) => board.pins[f]);
  return (
    <div className="diy-board-detail">
      <div className="diy-level-head">
        <strong style={{ fontSize: 16, flex: 1 }}>{board.name}</strong>
        <span className={`badge ${board.status === 'probada' ? 'badge-ok' : 'badge-pending'}`}>
          {board.status === 'probada' ? 'Probada en placa' : 'Probada en simulador'}
        </span>
      </div>
      <div className="dim" style={{ fontSize: 13 }}>{board.aka}</div>
      <div className="chip-wrap" style={{ margin: '6px 0' }}>
        <span className="badge badge-accent">{ROLE_LABEL[board.role]}</span>
        <span className="badge">{board.chip}</span>
        <span className="badge">{board.voltage}</span>
        {board.links.map((l) => (
          <span key={l} className="badge">{LINK_LABEL[l]}</span>
        ))}
        <span className="badge">{board.price}</span>
      </div>
      {board.notes.map((n) => (
        <div key={n} className="muted" style={{ fontSize: 13 }}>· {n}</div>
      ))}

      {used.length > 0 && (
        <>
          <div className="label" style={{ marginTop: 10 }}>Conexiones</div>
          <table className="diy-pins">
            <thead>
              <tr>
                <th>Qué</th>
                <th>Pin de la placa</th>
                <th>El otro cable</th>
              </tr>
            </thead>
            <tbody>
              {used.map((f) => (
                <tr key={f}>
                  <td>
                    <strong>{PIN_FUNCTIONS[f].label}</strong>
                    <div className="dim">{PIN_FUNCTIONS[f].detail}</div>
                  </td>
                  <td className="diy-pin">{board.pins[f]}</td>
                  <td className="muted">{board.role === 'pantalla' && PIN_FUNCTIONS[f].kind === 'sensor' ? 'Común de las entradas (COM)' : PIN_FUNCTIONS[f].other}</td>
                </tr>
              ))}
            </tbody>
          </table>
          <WiringDiagram board={board} used={used} />
          <div className="dim" style={{ fontSize: 13, marginTop: 4 }}>
            Línea continua: del pin al componente · Línea discontinua: el otro cable de cada componente, todos unidos a{' '}
            {board.role === 'pantalla' ? 'COM' : 'GND'}.
          </div>
        </>
      )}

      <div className="label" style={{ marginTop: 10 }}>Cómo instalarlo</div>
      <ol className="diy-steps">
        {installSteps(board).map((s) => (
          <li key={s}>{s}</li>
        ))}
      </ol>
    </div>
  );
}

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
