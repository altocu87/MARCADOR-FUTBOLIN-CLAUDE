/**
 * Coordinación al finalizar: aplicación → resultado completo → repositorio local.
 * El modo prueba nunca guarda partidos, eventos, progresión ni colas.
 * Si el partido pertenece a un torneo, se registra su resultado en el cuadro/liguilla.
 */
import type { MatchState } from '../match-engine';
import { STORAGE_FORMAT_VERSION, type Repositories, type StoredMatch } from '../services/persistence';
import { autoScorers } from '../services/statistics/extras';
import { recordFixtureResult } from '../services/tournaments';
import type { MatchExtras } from './routes';

export function toStoredMatch(state: MatchState, extras: MatchExtras = {}): StoredMatch {
  if (state.phase !== 'finished' || !state.result || state.finishedAt === undefined) {
    throw new Error('El partido aún no ha terminado.');
  }
  const base: StoredMatch = {
    formatVersion: STORAGE_FORMAT_VERSION,
    id: state.id,
    engineVersion: state.engineVersion,
    rulesVersion: state.rulesVersion,
    config: state.config,
    participants: state.participants,
    createdAt: state.createdAt,
    startedAt: state.startedAt ?? state.createdAt,
    finishedAt: state.finishedAt,
    result: state.result,
    periods: state.periods,
    events: state.events,
    penalties: state.penalties,
    ...(extras.tournament ? { tournament: extras.tournament } : {}),
  };
  // En 1v1 el goleador es inequívoco: se asigna solo.
  const scorers = autoScorers(base);
  return scorers ? { ...base, scorers } : base;
}

export type SaveStatus = { kind: 'test' } | { kind: 'saved' } | { kind: 'error'; message: string };

export async function persistFinishedMatch(
  state: MatchState,
  repos: Repositories,
  extras: MatchExtras = {},
): Promise<SaveStatus> {
  if (state.config.testMode) {
    await repos.activeMatch.clear().catch(() => undefined);
    return { kind: 'test' };
  }
  try {
    const match = toStoredMatch(state, extras);
    await repos.matches.save(match);
    if (extras.tournament) {
      const list = await repos.tournaments.list();
      const t = list.find((x) => x.id === extras.tournament!.id);
      if (t) await repos.tournaments.save(recordFixtureResult(t, extras.tournament.fixtureId, match, await repos.matches.list()));
    }
    await repos.activeMatch.clear();
    return { kind: 'saved' };
  } catch (err) {
    return { kind: 'error', message: err instanceof Error ? err.message : String(err) };
  }
}
