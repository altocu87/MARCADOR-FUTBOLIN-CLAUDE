/**
 * Torneos: lista, creación a partir de un predefinido, creador de predefinidos y detalle
 * (clasificación, cuadro o Pool rotativo, final y partidos).
 */
import { useEffect, useMemo, useRef, useState, type ReactNode } from 'react';
import { useApp } from '../../app/AppContext';
import type { MatchConfig } from '../../match-engine';
import { restoreSnapshot } from '../../app/recovery';
import type { ActiveMatchSnapshot, Fixture, Player, Tournament, TournamentFinal, TournamentFormat, TournamentTemplate } from '../../services/persistence';
import { NAME_MAX, findNameClash, initials, sortPlayers } from '../../services/players';
import { displayTitle, predict } from '../../services/progression';
import { seededRandom } from '../../services/statistics/calendar';
import {
  FORMAT_LABEL,
  allTemplates,
  competitions,
  editionName,
  competitionName,
  lastEdition,
  nextEdition,
  activeEdition,
  tournamentReport,
  createTournament,
  describeTemplate,
  templateAllowsDraw,
  templatePoints,
  draftFromTemplate,
  finalOptions,
  finalRulesOf,
  fixtureDone,
  finalSeriesEditable,
  isFinalFixture,
  recordedMatches,
  undoMatch,
  updateTournamentRules,
  hasTemplateFinal,
  withFinalRules,
  poolSettings,
  poolLimitText,
  LADDER_MINUTES,
  LADDER_ROUNDS,
  ladderClock,
  lineupOfRound,
  type MatchRules,
  rulesShort,
  fixtureConfig,
  fixtureParticipants,
  fixtureScore,
  newTemplate,
  fixtureProgress,
  nextFixture,
  roundLabel,
  standings,
  standingsMovement,
  templateFit,
  validateDraft,
  type Movement,
  type TemplateFit,
} from '../../services/tournaments';
import { AssetImage, assetUrl } from '../components/assets';
import { BannerArt, Crest, Cup, IconPicker, defaultBanner, iconCategories, defaultCup, defaultLogo } from '../components/Crest';
import { ClubEditor } from '../components/ClubEditor';
import { findClub, saveClub } from '../../services/clubs';
import { Avatar, Modal, ScreenFrame, Tabs, Toggle } from '../components/common';
import { NameClashNotice } from '../components/NameClash';
import { PlayerEditor } from '../components/PlayerEditor';
import { RankPhoto } from '../components/PlayerCards';
import { PlayerOptions, useLongPress } from '../components/PlayerQr';
import { newId } from '../../services/ids';
import { sound } from '../../services/sound/sound';
import { Confetti } from '../components/graphics';
import { TournamentCeremony } from '../components/TournamentCeremony';

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

const GOAL_OPTIONS: [number, string][] = [[3, 'A 3'], [5, 'A 5'], [7, 'A 7'], [10, 'A 10']];
const MINUTE_OPTIONS: [number, string][] = [[3, '3 MIN'], [5, '5 MIN'], [8, '8 MIN'], [10, '10 MIN']];

function pickRules(t: MatchRules): MatchRules {
  return { endCondition: t.endCondition, goalsPerPeriod: t.goalsPerPeriod, minutesPerPeriod: t.minutesPerPeriod };
}

/** «Gana quien llegue a 5 goles» / «2 partes de 5 minutos: gana quien lleve más goles» / … */
function rulesText(r: MatchRules): string {
  const g = `${r.goalsPerPeriod} goles`;
  const m = `2 partes de ${r.minutesPerPeriod} minutos`;
  if (r.endCondition === 'goals') return `Gana quien llegue a ${g}.`;
  if (r.endCondition === 'time') return `${m[0].toUpperCase()}${m.slice(1)}: gana quien lleve más goles.`;
  return `A ${g} o ${m}, lo que llegue antes.`;
}

/**
 * Cómo se acaba un partido, igual que en los partidos sueltos: «Goles» y «Tiempo» se marcan o desmarcan
 * por separado (las dos marcadas: lo que llegue antes) y cada una tiene su cantidad en su propia sección.
 * Siempre queda al menos una marcada.
 */
function RulesPicker({
  rules,
  onChange,
  timeIsParent,
  compact,
}: {
  rules: MatchRules;
  onChange: (patch: Partial<MatchRules>) => void;
  /** «Tiempo» abre más opciones al lado (en liga, qué pasa si acaban igualados). */
  timeIsParent?: boolean;
  /** Versión estrecha (dentro de un panel): cantidades sin «A» ni «MIN». */
  compact?: boolean;
}) {
  const { toast } = useApp();
  const goals = rules.endCondition !== 'time';
  const time = rules.endCondition !== 'goals';
  const toggle = (which: 'goals' | 'time') => {
    const g = which === 'goals' ? !goals : goals;
    const t = which === 'time' ? !time : time;
    if (!g && !t) return toast('Deja activa al menos una: goles o tiempo');
    onChange({ endCondition: g && t ? 'both' : g ? 'goals' : 'time' });
  };
  const way = (key: 'goals' | 'time', on: boolean, title: string, hint: string, amount: ReactNode) => (
    <div className={`rule-way ${on ? 'is-on' : ''}${on && key === 'time' && timeIsParent ? ' is-parent' : ''}`}>
      <button className="rule-way-toggle" aria-pressed={on} onClick={() => toggle(key)}>
        <span className="rule-way-check" aria-hidden="true">{on ? '✓' : ''}</span>
        <span className="rule-way-title">{title}</span>
        <small>{on ? hint : 'Toca para usarlo'}</small>
      </button>
      <div className="rule-way-amount" aria-disabled={!on}>
        {amount}
      </div>
    </div>
  );
  return (
    <div className={`rule-ways${compact ? ' is-compact' : ''}`}>
      {way(
        'goals',
        goals,
        '⚽ Goles',
        'A cuántos goles se gana',
        <Seg value={rules.goalsPerPeriod} options={compact ? GOAL_OPTIONS.map(([v]) => [v, String(v)] as [number, string]) : GOAL_OPTIONS} onChange={(v) => goals && onChange({ goalsPerPeriod: v })} />,
      )}
      {way(
        'time',
        time,
        '⏱ Tiempo',
        'Minutos de cada parte',
        <Seg value={rules.minutesPerPeriod} options={compact ? MINUTE_OPTIONS.map(([v]) => [v, `${v}'`] as [number, string]) : MINUTE_OPTIONS} onChange={(v) => time && onChange({ minutesPerPeriod: v })} />,
      )}
    </div>
  );
}

/** Cambiar las reglas de un torneo ya empezado: solo afectan a lo que queda por jugar. */
function RulesEditor({ t, onClose, onSave }: { t: Tournament; onClose: () => void; onSave: (t: Tournament) => Promise<void> }) {
  const hasFinal = t.fixtures.some((f) => isFinalFixture(t, f));
  const finalFx = t.fixtures.find((f) => isFinalFixture(t, f));
  const [rules, setRules] = useState<MatchRules>(() => pickRules(t.config));
  const [finalRules, setFinalRules] = useState<MatchRules>(() => pickRules(t.finalConfig ?? t.config));
  const [bestOf, setBestOf] = useState<1 | 3>((finalFx?.bestOf ?? 1) > 1 ? 3 : 1);
  const seriesEditable = finalSeriesEditable(t);
  return (
    <Modal
      title="Reglas del torneo"
      onClose={onClose}
      actions={
        <>
          <button className="btn btn-ghost" onClick={onClose}>Cancelar</button>
          <button
            className="btn btn-primary"
            onClick={() => void onSave(updateTournamentRules(t, rules, hasFinal ? finalRules : null, seriesEditable ? bestOf : undefined))}
          >
            Guardar
          </button>
        </>
      }
    >
      <div className="tn-rules-modal">
        <div className="tn-rules">
          <div className="label">Partidos</div>
          <RulesPicker rules={rules} onChange={(p) => setRules((r) => ({ ...r, ...p }))} />
        </div>
        {hasFinal && (
          <div className="tn-rules tn-rules-final">
            <div className="label">🏆 Final</div>
            {seriesEditable && (
              <div className="tn-rules-row">
                <Seg<1 | 3> value={bestOf} options={[[1, '1 PARTIDO'], [3, 'AL MEJOR DE 3']]} onChange={setBestOf} />
              </div>
            )}
            <RulesPicker rules={finalRules} onChange={(p) => setFinalRules((r) => ({ ...r, ...p }))} />
          </div>
        )}
        <small className="tn-rules-note">Solo cambia lo que queda por jugar; los partidos jugados se quedan como están.</small>
      </div>
    </Modal>
  );
}

/** Las reglas de una competición en iconos, para verlas de un vistazo en su tarjeta. */
function ruleChips(t: TournamentTemplate): { icon: string; text: string }[] {
  const out: { icon: string; text: string }[] = [];
  if (t.format === 'pool') {
    out.push(poolSettings(t).swap ? { icon: '🔄', text: 'Cambio de pareja' } : { icon: '👥', text: 'Parejas fijas' });
    out.push({ icon: poolSettings(t).limit === 'time' ? '⌛' : '🔢', text: poolLimitText(t) });
  } else out.push(t.teamSize === 2 ? { icon: '👥', text: 'Parejas' } : { icon: '👤', text: '1 vs 1' });
  if (t.endCondition !== 'time') out.push({ icon: '⚽', text: `A ${t.goalsPerPeriod}` });
  if (t.endCondition !== 'goals') out.push({ icon: '⏱', text: `${t.minutesPerPeriod}'` });
  if (t.format === 'league') {
    out.push({ icon: '🔁', text: (t.legs ?? 1) === 2 ? 'Ida y vuelta' : 'Solo ida' });
    const p = templatePoints(t);
    out.push({ icon: '🏅', text: templateAllowsDraw(t) ? `${p.win}-${p.draw}-${p.loss}` : `${p.win}-${p.loss}` });
    if (templateAllowsDraw(t)) out.push({ icon: '🤝', text: 'Empate' });
  }
  out.push({ icon: '🏆', text: hasTemplateFinal(t) ? (t.finalBestOf > 1 ? 'Final al mejor de 3' : 'Final') : 'Sin final' });
  if (t.ranked) out.push({ icon: '📈', text: 'ELO' });
  return out;
}

/** Reglas completas de una competición (ventana de la «?»). */
function RulesSummary({ tpl }: { tpl: TournamentTemplate }) {
  return (
    <div className="tn-summary">
      <div>
        <span className="label">Partidos</span>
        <span>{rulesText(tpl)}</span>
      </div>
      {tpl.format === 'pool' && (
        <>
          <div>
            <span className="label">Pistas</span>
            <span>Una pista por cada dos parejas: el que gana sube y el que pierde baja. Si sobra una pareja, espera en el banquillo.</span>
          </div>
          <div>
            <span className="label">Parejas</span>
            <span>
              {poolSettings(tpl).swap
                ? 'Con cambio de pareja: en cada pista, cada uno juega con uno de la otra pareja, por sorteo.'
                : 'Sin cambio de pareja: las mismas parejas todo el pool.'}
            </span>
          </div>
          <div>
            <span className="label">Duración</span>
            <span>
              {poolSettings(tpl).limit === 'time'
                ? `${poolSettings(tpl).minutes} minutos: cuando ya solo da tiempo a un partido, se juega la final en la pista 1.`
                : `${poolSettings(tpl).rounds} rondas: la pista 1 de la última ronda es la final.`}
            </span>
          </div>
        </>
      )}
      {tpl.format === 'league' && (
        <>
          <div>
            <span className="label">Empate</span>
            <span>{templateAllowsDraw(tpl) ? 'Se puede empatar: sin prórroga ni penaltis.' : 'Siempre hay ganador.'}</span>
          </div>
          <div>
            <span className="label">Puntos</span>
            <span>
              Victoria {templatePoints(tpl).win}
              {templateAllowsDraw(tpl) ? ` · empate ${templatePoints(tpl).draw}` : ''} · derrota {templatePoints(tpl).loss}
            </span>
          </div>
          <div>
            <span className="label">Vueltas</span>
            <span>{(tpl.legs ?? 1) === 2 ? 'Ida y vuelta: dos veces contra cada rival.' : 'Solo ida: una vez contra cada rival.'}</span>
          </div>
        </>
      )}
      <div>
        <span className="label">Último</span>
        <span>Se lleva el título «{tpl.lastTitle?.trim() || 'Patético'}».</span>
      </div>
      <div className={hasTemplateFinal(tpl) ? 'tn-summary-final' : ''}>
        <span className="label">🏆 Final</span>
        <span>
          {hasTemplateFinal(tpl)
            ? `${tpl.format === 'bracket' ? '' : tpl.format === 'pool' ? 'En la pista 1, ' : `${FINAL_LABEL[tpl.final]}, `}${tpl.finalBestOf > 1 ? 'al mejor de 3' : 'a un partido'}. ${rulesText(finalRulesOf(tpl))}`
            : 'Sin final: gana el primero de la clasificación.'}
        </span>
      </div>
      <div>
        <span className="label">ELO</span>
        <span>{tpl.ranked ? 'Cuenta para el ELO (se juega como Clasificatorio).' : 'No cuenta para el ELO.'}</span>
      </div>
    </div>
  );
}

/** Fondo de las pantallas de torneo, como en el resto de pantallas de neón. */
const TN_BG = <AssetImage name="fondo-configuracion" className="select-bg is-on tn-bg" fallback={null} />;

/** «5 oct 2026». */
function TrashIcon() {
  return (
    <svg width="20" height="20" viewBox="0 0 24 24" aria-hidden="true">
      <path d="M4 7h16M9 7V4.5h6V7M6.5 7l1 13h9l1-13M10 11v6M14 11v6" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  );
}

/** Confirmación para descartar un torneo en juego: se borra con todos sus partidos. */
function DiscardModal({ t, playedCount, onClose, onConfirm }: { t: Tournament; playedCount: number; onClose: () => void; onConfirm: () => Promise<void> }) {
  const [busy, setBusy] = useState(false);
  return (
    <Modal
      title="¿Descartar el torneo?"
      onClose={onClose}
      actions={
        <>
          <button className="btn btn-ghost" onClick={onClose}>No, seguir</button>
          <button
            className="btn btn-danger"
            disabled={busy}
            onClick={async () => {
              setBusy(true);
              await onConfirm();
            }}
          >
            🗑 Descartar
          </button>
        </>
      }
    >
      <p style={{ margin: 0 }}>
        <b>{t.name}</b> se borra{playedCount > 0 ? ` junto con sus ${playedCount} partido${playedCount === 1 ? '' : 's'} jugado${playedCount === 1 ? '' : 's'}` : ''}.
      </p>
      <p className="muted" style={{ margin: '8px 0 0', fontSize: 13 }}>
        Esos partidos desaparecen del historial y no cuentan para estadísticas, ELO, experiencia ni logros. No se puede deshacer.
      </p>
    </Modal>
  );
}

function shortDate(ts: number): string {
  return new Date(ts).toLocaleDateString('es-ES', { day: 'numeric', month: 'short', year: 'numeric' });
}

/** Flecha verde si sube, roja si baja, igual amarillo si se queda. */
function MoveIcon({ move }: { move?: Movement }) {
  if (!move) return null;
  const label = move === 'up' ? 'Sube' : move === 'down' ? 'Baja' : 'Se mantiene';
  return (
    <span className={`move move-${move}`} title={label} aria-label={label}>
      {move === 'up' ? '▲' : move === 'down' ? '▼' : '='}
    </span>
  );
}

function teamCountLabel(t: Tournament): string {
  return t.format === 'pool' ? `${t.entrants?.length ?? 0} jugadores` : `${t.teams.length} equipos · ${t.teamSize === 1 ? '1v1' : '2v2'}`;
}

/**
 * Torneos, en dos pestañas con una sola función cada una:
 * - En curso: los torneos empezados, para seguir jugando (o descartarlos).
 * - Palmarés: lo ya jugado; las competiciones con su campeón y los últimos torneos terminados.
 */
export function TournamentListScreen({ tab: initialTab }: { tab?: 'list' | 'honours' }) {
  const { navigate, tournaments, matches, players, discardTournament, toast } = useApp();
  const running = tournaments.filter((t) => t.status === 'active').sort((a, b) => b.createdAt - a.createdAt);
  // Sin torneos en curso se abre directamente el palmarés.
  const [tab, setTab] = useState<'list' | 'honours'>(initialTab ?? (running.length ? 'list' : 'honours'));
  const [discarding, setDiscarding] = useState<Tournament | null>(null);
  const played = tournaments.filter((t) => t.status !== 'active').sort((a, b) => (b.finishedAt ?? b.createdAt) - (a.finishedAt ?? a.createdAt));
  const comps = tab === 'honours' ? competitions(tournaments, matches, players).filter((c) => c.lastChampion) : [];
  const playerOf = (id: string) => players.find((p) => p.id === id);
  const faces = (ids: string[], size: number) => (
    <span className="tn-faces">
      {ids.map((id) => (
        <Avatar key={id} name={playerOf(id)?.name ?? '?'} photo={playerOf(id)?.photo} size={size} />
      ))}
    </span>
  );
  const open = (t: Tournament) => navigate({ name: 'tournamentDetail', id: t.id, from: 'list' });

  return (
    <ScreenFrame
      title="Torneos"
      className="select-screen tn-screen"
      background={TN_BG}
      onBack={() => navigate({ name: 'tournamentNew' })}
      right={
        <>
          <Tabs
            label="Vista"
            value={tab}
            onChange={setTab}
            tabs={[
              { id: 'list', label: running.length ? `En curso · ${running.length}` : 'En curso' },
              { id: 'honours', label: 'Palmarés' },
            ]}
          />
          <button className="btn btn-primary btn-sm" onClick={() => navigate({ name: 'tournamentNew' })}>
            + Nuevo torneo
          </button>
        </>
      }
    >
      {tab === 'list' ? (
        running.length === 0 ? (
          <div className="empty">
            <div style={{ maxWidth: 520 }}>
              <Trophy size={80} />
              <strong>No hay ningún torneo en curso</strong>
              Elige quién juega y la app te propone el torneo que mejor encaja.
              <div style={{ marginTop: 12, display: 'flex', gap: 8, justifyContent: 'center' }}>
                <button className="btn btn-primary" onClick={() => navigate({ name: 'tournamentNew' })}>
                  Crear torneo
                </button>
                {played.length > 0 && (
                  <button className="btn" onClick={() => setTab('honours')}>
                    Ver palmarés
                  </button>
                )}
              </div>
            </div>
          </div>
        ) : (
          <div className="tn-running scroll">
            {running.map((t) => {
              const pr = fixtureProgress(t);
              const nx = nextFixture(t);
              const side = (tid: string | null) => t.teams.find((x) => x.id === tid);
              const ids = [...new Set(t.entrants ?? t.teams.flatMap((x) => x.playerIds))];
              return (
                <div key={t.id} className="tn-run-card">
                  <button className="tn-run-main" onClick={() => open(t)}>
                    {t.logo ? <Crest id={t.logo} size={44} /> : <span className="tn-run-icon" aria-hidden="true">⚔</span>}
                    <span className="tn-run-body">
                      <b>{t.name}</b>
                      <small>
                        {FORMAT_LABEL[t.format]} · empezó el {shortDate(t.createdAt)}
                      </small>
                      <span className="tn-run-progress" aria-label={`${pr.done} de ${pr.total} partidos`}>
                        <span style={{ width: `${pr.total ? (pr.done / pr.total) * 100 : 0}%` }} />
                      </span>
                      <small>
                        {t.ladder ? `${pr.done} partidos jugados` : `${pr.done} de ${pr.total} partidos`}
                        {nx ? ` · siguiente: ${side(nx.whiteTeamId)?.name ?? '?'} vs ${side(nx.blueTeamId)?.name ?? '?'}` : ''}
                      </small>
                    </span>
                    {faces(ids.slice(0, 6), 34)}
                    {ids.length > 6 && <span className="tn-faces-more">+{ids.length - 6}</span>}
                  </button>
                  <button className="neon-go tn-resume-go" onClick={() => open(t)}>
                    ▶ CONTINUAR
                  </button>
                  <button className="tn-discard" onClick={() => setDiscarding(t)} aria-label={`Descartar ${t.name}`} title="Descartar torneo">
                    <TrashIcon />
                  </button>
                </div>
              );
            })}
          </div>
        )
      ) : played.length === 0 ? (
        <div className="empty">
          <div>
            <Trophy size={70} />
            <strong>Palmarés vacío</strong>
            Cuando termine el primer torneo, aquí saldrá su campeón.
          </div>
        </div>
      ) : (
        <div className="tn-honours scroll">
          {comps.length > 0 && (
            <>
              <div className="label">Competiciones</div>
              <div className="hon-grid">
                {comps.map((c) => {
                  const top = c.honours[0];
                  const last = c.lastChampion!;
                  return (
                    <button key={c.key} className={`hon-card${c.banner ? ' has-banner' : ''}`} onClick={() => navigate({ name: 'tournamentHonours', key: c.key })}>
                      <span className="hon-card-head">
                        <BannerArt id={c.banner} className="hon-card-banner" />
                        {c.logo ? <Crest id={c.logo} size={40} /> : null}
                        {c.cup ? <Cup id={c.cup} size={40} /> : <Trophy size={40} />}
                        <span>
                          <strong>{c.name}</strong>
                          <small>
                            {c.editions.length} edici{c.editions.length === 1 ? 'ón' : 'ones'}
                          </small>
                        </span>
                      </span>
                      <span className="hon-last">
                        <small>Último campeón · {shortDate(last.date)}</small>
                        <span className="tn-champ-line">
                          {faces(last.championIds, 28)}
                          <b>{last.champion}</b>
                        </span>
                      </span>
                      {top && top.titles > 1 && (
                        <span className="hon-top">
                          Más títulos: <b>{top.name}</b> · {top.titles} 🏆
                        </span>
                      )}
                    </button>
                  );
                })}
              </div>
            </>
          )}
          <div className="label" style={{ marginTop: comps.length ? 12 : 0 }}>Últimos torneos jugados</div>
          <div className="list">
            {played.map((t) => {
              const winner = t.teams.find((x) => x.id === t.winnerTeamId);
              return (
                <button key={t.id} className="row tn-row" onClick={() => open(t)}>
                  {t.cup && winner ? <Cup id={t.cup} size={34} /> : <span style={{ fontSize: 24 }} aria-hidden="true">{winner ? '🏆' : '✕'}</span>}
                  <span style={{ flex: 1, minWidth: 0 }}>
                    <strong>{t.name}</strong>
                    <div className="muted" style={{ fontSize: 12 }}>
                      {FORMAT_LABEL[t.format]} · {shortDate(t.finishedAt ?? t.createdAt)}
                    </div>
                  </span>
                  {winner ? (
                    <span className="tn-champ-line">
                      {faces(winner.playerIds, 30)}
                      <b>{winner.name}</b>
                    </span>
                  ) : (
                    <span className="badge">Cancelado</span>
                  )}
                </button>
              );
            })}
          </div>
        </div>
      )}
      {discarding && (
        <DiscardModal
          t={discarding}
          playedCount={fixtureProgress(discarding).done}
          onClose={() => setDiscarding(null)}
          onConfirm={async () => {
            await discardTournament(discarding.id);
            setDiscarding(null);
            toast('Torneo descartado');
          }}
        />
      )}
    </ScreenFrame>
  );
}

/** Palmarés de una competición: todas sus ediciones con campeón y MVP, y quién suma más títulos. */
export function TournamentHonoursScreen({ competitionKey }: { competitionKey: string }) {
  const { navigate, tournaments, matches, players } = useApp();
  const c = competitions(tournaments, matches, players).find((x) => x.key === competitionKey);
  if (!c) {
    return (
      <ScreenFrame title="Palmarés" onBack={() => navigate({ name: 'tournament', tab: 'honours' })}>
        <div className="empty">Competición no encontrada.</div>
      </ScreenFrame>
    );
  }
  const photoOf = (id: string) => players.find((p) => p.id === id)?.photo;
  return (
    <ScreenFrame
      title={
        <span className="td-title">
          {c.logo && <Crest id={c.logo} size={38} />}
          {c.name}
          {c.cup && <Cup id={c.cup} size={38} />}
        </span>
      }
      subtitle={`Palmarés · ${c.format} · ${c.editions.length} edici${c.editions.length === 1 ? 'ón' : 'ones'}`}
      className="select-screen tn-screen"
      background={TN_BG}
      headerArt={c.banner ? <BannerArt id={c.banner} className="screen-header-art" /> : undefined}
      onBack={() => navigate({ name: 'tournament', tab: 'honours' })}
    >
      <div className="td-layout">
        <div className="td-panel scroll">
          <div className="label">Ediciones</div>
          <div className="hon-editions">
            {c.editions.map((e) => (
              <button key={e.tournament.id} className="hon-edition" onClick={() => navigate({ name: 'tournamentDetail', id: e.tournament.id, from: 'list' })}>
                <span className="hon-ed-num">
                  {e.edition}
                  <small>ª</small>
                </span>
                <span className="hon-ed-body">
                  <small>
                    {e.tournament.status === 'finished' ? shortDate(e.date) : `En juego desde ${shortDate(e.tournament.createdAt)}`}
                  </small>
                  {e.champion ? <b>🏆 {e.champion}</b> : <b className="dim">Sin campeón todavía</b>}
                  {e.mvp && <span>MVP: {e.mvp.name}</span>}
                </span>
                <span className="hon-ed-avatars">
                  {e.championIds.map((id) => (
                    <Avatar key={id} name={players.find((p) => p.id === id)?.name ?? '?'} photo={photoOf(id)} size={34} />
                  ))}
                </span>
              </button>
            ))}
          </div>
        </div>
        <div className="td-panel scroll">
          <div className="label">Cuadro de honor</div>
          <table className="rep-table">
            <thead>
              <tr>
                <th>#</th>
                <th>Jugador</th>
                <th title="Títulos">🏆</th>
                <th title="Veces MVP">MVP</th>
                <th title="Ediciones jugadas">Ed.</th>
              </tr>
            </thead>
            <tbody>
              {c.honours.map((h, i) => (
                <tr key={h.playerId} className={i === 0 && h.titles > 0 ? 'leader' : ''}>
                  <td>{i + 1}</td>
                  <td>{h.name}</td>
                  <td><strong>{h.titles}</strong></td>
                  <td>{h.mvps}</td>
                  <td>{h.editions}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </ScreenFrame>
  );
}

/**
 * Crear un torneo en dos pasos: primero quién juega (fichas con foto) y después el tipo de torneo.
 * La lista de tipos se ordena según el número de jugadores: arriba los que se pueden jugar (con el
 * recomendado destacado) y abajo, apagados, los que no encajan y por qué.
 */
/**
 * Los tres tipos de torneo, tal y como se le enseñan al jugador en «¿Qué torneo jugamos?».
 * `noun`/`the` van en la pantalla siguiente («Elige la liga», «+ Crear copa»).
 */
const KINDS: {
  format: TournamentFormat;
  file: string;
  title: string;
  noun: string;
  the: string;
  none: string;
  /** «primera liga» / «primer pool», «creada» / «creado». */
  first: string;
  created: string;
  /** Color de neón del tipo (el mismo de su cartel). */
  color: string;
  desc: string;
}[] = [
  { format: 'league', file: 'liga', title: 'Liga', noun: 'liga', the: 'la liga', none: 'ninguna', first: 'primera', created: 'creada', color: '#62d6ff', desc: 'Todos contra todos. Gana quien más puntos sume; puede acabar con final.' },
  { format: 'bracket', file: 'copa', title: 'Copa', noun: 'copa', the: 'la copa', none: 'ninguna', first: 'primera', created: 'creada', color: '#f2c94c', desc: 'Eliminatoria: quien pierde, fuera, hasta llegar a la final.' },
  { format: 'pool', file: 'pool', title: 'Pool', noun: 'pool', the: 'el pool', none: 'ningún', first: 'primer', created: 'creado', color: '#d65cff', desc: 'Escalera de pistas: el que gana sube y el que pierde baja. La final, en la pista 1.' },
];

/** Igual que las tarjetas de modo del inicio: la elegida se apaga sola si nadie confirma, y la pulsada
 * se ve hundida un instante antes de entrar. */
const KIND_SELECT_TIMEOUT_MS = 8000;
const KIND_PRESS_MS = 180;

export function TournamentNewScreen({
  templateId,
  initialSelected,
  initialStep,
  initialFormat,
}: {
  templateId?: string;
  initialSelected?: string[];
  initialStep?: 'players' | 'kind' | 'format';
  initialFormat?: TournamentFormat;
}) {
  const { navigate, players, progression, prefs, saveTournament, discardTournament, savePlayer, savePrefs, toast, tournaments } = useApp();
  // Dejar pulsado un jugador: sus opciones (QR del perfil…).
  const [options, setOptions] = useState<Player | null>(null);
  const longPress = useLongPress();
  // Torneo en juego que se quiere descartar (pide confirmación).
  const [discarding, setDiscarding] = useState<Tournament | null>(null);
  // Tipo de torneo (liga, copa o pool): el pedido, o el del predefinido con el que se vuelve del creador.
  const [kind, setKind] = useState<TournamentFormat | undefined>(
    () => initialFormat ?? allTemplates(prefs.tournamentTemplates).find((t) => t.id === templateId)?.format,
  );
  const [step, setStep] = useState<'players' | 'kind' | 'format'>(
    initialStep ?? (initialSelected?.length ? (kind ? 'format' : 'kind') : 'players'),
  );
  const [picked, setSelected] = useState<string[]>(initialSelected ?? []);
  // Solo cuentan los jugadores que siguen existiendo y están en activo: al volver de un torneo
  // anterior la lista puede traer jugadores borrados, fusionados o dados de baja que no se ven
  // en pantalla y que inflarían la cuenta («hay 9» con 6 marcados).
  const selected = useMemo(() => {
    const ok = new Set(players.filter((p) => p.active).map((p) => p.id));
    return [...new Set(picked)].filter((id) => ok.has(id));
  }, [picked, players]);
  const [chosenTpl, setChosenTpl] = useState<string | undefined>(templateId);
  // Semilla del sorteo: el reparto no cambia al redibujar, solo al pulsar «Sortear otra vez».
  const [seed, setSeed] = useState(() => String(Date.now()));
  const [creating, setCreating] = useState(false);
  const [guestName, setGuestName] = useState<string | null>(null);
  // Jugador o invitado que ya tiene el nombre escrito.
  const [guestClash, setGuestClash] = useState<Player | null>(null);
  const guestRef = useRef<HTMLInputElement>(null);
  // Dos toques para entrar en un tipo, como en el inicio: el primero lo enciende y el segundo entra.
  const [kindSelected, setKindSelected] = useState<TournamentFormat | null>(null);
  const [kindPressed, setKindPressed] = useState<TournamentFormat | null>(null);
  const kindTimer = useRef<number | undefined>(undefined);
  useEffect(() => () => window.clearTimeout(kindTimer.current), []);
  useEffect(() => {
    if (!kindSelected) return;
    const id = window.setTimeout(() => setKindSelected(null), KIND_SELECT_TIMEOUT_MS);
    return () => window.clearTimeout(id);
  }, [kindSelected]);
  // Los predefinidos de este tipo que no encajan con estos jugadores, plegados.
  const [showBlocked, setShowBlocked] = useState(false);
  // Reglas completas de una competición («?» de su tarjeta).
  const [rulesOf, setRulesOf] = useState<TournamentTemplate | null>(null);
  // Nombre, logo y copa del torneo (por defecto, los del tipo elegido y su última edición).
  // Nombre y logo de las parejas fijas (por pareja de jugadores); se guardan como equipos al crear.
  const [teamEdits, setTeamEdits] = useState<Record<string, { name: string; logo: string }>>({});
  const [editingTeam, setEditingTeam] = useState<string[] | null>(null);
  // Parejas de esta edición: se revisan (o se forman a mano) en una ventana al pulsar «Continuar».
  const [pairsOpen, setPairsOpen] = useState(false);
  const [manualPairs, setManualPairs] = useState<string[][]>([]);
  const [pickA, setPickA] = useState<string | null>(null);
  const elo = (id: string) => progression?.players.get(id)?.elo ?? prefs.progression.eloInitial;
  const nameOf = (id: string) => players.find((p) => p.id === id)?.name ?? '?';
  const n = selected.length;

  // Primero se elige el tipo (liga, copa o pool) y luego, dentro de él, cuál de los creados.
  // Los que no encajan con este número de jugadores, plegados al final.
  const allFits = allTemplates(prefs.tournamentTemplates).map((t) => ({ t, fit: templateFit(t, n) }));
  const kindInfo = KINDS.find((k) => k.format === kind);
  const fits = allFits.filter((x) => x.t.format === kind);
  const shown = fits.filter((x) => x.fit.ok);
  const blocked = fits.filter((x) => !x.fit.ok);
  // Las reglas (goles, tiempo, final) son las del tipo elegido: aquí solo se resumen y se cambian con «Editar».
  // Una competición con una edición en juego no se puede volver a empezar hasta que esa acabe o se descarte.
  const playing = (id: string) => activeEdition(id, tournaments);
  const free = shown.filter((x) => !playing(x.t.id));
  const tpl = free.find((x) => x.t.id === chosenTpl)?.t ?? free[0]?.t;

  const toggle = (id: string) => setSelected((s) => (s.includes(id) ? s.filter((x) => x !== id) : [...s, id]));

  // Invitado: solo el nombre. Si ya existe alguien con ese nombre, se pregunta si es la misma persona.
  const addGuest = async () => {
    const clean = (guestName ?? '').trim();
    if (!clean) return;
    // No puede haber dos con el mismo nombre (ni jugador ni invitado): se pregunta qué hacer.
    const same = findNameClash(clean, players);
    if (same) {
      setGuestClash(same);
      return;
    }
    const now = Date.now();
    const guest = { id: newId('p'), name: clean, guest: true, active: true, createdAt: now, updatedAt: now };
    await savePlayer(guest);
    setSelected((s) => [...s, guest.id]);
    setGuestName(null);
  };

  // «Es la misma persona»: se apunta al que ya existe (si estaba de baja, se reactiva).
  const useExisting = async () => {
    if (!guestClash) return;
    if (!guestClash.active) await savePlayer({ ...guestClash, active: true, updatedAt: Date.now() });
    setSelected((s) => (s.includes(guestClash.id) ? s : [...s, guestClash.id]));
    toast(`${guestClash.name} apuntado`);
    setGuestClash(null);
    setGuestName(null);
  };

  if (step === 'players') {
    // Los invitados, al final de la lista.
    const available = sortPlayers(players.filter((p) => p.active)).sort((a, b) => Number(!!a.guest) - Number(!!b.guest));
    const runningList = tournaments.filter((t) => t.status === 'active').sort((a, b) => b.createdAt - a.createdAt);
    const running = runningList.length;
    return (
      <ScreenFrame
        title="¿Quién juega?"
        subtitle="Torneo"
        className="select-screen tn-screen"
        background={TN_BG}
        onBack={() => navigate({ name: 'home' })}
        right={
          <>
            <button className="btn btn-sm" onClick={() => navigate({ name: 'tournament', tab: running ? 'list' : 'honours' })}>
              {running ? `En curso · ${running}` : 'Palmarés'}
            </button>
            <button className="btn btn-sm" onClick={() => setCreating(true)}>+ Jugador</button>
            <button className="btn btn-sm tn-guest-btn" onClick={() => setGuestName('')}>+ Invitado</button>
          </>
        }
        footer={
          <>
            <span className="tn-count" aria-live="polite">
              <b>{n}</b> {n === 1 ? 'jugador' : 'jugadores'}
            </span>
            <span className="notice select-status">{n < 3 ? `Marca al menos ${3 - n} más` : `${n} jugadores listos`}</span>
            <button className="btn btn-sm" onClick={() => setSelected(n === available.length ? [] : available.map((p) => p.id))}>
              {n === available.length && n > 0 ? 'Quitar todos' : 'Todos'}
            </button>
            <button className="btn btn-primary btn-lg tn-continue" disabled={n < 3} onClick={() => setStep('kind')}>
              <span>Siguiente</span>
              <span className="tn-continue-arrow" aria-hidden="true">
                ▶
              </span>
            </button>
          </>
        }
      >
        {runningList.slice(0, 1).map((t) => {
          const nx = nextFixture(t);
          const pr = fixtureProgress(t);
          const teamOf = (tid: string | null) => t.teams.find((x) => x.id === tid)?.name ?? '?';
          return (
            <div key={t.id} className="tn-resume">
              <span className="tn-resume-tag">En juego</span>
              <span className="tn-resume-body">
                <b>{t.name}</b>
                <small>
                  {t.ladder ? `${pr.done} partidos jugados` : `${pr.done} de ${pr.total} partidos`}{nx ? ` · siguiente: ${teamOf(nx.whiteTeamId)} vs ${teamOf(nx.blueTeamId)}` : ''}
                  {running > 1 ? ` · y ${running - 1} más en «En curso»` : ''}
                </small>
              </span>
              <button className="neon-go tn-resume-go" onClick={() => navigate({ name: 'tournamentDetail', id: t.id })}>
                ▶ CONTINUAR
              </button>
              <button className="tn-discard" onClick={() => setDiscarding(t)} aria-label={`Descartar ${t.name}`} title="Descartar torneo">
                <TrashIcon />
              </button>
            </div>
          );
        })}
        {discarding && (
          <DiscardModal
            t={discarding}
            playedCount={fixtureProgress(discarding).done}
            onClose={() => setDiscarding(null)}
            onConfirm={async () => {
              await discardTournament(discarding.id);
              setDiscarding(null);
              toast('Torneo descartado');
            }}
          />
        )}
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
                  {...longPress(() => setOptions(p), () => toggle(p.id))}
                  aria-label={`${on ? 'Quitar a' : 'Apuntar a'} ${p.name}`}
                >
                  <span className="pick-photo">
                    {p.photo ? <img src={p.photo} alt="" draggable={false} /> : <span className="pick-initials">{initials(p.name)}</span>}
                    {on && <span className="tn-check" aria-hidden="true">✓</span>}
                  </span>
                  <span className="pick-info">
                    <span className="pick-name">{p.name}</span>
                    <span className="pick-meta">{prog && !p.guest ? `Nv ${prog.level}${prog.rankedPlayed ? ` · ELO ${prog.elo}` : ''}` : '\u00a0'}</span>
                    {p.guest ? <span className="pick-title tn-guest-tag">Invitado</span> : title && <span className="pick-title">{title}</span>}
                  </span>
                </button>
              );
            })
          )}
        </div>
        {creating && <PlayerEditor onClose={() => setCreating(false)} onSaved={(p) => setSelected((s) => [...s, p.id])} />}
        {options && <PlayerOptions player={options} onClose={() => setOptions(null)} />}
        {guestName !== null && (
          <Modal
            title="Añadir invitado"
            onClose={() => {
              setGuestName(null);
              setGuestClash(null);
            }}
            actions={
              <>
                <button
                  className="btn btn-ghost"
                  onClick={() => {
                    setGuestName(null);
                    setGuestClash(null);
                  }}
                >
                  Cancelar
                </button>
                <button className="btn btn-primary" disabled={!guestName.trim() || !!guestClash} onClick={addGuest}>
                  Aceptar
                </button>
              </>
            }
          >
            <input
              ref={guestRef}
              className="input"
              autoFocus
              value={guestName}
              maxLength={NAME_MAX}
              placeholder="Nombre del invitado"
              aria-label="Nombre del invitado"
              onChange={(e) => {
                setGuestName(e.target.value);
                setGuestClash(null);
              }}
              onKeyDown={(e) => e.key === 'Enter' && void addGuest()}
            />
            {guestClash && (
              <NameClashNotice
                clash={guestClash}
                sameLabel="Es la misma persona: apuntarlo"
                onSame={() => void useExisting()}
                onRename={() => {
                  setGuestClash(null);
                  guestRef.current?.focus();
                  guestRef.current?.select();
                }}
              />
            )}
            <p className="muted" style={{ margin: '8px 0 0', fontSize: 12 }}>
              Juega el torneo con su nombre y no sale en el ranking. Queda guardado para la próxima vez.
            </p>
          </Modal>
        )}
      </ScreenFrame>
    );
  }

  if (step === 'kind' || !kindInfo) {
    return (
      <ScreenFrame
        title="¿Qué torneo jugamos?"
        subtitle={`${n} jugadores`}
        className="select-screen tn-screen"
        background={TN_BG}
        onBack={() => setStep('players')}
      >
        <div className="tn-kinds">
          {KINDS.map((k) => {
            const total = allFits.filter((x) => x.t.format === k.format).length;
            const ok = allFits.filter((x) => x.t.format === k.format && x.fit.ok).length;
            const why = allFits.find((x) => x.t.format === k.format && !x.fit.ok)?.fit;
            // Cartel con el título dentro, en reposo (-off) y pulsado (-on); sin imágenes, la tarjeta de texto.
            const art = !!assetUrl(`tarjeta-${k.file}-off`);
            return (
              <button
                key={k.format}
                className={`tpl-card tn-kind tn-kind-${k.file} ${art ? 'has-art' : ''} ${ok || !total ? '' : 'is-off'}${kindSelected === k.format ? ' is-selected' : ''}${kindPressed === k.format ? ' is-pressed' : ''}`}
                aria-label={`${k.title}: ${k.desc}`}
                aria-pressed={kindSelected === k.format}
                onClick={() => {
                  if (kindPressed) return;
                  sound.unlock();
                  sound.play('ui');
                  if (kindSelected !== k.format) {
                    setKindSelected(k.format);
                    return;
                  }
                  setKindPressed(k.format);
                  kindTimer.current = window.setTimeout(() => {
                    setKind(k.format);
                    setShowBlocked(false);
                    setKindSelected(null);
                    setKindPressed(null);
                    setStep('format');
                  }, KIND_PRESS_MS);
                }}
              >
                {art ? (
                  <span className="tn-kind-art">
                    <AssetImage name={`tarjeta-${k.file}-off`} className="tn-kind-img" fallback={null} />
                    <AssetImage name={`tarjeta-${k.file}-on`} className="tn-kind-img tn-kind-img-on" fallback={null} />
                  </span>
                ) : (
                  <strong>{k.title}</strong>
                )}
                <span className="tn-kind-desc">{k.desc}</span>
                {!total ? (
                  <span className="tpl-fit">Sin crear todavía · toca para crear</span>
                ) : ok ? (
                  <span className="tpl-fit">
                    {ok} disponible{ok === 1 ? '' : 's'} para {n} jugadores
                  </span>
                ) : (
                  <span className="tpl-why">✕ {why && !why.ok ? why.reason : `Ninguna encaja con ${n} jugadores`}</span>
                )}
              </button>
            );
          })}
        </div>
      </ScreenFrame>
    );
  }

  // Ninguno creado de este tipo: cartel de neón que invita a crear el primero (en vez de una lista vacía).
  if (fits.length === 0) {
    const createFirst = () => navigate({ name: 'tournamentTemplate', selected, format: kindInfo.format });
    return (
      <ScreenFrame
        title={kindInfo.title}
        subtitle={`${n} jugadores`}
        className="select-screen tn-screen tn-empty-screen"
        background={
          <div className="select-bg is-on tn-empty-bg" style={{ ['--kc' as string]: kindInfo.color }}>
            {TN_BG}
            <AssetImage name={`tarjeta-${kindInfo.file}-on`} className="tn-empty-bg-art" fallback={null} />
            <span className="tn-empty-bg-glow" />
            <span className="tn-empty-bg-grid" />
          </div>
        }
        onBack={() => setStep('kind')}
      >
        <div className="tn-empty" style={{ ['--kc' as string]: kindInfo.color }}>
          <div className="tn-neon-sign" role="status">
            <span className="tn-neon-small">
              No hay {kindInfo.none} {kindInfo.noun} {kindInfo.created}
            </span>
            <span className="tn-neon-big">
              ¡Crea tu {kindInfo.first} {kindInfo.noun}!
            </span>
          </div>
          <button className="tn-neon-btn" onClick={createFirst}>
            + Crear {kindInfo.noun}
          </button>
          <small className="tn-empty-note">{kindInfo.desc}</small>
        </div>
      </ScreenFrame>
    );
  }

  // Nombre de la competición: el del tipo de torneo o uno propio. Cada nombre lleva sus ediciones.
  // Nombre, escudo, copa y banner son los del tipo de torneo: aquí solo se enseñan (se cambian con «Editar»).
  const compName = (tpl?.name ?? '').replace(/\s+/g, ' ');
  const compKey = tpl ? tpl.id : '';
  const edition = tpl ? nextEdition(compKey, tournaments) : 1;
  const prevEdition = tpl ? lastEdition(compKey, tournaments) : undefined;
  // Banner y copa: lo cambiado aquí, si no los del torneo, si no los de la edición anterior.
  const logo = tpl?.logo ?? prevEdition?.logo ?? defaultLogo(compName || 'torneo', tpl?.format);
  const cup = tpl?.cup ?? prevEdition?.cup ?? defaultCup();
  const banner = tpl?.banner ?? prevEdition?.banner ?? defaultBanner(compName || 'torneo', tpl?.format);
  const name = tpl ? editionName(compName.trim() || tpl.name, edition) : '';
  // Parejas elegidas por los jugadores: solo valen las que se formen aquí (no los equipos guardados).
  // El Pool también se juega por parejas: las de salida, al azar o elegidas aquí.
  const isPool = tpl?.format === 'pool';
  const swapPool = !!tpl && isPool && poolSettings(tpl).swap;
  const isManual = !!tpl && (isPool || tpl.teamSize === 2) && tpl.pairing === 'manual';
  // Con cambio de pareja, las parejas solo valen para el primer partido: los equipos guardados no cuentan.
  const fixedPairs = isManual ? manualPairs.filter((p) => p.every((id) => selected.includes(id))) : swapPool ? [] : prefs.clubs.map((c) => c.playerIds);
  const built = tpl ? draftFromTemplate(tpl, name, selected, elo, { penaltyFirstTeam: prefs.penaltyFirstTeam }, seededRandom(seed), fixedPairs) : null;
  // Parejas fijas: su equipo guardado o lo que se haya escrito aquí.
  const teamKey = (ids: string[]) => [...ids].sort().join('|');
  const teamInfo = (ids: string[]) => {
    const edit = teamEdits[teamKey(ids)];
    const club = findClub(prefs.clubs, ids);
    return edit ?? (club ? { name: club.name, logo: club.logo } : null);
  };
  const draft = built
    ? {
        ...built.draft,
        edition,
        logo,
        cup,
        ...(banner ? { banner } : {}),
        teams: built.draft.teams.map((tm) => {
          const info = tm.playerIds.length === 2 ? teamInfo(tm.playerIds) : null;
          return info ? { ...tm, name: info.name, logo: info.logo } : tm;
        }),
      }
    : null;
  const leftover = built?.leftover ?? [];
  const errors = !tpl || !draft
    ? [
        shown.length > 0
          ? `${shown.length === 1 ? `${kindInfo.the[0].toUpperCase()}${kindInfo.the.slice(1)} ya tiene` : kindInfo.format === 'pool' ? 'Todos tienen' : 'Todas tienen'} una edición en juego: termínala o descártala antes de empezar otra.`
          : `Ninguna ${kindInfo.noun} encaja con ${n} jugadores: crea una o elige otro tipo.`,
      ]
    : leftover.length
      ? [isManual ? `Faltan ${leftover.length} jugadores por emparejar.` : `Sobra ${nameOf(leftover[0])}.`]
      : validateDraft(draft);
  // Torneo por parejas (y el Pool): antes de crear se pasa por la ventana de parejas de esta edición.
  const withPairs = !!tpl && (isPool || tpl.teamSize === 2);
  // En la pantalla principal, con parejas a mano aún sin formar no es un error: se forman en la ventana.
  const mainErrors = isManual && !!draft && n % 2 === 0 ? [] : errors;

  const create = async () => {
    if (!draft) return;
    try {
      // Las parejas con nombre se guardan como equipos (o se actualizan) y el torneo las enlaza.
      let clubs = prefs.clubs;
      const teams = draft.teams.map((tm) => {
        const edit = tm.playerIds.length === 2 ? teamEdits[teamKey(tm.playerIds)] : undefined;
        const existing = tm.playerIds.length === 2 ? findClub(clubs, tm.playerIds) : undefined;
        if (edit) {
          const r = saveClub(clubs, tm.playerIds, edit.name, edit.logo, Date.now());
          clubs = r.clubs;
          return { ...tm, clubId: r.club.id };
        }
        return existing ? { ...tm, clubId: existing.id } : tm;
      });
      if (clubs !== prefs.clubs) await savePrefs({ ...prefs, clubs });
      const t = createTournament({ ...draft, teams }, players, elo, Date.now(), seededRandom(`${seed}-cal`));
      await saveTournament(t);
      navigate({ name: 'tournamentDetail', id: t.id });
    } catch (err) {
      toast(err instanceof Error ? err.message : 'No se pudo crear');
    }
  };

  // «Editar» abre el creador con este tipo cargado.
  const editTemplate = (t: TournamentTemplate) =>
    navigate({ name: 'tournamentTemplate', templateId: t.id, selected });
  // Una competición por tarjeta, a todo lo ancho: su cartel (banner, escudo, nombre, edición y copa) y
  // debajo sus reglas en iconos. Las reglas completas, con la «?»; cambiarlas, solo con «Editar».
  const tplCard = ({ t, fit }: { t: TournamentTemplate; fit: TemplateFit }) => {
    const on = fit.ok && t.id === tpl?.id;
    const prev = lastEdition(t.id, tournaments);
    const tName = t.name.replace(/\s+/g, ' ');
    const tLogo = t.logo ?? prev?.logo ?? defaultLogo(tName || 'torneo', t.format);
    const tCup = t.cup ?? prev?.cup ?? defaultCup();
    const tBanner = t.banner ?? prev?.banner ?? defaultBanner(tName || 'torneo', t.format);
    const live = playing(t.id);
    return (
      <div key={t.id} className={`comp-card${on ? ' is-on' : ''}${fit.ok && !live ? '' : ' is-off'}${live ? ' is-live' : ''}${tBanner ? ' has-banner' : ''}`}>
        <button className="comp-main" aria-pressed={on} disabled={!fit.ok || !!live} onClick={() => setChosenTpl(t.id)}>
          <BannerArt id={tBanner} className="comp-banner" />
          <span className="comp-art">
            <Crest id={tLogo} size={58} />
          </span>
          <span className="comp-body">
            <span className="comp-name">{tName}</span>
            <span className="comp-sub">
              {live ? (
                <b className="comp-live">▶ {live.edition ?? ''}{live.edition ? 'ª edición ' : ''}en juego · termínala o descártala para empezar otra</b>
              ) : (
                <>
                  <b>{nextEdition(t.id, tournaments)}ª edición</b>
                  {fit.ok ? ` · ${fit.summary?.split(' · ')[0]}` : ''}
                </>
              )}
            </span>
            {live ? null : fit.ok ? (
              <span className="comp-chips">
                {ruleChips(t).map((c) => (
                  <span key={c.text} className="comp-chip">
                    <i aria-hidden="true">{c.icon}</i>
                    {c.text}
                  </span>
                ))}
              </span>
            ) : (
              <span className="comp-why">✕ {fit.reason}</span>
            )}
          </span>
          <span className="comp-art">
            <Cup id={tCup} size={58} />
          </span>
        </button>
        <div className="comp-actions">
          <button className="comp-help" aria-label={`Reglas de ${tName}`} onClick={() => setRulesOf(t)}>
            ?
          </button>
          {live ? (
            <button className="btn btn-sm btn-primary comp-edit" onClick={() => navigate({ name: 'tournamentDetail', id: live.id })}>
              Continuar
            </button>
          ) : (
            <button className="btn btn-sm comp-edit" onClick={() => editTemplate(t)}>
              Editar
            </button>
          )}
        </div>
      </div>
    );
  };

  return (
    <ScreenFrame
      title={`Elige ${kindInfo.the}`}
      subtitle={`${n} jugadores`}
      className="select-screen tn-screen"
      background={TN_BG}
      onBack={() => setStep('kind')}
      right={
        <>
          <button className="btn btn-sm" onClick={() => navigate({ name: 'tournamentTemplate', selected, format: kindInfo.format })}>
            + Crear {kindInfo.noun}
          </button>
          {tpl && (
            <button className="btn btn-sm" onClick={() => navigate({ name: 'tournamentTemplate', baseId: tpl.id, selected })}>
              Duplicar
            </button>
          )}
        </>
      }
      footer={
        <>
          <span className={`notice select-status tn-status ${mainErrors.length ? 'warn' : ''}`}>
            {mainErrors[0] ?? `${n} jugadores${withPairs ? ' · ahora, las parejas' : ' listos'}`}
          </span>
          {blocked.length > 0 && (
            <button className="tpl-more tn-foot-more" onClick={() => setShowBlocked((v) => !v)}>
              {showBlocked ? 'Ocultar las que no encajan' : `Ver las que no encajan (${blocked.length})`}
            </button>
          )}
          <button className="btn btn-primary btn-lg tn-continue" disabled={mainErrors.length > 0} onClick={() => (withPairs ? setPairsOpen(true) : create())}>
            <span>Continuar</span>
            <span className="tn-continue-arrow" aria-hidden="true">
              ▶
            </span>
          </button>
        </>
      }
    >
      <div className="comp-list scroll">
        {shown.map(tplCard)}
        {shown.length === 0 && (
          <div className="tpl-empty">
            <span>{fits.length ? `Con ${n} jugadores no encaja ${kindInfo.none} ${kindInfo.noun} de las creadas.` : `Aún no has creado ${kindInfo.none} ${kindInfo.noun}.`}</span>
            <button className="btn btn-primary" onClick={() => navigate({ name: 'tournamentTemplate', selected, format: kindInfo.format })}>
              + Crear {kindInfo.noun}
            </button>
          </div>
        )}
        {showBlocked && (
          <>
            {shown.length > 0 && <div className="tpl-sep">No encajan con {n} jugadores</div>}
            {blocked.map(tplCard)}
          </>
        )}
      </div>
      {rulesOf && (
        <Modal
          title={rulesOf.name}
          onClose={() => setRulesOf(null)}
          actions={
            <button className="btn btn-primary" onClick={() => setRulesOf(null)}>
              Vale
            </button>
          }
        >
          <RulesSummary tpl={rulesOf} />
        </Modal>
      )}
      {pairsOpen && draft && (
        <Modal
          title={swapPool ? 'Parejas del primer partido' : `Parejas · ${edition}ª edición`}
          onClose={() => {
            setPairsOpen(false);
            setPickA(null);
          }}
          actions={
            <>
              <span className={`notice ${errors.length ? 'warn' : ''}`} style={{ marginRight: 'auto' }}>
                {errors[0] ?? `${draft.teams.length} parejas listas`}
              </span>
              {tpl?.pairing === 'random' && (
                <button className="btn btn-ghost" onClick={() => setSeed(String(Date.now()))}>
                  ↻ Sortear otra vez
                </button>
              )}
              <button className="btn btn-primary tn-continue tn-continue-sm" disabled={errors.length > 0} onClick={create}>
                <span>Crear torneo</span>
                <span className="tn-continue-arrow" aria-hidden="true">
                ▶
              </span>
              </button>
            </>
          }
        >
          {isManual && leftover.length > 0 && (
            <>
              <p className="muted" style={{ margin: '0 0 8px' }}>
                Toca dos jugadores para juntarlos en una pareja.
              </p>
              <div className="tn-pair-pick">
                {leftover.map((id) => {
                  const p = players.find((x) => x.id === id);
                  return (
                    <button
                      key={id}
                      className={`tn-pair-player ${pickA === id ? 'is-on' : ''}`}
                      aria-pressed={pickA === id}
                      onClick={() => {
                        if (!pickA) return setPickA(id);
                        if (pickA === id) return setPickA(null);
                        setManualPairs((ps) => [...ps, [pickA, id]]);
                        setPickA(null);
                      }}
                    >
                      <Avatar name={p?.name ?? '?'} photo={p?.photo} size={44} />
                      <span>{p?.name ?? '?'}</span>
                    </button>
                  );
                })}
              </div>
            </>
          )}
          <div className="tn-pair-list">
            {draft.teams.map((tm) => (
              <div key={tm.playerIds.join('|')} className="tn-pair">
                {/* Con cambio de pareja, la pareja solo dura un partido: ni nombre ni escudo. */}
                {!swapPool && <Crest id={tm.logo} size={30} />}
                <span className="tn-pair-names">
                  {!swapPool && <b>{tm.name ?? 'Sin nombre'}</b>}
                  <span>
                    {tm.playerIds.map((id) => {
                      const p = players.find((x) => x.id === id);
                      return (
                        <span key={id} className="tn-pair-who">
                          <Avatar name={p?.name ?? '?'} photo={p?.photo} size={24} />
                          {p?.name ?? '?'}
                        </span>
                      );
                    })}
                  </span>
                </span>
                {!swapPool && (
                  <button className="btn btn-sm btn-ghost" onClick={() => setEditingTeam(tm.playerIds)} aria-label="Poner nombre a la pareja">
                    ✎
                  </button>
                )}
                {isManual && (
                  <button
                    className="btn btn-sm btn-ghost"
                    aria-label="Deshacer la pareja"
                    onClick={() => setManualPairs((ps) => ps.filter((p) => teamKey(p) !== teamKey(tm.playerIds)))}
                  >
                    ✕
                  </button>
                )}
              </div>
            ))}
          </div>
        </Modal>
      )}
      {editingTeam && (
        <ClubEditor
          fixedPlayers={editingTeam}
          initial={teamInfo(editingTeam)}
          onSave={(v) => setTeamEdits((e) => ({ ...e, [teamKey(editingTeam)]: { name: v.name, logo: v.logo } }))}
          onClose={() => setEditingTeam(null)}
        />
      )}
    </ScreenFrame>
  );
}

/** Ideas para el título del último (se puede escribir cualquier otro). */
const LAST_TITLES = ['Farolillo rojo', 'Cuchara de madera', 'Colista', 'El paquete', 'Patético'];

/** Pasos del creador de torneos, en orden. */
const ALL_CREATOR_STEPS = [
  { id: 'kind', label: 'Tipo' },
  { id: 'players', label: 'Jugadores' },
  { id: 'duration', label: 'Duración' },
  { id: 'rules', label: 'Partidos' },
  { id: 'points', label: 'Puntos' },
  { id: 'final', label: 'Final' },
  { id: 'name', label: 'Nombre' },
] as const;
type CreatorStep = (typeof ALL_CREATOR_STEPS)[number]['id'];
/** Pasos del creador para un formato: «Puntos» (vueltas y puntuación) solo en la liga; «Duración», solo en el Pool. */
const creatorSteps = (format: TournamentFormat) =>
  ALL_CREATOR_STEPS.filter((x) => (x.id !== 'points' || format === 'league') && (x.id !== 'duration' || format === 'pool'));

interface OptionItem<T> {
  v: T;
  title: string;
  /** Solo si el título no se entiende solo: sale una «?» que lo explica. */
  help?: string;
}

/**
 * Opciones en lista, pensadas para el dedo: cada una es un botón grande con su título y nada más (la elegida
 * se ilumina). Si alguna necesita explicación, lleva una «?» al lado.
 */
function OptionList<T>({
  value,
  options,
  onChange,
  parentOf,
}: {
  value: T;
  options: OptionItem<T>[];
  onChange: (v: T) => void;
  /** Opciones que abren más opciones (al lado): elegidas, se unen con ellas por una pestaña. */
  parentOf?: T[];
}) {
  const [help, setHelp] = useState<string | null>(null);
  return (
    <div className="cr-opts" role="radiogroup">
      {options.map((o) => (
        <div key={String(o.v)} className={`cr-opt-wrap${parentOf?.includes(o.v) && value === o.v ? ' is-parent' : ''}`}>
          <button className="cr-opt" role="radio" aria-checked={value === o.v} onClick={() => onChange(o.v)}>
            {o.title}
          </button>
          {o.help && (
            <button
              className={`cr-help ${help === o.title ? 'is-open' : ''}`}
              aria-label={`¿Qué es «${o.title}»?`}
              onClick={() => setHelp(help === o.title ? null : o.title)}
            >
              ?
            </button>
          )}
          {o.help && help === o.title && (
            // Encima de todo y centrada (que no la corte el borde de la lista); un toque en cualquier sitio la cierra.
            <div className="cr-help-layer" onClick={() => setHelp(null)}>
              <div className="cr-help-bubble" role="note">
                <b>{o.title}</b>
                <span>{o.help}</span>
              </div>
            </div>
          )}
        </div>
      ))}
    </div>
  );
}

/** Puntos de un resultado, con − y + grandes. */
function PointsStepper({ label, value, onChange }: { label: string; value: number; onChange: (v: number) => void }) {
  return (
    <div className="cr-stepper">
      <span>{label}</span>
      <button className="btn" onClick={() => onChange(value - 1)} aria-label={`Menos puntos por ${label.toLowerCase()}`} disabled={value <= 0}>
        −
      </button>
      <b aria-live="polite">{value}</b>
      <button className="btn" onClick={() => onChange(value + 1)} aria-label={`Más puntos por ${label.toLowerCase()}`} disabled={value >= 9}>
        +
      </button>
    </div>
  );
}

/**
 * Opciones que dependen de otra («Cómo se forman las parejas» solo existe con «Parejas fijas»): van en un
 * panel enganchado a su opción madre, con su nombre arriba, para que se vea de quién cuelgan.
 */
function SubPanel({ parent, children }: { parent: string; children: ReactNode }) {
  return (
    <div className="cr-sub">
      <div className="cr-sub-head">
        <span aria-hidden="true">↳</span> {parent}
      </div>
      {children}
    </div>
  );
}

/** Una opción del creador: título, botones y una línea que explica lo elegido. */
function CreatorField({ label, children, note }: { label: string; children: ReactNode; note?: ReactNode }) {
  return (
    <div className="cr-field">
      <div className="label">{label}</div>
      {children}
      {note && <small className="cr-note">{note}</small>}
    </div>
  );
}

/**
 * Creador de torneos: un asistente por pasos (tipo → jugadores → partidos → final → nombre). Lo que se
 * guarda es un tipo de torneo reutilizable; los jugadores de cada edición se eligen al crearla.
 */
export function TournamentTemplateScreen({
  templateId,
  baseId,
  selected,
  format,
}: {
  templateId?: string;
  baseId?: string;
  selected?: string[];
  format?: TournamentFormat;
}) {
  const { navigate, prefs, savePrefs, toast } = useApp();
  const saved = prefs.tournamentTemplates;
  const existing = saved.find((x) => x.id === templateId);
  const [tpl, setTpl] = useState<TournamentTemplate>(() => {
    if (existing) return { ...existing };
    const base = saved.find((x) => x.id === baseId);
    return newTemplate(base, format);
  });
  // Si ya se sabe el tipo (se viene de «+ Crear liga») o se edita uno, se empieza por los jugadores.
  const [step, setStep] = useState<CreatorStep>(existing || baseId || format ? 'players' : 'kind');
  const [confirmDelete, setConfirmDelete] = useState(false);
  const [picker, setPicker] = useState<'logo' | 'cup' | 'banner' | null>(null);
  const set = <K extends keyof TournamentTemplate>(key: K, value: TournamentTemplate[K]) => setTpl((t) => ({ ...t, [key]: value }));
  const kind = KINDS.find((k) => k.format === tpl.format)!;
  const isPool = tpl.format === 'pool';
  const finals = finalOptions(tpl.format);
  const hasFinal = hasTemplateFinal(tpl);
  // La final «con sus propias reglas» solo si alguna difiere de las del resto de partidos.
  const ownFinalRules = tpl.finalEndCondition !== undefined || tpl.finalGoals !== null || tpl.finalMinutes !== undefined;
  const CREATOR_STEPS = creatorSteps(tpl.format);
  const idx = Math.max(0, CREATOR_STEPS.findIndex((x) => x.id === step));
  const last = idx === CREATOR_STEPS.length - 1;
  const n = selected?.length ?? 0;
  const fit = n ? templateFit(tpl, n) : null;

  const setFormat = (f: TournamentFormat) =>
    setTpl((t) => ({
      ...t,
      format: f,
      teamSize: f === 'pool' ? 2 : t.format === 'pool' ? 1 : t.teamSize,
      // En el Pool las parejas son al azar o elegidas (no equilibradas).
      pairing: f === 'pool' && t.pairing === 'elo' ? 'random' : t.pairing,
      final: finalOptions(f).includes(t.final) ? t.final : 'none',
    }));
  const setOwnFinal = (own: boolean) =>
    setTpl((t) => (own ? withFinalRules(t, finalRulesOf(t)) : { ...t, finalEndCondition: undefined, finalGoals: null, finalMinutes: undefined }));

  const back = () => navigate({ name: 'tournamentNew', templateId: existing?.id, selected, format: tpl.format });
  const save = async () => {
    const clean: TournamentTemplate = { ...tpl, name: tpl.name.trim(), builtIn: false };
    const list = existing ? saved.map((x) => (x.id === clean.id ? clean : x)) : [...saved, clean];
    await savePrefs({ ...prefs, tournamentTemplates: list });
    toast(existing ? 'Cambios guardados' : `«${clean.name}» guardado`);
    navigate({ name: 'tournamentNew', templateId: clean.id, selected, format: clean.format });
  };
  const remove = async () => {
    await savePrefs({ ...prefs, tournamentTemplates: saved.filter((x) => x.id !== tpl.id) });
    toast('Borrado');
    navigate({ name: 'tournamentNew', selected, format: tpl.format });
  };

  let body: ReactNode = null;
  if (step === 'kind') {
    body = (
      <div className="tn-kinds cr-kinds">
        {KINDS.map((k) => {
          const art = !!assetUrl(`tarjeta-${k.file}-off`);
          const on = tpl.format === k.format;
          return (
            <button
              key={k.format}
              className={`tpl-card tn-kind tn-kind-${k.file} ${art ? 'has-art' : ''}${on ? ' is-selected' : ''}`}
              aria-pressed={on}
              aria-label={`${k.title}: ${k.desc}`}
              onClick={() => setFormat(k.format)}
            >
              {art ? (
                <span className="tn-kind-art">
                  <AssetImage name={`tarjeta-${k.file}-off`} className="tn-kind-img" fallback={null} />
                  <AssetImage name={`tarjeta-${k.file}-on`} className="tn-kind-img tn-kind-img-on" fallback={null} />
                </span>
              ) : (
                <strong>{k.title}</strong>
              )}
              <span className="tn-kind-desc">{k.desc}</span>
            </button>
          );
        })}
      </div>
    );
  } else if (step === 'players') {
    const pairingOpts: OptionItem<TournamentTemplate['pairing']>[] = [
      { v: 'random', title: 'Al azar' },
      { v: 'manual', title: 'Las elegimos nosotros' },
      { v: 'elo', title: 'Equilibradas', help: 'Se junta a un jugador fuerte con uno flojo según su ELO, para que todas las parejas estén igualadas.' },
    ];
    const swap = poolSettings(tpl).swap;
    body = isPool ? (
      <div className="cr-cols">
        <div className="cr-col">
          <CreatorField label="Parejas">
            <OptionList<boolean>
              value={swap}
              options={[
                { v: true, title: 'Con cambio de pareja', help: 'En cada pista, las dos parejas se deshacen y cada uno juega con uno de la otra pareja, por sorteo.' },
                { v: false, title: 'Sin cambio de pareja', help: 'Las mismas parejas durante todo el pool: suben y bajan juntas.' },
              ]}
              onChange={(v) => set('poolSwap', v)}
            />
          </CreatorField>
        </div>
        <div className="cr-col">
          <CreatorField label={swap ? 'Parejas del primer partido' : 'Cómo se forman las parejas'} note="Hace falta un número par de jugadores.">
            <OptionList<TournamentTemplate['pairing']>
              value={tpl.pairing === 'manual' ? 'manual' : 'random'}
              options={[
                { v: 'random', title: 'Al azar' },
                { v: 'manual', title: 'Las elegimos nosotros' },
              ]}
              onChange={(v) => set('pairing', v)}
            />
          </CreatorField>
        </div>
      </div>
    ) : (
      <div className="cr-cols">
        <div className="cr-col">
          <CreatorField label="Quién juega">
            <OptionList<1 | 2>
              value={tpl.teamSize}
              options={[
                { v: 1, title: '1 contra 1' },
                { v: 2, title: 'Parejas fijas', help: 'Las mismas parejas durante todo el torneo. Hace falta un número par de jugadores.' },
              ]}
              onChange={(v) => set('teamSize', v)}
              parentOf={[2]}
            />
          </CreatorField>
          {tpl.format === 'bracket' && (
            <CreatorField label="Cruces">
              <OptionList<'elo' | 'random'>
                value={tpl.seeding}
                options={[
                  { v: 'elo', title: 'Por ELO', help: 'Los mejores no se cruzan hasta el final. Si faltan rivales, los mejores pasan directos a la siguiente ronda.' },
                  { v: 'random', title: 'Sorteo' },
                ]}
                onChange={(v) => set('seeding', v)}
              />
            </CreatorField>
          )}
        </div>
        {tpl.teamSize === 2 && (
          <div className="cr-col">
            <SubPanel parent="Parejas fijas">
              <CreatorField label="Cómo se forman las parejas" note="Los equipos ya guardados siempre juegan juntos.">
                <OptionList value={tpl.pairing} options={pairingOpts} onChange={(v) => set('pairing', v)} />
              </CreatorField>
            </SubPanel>
          </div>
        )}
      </div>
    );
  } else if (step === 'duration') {
    const p = poolSettings(tpl);
    body = (
      <div className="cr-cols">
        <div className="cr-col">
          <CreatorField label="Cuándo se acaba">
            <OptionList<'time' | 'rounds'>
              value={p.limit}
              options={[
                { v: 'time', title: 'Por tiempo', help: 'Cuando ya solo da tiempo a un partido, se juega la final en la pista 1.' },
                { v: 'rounds', title: 'Por rondas', help: 'En cada ronda se juega un partido en cada pista. En la última ronda, la pista 1 es la final.' },
              ]}
              onChange={(v) => set('poolLimit', v)}
              parentOf={['time', 'rounds']}
            />
          </CreatorField>
        </div>
        <div className="cr-col">
          {p.limit === 'time' ? (
            <SubPanel parent="Por tiempo">
              <CreatorField label="Minutos" note="El reloj arranca con el primer partido.">
                <Seg<number> value={p.minutes} options={LADDER_MINUTES.map((m) => [m, `${m} MIN`])} onChange={(v) => set('poolMinutes', v)} />
              </CreatorField>
            </SubPanel>
          ) : (
            <SubPanel parent="Por rondas">
              <CreatorField label="Rondas">
                <Seg<number> value={p.rounds} options={LADDER_ROUNDS.map((r) => [r, String(r)])} onChange={(v) => set('poolRounds', v)} />
              </CreatorField>
            </SubPanel>
          )}
        </div>
      </div>
    );
  } else if (step === 'rules') {
    const canDraw = tpl.format === 'league';
    body = (
      <div className={canDraw && tpl.endCondition !== 'goals' ? 'cr-cols cr-cols-rules' : 'cr-big'}>
        <div className="cr-col cr-big">
          <CreatorField label="Cómo se gana un partido" note={rulesText(tpl)}>
            <RulesPicker rules={tpl} onChange={(patch) => setTpl((t) => ({ ...t, ...patch }))} timeIsParent={canDraw} />
          </CreatorField>
        </div>
        {canDraw &&
          (tpl.endCondition === 'goals' ? (
            <small className="cr-note">A goles siempre hay ganador: no hay empates. Para poder empatar, activa también el tiempo.</small>
          ) : (
            <div className="cr-col">
              <SubPanel parent="⏱ Tiempo">
                <CreatorField label="Si acaban igualados">
                  <OptionList<boolean>
                    value={tpl.allowDraw !== false}
                    options={[
                      { v: true, title: 'Empate', help: 'Sin prórroga ni penaltis: cada uno se lleva los puntos del empate.' },
                      { v: false, title: 'Siempre hay ganador', help: 'Prórroga con gol de oro y, si sigue igualado, penaltis.' },
                    ]}
                    onChange={(v) => set('allowDraw', v)}
                  />
                </CreatorField>
              </SubPanel>
            </div>
          ))}
      </div>
    );
  } else if (step === 'points') {
    const pts = templatePoints(tpl);
    const draws = templateAllowsDraw(tpl);
    const setPts = (k: keyof typeof pts, v: number) => set('points', { ...pts, [k]: Math.max(0, Math.min(9, v)) });
    body = (
      <div className="cr-cols">
        <div className="cr-col">
          <CreatorField label="Vueltas">
            <OptionList<1 | 2>
              value={tpl.legs ?? 1}
              options={[
                { v: 1, title: 'Solo ida' },
                { v: 2, title: 'Ida y vuelta' },
              ]}
              onChange={(v) => set('legs', v)}
            />
          </CreatorField>
        </div>
        <div className="cr-col">
          <CreatorField label="Puntos por partido" note={draws ? undefined : 'Sin empates en esta liga (a goles, o con prórroga y penaltis).'}>
            <div className="cr-points">
              <PointsStepper label="Victoria" value={pts.win} onChange={(v) => setPts('win', v)} />
              {draws && <PointsStepper label="Empate" value={pts.draw} onChange={(v) => setPts('draw', v)} />}
              <PointsStepper label="Derrota" value={pts.loss} onChange={(v) => setPts('loss', v)} />
            </div>
          </CreatorField>
        </div>
      </div>
    );
  } else if (step === 'final') {
    body = (
      <div className="cr-cols">
        <div className="cr-col">
        {finals.length > 0 ? (
          <CreatorField label="¿Acaba con final?" note={tpl.final === 'none' ? 'Gana el primero de la clasificación.' : 'Al acabar la fase regular, la final decide el campeón.'}>
            <OptionList<TournamentFinal>
              value={tpl.final}
              options={finals.map((f) => ({ v: f, title: f === 'none' ? 'Sin final' : FINAL_LABEL[f] }))}
              onChange={(v) => set('final', v)}
              parentOf={finals.filter((f) => f !== 'none')}
            />
          </CreatorField>
        ) : isPool ? (
          <small className="cr-note">El pool siempre acaba con la final en la pista 1: la juegan los que estén arriba cuando llegue la hora.</small>
        ) : (
          <small className="cr-note">La copa siempre acaba en final: la juegan los dos que llegan.</small>
        )}
        </div>
        {hasFinal && (
          <div className="cr-col">
            <SubPanel parent={tpl.final === 'none' ? 'Final' : `Final · ${FINAL_LABEL[tpl.final]}`}>
              <Seg<1 | 3> value={tpl.finalBestOf} options={[[1, 'PARTIDO ÚNICO'], [3, 'AL MEJOR DE 3']]} onChange={(v) => set('finalBestOf', v)} />
              <Seg<boolean> value={ownFinalRules} options={[[false, 'REGLAS IGUALES'], [true, 'REGLAS DISTINTAS']]} onChange={setOwnFinal} />
              {ownFinalRules ? (
                // Justo debajo de «Reglas distintas», que es de quien dependen.
                <RulesPicker compact rules={finalRulesOf(tpl)} onChange={(patch) => setTpl((t) => withFinalRules(t, { ...finalRulesOf(t), ...patch }))} />
              ) : (
                <small className="cr-note">Como el resto: {rulesText(tpl).toLowerCase()}</small>
              )}
            </SubPanel>
          </div>
        )}
      </div>
    );
  } else {
    const crest = tpl.logo ?? defaultLogo(tpl.name || 'torneo', tpl.format);
    const cupId = tpl.cup ?? defaultCup();
    const bannerId = tpl.banner ?? defaultBanner(tpl.name || 'torneo', tpl.format);
    // Con los de un evento también (una copa no tiene banners propios, pero puede tener el de Halloween).
    const hasBanners = iconCategories('banner', tpl.format).length > 0;
    body = (
      <div className="cr-cols">
        <div className="cr-col">
        <CreatorField label={`Nombre ${kind.format === 'pool' ? 'del pool' : `de ${kind.the}`}`} note="Así se llamará cada edición: «1ª edición», «2ª edición»…">
          <input
            className="input cr-name"
            value={tpl.name}
            autoFocus
            onChange={(e) => set('name', e.target.value)}
            aria-label="Nombre del torneo"
            maxLength={24}
            placeholder={`Ej.: ${kind.format === 'league' ? 'Liga del Almacén' : kind.format === 'bracket' ? 'Copa del Viernes' : 'Pool de los Jueves'}`}
          />
        </CreatorField>
        <Toggle checked={tpl.ranked} onChange={(v) => set('ranked', v)} label="Cuenta para ELO" description="Los partidos se juegan como Clasificatorio y suben o bajan el ELO." />
        <div className="cr-summary">
          <div className="label">Resumen</div>
          <span>{describeTemplate(tpl)}</span>
          {fit && <span className={fit.ok ? 'tpl-fit' : 'tpl-why'}>{fit.ok ? `Con ${n} jugadores: ${fit.summary}` : `✕ Con ${n} jugadores: ${fit.reason}`}</span>}
        </div>
        </div>
        <div className="cr-col">
          <div className="cr-prizes">
            {hasBanners && (
              <button className="cr-banner" onClick={() => setPicker('banner')} aria-label="Elegir el banner">
                <BannerArt id={bannerId} />
                <span>Banner</span>
              </button>
            )}
            <button className="cr-prize" onClick={() => setPicker('logo')} aria-label="Elegir el escudo">
              <Crest id={crest} size={44} />
              <span>Escudo</span>
            </button>
            <button className="cr-prize" onClick={() => setPicker('cup')} aria-label="Elegir la copa">
              <Cup id={cupId} size={44} />
              <span>Copa</span>
            </button>
          </div>
          <CreatorField label="Título del último" note="Lo que se lleva el último en la ceremonia final.">
            <input
              className="input"
              value={tpl.lastTitle ?? ''}
              onChange={(e) => set('lastTitle', e.target.value || undefined)}
              aria-label="Título del último"
              maxLength={22}
              placeholder="Patético"
            />
            <div className="cr-chips">
              {LAST_TITLES.map((x) => (
                <button key={x} className="cr-chip" aria-pressed={tpl.lastTitle === x} onClick={() => set('lastTitle', x)}>
                  {x}
                </button>
              ))}
            </div>
          </CreatorField>
        </div>
        {picker && (
          <IconPicker
            kind={picker}
            value={picker === 'logo' ? crest : picker === 'banner' ? bannerId : cupId}
            title={picker === 'logo' ? 'Elige el escudo' : picker === 'banner' ? 'Elige el banner' : 'Elige la copa'}
            onPick={(v) => set(picker, v)}
            playerIds={selected ?? []}
            format={tpl.format}
            onClose={() => setPicker(null)}
          />
        )}
      </div>
    );
  }

  return (
    <ScreenFrame
      title={existing ? `Editar ${kind.noun}` : 'Crear torneo'}
      className="select-screen tn-screen cr-screen"
      background={TN_BG}
      onBack={back}
      right={
        <nav className="cr-steps" aria-label="Pasos">
          {CREATOR_STEPS.map((x, i) => (
            <button key={x.id} className={`cr-step${i === idx ? ' is-on' : ''}${i < idx ? ' is-done' : ''}`} aria-current={i === idx ? 'step' : undefined} onClick={() => setStep(x.id)}>
              <b>{i < idx ? '✓' : i + 1}</b>
              {x.id === 'kind' && i !== idx ? kind.title : x.label}
            </button>
          ))}
        </nav>
      }
      footer={
        <>
          {last ? <span className="select-status" /> : <span className="notice select-status">{describeTemplate(tpl)}</span>}
          {existing && (
            <button className="btn btn-danger btn-sm" onClick={() => setConfirmDelete(true)}>
              Borrar
            </button>
          )}
          {idx > 0 && (
            <button className="btn btn-lg" onClick={() => setStep(CREATOR_STEPS[idx - 1].id)}>
              Atrás
            </button>
          )}
          {last ? (
            <button className="btn btn-primary btn-lg tn-continue" disabled={!tpl.name.trim()} onClick={save}>
              <span>Guardar {kind.noun}</span>
              <span className="tn-continue-arrow" aria-hidden="true">
                ✓
              </span>
            </button>
          ) : (
            <button className="btn btn-primary btn-lg tn-continue" onClick={() => setStep(CREATOR_STEPS[idx + 1].id)}>
              <span>Siguiente</span>
              <span className="tn-continue-arrow" aria-hidden="true">
                ▶
              </span>
            </button>
          )}
        </>
      }
    >
      <div className={`cr-body cr-step-${step}${step === 'kind' ? ' cr-body-kinds' : ''}`}>{body}</div>
      {confirmDelete && (
        <Modal
          title={`¿Borrar «${tpl.name}»?`}
          onClose={() => setConfirmDelete(false)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setConfirmDelete(false)}>No</button>
              <button className="btn btn-danger" onClick={remove}>Borrar</button>
            </>
          }
        >
          <p style={{ margin: 0 }}>Los torneos ya jugados con él no cambian.</p>
        </Modal>
      )}
    </ScreenFrame>
  );
}

export function TournamentDetailScreen({
  id,
  view: initialView,
  from,
  ceremony: openCeremony,
}: {
  id: string;
  view?: 'report' | 'play';
  from?: 'list';
  /** Abrir el resumen final (al volver del partido que cierra el torneo). */
  ceremony?: boolean;
}) {
  const { navigate, tournaments, matches, players, saveTournament, discardTournament, deleteMatch, repos, prefs, progression, toast } = useApp();
  const [confirmCancel, setConfirmCancel] = useState(false);
  // Resumen final (campeón y reparto de XP y logros): se abre solo al acabar el torneo.
  const [ceremony, setCeremony] = useState(!!openCeremony);
  const [confirmUndo, setConfirmUndo] = useState(false);
  const [editRules, setEditRules] = useState(false);
  // Partido de este torneo que se quedó a medias (app cerrada o recargada): se puede seguir.
  const [pending, setPending] = useState<ActiveMatchSnapshot | null>(null);
  useEffect(() => {
    void repos.activeMatch.load().then((snap) => setPending(snap?.extras?.tournament?.id === id ? snap : null));
  }, [repos, id]);
  const t = tournaments.find((x) => x.id === id);
  // Terminado: se abre en la ficha; en juego: en los partidos.
  const [view, setView] = useState<'report' | 'play'>(initialView ?? (t?.status === 'finished' ? 'report' : 'play'));
  if (!t) {
    return (
      <ScreenFrame title="Torneo" onBack={() => navigate({ name: 'tournament' })}>
        <div className="empty">Torneo no encontrado.</div>
      </ScreenFrame>
    );
  }
  const teamName = (tid: string | null) => (tid ? t.teams.find((x) => x.id === tid)?.name ?? '?' : '—');
  // Nombre del equipo con su logo (si es un equipo guardado).
  const teamLabel = (tid: string | null) => {
    const tt = tid ? t.teams.find((x) => x.id === tid) : undefined;
    if (!tt?.logo) return teamName(tid);
    return (
      <span className="team-label">
        <Crest id={tt.logo} size={20} /> {tt.name}
      </span>
    );
  };
  const nameOf = (pid: string) => players.find((p) => p.id === pid)?.name ?? '?';
  const byId = new Map(matches.map((m) => [m.id, m]));
  // Se juega en orden: el propio torneo dice cuál es el siguiente partido.
  const next = nextFixture(t);
  const progress = fixtureProgress(t);
  const winner = t.teams.find((x) => x.id === t.winnerTeamId);
  const finalFx = t.fixtures.find((f) => f.stage === 'final');
  // Subidas y bajadas de puesto con el último partido.
  const movement = standingsMovement(t, matches);
  // Columna de empates solo en una liga donde se puede empatar.
  const hasDraws = t.format === 'league' && !!t.config.allowDraw;
  const allPlayerIds = [...new Set(t.entrants ?? t.teams.flatMap((x) => x.playerIds))];
  const back = () => (from === 'list' ? navigate({ name: 'tournament', tab: t.status === 'active' ? 'list' : 'honours' }) : navigate({ name: 'tournamentNew', selected: allPlayerIds, step: 'players' }));

  // Siempre pasa por la pantalla de Previsión (pronóstico, últimos enfrentamientos) antes de jugar.
  const play = (f: Fixture) => {
    const participants = fixtureParticipants(t, f, players);
    const config: MatchConfig = { ...fixtureConfig(t, f), testMode: false, penaltyFirstTeam: prefs.penaltyFirstTeam };
    navigate({ name: 'prematch', config, participants, extras: { tournament: { id: t.id, fixtureId: f.id } } });
  };

  const resume = () => {
    if (!pending) return;
    const state = restoreSnapshot(pending, Date.now());
    navigate({ name: 'match', config: state.config, participants: state.participants, resume: state, extras: pending.extras });
  };

  // Repetir el último partido: se borra su resultado y vuelve a quedar pendiente.
  const lastMatch = recordedMatches(t, matches).at(-1);
  const undoLast = async () => {
    if (!lastMatch) return;
    try {
      await saveTournament(undoMatch(t, lastMatch.id, matches));
      await deleteMatch(lastMatch.id);
      toast('Resultado borrado: el partido vuelve a estar pendiente');
    } catch {
      toast('No se pudo deshacer');
    }
    setConfirmUndo(false);
  };

  // Fotos pequeñas de un equipo en las filas de jornadas.
  const miniPhotos = (tid: string | null) => (
    <span className="fx-photos" aria-hidden="true">
      {(t.teams.find((x) => x.id === tid)?.playerIds ?? []).map((pid) => (
        <Avatar key={pid} name={nameOf(pid)} photo={players.find((p) => p.id === pid)?.photo} size={24} />
      ))}
    </span>
  );
  const rankOf = (id: string) => progression?.players.get(id)?.category.id ?? 'none';

  const fixtureRow = (f: Fixture) => {
    const series = (f.bestOf ?? 1) > 1;
    const isNext = f.id === next?.id;
    return (
      <div key={f.id} className={`fixture ${fixtureDone(f) ? 'done' : ''} ${f.draw ? 'is-draw' : ''} ${isNext ? 'is-next' : ''} ${f.skipped ? 'is-skipped' : ''}`}>
        <span className={`fx-team ${f.winnerTeamId === f.whiteTeamId ? 'win' : ''}`}>
          <i className="pre-dot white" aria-label="Blanco" />
          {miniPhotos(f.whiteTeamId)}
          <span className="fx-name">{teamLabel(f.whiteTeamId)}</span>
        </span>
        <span className="fx-score">
          {isNext && !f.series ? <span className="fx-next">SIGUIENTE</span> : f.bye ? 'pase' : f.skipped ? <small className="fx-skipped">no se jugó</small> : fixtureScore(f, byId) ?? 'vs'}
          {series && <small className="fx-series">al mejor de {f.bestOf}</small>}
        </span>
        <span className={`fx-team right ${f.winnerTeamId === f.blueTeamId ? 'win' : ''}`}>
          <span className="fx-name">{teamLabel(f.blueTeamId)}</span>
          {miniPhotos(f.blueTeamId)}
          <i className="pre-dot blue" aria-label="Azul" />
        </span>
      </div>
    );
  };

  // Tarjeta del siguiente partido: quién juega, quién descansa, pronóstico y un único botón.
  let nextCard: ReactNode = null;
  if (next) {
    const side = (tid: string | null) => t.teams.find((x) => x.id === tid)?.playerIds ?? [];
    const white = side(next.whiteTeamId);
    const blue = side(next.blueTeamId);
    const pr = progression ? predict(white, blue, matches, progression) : null;
    // En la escalera solo descansa el banquillo (los demás esperan su pista).
    const resting = next.resting ?? (t.ladder ? [] : allPlayerIds.filter((id) => !white.includes(id) && !blue.includes(id) && t.format === 'pool'));
    const game = (next.matchIds?.length ?? 0) + 1;
    // Con uno por lado, fotos grandes; en pareja, algo menores para que quepan las dos.
    const photoSize = Math.max(white.length, blue.length) > 1 ? 68 : 92;
    const teamBlock = (ids: string[], team: 'white' | 'blue') => {
      const tt = t.teams.find((x) => x.id === (team === 'white' ? next.whiteTeamId : next.blueTeamId));
      return (
        <div className={`next-side next-${team}`}>
          <span className="next-team-label">{team === 'white' ? 'BLANCO' : 'AZUL'}</span>
          <div className="next-avatars">
            {ids.map((pid) => {
              const photo = players.find((p) => p.id === pid)?.photo;
              const rank = rankOf(pid);
              // Con rango, su marco; sin él (invitados), la foto redonda.
              return rank !== 'none' ? (
                <RankPhoto key={pid} name={nameOf(pid)} photo={photo} rank={rank} size={photoSize} />
              ) : (
                <Avatar key={pid} name={nameOf(pid)} photo={photo} size={photoSize} />
              );
            })}
          </div>
          <b className="next-names">{ids.map(nameOf).join(' + ')}</b>
          {tt?.logo && (
            <span className="next-club">
              <Crest id={tt.logo} size={22} /> {tt.name}
            </span>
          )}
        </div>
      );
    };
    const wp = pr?.available ? pr.whitePct : null;
    const bp = pr?.available ? pr.bluePct : null;
    nextCard = (
      <div className="next-card">
        <div className="next-head">
          <span className="label">Siguiente partido</span>
          <span className="next-chips">
            {t.ladder && next.court && <span className="next-chip next-chip-court">{next.stage === 'final' ? '🏆 Final · pista 1' : `Pista ${next.court}`}</span>}
            <span className="next-chip">{roundLabel(t, next.round)}</span>
            <span className="next-chip">{t.ladder ? `Partido ${progress.done + 1}` : `Partido ${progress.done + 1} de ${progress.total}`}</span>
            {(next.bestOf ?? 1) > 1 && (
              <span className="next-chip">
                Serie {next.series?.white ?? 0}–{next.series?.blue ?? 0} · partido {game}
              </span>
            )}
            <span className="next-chip">⏱ {rulesShort(fixtureConfig(t, next))}</span>
          </span>
        </div>
        {next.abandonedAt && !pending && <div className="next-abandoned">⏸ Se dejó a medias · se empieza de cero</div>}
        <div className="next-teams">
          {teamBlock(white, 'white')}
          <span className="next-vs">VS</span>
          {teamBlock(blue, 'blue')}
        </div>
        {wp !== null && bp !== null && (
          <div className="next-odds" aria-label={`Pronóstico: Blanco ${wp} %, Azul ${bp} %`}>
            <b className="next-odds-w">{wp} %</b>
            <span className="next-odds-bar">
              <span className="next-odds-fill" style={{ width: `${wp}%` }} />
              <small>Pronóstico</small>
            </span>
            <b className="next-odds-b">{bp} %</b>
          </div>
        )}
        {resting.length > 0 && <div className="next-rest">{t.ladder ? 'Banquillo' : `Descansa${resting.length > 1 ? 'n' : ''}`}: {resting.map(nameOf).join(', ')}</div>}
        <button className="neon-go next-btn" onClick={() => play(next)}>
          ▶ SIGUIENTE PARTIDO
        </button>
      </div>
    );
  }

  const rounds = [...new Set(t.fixtures.map((f) => f.round))].sort((a, b) => a - b);

  // Pool en escalera: cada jugador con su foto (y si ha subido o bajado) y el nombre debajo.
  const ladderPlayer = (pid: string, moves?: Map<string, 'up' | 'down'>) => (
    <span key={pid} className="ladder-player">
      <span className="fx-photo">
        <Avatar name={nameOf(pid)} photo={players.find((p) => p.id === pid)?.photo} size={34} />
        {moves?.get(pid) && <i className={`lad-move ${moves.get(pid)}`}>{moves.get(pid) === 'up' ? '▲' : '▼'}</i>}
      </span>
      <span className="ladder-name">{nameOf(pid)}</span>
    </span>
  );
  // Una pista: sus dos parejas (la que ganó, en verde) y el marcador.
  const ladderCourt = (f: Fixture, moves?: Map<string, 'up' | 'down'>) => {
    const isNext = f.id === next?.id;
    const side = (tid: string | null) => (
      <span className={`ladder-pair ${f.winnerTeamId && f.winnerTeamId === tid ? 'win' : ''}`}>
        {(t.teams.find((x) => x.id === tid)?.playerIds ?? []).map((pid) => ladderPlayer(pid, moves))}
      </span>
    );
    return (
      <div key={f.id} className={`ladder-court ${f.court === 1 ? 'is-top' : ''} ${f.stage === 'final' ? 'is-final' : ''} ${isNext ? 'is-next' : ''} ${f.skipped ? 'is-skipped' : ''}`}>
        <span className="ladder-badge">{f.stage === 'final' ? '🏆 FINAL' : `PISTA ${f.court}`}</span>
        {side(f.whiteTeamId)}
        <span className="ladder-score">
          {isNext && !f.series ? <span className="fx-next">SIGUIENTE</span> : f.skipped ? <small className="fx-skipped">no se jugó</small> : fixtureScore(f, byId) ?? 'vs'}
          {(f.bestOf ?? 1) > 1 && <small className="fx-series">al mejor de {f.bestOf}</small>}
        </span>
        {side(f.blueTeamId)}
      </div>
    );
  };

  // Las pistas de la ronda en juego (o la última), con quién ha subido y quién ha bajado.
  let ladderPanel: ReactNode = null;
  if (t.ladder) {
    const round = next?.round ?? Math.max(...rounds);
    const courtOf = (r: number) => {
      const lu = lineupOfRound(t, r);
      const out = new Map<string, number>();
      lu.courts.forEach((c, i) => c.flat().forEach((pid) => out.set(pid, i + 1)));
      lu.bench?.forEach((pid) => out.set(pid, lu.courts.length + 1));
      return out;
    };
    const now = courtOf(round);
    const before = round > 1 ? courtOf(round - 1) : new Map<string, number>();
    const moves = new Map<string, 'up' | 'down'>();
    now.forEach((c, pid) => {
      const was = before.get(pid);
      if (was && was !== c) moves.set(pid, was > c ? 'up' : 'down');
    });
    const fx = t.fixtures.filter((f) => f.round === round).sort((a, b) => (a.court ?? 0) - (b.court ?? 0));
    const bench = fx[0]?.resting ?? [];
    const finalOn = t.fixtures.some((f) => f.stage === 'final');
    let clock = '';
    if (t.status === 'active') {
      if (finalOn) clock = '🏆 Toca la final';
      else if (t.ladder.limit === 'rounds') clock = `Ronda ${round} de ${t.ladder.rounds}`;
      else {
        const c = ladderClock(t, matches, Date.now());
        clock = c.started ? (c.remainingMs > 0 ? `⌛ Quedan ${Math.ceil(c.remainingMs / 60_000)} min` : '⌛ Tiempo cumplido') : `⌛ ${t.ladder.minutes} min`;
      }
    }
    ladderPanel = (
      <div className="td-panel scroll ladder-panel">
        <div className="ladder-head">
          <span className="label">Pistas · {roundLabel(t, round)}</span>
          {clock && <span className="ladder-clock">{clock}</span>}
        </div>
        {fx.map((f) => ladderCourt(f, moves))}
        {bench.length > 0 && (
          <div className="ladder-court ladder-bench">
            <span className="ladder-badge">BANQUILLO</span>
            <span className="ladder-pair">{bench.map((pid) => ladderPlayer(pid, moves))}</span>
          </div>
        )}
        <small className="cr-note ladder-note">
          {t.ladder.swap ? 'Con cambio de pareja: ' : 'Sin cambio de pareja: '}el que gana sube y el que pierde baja.
          {t.ladder.limit === 'time' && !finalOn && ' El reloj arranca con el primer partido; cuando ya solo dé para uno, se juega la final en la pista 1.'}
        </small>
      </div>
    );
  }
  const finalCard = finalFx && (
    <div className="final-card">
      <div className="label" style={{ color: 'var(--ranked)' }}>
        Final · {FINAL_LABEL[t.final ?? 'top2']}
        {(finalFx.bestOf ?? 1) > 1 ? ' · al mejor de 3' : ' · partido único'}
        {` · ${rulesShort(fixtureConfig(t, finalFx))}`}
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
      title={
        <span className="td-title">
          {t.logo && <Crest id={t.logo} size={38} />}
          {t.templateName && t.edition ? competitionName(t) : t.name}
        </span>
      }
      className="select-screen tn-screen"
      background={TN_BG}
      headerArt={t.banner ? <BannerArt id={t.banner} className="screen-header-art" /> : undefined}
      subtitle={t.templateName && t.edition ? `${t.edition}ª edición${t.ranked ? ' · ELO' : ''}` : `${FORMAT_LABEL[t.format]}${t.ranked ? ' · ELO' : ''}`}
      onBack={back}
      right={
        <>
          <Tabs
            label="Vista del torneo"
            value={view}
            onChange={setView}
            tabs={[
              { id: 'play', label: t.format === 'bracket' ? 'Cuadro' : t.ladder ? 'Pistas' : 'Clasificación' },
              { id: 'report', label: 'Ficha' },
            ]}
          />
          {t.status === 'active' && (
            <>
              <button
                className="btn btn-sm"
                onClick={() => {
                  toast('Torneo guardado: para seguir, entra en Torneo → Continuar');
                  navigate({ name: 'home' });
                }}
              >
                Guardar y salir
              </button>
              <button className="tn-discard tn-discard-sm" onClick={() => setConfirmCancel(true)} aria-label="Descartar torneo" title="Descartar torneo">
                <TrashIcon />
              </button>
            </>
          )}
        </>
      }
    >
      {view === 'report' ? (
        <TournamentReportView t={t} />
      ) : (
      <>
      {t.status === 'finished' && winner && (
        <div className="champion-banner">
          {prefs.effects !== 'off' && <Confetti count={30} />}
          {t.cup ? <Cup id={t.cup} size={60} /> : <Trophy size={56} />}
          <div>
            <div className="label" style={{ color: 'var(--ranked)' }}>Campeón</div>
            <div className="champion-name">
              {winner.logo && <Crest id={winner.logo} size={30} />} {winner.name}
            </div>
          </div>
          <div className="champion-avatars">
            {winner.playerIds.map((pid) => {
              const p = players.find((x) => x.id === pid);
              return <Avatar key={pid} name={p?.name ?? '?'} photo={p?.photo} size={48} />;
            })}
          </div>
          <button className="btn btn-sm champion-replay" onClick={() => setCeremony(true)}>
            🎬 Resumen
          </button>
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
                      <span className={f.winnerTeamId && f.winnerTeamId === f.whiteTeamId ? 'win' : ''}>{teamLabel(f.whiteTeamId)}</span>
                      <span className={f.winnerTeamId && f.winnerTeamId === f.blueTeamId ? 'win' : ''}>{f.bye && !f.blueTeamId ? 'pase directo' : teamLabel(f.blueTeamId)}</span>
                    </div>
                  ))}
              </div>
            ))}
          </div>
        ) : ladderPanel ? (
          ladderPanel
        ) : (
          <div className="td-panel scroll">
            <div className="label">{t.format === 'pool' ? 'Clasificación individual' : 'Clasificación'}</div>
            <table className="rep-table">
              <thead>
                <tr>
                  <th>#</th>
                  <th aria-label="Cambio de puesto" />
                  <th>{t.format === 'pool' ? 'Jugador' : 'Equipo'}</th>
                  <th>PJ</th>
                  <th>G</th>
                  {hasDraws && <th>E</th>}
                  <th>P</th>
                  <th>Dif</th>
                  <th>Pts</th>
                </tr>
              </thead>
              <tbody>
                {standings(t, matches).map((r, i) => (
                  <tr key={r.team.id} className={i === 0 && r.played > 0 ? 'leader' : ''}>
                    <td>{i + 1}</td>
                    <td>
                      <MoveIcon move={movement.get(r.team.id)} />
                    </td>
                    <td>{teamLabel(r.team.id)}</td>
                    <td>{r.played}</td>
                    <td>{r.wins}</td>
                    {hasDraws && <td>{r.draws}</td>}
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
          {pending && (
            <button className="td-resume" onClick={resume}>
              <span>⏸ Hay un partido a medias</span>
              <b>▶ CONTINUAR</b>
            </button>
          )}
          {nextCard}
          {(lastMatch || t.status === 'active') && (
            <div className="td-tools">
              {t.status === 'active' && (
                <button className="td-tool" onClick={() => setEditRules(true)}>
                  ⚙ Reglas
                </button>
              )}
              {lastMatch && (
                <button className="td-tool" onClick={() => setConfirmUndo(true)}>
                  ↶ Repetir último partido
                </button>
              )}
            </div>
          )}
          <div className="label" style={{ marginTop: nextCard ? 10 : 0 }}>{t.format === 'bracket' || t.format === 'pool' ? 'Rondas' : 'Jornadas'}</div>
          {rounds
            .filter((r) => t.format === 'bracket' || !!t.ladder || !t.fixtures.some((f) => f.round === r && f.stage === 'final'))
            .map((r) => {
              // En la escalera, de la pista 1 hacia abajo.
              const fx = t.fixtures
                .filter((f) => f.round === r && (f.whiteTeamId || f.blueTeamId))
                .sort((a, b) => (t.ladder ? (a.court ?? 0) - (b.court ?? 0) : 0));
              // Liguilla con número impar: el equipo que no juega esa jornada descansa.
              const resting =
                fx[0]?.resting ??
                (t.format === 'league' && !fx.some((f) => f.stage)
                  ? t.teams.filter((tt) => !fx.some((f) => f.whiteTeamId === tt.id || f.blueTeamId === tt.id)).flatMap((tt) => tt.playerIds)
                  : []);
              const state = fx.length && fx.every(fixtureDone) ? 'done' : fx.some((f) => f.id === next?.id) ? 'current' : 'pending';
              return (
                <div key={r} className={`round-block round-${state}`}>
                  <div className="round-head">
                    <b>{roundLabel(t, r)}</b>
                    <span>{state === 'done' ? 'Jugada' : state === 'current' ? 'En juego' : 'Pendiente'}</span>
                    {resting.length > 0 && <small>{t.ladder ? 'banquillo' : `descansa${resting.length > 1 ? 'n' : ''}`}: {resting.map(nameOf).join(', ')}</small>}
                  </div>
                  {fx.map((f) => (t.ladder ? ladderCourt(f) : fixtureRow(f)))}
                </div>
              );
            })}
        </div>
      </div>
      </>
      )}
      {confirmUndo && lastMatch && (
        <Modal
          title="¿Repetir el último partido?"
          onClose={() => setConfirmUndo(false)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setConfirmUndo(false)}>No</button>
              <button className="btn btn-danger" onClick={() => void undoLast()}>Borrar y repetir</button>
            </>
          }
        >
          <p style={{ margin: 0 }}>
            <b>
              {lastMatch.participants.filter((p) => p.team === 'white').map((p) => p.nameSnapshot).join(' + ')} {lastMatch.result.score.white}–
              {lastMatch.result.score.blue} {lastMatch.participants.filter((p) => p.team === 'blue').map((p) => p.nameSnapshot).join(' + ')}
            </b>
          </p>
          <p className="muted" style={{ margin: '8px 0 0', fontSize: 13 }}>
            Se borra este resultado (también de las estadísticas) y el partido vuelve a quedar pendiente para jugarlo otra vez.
          </p>
        </Modal>
      )}
      {editRules && (
        <RulesEditor
          t={t}
          onClose={() => setEditRules(false)}
          onSave={async (next) => {
            await saveTournament(next);
            setEditRules(false);
            toast('Reglas cambiadas para los partidos que quedan');
          }}
        />
      )}
      {confirmCancel && (
        <DiscardModal
          t={t}
          playedCount={progress.done}
          onClose={() => setConfirmCancel(false)}
          onConfirm={async () => {
            await discardTournament(t.id);
            setConfirmCancel(false);
            toast('Torneo descartado');
            // De vuelta a elegir jugadores, con los mismos ya marcados.
            navigate({ name: 'tournamentNew', selected: allPlayerIds, step: 'players' });
          }}
        />
      )}
      {ceremony && t.status === 'finished' && <TournamentCeremony t={t} onClose={() => setCeremony(false)} />}
    </ScreenFrame>
  );
}

/**
 * Ficha completa del torneo: campeón y MVP arriba; información, destacados y estadísticas
 * de cada jugador debajo. Los destacados abren el partido.
 */
function TournamentReportView({ t }: { t: Tournament }) {
  const { matches, players, navigate, prefs } = useApp();
  const r = tournamentReport(t, matches, players);
  const champ = t.teams.find((x) => x.id === t.winnerTeamId);
  const photo = (id: string) => players.find((p) => p.id === id)?.photo;
  const openMatch = (matchId: string) => navigate({ name: 'matchDetail', matchId, fromTournament: true });
  const anyScored = r.players.some((p) => p.scored > 0);
  // Columna de empates solo si los hubo (y entonces nunca se cuentan como derrota).
  const anyDraws = r.players.some((p) => p.draws > 0);
  const total = t.fixtures.filter((f) => !f.bye).length;
  const minutes = (ms: number) => `${Math.max(1, Math.round(ms / 60000))} min`;
  const sign = (n: number) => (n > 0 ? `+${n}` : String(n));

  const info: [string, string][] = [
    ['Competición', competitionName(t)],
    ['Edición', t.edition ? `${t.edition}ª` : '—'],
    ['Formato', `${FORMAT_LABEL[t.format]} · ${teamCountLabel(t)}`],
    ['Empezó', shortDate(r.startedAt ?? t.createdAt)],
    ['Terminó', t.status === 'finished' && r.finishedAt ? shortDate(r.finishedAt) : t.status === 'cancelled' ? 'Cancelado' : 'En juego'],
    ['Partidos', t.status === 'finished' ? String(r.matches.length) : `${r.matches.length} de ${total}`],
    ['Goles', `${r.totalGoals}${r.matches.length ? ` · ${r.avgGoals.toFixed(1)} por partido` : ''}`],
    ['Tiempo de juego', r.matches.length ? minutes(r.playTimeMs) : '—'],
    ['Prórroga o penaltis', String(r.extraTime)],
    ['Cuenta para ELO', t.ranked ? 'Sí' : 'No'],
  ];
  if (r.finalScore) info.splice(6, 0, ['Final', r.finalScore]);

  const highlights: { icon: string; label: string; text: string; matchId?: string }[] = [];
  if (r.topScorer) highlights.push({ icon: '⚽', label: 'Pichichi', text: `${r.topScorer.name} · ${r.topScorer.scored} goles` });
  if (r.biggestWin) highlights.push({ icon: '💥', label: 'Mayor goleada', text: r.biggestWin.label, matchId: r.biggestWin.match.id });
  if (r.highestScoring) highlights.push({ icon: '🔥', label: 'Partido con más goles', text: r.highestScoring.label, matchId: r.highestScoring.match.id });
  if (r.longest) highlights.push({ icon: '⏱', label: `Partido más largo · ${minutes(r.longest.match.result.totalTimeMs)}`, text: r.longest.label, matchId: r.longest.match.id });

  return (
    <div className="rp-layout scroll">
      <div className="rp-hero">
        <div className={`rp-champ ${champ ? '' : 'is-pending'}`}>
          {champ && prefs.effects !== 'off' && <Confetti count={24} />}
          <Trophy size={58} />
          <div className="rp-champ-body">
            <div className="label">{champ ? 'Campeón' : t.status === 'cancelled' ? 'Cancelado' : 'En juego'}</div>
            <div className="rp-champ-name">{champ?.name ?? 'Por decidir'}</div>
            {r.finalScore && <small>Final: {r.finalScore}</small>}
          </div>
          {champ && (
            <div className="rp-avatars">
              {champ.playerIds.map((id) => (
                <Avatar key={id} name={players.find((p) => p.id === id)?.name ?? '?'} photo={photo(id)} size={46} />
              ))}
            </div>
          )}
        </div>
        {r.mvp && (
          <div className="rp-mvp">
            <Avatar name={r.mvp.name} photo={photo(r.mvp.playerId)} size={52} />
            <div className="rp-mvp-body">
              <div className="label">★ MVP{t.status === 'finished' ? '' : ' provisional'}</div>
              <div className="rp-mvp-name">{r.mvp.name}</div>
              <small>
                {r.mvp.wins} G{r.mvp.draws ? ` · ${r.mvp.draws} E` : ''} · {r.mvp.losses} P{r.mvp.scored ? ` · ${r.mvp.scored} goles` : ''} · {sign(r.mvp.goalsFor - r.mvp.goalsAgainst)} · {r.mvp.mvp} pts
              </small>
            </div>
          </div>
        )}
      </div>
      <div className="rp-grid">
        <div className="rp-side">
          <div className="rp-card">
            <div className="label">Información</div>
            <dl className="rp-info">
              {info.map(([k, v]) => (
                <div key={k}>
                  <dt>{k}</dt>
                  <dd>{v}</dd>
                </div>
              ))}
            </dl>
          </div>
          {highlights.length > 0 && (
            <div className="rp-card">
              <div className="label">Destacados</div>
              {highlights.map((h) => (
                <button key={h.label} className="rp-hl" disabled={!h.matchId} onClick={() => h.matchId && openMatch(h.matchId)}>
                  <span aria-hidden="true">{h.icon}</span>
                  <span>
                    <small>{h.label}</small>
                    <b>{h.text}</b>
                  </span>
                </button>
              ))}
            </div>
          )}
        </div>
        <div className="rp-card">
          <div className="label">Jugadores</div>
          <table className="rep-table rp-table">
            <thead>
              <tr>
                <th>Jugador</th>
                <th>PJ</th>
                <th>G</th>
                {anyDraws && <th>E</th>}
                <th>P</th>
                {anyScored && <th title="Goles marcados">Goles</th>}
                <th title="Diferencia de goles de su equipo">Dif</th>
                <th>% vic.</th>
                <th title="Puntuación MVP">MVP</th>
              </tr>
            </thead>
            <tbody>
              {r.players.map((p, i) => (
                <tr key={p.playerId} className={i === 0 && r.mvp ? 'leader' : ''}>
                  <td>
                    <span className="rp-player">
                      <Avatar name={p.name} photo={photo(p.playerId)} size={24} />
                      {p.name}
                      {p.champion && <span title="Campeón"> 🏆</span>}
                    </span>
                  </td>
                  <td>{p.played}</td>
                  <td>{p.wins}</td>
                  {anyDraws && <td>{p.draws}</td>}
                  <td>{p.losses}</td>
                  {anyScored && <td>{p.scored}</td>}
                  <td>{sign(p.goalsFor - p.goalsAgainst)}</td>
                  <td>{p.played ? `${Math.round(p.winPct)} %` : '—'}</td>
                  <td><strong>{p.mvp}</strong></td>
                </tr>
              ))}
            </tbody>
          </table>
          <div className="dim" style={{ fontSize: 10, marginTop: 6 }}>
            MVP = 3 por victoria + 1 por empate + 1 por gol marcado + la mitad de la diferencia de goles + 2 al campeón.
          </div>
        </div>
      </div>
    </div>
  );
}
