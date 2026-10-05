/**
 * Pool: parejas rotativas con puntos individuales (estilo «DYP»).
 * Cada partido es un 2 contra 2 con parejas nuevas; si sobran jugadores, descansan por turnos.
 * Todos juegan el mismo número de partidos: por eso ese número se ajusta al de jugadores
 * (con 5 jugadores, por ejemplo, cada uno juega 4 y descansa 1).
 */

export const POOL_MIN_PLAYERS = 4;
export const POOL_MAX_PLAYERS = 16;
/** Límite de partidos de un Pool para que quepa en una tarde. */
export const POOL_MAX_MATCHES = 40;
/** Máximo de partidos por jugador. */
export const POOL_MAX_GAMES = 8;

/** Partidos por jugador posibles con `n` jugadores (cada partido ocupa 4 plazas). */
export function poolGameOptions(n: number): number[] {
  const out: number[] = [];
  for (let g = 1; g <= POOL_MAX_GAMES; g += 1) if ((n * g) % 4 === 0 && (n * g) / 4 <= POOL_MAX_MATCHES) out.push(g);
  return out;
}

/** Ajusta los partidos por jugador deseados al valor válido más cercano (hacia arriba si se puede). */
export function effectivePoolGames(n: number, desired: number): number {
  const opts = poolGameOptions(n);
  if (!opts.length) return 0;
  return opts.find((g) => g >= desired) ?? opts[opts.length - 1];
}

export interface PoolRound {
  /** Partidos de la ronda: [pareja blanca, pareja azul]. */
  matches: [string[], string[]][];
  resting: string[];
}

const pairKey = (a: string, b: string) => (a < b ? `${a}|${b}` : `${b}|${a}`);

/**
 * Calendario del Pool. Prueba muchos sorteos y se queda con el que menos repite
 * compañero (lo más importante), rival y, si se pide, el más equilibrado por ELO.
 */
export function schedulePool(
  playerIds: string[],
  gamesPerPlayer: number,
  pairing: 'elo' | 'random',
  eloOf: (id: string) => number,
  rnd: () => number = Math.random,
  attempts = 250,
): PoolRound[] {
  const n = playerIds.length;
  const total = (n * gamesPerPlayer) / 4;
  if (n < POOL_MIN_PLAYERS || !Number.isInteger(total) || total < 1) return [];
  const perRound = Math.floor(n / 4);
  let best: { cost: number; rounds: PoolRound[] } | null = null;

  for (let a = 0; a < attempts; a += 1) {
    const played = new Map(playerIds.map((id) => [id, 0]));
    const last = new Map(playerIds.map((id) => [id, -1]));
    const partners = new Map<string, number>();
    const rivals = new Map<string, number>();
    const rounds: PoolRound[] = [];
    let cost = 0;
    let k = 0;
    while (k < total) {
      const round: PoolRound = { matches: [], resting: [] };
      const inRound = new Set<string>();
      for (let m = 0; m < perRound && k < total; m += 1, k += 1) {
        const tie = new Map(playerIds.map((id) => [id, rnd()]));
        // Juegan los que menos llevan; a igualdad, los que más tiempo llevan sin jugar.
        const four = [...playerIds]
          .sort(
            (x, y) =>
              played.get(x)! - played.get(y)! ||
              Number(inRound.has(x)) - Number(inRound.has(y)) ||
              last.get(x)! - last.get(y)! ||
              tie.get(x)! - tie.get(y)!,
          )
          .slice(0, 4);
        // Tres formas de partir 4 jugadores en 2 parejas: la de menor coste.
        const splits: [string[], string[]][] = [
          [[four[0], four[1]], [four[2], four[3]]],
          [[four[0], four[2]], [four[1], four[3]]],
          [[four[0], four[3]], [four[1], four[2]]],
        ];
        const splitCost = ([w, b]: [string[], string[]]) => {
          let c = 100 * ((partners.get(pairKey(w[0], w[1])) ?? 0) + (partners.get(pairKey(b[0], b[1])) ?? 0));
          for (const x of w) for (const y of b) c += 10 * (rivals.get(pairKey(x, y)) ?? 0);
          if (pairing === 'elo') c += Math.abs(eloOf(w[0]) + eloOf(w[1]) - eloOf(b[0]) - eloOf(b[1])) / 25;
          return c + rnd() * 0.01;
        };
        const [w, b] = splits.reduce((p, q) => (splitCost(q) < splitCost(p) ? q : p));
        cost += splitCost([w, b]);
        partners.set(pairKey(w[0], w[1]), (partners.get(pairKey(w[0], w[1])) ?? 0) + 1);
        partners.set(pairKey(b[0], b[1]), (partners.get(pairKey(b[0], b[1])) ?? 0) + 1);
        for (const x of w) for (const y of b) rivals.set(pairKey(x, y), (rivals.get(pairKey(x, y)) ?? 0) + 1);
        for (const id of [...w, ...b]) {
          // Penaliza jugar dos rondas seguidas si había alguien descansando.
          if (perRound === 1 && last.get(id) === k - 1) cost += 3;
          played.set(id, played.get(id)! + 1);
          last.set(id, k);
          inRound.add(id);
        }
        // Alterna quién sale de blanco para repartir los lados.
        round.matches.push(k % 2 === 0 ? [w, b] : [b, w]);
      }
      round.resting = playerIds.filter((id) => !inRound.has(id));
      rounds.push(round);
    }
    if (!best || cost < best.cost) best = { cost, rounds };
  }
  return best!.rounds;
}
