/**
 * Partido Loco: un animal sale de un agujero de gusano, hace su travesura y se vuelve a ir.
 *
 * La escena se dibuja encima del marcador (sin oscurecerlo) y va al ritmo de VISIT_TIMING:
 * el controlador aplica la travesura (VISIT_APPLY) y termina la visita (VISIT_END) a esos
 * tiempos, así el número cambia justo cuando el animal lo toca.
 *
 * Ardilla: piezas recortadas de sus hojas de personaje (carrera de 6 fotogramas, cuerpo, cola
 * y cabezas con expresiones). Caracol y gato: provisionales hasta tener sus imágenes.
 */
import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { VISIT_TIMING, otherTeam, type Visitor } from '../../match-engine';
import { assetUrl } from './assets';

type Pose = 'hidden' | 'pop' | 'run' | 'grab' | 'carry' | 'wink' | 'dive';
type Face = 'sorpresa' | 'ladrona' | 'picara' | 'guino' | 'risa' | 'burla';

interface Pt {
  x: number;
  y: number;
}

/**
 * Rectángulo de un elemento del marcador, relativo a la pantalla del partido. La app se dibuja
 * en un lienzo escalado para que quepa en cualquier pantalla: se deshace esa escala.
 */
function rectOf(root: HTMLElement, selector: string) {
  const el = root.querySelector(selector);
  const base = root.getBoundingClientRect();
  if (!el) return null;
  const k = base.width / root.clientWidth || 1;
  const r = el.getBoundingClientRect();
  return { x: (r.left - base.left) / k, y: (r.top - base.top) / k, w: r.width / k, h: r.height / k };
}

export function VisitorOverlay({ visitor }: { visitor: Visitor }) {
  const box = useRef<HTMLDivElement>(null);
  const actor = useRef<HTMLDivElement>(null);
  const portal = useRef<HTMLDivElement>(null);
  const [pose, setPose] = useState<Pose>('hidden');
  const [face, setFace] = useState<Face>('sorpresa');
  const [facing, setFacing] = useState<1 | -1>(1);
  const [frame, setFrame] = useState(0);
  const [pops, setPops] = useState<{ id: number; x: number; y: number; text: string; tone: 'plus' | 'minus' }[]>([]);
  const [glow, setGlow] = useState<string | null>(null);
  const timing = VISIT_TIMING[visitor.animal];

  // Fotogramas de la carrera (12 por segundo).
  useEffect(() => {
    if (pose !== 'run' && pose !== 'carry') return;
    const id = window.setInterval(() => setFrame((f) => (f + 1) % 6), 85);
    return () => window.clearInterval(id);
  }, [pose]);

  useLayoutEffect(() => {
    const root = box.current?.closest('.match') as HTMLElement | null;
    const el = actor.current;
    const hole = portal.current;
    if (!root || !el || !hole || !box.current) return;
    const W = root.clientWidth;
    const H = root.clientHeight;
    const timers: number[] = [];
    const at = (ms: number, fn: () => void) => timers.push(window.setTimeout(fn, ms));
    const pop = (p: Pt, text: string, tone: 'plus' | 'minus') =>
      setPops((list) => [...list, { id: Date.now() + Math.random(), x: p.x, y: p.y, text, tone }]);

    // El agujero de gusano se abre abajo, en el centro.
    const P: Pt = { x: W / 2, y: H * 0.93 };
    hole.style.left = `${P.x}px`;
    hole.style.top = `${P.y}px`;
    hole.animate(
      [
        { transform: 'translate(-50%, -50%) scale(0)', opacity: 0 },
        { transform: 'translate(-50%, -50%) scale(1)', opacity: 1, offset: 700 / timing.end },
        { transform: 'translate(-50%, -50%) scale(1)', opacity: 1, offset: (timing.end - 650) / timing.end },
        { transform: 'translate(-50%, -50%) scale(0)', opacity: 0 },
      ],
      { duration: timing.end, fill: 'forwards', easing: 'ease-in-out' },
    );

    const T = timing.end;
    const move = (points: { t: number; p: Pt; s?: number; r?: number; o?: number }[]) =>
      el.animate(
        points.map(({ t, p, s = 1, r = 0, o = 1 }) => ({
          offset: Math.min(1, t / T),
          transform: `translate(${p.x}px, ${p.y}px) translate(-50%, -100%) scale(${s}) rotate(${r}deg)`,
          opacity: o,
        })),
        { duration: T, fill: 'forwards', easing: 'linear' },
      );

    if (visitor.animal === 'squirrel') {
      const from = visitor.from ?? 'white';
      const to = otherTeam(from);
      const victim = rectOf(root, `.score-${from}`);
      const target = rectOf(root, `.score-${to}`);
      if (!victim || !target) return;
      // Se planta en la parte baja de cada tarjeta, por el lado del centro.
      const V: Pt = { x: victim.x + victim.w * (from === 'white' ? 0.66 : 0.34), y: victim.y + victim.h * 0.98 };
      const B: Pt = { x: target.x + target.w * (to === 'white' ? 0.66 : 0.34), y: target.y + target.h * 0.98 };
      const drop = timing.apply;
      move([
        { t: 0, p: P, s: 0.1, o: 0 },
        { t: 650, p: P, s: 0.15 },
        { t: 1000, p: { x: P.x, y: P.y - H * 0.18 }, s: 1.05 },
        { t: 1350, p: P, s: 1 },
        { t: 2550, p: V },
        { t: 2950, p: V },
        { t: drop - 150, p: B },
        { t: drop + 1100, p: B },
        { t: T - 800, p: P },
        { t: T - 400, p: { x: P.x, y: P.y + 10 }, s: 0.4, r: 25 },
        { t: T, p: { x: P.x, y: P.y + 20 }, s: 0.05, r: 60, o: 0 },
      ]);
      const dir = (a: Pt, b: Pt) => (b.x >= a.x ? 1 : -1);
      at(0, () => {
        setPose('pop');
        setFace('sorpresa');
      });
      at(1350, () => {
        setPose('run');
        setFacing(dir(P, V));
      });
      at(2550, () => {
        setPose('grab');
        setFace('ladrona');
        setGlow(from);
        pop({ x: victim.x + victim.w / 2, y: victim.y + victim.h * 0.35 }, '−1', 'minus');
      });
      at(2950, () => {
        setPose('carry');
        setFacing(dir(V, B));
        setGlow(null);
      });
      at(drop - 150, () => {
        setPose('wink');
        setFace('guino');
        setGlow(to);
        pop({ x: target.x + target.w / 2, y: target.y + target.h * 0.35 }, '+1', 'plus');
      });
      at(drop + 500, () => setFace('burla'));
      at(drop + 1100, () => {
        setPose('run');
        setFacing(dir(B, P));
        setGlow(null);
      });
      at(T - 800, () => {
        setPose('dive');
        setFace('risa');
      });
    } else {
      // Caracol o gato: van a la meta (por goles) o al reloj (por tiempo).
      const goal = visitor.goals !== undefined ? rectOf(root, '.period-goals') : rectOf(root, '.clock-panel');
      if (!goal) return;
      const G: Pt = { x: goal.x + goal.w / 2, y: goal.y + goal.h + (visitor.goals !== undefined ? 70 : 60) };
      const text =
        visitor.goals !== undefined ? (visitor.goals > 0 ? '+1 GOL' : '−1 GOL') : visitor.timeMs! > 0 ? '+1:00' : '−1:00';
      const tone = (visitor.goals ?? visitor.timeMs ?? 0) > 0 ? 'plus' : 'minus';
      if (visitor.animal === 'snail') {
        move([
          { t: 0, p: P, s: 0.1, o: 0 },
          { t: 700, p: P, s: 1 },
          { t: timing.apply - 200, p: G },
          { t: T - 900, p: G },
          { t: T - 300, p: { x: G.x, y: G.y + 20 }, s: 0.1, r: -30, o: 0 },
          { t: T, p: G, s: 0, o: 0 },
        ]);
        at(0, () => setPose('run'));
      } else {
        // El gato salta en arco hasta el objetivo y vuelve de otro salto.
        const mid = (a: Pt, b: Pt): Pt => ({ x: (a.x + b.x) / 2, y: Math.min(a.y, b.y) - H * 0.25 });
        move([
          { t: 0, p: P, s: 0.1, o: 0 },
          { t: 600, p: P, s: 1 },
          { t: 1300, p: mid(P, G), r: -10 },
          { t: 2000, p: G },
          { t: timing.apply + 900, p: G },
          { t: timing.apply + 1500, p: mid(G, P), r: 12 },
          { t: T - 500, p: P, s: 0.6 },
          { t: T, p: P, s: 0, o: 0 },
        ]);
        at(0, () => setPose('run'));
        at(2000, () => setPose('grab'));
      }
      at(timing.apply, () => {
        setGlow(visitor.goals !== undefined ? 'goals' : 'clock');
        pop({ x: goal.x + goal.w / 2, y: goal.y - 6 }, text, tone);
      });
      at(timing.apply + 900, () => setGlow(null));
    }
    return () => timers.forEach((id) => window.clearTimeout(id));
  }, [visitor, timing]);

  return (
    <div className={`visit-layer ${glow ? `glow-${glow}` : ''}`} ref={box} aria-live="polite">
      <div className="wh-portal" ref={portal} aria-hidden="true">
        <span className="wh-ring" />
        <span className="wh-ring r2" />
        <span className="wh-core" />
        <span className="wh-beam" />
      </div>
      <div className={`visit-actor animal-${visitor.animal} pose-${pose}`} ref={actor}>
        {visitor.animal === 'squirrel' ? <Squirrel pose={pose} face={face} facing={facing} frame={frame} /> : <Provisional animal={visitor.animal} facing={facing} />}
      </div>
      {pops.map((p) => (
        <span key={p.id} className={`visit-pop ${p.tone}`} style={{ left: p.x, top: p.y }}>
          {p.text}
        </span>
      ))}
    </div>
  );
}

/** La ardilla montada con sus piezas. */
function Squirrel({ pose, face, facing, frame }: { pose: Pose; face: Face; facing: 1 | -1; frame: number }) {
  if (pose === 'hidden') return null;
  if (pose === 'run' || pose === 'carry') {
    return (
      <div className="sq" style={{ transform: `scaleX(${facing})` }}>
        <img className="sq-run" src={assetUrl(`ardilla-carrera-${frame}`)} alt="" draggable={false} />
        {pose === 'carry' && <span className="sq-token">1</span>}
      </div>
    );
  }
  // De pie: cola que se mueve detrás, cuerpo y cabeza con la expresión del momento.
  return (
    <div className="sq sq-stand" style={{ transform: `scaleX(${facing})` }}>
      <img className="sq-tail" src={assetUrl(pose === 'pop' ? 'ardilla-cola-5' : 'ardilla-cola-2')} alt="" draggable={false} />
      <img className="sq-body" src={assetUrl('ardilla-cuerpo-34')} alt="" draggable={false} />
      <img className="sq-head" src={assetUrl(`ardilla-cara-${face}`)} alt="" draggable={false} />
      {pose === 'grab' && <img className="sq-hand" src={assetUrl('ardilla-mano-agarra')} alt="" draggable={false} />}
      {pose === 'wink' && <img className="sq-hand win" src={assetUrl('ardilla-mano-victoria')} alt="" draggable={false} />}
    </div>
  );
}

/** Caracol y gato provisionales (hasta tener sus hojas de personaje). */
function Provisional({ animal, facing }: { animal: Visitor['animal']; facing: 1 | -1 }) {
  return (
    <div className="visit-prov" style={{ transform: `scaleX(${-facing})` }}>
      {animal === 'snail' ? '🐌' : '🐈‍⬛'}
      {animal === 'cat' && <span className="visit-visor" aria-hidden="true" />}
    </div>
  );
}
