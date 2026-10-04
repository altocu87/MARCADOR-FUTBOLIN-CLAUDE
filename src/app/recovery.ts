/**
 * Recuperación local (propuesta): al reabrir se ofrece reanudar o descartar.
 * Nunca se reanuda con un reloj corriendo: la partida vuelve EN PAUSA con el
 * tiempo de juego que tenía al guardarse el último snapshot (el tiempo con la
 * aplicación cerrada no cuenta). Una cuenta atrás interrumpida se reinicia.
 */
import { COUNTDOWN_MS, HANDICAP_END_PAUSE_MS, advance, periodElapsed, type MatchState } from '../match-engine';
import { STORAGE_FORMAT_VERSION, type ActiveMatchSnapshot } from '../services/persistence';

export function makeSnapshot(state: MatchState, now: number, extras?: ActiveMatchSnapshot['extras']): ActiveMatchSnapshot {
  return { formatVersion: STORAGE_FORMAT_VERSION, savedAt: now, state, ...(extras ? { extras } : {}) };
}

export function restoreSnapshot(snapshot: ActiveMatchSnapshot, now: number): MatchState {
  // Aplicar lo que ya hubiera vencido antes del guardado (p. ej. límite de tiempo).
  const s = advance(snapshot.state, snapshot.savedAt).state;
  if (s.phase === 'playing') {
    return { ...s, phase: 'paused', periodElapsedMs: periodElapsed(s, snapshot.savedAt), runningSince: undefined };
  }
  if (s.phase === 'countdown') {
    return { ...s, countdownEndsAt: now + COUNTDOWN_MS };
  }
  // Partido Loco: la «vuelta a la normalidad» vuelve a contar sus segundos al reabrir.
  if (s.phase === 'handicap' && s.handicap?.stage === 'ending') {
    return { ...s, handicap: { ...s.handicap, resumeAt: now + HANDICAP_END_PAUSE_MS } };
  }
  return s;
}
