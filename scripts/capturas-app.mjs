// Capturas de la app a 800 × 480 (el tamaño de la pantalla de 7") para compararlas con la placa.
// Uso: npm run dev (en otra consola) y después `node scripts/capturas-app.mjs [escena…]`.
// Guarda un PNG por escena en docs/capturas-app/. Hora fija (sábado 17/10/2026, 18:30) para que las capturas
// salgan siempre iguales y la placa se pueda poner a la misma hora.
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const OUT = join(ROOT, 'docs', 'capturas-app');
const URL = process.env.APP_URL ?? 'http://localhost:5173/';
export const FIXED_TIME = new Date('2026-10-17T18:30:00+02:00');

// Esperas largas (bloqueo de 3 s tras un gol…) también en tiempo real: las animaciones de CSS no siguen al reloj
// simulado, y la pantalla de gol tiene que haberse ido de verdad.
export const wait = async (page, ms) => {
  await page.clock.runFor(ms);
  if (ms >= 3000) await page.waitForTimeout(3400);
};
// La app deja 14 px arriba para la barra del móvil (Stage.tsx): con la ventana 14 px más alta, el lienzo queda a
// 800 × 480 exactos y sin escalar, como en la placa. Se recorta esa franja en capturas y medidas.
export const TOP = 14;

/** Toca el elemento que contiene ese texto (el primero visible). */
export async function tap(page, text, opts = {}) {
  const loc = page.getByText(text, { exact: opts.exact ?? false }).filter({ visible: true }).first();
  await loc.click({ timeout: 4000 });
  await wait(page, opts.after ?? 400);
}
export async function tapSel(page, sel, after = 400) {
  await page.locator(sel).filter({ visible: true }).first().click({ timeout: 4000 });
  await wait(page, after);
}

function demo(fn) {
  return Object.assign(fn, { demo: true });
}

/** Gol del equipo `t` tocando su marcador; espera a que pase el bloqueo de 3 s salvo que se pida otra espera. */
export async function gol(p, t, after = 3300) {
  await tapSel(p, `.score-btn.score-${t}`, after);
}

/** Partido rápido Alex (Blanco) contra José (Azul), por goles (5) o por tiempo (5 min por parte). */
export async function partidoRapido(p, { tiempo = false, start = true, modo = 'quick' } = {}) {
  await tapSel(p, `.mode-card.mode-${modo}`, 300);
  await tapSel(p, `.mode-card.mode-${modo}`, 1500);
  if (tiempo) {
    await p.locator('.cond').nth(1).locator('.cond-box').click();
    await p.locator('.cond').nth(0).locator('.cond-box').click();
    await wait(p, 400);
  }
  await tapSel(p, '.continue-btn', 1500);
  await tap(p, 'Alex', { exact: true });
  await tap(p, 'SIGUIENTE', { after: 800 });
  await tap(p, 'José', { exact: true });
  await tap(p, 'CONTINUAR', { after: 1500 });
  // Clasificatorio: antes del partido, la previa con el pronóstico (EMPEZAR PARTIDO).
  if (modo === 'ranked') await tapSel(p, '.pre-start', 1500);
  if (start) await tapSel(p, '.overlay-countdown', 1200);
}

// Cada escena parte de la app recién abierta (datos vacíos, salvo `demo`) y llega a su pantalla.
export const SCENES = {
  inicio: async () => {},
  'inicio-elegida': async (p) => {
    await tapSel(p, '.mode-card.mode-quick', 1200);
  },
  configuracion: async (p) => {
    await tapSel(p, '.mode-card.mode-quick', 300);
    await tapSel(p, '.mode-card.mode-quick', 1500);
  },
  jugadores: async (p) => {
    await SCENES.configuracion(p);
    await tapSel(p, '.continue-btn', 1500);
  },
  'jugadores-demo': Object.assign(async (p) => {
    await SCENES.configuracion(p);
    await tapSel(p, '.continue-btn', 1500);
  }, { demo: true }),
  'jugadores-azul': Object.assign(async (p) => {
    await SCENES['jugadores-demo'](p);
    await tap(p, 'Alex', { exact: true });
    await tap(p, 'SIGUIENTE', { after: 800 });
  }, { demo: true }),
  'cuenta-atras': demo(async (p) => {
    await partidoRapido(p, { start: false });
  }),
  partido: demo(async (p) => {
    await partidoRapido(p);
  }),
  'partido-gol': demo(async (p) => {
    await partidoRapido(p);
    await gol(p, 'white', 600);
  }),
  'partido-marcador': demo(async (p) => {
    await partidoRapido(p);
    for (const t of ['white', 'blue', 'white', 'blue', 'white']) await gol(p, t);
    await wait(p, 65_000);
  }),
  'bola-de-partido': demo(async (p) => {
    await partidoRapido(p);
    for (const t of ['white', 'blue', 'white', 'white', 'white']) await gol(p, t);
  }),
  pausa: demo(async (p) => {
    await partidoRapido(p);
    await gol(p, 'white');
    await wait(p, 30_000);
    await tapSel(p, '.pause-btn', 600);
  }),
  victoria: demo(async (p) => {
    await partidoRapido(p);
    for (const t of ['white', 'blue', 'white', 'white', 'white', 'white']) await gol(p, t);
    await wait(p, 2500);
  }),
  descanso: demo(async (p) => {
    await partidoRapido(p, { tiempo: true });
    await gol(p, 'white');
    await gol(p, 'blue');
    await gol(p, 'blue');
    await wait(p, 5 * 60_000);
  }),
  prorroga: demo(async (p) => {
    await partidoRapido(p, { tiempo: true });
    await gol(p, 'white');
    await wait(p, 5 * 60_000);
    await tapSel(p, '.overlay-period .overlay-cta', 1000);
    await gol(p, 'blue');
    await wait(p, 5 * 60_000);
    await tapSel(p, '.overlay-period .overlay-cta', 1000);
    await wait(p, 20_000);
  }),
  penaltis: demo(async (p) => {
    await SCENES.prorroga(p);
    await wait(p, 5 * 60_000);
    for (let i = 0; i < 3; i++) {
      const o = p.locator('.overlay-period .overlay-cta');
      if (await o.isVisible()) await tapSel(p, '.overlay-period .overlay-cta', 1000);
    }
    await tapSel(p, '.pen-col.is-turn .pen-goal', 800);
    await tapSel(p, '.pen-col.is-turn .pen-miss', 800);
  }),
  reposo: demo(async (p) => {
    await wait(p, 6 * 60_000);
  }),
  retos: demo(async (p) => {
    await tapSel(p, '.menu-challenges', 1200);
  }),
  'retos-vacio': async (p) => {
    await tapSel(p, '.menu-challenges', 1200);
  },
  // Ficha de Alex (desde Retos, tocando su fila) y cada pestaña.
  ...Object.fromEntries(
    [['general'], ['clasificatorios', 'CLASIFICATORIOS'], ['habitos', 'Hábitos'], ['rivales', 'Rivales'], ['historial', 'Historial'],
      ['torneos', 'Torneos'], ['logros', 'Logros']].map(([name, tab, sub]) => [
      `perfil-${name}`,
      demo(async (p) => {
        await tapSel(p, '.menu-challenges', 1200);
        await tapSel(p, '.challenge-row', 1200);
        if (name === 'clasificatorios') await tap(p, tab, { exact: true });
        else if (tab) await tapSel(p, `[role="tab"]:has-text("${tab}")`, 600);
      }),
    ]),
  ),
  // Cara a cara: ficha de Alex → RIVALES → el primero de «Enfrentamientos directos» (o su némesis).
  'cara-a-cara': demo(async (p) => {
    await SCENES['perfil-rivales'](p);
    await tapSel(p, '.list button.row', 1200);
  }),
  'cara-a-cara-nemesis': demo(async (p) => {
    await SCENES['perfil-rivales'](p);
    await tapSel(p, '.card-btn:has-text("Némesis")', 1200);
  }),
  // «⋯ Opciones» de la ficha de Alex: el menú y «Título y melodía».
  'perfil-opciones': demo(async (p) => {
    await SCENES['perfil-general'](p);
    await tap(p, '⋯ Opciones');
  }),
  'perfil-opciones-estilo': demo(async (p) => {
    await SCENES['perfil-opciones'](p);
    await tap(p, 'Título y melodía');
  }),
};

export async function openApp(browser, { demo = false } = {}) {
  const ctx = await browser.newContext({ viewport: { width: 800, height: 480 + TOP }, deviceScaleFactor: 1, locale: 'es-ES', timezoneId: 'Europe/Madrid' });
  const page = await ctx.newPage();
  await page.clock.install({ time: FIXED_TIME });
  await page.goto(URL);
  await page.evaluate(() => document.fonts.ready);
  await wait(page, 1500);
  if (demo) {
    // Ajustes → Datos → «Modo prueba» (12 jugadores, más de 100 partidos y 4 torneos, siempre los mismos).
    await tapSel(page, '.home-icon-btn', 600);
    await page.getByRole('tab', { name: 'Datos' }).click();
    await wait(page, 600);
    await page.locator('.demo-card').getByText('Modo prueba', { exact: true }).click();
    await wait(page, 2500);
    await page.goto(URL);
    await wait(page, 1500);
  }
  return { ctx, page };
}

// --medir: además del PNG, guarda <escena>.json con cada elemento visible (caja, letra, colores, bordes, sombras,
// textos e imágenes) para reproducir la pantalla en la placa con las mismas medidas.
const MEDIR = process.argv.includes('--medir');

async function medir(page) {
  return page.evaluate((top) => {
    const out = [];
    const skip = new Set(['transparent', 'rgba(0, 0, 0, 0)', 'none', '0px', 'normal']);
    const walk = (el, depth) => {
      const r = el.getBoundingClientRect();
      const cs = getComputedStyle(el);
      if (cs.display === 'none' || cs.visibility === 'hidden' || r.width < 1 || r.height < 1) return;
      const own = [...el.childNodes].filter((n) => n.nodeType === 3).map((n) => n.textContent.trim()).join(' ').trim();
      const e = { d: depth, tag: el.tagName.toLowerCase(), cls: (el.getAttribute('class') || '').slice(0, 80),
        x: Math.round(r.x * 10) / 10, y: Math.round((r.y - top) * 10) / 10, w: Math.round(r.width * 10) / 10, h: Math.round(r.height * 10) / 10 };
      if (own) Object.assign(e, { text: own, font: `${cs.fontStyle === 'italic' ? 'i' : ''}${cs.fontWeight} ${cs.fontSize}`,
        lh: cs.lineHeight, ls: cs.letterSpacing, color: cs.color, align: cs.textAlign, tt: cs.textTransform, ts: cs.textShadow });
      for (const [k, v] of [['bg', cs.backgroundColor], ['bgi', cs.backgroundImage], ['border', cs.borderTopWidth !== '0px' ? `${cs.borderTopWidth} ${cs.borderTopColor}` : 'none'],
        ['radius', cs.borderTopLeftRadius], ['shadow', cs.boxShadow], ['opacity', cs.opacity === '1' ? 'none' : cs.opacity],
        ['transform', cs.transform], ['filter', cs.filter], ['clip', cs.clipPath]]) if (!skip.has(v)) e[k] = v.length > 300 ? v.slice(0, 300) + '…' : v;
      if (el.tagName === 'IMG') e.src = el.currentSrc.split('/').pop().split('?')[0];
      if (el.tagName === 'svg') { e.svg = el.outerHTML.slice(0, 400); out.push(e); return; }
      out.push(e);
      for (const c of el.children) walk(c, depth + 1);
    };
    walk(document.querySelector('.stage') || document.body, 0);
    return out;
  }, TOP);
}

async function main() {
  mkdirSync(OUT, { recursive: true });
  const wanted = process.argv.slice(2).filter((a) => !a.startsWith('--'));
  const names = wanted.length ? wanted : Object.keys(SCENES);
  const browser = await chromium.launch();
  for (const name of names) {
    const scene = SCENES[name];
    if (!scene) throw new Error(`Escena desconocida: ${name}`);
    const { ctx, page } = await openApp(browser, { demo: scene.demo });
    try {
      await scene(page);
      await page.screenshot({ path: join(OUT, `${name}.png`), clip: { x: 0, y: TOP, width: 800, height: 480 } });
      if (MEDIR) writeFileSync(join(OUT, `${name}.json`), JSON.stringify(await medir(page), null, 1));
      console.log(`ok  ${name}`);
    } catch (err) {
      await page.screenshot({ path: join(OUT, `${name}-ERROR.png`), clip: { x: 0, y: TOP, width: 800, height: 480 } });
      console.log(`MAL ${name}: ${err.message.split('\n')[0]}`);
    }
    await ctx.close();
  }
  await browser.close();
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
