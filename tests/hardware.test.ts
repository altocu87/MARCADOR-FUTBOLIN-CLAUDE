import { describe, expect, it } from 'vitest';
import { LineSplitter, clockLine, outgoingLines, parseBoardLine } from '../src/inputs/hardware/protocol';
import { DEFAULT_CONFIG, createMatch, dispatch } from '../src/match-engine';
import { computeStage } from '../src/ui/layout/Stage';

describe('Protocolo MFV3', () => {
  it('interpreta órdenes, alias y origen', () => {
    expect(parseBoardLine('GOL_BLANCO')).toEqual({ kind: 'command', command: 'GOL_BLANCO', source: 'button' });
    expect(parseBoardLine('gol_azul sensor\r')).toEqual({ kind: 'command', command: 'GOL_AZUL', source: 'sensor' });
    expect(parseBoardLine('GB')).toMatchObject({ command: 'GOL_BLANCO' });
    expect(parseBoardLine('PAUSA')).toMatchObject({ command: 'PAUSA' });
    expect(parseBoardLine('ANULAR_BLANCO')).toMatchObject({ command: 'ANULAR_BLANCO' });
    expect(parseBoardLine('aa')).toMatchObject({ command: 'ANULAR_AZUL' });
    expect(parseBoardLine('HELLO ESP32-C3 1.0')).toEqual({ kind: 'hello', name: 'ESP32-C3', version: '1.0', caps: [] });
    expect(parseBoardLine('HELLO esp32c3 1.1 caps=goles,anular,Wifi')).toEqual({
      kind: 'hello',
      name: 'esp32c3',
      version: '1.1',
      caps: ['goles', 'anular', 'wifi'],
    });
    expect(parseBoardLine('PING')).toEqual({ kind: 'ping' });
    expect(parseBoardLine('   ')).toBeNull();
    expect(parseBoardLine('XYZ')).toEqual({ kind: 'unknown', raw: 'XYZ' });
  });
  it('pone en hora la placa con la hora local', () => {
    expect(clockLine(new Date(2026, 9, 7, 8, 5, 3))).toBe('HORA 2026-10-07 08:05:03');
  });

  it('junta datos troceados en líneas', () => {
    const s = new LineSplitter();
    expect(s.push('GOL_BL')).toEqual([]);
    expect(s.push('ANCO\r\nPAU')).toEqual(['GOL_BLANCO']);
    expect(s.push('SA\n\n')).toEqual(['PAUSA']);
    expect(s.push('x'.repeat(600))).toEqual([]);
  });
  it('envía a la placa estado, gol aceptado, bloqueo y marcador', () => {
    const p = [
      { playerId: 'a', team: 'white' as const, slot: 1 as const, nameSnapshot: 'A' },
      { playerId: 'b', team: 'blue' as const, slot: 1 as const, nameSnapshot: 'B' },
    ];
    let s = createMatch('m', { ...DEFAULT_CONFIG }, p, 0);
    expect(outgoingLines(null, s, [])).toEqual(['STATE countdown', 'SCORE 0 0']);
    const started = dispatch(s, { type: 'SKIP_COUNTDOWN' }, 0);
    expect(outgoingLines(s, started.state, started.events)).toEqual(['STATE playing']);
    s = started.state;
    const goal = dispatch(s, { type: 'GOAL', team: 'blue' }, 1000);
    expect(outgoingLines(s, goal.state, goal.events)).toEqual(['GOAL AZUL', 'LOCK 3000', 'SCORE 0 1']);
    // Un gol rechazado por el bloqueo no se anuncia.
    const rejected = dispatch(goal.state, { type: 'GOAL', team: 'white' }, 1500);
    expect(outgoingLines(goal.state, rejected.state, rejected.events)).toEqual([]);
  });
});

describe('Lienzo adaptable', () => {
  it('800×480 exacto en la pantalla de referencia', () => {
    expect(computeStage(800, 480)).toMatchObject({ width: 800, height: 480, scale: 1, portrait: false });
  });
  it('panorámica 16:9 ensancha el lienzo sin franjas', () => {
    const s = computeStage(1920, 1080);
    expect(s.height).toBe(480);
    expect(s.width).toBe(853);
    expect(Math.abs(s.width * s.scale - 1920)).toBeLessThan(2);
  });
  it('4:3 crece en alto', () => {
    const s = computeStage(1024, 768);
    expect(s.width).toBe(800);
    expect(s.height).toBe(600);
  });
  it('vertical mantiene 800×480 escalado y lo marca', () => {
    const s = computeStage(390, 844);
    expect(s).toMatchObject({ width: 800, height: 480, portrait: true });
    expect(s.scale).toBeCloseTo(390 / 800);
  });
  it('girado en vertical: usa la pantalla entera como horizontal', () => {
    const s = computeStage(390, 844, true);
    expect(s.portrait).toBe(true);
    expect(s.height).toBe(480);
    expect(s.width * s.scale).toBeGreaterThan(800); // ocupa el alto del móvil
  });
  it('nunca menor que 800×480 ni mayor que los límites', () => {
    const ultra = computeStage(3440, 1440);
    expect(ultra.width).toBe(1100);
    const square = computeStage(1000, 1000);
    expect(square.height).toBe(640);
  });
});
