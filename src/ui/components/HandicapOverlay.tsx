/**
 * Partido Loco a pantalla completa (reloj parado):
 * 1. Cuenta atrás 3 · 2 · 1 con uno de 12 estilos distintos (cambia en cada hándicap).
 * 2. El hándicap: icono, título, a quién afecta y cuánto dura. Se sigue tocando la pantalla
 *    o con un pulsador. El penalti tiene su propia pantalla: GOL o FALLO y se sigue jugando.
 * 3. Al acabar uno con duración: «¡VUELTA A LA NORMALIDAD!», que sigue solo en 3 s.
 */
import { useEffect, useRef } from 'react';
import {
  HANDICAP_END_PAUSE_MS,
  HANDICAP_ICON,
  handicapDurationText,
  handicapEndText,
  handicapText,
  type Handicap,
} from '../../match-engine';
import { sound } from '../../services/sound/sound';
import type { MatchController } from '../screens/useMatchController';

export const COUNTDOWN_STYLES = 12;

/** Estilo de cuenta atrás de un hándicap: fijo para ese hándicap, distinto entre uno y otro. */
export function countdownStyle(id: string): number {
  let h = 0;
  for (const c of id) h = (h * 33 + c.charCodeAt(0)) >>> 0;
  return h % COUNTDOWN_STYLES;
}

/** Nombres que pasan como en una ruleta (estilo 7). */
const ROULETTE = ['GOLES x2', 'ROBO', 'PENALTI', 'CONGELADO', 'TRASPASO', 'MANO MALA', 'GOL x3', 'CAMBIO DE SITIO'];

export function HandicapOverlay({ ctl }: { ctl: MatchController }) {
  const { state, now, send } = ctl;
  const h = state.handicap;
  if (state.phase !== 'handicap' || !h) return null;

  if (h.stage === 'ending') {
    const left = Math.max(0, (h.resumeAt ?? now) - now);
    return (
      <button className="overlay hc-overlay hc-normal" onClick={() => send({ type: 'HANDICAP_GO' })}>
        <span className="hc-normal-title">¡VUELTA A LA NORMALIDAD!</span>
        <span className="hc-normal-sub">{handicapEndText(h.spec)}</span>
        <span className="hc-normal-bar" aria-hidden="true">
          <span style={{ width: `${(left / HANDICAP_END_PAUSE_MS) * 100}%` }} />
        </span>
        <span className="overlay-hint">Toca para seguir ya</span>
      </button>
    );
  }

  if (ctl.handicapIntroLeft > 0) {
    return <HandicapCountdown spec={h.spec} left={ctl.handicapIntroLeft} replaced={h.replaced} onSkip={ctl.skipHandicapIntro} />;
  }

  const t = handicapText(h.spec, state.participants);
  if (h.spec.kind === 'penalty') {
    const team = h.spec.team === 'blue' ? 'blue' : 'white';
    return (
      <div className={`overlay hc-overlay hc-card hc-penalty hc-team-${team}`}>
        <span className="hc-icon" aria-hidden="true">
          {HANDICAP_ICON.penalty}
        </span>
        <span className="hc-title">{t.title}</span>
        <span className="hc-detail">{t.detail}</span>
        <div className="hc-pen-actions">
          <button className="hc-pen-btn goal" onClick={() => send({ type: 'HANDICAP_PENALTY', scored: true, source: 'touch' })}>
            ⚽ GOL
          </button>
          <button className="hc-pen-btn miss" onClick={() => send({ type: 'HANDICAP_PENALTY', scored: false, source: 'touch' })}>
            ✕ FALLO
          </button>
        </div>
        <span className="overlay-hint">Con pulsadores: el del equipo que tira = gol · el del rival = fallo</span>
      </div>
    );
  }

  return (
    <button className="overlay hc-overlay hc-card" onClick={() => send({ type: 'HANDICAP_GO' })}>
      <span className="hc-kicker">HÁNDICAP</span>
      <span className="hc-icon" aria-hidden="true">
        {HANDICAP_ICON[h.spec.kind]}
      </span>
      <span className="hc-title">{t.title}</span>
      <span className="hc-detail">{t.detail}</span>
      <span className="hc-duration">⏱ {handicapDurationText(h.spec)}</span>
      <span className="hc-go">Toca la pantalla o pulsa un botón para seguir</span>
    </button>
  );
}

/** Cuenta atrás 3 · 2 · 1 antes de revelar el hándicap, con uno de los 12 estilos. */
function HandicapCountdown({ spec, left, replaced, onSkip }: { spec: Handicap; left: number; replaced?: Handicap; onSkip: () => void }) {
  const n = Math.min(3, Math.max(1, Math.ceil(left / 1000)));
  const style = countdownStyle(spec.id);
  const last = useRef(0);
  useEffect(() => {
    if (n !== last.current) {
      last.current = n;
      sound.play('countdown');
    }
  }, [n]);
  return (
    <button className={`overlay hc-overlay hc-count hc-v${style}`} onClick={onSkip} aria-label="Saltar la cuenta atrás">
      <span className="hc-bg" aria-hidden="true" />
      {replaced && <span className="hc-replaced">{handicapEndText(replaced)}</span>}
      <span className="hc-count-label">¡HÁNDICAP!</span>
      <span className="hc-num-wrap">
        <span key={n} className="hc-num">
          {n}
        </span>
      </span>
      {style === 7 && (
        <span key={`r${n}`} className="hc-roulette" aria-hidden="true">
          {ROULETTE.map((r) => (
            <span key={r}>{r}</span>
          ))}
        </span>
      )}
      <span className="overlay-hint">Toca para saltar</span>
    </button>
  );
}
