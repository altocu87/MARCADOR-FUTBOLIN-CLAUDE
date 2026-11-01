import { describe, expect, it } from 'vitest';
import { createPlayer, initialGenders, updatePlayer } from '../src/services/players';

describe('chico o chica de cada jugador', () => {
  const p = (name: string) => createPlayer({ name }, 1);

  it('se pone una vez a los que ya había, sin tildes ni mayúsculas', () => {
    const list = [p('Vicky'), p('Victoria'), p('Lucía'), p('MARIA'), p('Alex'), p('José'), p('Carlos')];
    const got = initialGenders(list, new Set(), 5);
    expect(got.map((x) => [x.name, x.gender])).toEqual([
      ['Vicky', 'f'],
      ['Victoria', 'f'],
      ['Lucía', 'f'],
      ['MARIA', 'f'],
      ['Alex', 'm'],
      ['José', 'm'],
    ]);
    expect(got.every((x) => x.updatedAt === 5)).toBe(true);
  });

  it('respeta lo ya puesto y no vuelve a ponerlo si se quitó', () => {
    const vicky = { ...p('Vicky'), gender: 'm' as const };
    const alex = p('Alex');
    expect(initialGenders([vicky, alex], new Set([alex.id]), 5)).toEqual([]);
  });

  it('la ficha lo guarda y lo puede quitar', () => {
    const a = createPlayer({ name: 'Ana', gender: 'f' }, 1);
    expect(a.gender).toBe('f');
    expect(updatePlayer(a, { name: 'Ana' }, 2).gender).toBeUndefined();
  });
});
