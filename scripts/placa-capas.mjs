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
import { SCENES, openApp, wait, tapSel, TOP } from './capturas-app.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const OUT = join(ROOT, 'build', 'placa', 'capas');
const MANIFEST = join(ROOT, 'build', 'placa', 'capas.json');

const pulsa = (sel, ms = 600) => async (p) => tapSel(p, sel, ms);
/** Pulsado a medias: el instante en que la app enseña el botón hundido, antes de cambiar de pantalla. */
const hundido = (sel) => async (p) => {
  await p.locator(sel).filter({ visible: true }).first().dispatchEvent('pointerdown');
  await p.locator(sel).first().evaluate((el) => el.classList.add('is-pressed'));
  await wait(p, 120);
};

export const CAPAS = [
  // ---------------------------------------------------------------- Inicio
  { nombre: 'inicio', escena: 'inicio' },
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
];

async function shot(page, file) {
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

async function hacer(browser, capa, byName) {
  const base = capa.base ? byName[capa.base] : null;
  const escena = SCENES[capa.escena ?? base?.escena];
  if (!escena) throw new Error(`Capa ${capa.nombre}: escena desconocida`);
  const { ctx, page } = await openApp(browser, { demo: escena.demo });
  try {
    await escena(page);
    await ocultar(page, [...(base?.ocultar ?? []), ...(capa.ocultar ?? [])]);
    for (const paso of capa.pasos ?? []) await paso(page);
    const file = join(OUT, `${capa.nombre}.png`);
    if (capa.solo) {
      // Recorte con transparencia: solo se ve el elemento pedido.
      await page.addStyleTag({
        content: `html,body,#root,.viewport,.stage,.screen{background:transparent !important;box-shadow:none !important}
          body *{visibility:hidden !important} ${capa.solo}, ${capa.solo} *{visibility:visible !important}`,
      });
      await wait(page, 100);
      const c = capa.clip;
      await page.screenshot({ path: file, omitBackground: true, clip: { x: c.x, y: c.y + TOP, width: c.w, height: c.h } });
      return { ...c, alfa: true };
    }
    await shot(page, file);
    if (!base) return { x: 0, y: 0, w: 800, h: 480 };
    const r = diffRect(join(OUT, `${base.nombre}.png`), file);
    if (!r) throw new Error(`Capa ${capa.nombre}: no cambia nada respecto a ${base.nombre}`);
    await page.screenshot({ path: file, clip: { x: r.x, y: r.y + TOP, width: r.w, height: r.h } });
    return { ...r, base: base.nombre };
  } finally {
    await ctx.close();
  }
}

async function main() {
  mkdirSync(OUT, { recursive: true });
  const wanted = process.argv.slice(2);
  const byName = Object.fromEntries(CAPAS.map((c) => [c.nombre, c]));
  const manifest = existsSync(MANIFEST) ? JSON.parse(readFileSync(MANIFEST, 'utf8')) : {};
  const browser = await chromium.launch();
  // Los fondos primero: las variantes se comparan con ellos.
  const lista = CAPAS.filter((c) => !wanted.length || wanted.includes(c.nombre) || wanted.includes(c.base));
  for (const capa of [...lista.filter((c) => !c.base), ...lista.filter((c) => c.base)]) {
    try {
      manifest[capa.nombre] = await hacer(browser, capa, byName);
      const m = manifest[capa.nombre];
      console.log(`ok  ${capa.nombre}  ${m.x},${m.y} ${m.w}×${m.h}`);
    } catch (err) {
      console.log(`MAL ${capa.nombre}: ${err.message.split('\n')[0]}`);
    }
  }
  await browser.close();
  writeFileSync(MANIFEST, JSON.stringify(manifest, null, 1));
}

main();
