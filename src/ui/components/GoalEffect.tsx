/**
 * Celebraciones a pantalla completa del partido: «¡GOL!» al marcar y
 * «GOL ANULADO» al restar o deshacer un gol. Solo CSS (sin temporizadores)
 * y sin capturar toques.
 */
import type { CSSProperties } from 'react';
import type { Team } from '../../match-engine';
import type { EffectsLevel } from '../../services/persistence';
import { initials } from '../../services/players';

function hash(seed: string): number {
  let x = 7;
  for (const ch of seed) x = (x * 33 + ch.charCodeAt(0)) % 100_003;
  return x;
}

/** Variantes de la pantalla completa de gol; se elige una según el gol. */
/** Cada variante tiene su propio color (no el del equipo) para no repetir siempre azul. */
export const GOAL_SHOW_VARIANTS = [
  'zoom',
  'slide',
  'neon',
  'stamp',
  'split',
  'letters',
  'rays',
  'glitch',
  'gooool',
  'confetti',
  'flip',
] as const;
export type GoalShowVariant = (typeof GOAL_SHOW_VARIANTS)[number];

const CONFETTI = ['#ffe14d', '#3dff7a', '#ff5fa2', '#4fd8ff', '#ffffff', '#ff8a1f'];

export function pickGoalShowVariant(seed: string): GoalShowVariant {
  return GOAL_SHOW_VARIANTS[hash(seed) % GOAL_SHOW_VARIANTS.length];
}

/**
 * Pantalla completa «¡GOL! · EQUIPO …»: aparece nada más marcar y dura 3,2 s,
 * hasta pasado el bloqueo de 3 s.
 */
export function GoalShow({
  team,
  level,
  seed,
  label,
  people,
}: {
  team: Team;
  level: EffectsLevel;
  seed: string;
  label?: string | null;
  people: { id: string; name: string; photo?: string }[];
}) {
  if (level === 'off') return null;
  const variant = pickGoalShowVariant(seed);
  const word = variant === 'gooool' ? '¡GOOOOL!' : '¡GOL!';
  return (
    <div className={`goal-show gs-${team} gs-v-${variant} ${level === 'reduced' ? 'reduced' : ''}`} aria-hidden="true">
      <div className="gs-deco">
        {variant === 'confetti' &&
          Array.from({ length: 34 }, (_, i) => {
            const r = (hash(`${seed}-${i}`) % 1000) / 1000;
            const r2 = (hash(`${i}-${seed}`) % 1000) / 1000;
            return (
              <span
                key={i}
                className="gs-confetti"
                style={
                  {
                    left: `${r * 100}%`,
                    '--c': CONFETTI[i % CONFETTI.length],
                    '--delay': `${r2 * 0.6}s`,
                    '--spin': `${(r - 0.5) * 900}deg`,
                    '--drift': `${(r2 - 0.5) * 160}px`,
                  } as CSSProperties
                }
              />
            );
          })}
      </div>
      <div className="gs-word" data-text={word}>
        {[...word].map((ch, i) => (
          <span key={i} style={{ '--i': i } as CSSProperties}>
            {ch}
          </span>
        ))}
      </div>
      <div className="gs-team">EQUIPO {team === 'white' ? 'BLANCO' : 'AZUL'}</div>
      {label && <div className="gs-label">{label}</div>}
      <div className="gs-people">
        {people.map((p) => (
          <div key={p.id} className="gs-person">
            <span className="gs-photo">{p.photo ? <img src={p.photo} alt="" draggable={false} /> : initials(p.name)}</span>
            <span className="gs-name">{p.name}</span>
          </div>
        ))}
      </div>
    </div>
  );
}

/** Variantes de «GOL ANULADO» (en rojo); se elige una cada vez. */
export const ANNUL_VARIANTS = ['cross', 'stamp', 'glitch', 'var'] as const;
export type AnnulVariant = (typeof ANNUL_VARIANTS)[number];

export function pickAnnulVariant(seed: string): AnnulVariant {
  return ANNUL_VARIANTS[hash(seed) % ANNUL_VARIANTS.length];
}

/** Pantalla completa roja «GOL ANULADO» al restar un gol (−1) o deshacerlo. */
export function AnnulShow({ team, level, seed }: { team: Team; level: EffectsLevel; seed: string }) {
  if (level === 'off') return null;
  const variant = pickAnnulVariant(seed);
  return (
    <div className={`annul-show an-v-${variant} ${level === 'reduced' ? 'reduced' : ''}`} aria-hidden="true">
      <div className="an-deco" />
      {variant === 'var' && <div className="an-var">REVISIÓN VAR</div>}
      <div className="an-word">
        <span>GOL</span>
        <span>ANULADO</span>
      </div>
      {variant === 'cross' && (
        <svg className="an-cross" viewBox="0 0 100 100" preserveAspectRatio="none">
          <line x1="8" y1="8" x2="92" y2="92" />
          <line x1="92" y1="8" x2="8" y2="92" />
        </svg>
      )}
      <div className="an-team">
        EQUIPO {team === 'white' ? 'BLANCO' : 'AZUL'} · −1
      </div>
    </div>
  );
}
