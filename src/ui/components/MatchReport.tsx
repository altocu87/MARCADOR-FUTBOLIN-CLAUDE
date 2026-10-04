/** Informe de un partido terminado: resumen, cronología, evolución y progresión. */
import { useState } from 'react';
import { useApp } from '../../app/AppContext';
import { annulledGoalIdsFromEvents, isSinglePeriod, validGoalsFromEvents, type Period, type Team } from '../../match-engine';
import type { StoredMatch } from '../../services/persistence';
import { ACHIEVEMENTS } from '../../services/progression';
import { formatDuration } from '../../services/statistics';
import { Avatar, Tabs, formatDate } from './common';
import { TeamFrame } from './PlayerCards';
import { ScoreChart } from './ScoreChart';

const TEAM: Record<Team, string> = { white: 'Blanco', blue: 'Azul' };
const PERIOD: Record<Period, string> = { first: '1ª parte', second: '2ª parte', overtime: 'Prórroga', shootout: 'Penaltis' };
const REASON = { regulation: 'Tiempo reglamentario', golden_goal: 'Gol de oro en la prórroga', penalties: 'Tanda de penaltis' };

export function resultLine(m: StoredMatch): string {
  const r = m.result;
  const base = `${r.score.white}–${r.score.blue}`;
  if (r.penaltyScore) return `${base} · Penaltis ${r.penaltyScore.white}–${r.penaltyScore.blue} · Gana ${TEAM[r.winner]}`;
  if (r.reason === 'golden_goal') return `${base} · Gol de oro · Gana ${TEAM[r.winner]}`;
  return `${base} · Gana ${TEAM[r.winner]}`;
}

/** Por goles no hay partes: el periodo único se llama «Partido» (los partidos antiguos sí tenían dos). */
function periodName(m: StoredMatch, period: Period): string {
  const single = isSinglePeriod(m.config) && !m.periods.some((p) => p.period === 'second');
  return period === 'first' && single ? 'Partido' : PERIOD[period];
}

type ReportTab = 'summary' | 'chart' | 'scorers' | 'progress';

export function MatchReport({ match: given }: { match: StoredMatch }) {
  const { progression, players, matches } = useApp();
  // Versión guardada (incluye goleadores editados después).
  const saved = matches.find((m) => m.id === given.id);
  const match = saved ?? given;
  const [tab, setTab] = useState<ReportTab>('summary');
  const photos = new Map(players.map((p) => [p.id, p.photo]));
  const r = match.result;
  const progressEntries = progression?.byMatch.get(match.id);


  return (
    <div className="report">
      <Tabs
        label="Secciones del resumen"
        value={tab}
        onChange={setTab}
        tabs={[
          { id: 'summary', label: 'Resumen' },
          { id: 'chart', label: 'Evolución' },
          { id: 'scorers', label: 'Goleadores' },
          { id: 'progress', label: 'Progresión' },
        ]}
      />
      {tab === 'summary' && (
        <div className="rep-summary">
          <TeamFrame team="white" participants={match.participants} compact />
          <div className="rep-center">
            {/* Marcador grande: el ganador en dorado. */}
            <div className="rep-big-score">
              <span className={`rep-big-num white ${r.winner === 'white' ? 'won' : ''}`}>{r.score.white}</span>
              <span className="rep-big-sep">–</span>
              <span className={`rep-big-num blue ${r.winner === 'blue' ? 'won' : ''}`}>{r.score.blue}</span>
            </div>
            <div className="rep-sub">
              {r.penaltyScore ? `Penaltis ${r.penaltyScore.white}–${r.penaltyScore.blue} · ` : ''}
              {REASON[r.reason]} · {formatDuration(r.totalTimeMs)}
              <span className="rep-date"> · {formatDate(match.finishedAt)}</span>
            </div>
            {/* Cronología de goles: los de Blanco a la izquierda y los de Azul a la derecha;
                los anulados, tachados en rojo. */}
            <GoalTimeline match={match} name={(p) => periodName(match, p)} />
          </div>
          <TeamFrame team="blue" participants={match.participants} compact />
        </div>
      )}
      {tab === 'chart' && <ScoreChart events={match.events} totalTimeMs={r.totalTimeMs} />}
      {tab === 'scorers' && <ScorersEditor match={match} editable={!!saved} />}
      {tab === 'progress' && (
        <div className="scroll" style={{ flex: 1, minHeight: 0 }}>
          {!progression ? (
            <div className="notice warn">Clasificación pendiente: la progresión está desactivada.</div>
          ) : !progressEntries ? (
            <div className="notice">Sin progresión: el partido no está guardado (modo prueba o error de guardado).</div>
          ) : (
            <div className="list">
              {match.participants.map((p) => {
                const e = progressEntries.get(p.playerId);
                if (!e) return null;
                return (
                  <div key={p.playerId} className="row" style={{ alignItems: 'flex-start' }}>
                    <Avatar name={p.nameSnapshot} photo={photos.get(p.playerId)} size={34} />
                    <div style={{ flex: 1 }}>
                      <strong>{p.nameSnapshot}</strong>{' '}
                      <span className="muted" style={{ fontSize: 12 }}>
                        Nivel {e.levelBefore}
                        {e.levelAfter !== e.levelBefore && ` → ${e.levelAfter}`}
                      </span>
                      <div className="dim" style={{ fontSize: 12 }}>
                        {e.xpBreakdown.map((l) => `${l.label} +${l.xp}`).join(' · ')}
                      </div>
                      {e.unlocked.length > 0 && (
                        <div style={{ fontSize: 12, color: 'var(--ranked)' }}>
                          Logros: {e.unlocked.map((id) => ACHIEVEMENTS.find((a) => a.id === id)?.name ?? id).join(', ')}
                        </div>
                      )}
                    </div>
                    <div style={{ textAlign: 'right' }}>
                      <div style={{ fontWeight: 800, color: 'var(--accent)' }}>+{e.xpGained} XP</div>
                      {e.eloDelta !== undefined && (
                        <div style={{ fontWeight: 800, color: e.eloDelta >= 0 ? 'var(--ok)' : 'var(--danger)' }}>
                          ELO {e.eloDelta >= 0 ? '+' : ''}
                          {e.eloDelta} ({e.eloAfter})
                        </div>
                      )}
                    </div>
                  </div>
                );
              })}
              <div className="dim" style={{ fontSize: 11 }}>
                Reglas propuestas ({progression.rulesVersion}) pendientes de aprobación.
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
}

/** Goleador opcional: se asigna tras el partido, sin obligar. En 1v1 es automático. */
function ScorersEditor({ match, editable }: { match: StoredMatch; editable: boolean }) {
  const { saveMatch, toast } = useApp();
  const goals = validGoalsFromEvents(match.events);
  const assign = async (goalId: string, playerId: string | null) => {
    const scorers = { ...(match.scorers ?? {}) };
    if (playerId) scorers[goalId] = playerId;
    else delete scorers[goalId];
    try {
      await saveMatch({ ...match, scorers });
    } catch {
      toast('No se pudo guardar el goleador');
    }
  };
  if (goals.length === 0) return <div className="muted">Sin goles ordinarios.</div>;
  return (
    <div className="scroll" style={{ flex: 1, minHeight: 0 }}>
      <div className="dim" style={{ fontSize: 12, marginBottom: 6 }}>
        Opcional: indica quién marcó cada gol para las estadísticas personales (pichichi). No afecta al resultado, ELO ni XP del partido.
        {!editable && ' Este partido no está guardado (modo prueba): no se puede asignar.'}
      </div>
      <div className="list">
        {goals.map((g) => {
          const options = match.participants.filter((p) => p.team === g.team);
          const current = match.scorers?.[g.id];
          return (
            <div key={g.id} className="row">
              <span className={`badge ${g.team === 'white' ? '' : 'badge-accent'}`}>{TEAM[g.team!]}</span>
              <span className="muted" style={{ width: 150, fontSize: 13 }}>
                {periodName(match, g.period)} · {formatDuration(g.periodTimeMs)}
              </span>
              <span style={{ fontWeight: 800, width: 50 }}>
                {g.scoreAfter.white}–{g.scoreAfter.blue}
              </span>
              <span style={{ flex: 1 }} />
              {options.map((p) => (
                <button
                  key={p.playerId}
                  className={`btn btn-sm ${current === p.playerId ? 'btn-primary' : ''}`}
                  disabled={!editable || options.length === 1}
                  onClick={() => assign(g.id, current === p.playerId ? null : p.playerId)}
                >
                  ⚽ {p.nameSnapshot}
                </button>
              ))}
            </div>
          );
        })}
      </div>
    </div>
  );
}

/**
 * Cronología de los goles del partido en el centro: cada gol sale hacia el lado de su equipo
 * (Blanco a la izquierda, Azul a la derecha) con el minuto y el marcador. Los anulados aparecen
 * tachados en rojo con «ANULADO».
 */
function GoalTimeline({ match, name }: { match: StoredMatch; name: (p: Period) => string }) {
  const annulled = annulledGoalIdsFromEvents(match.events);
  const goals = match.events.filter((e) => e.type === 'GOAL');
  const multi = new Set(goals.map((g) => g.period)).size > 1;
  if (goals.length === 0) return <div className="rep-tl-empty">Sin goles</div>;
  return (
    <ol className="rep-tl" aria-label="Cronología de goles">
      {goals.map((g) => {
        const off = annulled.has(g.id);
        return (
          <li key={g.id} className={`rep-tl-row ${g.team} ${off ? 'off' : ''}`}>
            <span className="rep-tl-goal">
              <span className="rep-tl-ball" aria-hidden="true">
                {off ? '✕' : '⚽'}
              </span>
              <span className="rep-tl-time">
                {multi ? `${name(g.period)} · ` : ''}
                {formatDuration(g.periodTimeMs)}
              </span>
              {off ? (
                <b className="rep-tl-tag">ANULADO</b>
              ) : (
                <b className="rep-tl-score">
                  {g.scoreAfter.white}–{g.scoreAfter.blue}
                  {(g.value ?? 1) > 1 && <span className="rep-tl-x"> x{g.value}</span>}
                </b>
              )}
            </span>
          </li>
        );
      })}
    </ol>
  );
}
