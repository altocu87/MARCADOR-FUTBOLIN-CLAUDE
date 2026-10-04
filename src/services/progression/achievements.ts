/**
 * Catálogo de logros PROPUESTO (pendiente de aprobación): 54 logros, 10 secretos.
 * Cada logro único solo se concede una vez, aunque se reprocese el historial.
 * Algunos logros otorgan un TÍTULO que el jugador puede mostrar bajo su nombre.
 */
import type { StoredMatch } from '../persistence';

export type Rarity = 'common' | 'rare' | 'epic';

export const RARITY_XP: Record<Rarity, number> = { common: 25, rare: 50, epic: 100 };
export const RARITY_LABEL: Record<Rarity, string> = { common: 'Común', rare: 'Rara', epic: 'Épica' };

export type AchievementCategory = 'progresion' | 'victorias' | 'rachas' | 'goles' | 'competicion' | 'especiales';

export const CATEGORY_LABEL: Record<AchievementCategory, string> = {
  progresion: 'Progresión',
  victorias: 'Victorias',
  rachas: 'Rachas',
  goles: 'Goles',
  competicion: 'Competición',
  especiales: 'Especiales',
};

/** Contexto del jugador tras procesar un partido. */
export interface AchievementContext {
  match: StoredMatch;
  won: boolean;
  goalsFor: number;
  goalsAgainst: number;
  played: number;
  wins: number;
  winStreak: number;
  /** Derrotas seguidas justo antes de este partido. */
  lossStreakBefore: number;
  rankedPlayed: number;
  rankedWins: number;
  chaosWins: number;
  elo: number;
  /** ELO medio rival menos ELO medio propio antes del partido (solo clasificatorio). */
  eloGapBefore: number;
  level: number;
  comeback: number;
  /** Gol propio más temprano (ms desde el inicio de su parte) o null. */
  earliestOwnGoalMs: number | null;
  /** Mayor serie de goles seguidos del propio equipo en el partido. */
  ownGoalRun: number;
  totalGoals: number;
  personalGoalsInMatch: number;
  personalGoalsTotal: number;
  /** El jugador marcó (asignado) el primer gol del partido. */
  firstGoalOfMatch: boolean;
  penaltiesPerfect: boolean;
  suddenDeathRounds: number;
  hour: number;
  matchesToday: number;
  distinctTeammates: number;
  distinctRivals: number;
  winsWithSameTeammate: number;
  tournamentsWon: number;
  challengesDone: number;
}

export interface AchievementDef {
  id: string;
  name: string;
  description: string;
  category: AchievementCategory;
  rarity: Rarity;
  secret?: boolean;
  /** Título que desbloquea. */
  title?: string;
  icon: string;
  check(ctx: AchievementContext): boolean;
}

const A = (d: AchievementDef) => d;

export const ACHIEVEMENTS: AchievementDef[] = [
  // Progresión
  A({ id: 'debut', name: 'Debut', description: 'Completa tu primer partido.', category: 'progresion', rarity: 'common', icon: '⚑', check: (c) => c.played >= 1 }),
  A({ id: 'veteran', name: 'Veterano', description: 'Completa 25 partidos.', category: 'progresion', rarity: 'rare', icon: '★', title: 'Veterano', check: (c) => c.played >= 25 }),
  A({ id: 'legend', name: 'Leyenda de la mesa', description: 'Completa 100 partidos.', category: 'progresion', rarity: 'epic', icon: '♛', title: 'Leyenda', check: (c) => c.played >= 100 }),
  A({ id: 'level10', name: 'Nivel 10', description: 'Alcanza el nivel 10.', category: 'progresion', rarity: 'rare', icon: '⬆', check: (c) => c.level >= 10 }),
  A({ id: 'level25', name: 'Nivel 25', description: 'Alcanza el nivel 25.', category: 'progresion', rarity: 'epic', icon: '⏫', check: (c) => c.level >= 25 }),
  A({ id: 'level50', name: 'Medio centenar', description: 'Alcanza el nivel 50.', category: 'progresion', rarity: 'epic', secret: true, icon: '✶', title: 'Gran Maestro', check: (c) => c.level >= 50 }),
  A({ id: 'challenger', name: 'Retador', description: 'Completa tu primer reto.', category: 'progresion', rarity: 'common', icon: '◎', check: (c) => c.challengesDone >= 1 }),
  A({ id: 'tireless', name: 'Incansable', description: 'Completa 10 retos.', category: 'progresion', rarity: 'rare', icon: '∞', title: 'Incansable', check: (c) => c.challengesDone >= 10 }),
  // Victorias
  A({ id: 'first_win', name: 'Primera victoria', description: 'Gana tu primer partido.', category: 'victorias', rarity: 'common', icon: '✓', check: (c) => c.won && c.wins >= 1 }),
  A({ id: 'wins10', name: 'Diez victorias', description: 'Gana 10 partidos.', category: 'victorias', rarity: 'rare', icon: '✪', check: (c) => c.wins >= 10 }),
  A({ id: 'wins50', name: 'Cincuenta victorias', description: 'Gana 50 partidos.', category: 'victorias', rarity: 'epic', icon: '✹', check: (c) => c.wins >= 50 }),
  A({ id: 'wins100', name: 'Centenario', description: 'Gana 100 partidos.', category: 'victorias', rarity: 'epic', icon: '💯', title: 'Centenario', check: (c) => c.wins >= 100 }),
  A({ id: 'ranked_wins10', name: 'Competitivo', description: 'Gana 10 clasificatorios.', category: 'victorias', rarity: 'rare', icon: '⚔', check: (c) => c.rankedWins >= 10 }),
  A({ id: 'express', name: 'Exprés', description: 'Gana un partido en menos de 3 minutos.', category: 'victorias', rarity: 'rare', icon: '⏱', title: 'Relámpago', check: (c) => c.won && c.match.result.totalTimeMs < 180_000 }),
  A({ id: 'dynamic_duo', name: 'Dúo dinámico', description: 'Gana 10 partidos con el mismo compañero.', category: 'victorias', rarity: 'rare', icon: '⚭', title: 'Dúo Dinámico', check: (c) => c.winsWithSameTeammate >= 10 }),
  // Rachas
  A({ id: 'streak3', name: 'En racha', description: 'Gana 3 partidos seguidos.', category: 'rachas', rarity: 'common', icon: '➹', check: (c) => c.winStreak >= 3 }),
  A({ id: 'streak5', name: 'Imparable', description: 'Gana 5 partidos seguidos.', category: 'rachas', rarity: 'rare', icon: '⚡', title: 'Imparable', check: (c) => c.winStreak >= 5 }),
  A({ id: 'streak10', name: 'Invencible', description: 'Gana 10 partidos seguidos.', category: 'rachas', rarity: 'epic', icon: '♜', title: 'Invencible', check: (c) => c.winStreak >= 10 }),
  A({ id: 'phoenix', name: 'Fénix', description: 'Gana tras perder 5 seguidos.', category: 'rachas', rarity: 'rare', secret: true, icon: '🜂', title: 'Fénix', check: (c) => c.won && c.lossStreakBefore >= 5 }),
  // Goles
  A({ id: 'shutout', name: 'Portería a cero', description: 'Gana sin encajar ningún gol.', category: 'goles', rarity: 'rare', icon: '⛨', title: 'El Muro', check: (c) => c.won && c.goalsAgainst === 0 && c.goalsFor > 0 }),
  A({ id: 'rout', name: 'Goleada', description: 'Gana por 5 o más goles de diferencia.', category: 'goles', rarity: 'rare', icon: '✺', check: (c) => c.won && c.goalsFor - c.goalsAgainst >= 5 }),
  A({ id: 'double_digits', name: 'Doble dígito', description: 'Tu equipo marca 10 o más goles en un partido.', category: 'goles', rarity: 'common', icon: '⑩', check: (c) => c.goalsFor >= 10 }),
  A({ id: 'goal_fest', name: 'Festival', description: 'Juega un partido con 15 o más goles.', category: 'goles', rarity: 'common', icon: '🎉', check: (c) => c.totalGoals >= 15 }),
  A({ id: 'early_goal', name: 'Madrugador', description: 'Tu equipo marca en los primeros 10 segundos de una parte.', category: 'goles', rarity: 'rare', icon: '☄', check: (c) => c.earliestOwnGoalMs !== null && c.earliestOwnGoalMs <= 10_000 }),
  A({ id: 'run5', name: 'Apisonadora', description: 'Tu equipo marca 5 goles seguidos sin respuesta.', category: 'goles', rarity: 'rare', icon: '⛟', title: 'Apisonadora', check: (c) => c.ownGoalRun >= 5 }),
  A({ id: 'hat_trick', name: 'Hat-trick', description: 'Marca 3 goles personales (asignados) en un partido.', category: 'goles', rarity: 'rare', icon: '⚽', check: (c) => c.personalGoalsInMatch >= 3 }),
  A({ id: 'scorer50', name: 'Goleador', description: 'Llega a 50 goles personales asignados.', category: 'goles', rarity: 'epic', icon: '🥅', title: 'Pichichi', check: (c) => c.personalGoalsTotal >= 50 }),
  A({ id: 'first_blood', name: 'Primera sangre', description: 'Marca (asignado) el primer gol de un partido.', category: 'goles', rarity: 'common', secret: true, icon: '◉', check: (c) => c.firstGoalOfMatch }),
  // Competición
  A({ id: 'ranked_debut', name: 'Competidor', description: 'Juega tu primer partido clasificatorio.', category: 'competicion', rarity: 'common', icon: '⚔', check: (c) => c.rankedPlayed >= 1 }),
  A({ id: 'platinum', name: 'Platino', description: 'Alcanza 1500 de ELO.', category: 'competicion', rarity: 'rare', icon: '◆', check: (c) => c.elo >= 1500 }),
  A({ id: 'diamond', name: 'Diamante', description: 'Alcanza 1600 de ELO.', category: 'competicion', rarity: 'epic', icon: '◈', check: (c) => c.elo >= 1600 }),
  A({ id: 'elite', name: 'Élite', description: 'Alcanza 1800 de ELO.', category: 'competicion', rarity: 'epic', icon: '♔', title: 'Élite', check: (c) => c.elo >= 1800 }),
  A({ id: 'giant_killer', name: 'Matagigantes', description: 'Gana un clasificatorio contra rivales con 150+ ELO más.', category: 'competicion', rarity: 'rare', icon: '⚒', title: 'Matagigantes', check: (c) => c.won && c.match.config.mode === 'ranked' && c.eloGapBefore >= 150 }),
  A({ id: 'champion', name: 'Campeón', description: 'Gana un torneo.', category: 'competicion', rarity: 'epic', icon: '🏆', title: 'Campeón', check: (c) => c.tournamentsWon >= 1 }),
  A({ id: 'tri_champion', name: 'Tricampeón', description: 'Gana 3 torneos.', category: 'competicion', rarity: 'epic', secret: true, icon: '♕', title: 'Tricampeón', check: (c) => c.tournamentsWon >= 3 }),
  // Especiales
  A({ id: 'golden_goal', name: 'Gol de oro', description: 'Gana un partido en la prórroga.', category: 'especiales', rarity: 'rare', icon: '◎', title: 'Rey de la Prórroga', check: (c) => c.won && c.match.result.reason === 'golden_goal' }),
  A({ id: 'ice_cold', name: 'Sangre fría', description: 'Gana una tanda de penaltis.', category: 'especiales', rarity: 'rare', icon: '❄', title: 'Sangre Fría', check: (c) => c.won && c.match.result.reason === 'penalties' }),
  A({ id: 'perfect_pens', name: 'Penaltis perfectos', description: 'Tu equipo no falla ningún penalti en una tanda.', category: 'especiales', rarity: 'rare', icon: '🎯', check: (c) => c.penaltiesPerfect }),
  A({ id: 'sudden_death', name: 'Al límite', description: 'Gana una tanda en muerte súbita.', category: 'especiales', rarity: 'rare', icon: '☠', check: (c) => c.won && c.suddenDeathRounds >= 1 }),
  A({ id: 'eternal', name: 'Tanda eterna', description: 'Juega una muerte súbita de 3 o más rondas.', category: 'especiales', rarity: 'rare', secret: true, icon: '⧗', check: (c) => c.suddenDeathRounds >= 3 }),
  A({ id: 'comeback', name: 'Remontada', description: 'Gana tras ir perdiendo por 2 o más goles.', category: 'especiales', rarity: 'epic', icon: '↺', title: 'El Remontador', check: (c) => c.won && c.comeback >= 2 }),
  A({ id: 'against_ropes', name: 'Contra las cuerdas', description: 'Gana tras ir perdiendo por 3 o más goles.', category: 'especiales', rarity: 'epic', icon: '🥊', check: (c) => c.won && c.comeback >= 3 }),
  A({ id: 'impossible', name: 'Misión imposible', description: 'Gana tras ir perdiendo por 4 o más goles.', category: 'especiales', rarity: 'epic', secret: true, icon: '✧', check: (c) => c.won && c.comeback >= 4 }),
  A({ id: 'chaos_win', name: 'Locura controlada', description: 'Gana un Partido Loco.', category: 'especiales', rarity: 'common', icon: '✦', check: (c) => c.won && c.match.config.mode === 'chaos' }),
  A({ id: 'chaos_lord', name: 'Señor de la Locura', description: 'Gana 10 Partidos Locos.', category: 'especiales', rarity: 'rare', icon: '✺', title: 'Señor de la Locura', check: (c) => c.chaosWins >= 10 }),
  A({ id: 'social', name: 'Sociable', description: 'Juega con 5 compañeros distintos.', category: 'especiales', rarity: 'common', icon: '☺', check: (c) => c.distinctTeammates >= 5 }),
  A({ id: 'globetrotter', name: 'Trotamundos', description: 'Enfréntate a 10 rivales distintos.', category: 'especiales', rarity: 'rare', icon: '🌐', check: (c) => c.distinctRivals >= 10 }),
  A({ id: 'night_owl', name: 'Búho', description: 'Juega un partido entre las 23:00 y las 5:00.', category: 'especiales', rarity: 'common', secret: true, icon: '🦉', title: 'Búho Nocturno', check: (c) => c.hour >= 23 || c.hour < 5 }),
  A({ id: 'early_bird', name: 'Al alba', description: 'Juega un partido antes de las 9:00.', category: 'especiales', rarity: 'common', secret: true, icon: '🌅', check: (c) => c.hour >= 5 && c.hour < 9 }),
  A({ id: 'marathon_day', name: 'Día de maratón', description: 'Juega 10 partidos en un mismo día.', category: 'especiales', rarity: 'rare', secret: true, icon: '🏃', check: (c) => c.matchesToday >= 10 }),
  A({ id: 'secret_marathon', name: 'Maratón', description: 'Juega un partido de más de 20 minutos.', category: 'especiales', rarity: 'rare', secret: true, icon: '⌛', check: (c) => c.match.result.totalTimeMs >= 20 * 60_000 }),
];

export const achievementById = (id: string) => ACHIEVEMENTS.find((a) => a.id === id);
