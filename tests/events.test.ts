import { describe, expect, it } from 'vitest';
import type { StoredMatch, Tournament } from '../src/services/persistence';
import {
  activeEvents,
  BUNDLED_CATALOG,
  computeRewards,
  cuandoTexto,
  ownedBy,
  unlockedByMatch,
  type Catalog,
  type CatalogCollection,
  type CatalogItem,
  type Obtencion,
} from '../src/services/events';
import { makeMatch } from './helpers';
import { eventTabName } from '../src/app/catalogIcons';

const at = (iso: string) => new Date(`${iso}T12:00:00`).getTime();
const halloween = { modo: 'anual' as const, desdeDia: '10-31', hastaDia: '10-31' };

const col = (id: string, nombre: string, tipo: CatalogCollection['tipo'], obtencion: Obtencion, extra: Partial<CatalogCollection> = {}): CatalogCollection => ({
  id, tipo, nombre, descripcion: '', rareza: 'epica', estado: 'publicada', ambito: 'coleccion',
  obtencion, disponibilidad: halloween, torneo: 'league', orden: 0, creada: 0, ...extra,
});
const item = (id: string, coleccionId: string): CatalogItem => ({ id, coleccionId, nombre: id, orden: 0, activo: true });

const catalog: Catalog = {
  collections: [
    col('c-escudo', 'Halloween 2026 · Escudo', 'escudo', { tipo: 'entrar' }),
    col('c-banner', 'Halloween 2026 · Banner', 'fondo', { tipo: 'jugarPartido', modo: 'cualquiera' }),
    col('c-copa', 'Halloween 2026 · Copa', 'copa', { tipo: 'ganarPartido', modo: 'quick' }),
    col('c-liga', 'Halloween 2026 · Liga', 'copa', { tipo: 'ganarTorneo', formato: 'league' }),
    col('c-borrador', 'Borrador', 'copa', { tipo: 'entrar' }, { estado: 'borrador' }),
  ],
  items: [item('escudo', 'c-escudo'), item('banner', 'c-banner'), item('copa', 'c-copa'), item('copaLiga', 'c-liga'), item('nada', 'c-borrador')],
};

const match = (iso: string, goals: string, extra: Partial<StoredMatch> = {}) => ({
  ...makeMatch({ white: ['ana'], blue: ['beto'], goals, at: at(iso), mode: 'quick' }),
  ...extra,
});

describe('Eventos · premios', () => {
  it('entrar en la app el día del evento da la pieza al aparato; otro día, no', () => {
    expect(computeRewards(catalog, { matches: [], tournaments: [], visitDays: ['2026-10-30'] }).device.has('escudo')).toBe(false);
    expect(computeRewards(catalog, { matches: [], tournaments: [], visitDays: ['2026-10-31'] }).device.has('escudo')).toBe(true);
  });
  it('jugar ese día da el banner a los dos; ganar un rápido da la copa solo al ganador', () => {
    const r = computeRewards(catalog, { matches: [match('2026-10-31', 'WWB')], tournaments: [] });
    expect(ownedBy(r, 'banner', ['ana']) && ownedBy(r, 'banner', ['beto'])).toBe(true);
    expect(ownedBy(r, 'copa', ['ana'])).toBe(true);
    expect(ownedBy(r, 'copa', ['beto'])).toBe(false);
  });
  it('fuera de fechas o en otro tipo de partido no cuenta', () => {
    const r = computeRewards(catalog, { matches: [match('2026-11-01', 'WW')], tournaments: [] });
    expect(r.byPlayer.size).toBe(0);
    const liga = match('2026-10-31', 'WW', { tournament: { id: 't1', fixtureId: 'f' } });
    const t = { id: 't1', format: 'league', status: 'active', teams: [], fixtures: [] } as unknown as Tournament;
    const r2 = computeRewards(catalog, { matches: [liga], tournaments: [t] });
    expect(ownedBy(r2, 'copa', ['ana'])).toBe(false); // la copa pide un rápido suelto
    expect(ownedBy(r2, 'banner', ['ana'])).toBe(true);
  });
  it('ganar una liga ese día da la copa de liga a sus campeones', () => {
    const t = {
      id: 't1', format: 'league', status: 'finished', finishedAt: at('2026-10-31'), winnerTeamId: 'x',
      teams: [{ id: 'x', name: 'Ana', playerIds: ['ana'] }], fixtures: [],
    } as unknown as Tournament;
    expect(ownedBy(computeRewards(catalog, { matches: [], tournaments: [t] }), 'copaLiga', ['ana'])).toBe(true);
  });
  it('cada año vuelve: Halloween 2027 también cuenta', () => {
    expect(ownedBy(computeRewards(catalog, { matches: [match('2027-10-31', 'BB')], tournaments: [] }), 'copa', ['beto'])).toBe(true);
  });
  it('avisa de lo que se desbloquea con un partido concreto', () => {
    const m = match('2026-10-31', 'WW');
    const got = unlockedByMatch(catalog, { matches: [m], tournaments: [] }, m.id);
    expect(got).toEqual(expect.arrayContaining([{ playerId: 'ana', itemId: 'copa' }, { playerId: 'beto', itemId: 'banner' }]));
  });
  it('el aviso del evento solo sale en sus fechas y sin borradores', () => {
    expect(activeEvents(catalog, at('2026-10-30'))).toEqual([]);
    const ev = activeEvents(catalog, at('2026-10-31'));
    expect(ev).toHaveLength(1);
    expect(ev[0].title).toBe('Halloween 2026');
    expect(ev[0].rewards.map((r) => r.item.id)).toEqual(['escudo', 'banner', 'copa', 'copaLiga']);
  });
  it('dice cuándo en corto', () => {
    expect(cuandoTexto({ modo: 'anual', desdeDia: '10-31', hastaDia: '10-31' })).toBe('cada año el 31 oct');
    expect(cuandoTexto({ modo: 'anual', desdeDia: '12-20', hastaDia: '01-06' })).toBe('cada año del 20 dic al 6 ene');
    expect(cuandoTexto({ modo: 'siempre' })).toBe('siempre');
  });
  it('Halloween 2026 viene en la app: escudo al entrar ese día, un juego por tipo de torneo', () => {
    const r = computeRewards(BUNDLED_CATALOG, { matches: [], tournaments: [], visitDays: ['2026-10-31'] });
    expect(r.device.has('app-halloween-2026-escudo-1')).toBe(true);
    expect(new Set(BUNDLED_CATALOG.collections.map((c) => c.torneo))).toEqual(new Set(['league', 'bracket', 'pool']));
    expect(r.device.has('app-halloween-2026-copa-escudo-1')).toBe(true);
    expect(computeRewards(BUNDLED_CATALOG, { matches: [], tournaments: [], visitDays: ['2026-11-01'] }).device.size).toBe(0);
  });
  it('la pestaña del muestrario es el evento sin el año', () => {
    expect(eventTabName('Halloween 2026 · Copa')).toBe('Halloween');
    expect(eventTabName('Navidad 2027')).toBe('Navidad');
    expect(eventTabName('Retro')).toBe('Retro');
  });
});

