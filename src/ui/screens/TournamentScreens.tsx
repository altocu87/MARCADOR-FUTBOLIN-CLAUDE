/**
 * Torneos: lista, creación a partir de un predefinido, creador de predefinidos y detalle
 * (clasificación, cuadro o Pool rotativo, final y partidos).
 */
import { useState, type ReactNode } from 'react';
import { useApp } from '../../app/AppContext';
import type { MatchConfig } from '../../match-engine';
import type { Fixture, Tournament, TournamentFinal, TournamentTemplate } from '../../services/persistence';
import { sortPlayers } from '../../services/players';
import { seededRandom } from '../../services/statistics/calendar';
import {
  BUILT_IN_TEMPLATES,
  FORMAT_LABEL,
  MAX_TEAMS,
  MIN_TEAMS,
  POOL_MAX_PLAYERS,
  POOL_MIN_PLAYERS,
  TOURNAMENT_RULES_VERSION,
  allTemplates,
  createTournament,
  describeTemplate,
  draftFromTemplate,
  finalOptions,
  fixtureConfig,
  fixtureParticipants,
  fixtureScore,
  newTemplate,
  playableFixtures,
  roundLabel,
  standings,
  validateDraft,
} from '../../services/tournaments';
import { AssetImage } from '../components/assets';
import { Avatar, Modal, ScreenFrame, Toggle, formatDate } from '../components/common';
import { Confetti } from '../components/graphics';

const FINAL_LABEL: Record<TournamentFinal, string> = {
  none: 'Sin final',
  top2: '1º contra 2º',
  top4: '1º+4º contra 2º+3º',
};

function Trophy({ size = 64 }: { size?: number }) {
  return (
    <AssetImage
      name="trofeo"
      alt="Trofeo"
      style={{ width: size, height: size, objectFit: 'contain' }}
      fallback={<span className="trophy-glyph" style={{ fontSize: size * 0.8 }} aria-hidden="true">🏆</span>}
    />
  );
}

function Seg<T>({ value, options, onChange }: { value: T; options: [T, string][]; onChange: (v: T) => void }) {
  return (
    <div className="segmented">
      {options.map(([v, label]) => (
        <button key={String(v)} className="seg seg-compact" aria-pressed={value === v} onClick={() => onChange(v)}>
          {label}
        </button>
      ))}
    </div>
  );
}

function teamCountLabel(t: Tournament): string {
  return t.format === 'pool' ? `${t.entrants?.length ?? 0} jugadores` : `${t.teams.length} equipos · ${t.teamSize === 1 ? '1v1' : '2v2'}`;
}

export function TournamentListScreen() {
  const { navigate, tournaments } = useApp();
  const list = [...tournaments].sort((a, b) => b.createdAt - a.createdAt);
  return (
    <ScreenFrame
      title="Torneos"
      onBack={() => navigate({ name: 'home' })}
      right={
        <button className="btn btn-primary btn-sm" onClick={() => navigate({ name: 'tournamentNew' })}>
          + Nuevo torneo
        </button>
      }
    >
      {list.length === 0 ? (
        <div className="empty">
          <div style={{ maxWidth: 520 }}>
            <Trophy size={80} />
            <strong>Aún no hay torneos</strong>
            Elige un tipo (Pool rotativo, liguilla, eliminatoria…) o crea el tuyo y guárdalo como predefinido.
            <div style={{ marginTop: 12 }}>
              <button className="btn btn-primary" onClick={() => navigate({ name: 'tournamentNew' })}>
                Crear torneo
              </button>
            </div>
          </div>
        </div>
      ) : (
        <div className="list scroll" style={{ flex: 1, minHeight: 0 }}>
          {list.map((t) => {
            const real = t.fixtures.filter((f) => !f.bye);
            const done = real.filter((f) => f.winnerTeamId).length;
            const winner = t.teams.find((x) => x.id === t.winnerTeamId);
            return (
              <button key={t.id} className="row" onClick={() => navigate({ name: 'tournamentDetail', id: t.id })}>
                <span style={{ fontSize: 26 }} aria-hidden="true">{t.status === 'finished' ? '🏆' : t.status === 'cancelled' ? '✕' : '⚔'}</span>
                <span style={{ flex: 1 }}>
                  <strong>{t.name}</strong>
                  <div className="muted" style={{ fontSize: 12 }}>
                    {FORMAT_LABEL[t.format]} · {teamCountLabel(t)} · {t.ranked ? 'cuenta para ELO' : 'sin ELO'} · {formatDate(t.createdAt)}
                  </div>
                </span>
                {t.status === 'finished' && winner ? (
                  <span className="badge badge-ranked">Campeón: {winner.name}</span>
                ) : t.status === 'cancelled' ? (
                  <span className="badge">Cancelado</span>
                ) : (
                  <span className="badge badge-accent">{done}/{real.length} partidos</span>
                )}
              </button>
            );
          })}
        </div>
      )}
      <div className="dim" style={{ fontSize: 11 }}>
        Formato propuesto «{TOURNAMENT_RULES_VERSION}»: victoria = 3 puntos; desempate por diferencia de goles y goles a favor. En el Pool puntúa cada jugador. Ganar un torneo da +300 XP.
      </div>
    </ScreenFrame>
  );
}

/** Crear un torneo: se elige el predefinido, los jugadores y se revisa el reparto. */
export function TournamentNewScreen({ templateId, initialSelected }: { templateId?: string; initialSelected?: string[] }) {
  const { navigate, players, progression, prefs, saveTournament, toast } = useApp();
  const templates = allTemplates(prefs.tournamentTemplates);
  const [tplId, setTplId] = useState(templateId ?? BUILT_IN_TEMPLATES[0].id);
  const [name, setName] = useState(`Torneo ${new Date().toLocaleDateString('es-ES', { day: 'numeric', month: 'short' })}`);
  const [selected, setSelected] = useState<string[]>(initialSelected ?? []);
  // Semilla del sorteo: el reparto no cambia al redibujar, solo al pulsar «Sortear otra vez».
  const [seed, setSeed] = useState(() => String(Date.now()));
  const tpl = templates.find((x) => x.id === tplId) ?? templates[0];
  const elo = (id: string) => progression?.players.get(id)?.elo ?? prefs.progression.eloInitial;
  const nameOf = (id: string) => players.find((p) => p.id === id)?.name ?? '?';
  const toggle = (id: string) => setSelected((s) => (s.includes(id) ? s.filter((x) => x !== id) : [...s, id]));

  const { draft, leftover } = draftFromTemplate(tpl, name, selected, elo, { penaltyFirstTeam: prefs.penaltyFirstTeam }, seededRandom(seed));
  const errors = leftover.length
    ? [`Parejas fijas: sobra ${nameOf(leftover[0])}.`]
    : validateDraft(draft);
  const isPool = tpl.format === 'pool';
  const usesDraw = isPool ? tpl.pairing === 'random' : (tpl.teamSize === 2 && tpl.pairing === 'random') || (tpl.format === 'bracket' && tpl.seeding === 'random');
  const range = isPool
    ? `${POOL_MIN_PLAYERS}–${POOL_MAX_PLAYERS}`
    : tpl.teamSize === 1
      ? `${MIN_TEAMS}–${MAX_TEAMS}`
      : `${MIN_TEAMS * 2}–${MAX_TEAMS * 2}, número par`;

  const create = async () => {
    try {
      const t = createTournament(draft, players, elo, Date.now(), seededRandom(`${seed}-cal`));
      await saveTournament(t);
      navigate({ name: 'tournamentDetail', id: t.id });
    } catch (err) {
      toast(err instanceof Error ? err.message : 'No se pudo crear');
    }
  };

  let preview: ReactNode;
  if (selected.length === 0) preview = <span className="dim">Selecciona jugadores.</span>;
  else if (isPool) {
    const n = selected.length;
    const g = draft.gamesPerPlayer ?? 0;
    const rest = n % 4;
    preview =
      n < POOL_MIN_PLAYERS ? (
        <span className="dim">Faltan {POOL_MIN_PLAYERS - n} jugador(es) para el primer 2 contra 2.</span>
      ) : (
        <div className="pool-preview">
          <strong>{(n * g) / 4} partidos de 2 contra 2</strong>
          <span>Cada jugador juega {g} partido(s) con parejas distintas{g !== tpl.gamesPerPlayer ? ` (ajustado de ${tpl.gamesPerPlayer} para que todos jueguen lo mismo)` : ''}.</span>
          <span>{rest === 0 ? 'En cada ronda juegan todos.' : `En cada ronda descansa${rest > 1 ? 'n' : ''} ${rest}, por turnos.`}</span>
          {draft.final !== 'none' && <span>Final: {FINAL_LABEL[draft.final!]}{tpl.finalBestOf > 1 ? ', al mejor de 3' : ''}.</span>}
        </div>
      );
  } else {
    preview = (
      <div className="tn-teams">
        {draft.teams.map((t, i) => (
          <span key={i} className="tn-team">{t.playerIds.map(nameOf).join(' + ')}</span>
        ))}
        {leftover.map((id) => (
          <span key={id} className="tn-team tn-team-out">{nameOf(id)} · sin pareja</span>
        ))}
      </div>
    );
  }

  return (
    <ScreenFrame
      title="Nuevo torneo"
      onBack={() => navigate({ name: 'tournament' })}
      footer={
        <>
          <span className={`notice ${errors.length ? 'warn' : ''}`} style={{ marginRight: 'auto' }}>
            {errors[0] ?? (isPool ? `${selected.length} jugadores listos` : `${draft.teams.length} equipos listos`)}
          </span>
          {leftover.length > 0 && (
            <button className="btn btn-ghost btn-sm" onClick={() => setTplId(BUILT_IN_TEMPLATES[0].id)}>
              Pasar a Pool rotativo
            </button>
          )}
          <button className="btn btn-primary btn-lg" disabled={errors.length > 0} onClick={create}>
            Crear torneo
          </button>
        </>
      }
    >
      <div className="tn-layout">
        <div className="tn-col">
          <div className="label">Tipo de torneo</div>
          <div className="tpl-list scroll">
            {templates.map((t) => (
              <button key={t.id} className="tpl-card" aria-pressed={t.id === tpl.id} onClick={() => setTplId(t.id)}>
                <strong>
                  {t.name}
                  {!t.builtIn && <span className="badge" style={{ marginLeft: 6 }}>mío</span>}
                </strong>
                <span>{describeTemplate(t)}</span>
              </button>
            ))}
          </div>
          <div style={{ display: 'flex', gap: 6 }}>
            <button className="btn btn-ghost btn-sm" onClick={() => navigate({ name: 'tournamentTemplate', selected })}>
              + Crear tipo
            </button>
            <button
              className="btn btn-ghost btn-sm"
              onClick={() => navigate(tpl.builtIn ? { name: 'tournamentTemplate', baseId: tpl.id, selected } : { name: 'tournamentTemplate', templateId: tpl.id, selected })}
            >
              {tpl.builtIn ? 'Copiar y ajustar' : 'Editar tipo'}
            </button>
          </div>
        </div>
        <div className="tn-col">
          <input className="input" value={name} onChange={(e) => setName(e.target.value)} aria-label="Nombre del torneo" maxLength={30} />
          <div className="label">Jugadores ({selected.length}) · {range}</div>
          <div className="chip-wrap scroll" style={{ maxHeight: 110 }}>
            {sortPlayers(players.filter((p) => p.active)).map((p) => (
              <button key={p.id} className={`pick-chip ${selected.includes(p.id) ? 'pick-blue' : ''}`} onClick={() => toggle(p.id)}>
                {p.name}
              </button>
            ))}
          </div>
          <div className="label" style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <span style={{ flex: 1 }}>
              {isPool ? 'Cómo se jugará' : `Equipos${tpl.teamSize === 2 ? (tpl.pairing === 'elo' ? ' (parejas equilibradas por ELO)' : ' (parejas al azar)') : ''}`}
            </span>
            {usesDraw && selected.length > 0 && (
              <button className="btn btn-ghost btn-sm" onClick={() => setSeed(String(Date.now()))}>
                ↻ Sortear otra vez
              </button>
            )}
          </div>
          <div className="scroll" style={{ minHeight: 0 }}>{preview}</div>
        </div>
      </div>
    </ScreenFrame>
  );
}

/** Creador de tipos de torneo: se guardan como predefinidos para reutilizarlos. */
export function TournamentTemplateScreen({ templateId, baseId, selected }: { templateId?: string; baseId?: string; selected?: string[] }) {
  const { navigate, prefs, savePrefs, toast } = useApp();
  const saved = prefs.tournamentTemplates;
  const existing = saved.find((x) => x.id === templateId);
  const [tpl, setTpl] = useState<TournamentTemplate>(() => {
    if (existing) return { ...existing };
    const base = allTemplates(saved).find((x) => x.id === baseId);
    return newTemplate(base);
  });
  const [confirmDelete, setConfirmDelete] = useState(false);
  const set = <K extends keyof TournamentTemplate>(key: K, value: TournamentTemplate[K]) => setTpl((t) => ({ ...t, [key]: value }));
  const isPool = tpl.format === 'pool';
  const finals = finalOptions(tpl.format);
  const hasFinal = tpl.format === 'bracket' || tpl.final !== 'none';

  const setFormat = (format: TournamentTemplate['format']) =>
    setTpl((t) => ({
      ...t,
      format,
      teamSize: format === 'pool' ? 2 : t.teamSize,
      final: finalOptions(format).includes(t.final) ? t.final : 'none',
    }));

  const save = async () => {
    const clean: TournamentTemplate = { ...tpl, name: tpl.name.trim(), builtIn: false };
    const list = existing ? saved.map((x) => (x.id === clean.id ? clean : x)) : [...saved, clean];
    await savePrefs({ ...prefs, tournamentTemplates: list });
    toast('Predefinido guardado');
    navigate({ name: 'tournamentNew', templateId: clean.id, selected });
  };

  const remove = async () => {
    await savePrefs({ ...prefs, tournamentTemplates: saved.filter((x) => x.id !== tpl.id) });
    toast('Predefinido borrado');
    navigate({ name: 'tournamentNew', selected });
  };

  return (
    <ScreenFrame
      title={existing ? 'Editar tipo de torneo' : 'Crear tipo de torneo'}
      onBack={() => navigate({ name: 'tournamentNew', templateId: existing?.id ?? baseId, selected })}
      footer={
        <>
          <span className="notice" style={{ marginRight: 'auto', maxWidth: 470 }}>{describeTemplate(tpl)}</span>
          {existing && (
            <button className="btn btn-danger btn-sm" onClick={() => setConfirmDelete(true)}>
              Borrar
            </button>
          )}
          <button className="btn btn-primary btn-lg" disabled={!tpl.name.trim()} onClick={save}>
            Guardar predefinido
          </button>
        </>
      }
    >
      <div className="tn-layout">
        <div className="tn-col scroll">
          <input className="input" value={tpl.name} onChange={(e) => set('name', e.target.value)} aria-label="Nombre del predefinido" maxLength={24} placeholder="Nombre del predefinido" />
          <div className="label">Formato</div>
          <Seg<TournamentTemplate['format']> value={tpl.format} options={[['pool', 'POOL'], ['league', 'LIGUILLA'], ['bracket', 'CUADRO']]} onChange={setFormat} />
          <div className="dim" style={{ fontSize: 11 }}>
            {isPool
              ? 'Parejas que cambian en cada partido; puntúa cada jugador. Vale con número par o impar: si sobra alguien, descansa por turnos.'
              : tpl.format === 'league'
                ? 'Todos contra todos a una vuelta.'
                : 'Eliminación directa; los huecos pasan directos.'}
          </div>
          {isPool ? (
            <>
              <div className="label">Partidos por jugador</div>
              <Seg value={tpl.gamesPerPlayer} options={[[2, '2'], [3, '3'], [4, '4'], [5, '5'], [6, '6']]} onChange={(v) => set('gamesPerPlayer', v)} />
              <div className="dim" style={{ fontSize: 11 }}>Se ajusta al crear para que todos jueguen los mismos.</div>
            </>
          ) : (
            <>
              <div className="label">Equipos</div>
              <Seg<1 | 2> value={tpl.teamSize} options={[[1, '1 JUGADOR'], [2, 'PAREJAS FIJAS']]} onChange={(v) => set('teamSize', v)} />
            </>
          )}
          {(isPool || tpl.teamSize === 2) && (
            <>
              <div className="label">Parejas</div>
              <Seg<'elo' | 'random'> value={tpl.pairing} options={[['elo', 'EQUILIBRADAS'], ['random', 'AL AZAR']]} onChange={(v) => set('pairing', v)} />
            </>
          )}
          {tpl.format === 'bracket' && (
            <>
              <div className="label">Siembra</div>
              <Seg<'elo' | 'random'> value={tpl.seeding} options={[['elo', 'POR ELO'], ['random', 'SORTEO']]} onChange={(v) => set('seeding', v)} />
            </>
          )}
        </div>
        <div className="tn-col scroll">
          <div className="label">Partidos</div>
          <Seg<TournamentTemplate['endCondition']> value={tpl.endCondition} options={[['goals', 'GOLES'], ['time', 'TIEMPO'], ['both', 'AMBAS']]} onChange={(v) => set('endCondition', v)} />
          {tpl.endCondition !== 'time' && (
            <Seg value={tpl.goalsPerPeriod} options={[[3, '3'], [5, '5'], [7, '7'], [10, '10']]} onChange={(v) => set('goalsPerPeriod', v)} />
          )}
          {tpl.endCondition !== 'goals' && (
            <Seg value={tpl.minutesPerPeriod} options={[[3, '3 min'], [5, '5 min'], [8, '8 min']]} onChange={(v) => set('minutesPerPeriod', v)} />
          )}
          {finals.length > 0 && (
            <>
              <div className="label">Final</div>
              <Seg value={tpl.final} options={finals.map((f) => [f, FINAL_LABEL[f].toUpperCase()] as [TournamentFinal, string])} onChange={(v) => set('final', v)} />
            </>
          )}
          {hasFinal && (
            <>
              <div className="label">La final se juega</div>
              <Seg<1 | 3> value={tpl.finalBestOf} options={[[1, 'A 1 PARTIDO'], [3, 'AL MEJOR DE 3']]} onChange={(v) => set('finalBestOf', v)} />
              {tpl.endCondition !== 'time' && (
                <Seg<number | null>
                  value={tpl.finalGoals}
                  options={[[null, 'MISMOS GOLES'], [7, 'A 7'], [10, 'A 10']]}
                  onChange={(v) => set('finalGoals', v)}
                />
              )}
            </>
          )}
          <Toggle checked={tpl.ranked} onChange={(v) => set('ranked', v)} label="Cuenta para ELO" description="Los partidos se juegan como Clasificatorio." />
        </div>
      </div>
      {confirmDelete && (
        <Modal
          title="¿Borrar este predefinido?"
          onClose={() => setConfirmDelete(false)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setConfirmDelete(false)}>No</button>
              <button className="btn btn-danger" onClick={remove}>Borrar</button>
            </>
          }
        >
          <p style={{ margin: 0 }}>Los torneos ya creados con él no cambian.</p>
        </Modal>
      )}
    </ScreenFrame>
  );
}

export function TournamentDetailScreen({ id }: { id: string }) {
  const { navigate, tournaments, matches, players, saveTournament, prefs } = useApp();
  const [confirmCancel, setConfirmCancel] = useState(false);
  const t = tournaments.find((x) => x.id === id);
  if (!t) {
    return (
      <ScreenFrame title="Torneo" onBack={() => navigate({ name: 'tournament' })}>
        <div className="empty">Torneo no encontrado.</div>
      </ScreenFrame>
    );
  }
  const teamName = (tid: string | null) => (tid ? t.teams.find((x) => x.id === tid)?.name ?? '?' : '—');
  const nameOf = (pid: string) => players.find((p) => p.id === pid)?.name ?? '?';
  const byId = new Map(matches.map((m) => [m.id, m]));
  // En el Pool se juega en orden para respetar los descansos; en el resto, en cualquier orden.
  const playable = t.format === 'pool' ? playableFixtures(t).slice(0, 1) : playableFixtures(t);
  const winner = t.teams.find((x) => x.id === t.winnerTeamId);
  const finalFx = t.fixtures.find((f) => f.stage === 'final');

  const play = (f: Fixture) => {
    const participants = fixtureParticipants(t, f, players);
    const config: MatchConfig = { ...fixtureConfig(t, f), testMode: false, penaltyFirstTeam: prefs.penaltyFirstTeam };
    const extras = { tournament: { id: t.id, fixtureId: f.id } };
    if (config.mode === 'ranked') navigate({ name: 'prematch', config, participants, extras });
    else navigate({ name: 'match', config, participants, extras });
  };

  const fixtureRow = (f: Fixture) => {
    const series = (f.bestOf ?? 1) > 1;
    const game = (f.matchIds?.length ?? 0) + 1;
    return (
      <div key={f.id} className={`fixture ${f.winnerTeamId ? 'done' : ''}`}>
        <span className={`fx-team ${f.winnerTeamId === f.whiteTeamId ? 'win' : ''}`}>{teamName(f.whiteTeamId)}</span>
        <span className="fx-score">
          {f.bye ? 'pase' : fixtureScore(f, byId) ?? 'vs'}
          {series && <small className="fx-series">al mejor de {f.bestOf}</small>}
        </span>
        <span className={`fx-team right ${f.winnerTeamId === f.blueTeamId ? 'win' : ''}`}>{teamName(f.blueTeamId)}</span>
        {playable.includes(f) && (
          <button className="btn btn-primary btn-sm" onClick={() => play(f)}>
            ▶ {series && game > 1 ? `Partido ${game}` : 'Jugar'}
          </button>
        )}
      </div>
    );
  };

  const rounds = [...new Set(t.fixtures.map((f) => f.round))].sort((a, b) => a - b);
  const finalCard = finalFx && (
    <div className="final-card">
      <div className="label" style={{ color: 'var(--ranked)' }}>
        Final · {FINAL_LABEL[t.final ?? 'top2']}
        {(finalFx.bestOf ?? 1) > 1 ? ' · al mejor de 3' : ''}
      </div>
      {finalFx.whiteTeamId ? (
        fixtureRow(finalFx)
      ) : (
        <div className="dim" style={{ fontSize: 12 }}>Se decide al acabar la fase regular.</div>
      )}
    </div>
  );

  return (
    <ScreenFrame
      title={t.name}
      subtitle={`${FORMAT_LABEL[t.format]} · ${teamCountLabel(t)}${t.ranked ? ' · ELO' : ''}`}
      onBack={() => navigate({ name: 'tournament' })}
      right={
        t.status === 'active' && (
          <button className="btn btn-danger btn-sm" onClick={() => setConfirmCancel(true)}>
            Cancelar torneo
          </button>
        )
      }
    >
      {t.status === 'finished' && winner && (
        <div className="champion-banner">
          {prefs.effects !== 'off' && <Confetti count={30} />}
          <Trophy size={56} />
          <div>
            <div className="label" style={{ color: 'var(--ranked)' }}>Campeón</div>
            <div className="champion-name">{winner.name}</div>
          </div>
          <div className="champion-avatars">
            {winner.playerIds.map((pid) => {
              const p = players.find((x) => x.id === pid);
              return <Avatar key={pid} name={p?.name ?? '?'} photo={p?.photo} size={48} />;
            })}
          </div>
        </div>
      )}
      <div className="td-layout">
        {t.format === 'bracket' ? (
          <div className="td-panel bracket scroll">
            {rounds.map((r) => (
              <div key={r} className="bracket-round">
                <div className="label">{roundLabel(t, r)}</div>
                {t.fixtures
                  .filter((f) => f.round === r)
                  .map((f) => (
                    <div key={f.id} className={`bracket-match ${f.winnerTeamId ? 'done' : ''}`}>
                      <span className={f.winnerTeamId && f.winnerTeamId === f.whiteTeamId ? 'win' : ''}>{teamName(f.whiteTeamId)}</span>
                      <span className={f.winnerTeamId && f.winnerTeamId === f.blueTeamId ? 'win' : ''}>{f.bye && !f.blueTeamId ? 'pase directo' : teamName(f.blueTeamId)}</span>
                    </div>
                  ))}
              </div>
            ))}
          </div>
        ) : (
          <div className="td-panel scroll">
            <div className="label">{t.format === 'pool' ? 'Clasificación individual' : 'Clasificación'}</div>
            <table className="rep-table">
              <thead>
                <tr>
                  <th>#</th>
                  <th>{t.format === 'pool' ? 'Jugador' : 'Equipo'}</th>
                  <th>PJ</th>
                  <th>G</th>
                  <th>P</th>
                  <th>Dif</th>
                  <th>Pts</th>
                </tr>
              </thead>
              <tbody>
                {standings(t, matches).map((r, i) => (
                  <tr key={r.team.id} className={i === 0 && r.played > 0 ? 'leader' : ''}>
                    <td>{i + 1}</td>
                    <td>{r.team.name}</td>
                    <td>{r.played}</td>
                    <td>{r.wins}</td>
                    <td>{r.losses}</td>
                    <td>{r.diff > 0 ? `+${r.diff}` : r.diff}</td>
                    <td><strong>{r.points}</strong></td>
                  </tr>
                ))}
              </tbody>
            </table>
            {finalCard}
          </div>
        )}
        <div className="td-panel scroll">
          <div className="label">Partidos</div>
          {rounds
            .filter((r) => t.format === 'bracket' || !t.fixtures.some((f) => f.round === r && f.stage === 'final'))
            .map((r) => {
              const fx = t.fixtures.filter((f) => f.round === r && (f.whiteTeamId || f.blueTeamId));
              const resting = fx[0]?.resting ?? [];
              return (
                <div key={r}>
                  <div className="dim" style={{ fontSize: 11, margin: '6px 0 3px' }}>
                    {roundLabel(t, r)}
                    {resting.length > 0 && ` · descansa${resting.length > 1 ? 'n' : ''}: ${resting.map(nameOf).join(', ')}`}
                  </div>
                  {fx.map(fixtureRow)}
                </div>
              );
            })}
        </div>
      </div>
      {confirmCancel && (
        <Modal
          title="¿Cancelar el torneo?"
          onClose={() => setConfirmCancel(false)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setConfirmCancel(false)}>Seguir</button>
              <button
                className="btn btn-danger"
                onClick={async () => {
                  const cancelled: Tournament = { ...t, status: 'cancelled' };
                  await saveTournament(cancelled);
                  setConfirmCancel(false);
                }}
              >
                Cancelar torneo
              </button>
            </>
          }
        >
          <p style={{ margin: 0 }}>Los partidos ya jugados se conservan en el historial; el torneo queda sin campeón.</p>
        </Modal>
      )}
    </ScreenFrame>
  );
}
