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

type Pose = 'hidden' | 'pop' | 'run' | 'sneak' | 'lift' | 'carry' | 'mock' | 'wink' | 'dive' | 'ball' | 'grab' | 'walk' | 'jump' | 'retch' | 'puke' | 'proud';

/** Postura de cuerpo entero de la ardilla para cada momento de la escena. */
const SQUIRREL_POSE: Partial<Record<Pose, string>> = {
  pop: 'ardilla-pose-sorpresa',
  sneak: 'ardilla-pose-sigilosa',
  lift: 'ardilla-pose-brazos-arriba',
  mock: 'ardilla-pose-burla',
  wink: 'ardilla-pose-victoria',
  dive: 'ardilla-pose-salto',
  ball: 'ardilla-pose-bola',
};

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
  // Gato vomitón: su gatera, lo que vomita y el charco.
  const door = useRef<HTMLDivElement>(null);
  const fish = useRef<HTMLSpanElement>(null);
  const hair = useRef<HTMLSpanElement>(null);
  const splat = useRef<HTMLSpanElement>(null);
  const vomitCat = visitor.animal === 'cat' && !!visitor.to;
  const [pose, setPose] = useState<Pose>('hidden');
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
    if (!root || !el || !box.current) return;
    const W = root.clientWidth;
    const H = root.clientHeight;
    const timers: number[] = [];
    const at = (ms: number, fn: () => void) => timers.push(window.setTimeout(fn, ms));
    const pop = (p: Pt, text: string, tone: 'plus' | 'minus') =>
      setPops((list) => [...list, { id: Date.now() + Math.random(), x: p.x, y: p.y, text, tone }]);

    if (vomitCat) {
      catScene({ root, el, W, H, T: timing.end, apply: timing.apply, to: visitor.to!, door: door.current, fish: fish.current, hair: hair.current, splat: splat.current, at, pop, setPose, setFacing, setGlow });
      return () => timers.forEach((id) => window.clearTimeout(id));
    }
    const hole = portal.current;
    if (!hole) return;

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
      // 0 sale del portal sorprendida · 1.3 corre · 2.4 se acerca sigilosa · 2.8 levanta el gol con los
      // brazos en alto · 3.3 corre con él · 4.55 lo suelta y se burla · 5.2 guiña · 5.9 vuelve corriendo
      // · 7.0 se lanza de cabeza · 7.4 hecha una bola entra girando en el portal.
      move([
        { t: 0, p: P, s: 0.1, o: 0 },
        { t: 600, p: P, s: 0.2 },
        { t: 950, p: { x: P.x, y: P.y - H * 0.2 }, s: 1.05 },
        { t: 1300, p: P, s: 1 },
        { t: 2400, p: { x: V.x + (V.x < P.x ? 70 : -70), y: V.y } },
        { t: 2800, p: V },
        { t: 3300, p: V },
        { t: drop - 150, p: B },
        { t: drop + 1350, p: B },
        { t: T - 800, p: { x: P.x, y: P.y - 10 } },
        { t: T - 450, p: { x: P.x, y: P.y - H * 0.16 }, r: 0 },
        { t: T - 150, p: { x: P.x, y: P.y + 10 }, s: 0.5, r: 360 },
        { t: T, p: { x: P.x, y: P.y + 20 }, s: 0.05, r: 540, o: 0 },
      ]);
      const dir = (a: Pt, b: Pt) => (b.x >= a.x ? 1 : -1);
      at(0, () => setPose('pop'));
      at(1300, () => {
        setPose('run');
        setFacing(dir(P, V));
      });
      at(2400, () => setPose('sneak'));
      at(2800, () => {
        setPose('lift');
        setGlow(from);
        pop({ x: victim.x + victim.w / 2, y: victim.y + victim.h * 0.35 }, '−1', 'minus');
      });
      at(3300, () => {
        setPose('carry');
        setFacing(dir(V, B));
        setGlow(null);
      });
      at(drop - 150, () => {
        setPose('mock');
        setFacing(dir(B, P));
        setGlow(to);
        pop({ x: target.x + target.w / 2, y: target.y + target.h * 0.35 }, '+1', 'plus');
      });
      at(drop + 500, () => setPose('wink'));
      at(drop + 1350, () => {
        setPose('run');
        setFacing(dir(B, P));
        setGlow(null);
      });
      at(T - 800, () => setPose('dive'));
      at(T - 450, () => setPose('ball'));
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
  }, [visitor, timing, vomitCat]);

  return (
    <div className={`visit-layer visit-${visitor.animal} ${glow ? `glow-${glow}` : ''}`} ref={box} aria-live="polite">
      {/* Ardilla: alarma de robo con los bordes en rojo y un rótulo de neón que pasa arriba. */}
      {visitor.animal === 'squirrel' && (
        <>
          <span className="alarm-frame" aria-hidden="true" />
          <div className="alarm-ticker" role="status">
            <span className="alarm-track">
              {Array.from({ length: 4 }, (_, i) => (
                <span key={i} className="alarm-text">
                  🚨 ¡ROBO EN MARCHA! · ¡CUIDADO CON LA ARDILLA! 🐿
                </span>
              ))}
            </span>
          </div>
        </>
      )}
      {vomitCat ? (
        <>
          {/* La gatera: marco de neón con su trampilla, que se balancea al pasar el gato. */}
          <div className="cat-door" ref={door} aria-hidden="true">
            <span className="cat-door-frame" />
            <span className="cat-door-flap" />
            <span className="cat-door-sign">🐾</span>
          </div>
          <span className="cat-splat" ref={splat} aria-hidden="true">
            <svg viewBox="0 0 120 80">
              <path
                d="M18 46c-9-2-14-10-6-15 6-4 13 1 16-3 3-6-4-13 4-16 7-3 11 6 17 4 6-2 5-11 13-11 9 0 9 10 15 12 7 2 12-6 18-2 7 5 0 12 5 16 5 4 15 1 16 8 1 7-9 9-11 14-2 6 5 12-2 15-8 3-11-6-18-5-7 1-8 10-17 9-9-1-8-10-15-11-7-1-10 8-18 5-7-3-2-11-7-15-4-3-10 0-10-5z"
                fill="#9acd32"
                stroke="#5d8a1c"
                strokeWidth="3"
              />
              <circle cx="36" cy="38" r="5" fill="#c6e86b" />
              <circle cx="72" cy="30" r="4" fill="#c6e86b" />
              <circle cx="86" cy="50" r="3" fill="#c6e86b" />
            </svg>
            <span className="cat-stink s1">〰</span>
            <span className="cat-stink s2">〰</span>
            <span className="cat-stink s3">〰</span>
          </span>
          <span className="cat-fish" ref={fish} aria-hidden="true">
            🐟<span className="cat-fly">·</span>
          </span>
          <span className="cat-hair" ref={hair} aria-hidden="true" />
        </>
      ) : (
        <div className="wh-portal" ref={portal} aria-hidden="true">
          <span className="wh-ring" />
          <span className="wh-ring r2" />
          <span className="wh-core" />
          <span className="wh-beam" />
        </div>
      )}
      <div className={`visit-actor animal-${visitor.animal} pose-${pose}`} ref={actor}>
        {visitor.animal === 'squirrel' ? (
          <Squirrel pose={pose} facing={facing} frame={frame} />
        ) : vomitCat ? (
          <VomitCat pose={pose} facing={facing} />
        ) : (
          <Provisional animal={visitor.animal} facing={facing} />
        )}
      </div>
      {pops.map((p) => (
        <span key={p.id} className={`visit-pop ${p.tone}`} style={{ left: p.x, top: p.y }}>
          {p.text}
        </span>
      ))}
    </div>
  );
}

/** La ardilla: carrera de 6 fotogramas o una de sus 8 posturas de cuerpo entero. */
function Squirrel({ pose, facing, frame }: { pose: Pose; facing: 1 | -1; frame: number }) {
  if (pose === 'hidden') return null;
  const run = pose === 'run' || pose === 'carry';
  const src = run ? `ardilla-carrera-${frame}` : SQUIRREL_POSE[pose];
  return (
    <div className={`sq sq-${pose}`} style={{ transform: `scaleX(${facing})` }}>
      <img key={run ? 'run' : pose} className="sq-img" src={src ? assetUrl(src) : undefined} alt="" draggable={false} />
      {/* El gol robado: bola de neón sobre las manos (al levantarlo) o sobre la cabeza (corriendo). */}
      {(pose === 'lift' || pose === 'carry') && <span className="sq-token">1</span>}
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

/**
 * Escena del gato vomitón (8,6 s): sale por una gatera abajo en el centro, va andando hasta el
 * marcador del equipo, salta encima, le dan arcadas y vomita un pescado apestoso y una bola de pelo
 * sobre el número (gol para ese equipo), se queda tan ancho y vuelve a meterse por la gatera.
 */
function catScene({
  root,
  el,
  W,
  H,
  T,
  apply,
  to,
  door,
  fish,
  hair,
  splat,
  at,
  pop,
  setPose,
  setFacing,
  setGlow,
}: {
  root: HTMLElement;
  el: HTMLElement;
  W: number;
  H: number;
  T: number;
  apply: number;
  to: 'white' | 'blue';
  door: HTMLElement | null;
  fish: HTMLElement | null;
  hair: HTMLElement | null;
  splat: HTMLElement | null;
  at: (ms: number, fn: () => void) => void;
  pop: (p: Pt, text: string, tone: 'plus' | 'minus') => void;
  setPose: (p: Pose) => void;
  setFacing: (f: 1 | -1) => void;
  setGlow: (g: string | null) => void;
}) {
  const card = rectOf(root, `.score-${to}`);
  if (!card || !door || !fish || !hair || !splat) return;
  const D: Pt = { x: W / 2, y: H * 0.97 };
  // Al pie de la tarjeta (por el lado del centro) y encima de ella.
  const side = to === 'white' ? 1 : -1;
  // Abajo, junto a la tarjeta (por el lado del centro); luego salta a la esquina de abajo de la tarjeta.
  const foot: Pt = { x: card.x + card.w / 2 + side * (card.w / 2 + 40), y: card.y + card.h + 6 };
  const top: Pt = { x: card.x + card.w / 2 + side * card.w * 0.3, y: card.y + card.h - 4 };
  const face: 1 | -1 = side === 1 ? -1 : 1; // en la tarjeta mira hacia el número
  const big = 1.35;
  const mouth: Pt = { x: top.x + face * 46, y: top.y - H * 0.13 };
  const hit: Pt = { x: card.x + card.w / 2, y: card.y + card.h * 0.45 };

  door.style.left = `${D.x}px`;
  door.style.top = `${D.y}px`;
  door.animate(
    [
      { transform: 'translate(-50%, -100%) scale(0)', opacity: 0 },
      { transform: 'translate(-50%, -100%) scale(1.1)', opacity: 1, offset: 450 / T },
      { transform: 'translate(-50%, -100%) scale(1)', opacity: 1, offset: 650 / T },
      { transform: 'translate(-50%, -100%) scale(1)', opacity: 1, offset: (T - 500) / T },
      { transform: 'translate(-50%, -100%) scale(0)', opacity: 0 },
    ],
    { duration: T, fill: 'forwards', easing: 'ease-in-out' },
  );
  // La trampilla se abre al salir y al volver a entrar.
  const flap = door.querySelector<HTMLElement>('.cat-door-flap');
  const swing = (delay: number) =>
    flap?.animate(
      [
        { transform: 'perspective(200px) rotateX(0deg)' },
        { transform: 'perspective(200px) rotateX(-75deg)', offset: 0.25 },
        { transform: 'perspective(200px) rotateX(35deg)', offset: 0.55 },
        { transform: 'perspective(200px) rotateX(-15deg)', offset: 0.8 },
        { transform: 'perspective(200px) rotateX(0deg)' },
      ],
      { duration: 900, delay, easing: 'ease-out' },
    );
  swing(700);
  swing(T - 1300);

  const move = (points: { t: number; p: Pt; s?: number; r?: number; o?: number }[]) =>
    el.animate(
      points.map(({ t, p, s = 1, r = 0, o = 1 }) => ({
        offset: Math.min(1, t / T),
        transform: `translate(${p.x}px, ${p.y}px) translate(-50%, -100%) scale(${s}) rotate(${r}deg)`,
        opacity: o,
      })),
      { duration: T, fill: 'forwards', easing: 'linear' },
    );
  const arc = (a: Pt, b: Pt, h: number): Pt => ({ x: (a.x + b.x) / 2, y: Math.min(a.y, b.y) - h });
  const back = { x: D.x, y: D.y - 4 };
  move([
    { t: 0, p: { x: D.x, y: D.y }, s: 0.4, o: 0 },
    { t: 700, p: { x: D.x, y: D.y }, s: 0.4, o: 0 },
    { t: 1000, p: { x: D.x, y: D.y - 6 }, s: 1 },
    { t: 2500, p: foot },
    { t: 2800, p: arc(foot, top, 70), r: side * -12, s: 1.2 },
    { t: 3100, p: top, s: big },
    { t: apply + 900, p: top, s: big },
    { t: apply + 1250, p: arc(top, foot, 50), r: side * 12, s: 1.2 },
    { t: apply + 1500, p: foot },
    { t: T - 1100, p: back },
    { t: T - 800, p: { x: D.x, y: D.y }, s: 0.5, o: 0 },
    { t: T, p: { x: D.x, y: D.y }, s: 0.2, o: 0 },
  ]);
  const toward = (a: Pt, b: Pt): 1 | -1 => (b.x >= a.x ? 1 : -1);
  at(700, () => setPose('walk'));
  at(1000, () => setFacing(toward(D, foot)));
  at(2500, () => setPose('jump'));
  at(3100, () => {
    setPose('retch');
    setFacing(face);
  });
  at(apply - 800, () => setPose('puke'));

  // Lo que vomita: primero el pescado y luego la bola de pelo, en arco hasta el número.
  const fly = (node: HTMLElement, delay: number, end: Pt, spin: number) =>
    node.animate(
      [
        { transform: `translate(${mouth.x}px, ${mouth.y}px) translate(-50%, -50%) scale(0.2) rotate(0deg)`, opacity: 1 },
        { transform: `translate(${(mouth.x + end.x) / 2}px, ${Math.min(mouth.y, end.y) - 60}px) translate(-50%, -50%) scale(1.1) rotate(${spin / 2}deg)`, opacity: 1, offset: 0.45 },
        { transform: `translate(${end.x}px, ${end.y}px) translate(-50%, -50%) scale(1) rotate(${spin}deg)`, opacity: 1, offset: 0.7 },
        { transform: `translate(${end.x}px, ${end.y}px) translate(-50%, -50%) scale(1.05, 0.9) rotate(${spin}deg)`, opacity: 1, offset: 0.78 },
        { transform: `translate(${end.x}px, ${end.y}px) translate(-50%, -50%) scale(1) rotate(${spin}deg)`, opacity: 1, offset: 0.93 },
        { transform: `translate(${end.x}px, ${end.y + 30}px) translate(-50%, -50%) scale(0.9) rotate(${spin}deg)`, opacity: 0 },
      ],
      { duration: T - (apply - 700) - 300, delay: delay, fill: 'forwards', easing: 'cubic-bezier(0.3, 0.7, 0.5, 1)' },
    );
  fly(fish, apply - 700, { x: hit.x - 34, y: hit.y + 6 }, face * 300);
  fly(hair, apply - 350, { x: hit.x + 38, y: hit.y + 30 }, face * -260);
  splat.animate(
    [
      { transform: `translate(${hit.x}px, ${hit.y}px) translate(-50%, -50%) scale(0)`, opacity: 0 },
      { transform: `translate(${hit.x}px, ${hit.y}px) translate(-50%, -50%) scale(1.25)`, opacity: 1, offset: 0.12 },
      { transform: `translate(${hit.x}px, ${hit.y}px) translate(-50%, -50%) scale(1)`, opacity: 0.95, offset: 0.2 },
      { transform: `translate(${hit.x}px, ${hit.y + 18}px) translate(-50%, -50%) scale(1, 1.15)`, opacity: 0.9, offset: 0.85 },
      { transform: `translate(${hit.x}px, ${hit.y + 30}px) translate(-50%, -50%) scale(1, 1.2)`, opacity: 0 },
    ],
    { duration: T - apply + 200, delay: apply - 300, fill: 'both', easing: 'ease-out' },
  );
  at(apply, () => {
    setGlow(to);
    pop({ x: hit.x, y: card.y + card.h * 0.18 }, '+1', 'plus');
  });
  at(apply + 400, () => setPose('proud'));
  at(apply + 900, () => {
    setGlow(null);
    setPose('jump');
  });
  at(apply + 1500, () => {
    setPose('walk');
    setFacing(toward(foot, back));
  });
}

const CAT_BUBBLE: Partial<Record<Pose, string>> = {
  retch: '🤢',
  puke: '¡BLUARGH!',
  proud: '😼',
};

/** Gato vomitón provisional (hasta tener su hoja de personaje): anda, salta, arcadas y vómito. */
function VomitCat({ pose, facing }: { pose: Pose; facing: 1 | -1 }) {
  if (pose === 'hidden') return null;
  return (
    <div className={`vcat vcat-${pose}`}>
      <span className="vcat-body" style={{ transform: `scaleX(${-facing})` }}>
        🐈
      </span>
      {CAT_BUBBLE[pose] && <span className={`vcat-bubble ${pose === 'puke' ? 'is-word' : ''}`}>{CAT_BUBBLE[pose]}</span>}
    </div>
  );
}
