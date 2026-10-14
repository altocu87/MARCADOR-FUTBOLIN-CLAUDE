/**
 * Reglas del juego (ELO, rangos, experiencia y niveles) que se configuran en el panel de administración.
 * - El panel las publica (tabla `game_rules` de Supabase) y la app las descarga, como el catálogo.
 * - Sin conexión ni nada publicado, la app usa DEFAULT_RULES (los valores recomendados).
 * - Todo se recalcula desde el historial: cambiar una regla cambia el ELO y la XP de todos, también hacia atrás.
 * RULE_GROUPS lleva, para cada dato, qué significa, el valor recomendado y por qué: lo enseña el panel.
 */
import { RARITY_XP } from './achievements';
import { CHALLENGE_XP } from './challenges';
import { CATEGORIES, XP_TABLE, setLevelCurve } from './rules';

export interface GameRules {
  // ELO
  eloInitial: number;
  kProvisional: number;
  provisionalMatches: number;
  kEstablished: number;
  goalDiffMultiplier: boolean;
  // Rangos: ELO mínimo de cada uno (Chatarra es todo lo de debajo de Madera)
  rankWood: number;
  rankBronze: number;
  rankSilver: number;
  rankGold: number;
  rankPlatinum: number;
  rankDiamond: number;
  // Experiencia por partido
  xpComplete: number;
  xpWin: number;
  xpDraw: number;
  xpLoss: number;
  xpRankedWinBonus: number;
  xpOvertimeWin: number;
  xpPenaltiesWin: number;
  xpTournamentWin: number;
  // Logros y retos
  xpAchievementCommon: number;
  xpAchievementRare: number;
  xpAchievementEpic: number;
  xpChallengeDaily: number;
  xpChallengeWeekly: number;
  // Niveles
  levelBase: number;
  levelExponent: number;
  maxLevel: number;
}

/** Valores recomendados (y los que usa la app mientras no haya nada publicado). */
export const DEFAULT_RULES: GameRules = {
  eloInitial: 1200,
  kProvisional: 40,
  provisionalMatches: 10,
  kEstablished: 20,
  goalDiffMultiplier: false,
  rankWood: 1225,
  rankBronze: 1275,
  rankSilver: 1350,
  rankGold: 1425,
  rankPlatinum: 1500,
  rankDiamond: 1600,
  xpComplete: 50,
  xpWin: 100,
  xpDraw: 60,
  xpLoss: 25,
  xpRankedWinBonus: 50,
  xpOvertimeWin: 25,
  xpPenaltiesWin: 25,
  xpTournamentWin: 300,
  xpAchievementCommon: 25,
  xpAchievementRare: 50,
  xpAchievementEpic: 100,
  xpChallengeDaily: 40,
  xpChallengeWeekly: 100,
  levelBase: 100,
  levelExponent: 1.35,
  maxLevel: 100,
};

export type RuleKey = keyof GameRules;
type NumberKey = { [K in RuleKey]: GameRules[K] extends number ? K : never }[RuleKey];

export interface RuleField {
  key: RuleKey;
  label: string;
  /** Unidad que se enseña junto al número. */
  unit?: string;
  min?: number;
  max?: number;
  step?: number;
  /** Qué significa el dato. */
  meaning: string;
  /** Por qué se recomienda ese valor. */
  why: string;
}

export interface RuleGroup {
  id: string;
  title: string;
  intro: string;
  fields: RuleField[];
}

export const RULE_GROUPS: RuleGroup[] = [
  {
    id: 'elo',
    title: 'ELO: quién es mejor',
    intro:
      'El ELO es la puntuación de nivel de cada jugador y decide la clasificación. Solo se mueve en los partidos Clasificatorios (y en los torneos que cuentan para ELO). Ganar a alguien mejor que tú da muchos puntos; ganar a alguien peor, pocos. Lo que gana uno lo pierde el otro.',
    fields: [
      {
        key: 'eloInitial',
        label: 'ELO inicial',
        unit: 'puntos',
        min: 800,
        max: 2000,
        step: 50,
        meaning: 'Los puntos con los que empieza cada jugador nuevo.',
        why: 'Es el número de casi todos los sistemas ELO. En realidad da igual cuál sea: lo que importa son las diferencias entre jugadores. Si lo cambias, cambia también los rangos, porque están puestos contando desde 1200.',
      },
      {
        key: 'kProvisional',
        label: 'Puntos en juego en los primeros partidos',
        unit: 'K',
        min: 10,
        max: 80,
        step: 5,
        meaning: 'Cuánto se mueve el ELO mientras el jugador se está colocando. Ganar a un rival de tu mismo nivel da la mitad de este número.',
        why: 'Al principio no se sabe lo bueno que es alguien. Con 40, en unos 10 partidos ya está en su sitio. Más alto, un par de partidos con suerte le colocan demasiado arriba; más bajo, tarda mucho en llegar a donde le toca.',
      },
      {
        key: 'provisionalMatches',
        label: 'Partidos de colocación',
        unit: 'partidos',
        min: 0,
        max: 30,
        step: 1,
        meaning: 'Cuántos partidos Clasificatorios juega cada uno con los puntos de arriba antes de pasar a los normales.',
        why: 'Con 10 da tiempo a colocar a cualquiera sin que dure eternamente. Con muy pocos, un jugador bueno que llega nuevo tarda mucho en subir.',
      },
      {
        key: 'kEstablished',
        label: 'Puntos en juego después',
        unit: 'K',
        min: 8,
        max: 50,
        step: 2,
        meaning: 'Cuánto se mueve el ELO a partir de ahí. Con 20: ganar a un igual da +10, ganar a alguien 200 puntos mejor da +15 y ganar a alguien 200 peor, +5.',
        why: 'Con 20 el ranking se mueve lo justo: una buena tarde se nota, pero una mala racha no te hunde. En un grupo pequeño que juega mucho entre sí, más alto hace que la clasificación baile demasiado de un día a otro.',
      },
      {
        key: 'goalDiffMultiplier',
        label: 'Premiar las goleadas',
        meaning: 'Encendido, ganar por mucha diferencia mueve algo más el ELO: un 5 % más con 2 goles de diferencia y hasta un 20 % con 5 o más. En penaltis no cuenta.',
        why: 'Apagado es más justo en futbolín: un 5-0 a 5 goles depende mucho de rachas, y así el ELO mide solo quién gana. Enciéndelo si queréis que las palizas cuenten más.',
      },
    ],
  },
  {
    id: 'ranks',
    title: 'Rangos: el escalón de cada uno',
    intro:
      'Cada jugador tiene un rango según su ELO: Chatarra, Madera, Bronce, Plata, Oro, Platino y Diamante. Aquí se pone el ELO mínimo de cada uno; por debajo de Madera es Chatarra, donde empieza todo el mundo. En un grupo de 10 a 20 personas, tras muchos partidos, casi todos quedan entre 1100 y 1350: los escalones están pensados para que en ese tramo haya varios rangos y arriba cueste mucho. Cada uno tiene que ser mayor que el anterior.',
    fields: [
      {
        key: 'rankWood',
        label: 'Madera',
        unit: 'ELO',
        step: 5,
        meaning: 'ELO para salir de Chatarra y pasar a Madera.',
        why: 'Unas 3 victorias más que derrotas contra rivales parecidos. El primer escalón llega pronto para que todo el mundo vea enseguida que se puede subir.',
      },
      {
        key: 'rankBronze',
        label: 'Bronce',
        unit: 'ELO',
        step: 5,
        meaning: 'ELO para pasar a Bronce.',
        why: 'Unas 8 victorias de ventaja: ya juegas mejor que la media del grupo.',
      },
      {
        key: 'rankSilver',
        label: 'Plata',
        unit: 'ELO',
        step: 5,
        meaning: 'ELO para pasar a Plata.',
        why: 'Unas 15 victorias de ventaja: de los mejores del grupo.',
      },
      {
        key: 'rankGold',
        label: 'Oro',
        unit: 'ELO',
        step: 5,
        meaning: 'ELO para pasar a Oro.',
        why: 'Solo los 2 o 3 mejores de un grupo que juega a menudo.',
      },
      {
        key: 'rankPlatinum',
        label: 'Platino',
        unit: 'ELO',
        step: 5,
        meaning: 'ELO para pasar a Platino.',
        why: 'El mejor del grupo, tras muchos meses de partidos.',
      },
      {
        key: 'rankDiamond',
        label: 'Diamante',
        unit: 'ELO',
        step: 5,
        meaning: 'ELO para pasar a Diamante.',
        why: 'Casi inalcanzable en un grupo pequeño: es la meta, y tiene que costar.',
      },
    ],
  },
  {
    id: 'xp',
    title: 'Experiencia: lo que se gana jugando',
    intro:
      'La experiencia (XP) siempre suma, se gane o se pierda, y sirve para subir de nivel. El ELO premia ganar; la XP premia jugar. Cuenta en todos los modos (Rápido, Loco y Clasificatorio). En un torneo, la XP de sus partidos se da toda junta al terminarlo.',
    fields: [
      {
        key: 'xpComplete',
        label: 'Por jugar un partido',
        unit: 'XP',
        min: 0,
        max: 500,
        step: 5,
        meaning: 'Lo que gana cada jugador solo por terminar un partido, gane o pierda.',
        why: 'Es la base: hace que jugar siempre merezca la pena.',
      },
      {
        key: 'xpWin',
        label: 'Por ganar',
        unit: 'XP',
        min: 0,
        max: 500,
        step: 5,
        meaning: 'Lo que se suma por ganar, además de lo de jugar.',
        why: 'Con los valores recomendados, ganar da el doble que perder (150 frente a 75): se premia ganar sin castigar demasiado al que pierde.',
      },
      {
        key: 'xpDraw',
        label: 'Por empatar',
        unit: 'XP',
        min: 0,
        max: 500,
        step: 5,
        meaning: 'Lo que se suma por empatar. Solo puede pasar en partidos a tiempo.',
        why: 'Algo más que perder y bastante menos que ganar.',
      },
      {
        key: 'xpLoss',
        label: 'Por perder',
        unit: 'XP',
        min: 0,
        max: 500,
        step: 5,
        meaning: 'Lo que se suma por perder, además de lo de jugar.',
        why: 'Que perder también sume anima a seguir jugando, sobre todo a los que van peor.',
      },
      {
        key: 'xpRankedWinBonus',
        label: 'Extra por ganar un Clasificatorio',
        unit: 'XP',
        min: 0,
        max: 500,
        step: 5,
        meaning: 'Se suma al ganar un partido Clasificatorio.',
        why: 'Los Clasificatorios son los partidos serios; ganarlos debe notarse un poco más.',
      },
      {
        key: 'xpOvertimeWin',
        label: 'Extra por ganar en prórroga',
        unit: 'XP',
        min: 0,
        max: 500,
        step: 5,
        meaning: 'Se suma al ganar con el gol de oro de la prórroga.',
        why: 'Un pequeño premio a los partidos más ajustados.',
      },
      {
        key: 'xpPenaltiesWin',
        label: 'Extra por ganar en penaltis',
        unit: 'XP',
        min: 0,
        max: 500,
        step: 5,
        meaning: 'Se suma al ganar en la tanda de penaltis.',
        why: 'Igual que la prórroga: premia aguantar hasta el final.',
      },
      {
        key: 'xpTournamentWin',
        label: 'Por ganar un torneo',
        unit: 'XP',
        min: 0,
        max: 2000,
        step: 25,
        meaning: 'Para cada jugador del equipo campeón, al terminar el torneo.',
        why: 'Equivale a 2 partidos ganados. Ganar un torneo cuesta muchos partidos y tiene que notarse en el nivel, sin que valga más que una buena temporada.',
      },
    ],
  },
  {
    id: 'extras',
    title: 'Logros y retos',
    intro: 'Experiencia extra por conseguir un logro (según lo difícil que sea) y por completar los retos del día y de la semana.',
    fields: [
      {
        key: 'xpAchievementCommon',
        label: 'Logro común',
        unit: 'XP',
        min: 0,
        max: 1000,
        step: 5,
        meaning: 'Por conseguir un logro común (los fáciles, como ganar el primer partido).',
        why: 'Un detalle: la mitad de lo que se gana solo por jugar un partido.',
      },
      {
        key: 'xpAchievementRare',
        label: 'Logro raro',
        unit: 'XP',
        min: 0,
        max: 1000,
        step: 5,
        meaning: 'Por conseguir un logro raro.',
        why: 'El doble que uno común: cuestan bastante más.',
      },
      {
        key: 'xpAchievementEpic',
        label: 'Logro épico',
        unit: 'XP',
        min: 0,
        max: 1000,
        step: 5,
        meaning: 'Por conseguir un logro épico (los más difíciles).',
        why: 'Casi lo mismo que ganar un partido: se consiguen pocas veces y deben celebrarse.',
      },
      {
        key: 'xpChallengeDaily',
        label: 'Reto del día',
        unit: 'XP',
        min: 0,
        max: 1000,
        step: 5,
        meaning: 'Por completar el reto diario (hay uno cada día).',
        why: 'Algo menos que lo que da jugar un partido: un motivo para jugar cada día sin desequilibrar los niveles.',
      },
      {
        key: 'xpChallengeWeekly',
        label: 'Reto de la semana',
        unit: 'XP',
        min: 0,
        max: 1000,
        step: 5,
        meaning: 'Por completar cada reto semanal (hay tres por semana).',
        why: 'Casi como un partido ganado: piden varios días de partidos.',
      },
    ],
  },
  {
    id: 'levels',
    title: 'Niveles',
    intro: 'El nivel sale de toda la experiencia acumulada. Cada nivel cuesta un poco más que el anterior, para que los primeros lleguen rápido y los altos signifiquen algo.',
    fields: [
      {
        key: 'levelBase',
        label: 'Experiencia del primer nivel',
        unit: 'XP',
        min: 20,
        max: 1000,
        step: 10,
        meaning: 'La XP para llegar a nivel 1. Es la escala de toda la tabla: si la doblas, todos los niveles cuestan el doble.',
        why: 'Con 100, el nivel 1 llega en el primer o segundo partido: se ve enseguida que se sube.',
      },
      {
        key: 'levelExponent',
        label: 'Cuánto se endurece cada nivel',
        min: 1,
        max: 2,
        step: 0.05,
        meaning: 'Con 1, todos los niveles cuestan lo mismo. Cuanto más alto, más cuestan los niveles altos respecto a los primeros.',
        why: 'Con 1,35 el nivel 5 llega en unos 8 partidos y el 50 en unos 175: al principio se sube rápido y arriba cuesta de verdad, sin que sea imposible.',
      },
      {
        key: 'maxLevel',
        label: 'Nivel máximo',
        min: 10,
        max: 500,
        step: 10,
        meaning: 'A partir de aquí ya no se sube más, aunque se siga ganando experiencia.',
        why: 'Con los valores recomendados, el nivel 100 son unos 450 partidos: años de jugar. Es un techo que casi nadie toca.',
      },
    ],
  },
];

export const RULE_FIELDS: RuleField[] = RULE_GROUPS.flatMap((g) => g.fields);

const RANK_KEYS: NumberKey[] = ['rankWood', 'rankBronze', 'rankSilver', 'rankGold', 'rankPlatinum', 'rankDiamond'];

/**
 * Reglas válidas a partir de lo que venga (del panel, de la copia guardada o de un formulario): lo que falte o
 * no sea un número se queda en el valor recomendado, cada número dentro de sus límites y los rangos en orden.
 */
export function sanitizeRules(raw: unknown): GameRules {
  const src = raw && typeof raw === 'object' ? (raw as Record<string, unknown>) : {};
  const out: GameRules = { ...DEFAULT_RULES };
  for (const f of RULE_FIELDS) {
    const v = src[f.key];
    const def = DEFAULT_RULES[f.key];
    if (typeof def === 'boolean') {
      if (typeof v === 'boolean') (out as unknown as Record<string, boolean>)[f.key] = v;
      continue;
    }
    if (typeof v !== 'number' || !Number.isFinite(v)) continue;
    let n = v;
    if (f.min !== undefined) n = Math.max(f.min, n);
    if (f.max !== undefined) n = Math.min(f.max, n);
    (out as unknown as Record<string, number>)[f.key] = n;
  }
  // Rangos siempre crecientes: cada uno, al menos 1 punto por encima del anterior.
  for (let i = 1; i < RANK_KEYS.length; i += 1) {
    if (out[RANK_KEYS[i]] <= out[RANK_KEYS[i - 1]]) out[RANK_KEYS[i]] = out[RANK_KEYS[i - 1]] + 1;
  }
  return out;
}

/** Problemas de unas reglas tal y como están escritas (para avisar en el panel antes de guardar). */
export function rulesProblems(r: GameRules): string[] {
  const out: string[] = [];
  for (let i = 1; i < RANK_KEYS.length; i += 1) {
    if (r[RANK_KEYS[i]] <= r[RANK_KEYS[i - 1]]) {
      const a = RULE_FIELDS.find((f) => f.key === RANK_KEYS[i - 1])!.label;
      const b = RULE_FIELDS.find((f) => f.key === RANK_KEYS[i])!.label;
      out.push(`${b} tiene que pedir más ELO que ${a}.`);
    }
  }
  if (r.rankWood <= r.eloInitial) out.push('Madera tiene que pedir más ELO que el inicial: si no, nadie empieza en Chatarra.');
  if (r.xpWin < r.xpLoss) out.push('Ganar da menos experiencia que perder.');
  for (const f of RULE_FIELDS) {
    const v = r[f.key];
    if (typeof v !== 'number') continue;
    if (!Number.isFinite(v)) out.push(`${f.label}: no es un número.`);
    else if ((f.min !== undefined && v < f.min) || (f.max !== undefined && v > f.max)) out.push(`${f.label}: tiene que estar entre ${f.min} y ${f.max}.`);
  }
  return out;
}

/** Cambios respecto a otras reglas, en texto («Por ganar: 100 → 120»), para el registro de actividad. */
export function rulesDiff(before: GameRules, after: GameRules): string[] {
  return RULE_FIELDS.filter((f) => before[f.key] !== after[f.key]).map((f) => `${f.label}: ${fmt(before[f.key])} → ${fmt(after[f.key])}`);
}

const fmt = (v: number | boolean) => (typeof v === 'boolean' ? (v ? 'sí' : 'no') : String(v).replace('.', ','));

/**
 * Pone estas reglas en marcha en la app: tablas de XP, rangos y curva de niveles (los ajustes de ELO van aparte,
 * en los ajustes de progresión, ver `eloSettings`). Después hay que recalcular la progresión.
 */
export function applyGameRules(r: GameRules): void {
  const xp = XP_TABLE;
  xp.complete = r.xpComplete;
  xp.win = r.xpWin;
  xp.draw = r.xpDraw;
  xp.loss = r.xpLoss;
  xp.rankedWinBonus = r.xpRankedWinBonus;
  xp.overtimeWin = r.xpOvertimeWin;
  xp.penaltiesWin = r.xpPenaltiesWin;
  xp.tournamentWin = r.xpTournamentWin;
  RARITY_XP.common = r.xpAchievementCommon;
  RARITY_XP.rare = r.xpAchievementRare;
  RARITY_XP.epic = r.xpAchievementEpic;
  CHALLENGE_XP.daily = r.xpChallengeDaily;
  CHALLENGE_XP.weekly = r.xpChallengeWeekly;
  const mins: Record<string, number> = {
    wood: r.rankWood,
    bronze: r.rankBronze,
    silver: r.rankSilver,
    gold: r.rankGold,
    platinum: r.rankPlatinum,
    diamond: r.rankDiamond,
  };
  for (const c of CATEGORIES) if (c.id in mins) c.min = mins[c.id];
  setLevelCurve(r.levelBase, r.levelExponent, r.maxLevel);
}

/** La parte de ELO de las reglas, en el formato de los ajustes de progresión. */
export function eloSettings(r: GameRules) {
  return {
    eloInitial: r.eloInitial,
    kProvisional: r.kProvisional,
    kEstablished: r.kEstablished,
    provisionalMatches: r.provisionalMatches,
    goalDiffMultiplier: r.goalDiffMultiplier,
  };
}

/** Reglas publicadas en el panel: su contenido y cuándo se guardaron. */
export interface PublishedRules {
  rules: GameRules;
  updatedAt: number;
}

const CACHE_KEY = 'mfv3:reglas';

export function readCachedRules(store: Pick<Storage, 'getItem'>): PublishedRules | null {
  try {
    const raw = store.getItem(CACHE_KEY);
    if (!raw) return null;
    const p = JSON.parse(raw) as { rules?: unknown; updatedAt?: unknown };
    return typeof p.updatedAt === 'number' ? { rules: sanitizeRules(p.rules), updatedAt: p.updatedAt } : null;
  } catch {
    return null;
  }
}

export function writeCachedRules(store: Pick<Storage, 'setItem'>, p: PublishedRules): void {
  try {
    store.setItem(CACHE_KEY, JSON.stringify(p));
  } catch {
    // Sin sitio: se volverán a descargar.
  }
}

/** Descarga las reglas publicadas (null si aún no hay ninguna). Lanza si no hay conexión o falla. */
export async function fetchRemoteRules(url: string, anonKey: string, http: typeof fetch = (...a) => fetch(...a)): Promise<PublishedRules | null> {
  const base = url.replace(/\/+$/, '');
  const r = await http(`${base}/rest/v1/game_rules?id=eq.global&select=data,updated_at`, { headers: { apikey: anonKey, Authorization: `Bearer ${anonKey}` } });
  if (!r.ok) throw new Error(`Reglas: ${r.status}`);
  const rows = (await r.json()) as { data?: unknown; updated_at?: number }[];
  const row = rows[0];
  return row ? { rules: sanitizeRules(row.data), updatedAt: Number(row.updated_at) || 0 } : null;
}

// ---------------------------------------------------------------- cifras de ejemplo (para el panel)

/** XP total para llegar al nivel N con estas reglas. */
export function xpForLevelWith(r: GameRules, level: number): number {
  if (level <= 0) return 0;
  return Math.round(r.levelBase * level ** r.levelExponent);
}

/** XP de un partido normal de media, ganando la mitad (sin extras). */
export function averageMatchXp(r: GameRules): number {
  return r.xpComplete + (r.xpWin + r.xpLoss) / 2;
}

/** ELO que se gana al ganar a un rival con `gap` puntos más (negativo: con menos). */
export function eloGain(k: number, gap: number): number {
  const expected = 1 / (1 + 10 ** (gap / 400));
  return Math.round(k * (1 - expected));
}
