/**
 * Torneos: lista, creación a partir de un predefinido, creador de predefinidos y detalle
 * (clasificación, cuadro o Pool rotativo, final y partidos).
 */
import { useEffect, useMemo, useRef, useState, type ReactNode } from 'react';
import { useApp } from '../../app/AppContext';
import type { MatchConfig } from '../../match-engine';
import { restoreSnapshot } from '../../app/recovery';
import type { ActiveMatchSnapshot, Fixture, Player, Tournament, TournamentFinal, TournamentTemplate } from '../../services/persistence';
import { NAME_MAX, findNameClash, initials, sortPlayers } from '../../services/players';
import { displayTitle, predict } from '../../services/progression';
import { seededRandom } from '../../services/statistics/calendar';
import {
  FORMAT_LABEL,
  TOURNAMENT_RULES_VERSION,
  allTemplates,
  competitions,
  editionName,
  competitionNameKey,
  competitionName,
  lastEdition,
  nextEdition,
  tournamentReport,
  createTournament,
  describeTemplate,
  draftFromTemplate,
  finalOptions,
  finalRulesOf,
  finalSeriesEditable,
  isFinalFixture,
  recordedMatches,
  undoMatch,
  updateTournamentRules,
  hasTemplateFinal,
  withFinalRules,
  type MatchRules,
  rulesShort,
  fixtureConfig,
  fixtureParticipants,
  fixtureScore,
  newTemplate,
  fixtureProgress,
  nextFixture,
  recommendedTemplateIds,
  roundLabel,
  standings,
  standingsMovement,
  templateFit,
  validateDraft,
  type Movement,
  type TemplateFit,
} from '../../services/tournaments';
import { AssetImage } from '../components/assets';
import { Crest, Cup, IconPicker, defaultCup, defaultLogo } from '../components/Crest';
import { ClubEditor } from '../components/ClubEditor';
import { findClub, saveClub } from '../../services/clubs';
import { Avatar, Modal, ScreenFrame, Tabs, Toggle } from '../components/common';
import { NameClashNotice } from '../components/NameClash';
import { PlayerEditor } from '../components/PlayerEditor';
import { newId } from '../../services/ids';
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

const GOAL_OPTIONS: [number, string][] = [[3, 'A 3'], [5, 'A 5'], [7, 'A 7'], [10, 'A 10']];
const MINUTE_OPTIONS: [number, string][] = [[3, '3 MIN'], [5, '5 MIN'], [8, '8 MIN'], [10, '10 MIN']];

function pickRules(t: MatchRules): MatchRules {
  return { endCondition: t.endCondition, goalsPerPeriod: t.goalsPerPeriod, minutesPerPeriod: t.minutesPerPeriod };
}

/** El tipo de torneo con otras reglas de partido. La final a más goles solo se queda si sigue siendo más. */
function withRules(t: TournamentTemplate, r: MatchRules): TournamentTemplate {
  const finalGoals = t.finalGoals && t.finalGoals > r.goalsPerPeriod ? t.finalGoals : null;
  return { ...t, endCondition: r.endCondition, goalsPerPeriod: r.goalsPerPeriod, minutesPerPeriod: r.minutesPerPeriod, finalGoals };
}

/** «Gana quien llegue a 5 goles» / «2 partes de 5 minutos: gana quien lleve más goles» / … */
function rulesText(r: MatchRules): string {
  const g = `${r.goalsPerPeriod} goles`;
  const m = `2 partes de ${r.minutesPerPeriod} minutos`;
  if (r.endCondition === 'goals') return `Gana quien llegue a ${g}.`;
  if (r.endCondition === 'time') return `${m[0].toUpperCase()}${m.slice(1)}: gana quien lleve más goles.`;
  return `A ${g} o ${m}, lo que llegue antes.`;
}

/** Elegir cómo se acaba un partido: a goles, a tiempo o ambas, y cuántos goles o minutos. */
function ConditionSeg({ rules, onChange }: { rules: MatchRules; onChange: (patch: Partial<MatchRules>) => void }) {
  return (
    <Seg<MatchRules['endCondition']>
      value={rules.endCondition}
      options={[['goals', '⚽ GOLES'], ['time', '⏱ TIEMPO'], ['both', 'AMBAS']]}
      onChange={(v) => onChange({ endCondition: v })}
    />
  );
}

function AmountRow({ rules, onChange }: { rules: MatchRules; onChange: (patch: Partial<MatchRules>) => void }) {
  return (
    <div className="tn-rules-row">
      {rules.endCondition !== 'time' && <Seg value={rules.goalsPerPeriod} options={GOAL_OPTIONS} onChange={(v) => onChange({ goalsPerPeriod: v })} />}
      {rules.endCondition !== 'goals' && <Seg value={rules.minutesPerPeriod} options={MINUTE_OPTIONS} onChange={(v) => onChange({ minutesPerPeriod: v })} />}
    </div>
  );
}

function RulesPicker({ rules, onChange }: { rules: MatchRules; onChange: (patch: Partial<MatchRules>) => void }) {
  return (
    <>
      <div className="tn-rules-row">
        <ConditionSeg rules={rules} onChange={onChange} />
      </div>
      <AmountRow rules={rules} onChange={onChange} />
    </>
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
          <div className="tn-rules-head">
            <div className="label">Partidos</div>
            <ConditionSeg rules={rules} onChange={(p) => setRules((r) => ({ ...r, ...p }))} />
          </div>
          <AmountRow rules={rules} onChange={(p) => setRules((r) => ({ ...r, ...p }))} />
        </div>
        {hasFinal && (
          <div className="tn-rules tn-rules-final">
            <div className="tn-rules-head">
              <div className="label">🏆 Final</div>
              <ConditionSeg rules={finalRules} onChange={(p) => setFinalRules((r) => ({ ...r, ...p }))} />
            </div>
            {seriesEditable && (
              <div className="tn-rules-row">
                <Seg<1 | 3> value={bestOf} options={[[1, '1 PARTIDO'], [3, 'AL MEJOR DE 3']]} onChange={setBestOf} />
              </div>
            )}
            <AmountRow rules={finalRules} onChange={(p) => setFinalRules((r) => ({ ...r, ...p }))} />
          </div>
        )}
        <small className="tn-rules-note">Solo cambia lo que queda por jugar; los partidos jugados se quedan como están.</small>
      </div>
    </Modal>
  );
}

/** Fondo de las pantallas de torneo, como en el resto de pantallas de neón. */
const TN_BG = <AssetImage name="fondo-configuracion" className="select-bg is-on tn-bg" fallback={null} />;

/** «5 oct 2026». */
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

export function TournamentListScreen({ tab: initialTab }: { tab?: 'list' | 'honours' }) {
  const { navigate, tournaments, matches, players } = useApp();
  const [tab, setTab] = useState<'list' | 'honours'>(initialTab ?? 'list');
  const list = [...tournaments].sort((a, b) => b.createdAt - a.createdAt);
  const comps = tab === 'honours' ? competitions(tournaments, matches, players) : [];
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
              { id: 'list', label: 'Torneos' },
              { id: 'honours', label: 'Palmarés' },
            ]}
          />
          <button className="btn btn-primary btn-sm" onClick={() => navigate({ name: 'tournamentNew' })}>
            + Nuevo torneo
          </button>
        </>
      }
    >
      {tab === 'honours' ? (
        comps.length === 0 ? (
          <div className="empty">
            <div>
              <Trophy size={70} />
              <strong>Palmarés vacío</strong>
              Cuando termine el primer torneo, aquí saldrá su campeón.
            </div>
          </div>
        ) : (
          <div className="hon-grid scroll">
            {comps.map((c) => {
              const top = c.honours[0];
              return (
                <button key={c.key} className="hon-card" onClick={() => navigate({ name: 'tournamentHonours', key: c.key })}>
                  <span className="hon-card-head">
                    {c.logo ? <Crest id={c.logo} size={40} /> : null}
                    {c.cup ? <Cup id={c.cup} size={40} /> : <Trophy size={40} />}
                    <span>
                      <strong>{c.name}</strong>
                      <small>
                        {c.format} · {c.editions.length} edici{c.editions.length === 1 ? 'ón' : 'ones'}
                      </small>
                    </span>
                  </span>
                  {c.lastChampion ? (
                    <span className="hon-last">
                      <small>Último campeón · {c.lastChampion.edition}ª edición · {shortDate(c.lastChampion.date)}</small>
                      <b>{c.lastChampion.champion}</b>
                    </span>
                  ) : (
                    <span className="hon-last">
                      <small>1ª edición en juego</small>
                    </span>
                  )}
                  {top && top.titles > 0 && (
                    <span className="hon-top">
                      Más títulos: <b>{top.name}</b> · {top.titles} 🏆
                    </span>
                  )}
                </button>
              );
            })}
          </div>
        )
      ) : list.length === 0 ? (
        <div className="empty">
          <div style={{ maxWidth: 520 }}>
            <Trophy size={80} />
            <strong>Aún no hay torneos</strong>
            Elige quién juega y la app te propone los tipos de torneo que encajan.
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
              <button key={t.id} className="row tn-row" onClick={() => navigate({ name: 'tournamentDetail', id: t.id, from: 'list' })}>
                {t.logo ? (
                  <Crest id={t.logo} size={40} />
                ) : (
                  <span style={{ fontSize: 26 }} aria-hidden="true">{t.status === 'finished' ? '🏆' : t.status === 'cancelled' ? '✕' : '⚔'}</span>
                )}
                <span style={{ flex: 1, minWidth: 0 }}>
                  <strong>{t.name}</strong>
                  <div className="muted" style={{ fontSize: 12 }}>
                    {FORMAT_LABEL[t.format]} · {teamCountLabel(t)} · {t.ranked ? 'cuenta para ELO' : 'sin ELO'} · {shortDate(t.createdAt)}
                  </div>
                </span>
                {t.status === 'finished' && winner ? (
                  <span className="badge badge-ranked">
                    {t.cup && <Cup id={t.cup} size={18} />} Campeón: {winner.name}
                  </span>
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
export function TournamentNewScreen({
  templateId,
  initialSelected,
  initialStep,
}: {
  templateId?: string;
  initialSelected?: string[];
  initialStep?: 'players' | 'format';
}) {
  const { navigate, players, progression, prefs, saveTournament, savePlayer, savePrefs, toast, tournaments } = useApp();
  const [step, setStep] = useState<'players' | 'format'>(initialStep ?? (initialSelected?.length ? 'format' : 'players'));
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
  const [showAll, setShowAll] = useState(false);
  // Cómo se juegan los partidos (goles, tiempo o ambas): sale del tipo elegido y se puede cambiar aquí.
  const [rules, setRules] = useState<(MatchRules & { tplId: string }) | null>(null);
  // Y la final: a partido único o al mejor de 3, y sus propias reglas.
  const [finalPick, setFinalPick] = useState<(MatchRules & { tplId: string; bestOf: 1 | 3 }) | null>(null);
  // Nombre, logo y copa del torneo (por defecto, los del tipo elegido y su última edición).
  const [brand, setBrand] = useState<{ tplId: string; name?: string; logo?: string; cup?: string } | null>(null);
  const [picker, setPicker] = useState<'logo' | 'cup' | null>(null);
  // Nombre y logo de las parejas fijas (por pareja de jugadores); se guardan como equipos al crear.
  const [teamEdits, setTeamEdits] = useState<Record<string, { name: string; logo: string }>>({});
  const [editingTeam, setEditingTeam] = useState<string[] | null>(null);
  const elo = (id: string) => progression?.players.get(id)?.elo ?? prefs.progression.eloInitial;
  const nameOf = (id: string) => players.find((p) => p.id === id)?.name ?? '?';
  const n = selected.length;

  // Solo se enseñan los tipos recomendados para este número de jugadores (y los propios que encajen);
  // el resto queda detrás de «Ver todos los tipos».
  const recIds = recommendedTemplateIds(n);
  const recommended = recIds[0] ?? null;
  const fits = allTemplates(prefs.tournamentTemplates).map((t) => ({ t, fit: templateFit(t, n) }));
  const rank = (id: string) => (recIds.includes(id) ? recIds.indexOf(id) : recIds.length);
  const shown = fits.filter((x) => x.fit.ok && (recIds.includes(x.t.id) || !x.t.builtIn)).sort((a, b) => rank(a.t.id) - rank(b.t.id));
  const otherOk = fits.filter((x) => x.fit.ok && !shown.includes(x));
  const blocked = fits.filter((x) => !x.fit.ok);
  const pickable = showAll ? [...shown, ...otherOk] : shown.length ? shown : otherOk;
  const baseTpl = pickable.find((x) => x.t.id === chosenTpl)?.t ?? pickable[0]?.t;
  const ruledTpl = baseTpl && rules?.tplId === baseTpl.id ? withRules(baseTpl, rules) : baseTpl;
  const tpl =
    ruledTpl && finalPick?.tplId === ruledTpl.id ? { ...withFinalRules(ruledTpl, finalPick), finalBestOf: finalPick.bestOf } : ruledTpl;
  const setRule = (patch: Partial<MatchRules>) => baseTpl && setRules({ ...pickRules(tpl!), ...patch, tplId: baseTpl.id });
  const setFinal = (patch: Partial<MatchRules & { bestOf: 1 | 3 }>) =>
    baseTpl && setFinalPick({ ...finalRulesOf(tpl!), bestOf: tpl!.finalBestOf, ...patch, tplId: baseTpl.id });

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
            <button className="btn btn-sm" onClick={() => navigate({ name: 'tournament' })}>
              {running ? `Mis torneos · ${running} en juego` : 'Mis torneos'}
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
            <span className="notice select-status">
              {n < 3 ? `Marca al menos ${3 - n} más` : `${shown.length} torneo${shown.length === 1 ? '' : 's'} recomendado${shown.length === 1 ? '' : 's'}`}
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
                  {pr.done} de {pr.total} partidos{nx ? ` · siguiente: ${teamOf(nx.whiteTeamId)} vs ${teamOf(nx.blueTeamId)}` : ''}
                  {running > 1 ? ` · y ${running - 1} más en «Mis torneos»` : ''}
                </small>
              </span>
              <button className="neon-go tn-resume-go" onClick={() => navigate({ name: 'tournamentDetail', id: t.id })}>
                ▶ CONTINUAR
              </button>
            </div>
          );
        })}
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
                    <span className="pick-meta">{prog && !p.guest ? `Nv ${prog.level}${prog.rankedPlayed ? ` · ELO ${prog.elo}` : ''}` : '\u00a0'}</span>
                    {p.guest ? <span className="pick-title tn-guest-tag">Invitado</span> : title && <span className="pick-title">{title}</span>}
                  </span>
                </button>
              );
            })
          )}
        </div>
        {creating && <PlayerEditor onClose={() => setCreating(false)} onSaved={(p) => setSelected((s) => [...s, p.id])} />}
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

  // Nombre de la competición: el del tipo de torneo o uno propio. Cada nombre lleva sus ediciones.
  const myBrand = brand && tpl && brand.tplId === tpl.id ? brand : null;
  const compName = (myBrand?.name ?? tpl?.name ?? '').replace(/\s+/g, ' ');
  const isCustom = !!tpl && compName.trim().toLocaleLowerCase('es') !== tpl.name.toLocaleLowerCase('es');
  const compKey = tpl ? (isCustom ? competitionNameKey(compName) : tpl.id) : '';
  const edition = tpl ? nextEdition(compKey, tournaments) : 1;
  const prevEdition = tpl ? lastEdition(compKey, tournaments) : undefined;
  const logo = myBrand?.logo ?? prevEdition?.logo ?? defaultLogo(compName || 'torneo');
  const cup = myBrand?.cup ?? prevEdition?.cup ?? defaultCup();
  const setBrandField = (patch: { name?: string; logo?: string; cup?: string }) =>
    tpl && setBrand({ tplId: tpl.id, name: compName, logo: myBrand?.logo, cup: myBrand?.cup, ...patch });
  const name = tpl ? editionName(compName.trim() || tpl.name, edition) : '';
  const built = tpl ? draftFromTemplate(tpl, name, selected, elo, { penaltyFirstTeam: prefs.penaltyFirstTeam }, seededRandom(seed), prefs.clubs.map((c) => c.playerIds)) : null;
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
        ...(isCustom && compName.trim() ? { competition: compName.trim() } : {}),
        teams: built.draft.teams.map((tm) => {
          const info = tm.playerIds.length === 2 ? teamInfo(tm.playerIds) : null;
          return info ? { ...tm, name: info.name, logo: info.logo } : tm;
        }),
      }
    : null;
  const leftover = built?.leftover ?? [];
  const errors = !tpl || !draft ? ['Ningún tipo de torneo encaja con estos jugadores.'] : leftover.length ? [`Sobra ${nameOf(leftover[0])}.`] : validateDraft(draft);
  const isPool = tpl?.format === 'pool';
  const usesDraw = !!tpl && (isPool ? tpl.pairing === 'random' : (tpl.teamSize === 2 && tpl.pairing === 'random') || (tpl.format === 'bracket' && tpl.seeding === 'random'));

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
          {draft.teams.map((t, i) =>
            t.playerIds.length === 2 ? (
              <button key={i} className={`tn-team tn-team-edit ${t.name ? 'has-name' : ''}`} onClick={() => setEditingTeam(t.playerIds)}>
                <Crest id={t.logo} size={28} />
                <span className="tn-team-text">
                  <b>{t.name ?? 'Sin nombre'}</b>
                  <small>{t.playerIds.map(nameOf).join(' + ')}</small>
                </span>
                <span className="tn-team-pen" aria-hidden="true">✎</span>
              </button>
            ) : (
              <span key={i} className="tn-team">{t.playerIds.map(nameOf).join(' + ')}</span>
            ),
          )}
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
          <div className="label">Recomendados para {n} jugadores</div>
          <div className="tpl-list scroll">
            {(shown.length ? shown : otherOk).map(tplCard)}
            {showAll ? (
              <>
                {shown.length > 0 && otherOk.length > 0 && <div className="tpl-sep">Otros tipos</div>}
                {shown.length > 0 && otherOk.map(tplCard)}
                {blocked.length > 0 && <div className="tpl-sep">No encajan con {n} jugadores</div>}
                {blocked.map(tplCard)}
              </>
            ) : (
              (otherOk.length > 0 || blocked.length > 0) && (
                <button className="tpl-more" onClick={() => setShowAll(true)}>
                  Ver todos los tipos ({otherOk.length + blocked.length} más)
                </button>
              )
            )}
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
        <div className="tn-col tn-col-rules scroll">
          {tpl && (
            <>
            <div className="tn-nameplate" aria-label={`Nombre del torneo: ${name}`}>
              <button className="tn-np-icon" onClick={() => setPicker('logo')} aria-label="Cambiar el logo del torneo">
                <Crest id={logo} size={52} />
              </button>
              <span className="tn-np-body">
                <input
                  className="tn-np-name"
                  value={compName}
                  maxLength={28}
                  onChange={(e) => setBrandField({ name: e.target.value })}
                  aria-label="Nombre del torneo"
                  placeholder={tpl.name}
                />
                <small>
                  <b className="tn-np-ed">{edition}ª edición</b> · {shortDate(Date.now())} · {isCustom ? tpl.name : 'toca el nombre para cambiarlo'}
                </small>
              </span>
              <button className="tn-np-icon tn-np-cup" onClick={() => setPicker('cup')} aria-label="Cambiar la copa">
                <Cup id={cup} size={52} />
              </button>
            </div>
            {picker && (
              <IconPicker
                kind={picker}
                value={picker === 'logo' ? logo : cup}
                onPick={(v) => setBrandField(picker === 'logo' ? { logo: v } : { cup: v })}
                playerIds={selected}
                onClose={() => setPicker(null)}
              />
            )}
            {editingTeam && (
              <ClubEditor
                fixedPlayers={editingTeam}
                initial={teamInfo(editingTeam)}
                onSave={(v) => setTeamEdits((e) => ({ ...e, [teamKey(editingTeam)]: { name: v.name, logo: v.logo } }))}
                onClose={() => setEditingTeam(null)}
              />
            )}
            </>
          )}
          {tpl && (
            <div className="tn-rules">
              <div className="tn-rules-head">
                <div className="label">Partidos</div>
                <ConditionSeg rules={tpl} onChange={setRule} />
              </div>
              <AmountRow rules={tpl} onChange={setRule} />
              <small className="tn-rules-note">{rulesText(tpl)}</small>
            </div>
          )}
          {tpl && hasTemplateFinal(tpl) && (
            <div className="tn-rules tn-rules-final">
              <div className="tn-rules-head">
                <div className="label">🏆 Final</div>
                <ConditionSeg rules={finalRulesOf(tpl)} onChange={setFinal} />
              </div>
              <div className="tn-rules-row">
                <Seg<1 | 3> value={tpl.finalBestOf} options={[[1, '1 PARTIDO'], [3, 'AL MEJOR DE 3']]} onChange={(v) => setFinal({ bestOf: v })} />
              </div>
              <AmountRow rules={finalRulesOf(tpl)} onChange={setFinal} />
              <small className="tn-rules-note">
                {tpl.finalBestOf > 1 ? 'Al mejor de 3. ' : 'A un partido. '}
                {rulesText(finalRulesOf(tpl))}
              </small>
            </div>
          )}
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
          <div>{preview}</div>
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
              <Seg<1 | 3> value={tpl.finalBestOf} options={[[1, 'PARTIDO ÚNICO'], [3, 'AL MEJOR DE 3']]} onChange={(v) => set('finalBestOf', v)} />
              <RulesPicker rules={finalRulesOf(tpl)} onChange={(patch) => setTpl((t) => withFinalRules(t, { ...finalRulesOf(t), ...patch }))} />
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

export function TournamentDetailScreen({ id, view: initialView, from }: { id: string; view?: 'report' | 'play'; from?: 'list' }) {
  const { navigate, tournaments, matches, players, saveTournament, deleteMatch, repos, prefs, progression, toast } = useApp();
  const [confirmCancel, setConfirmCancel] = useState(false);
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
  const allPlayerIds = [...new Set(t.entrants ?? t.teams.flatMap((x) => x.playerIds))];
  const back = () => (from === 'list' ? navigate({ name: 'tournament' }) : navigate({ name: 'tournamentNew', selected: allPlayerIds, step: 'players' }));

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

  const fixtureRow = (f: Fixture) => {
    const series = (f.bestOf ?? 1) > 1;
    const isNext = f.id === next?.id;
    return (
      <div key={f.id} className={`fixture ${f.winnerTeamId ? 'done' : ''} ${isNext ? 'is-next' : ''}`}>
        <span className={`fx-team ${f.winnerTeamId === f.whiteTeamId ? 'win' : ''}`}>
          <i className="pre-dot white" aria-label="Blanco" /> {teamLabel(f.whiteTeamId)}
        </span>
        <span className="fx-score">
          {isNext && !f.series ? <span className="fx-next">SIGUIENTE</span> : f.bye ? 'pase' : fixtureScore(f, byId) ?? 'vs'}
          {series && <small className="fx-series">al mejor de {f.bestOf}</small>}
        </span>
        <span className={`fx-team right ${f.winnerTeamId === f.blueTeamId ? 'win' : ''}`}>
          {teamLabel(f.blueTeamId)} <i className="pre-dot blue" aria-label="Azul" />
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
    const pct = (team: 'white' | 'blue') => (pr?.available ? `${team === 'white' ? pr.whitePct : pr.bluePct} %` : null);
    const resting = next.resting ?? allPlayerIds.filter((id) => !white.includes(id) && !blue.includes(id) && t.format === 'pool');
    const game = (next.matchIds?.length ?? 0) + 1;
    const teamBlock = (ids: string[], team: 'white' | 'blue') => {
      const tt = t.teams.find((x) => x.id === (team === 'white' ? next.whiteTeamId : next.blueTeamId));
      return (
      <div className={`next-side next-${team}`}>
        <span className="next-team-label">{team === 'white' ? 'BLANCO' : 'AZUL'}</span>
        {tt?.logo && (
          <span className="next-club">
            <Crest id={tt.logo} size={26} /> {tt.name}
          </span>
        )}
        <div className="next-avatars">
          {ids.map((pid) => (
            <Avatar key={pid} name={nameOf(pid)} photo={players.find((p) => p.id === pid)?.photo} size={46} />
          ))}
        </div>
        <b>{ids.map(nameOf).join(' + ')}</b>
        {pct(team) && <span className="next-pct">{pct(team)}</span>}
      </div>
      );
    };
    nextCard = (
      <div className="next-card">
        <div className="next-head">
          <span className="label">Siguiente partido</span>
          <span>
            {roundLabel(t, next.round)} · partido {progress.done + 1} de {progress.total}
            {(next.bestOf ?? 1) > 1 && ` · serie ${next.series?.white ?? 0}–${next.series?.blue ?? 0}, partido ${game}`}
            {` · ${rulesShort(fixtureConfig(t, next))}`}
          </span>
        </div>
        {next.abandonedAt && !pending && <div className="next-abandoned">⏸ Se dejó a medias · se empieza de cero</div>}
        <div className="next-teams">
          {teamBlock(white, 'white')}
          <span className="next-vs">VS</span>
          {teamBlock(blue, 'blue')}
        </div>
        {resting.length > 0 && <div className="next-rest">Descansa{resting.length > 1 ? 'n' : ''}: {resting.map(nameOf).join(', ')}</div>}
        <button className="neon-go next-btn" onClick={() => play(next)}>
          ▶ SIGUIENTE PARTIDO
        </button>
      </div>
    );
  }

  const rounds = [...new Set(t.fixtures.map((f) => f.round))].sort((a, b) => a - b);
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
      subtitle={t.templateName && t.edition ? `${t.edition}ª edición${t.ranked ? ' · ELO' : ''}` : `${FORMAT_LABEL[t.format]}${t.ranked ? ' · ELO' : ''}`}
      onBack={back}
      right={
        <>
          <Tabs
            label="Vista del torneo"
            value={view}
            onChange={setView}
            tabs={[
              { id: 'play', label: t.format === 'bracket' ? 'Cuadro' : 'Clasificación' },
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
              <button className="btn btn-danger btn-sm" onClick={() => setConfirmCancel(true)}>
                Cancelar
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
          <div className="label" style={{ marginTop: nextCard ? 10 : 0 }}>{t.format === 'bracket' ? 'Rondas' : 'Jornadas'}</div>
          {rounds
            .filter((r) => t.format === 'bracket' || !t.fixtures.some((f) => f.round === r && f.stage === 'final'))
            .map((r) => {
              const fx = t.fixtures.filter((f) => f.round === r && (f.whiteTeamId || f.blueTeamId));
              // Liguilla con número impar: el equipo que no juega esa jornada descansa.
              const resting =
                fx[0]?.resting ??
                (t.format === 'league' && !fx.some((f) => f.stage)
                  ? t.teams.filter((tt) => !fx.some((f) => f.whiteTeamId === tt.id || f.blueTeamId === tt.id)).flatMap((tt) => tt.playerIds)
                  : []);
              const state = fx.length && fx.every((f) => f.winnerTeamId) ? 'done' : fx.some((f) => f.id === next?.id) ? 'current' : 'pending';
              return (
                <div key={r} className={`round-block round-${state}`}>
                  <div className="round-head">
                    <b>{roundLabel(t, r)}</b>
                    <span>{state === 'done' ? 'Jugada' : state === 'current' ? 'En juego' : 'Pendiente'}</span>
                    {resting.length > 0 && <small>descansa{resting.length > 1 ? 'n' : ''}: {resting.map(nameOf).join(', ')}</small>}
                  </div>
                  {fx.map(fixtureRow)}
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
                  // De vuelta a elegir jugadores, con los mismos ya marcados.
                  navigate({ name: 'tournamentNew', selected: allPlayerIds, step: 'players' });
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
                {r.mvp.wins} G · {r.mvp.losses} P{r.mvp.scored ? ` · ${r.mvp.scored} goles` : ''} · {sign(r.mvp.goalsFor - r.mvp.goalsAgainst)} · {r.mvp.mvp} pts
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
            MVP = 3 por victoria + 1 por gol marcado + la mitad de la diferencia de goles + 2 al campeón.
          </div>
        </div>
      </div>
    </div>
  );
}
