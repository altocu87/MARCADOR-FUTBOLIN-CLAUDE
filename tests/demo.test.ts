import { describe, expect, it } from 'vitest';
import { DEFAULT_PROGRESSION, DEMO_NAMESPACE, KEYS, createLocalRepositories, createMemoryStore } from '../src/services/persistence';
import { computeProgression } from '../src/services/progression';
import { generateDemoData } from '../src/services/demo';

describe('datos de prueba', () => {
  const now = new Date('2026-10-03T12:00:00').getTime();
  const data = generateDemoData(now);

  it('genera jugadores, más de 100 partidos y torneos', () => {
    expect(data.players.length).toBeGreaterThanOrEqual(10);
    expect(data.matches.length).toBeGreaterThanOrEqual(100);
    expect(data.tournaments.filter((t) => t.status === 'finished').length).toBe(4);
    expect(data.tournaments.filter((t) => t.status === 'active').length).toBe(1);
  });

  it('los partidos son coherentes y anteriores a hoy', () => {
    for (const m of data.matches) {
      expect(m.finishedAt).toBeLessThan(now);
      if (m.result.reason !== 'penalties') expect(m.result.score[m.result.winner]).toBeGreaterThan(m.result.score[m.result.winner === 'white' ? 'blue' : 'white']);
      const ids = new Set(data.players.map((p) => p.id));
      for (const p of m.participants) expect(ids.has(p.playerId)).toBe(true);
    }
    expect(data.matches.some((m) => m.result.reason === 'penalties')).toBe(true);
    expect(data.matches.some((m) => m.config.mode === 'chaos')).toBe(true);
    expect(data.matches.some((m) => m.participants.length === 4)).toBe(true);
  });

  it('la progresión se calcula sin errores', () => {
    const snap = computeProgression(data.players.map((p) => p.id), data.matches, DEFAULT_PROGRESSION, { tournaments: data.tournaments, challenges: true });
    expect(snap).toBeTruthy();
  });

  it('el espacio de prueba no toca los datos reales', async () => {
    const store = createMemoryStore();
    const real = createLocalRepositories(store);
    const demo = createLocalRepositories(store, DEMO_NAMESPACE);
    await real.players.saveAll([{ ...data.players[0], name: 'Real' }]);
    await demo.players.saveAll(data.players);
    await demo.matches.saveAll(data.matches);
    await demo.wipe();
    expect(await demo.players.list()).toEqual([]);
    expect((await real.players.list())[0].name).toBe('Real');
    expect(store.data.has(KEYS.players)).toBe(true);
  });
});
