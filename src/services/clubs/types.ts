export type { Club } from '../persistence';

/** Lo mínimo de un participante para saber de qué lado juega. */
export interface ParticipantRefLike {
  playerId: string;
  team: 'white' | 'blue';
}
