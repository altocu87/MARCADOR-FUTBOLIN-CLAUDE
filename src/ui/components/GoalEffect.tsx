/**
 * Celebraciones a pantalla completa del partido: «¡GOL!» al marcar y
 * «GOL ANULADO» al restar o deshacer un gol. Solo CSS (sin temporizadores)
 * y sin capturar toques.
 */
import { useApp } from '../../app/AppContext';
import { findClub } from '../../services/clubs';
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
  // Segunda tanda (estilos en celebrations.css).
  'fireworks',
  'bubbles',
  'lava',
  'comic',
  'retro',
  'pixel',
  'hearts',
  'spotlight',
  'typewriter',
  'golden',
  'warp',
  'bounce',
  'paint',
  'matrix',
  'shockwave',
] as const;

/** Palabra que se muestra en cada variante (por defecto «¡GOL!»). */
const WORD: Partial<Record<GoalShowVariant, string>> = {
  gooool: '¡GOOOOL!',
  comic: '¡GOLAZO!',
  golden: '¡GOLAZO!',
  lava: '¡GOOOL!',
};

/** Variantes con partículas decorativas y cuántas lleva cada una. */
const PARTICLES: Partial<Record<GoalShowVariant, number>> = {
  confetti: 34,
  fireworks: 42,
  bubbles: 22,
  hearts: 18,
  warp: 44,
  matrix: 22,
  paint: 8,
  lava: 10,
  shockwave: 4,
};
const MATRIX_CHARS = 'GOL01⚽GOLGOL10';
export type GoalShowVariant = (typeof GOAL_SHOW_VARIANTS)[number];

/** Número pseudoaleatorio 0–1 bien repartido (FNV-1a + mezcla), para colocar partículas. */
function scatter(key: string): number {
  let h = 2166136261;
  for (let i = 0; i < key.length; i += 1) h = Math.imul(h ^ key.charCodeAt(i), 16777619);
  h = Math.imul(h ^ (h >>> 16), 0x45d9f3b);
  h = Math.imul(h ^ (h >>> 13), 0x45d9f3b);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}

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
  const { prefs } = useApp();
  const club = findClub(prefs.clubs, people.map((p) => p.id));
  if (level === 'off') return null;
  const variant = pickGoalShowVariant(seed);
  const word = WORD[variant] ?? '¡GOL!';
  const rnd = (i: number, salt: string) => scatter(`${seed}-${salt}-${i}`);
  return (
    <div className={`goal-show gs-${team} gs-v-${variant} ${level === 'reduced' ? 'reduced' : ''}`} aria-hidden="true">
      <div className="gs-deco">
        {Array.from({ length: PARTICLES[variant] ?? 0 }, (_, i) => (
          <span
            key={i}
            className={variant === 'confetti' ? 'gs-confetti' : 'gs-p'}
            style={
              {
                left: `${rnd(i, 'x') * 100}%`,
                top: variant === 'confetti' ? undefined : `${rnd(i, 'y') * 100}%`,
                '--c': CONFETTI[i % CONFETTI.length],
                '--delay': `${rnd(i, 'd') * 0.7}s`,
                '--spin': `${(rnd(i, 'r') - 0.5) * 900}deg`,
                '--drift': `${(rnd(i, 'd') - 0.5) * 160}px`,
                '--a': `${rnd(i, 'a') * 360}deg`,
                '--s': rnd(i, 's'),
                '--i': i,
              } as CSSProperties
            }
          >
            {variant === 'matrix'
              ? Array.from({ length: 14 }, (_, k) => MATRIX_CHARS[Math.floor(scatter(`${seed}m${i}-${k}`) * MATRIX_CHARS.length)]).join('\n')
              : variant === 'hearts'
                ? '♥'
                : null}
          </span>
        ))}
      </div>
      {variant === 'bounce' && <div className="gs-ball">⚽</div>}
      {variant === 'comic' && <div className="gs-burst" />}
      <div className="gs-word" data-text={word}>
        {[...word].map((ch, i) => (
          <span key={i} style={{ '--i': i } as CSSProperties}>
            {ch}
          </span>
        ))}
      </div>
      <div className="gs-team">{club ? club.name.toLocaleUpperCase('es') : `EQUIPO ${team === 'white' ? 'BLANCO' : 'AZUL'}`}</div>
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

/** Variantes de «GOL ANULADO» (en tonos rojos); se elige una cada vez. */
export const ANNUL_VARIANTS = [
  'cross',
  'stamp',
  'glitch',
  'var',
  // Segunda tanda (estilos en celebrations.css).
  'shatter',
  'siren',
  'tvoff',
  'redcard',
  'rewind',
  'erase',
  'drop',
  'ecg',
  'strike',
  'deflate',
] as const;
export type AnnulVariant = (typeof ANNUL_VARIANTS)[number];
/** Variantes que primero enseñan «¡GOL!» y después lo anulan. */
const ANNUL_WITH_PRE: readonly AnnulVariant[] = ['erase', 'strike', 'deflate'];

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
      {variant === 'rewind' && <div className="an-var an-rew">◀◀ REBOBINANDO</div>}
      {variant === 'redcard' && <div className="an-card" />}
      {ANNUL_WITH_PRE.includes(variant) && <div className="an-pre">¡GOL!</div>}
      {variant === 'ecg' && (
        <svg className="an-ecg" viewBox="0 0 400 100" preserveAspectRatio="none">
          <polyline points="0,50 90,50 110,15 130,85 150,50 200,50 215,30 230,70 245,50 400,50" />
        </svg>
      )}
      <div className="an-word">
        {variant === 'shatter' ? (
          <>
            <span>
              {[...'GOL'].map((ch, i) => (
                <i key={i} style={{ '--i': i } as CSSProperties}>
                  {ch}
                </i>
              ))}
            </span>
            <span>
              {[...'ANULADO'].map((ch, i) => (
                <i key={i} style={{ '--i': i + 3 } as CSSProperties}>
                  {ch}
                </i>
              ))}
            </span>
          </>
        ) : (
          <>
            <span>GOL</span>
            <span>ANULADO</span>
          </>
        )}
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
