/**
 * Celebración de gol: flash, explosión, ondas, partículas y líneas de velocidad.
 * Solo se dispara tras un gol aceptado; dura ~1 s, no captura toques y no
 * modifica el bloqueo del motor. Nivel «reducido» evita destellos repetidos.
 */
import { useMemo, type CSSProperties } from 'react';
import type { Team } from '../../match-engine';
import type { EffectsLevel } from '../../services/persistence';
import { initials } from '../../services/players';

export function GoalEffect({ team, level, seed, label }: { team: Team; level: EffectsLevel; seed: string; label?: string | null }) {
  const particles = useMemo(() => {
    let x = 0;
    for (const ch of seed) x = (x * 31 + ch.charCodeAt(0)) % 9973;
    const rnd = () => {
      x = (x * 9301 + 49297) % 233280;
      return x / 233280;
    };
    return Array.from({ length: 22 }, () => ({
      angle: rnd() * 360,
      dist: 90 + rnd() * 140,
      size: 4 + rnd() * 7,
      delay: rnd() * 0.12,
    }));
  }, [seed]);

  if (level === 'off') return null;
  const color = team === 'white' ? '#EEF6FF' : '#62D6FF';
  return (
    <div className={`goal-fx goal-fx-${team} ${level === 'reduced' ? 'reduced' : ''}`} aria-hidden="true">
      {level === 'full' && <div className="fx-flash" />}
      <div className="fx-ring" style={{ borderColor: color }} />
      {level === 'full' && <div className="fx-ring fx-ring-2" style={{ borderColor: color }} />}
      {level === 'full' && (
        <div className="fx-lines">
          {Array.from({ length: 8 }, (_, i) => (
            <span key={i} style={{ top: `${10 + i * 11}%`, animationDelay: `${i * 0.03}s`, background: color }} />
          ))}
        </div>
      )}
      <div className="fx-burst">
        {particles.slice(0, level === 'full' ? 22 : 8).map((p, i) => (
          <span
            key={i}
            style={
              {
                '--a': `${p.angle}deg`,
                '--d': `${p.dist}px`,
                width: p.size,
                height: p.size,
                background: color,
                animationDelay: `${p.delay}s`,
              } as CSSProperties
            }
          />
        ))}
      </div>
      <div className="fx-text" style={{ color: team === 'white' ? '#176DBC' : '#FFFFFF' }}>
        ¡GOL!
      </div>
      {label && <div className="fx-moment">{label}</div>}
    </div>
  );
}

/** Variantes de la pantalla completa de gol; se elige una según el gol. */
export const GOAL_SHOW_VARIANTS = ['zoom', 'slide', 'neon', 'stamp', 'split', 'letters', 'rays', 'glitch', 'gooool'] as const;
export type GoalShowVariant = (typeof GOAL_SHOW_VARIANTS)[number];

export function pickGoalShowVariant(seed: string): GoalShowVariant {
  let x = 7;
  for (const ch of seed) x = (x * 33 + ch.charCodeAt(0)) % 100_003;
  return GOAL_SHOW_VARIANTS[x % GOAL_SHOW_VARIANTS.length];
}

/**
 * Pantalla completa «¡GOL! · EQUIPO …» que aparece justo después de la animación
 * del gol y se apaga antes de que termine el bloqueo de 3 s. Solo CSS (sin
 * temporizadores) y sin capturar toques.
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
      <div className="gs-deco" />
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
