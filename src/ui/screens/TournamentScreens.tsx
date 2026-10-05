/**
 * Torneos: lista, creación a partir de un predefinido, creador de predefinidos y detalle
 * (clasificación, cuadro o Pool rotativo, final y partidos).
 */
import { useState, type ReactNode } from 'react';
import { useApp } from '../../app/AppContext';
import type { MatchConfig } from '../../match-engine';
import type { Fixture, Tournament, TournamentFinal, TournamentTemplate } from '../../services/persistence';
import { initials, sortPlayers } from '../../services/players';
import { displayTitle } from '../../services/progression';
import { seededRandom } from '../../services/statistics/calendar';
import {
  FORMAT_LABEL,
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
  recommendedTemplateId,
  roundLabel,
  standings,
  templateFit,
  validateDraft,
  type TemplateFit,
} from '../../services/tournaments';
import { AssetImage } from '../components/assets';
import { Avatar, Modal, ScreenFrame, Toggle, formatDate } from '../components/common';
import { PlayerEditor } from '../components/PlayerEditor';
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

/** Fondo de las pantallas de torneo, como en el resto de pantallas de neón. */
const TN_BG = <AssetImage name="fondo-configuracion" className="select-bg is-on tn-bg" fallback={null} />;

function teamCountLabel(t: Tournament): string {
  return t.format === 'pool' ? `${t.entrants?.length ?? 0} jugadores` : `${t.teams.length} equipos · ${t.teamSize === 1 ? '1v1' : '2v2'}`;
}

export function TournamentListScreen() {
  const { navigate, tournaments } = useApp();
  const list = [...tournaments].sort((a, b) => b.createdAt - a.createdAt);
  return (
    <ScreenFrame
      title="Torneos"
      className="select-screen tn-screen"
      background={TN_BG}
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

/**
 * Crear un torneo en dos pasos: primero quién juega (fichas con foto) y después el tipo de torneo.
 * La lista de tipos se ordena según el número de jugadores: arriba los que se pueden jugar (con el
 * recomendado destacado) y abajo, apagados, los que no encajan y por qué.
 */
export function TournamentNewScreen({ templateId, initialSelected }: { templateId?: string; initialSelected?: string[] }) {
  const { navigate, players, progression, prefs, saveTournament, toast } = useApp();
  const [step, setStep] = useState<'players' | 'format'>(initialSelected?.length ? 'format' : 'players');
  const [selected, setSelected] = useState<string[]>(initialSelected ?? []);
  const [chosenTpl, setChosenTpl] = useState<string | undefined>(templateId);
  const [name, setName] = useState(`Torneo ${new Date().toLocaleDateString('es-ES', { day: 'numeric', month: 'short' })}`);
  // Semilla del sorteo: el reparto no cambia al redibujar, solo al pulsar «Sortear otra vez».
  const [seed, setSeed] = useState(() => String(Date.now()));
  const [creating, setCreating] = useState(false);
  const elo = (id: string) => progression?.players.get(id)?.elo ?? prefs.progression.eloInitial;
  const nameOf = (id: string) => players.find((p) => p.id === id)?.name ?? '?';
  const n = selected.length;

  // Tipos ordenados: los que encajan primero (el recomendado el primero de todos).
  const recommended = recommendedTemplateId(n);
  const fits = allTemplates(prefs.tournamentTemplates).map((t) => ({ t, fit: templateFit(t, n) }));
  const usable = fits.filter((x) => x.fit.ok).sort((a, b) => Number(b.t.id === recommended) - Number(a.t.id === recommended));
  const blocked = fits.filter((x) => !x.fit.ok);
  const tpl = usable.find((x) => x.t.id === chosenTpl)?.t ?? usable[0]?.t;

  const toggle = (id: string) => setSelected((s) => (s.includes(id) ? s.filter((x) => x !== id) : [...s, id]));

  if (step === 'players') {
    const available = sortPlayers(players.filter((p) => p.active));
    return (
      <ScreenFrame
        title="¿Quién juega?"
        subtitle="Nuevo torneo"
        className="select-screen tn-screen"
        background={TN_BG}
        onBack={() => navigate({ name: 'tournament' })}
        right={<button className="btn btn-sm" onClick={() => setCreating(true)}>+ Nuevo</button>}
        footer={
          <>
            <span className="tn-count" aria-live="polite">
              <b>{n}</b> {n === 1 ? 'jugador' : 'jugadores'}
            </span>
            <span className="notice select-status">
              {n < 3 ? `Marca al menos ${3 - n} más` : `${usable.length} tipo${usable.length === 1 ? '' : 's'} de torneo disponible${usable.length === 1 ? '' : 's'}`}
            </span>
            <button className="btn btn-sm" onClick={() => setSelected(n === available.length ? [] : available.map((p) => p.id))}>
              {n === available.length && n > 0 ? 'Quitar todos' : 'Todos'}
            </button>
            <button className="btn btn-primary btn-lg" disabled={n < 3} onClick={() => setStep('format')}>
              Siguiente
            </button>
          </>
        }
      >
        <div className="pick-grid tn-pick-grid scroll">
          {available.length === 0 ? (
            <div className="empty" style={{ gridColumn: '1 / -1' }}>
              <div>
                <strong>Sin jugadores</strong>
                Crea jugadores para montar un torneo.
              </div>
            </div>
          ) : (
            available.map((p) => {
              const on = selected.includes(p.id);
              const prog = progression?.players.get(p.id);
              const title = displayTitle(prog, p.titleId);
              return (
                <button
                  key={p.id}
                  className={`pick-card tn-pick ${on ? 'is-picked' : ''}`}
                  aria-pressed={on}
                  onClick={() => toggle(p.id)}
                  aria-label={`${on ? 'Quitar a' : 'Apuntar a'} ${p.name}`}
                >
                  <span className="pick-photo">
                    {p.photo ? <img src={p.photo} alt="" draggable={false} /> : <span className="pick-initials">{initials(p.name)}</span>}
                    {on && <span className="tn-check" aria-hidden="true">✓</span>}
                  </span>
                  <span className="pick-info">
                    <span className="pick-name">{p.name}</span>
                    <span className="pick-meta">{prog ? `Nv ${prog.level}${prog.rankedPlayed ? ` · ELO ${prog.elo}` : ''}` : '\u00a0'}</span>
                    {title && <span className="pick-title">{title}</span>}
                  </span>
                </button>
              );
            })
          )}
        </div>
        {creating && <PlayerEditor onClose={() => setCreating(false)} onSaved={(p) => setSelected((s) => [...s, p.id])} />}
      </ScreenFrame>
    );
  }

  const { draft, leftover } = tpl
    ? draftFromTemplate(tpl, name, selected, elo, { penaltyFirstTeam: prefs.penaltyFirstTeam }, seededRandom(seed))
    : { draft: null, leftover: [] as string[] };
  const errors = !tpl || !draft ? ['Ningún tipo de torneo encaja con estos jugadores.'] : leftover.length ? [`Sobra ${nameOf(leftover[0])}.`] : validateDraft(draft);
  const isPool = tpl?.format === 'pool';
  const usesDraw = !!tpl && (isPool ? tpl.pairing === 'random' : (tpl.teamSize === 2 && tpl.pairing === 'random') || (tpl.format === 'bracket' && tpl.seeding === 'random'));

  const create = async () => {
    if (!draft) return;
    try {
      const t = createTournament(draft, players, elo, Date.now(), seededRandom(`${seed}-cal`));
      await saveTournament(t);
      navigate({ name: 'tournamentDetail', id: t.id });
    } catch (err) {
      toast(err instanceof Error ? err.message : 'No se pudo crear');
    }
  };

  const tplCard = ({ t, fit }: { t: TournamentTemplate; fit: TemplateFit }) => (
    <button
      key={t.id}
      className={`tpl-card ${fit.ok ? '' : 'is-off'}`}
      aria-pressed={fit.ok && t.id === tpl?.id}
      disabled={!fit.ok}
      onClick={() => setChosenTpl(t.id)}
    >
      <strong>
        {t.name}
        {t.id === recommended && <span className="tpl-rec">Recomendado</span>}
        {!t.builtIn && <span className="tpl-mine">mío</span>}
      </strong>
      {fit.ok ? <span className="tpl-fit">{fit.summary}</span> : <span className="tpl-why">✕ {fit.reason}</span>}
      <span>{describeTemplate(t)}</span>
    </button>
  );

  let preview: ReactNode = null;
  if (tpl && draft) {
    preview = isPool ? (
      <div className="tn-plan">
        <strong>{templateFit(tpl, n).summary}</strong>
        <span>Las parejas cambian en cada partido y puntúa cada jugador.{draft.gamesPerPlayer !== tpl.gamesPerPlayer ? ` Ajustado a ${draft.gamesPerPlayer} partidos por jugador para que todos jueguen los mismos.` : ''}</span>
        {draft.final !== 'none' && <span>Final: {FINAL_LABEL[draft.final!]}{tpl.finalBestOf > 1 ? ', al mejor de 3' : ''}.</span>}
        <div className="tn-teams" style={{ marginTop: 4 }}>
          {selected.map((id) => <span key={id} className="tn-team">{nameOf(id)}</span>)}
        </div>
      </div>
    ) : (
      <div className="tn-plan">
        <strong>{templateFit(tpl, n).summary}</strong>
        <div className="tn-teams">
          {draft.teams.map((t, i) => (
            <span key={i} className="tn-team">{t.playerIds.map(nameOf).join(' + ')}</span>
          ))}
        </div>
      </div>
    );
  }

  return (
    <ScreenFrame
      title="Elige el torneo"
      subtitle={`${n} jugadores`}
      className="select-screen tn-screen"
      background={TN_BG}
      onBack={() => setStep('players')}
      footer={
        <>
          <span className={`notice select-status ${errors.length ? 'warn' : ''}`}>
            {errors[0] ?? (isPool ? `${n} jugadores listos` : `${draft?.teams.length ?? 0} equipos listos`)}
          </span>
          <button className="btn btn-primary btn-lg" disabled={errors.length > 0} onClick={create}>
            Crear torneo
          </button>
        </>
      }
    >
      <div className="tn-layout">
        <div className="tn-col">
          <div className="label">Tipos de torneo para {n} jugadores</div>
          <div className="tpl-list scroll">
            {usable.map(tplCard)}
            {blocked.length > 0 && <div className="tpl-sep">No encajan con {n} jugadores</div>}
            {blocked.map(tplCard)}
          </div>
          <div style={{ display: 'flex', gap: 6 }}>
            <button className="btn btn-ghost btn-sm" onClick={() => navigate({ name: 'tournamentTemplate', selected })}>
              + Crear tipo
            </button>
            {tpl && (
              <button
                className="btn btn-ghost btn-sm"
                onClick={() => navigate(tpl.builtIn ? { name: 'tournamentTemplate', baseId: tpl.id, selected } : { name: 'tournamentTemplate', templateId: tpl.id, selected })}
              >
                {tpl.builtIn ? 'Copiar y ajustar' : 'Editar tipo'}
              </button>
            )}
          </div>
        </div>
        <div className="tn-col">
          <div className="label">Nombre</div>
          <input className="input" value={name} onChange={(e) => setName(e.target.value)} aria-label="Nombre del torneo" maxLength={30} />
          <div className="label" style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            <span style={{ flex: 1 }}>
              {isPool ? 'Cómo se jugará' : `Equipos${tpl?.teamSize === 2 ? (tpl.pairing === 'elo' ? ' · parejas equilibradas' : ' · parejas al azar') : ''}`}
            </span>
            {usesDraw && (
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
      className="select-screen tn-screen"
      background={TN_BG}
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
      className="select-screen tn-screen"
      background={TN_BG}
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
