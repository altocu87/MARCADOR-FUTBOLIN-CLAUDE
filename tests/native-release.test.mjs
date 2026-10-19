import { describe, it, expect } from 'vitest';
import { readFileSync, mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { loadRelease, validateRelease, generateHeader, notesFor, assertNewRelease, compareVersions } from '../scripts/native-release.mjs';

const fixture = () => ({ version: '0.1', majorOneAuthorized: false, releases: [{ version: '0.1', date: '2026-10-19', title: 'Prueba', changes: ['Un cambio comprensible.'] }] });

describe('versiones de la pantalla nativa', () => {
  it('el firmware y el historial corresponden al registro unico', () => {
    const data = loadRelease();
    expect(readFileSync(new URL('../hardware/esp32s3_pantalla7/s3_release.h', import.meta.url), 'utf8')).toBe(generateHeader(data));
    expect(data.version).toBe(data.releases[0].version);
  });
  it('impide publicar sin subir version y bloquea 1.0 sin autorizacion', () => {
    const data = fixture();
    expect(() => assertNewRelease(data, { version: data.version })).toThrow(/sube la versión/);
    expect(compareVersions('0.10', '0.9')).toBeGreaterThan(0);
    expect(compareVersions('0.1', '0.1.0')).toBe(0);
    data.version = data.releases[0].version = '1.0';
    expect(() => validateRelease(data)).toThrow(/autorizado/);
  });
  it('rechaza novedades vacias, demasiado grandes o historia incoherente', () => {
    const data = fixture();
    data.releases[0].changes = [];
    expect(() => validateRelease(data)).toThrow(/cambios/);
    data.releases[0].changes = Array(20).fill('Mejora la claridad de los botones y explica que esta haciendo la pantalla.');
    expect(() => validateRelease(data)).toThrow(/no caben/);
    data.releases[0].changes = ['Texto breve'];
    data.releases.push(structuredClone(data.releases[0]));
    expect(() => validateRelease(data)).toThrow(/repetir/);
  });
  it('mantiene las notas publicadas y admite el primer manifiesto antiguo sin nombre', () => {
    const data = fixture();
    const old = structuredClone(data.releases[0]);
    data.version = '0.2';
    data.releases.unshift({ ...old, version: '0.2', changes: ['Mejora de prueba'] });
    expect(() => assertNewRelease(data, { version: old.version, notes: notesFor(old) })).not.toThrow();
    data.releases[1].changes = ['Historia alterada'];
    expect(() => assertNewRelease(data, { version: old.version, notes: notesFor(old) })).toThrow(/Conserva/);
    expect(() => assertNewRelease(loadRelease(), { build: 99, size: 2000, md5: 'a'.repeat(32) })).not.toThrow();
    expect(() => assertNewRelease(loadRelease(), {})).toThrow(/no publicar/);
  });
  it('el manifiesto describe el binario real y lleva las novedades de esa version', () => {
    const dir = mkdtempSync(join(tmpdir(), 'native-release-'));
    try {
      const bin = Buffer.from('firmware de prueba');
      writeFileSync(join(dir, 'test.bin'), bin);
      execFileSync(process.execPath, ['scripts/native-release.mjs', 'manifest', join(dir, 'test.bin'), '123', join(dir, 'ota.json')]);
      const result = JSON.parse(readFileSync(join(dir, 'ota.json'), 'utf8'));
      const data = loadRelease();
      expect(result).toEqual({ build: 123, size: bin.length, md5: createHash('md5').update(bin).digest('hex'), version: data.version, notes: notesFor(data.releases[0]) });
    } finally { rmSync(dir, { recursive: true, force: true }); }
  });
  it('permite reintentar el mismo artefacto, pero no publicar otro con la misma version', () => {
    const data = fixture();
    const previous = { version: data.version, notes: notesFor(data.releases[0]), build: 80, size: 123, md5: 'a'.repeat(32) };
    expect(() => assertNewRelease(data, previous, { ...previous })).not.toThrow();
    expect(() => assertNewRelease(data, previous, { ...previous, build: 81 })).toThrow(/sube la versión/);
    expect(() => assertNewRelease(data, previous, { ...previous, md5: 'b'.repeat(32) })).toThrow(/sube la versión/);
  });
});
