// Partido Loco en la pantalla de 7": los iconos de los hándicaps (los emoji de la app, HANDICAP_ICON) y los
// animales de las visitas, como imágenes con transparencia para la placa. Se pintan con el navegador (que sabe
// dibujar los emoji en color) y se añaden a build/placa/capas.json para que scripts/placa_recursos.py los pase a
// la memoria de la placa.
//
//   node scripts/placa-loco.mjs
import { chromium } from 'playwright';
import { mkdirSync, readFileSync, writeFileSync, existsSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const OUT = join(ROOT, 'build', 'placa', 'capas');
const MANIFEST = join(ROOT, 'build', 'placa', 'capas.json');

// Mismos iconos que src/match-engine/handicaps.ts (HANDICAP_ICON), por el orden del motor de la placa.
const ICONS = [
  ['double-all', '✖2'], ['double-team', '✖2'], ['triple-next', '✖3'], ['steal', '🦹'], ['freeze-score', '🧊'],
  ['swap-positions', '🔄'], ['frozen-player', '🥶'], ['bar-lock', '🔒'], ['penalty', '🎯'], ['transfer', '🔀'],
  ['weak-hand', '🫲'], ['no-spin', '🚫'], ['long-shots', '🎯'], ['one-hand', '☝'],
];
// Animales de las visitas: postura de la app (ardilla y gato) y el emoji del caracol (la app aún no tiene su dibujo).
const ANIMALS = [
  ['ardilla', 'ardilla-pose-burla.webp'], ['gato', 'gato-pose-vomito.webp'], ['caracol', '🐌'],
];

mkdirSync(OUT, { recursive: true });
const manifest = existsSync(MANIFEST) ? JSON.parse(readFileSync(MANIFEST, 'utf8')) : {};
const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 400, height: 400 } });

async function shot(name, html, size, scale = 0.74) {
  await page.setContent(`<html><body style="margin:0;background:transparent">
    <div id="x" style="width:${size}px;height:${size}px;display:flex;align-items:center;justify-content:center;
      font: 900 ${Math.round(size * scale)}px 'Segoe UI Emoji','Noto Color Emoji',sans-serif;color:#fff;
      white-space:nowrap">${html}</div></body></html>`);
  await page.waitForTimeout(80);
  const file = join(OUT, `${name}.png`);
  await page.locator('#x').screenshot({ path: file, omitBackground: true });
  manifest[name] = { x: 0, y: 0, w: size, h: size, alfa: true };
  console.log(`ok  ${name}`);
}

for (const [k, ch] of ICONS) {
  const scale = [...ch].length > 1 ? 0.44 : 0.74;  // «✖2» y «✖3» son dos signos
  await shot(`hc-${k}`, ch, 96, scale);      // tarjeta del hándicap
  await shot(`hc-${k}-p`, ch, 44, scale);    // botón del hándicap en el partido
}
for (const [name, src] of ANIMALS) {
  // Las imágenes van dentro de la página (el navegador no abre ficheros del disco desde una página en blanco).
  const html = src.endsWith('.webp')
    ? `<img src="data:image/webp;base64,${readFileSync(join(ROOT, 'src', 'assets', 'images', src)).toString('base64')}" style="max-width:100%;max-height:100%">`
    : src;
  await shot(`animal-${name}`, html, 140);
}
await browser.close();
writeFileSync(MANIFEST, JSON.stringify(manifest, null, 2));
console.log(`${MANIFEST} actualizado`);
