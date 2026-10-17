// Los 12 jugadores del modo prueba de la app, con su «Nv · ELO», título y foto, en las órdenes USB que entiende la
// pantalla de 7" (s3_jugadores.h): build/placa/jugadores-demo.txt. Sirve para las capturas del PC y para cargarlos
// en la placa real (python scripts/placa.py jugadores). Necesita `npm run dev`.
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { openApp } from './capturas-app.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const OUT = join(ROOT, 'build', 'placa', 'jugadores-demo.txt');

const browser = await chromium.launch();
const { page } = await openApp(browser, { demo: true });
const lines = await page.evaluate(async () => {
  const bp = await import('/src/inputs/hardware/boardPlayers.ts');
  const prog = await import('/src/services/progression/index.ts');
  const pl = await import('/src/services/players/index.ts');
  const defs = await import('/src/services/persistence/defaults.ts');
  const rules = await import('/src/services/progression/gameRules.ts');
  const get = (k) => JSON.parse(localStorage.getItem(`mfv3:v1:demo:${k}`) ?? '[]');
  const players = pl.sortPlayers(get('players').filter((p) => p.active));
  const snap = prog.computeProgression(players.map((p) => p.id), get('matches'), { ...defs.DEFAULT_PREFERENCES.progression, ...rules.eloSettings(rules.DEFAULT_RULES), enabled: true },
    { tournaments: get('tournaments') });
  const list = [];
  for (const p of players) {
    const g = snap.players.get(p.id);
    const meta = g ? `Nv ${g.level}${g.rankedPlayed ? ` · ELO ${g.elo}` : ''}` : '';
    const photo = p.photo ? await bp.photoForBoard(p.photo) : null;
    list.push(bp.boardPlayer(p, meta, prog.displayTitle(g, p.titleId), photo));
  }
  // Mismo diálogo que syncPlayers(), sin esperar respuestas: lista y fotos.
  const out = [`JUGADORES_INICIO ${list.length}`, ...list.map((p) => `JUGADOR ${bp.playerLine(p)}`), `JUGADORES_FIN ${bp.playersHash(list)}`];
  const { crc32 } = await import('/src/inputs/hardware/boardSounds.ts');
  for (const p of list) {
    if (!p.photo) continue;
    out.push(`FOTO_SUBIR ${p.code} ${p.photo.length}`);
    for (let i = 0; i < p.photo.length; i += 192) {
      let bin = '';
      for (const b of p.photo.subarray(i, i + 192)) bin += String.fromCharCode(b);
      out.push(`FOTO_DATOS ${btoa(bin)}`);
    }
    out.push(`FOTO_FIN ${crc32(p.photo).toString(16).padStart(8, '0')}`);
  }
  return out;
});
await browser.close();
mkdirSync(dirname(OUT), { recursive: true });
writeFileSync(OUT, `${lines.join('\n')}\n`);
console.log(`${OUT}: ${lines.length} líneas`);
