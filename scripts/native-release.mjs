// Registro único de las versiones de la pantalla nativa. Sin dependencias externas.
import { readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';

const source = new URL('../hardware/esp32s3_pantalla7/releases.json', import.meta.url);
const header = new URL('../hardware/esp32s3_pantalla7/s3_release.h', import.meta.url);
export function compareVersions(a, b) {
  const x = a.split('.').map(Number), y = b.split('.').map(Number);
  for (let i = 0; i < 3; i++) if ((x[i] ?? 0) !== (y[i] ?? 0)) return (x[i] ?? 0) - (y[i] ?? 0);
  return 0;
}
export function validateRelease(data) {
  if (!data || !Array.isArray(data.releases) || !data.releases.length) throw Error('Falta el historial de versiones');
  const versions = new Set();
  for (const [i, r] of data.releases.entries()) {
    if (!/^\d{1,3}\.\d{1,3}(?:\.\d{1,3})?$/.test(r.version)) throw Error('Versión no válida');
    if (Number(r.version.split('.')[0]) >= 1 && data.majorOneAuthorized !== true) throw Error('El dueño aún no ha autorizado pasar a 1.0');
    if (versions.has(r.version) || (i && compareVersions(data.releases[i - 1].version, r.version) <= 0)) throw Error('El historial debe ir de más nuevo a más antiguo, sin repetir versión');
    versions.add(r.version);
    if (!/^\d{4}-\d{2}-\d{2}$/.test(r.date) || Number.isNaN(Date.parse(r.date)) || new Date(r.date).toISOString().slice(0, 10) !== r.date) throw Error('Falta la fecha');
    if (typeof r.title !== 'string' || !r.title.trim() || r.title.length > 65) throw Error('Falta un título breve');
    if (!Array.isArray(r.changes) || !r.changes.length || r.changes.some(c => typeof c !== 'string' || !c.trim() || c.length > 140)) throw Error('Faltan cambios breves y legibles');
    if (Buffer.byteLength(notesFor(r), 'utf8') >= 1024) throw Error('Las notas de una versión no caben en la pantalla');
  }
  if (data.version !== data.releases[0].version) throw Error('La versión actual no coincide con la primera entrada');
  return data;
}
export function notesFor(r) { return `${r.title}\n\n${r.changes.map(c => `• ${c}`).join('\n\n')}`; }
export function loadRelease() { return validateRelease(JSON.parse(readFileSync(source, 'utf8'))); }
export function generateHeader(data) {
  validateRelease(data);
  const q = JSON.stringify;
  return `// Generado por scripts/native-release.mjs. Editar releases.json, no este archivo.\n#ifndef MFV3_S3_RELEASE_H\n#define MFV3_S3_RELEASE_H\n#include <stddef.h>\nnamespace nativeRelease {\nstatic const char VERSION[] = ${q(data.version)};\nstruct Entry { const char* version; const char* date; const char* notes; };\nstatic const Entry HISTORY[] = {\n${data.releases.map(r => `  {${q(r.version)}, ${q(r.date)}, ${q(notesFor(r))}},`).join('\n')}\n};\nstatic const size_t COUNT = sizeof(HISTORY) / sizeof(HISTORY[0]);\n}\n#endif\n`;
}
export function assertNewRelease(data, previous, incoming) {
  validateRelease(data);
  // Reintentar una subida interrumpida del MISMO artefacto no crea otra actualización.
  if (incoming && previous.version === data.version && incoming.version === data.version &&
      previous.notes === notesFor(data.releases[0]) && incoming.notes === previous.notes &&
      previous.build === incoming.build && previous.size === incoming.size && previous.md5 === incoming.md5 &&
      Number.isInteger(incoming.build) && incoming.build > 0 && incoming.size > 0 && /^[a-f0-9]{32}$/.test(incoming.md5)) return;
  if (previous.version && compareVersions(data.version, previous.version) <= 0) throw Error(`Ya está publicada ${previous.version}: sube la versión y añade sus novedades`);
  if (previous.version) {
    const old = data.releases.find(r => r.version === previous.version);
    if (!old || notesFor(old) !== previous.notes) throw Error('Conserva las novedades de la versión ya publicada');
  } else if (!Number.isInteger(previous.build) || previous.build <= 0 || !previous.size || !/^[a-fA-F0-9]{32}$/.test(previous.md5)) {
    throw Error('No se pudo verificar el manifiesto anterior; no publicar a ciegas');
  }
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const [command = 'check', ...args] = process.argv.slice(2);
  const data = loadRelease();
  if (command === 'generate') writeFileSync(header, generateHeader(data));
  else if (command === 'check') {
    if (readFileSync(header, 'utf8') !== generateHeader(data)) throw Error('Regenerar s3_release.h: node scripts/native-release.mjs generate');
  } else if (command === 'manifest') {
    const [binPath, build, outPath] = args;
    if (!binPath || !outPath || !/^\d+$/.test(build) || Number(build) <= 0 || Number(build) > 0xffffffff) throw Error('Se necesita binario, build positivo y destino');
    const bin = readFileSync(binPath);
    if (!bin.length) throw Error('Binario vacío');
    writeFileSync(outPath, JSON.stringify({ build: Number(build), size: bin.length, md5: createHash('md5').update(bin).digest('hex'), version: data.version, notes: notesFor(data.releases[0]) }) + '\n');
  } else if (command === 'check-published') {
    // Lee el archivo de la release real, no la copia de Pages (puede tardar en actualizarse).
    assertNewRelease(data, JSON.parse(readFileSync(args[0], 'utf8')), args[1] ? JSON.parse(readFileSync(args[1], 'utf8')) : undefined);
  } else throw Error(`Comando desconocido: ${command}`);
  console.log(`Pantalla nativa ${data.version}: ${command} correcto`);
}
