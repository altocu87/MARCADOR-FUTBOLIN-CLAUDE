import { describe, expect, it } from 'vitest';
import { clubNameTaken, clubOfSide, findClub, mergeClubPlayer, saveClub } from '../src/services/clubs';
import { competitionKey, nextEdition, lastEdition } from '../src/services/tournaments';
import type { Tournament } from '../src/services/persistence';

describe('Equipos guardados', () => {
  it('se reconocen por la pareja, en cualquier orden y en cualquier lado', () => {
    const { clubs, club } = saveClub([], ['b', 'a'], ' Los Halcones ', 'emoji:🦅', 1);
    expect(club.name).toBe('Los Halcones');
    expect(findClub(clubs, ['a', 'b'])?.id).toBe(club.id);
    expect(findClub(clubs, ['a'])).toBeUndefined();
    const parts = [
      { playerId: 'a', team: 'blue' as const },
      { playerId: 'b', team: 'blue' as const },
      { playerId: 'c', team: 'white' as const },
      { playerId: 'd', team: 'white' as const },
    ];
    expect(clubOfSide(clubs, parts, 'blue')?.name).toBe('Los Halcones');
    expect(clubOfSide(clubs, parts, 'white')).toBeUndefined();
  });
  it('guardar otra vez la misma pareja la actualiza y no deja nombres repetidos', () => {
    let { clubs } = saveClub([], ['a', 'b'], 'Halcones', 'emoji:🦅', 1);
    clubs = saveClub(clubs, ['a', 'b'], 'Águilas', 'emoji:🐺', 2).clubs;
    expect(clubs).toHaveLength(1);
    expect(clubs[0]).toMatchObject({ name: 'Águilas', logo: 'emoji:🐺' });
    expect(clubNameTaken(clubs, 'águilas', ['c', 'd'])).toBe(true);
    expect(clubNameTaken(clubs, 'águilas', ['a', 'b'])).toBe(false);
  });
  it('al fusionar jugadores el equipo pasa al que se queda', () => {
    const { clubs } = saveClub([], ['guest', 'b'], 'Halcones', 'emoji:🦅', 1);
    expect(mergeClubPlayer(clubs, 'guest', 'a')[0].playerIds).toEqual(['a', 'b']);
  });
});

describe('Nombre propio de la competición', () => {
  const t = (over: Partial<Tournament>): Tournament =>
    ({ id: Math.random().toString(), name: 'x', status: 'finished', createdAt: 1, fixtures: [], teams: [], format: 'league', teamSize: 1, ranked: false, config: {}, formatVersion: 1, ...over }) as unknown as Tournament;
  it('con nombre propio, las ediciones van por el nombre (sin mayúsculas ni espacios de más)', () => {
    const a = t({ competition: 'Copa del Almacén', templateId: 'builtin-pool', logo: 'emoji:🐺', createdAt: 1 });
    const b = t({ competition: 'copa  del almacén', templateId: 'builtin-league', createdAt: 2 });
    expect(competitionKey(a)).toBe(competitionKey(b));
    expect(nextEdition(competitionKey(a), [a, b])).toBe(3);
    expect(lastEdition(competitionKey(a), [a, b])?.createdAt).toBe(2);
    // Sin nombre propio sigue siendo la del tipo de torneo.
    expect(competitionKey(t({ templateId: 'builtin-pool' }))).toBe('builtin-pool');
  });
});
