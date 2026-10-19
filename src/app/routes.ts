import type { MatchConfig, MatchMode, MatchState, ParticipantRef } from '../match-engine';
import type { StoredMatch, TournamentFormat } from '../services/persistence';
import type { SaveStatus } from './matchFinalizer';

export type RankingTab = 'standings' | 'history' | 'players' | 'fame' | 'pairs' | 'digest';
export type SettingsTab = 'match' | 'players' | 'sounds' | 'table' | 'data' | 'account';

/** Datos extra que acompañan a un partido hasta guardarlo. */
export interface MatchExtras {
  tournament?: { id: string; fixtureId: string };
}

export type Route =
  | { name: 'home' }
  | { name: 'setup'; mode: MatchMode; config?: MatchConfig }
  | { name: 'select'; config: MatchConfig; participants?: ParticipantRef[] }
  | { name: 'prematch'; config: MatchConfig; participants: ParticipantRef[]; extras?: MatchExtras }
  | { name: 'match'; config: MatchConfig; participants: ParticipantRef[]; resume?: MatchState; extras?: MatchExtras }
  | { name: 'summary'; match: StoredMatch; save: SaveStatus; live: MatchState; extras?: MatchExtras }
  | { name: 'ranking'; tab?: RankingTab }
  | { name: 'matchDetail'; matchId: string; fromTournament?: boolean }
  | { name: 'profile'; playerId: string }
  | { name: 'duel'; a: string; b: string }
  | { name: 'tournament' }
  /** Palmarés de una competición (todas sus ediciones). */
  | { name: 'tournamentHonours'; key: string }
  /** `format`: tipo de torneo (liga, copa o pool) ya elegido; sin él se pregunta tras elegir jugadores. */
  | { name: 'tournamentNew'; templateId?: string; selected?: string[]; step?: 'players' | 'kind' | 'format'; format?: TournamentFormat }
  /**
   * Creador de predefinidos: `templateId` edita uno guardado; `baseId` parte de otro; `format` fija el tipo
   * de uno nuevo. `selected` se devuelve al volver.
   */
  | { name: 'tournamentTemplate'; templateId?: string; baseId?: string; selected?: string[]; format?: TournamentFormat }
  /** `from: 'list'`: se abrió desde «Mis torneos» (volver allí); si no, se vuelve a «¿Quién juega?». */
  /** `ceremony`: abrir el resumen final (al volver del partido que cierra el torneo). */
  | { name: 'tournamentDetail'; id: string; view?: 'report' | 'play'; from?: 'list'; ceremony?: boolean }
  | { name: 'challenges' }
  | { name: 'settings'; tab?: SettingsTab };
