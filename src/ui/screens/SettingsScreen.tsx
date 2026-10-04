import { useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import type { SettingsTab } from '../../app/routes';
import { CONFIG_LIMITS, ENGINE_VERSION, RULES_VERSION, type EndCondition } from '../../match-engine';
import {
  DEFAULT_PROGRESSION,
  STORAGE_FORMAT_VERSION,
  applyImport,
  buildBackup,
  parseBackup,
  previewImport,
  type BackupFile,
  type EffectsLevel,
  type GoalSoundId,
  type ImportPreview,
  type Player,
  type Preferences,
} from '../../services/persistence';
import { setPlayerActive, sortPlayers } from '../../services/players';
import { PROGRESSION_RULES_VERSION } from '../../services/progression';
import { GOAL_SOUNDS, sound } from '../../services/sound/sound';
import { voice } from '../../services/sound/voice';
import { fullscreenAvailable, toggleFullscreen, wakeLock } from '../../services/system/device';
import { STATS_DEFINITIONS_VERSION } from '../../services/statistics';
import { Avatar, Modal, ScreenFrame, Stepper, Tabs, Toggle } from '../components/common';
import { downloadJson } from '../components/download';
import { PlayerEditor } from '../components/PlayerEditor';
import { ConnectionsTab } from './ConnectionsTab';
import { DiyTab } from './DiyTab';

export const APP_VERSION = '0.3.0';

export function SettingsScreen({ tab: initialTab }: { tab?: SettingsTab }) {
  const { navigate } = useApp();
  const [tab, setTab] = useState<SettingsTab>(initialTab ?? 'general');
  const [diyBoard, setDiyBoard] = useState<string | undefined>();
  return (
    <ScreenFrame title="Ajustes" onBack={() => navigate({ name: 'home' })}>
      <Tabs
        label="Secciones de ajustes"
        value={tab}
        onChange={setTab}
        tabs={[
          { id: 'general', label: 'General' },
          { id: 'players', label: 'Jugadores' },
          { id: 'audio', label: 'Audio/Efectos' },
          { id: 'progression', label: 'Progresión' },
          { id: 'connections', label: 'Conexiones' },
          { id: 'diy', label: 'Hazlo tú mismo' },
          { id: 'system', label: 'Sistema' },
          { id: 'info', label: 'Información' },
        ]}
      />
      <div className="scroll settings-body">
        {tab === 'general' && <General />}
        {tab === 'players' && <Players />}
        {tab === 'audio' && <Audio />}
        {tab === 'progression' && <Progression />}
        {tab === 'connections' && (
          <ConnectionsTab
            onShowGuide={(boardId) => {
              setDiyBoard(boardId);
              setTab('diy');
            }}
          />
        )}
        {tab === 'diy' && <DiyTab key={diyBoard ?? 'diy'} initialBoard={diyBoard} />}
        {tab === 'system' && <SystemTab />}
        {tab === 'info' && <Info />}
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
        description="Carga 12 jugadores, más de 100 partidos y 4 torneos ficticios. Todo lo que juegues o añadas se guarda en el modo prueba. Al desactivarlo se borra y vuelven tus datos reales intactos."
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

function General() {
  const { prefs, update } = usePrefs();
  return (
    <div className="settings-grid">
      <TestModeCard />
      <div className="card">
        <div className="label">Condición por defecto</div>
        <div className="segmented" style={{ marginTop: 6 }}>
          {(['goals', 'time', 'both'] as EndCondition[]).map((c) => (
            <button key={c} className="seg" aria-pressed={prefs.defaultEndCondition === c} onClick={() => update({ defaultEndCondition: c })}>
              {c === 'goals' ? 'GOLES' : c === 'time' ? 'TIEMPO' : 'AMBAS'}
            </button>
          ))}
        </div>
      </div>
      <div className="card">
        <div className="label">Goles para ganar (por defecto)</div>
        <Stepper
          label="goles por defecto"
          value={prefs.defaultGoalsPerPeriod}
          min={CONFIG_LIMITS.goalsPerPeriod.min}
          max={CONFIG_LIMITS.goalsPerPeriod.max}
          onChange={(v) => update({ defaultGoalsPerPeriod: v })}
        />
      </div>
      <div className="card">
        <div className="label">Minutos por parte (por defecto)</div>
        <Stepper
          label="minutos por defecto"
          value={prefs.defaultMinutesPerPeriod}
          min={CONFIG_LIMITS.minutesPerPeriod.min}
          max={CONFIG_LIMITS.minutesPerPeriod.max}
          onChange={(v) => update({ defaultMinutesPerPeriod: v })}
        />
      </div>
      <div className="card">
        <div className="label">Primer lanzador en penaltis (propuesta)</div>
        <div className="segmented" style={{ marginTop: 6 }}>
          <button className="seg" aria-pressed={prefs.penaltyFirstTeam === 'white'} onClick={() => update({ penaltyFirstTeam: 'white' })}>
            BLANCO
          </button>
          <button className="seg" aria-pressed={prefs.penaltyFirstTeam === 'blue'} onClick={() => update({ penaltyFirstTeam: 'blue' })}>
            AZUL
          </button>
        </div>
      </div>
      <div className="card">
        <div className="label">Temporadas y retos</div>
        <div className="segmented" style={{ margin: '6px 0' }}>
          <button className="seg seg-compact" aria-pressed={prefs.seasonLength === 'month'} onClick={() => update({ seasonLength: 'month' })}>
            MENSUAL
          </button>
          <button className="seg seg-compact" aria-pressed={prefs.seasonLength === 'quarter'} onClick={() => update({ seasonLength: 'quarter' })}>
            TRIMESTRAL
          </button>
        </div>
        <Toggle
          checked={prefs.challenges}
          onChange={(v) => update({ challenges: v })}
          label="Retos diarios y semanales"
          description="Conceden XP extra (recalculado desde el historial)."
        />
      </div>
      <div className="card">
        <div className="label">Pantalla</div>
        <Toggle
          checked={prefs.keepAwake}
          onChange={(v) => update({ keepAwake: v })}
          label="Mantener pantalla encendida"
          description={wakeLock.available ? 'Evita que el móvil o la tablet se apague con la app abierta.' : 'Este navegador no lo permite.'}
        />
        {fullscreenAvailable() && (
          <button className="btn btn-sm" style={{ marginTop: 6 }} onClick={() => void toggleFullscreen()}>
            ⛶ Pantalla completa
          </button>
        )}
      </div>
      <div className="card">
        <div className="label">Reposo tras inactividad</div>
        <Stepper
          label="minutos de reposo"
          value={prefs.sleepMinutes}
          min={0}
          max={60}
          unit={prefs.sleepMinutes === 0 ? 'desactivado' : 'min'}
          onChange={(v) => update({ sleepMinutes: v })}
        />
      </div>
    </div>
  );
}

function Players() {
  const { players, savePlayer, toast } = useApp();
  const [editing, setEditing] = useState<Player | 'new' | null>(null);
  const list = sortPlayers(players);
  const toggle = async (p: Player) => {
    try {
      await savePlayer(setPlayerActive(p, !p.active, Date.now()));
    } catch {
      toast('No se pudo guardar');
    }
  };
  return (
    <>
      <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 8 }}>
        <span className="muted" style={{ flex: 1, fontSize: 13 }}>
          {players.length} jugadores · la baja es lógica: los inactivos no se ofrecen para nuevos partidos pero conservan su historial.
        </span>
        <button className="btn btn-primary btn-sm" onClick={() => setEditing('new')}>
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
            <div key={p.id} className="row" style={{ opacity: p.active ? 1 : 0.6 }}>
              <Avatar name={p.name} photo={p.photo} size={36} />
              <span style={{ flex: 1 }}>
                <strong>{p.name}</strong> {p.alias && <span className="muted">«{p.alias}»</span>}
                {!p.active && <span className="badge" style={{ marginLeft: 8 }}>Inactivo</span>}
              </span>
              <button className="btn btn-sm" onClick={() => setEditing(p)}>
                Editar
              </button>
              <button className="btn btn-sm" onClick={() => toggle(p)}>
                {p.active ? 'Desactivar' : 'Activar'}
              </button>
            </div>
          ))}
        </div>
      )}
      {editing && <PlayerEditor player={editing === 'new' ? undefined : editing} onClose={() => setEditing(null)} />}
    </>
  );
}

function Audio() {
  const { prefs, update } = usePrefs();
  const levels: { id: EffectsLevel; label: string }[] = [
    { id: 'full', label: 'COMPLETOS' },
    { id: 'reduced', label: 'REDUCIDOS' },
    { id: 'off', label: 'DESACTIVADOS' },
  ];
  return (
    <div className="settings-grid">
      <div className="card">
        <Toggle checked={prefs.muted} onChange={(v) => update({ muted: v })} label="Silencio" description="Desactiva todos los sonidos." />
        <div className="label" style={{ marginTop: 10 }}>Volumen {Math.round(prefs.volume * 100)} %</div>
        <input
          className="range"
          type="range"
          min={0}
          max={100}
          value={Math.round(prefs.volume * 100)}
          onChange={(e) => update({ volume: Number(e.target.value) / 100 })}
          aria-label="Volumen"
        />
        <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap' }}>
          <button className="btn btn-sm" onClick={() => { sound.unlock(); sound.play('periodEnd'); }}>Final parte</button>
          <button className="btn btn-sm" onClick={() => { sound.unlock(); sound.play('matchEnd'); }}>Final partido</button>
        </div>
      </div>
      <div className="card">
        <div className="label">Sonido de gol</div>
        <div className="chip-wrap" style={{ margin: '6px 0 10px' }}>
          {(Object.keys(GOAL_SOUNDS) as GoalSoundId[]).map((id) => (
            <button
              key={id}
              className={`pick-chip ${prefs.goalSound === id ? 'pick-blue' : ''}`}
              onClick={() => {
                sound.unlock();
                sound.playGoalVariant(id);
                void update({ goalSound: id });
              }}
            >
              ♪ {GOAL_SOUNDS[id].label}
            </button>
          ))}
        </div>
        <Toggle
          checked={prefs.voice}
          onChange={(v) => update({ voice: v })}
          label="Locutor"
          description={voice.available ? 'Voz del navegador: goles, bola de partido, finales.' : 'Este navegador no tiene síntesis de voz.'}
        />
        <button className="btn btn-sm" style={{ marginTop: 6 }} onClick={() => voice.test('¡Gol del equipo blanco! Dos a uno.')}>
          Probar locutor
        </button>
        <div className="label" style={{ marginTop: 10 }}>Efectos visuales de gol</div>
        <div className="segmented" style={{ marginTop: 6 }}>
          {levels.map((l) => (
            <button key={l.id} className="seg" aria-pressed={prefs.effects === l.id} onClick={() => update({ effects: l.id })}>
              {l.label}
            </button>
          ))}
        </div>
        <p className="dim" style={{ fontSize: 12 }}>
          Base: flash, explosión, ondas, partículas y líneas de velocidad. «Reducidos» evita destellos. Los sonidos se
          sintetizan en local (sin archivos externos).
        </p>
      </div>
    </div>
  );
}

function Progression() {
  const { prefs, update } = usePrefs();
  const p = prefs.progression;
  const set = (patch: Partial<typeof p>) => update({ progression: { ...p, ...patch } });
  return (
    <>
      <div className="notice warn" style={{ marginBottom: 8 }}>
        Parámetros PROPUESTOS ({PROGRESSION_RULES_VERSION}), pendientes de aprobación. Todo se recalcula desde el historial: cambiar
        un valor reprocesa ELO, XP, niveles y logros sin duplicar premios.
      </div>
      <div className="settings-grid">
        <div className="card">
          <Toggle
            checked={p.enabled}
            onChange={(v) => set({ enabled: v })}
            label="Activar progresión"
            description="Desactivada: se muestra «Clasificación pendiente» y no hay ELO/XP."
          />
        </div>
        <div className="card">
          <Toggle
            checked={p.goalDiffMultiplier}
            onChange={(v) => set({ goalDiffMultiplier: v })}
            label="Multiplicador por diferencia de goles"
            description="1→1,00 · 2→1,05 · 3→1,10 · 4→1,15 · 5+→1,20 (penaltis: 1,00)."
          />
        </div>
        <div className="card">
          <div className="label">K provisional (primeros partidos)</div>
          <Stepper label="K provisional" value={p.kProvisional} min={1} max={100} onChange={(v) => set({ kProvisional: v })} />
        </div>
        <div className="card">
          <div className="label">K establecido</div>
          <Stepper label="K establecido" value={p.kEstablished} min={1} max={100} onChange={(v) => set({ kEstablished: v })} />
        </div>
        <div className="card">
          <div className="label">Partidos con K provisional</div>
          <Stepper label="partidos provisionales" value={p.provisionalMatches} min={0} max={50} onChange={(v) => set({ provisionalMatches: v })} />
        </div>
        <div className="card">
          <div className="label">ELO inicial (aprobado: 1200)</div>
          <div style={{ fontSize: 28, fontWeight: 800 }}>{p.eloInitial}</div>
          <button className="btn btn-sm" onClick={() => set({ ...DEFAULT_PROGRESSION, enabled: p.enabled })}>
            Restaurar propuesta
          </button>
        </div>
      </div>
      <p className="dim" style={{ fontSize: 12 }}>
        XP propuesto: partido +50 · victoria +100 · derrota +25 · victoria clasificatoria +50 · prórroga +25 · penaltis +25 ·
        logros +25/+50/+100. Nivel N requiere 100 × N^1,35 XP acumulado (0–100). Solo Clasificatorio modifica ELO.
      </p>
    </>
  );
}

function SystemTab() {
  const { repos, refresh, persistent, players, matches, toast } = useApp();
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
    const backup = await buildBackup(repos);
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
    setPending({ backup: parsed.backup, preview: await previewImport(repos, parsed.backup) });
  };

  const doImport = async (strategy: 'merge' | 'replace') => {
    if (!pending) return;
    try {
      await applyImport(repos, pending.backup, strategy);
      await refresh();
      toast('Importación completada');
    } catch (err) {
      setErrors([err instanceof Error ? err.message : 'Error al importar']);
    }
    setPending(null);
  };

  const wipe = async () => {
    await repos.wipe();
    await refresh();
    setConfirmWipe(false);
    toast('Datos locales borrados');
  };

  return (
    <div className="settings-grid">
      <div className="card">
        <div className="label">Estado</div>
        <div style={{ fontWeight: 800, margin: '4px 0' }}>{persistent ? 'SISTEMA LOCAL' : 'SIN ALMACENAMIENTO PERSISTENTE'}</div>
        <div className="muted" style={{ fontSize: 13 }}>
          {players.length} jugadores · {matches.length} partidos · {usage} usados
        </div>
        <p className="dim" style={{ fontSize: 12 }}>
          Sin backend ni conexión online: base de datos y funciones online son la última fase. Una copia dentro del
          navegador no es un backup externo: exporta un archivo de vez en cuando.
        </p>
      </div>
      <div className="card">
        <div className="label">Copia de seguridad</div>
        <div style={{ display: 'flex', gap: 8, marginTop: 8, flexWrap: 'wrap' }}>
          <button className="btn btn-primary btn-sm" onClick={doExport}>Exportar archivo</button>
          <button className="btn btn-sm" onClick={() => fileRef.current?.click()}>Importar archivo</button>
          <input ref={fileRef} type="file" accept="application/json,.json" hidden onChange={(e) => { void onFile(e.target.files?.[0]); e.target.value = ''; }} />
        </div>
        <p className="dim" style={{ fontSize: 12 }}>Incluye jugadores, preferencias e historial (formato v{STORAGE_FORMAT_VERSION}). La progresión se recalcula.</p>
        {errors.length > 0 && <div className="notice error">{errors.join(' ')}</div>}
      </div>
      <div className="card">
        <div className="label">Entradas</div>
        <p className="muted" style={{ fontSize: 13, margin: '6px 0' }}>
          Teclado de desarrollo: Q/A gol Blanco · P/L gol Azul · Espacio pausa · Intro salta la cuenta atrás.
        </p>
        <p className="dim" style={{ fontSize: 12, margin: 0 }}>
          Simulador de pulsadores/sensores: en la consola, <code>marcador.enviar('GOL_BLANCO', 'button')</code>.
          Todas las entradas pasan por el mismo motor y respetan el bloqueo de 3 s.
        </p>
      </div>
      <div className="card">
        <div className="label">Mantenimiento</div>
        <button className="btn btn-danger btn-sm" style={{ marginTop: 8 }} onClick={() => setConfirmWipe(true)}>
          Borrar todos los datos locales
        </button>
        <p className="dim" style={{ fontSize: 12 }}>Firmware, OTA, Wi-Fi y reinicio remoto: pendientes del bloque de hardware/conectividad.</p>
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
          </p>
          <p className="muted" style={{ fontSize: 13 }}>
            <strong>Combinar</strong> añade solo lo nuevo y no sobrescribe nada local. <strong>Reemplazar</strong> borra
            jugadores, historial y preferencias actuales y los sustituye por los del archivo.
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
          <p style={{ margin: 0 }}>Se eliminarán jugadores, historial, preferencias y la partida en curso de este dispositivo. Exporta antes una copia si la necesitas.</p>
        </Modal>
      )}
    </div>
  );
}

const PENDING = [
  ['Caos', 'Implementada propuesta caos-1 (último minuto x2). Pendiente de aprobación.'],
  ['Torneos', 'Implementada propuesta torneos-1 (liguilla y cuadro 3–8 equipos). Pendiente de aprobación.'],
  ['Goleador', 'Asignación opcional tras el partido (1v1 automática). El documento original no lo registraba.'],
  ['Retos y temporadas', 'Propuestas retos-1 y temporadas mensuales/trimestrales. Pendientes de aprobación.'],
  ['Correcciones', 'Operaciones permitidas al finalizar parte/partido y corrección de penaltis.'],
  ['Penaltis', 'Equipo que inicia (propuesta: Blanco configurable) y política de corrección.'],
  ['XP y niveles', 'Acumulación, fórmula de nivel, redondeos y tope.'],
  ['ELO', 'K exacto, redondeos, equipos con K distintos y multiplicador por goles.'],
  ['Categorías', 'Umbrales finales y posible histéresis.'],
  ['Ranking', 'Desempates y muestra mínima.'],
  ['Estadísticas', 'Empates tras tanda, atribución en 2v2, rachas y remontadas.'],
  ['Predicción', 'Fórmula por factor y redistribución con pocos datos.'],
  ['Logros y récords', 'Catálogo, condiciones, muestras mínimas y premios.'],
  ['Recuperación', 'Comportamiento del reloj tras recarga, cierre o apagado.'],
  ['Hardware', 'Viabilidad en S3, runtime, transporte con C3, sensores y alimentación.'],
  ['Última fase', 'Modelo de acceso remoto, migración y conflictos entre dispositivos.'],
];

function Info() {
  return (
    <>
      <div className="grid-3" style={{ marginBottom: 8 }}>
        <div className="stat"><div className="v" style={{ fontSize: 16 }}>{APP_VERSION}</div><div className="k">Aplicación</div></div>
        <div className="stat"><div className="v" style={{ fontSize: 16 }}>{ENGINE_VERSION}</div><div className="k">Motor · {RULES_VERSION}</div></div>
        <div className="stat"><div className="v" style={{ fontSize: 16 }}>{STATS_DEFINITIONS_VERSION}</div><div className="k">Definiciones</div></div>
      </div>
      <div className="label" style={{ marginBottom: 6 }}>Decisiones pendientes del propietario</div>
      <div className="list">
        {PENDING.map(([k, v]) => (
          <div key={k} className="row" style={{ minHeight: 40 }}>
            <strong style={{ width: 150 }}>{k}</strong>
            <span className="muted" style={{ fontSize: 13 }}>{v}</span>
          </div>
        ))}
      </div>
    </>
  );
}
