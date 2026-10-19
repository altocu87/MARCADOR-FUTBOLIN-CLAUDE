import { useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import type { SettingsTab } from '../../app/routes';
import { CONFIG_LIMITS, type EndCondition } from '../../match-engine';
import {
  applyImport,
  buildBackup,
  parseBackup,
  previewImport,
  type BackupFile,
  type ImportPreview,
  type Player,
  type OverridableKey,
  type Preferences,
} from '../../services/persistence';
import { sortPlayers } from '../../services/players';
import { Avatar, Modal, ScreenFrame, Stepper, Tabs, Toggle } from '../components/common';
import { downloadJson } from '../components/download';
import { PlayerOptions } from '../components/PlayerQr';
import { PlayerEditor } from '../components/PlayerEditor';
import { ClubEditor } from '../components/ClubEditor';
import { Crest } from '../components/Crest';
import { saveClub, type Club } from '../../services/clubs';
import { ConnectionsTab } from './ConnectionsTab';
import { SoundsTab } from './SoundsTab';
import { AccountTab } from './AccountTab';
import { WEB_URL } from '../../services/profiles';

export const APP_VERSION = '0.3.0';

export function SettingsScreen({ tab: initialTab }: { tab?: SettingsTab }) {
  const { navigate } = useApp();
  const [tab, setTab] = useState<SettingsTab>(initialTab ?? 'match');
  return (
    <ScreenFrame title="Ajustes" onBack={() => navigate({ name: 'home' })}>
      <Tabs
        label="Secciones de ajustes"
        value={tab}
        onChange={setTab}
        tabs={[
          { id: 'match', label: 'Partido' },
          { id: 'players', label: 'Jugadores' },
          { id: 'sounds', label: 'Sonidos' },
          { id: 'table', label: 'Mesa' },
          { id: 'data', label: 'Datos' },
          { id: 'account', label: 'Cuenta' },
        ]}
      />
      <div className="scroll settings-body">
        {tab === 'match' && <MatchTab />}
        {tab === 'players' && <Players />}
        {tab === 'sounds' && <SoundsTab />}
        {tab === 'table' && <ConnectionsTab />}
        {tab === 'data' && <DataTab />}
        {tab === 'account' && <AccountTab />}
      </div>
    </ScreenFrame>
  );
}

function usePrefs() {
  const { prefs, savePrefs, toast } = useApp();
  const update = async (patch: Partial<Preferences>) => {
    try {
      await savePrefs({ ...prefs, ...patch });
    } catch {
      toast('No se pudo guardar la preferencia');
    }
  };
  return { prefs, update };
}

function TestModeCard() {
  const { demoMode, setDemoMode, toast } = useApp();
  const [confirmOff, setConfirmOff] = useState(false);
  const [busy, setBusy] = useState(false);
  const apply = async (on: boolean) => {
    setBusy(true);
    try {
      await setDemoMode(on);
      toast(on ? 'Modo prueba activado' : 'Modo prueba desactivado: datos de prueba borrados');
    } catch {
      toast('No se pudo cambiar el modo prueba');
      setBusy(false);
    }
  };
  return (
    <div className={`card demo-card${demoMode ? ' on' : ''}`}>
      <Toggle
        checked={demoMode}
        onChange={(v) => (busy ? undefined : v ? void apply(true) : setConfirmOff(true))}
        label="Modo prueba"
        help="Carga 12 jugadores, más de 100 partidos y 4 torneos ficticios. Todo lo que juegues o añadas se guarda en el modo prueba. Al desactivarlo se borra y vuelven tus datos reales intactos."
      />
      {confirmOff && (
        <Modal
          title="¿Desactivar el modo prueba?"
          onClose={() => setConfirmOff(false)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setConfirmOff(false)}>Cancelar</button>
              <button className="btn btn-danger" onClick={() => { setConfirmOff(false); void apply(false); }}>Desactivar y borrar</button>
            </>
          }
        >
          <p style={{ margin: 0 }}>Se borrarán los jugadores, partidos y torneos de prueba, también los que hayas añadido. Tus datos reales vuelven tal cual estaban.</p>
        </Modal>
      )}
    </div>
  );
}

/** De dónde sale cada valor: del panel de administración o cambiado en esta mesa (y cómo volver al del panel). */
function Origin({ keys }: { keys: OverridableKey[] }) {
  const { prefs, followPanel, toast } = useApp();
  const own = keys.some((k) => prefs.overrides?.includes(k));
  if (!own) return <span className="origin-tag">Del panel</span>;
  return (
    <button
      className="origin-tag own"
      onClick={() => void followPanel(keys).then(() => toast('Vuelve al valor del panel'), () => toast('No se pudo guardar'))}
    >
      Solo esta mesa · ↺ Volver al del panel
    </button>
  );
}

/** Cómo empieza un partido suelto en esta mesa (lo de los torneos lo decide cada torneo). */
function MatchTab() {
  const { prefs, update } = usePrefs();
  return (
    <div className="settings-grid">
      <div className="card">
        <div className="label origin-row">Cómo se gana <Origin keys={['defaultEndCondition']} /></div>
        <div className="segmented" style={{ marginTop: 6 }}>
          {(['goals', 'time', 'both'] as EndCondition[]).map((c) => (
            <button key={c} className="seg" aria-pressed={prefs.defaultEndCondition === c} onClick={() => update({ defaultEndCondition: c })}>
              {c === 'goals' ? 'GOLES' : c === 'time' ? 'TIEMPO' : 'AMBAS'}
            </button>
          ))}
        </div>
      </div>
      <div className="card">
        <div className="label origin-row">Goles para ganar <Origin keys={['defaultGoalsPerPeriod']} /></div>
        <Stepper
          label="goles por defecto"
          value={prefs.defaultGoalsPerPeriod}
          min={CONFIG_LIMITS.goalsPerPeriod.min}
          max={CONFIG_LIMITS.goalsPerPeriod.max}
          onChange={(v) => update({ defaultGoalsPerPeriod: v })}
        />
      </div>
      <div className="card">
        <div className="label origin-row">Minutos por parte <Origin keys={['defaultMinutesPerPeriod']} /></div>
        <Stepper
          label="minutos por defecto"
          value={prefs.defaultMinutesPerPeriod}
          min={CONFIG_LIMITS.minutesPerPeriod.min}
          max={CONFIG_LIMITS.minutesPerPeriod.max}
          onChange={(v) => update({ defaultMinutesPerPeriod: v })}
        />
      </div>
      <div className="card">
        <div className="label origin-row">Primer lanzador en penaltis <Origin keys={['penaltyFirstTeam']} /></div>
        <div className="segmented" style={{ marginTop: 6 }}>
          <button className="seg" aria-pressed={prefs.penaltyFirstTeam === 'white'} onClick={() => update({ penaltyFirstTeam: 'white' })}>
            BLANCO
          </button>
          <button className="seg" aria-pressed={prefs.penaltyFirstTeam === 'blue'} onClick={() => update({ penaltyFirstTeam: 'blue' })}>
            AZUL
          </button>
        </div>
      </div>
    </div>
  );
}

function Players() {
  const [view, setView] = useState<'players' | 'teams'>('players');
  return (
    <>
      <div className="segmented" style={{ marginBottom: 10, maxWidth: 360 }}>
        <button className="seg seg-compact" aria-pressed={view === 'players'} onClick={() => setView('players')}>
          JUGADORES
        </button>
        <button className="seg seg-compact" aria-pressed={view === 'teams'} onClick={() => setView('teams')}>
          EQUIPOS
        </button>
      </div>
      {view === 'players' ? <PlayerList /> : <TeamList />}
    </>
  );
}

/** Equipos fijos: una pareja con nombre y logo, que se reconoce sola cuando juegan juntos. */
function TeamList() {
  const { players, prefs, savePrefs, toast } = useApp();
  const [editing, setEditing] = useState<Club | 'new' | null>(null);
  const [removing, setRemoving] = useState<Club | null>(null);
  const nameOf = (id: string) => players.find((p) => p.id === id);
  const clubs = [...prefs.clubs].sort((a, b) => a.name.localeCompare(b.name, 'es'));
  const save = async (v: { name: string; logo: string; playerIds: string[] }) => {
    try {
      let list = prefs.clubs;
      // Si se cambian los jugadores de un equipo, el antiguo se sustituye.
      if (editing && editing !== 'new') list = list.filter((c) => c.id !== editing.id);
      const r = saveClub(list, v.playerIds, v.name, v.logo, Date.now());
      const club = editing && editing !== 'new' ? { ...r.club, id: editing.id, createdAt: editing.createdAt } : r.club;
      await savePrefs({ ...prefs, clubs: r.clubs.map((c) => (c.id === r.club.id ? club : c)) });
      toast(`Equipo «${v.name}» guardado`);
    } catch {
      toast('No se pudo guardar');
    }
  };
  const remove = async (c: Club) => {
    await savePrefs({ ...prefs, clubs: prefs.clubs.filter((x) => x.id !== c.id) });
    setRemoving(null);
    toast(`Equipo «${c.name}» borrado`);
  };
  return (
    <>
      <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 8 }}>
        <span className="muted" style={{ flex: 1, fontSize: 13 }}>
          {clubs.length} equipo{clubs.length === 1 ? "" : "s"} · cuando los dos juegan juntos salen con su nombre y logo, y en los torneos de parejas van siempre juntos.
        </span>
        <button className="btn btn-primary btn-sm" onClick={() => setEditing('new')}>
          + Nuevo equipo
        </button>
      </div>
      {clubs.length === 0 ? (
        <div className="empty">
          <div>
            <strong>Sin equipos</strong>
            Crea el primero con «Nuevo equipo».
          </div>
        </div>
      ) : (
        <div className="list">
          {clubs.map((c) => (
            <div key={c.id} className="row">
              <Crest id={c.logo} size={40} />
              <span style={{ flex: 1, minWidth: 0 }}>
                <strong>{c.name}</strong>
                <span className="club-row-players">
                  {c.playerIds.map((id) => {
                    const p = nameOf(id);
                    return (
                      <span key={id}>
                        <Avatar name={p?.name ?? '?'} photo={p?.photo} size={24} /> {p?.name ?? '?'}
                      </span>
                    );
                  })}
                </span>
              </span>
              <button className="btn btn-sm" onClick={() => setEditing(c)}>
                Editar
              </button>
              <button className="btn btn-sm btn-danger" onClick={() => setRemoving(c)}>
                Borrar
              </button>
            </div>
          ))}
        </div>
      )}
      {editing && <ClubEditor club={editing === 'new' ? undefined : editing} onSave={(v) => void save(v)} onClose={() => setEditing(null)} />}
      {removing && (
        <Modal
          title={`¿Borrar «${removing.name}»?`}
          onClose={() => setRemoving(null)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setRemoving(null)}>
                No
              </button>
              <button className="btn btn-danger" onClick={() => void remove(removing)}>
                Borrar
              </button>
            </>
          }
        >
          <p style={{ margin: 0 }}>Los jugadores no se borran y los torneos ya jugados conservan el nombre. Solo deja de reconocerse esta pareja como equipo.</p>
        </Modal>
      )}
    </>
  );
}

function PlayerList() {
  const { players } = useApp();
  const [creating, setCreating] = useState(false);
  // Tocar un jugador abre su menú (el mismo que en el resto de la app).
  const [options, setOptions] = useState<Player | null>(null);
  const list = sortPlayers(players);
  return (
    <>
      <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 8 }}>
        <span className="muted" style={{ flex: 1, fontSize: 13 }}>
          {players.length} jugadores · toca uno para ver sus opciones
        </span>
        <button className="btn btn-primary btn-sm" onClick={() => setCreating(true)}>
          + Nuevo jugador
        </button>
      </div>
      {list.length === 0 ? (
        <div className="empty">
          <div>
            <strong>Sin jugadores</strong>
            Crea el primero con «Nuevo jugador».
          </div>
        </div>
      ) : (
        <div className="list">
          {list.map((p) => (
            <button key={p.id} className="row row-btn" style={{ opacity: p.active ? 1 : 0.6 }} onClick={() => setOptions(p)}>
              <Avatar name={p.name} photo={p.photo} size={36} />
              <span style={{ flex: 1, textAlign: 'left' }}>
                <strong>{p.name}</strong> {p.alias && <span className="muted">«{p.alias}»</span>}
                {!p.active && <span className="badge" style={{ marginLeft: 8 }}>Inactivo</span>}
              </span>
              <span className="muted" aria-hidden="true">⋯</span>
            </button>
          ))}
        </div>
      )}
      {creating && <PlayerEditor onClose={() => setCreating(false)} />}
      {options && <PlayerOptions player={options} onClose={() => setOptions(null)} />}
    </>
  );
}

/** Panel de administración: junto a la app si se abre desde la web; si no (placa, instalada…), el de la web publicada. */
function adminUrl(): string {
  const { href, pathname } = window.location;
  return /\/app\/(index\.html)?$/.test(pathname) ? new URL('../admin/', href).href : `${WEB_URL}admin/`;
}

/** Datos de esta mesa: copia de seguridad, modo prueba, borrar todo, versión, reglas en uso y el panel. */
function DataTab() {
  const { repos, refresh, persistent, players, matches, sounds, soundRepo, toast, rulesUpdatedAt } = useApp();
  const fileRef = useRef<HTMLInputElement>(null);
  const [pending, setPending] = useState<{ backup: BackupFile; preview: ImportPreview } | null>(null);
  const [errors, setErrors] = useState<string[]>([]);
  const [confirmWipe, setConfirmWipe] = useState(false);
  const usage = (() => {
    try {
      let bytes = 0;
      for (let i = 0; i < window.localStorage.length; i += 1) {
        const k = window.localStorage.key(i) ?? '';
        if (k.startsWith('mfv3:')) bytes += (window.localStorage.getItem(k) ?? '').length * 2;
      }
      return `${(bytes / 1024).toFixed(1)} KB`;
    } catch {
      return 'desconocido';
    }
  })();

  const doExport = async () => {
    const backup = await buildBackup(repos, new Date(), soundRepo);
    downloadJson(`marcador-futbolin-backup-${new Date().toISOString().slice(0, 10)}.json`, backup);
  };

  const onFile = async (file?: File) => {
    if (!file) return;
    setErrors([]);
    const parsed = parseBackup(await file.text());
    if (!parsed.ok) {
      setErrors(parsed.errors);
      return;
    }
    setPending({ backup: parsed.backup, preview: await previewImport(repos, parsed.backup, soundRepo) });
  };

  const doImport = async (strategy: 'merge' | 'replace') => {
    if (!pending) return;
    try {
      await applyImport(repos, pending.backup, strategy, soundRepo);
      await refresh();
      toast('Importación completada');
    } catch (err) {
      setErrors([err instanceof Error ? err.message : 'Error al importar']);
    }
    setPending(null);
  };

  const wipe = async () => {
    await repos.wipe();
    await soundRepo.saveAll([]).catch(() => undefined);
    await refresh();
    setConfirmWipe(false);
    toast('Datos locales borrados');
  };

  return (
    <div className="settings-grid">
      <div className="card">
        <div className="label">Copia de seguridad</div>
        <div style={{ display: 'flex', gap: 8, marginTop: 8, flexWrap: 'wrap' }}>
          <button className="btn btn-primary btn-sm" onClick={doExport}>Exportar archivo</button>
          <button className="btn btn-sm" onClick={() => fileRef.current?.click()}>Importar archivo</button>
          <input ref={fileRef} type="file" accept="application/json,.json" hidden onChange={(e) => { void onFile(e.target.files?.[0]); e.target.value = ''; }} />
        </div>
        <p className="muted" style={{ fontSize: 13 }}>
          {players.length} jugadores · {matches.length} partidos · {sounds.length} sonidos · {usage} usados{persistent ? '' : ' · ¡este navegador no guarda los datos!'}
        </p>
        {errors.length > 0 && <div className="notice error">{errors.join(' ')}</div>}
      </div>
      <TestModeCard />
      <div className="card">
        <div className="label">Borrar</div>
        <button className="btn btn-danger btn-sm" style={{ marginTop: 8 }} onClick={() => setConfirmWipe(true)}>
          Borrar todos los datos de esta mesa
        </button>
      </div>
      <div className="card">
        <div className="label">Aplicación</div>
        <p className="muted" style={{ fontSize: 13, margin: '6px 0' }}>
          Versión {APP_VERSION} ·{' '}
          {rulesUpdatedAt
            ? `reglas del juego del ${new Date(rulesUpdatedAt).toLocaleDateString('es-ES', { day: 'numeric', month: 'long', year: 'numeric' })}`
            : 'reglas del juego recomendadas'}
        </p>
        <button className="btn btn-sm" onClick={() => window.open(adminUrl(), '_blank', 'noopener,noreferrer')}>
          🔒 Panel de administración ↗
        </button>
      </div>

      {pending && (
        <Modal
          title="Importar copia"
          onClose={() => setPending(null)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setPending(null)}>Cancelar</button>
              <button className="btn btn-danger" onClick={() => doImport('replace')}>Reemplazar todo</button>
              <button className="btn btn-primary" onClick={() => doImport('merge')}>Combinar</button>
            </>
          }
        >
          <p style={{ margin: 0 }}>
            Jugadores: {pending.preview.players.total} ({pending.preview.players.new} nuevos, {pending.preview.players.existing} ya existentes).
            <br />
            Partidos: {pending.preview.matches.total} ({pending.preview.matches.new} nuevos, {pending.preview.matches.existing} ya existentes).
            {pending.preview.sounds.total > 0 && (
              <>
                <br />
                Sonidos: {pending.preview.sounds.total} ({pending.preview.sounds.new} nuevos, {pending.preview.sounds.existing} ya existentes).
              </>
            )}
          </p>
          <p className="muted" style={{ fontSize: 13 }}>
            <strong>Combinar</strong> añade solo lo nuevo y no sobrescribe nada local. <strong>Reemplazar</strong> borra
            jugadores, historial, preferencias y sonidos actuales y los sustituye por los del archivo.
          </p>
        </Modal>
      )}
      {confirmWipe && (
        <Modal
          title="¿Borrar todos los datos?"
          onClose={() => setConfirmWipe(false)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setConfirmWipe(false)}>Cancelar</button>
              <button className="btn btn-danger" onClick={wipe}>Borrar definitivamente</button>
            </>
          }
        >
          <p style={{ margin: 0 }}>Se eliminarán jugadores, historial, preferencias, sonidos y el partido en curso de este dispositivo. Exporta antes una copia si la necesitas.</p>
        </Modal>
      )}
    </div>
  );
}


