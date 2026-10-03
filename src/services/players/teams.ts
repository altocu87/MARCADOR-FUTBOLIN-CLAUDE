/** Sorteo de equipos: equilibrado por ELO o aleatorio, para 2v2, 3v3 o 4v4. */

export interface TeamSplit {
  white: string[];
  blue: string[];
  /** Diferencia absoluta entre las medias de ELO. */
  diff: number;
}

function check(ids: string[]) {
  if (ids.length < 4 || ids.length > 8 || ids.length % 2 !== 0) {
    throw new Error('Se necesitan 4, 6 u 8 jugadores.');
  }
}

/** Prueba todos los repartos posibles (como mucho 35 con 8) y devuelve el más igualado. */
export function balancedTeams(ids: string[], elo: (id: string) => number): TeamSplit {
  check(ids);
  const size = ids.length / 2;
  const avg = (team: string[]) => team.reduce((s, id) => s + elo(id), 0) / team.length;
  let best: TeamSplit | null = null;
  // El primer jugador siempre va al blanco: así no se repite cada reparto con los colores cambiados.
  const walk = (start: number, white: string[]) => {
    if (white.length === size) {
      const blue = ids.filter((id) => !white.includes(id));
      const diff = Math.abs(avg(white) - avg(blue));
      if (!best || diff < best.diff) best = { white: [...white], blue, diff };
      return;
    }
    for (let i = start; i < ids.length; i += 1) walk(i + 1, [...white, ids[i]]);
  };
  walk(1, [ids[0]]);
  return best!;
}

export function randomTeams(ids: string[], rnd: () => number = Math.random): TeamSplit {
  check(ids);
  const shuffled = [...ids];
  for (let i = shuffled.length - 1; i > 0; i -= 1) {
    const j = Math.floor(rnd() * (i + 1));
    [shuffled[i], shuffled[j]] = [shuffled[j], shuffled[i]];
  }
  const size = ids.length / 2;
  return { white: shuffled.slice(0, size), blue: shuffled.slice(size), diff: 0 };
}
