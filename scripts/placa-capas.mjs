// «Capas» de la app para la pantalla de 7": lo que no cambia en cada pantalla (fondos, marcos, brillos, títulos)
// se fotografía de la propia app a 800 × 480 y la placa lo pinta tal cual; LVGL dibuja encima lo que cambia
// (números, reloj, nombres, fotos) con la misma letra Inter.
//
//   node scripts/placa-capas.mjs [capa…]      (con `npm run dev` en marcha)
//
// Cada capa es:
//   - un FONDO: la pantalla entera con lo que cambia oculto (`ocultar`: selectores CSS), o
//   - una VARIANTE de un fondo (tarjeta elegida, botón pulsado…): se compara con su fondo y solo se guarda el
//     rectángulo que cambia, con su posición, o
//   - un RECORTE con transparencia (`solo`: se oculta todo lo demás y se recorta `clip`).
// Salida: build/placa/capas/<nombre>.png y build/placa/capas.json (posición de cada recorte), que
// scripts/placa_recursos.py convierte para la placa y usa para escribir hardware/esp32s3_pantalla7/ui_capas.h.
import { chromium } from 'playwright';
import { mkdirSync, readFileSync, writeFileSync, existsSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { SCENES, openApp, wait, tapSel, TOP, gol, partidoRapido } from './capturas-app.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const OUT = join(ROOT, 'build', 'placa', 'capas');
const MANIFEST = join(ROOT, 'build', 'placa', 'capas.json');
const FULL = join(ROOT, 'build', 'placa', 'capas-completas');

const configEscena = (modo) => async (p) => {
  await tapSel(p, `.mode-card.mode-${modo}`, 300);
  await tapSel(p, `.mode-card.mode-${modo}`, 1500);
};
/** Animaciones sin fin, quietas (en la placa las mueve LVGL o no se mueven). */
const QUIETO = '.score-btn.match-point, .match-point-badge { animation: none !important; }';
/** Capas de encima de la pantalla sin su fondo oscuro (lo pone la placa). */
const OVERLAY_SIN_FONDO =
  '.overlay, .modal-backdrop { background: transparent !important; backdrop-filter: none !important; } .match-main, .match-bg { visibility: hidden !important; }';
/** Partido rápido Alex–José con esos goles (cada uno tras el bloqueo) y luego unos pasos. */
const partidoEsc = (goles, pasos = [], modo = 'quick') => async (p) => {
  await partidoRapido(p, { modo });
  for (const t of goles) await gol(p, t);
  await wait(p, 3000);
  for (const paso of pasos) await paso(p);
};
const partidoTiempo = (goles, ms) => async (p) => {
  await partidoRapido(p, { tiempo: true });
  for (const t of goles) await gol(p, t);
  await wait(p, ms - goles.length * 3300);
};
const partidoProrroga = async (p) => {
  await SCENES.prorroga(p);
  await wait(p, 3000);
};
/** Fondo de inicio y configuración en la placa (Alex, 18/10/2026): en vez de la foto de la app, un degradado radial de
 * cuatro colores (violeta, cian, rosa y azul noche). Va en las capas, así que en la placa cuesta lo mismo que antes. */
const DEGRADADO = `radial-gradient(circle at 16% 20%, rgba(139, 92, 246, 0.85) 0%, rgba(139, 92, 246, 0) 52%),
  radial-gradient(circle at 86% 26%, rgba(34, 211, 238, 0.75) 0%, rgba(34, 211, 238, 0) 48%),
  radial-gradient(circle at 58% 108%, rgba(236, 72, 153, 0.7) 0%, rgba(236, 72, 153, 0) 55%),
  radial-gradient(ellipse at 50% 45%, #1b1f4a 0%, #090c1f 100%)`;
const FONDO_INICIO = `.home-bg-img, .tron-grid { visibility: hidden !important; } .home-bg { background: ${DEGRADADO} !important; }`;
const FONDO_CONFIG = `.setup-bg { visibility: hidden !important; } .setup-screen { background: ${DEGRADADO} !important; }`;
const JUG_OCULTAR = ['.select-layout', '.screen-footer > *', '.screen-header .badge', '.screen-header .btn-sm', '.screen-title small'];
const VIC_OCULTAR = ['.vic-score', '.vic-facts', '.vic-body', '.confetti', '.match-main', '.goal-show', '.banner'];
const VIC_CSS = '.victory-rays { animation: none !important; }';
const victoriaEsc = (t) => async (p) => {
  await partidoRapido(p);
  for (let i = 0; i < 5; i++) await gol(p, t);
  await wait(p, 3000);
};
/** El cartel del comienzo del partido, con su texto y color cambiados y la animación parada en el centro. */
const cartelEsc = (texto, sub, tono) => async (p) => {
  await partidoRapido(p, { start: false });
  await p.locator('.overlay-countdown').click();
  await p.clock.runFor(50);
  await p.waitForTimeout(700);
  await p.evaluate(
    ([t, s, tone]) => {
      document.getAnimations().forEach((a) => a.pause());
      const banner = document.querySelector('.banner');
      banner.className = banner.className.replace(/banner-(accent|gold|danger|chaos)/, `banner-${tone}`);
      const strip = banner.querySelector('.banner-strip');
      strip.querySelector('.banner-text').textContent = t;
      const subEl = strip.querySelector('.banner-sub');
      if (subEl) {
        subEl.textContent = s;
        subEl.style.display = s ? '' : 'none';
      }
    },
    [texto, sub, tono],
  );
};
const GOL_VARIANTES = ['zoom', 'gooool', 'neon', 'golden', 'comic', 'confetti'];
const GOL_PALABRA = { gooool: '¡GOOOOL!', golden: '¡GOLAZO!', comic: '¡GOLAZO!' };
/** Gol de `t` y, con el reloj quieto (la app no repinta), se fuerza la variante y se deja que su animación llegue al final. */
const golEsc = (t, v) => async (p) => {
  await partidoRapido(p);
  await p.locator(`.score-btn.score-${t}`).click();
  await p.clock.runFor(50);
  await p.evaluate(
    ([v, w]) => {
      const el = document.querySelector('.goal-show');
      el.className = el.className.replace(/gs-v-[a-z]+/, `gs-v-${v}`);
      const word = el.querySelector('.gs-word');
      if (word && w) word.innerHTML = [...w].map((c) => `<span>${c}</span>`).join('');
    },
    [v, GOL_PALABRA[v]],
  );
  await p.waitForTimeout(1400);
  await p.evaluate(() => document.getAnimations().forEach((a) => a.pause()));
};
const COND = (n) => `.cond:nth-child(${n})`;
/** Toca el recuadro del número de la tarjeta n (activa o desactiva goles / tiempo). */
const caja = (n) => async (p) => tapSel(p, `${COND(n)} .cond-box`, 500);
const veces = (n, sel) => async (p) => {
  for (let i = 0; i < n; i++) await tapSel(p, sel, 60);
  await wait(p, 400);
};
const pulsa = (sel, ms = 600) => async (p) => tapSel(p, sel, ms);
/** Pulsado a medias: el instante en que la app enseña el botón hundido, antes de cambiar de pantalla. */
const hundido = (sel) => async (p) => {
  await p.locator(sel).filter({ visible: true }).first().dispatchEvent('pointerdown');
  await p.locator(sel).first().evaluate((el) => el.classList.add('is-pressed'));
  await wait(p, 120);
};

/** Cambia el DOM a mano (forzar un estado que no se puede tocar, poner un texto…). */
const forzar = (fn, arg) => async (p) => {
  await p.evaluate(fn, arg);
  await wait(p, 400);
};
/** Botón mantenido (estado :active de la app). */
const mantener = (sel) => async (p) => {
  const b = await p.locator(sel).filter({ visible: true }).first().boundingBox();
  await p.mouse.move(b.x + b.width / 2, b.y + b.height / 2);
  await p.mouse.down();
  await wait(p, 200);
};
const texto = (sel, t) => forzar(([s, v]) => document.querySelectorAll(s).forEach((e) => (e.textContent = v)), [sel, t]);
const clase = (sel, cls, on) => forzar(([s, c, o]) => document.querySelectorAll(s).forEach((e) => e.classList.toggle(c, o)), [sel, cls, on]);

export const CAPAS = [
  // ---------------------------------------------------------------- Inicio
  { nombre: 'inicio', escena: 'inicio', css: FONDO_INICIO },
  { nombre: 'inicio-sel-rapido', base: 'inicio', pasos: [pulsa('.mode-card.mode-quick', 800)] },
  { nombre: 'inicio-sel-loco', base: 'inicio', pasos: [pulsa('.mode-card.mode-chaos', 800)] },
  { nombre: 'inicio-sel-clasificatorio', base: 'inicio', pasos: [pulsa('.mode-card.mode-ranked', 800)] },
  { nombre: 'inicio-pul-rapido', base: 'inicio', pasos: [hundido('.mode-card.mode-quick')] },
  { nombre: 'inicio-pul-loco', base: 'inicio', pasos: [hundido('.mode-card.mode-chaos')] },
  { nombre: 'inicio-pul-clasificatorio', base: 'inicio', pasos: [hundido('.mode-card.mode-ranked')] },
  { nombre: 'inicio-pul-torneo', base: 'inicio', pasos: [hundido('.menu-tournament')] },
  { nombre: 'inicio-pul-ranking', base: 'inicio', pasos: [hundido('.menu-ranking')] },
  { nombre: 'inicio-pul-retos', base: 'inicio', pasos: [hundido('.menu-challenges')] },
  { nombre: 'inicio-ayuda-loco', base: 'inicio', pasos: [pulsa('.mode-chaos + .card-help, .card-wrap:nth-child(2) .card-help', 800)] },
  { nombre: 'inicio-ayuda-clasificatorio', base: 'inicio', pasos: [pulsa('.card-wrap:nth-child(3) .card-help', 800)] },

  // ---------------------------------------------------------------- Configuración (SetupScreen.tsx)
  // Fondo: solo goles (lo de siempre), sin números ni reloj (los dibuja la placa). Los demás estados, con toques
  // de verdad (la app repinta y desharía cualquier cambio forzado a mano).
  { nombre: 'config', escena: configEscena('quick'), ocultar: ['.cond-box .num', '.setup-clock'], css: FONDO_CONFIG },
  { nombre: 'config-modo-loco', base: 'config', escena: configEscena('chaos') },
  { nombre: 'config-modo-clasificatorio', base: 'config', escena: configEscena('ranked') },
  { nombre: 'config-tiempo', base: 'config', pasos: [caja(2), caja(1)] },
  { nombre: 'config-ambas', base: 'config', pasos: [caja(2)] },
  // Tarjeta de goles apagada, sola (sobre «las dos encendidas» solo cambia la izquierda): con ella y config-ambas
  // la placa repinta solo la tarjeta que se toca.
  { nombre: 'config-izq-apagada', base: 'config-ambas', pasos: [caja(1)] },
  { nombre: 'config-izq-max', base: 'config', pasos: [veces(15, `${COND(1)} .cond-btn.up`)] },
  { nombre: 'config-izq-min', base: 'config', pasos: [veces(4, `${COND(1)} .cond-btn.down`)] },
  { nombre: 'config-izq-pul-mas', base: 'config', pasos: [mantener(`${COND(1)} .cond-btn.up`)] },
  { nombre: 'config-izq-pul-menos', base: 'config', pasos: [mantener(`${COND(1)} .cond-btn.down`)] },
  { nombre: 'config-der-max', base: 'config-tiempo', pasos: [veces(25, `${COND(2)} .cond-btn.up`)] },
  { nombre: 'config-der-min', base: 'config-tiempo', pasos: [veces(4, `${COND(2)} .cond-btn.down`)] },
  { nombre: 'config-der-pul-mas', base: 'config-tiempo', pasos: [mantener(`${COND(2)} .cond-btn.up`)] },
  { nombre: 'config-der-pul-menos', base: 'config-tiempo', pasos: [mantener(`${COND(2)} .cond-btn.down`)] },
  { nombre: 'config-pul-continuar', base: 'config', pasos: [mantener('.continue-btn')] },
  // ---------------------------------------------------------------- Elegir jugadores (SelectPlayersScreen.tsx)
  // Fondo de cada turno (Equipo BLANCO / Equipo AZUL); columnas, fichas, avisos y botones los dibuja la placa.
  { nombre: 'jugadores-blanco', escena: 'jugadores-demo', ocultar: JUG_OCULTAR },
  { nombre: 'jugadores-azul', escena: 'jugadores-azul', ocultar: JUG_OCULTAR },

  // ---------------------------------------------------------------- Partido (MatchScreen.tsx)
  // Fondo: 1–1 por goles (los dos «Anular» activos), sin números, reloj, textos que cambian ni fichas (fase 2).
  {
    nombre: 'partido',
    demo: true,
    escena: partidoEsc(['white', 'blue']),
    ocultar: ['.score-num', '.clock-panel .clock', '.period-goals', '.center-cond', '.lock-msg', '.center-test', '.team-cards',
      '.center-badges .badge', '.banner', '.goal-show', '.annul-show', '.streak-badge', '.score-wrap'],
    css: `${QUIETO} .score-sheen, .score-btn::after { display: none !important; }`,
  },
  // Fondo y cartel de cada modo (LOCO: morado; CLASIFICATORIO: dorado). El LOCO de la app saca hándicaps cada
  // minuto; la placa aún no los tiene, así que se fotografía antes del primero.
  { nombre: 'partido-modo-loco', base: 'partido', escena: partidoEsc(['white', 'blue'], [], 'chaos') },
  { nombre: 'partido-modo-clasificatorio', base: 'partido', escena: partidoEsc(['white', 'blue'], [], 'ranked') },
  // Racha («🔥 x3»): pegatina con transparencia para cada número; la placa la pone arriba en la tarjeta.
  ...[3, 4, 5, 6, 7, 8, 9].map((n) => ({
    nombre: `racha-${n}`,
    demo: true,
    escena: partidoEsc(['white', 'white', 'white']),
    solo: '.streak-badge',
    margen: 16,
    pasos: [forzar((t) => (document.querySelector('.streak-badge').textContent = t), `🔥 x${n}`)],
    css: '.streak-badge { animation: none !important; }',
  })),
  { nombre: 'partido-anular-off-blanco', base: 'partido', escena: partidoEsc(['blue']) },
  { nombre: 'partido-anular-off-azul', base: 'partido', escena: partidoEsc(['white']) },
  { nombre: 'partido-pul-anular-blanco', base: 'partido', pasos: [mantener('.annul-white')] },
  { nombre: 'partido-pul-anular-azul', base: 'partido', pasos: [mantener('.annul-blue')] },
  { nombre: 'partido-pul-pausa', base: 'partido', pasos: [mantener('.pause-btn')] },
  // Bola de partido (el aro rojo late; aquí, quieto en su punto más fuerte) con su cartel.
  { nombre: 'partido-mp-blanco', base: 'partido', escena: partidoEsc(['white', 'blue', 'white', 'white', 'white']), mostrar: ['.center-badges .badge'] },
  { nombre: 'partido-mp-azul', base: 'partido', escena: partidoEsc(['white', 'blue', 'blue', 'blue', 'blue']), mostrar: ['.center-badges .badge'] },
  { nombre: 'partido-mp-ambos', base: 'partido', escena: partidoEsc(['white', 'blue', 'white', 'blue', 'white', 'blue', 'white', 'blue']), mostrar: ['.center-badges .badge'] },
  // Color del marco del reloj: amarillo (último minuto), rojo (últimos 20 s) y menta con «GOL DE ORO» (prórroga).
  { nombre: 'partido-reloj-amarillo', base: 'partido', escena: partidoTiempo(['white', 'blue'], 4 * 60_000 + 15_000) },
  { nombre: 'partido-reloj-rojo', base: 'partido', escena: partidoTiempo(['white', 'blue'], 4 * 60_000 + 45_000) },
  { nombre: 'partido-prorroga', base: 'partido', escena: partidoProrroga, mostrar: ['.center-badges .badge'] },
  // Pausa: la capa oscura la pone la placa; aquí, lo de encima (título, texto y botones) con transparencia.
  { nombre: 'pausa', demo: true, escena: partidoEsc(['white', 'blue'], [pulsa('.pause-btn', 800)]), solo: '.overlay-pause', css: OVERLAY_SIN_FONDO, clip: { x: 130, y: 76, w: 540, h: 330 } },
  { nombre: 'pausa-pul-continuar', base: 'pausa', pasos: [mantener('.overlay-pause .overlay-cta')] },
  { nombre: 'pausa-pul-abandonar', base: 'pausa', pasos: [mantener('.overlay-pause .btn-danger')] },
  { nombre: 'pausa-abandonar-seguir', demo: true, escena: partidoEsc(['white', 'blue'], [pulsa('.pause-btn', 800), pulsa('.overlay-pause .btn-danger', 800)]), medir: '.modal .btn-ghost' },
  { nombre: 'pausa-abandonar-si', demo: true, escena: partidoEsc(['white', 'blue'], [pulsa('.pause-btn', 800), pulsa('.overlay-pause .btn-danger', 800)]), medir: '.modal .btn-danger' },
  { nombre: 'pausa-abandonar', demo: true, escena: partidoEsc(['white', 'blue'], [pulsa('.pause-btn', 800), pulsa('.overlay-pause .btn-danger', 800)]), solo: '.modal-backdrop', css: OVERLAY_SIN_FONDO, clip: { x: 0, y: 0, w: 800, h: 480 } },

  // ---------------------------------------------------------------- Penaltis (PenaltiesView)
  // Fondo sin nada encima; la portería de neón sin balón y el botón de deshacer, con transparencia.
  { nombre: 'penaltis', demo: true, escena: 'penaltis', ocultar: ['.pen-main', '.match-top', '.goal-show', '.annul-show', '.banner'] },
  { nombre: 'penaltis-porteria', demo: true, escena: 'penaltis', solo: '.neon-goal', margen: 10, css: '.neon-goal .ball { visibility: hidden !important; }' },
  { nombre: 'penaltis-deshacer', demo: true, escena: 'penaltis', solo: '.pen-center .btn-sm', margen: 6 },

  // ---------------------------------------------------------------- Victoria (VictoryScreen.tsx)
  // Fondo con rayos (quietos), título y botones, uno por ganador; el marcador, los datos y el confeti, en directo.
  { nombre: 'victoria-blanco', demo: true, escena: victoriaEsc('white'), ocultar: VIC_OCULTAR, css: VIC_CSS },
  { nombre: 'victoria-azul', demo: true, escena: victoriaEsc('blue'), ocultar: VIC_OCULTAR, css: VIC_CSS },
  { nombre: 'victoria-pul-revancha', base: 'victoria-blanco', pasos: [mantener('.vic-main')] },
  { nombre: 'victoria-pul-inicio', base: 'victoria-blanco', pasos: [mantener('.vic-home')] },

  // ---------------------------------------------------------------- Carteles (Banner de graphics.tsx)
  // Franja inclinada con el texto; la placa la hace entrar por la izquierda y salir por la derecha (banner-in).
  ...[
    ['cartel-partido', 'PARTIDO', '¡A JUGAR!', 'accent'],
    ['cartel-partido-loco', 'PARTIDO', '¡A JUGAR!', 'chaos'],
    ['cartel-1-parte', '1ª PARTE', '¡A JUGAR!', 'accent'],
    ['cartel-2-parte', '2ª PARTE', '¡A JUGAR!', 'accent'],
    ['cartel-prorroga', 'PRÓRROGA', 'GOL DE ORO', 'gold'],
    ['cartel-penaltis', 'PENALTIS', 'TANDA', 'gold'],
    ['cartel-muerte-subita', 'MUERTE SÚBITA', '', 'danger'],
  ].map(([nombre, texto, sub, tono]) => ({
    nombre,
    demo: true,
    escena: cartelEsc(texto, sub, tono),
    solo: '.banner',
    clip: { x: 0, y: 130, w: 800, h: 220 },
  })),

  // ---------------------------------------------------------------- ¡GOL! (GoalEffect.tsx)
  // Seis de las variantes de la app, por equipo, en su momento de más lucimiento (sin fotos de jugadores: fase 2).
  ...['white', 'blue'].flatMap((t) =>
    GOL_VARIANTES.map((v) => ({
      nombre: `gol-${t === 'white' ? 'blanco' : 'azul'}-${v}`,
      demo: true,
      escena: golEsc(t, v),
      css: '.gs-people, .gs-label { visibility: hidden !important; }',
    })),
  ),

  // Cifras del número grande, encendida y apagada (todas del mismo ancho: tabular-nums), con su brillo.
  ...[true, false].flatMap((on) =>
    [...'0123456789'].map((d) => ({
      nombre: `config-cifra-${on ? 'on' : 'off'}-${d}`,
      escena: configEscena('quick'),
      solo: '.cond:nth-child(1) .num',
      margen: 24,
      pasos: [clase('.cond:nth-child(1)', 'on', on), texto('.cond:nth-child(1) .num', d)],
    })),
  ),
  // Medidas para colocar los números de dos cifras.
  { nombre: 'config-num-1', escena: configEscena('quick'), medir: '.cond:nth-child(1) .num', pasos: [texto('.cond:nth-child(1) .num', '8')] },
  { nombre: 'config-num-2', escena: configEscena('quick'), medir: '.cond:nth-child(1) .num', pasos: [texto('.cond:nth-child(1) .num', '88')] },
];

async function shot(page, file) {
  await page.waitForTimeout(450);  // las transiciones de CSS van con el reloj de verdad: que terminen
  await page.screenshot({ path: file, clip: { x: 0, y: TOP, width: 800, height: 480 } });
}

/** Oculta (sin mover nada) los elementos que LVGL dibujará en directo. */
async function ocultar(page, sels) {
  if (!sels?.length) return;
  await page.addStyleTag({ content: `${sels.join(',')} { visibility: hidden !important; }` });
  await wait(page, 100);
}

/** Rectángulo que cambia entre dos PNG (scripts/diff_rect.py, sin contar píxeles sueltos). */
function diffRect(a, b) {
  const r = spawnSync('python', [join(ROOT, 'scripts', 'diff_rect.py'), a, b], { encoding: 'utf8' });
  const v = r.stdout.trim().split(/\s+/).map(Number);
  return v.length === 4 && v.every((n) => !Number.isNaN(n)) ? { x: v[0], y: v[1], w: v[2], h: v[3] } : null;
}

async function hacer(browser, capaIn, byName) {
  let capa = capaIn;
  const base = capa.base ? byName[capa.base] : null;
  // Cadena de capas de las que sale esta (la raíz da la escena; cada una añade lo que oculta y sus pasos).
  const chain = [];
  for (let c = capaIn; c; c = c.base ? byName[c.base] : null) chain.unshift(c);
  const raiz = chain[0];
  const esc = capa.escena ?? raiz.escena;  // una variante puede venir de otra escena (p. ej. otro modo)
  const escena = typeof esc === 'function' ? esc : SCENES[esc];
  if (!escena) throw new Error(`Capa ${capa.nombre}: escena desconocida`);
  const { ctx, page } = await openApp(browser, { demo: escena.demo || raiz.demo });
  try {
    await escena(page);
    await ocultar(page, chain.flatMap((c) => c.ocultar ?? []));
    const css = chain.map((c) => c.css ?? '').join('\n');
    const mostrar = chain.flatMap((c) => c.mostrar ?? []);
    if (css || mostrar.length) {
      await page.addStyleTag({ content: `${css}\n${mostrar.length ? `${mostrar.join(',')} { visibility: visible !important; }` : ''}` });
      await wait(page, 100);
    }
    for (const paso of chain.flatMap((c) => c.pasos ?? [])) await paso(page);
    const file = join(OUT, `${capa.nombre}.png`);
    if (capa.medir) {
      const b = await page.locator(capa.medir).first().boundingBox();
      return { x: Math.round(b.x * 10) / 10, y: Math.round((b.y - TOP) * 10) / 10, w: Math.round(b.width * 10) / 10, h: Math.round(b.height * 10) / 10, medida: true };
    }
    // Variante de una capa con transparencia: la misma zona, también con transparencia (no se compara).
    const soloDe = chain.slice().reverse().find((c) => c.solo);
    if (soloDe && !capa.solo) {
      capa = { ...capa, solo: soloDe.solo, clip: soloDe.clip, margen: soloDe.margen };
    }
    if (capa.solo) {
      // Recorte con transparencia: solo se ve el elemento pedido.
      await page.addStyleTag({
        content: `html,body,#root,.viewport,.stage,.screen{background:transparent !important;box-shadow:none !important}
          body *{visibility:hidden !important} ${capa.solo}, ${capa.solo} *{visibility:visible !important}`,
      });
      await wait(page, 100);
      await page.waitForTimeout(450);
      let c = capa.clip;
      if (!c) {
        const b = await page.locator(capa.solo).first().boundingBox();
        const m = capa.margen ?? 0;
        c = { x: Math.floor(b.x - m), y: Math.floor(b.y - TOP - m), w: Math.ceil(b.width + 2 * m), h: Math.ceil(b.height + 2 * m) };
      }
      await page.screenshot({ path: file, omitBackground: true, clip: { x: c.x, y: c.y + TOP, width: c.w, height: c.h } });
      return { ...c, alfa: true };
    }
    const full = join(FULL, `${capa.nombre}.png`);
    await shot(page, full);
    if (!base) {
      await shot(page, file);
      return { x: 0, y: 0, w: 800, h: 480 };
    }
    const r = diffRect(join(FULL, `${base.nombre}.png`), full);
    if (!r) throw new Error(`Capa ${capa.nombre}: no cambia nada respecto a ${base.nombre}`);
    await page.screenshot({ path: file, clip: { x: r.x, y: r.y + TOP, width: r.w, height: r.h } });
    return { ...r, base: base.nombre };
  } finally {
    await ctx.close();
  }
}

async function main() {
  mkdirSync(OUT, { recursive: true });
  mkdirSync(FULL, { recursive: true });
  const wanted = process.argv.slice(2);
  const byName = Object.fromEntries(CAPAS.map((c) => [c.nombre, c]));
  const manifest = existsSync(MANIFEST) ? JSON.parse(readFileSync(MANIFEST, 'utf8')) : {};
  const browser = await chromium.launch();
  // Los fondos primero: las variantes se comparan con ellos.
  // Se puede pedir una capa o un prefijo («config» hace todas las config-…); sus bases se rehacen si faltan.
  const lista = CAPAS.filter((c) => !wanted.length || wanted.some((w) => c.nombre === w.replace(/=$/, '') || (!w.endsWith('=') && c.nombre.startsWith(`${w}-`))));
  for (const c of [...lista]) for (let b = c.base; b; b = byName[b].base) if (!lista.includes(byName[b]) && !existsSync(join(FULL, `${b}.png`))) lista.push(byName[b]);
  const depth = (c) => (c.base ? 1 + depth(byName[c.base]) : 0);
  for (const capa of [...lista].sort((a, b) => depth(a) - depth(b))) {
    try {
      manifest[capa.nombre] = await hacer(browser, capa, byName);
      const m = manifest[capa.nombre];
      console.log(`ok  ${capa.nombre}  ${m.x},${m.y} ${m.w}×${m.h}`);
    } catch (err) {
      console.log(`MAL ${capa.nombre}: ${err.message.split('\n')[0]}`);
    }
  }
  await browser.close();
  // Varios procesos pueden hornear a la vez: se mezcla con lo que haya escrito otro mientras tanto.
  const now = existsSync(MANIFEST) ? JSON.parse(readFileSync(MANIFEST, 'utf8')) : {};
  for (const c of lista) if (manifest[c.nombre]) now[c.nombre] = manifest[c.nombre];
  writeFileSync(MANIFEST, JSON.stringify(now, null, 1));
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
