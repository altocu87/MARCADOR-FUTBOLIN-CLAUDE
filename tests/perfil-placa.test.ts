/**
 * El perfil del QR que publica la pantalla de 7" (hardware/esp32s3_pantalla7/s3_perfil.h) = el de la app.
 * Los datos los deja hardware/test/s3_opciones_test.cpp (40 partidos de prueba jugados con el motor de la placa);
 * check_copies.sh lo ejecuta justo después con MFV3_PERFIL_PLACA=1 y la hora de España. Con los torneos de
 * demostración (carpeta torneos/): el palmarés y el torneo de cada partido también tienen que coincidir.
 */
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { expect, it } from 'vitest';
import { buildProfileSnapshot } from '../src/services/profiles/snapshot';

it.skipIf(!process.env.MFV3_PERFIL_PLACA)('el perfil de la placa coincide con el de la app', () => {
  const dir = 'build/perfil-demo';
  const board = JSON.parse(readFileSync(`${dir}/perfil.json`, 'utf8'));
  const players = JSON.parse(readFileSync(`${dir}/jugadores.json`, 'utf8')).map((p: { id: string; name: string }) => ({ ...p, active: true, createdAt: 0, updatedAt: 0 }));
  const matches = readdirSync(dir).filter((f) => f.endsWith('.json') && f.length > 30).map((f) => JSON.parse(readFileSync(`${dir}/${f}`, 'utf8')));
  const tdir = `${dir}/torneos`;
  const tournaments = existsSync(tdir) ? readdirSync(tdir).filter((f) => f.endsWith('.json')).map((f) => JSON.parse(readFileSync(`${tdir}/${f}`, 'utf8'))) : [];
  const me = players[0];
  const app = buildProfileSnapshot({ player: me, players, matches, tournaments, now: board.generatedAt });
  for (const snapshot of [app, board]) for (const rows of [snapshot.rivals, snapshot.teammates]) {
    for (let i = 1; i < rows.length; i++) expect(rows[i - 1].played).toBeGreaterThanOrEqual(rows[i].played);
  }
  // C/C++ qsort no es estable al empatar. Comparar todas las filas con desempate por id,
  // conservando la comprobación anterior del orden real por partidos jugados.
  const equalRank = (rows: { id: string; played: number }[]) => [...rows].sort((a, b) => b.played - a.played || a.id.localeCompare(b.id));
  const pick = (s: any) => ({ palmares: s.palmares, stats: s.stats, sides: s.sides, habits: s.habits, rivals: equalRank(s.rivals), teammates: equalRank(s.teammates), favoriteRival: s.favoriteRival, nemesis: s.nemesis, matches: s.matches, people: Object.keys(s.people).sort() });
  const a = pick(app), b = pick(board);
  for (const k of Object.keys(a) as (keyof typeof a)[]) expect(b[k], k).toEqual(a[k]);
  expect(matches.filter((m) => !m.tournament).length).toBe(40);
});
