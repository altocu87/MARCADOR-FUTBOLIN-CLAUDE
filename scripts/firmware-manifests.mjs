// Crea los manifest.json del instalador web (ESP Web Tools) para cada placa ESP32 del catálogo.
//   node scripts/firmware-manifests.mjs <carpeta> [id]
// Cada placa queda en <carpeta>/<id>/manifest.json y espera su programa en <carpeta>/<id>/<id>.bin
// (el binario «merged» que genera la compilación automática de GitHub).
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const [outDir = 'web/public/firmware', only] = process.argv.slice(2);
const catalog = JSON.parse(readFileSync(new URL('../src/inputs/hardware/diy-catalog.json', import.meta.url), 'utf8'));
const version = process.env.FW_VERSION || 'desarrollo';
const nativeRelease = JSON.parse(readFileSync(new URL('../hardware/esp32s3_pantalla7/releases.json', import.meta.url), 'utf8'));
let n = 0;
for (const b of catalog.boards) {
  if (!b.build?.chipFamily || (only && b.id !== only)) continue;
  const dir = join(outDir, b.id);
  mkdirSync(dir, { recursive: true });
  const manifest = {
    name: `Marcador Futbolín V3 · ${b.name}`,
    version: b.sketch === 'esp32s3_pantalla7' ? nativeRelease.version : version,
    new_install_prompt_erase: true,
    builds: [{ chipFamily: b.build.chipFamily, parts: [{ path: `${b.id}.bin`, offset: 0 }] }],
  };
  writeFileSync(join(dir, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
  n++;
}
console.log(`${n} manifiestos en ${outDir}`);
