import type { MatchConfig, MatchMode, MatchState, ParticipantRef } from '../match-engine';
import type { StoredMatch } from '../services/persistence';
import type { SaveStatus } from './matchFinalizer';

export type RankingTab = 'standings' | 'history' | 'players' | 'fame' | 'records' | 'pairs' | 'digest' | 'duel';
export type SettingsTab = 'general' | 'players' | 'audio' | 'progression' | 'connections' | 'diy' | 'system' | 'info';

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
  | { name: 'matchDetail'; matchId: string }
  | { name: 'profile'; playerId: string }
  | { name: 'tournament' }
  | { name: 'tournamentNew' }
  | { name: 'tournamentDetail'; id: string }
  | { name: 'challenges' }
  | { name: 'settings'; tab?: SettingsTab };
