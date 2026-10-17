/**
 * Ajustes → Sonidos: los sonidos propios de la mesa (subir, escuchar, borrar) y qué suena en cada momento.
 * Cada sonido sabe dónde está guardado (este aparato, chip o tarjeta SD de la placa) y se toca desde allí.
 */
import { useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import { useHardware } from '../../inputs/hardware/useHardware';
import type { BoardPlace } from '../../inputs/hardware/boardSounds';
import { newId } from '../../services/ids';
import type { SoundLocation, StoredSound } from '../../services/persistence';
import { decodeFile } from '../../services/sound/prepare';
import { sound } from '../../services/sound/sound';
import { choiceFor, SOUND_EVENTS, withoutSound, type SoundChoice, type SoundEventDef } from '../../services/sound/soundEvents';
import { soundPlayer } from '../../services/sound/soundPlayer';
import { HelpButton, Modal } from '../components/common';

export const LOCATION_LABEL: Record<SoundLocation, string> = {
  device: '📱 Este aparato',
  chip: '🔌 Chip de la placa',
  sd: '💾 Tarjeta SD',
};

const NAME_MAX = 24;
const seconds = (ms: number) => `${(ms / 1000).toLocaleString('es-ES', { maximumFractionDigits: 1 })} s`;

function summary(c: SoundChoice, sounds: StoredSound[]): string {
  if (c.kind === 'none') return 'Ninguno';
  if (c.kind === 'default') return 'El de siempre';
  const mine = c.ids.filter((id) => sounds.some((s) => s.id === id));
  if (mine.length === 1) return sounds.find((s) => s.id === mine[0])!.name;
  return mine.length ? `${mine.length} al azar` : 'El de siempre';
}

const pct = (f: number) => `${Math.round(f * 100)} %`;

export function SoundsTab() {
  const { sounds, soundRepo, reloadSounds, prefs, savePrefs, toast } = useApp();
  const hub = useHardware();
  // Sonido que se está pasando a la placa y cuánto lleva (0–1).
  const [sending, setSending] = useState<{ id: string; fraction: number } | null>(null);

  const sendToBoard = async (s: StoredSound): Promise<boolean> => {
    if (s.location === 'device') return true;
    setSending({ id: s.id, fraction: 0 });
    try {
      await hub.uploadSound(s, s.location as BoardPlace, (fraction) => setSending({ id: s.id, fraction }));
      return true;
    } catch (err) {
      toast(err instanceof Error ? err.message : 'No se pudo pasar a la placa');
      return false;
    } finally {
      setSending(null);
    }
  };
  const fileRef = useRef<HTMLInputElement>(null);
  const [busy, setBusy] = useState(false);
  const [draft, setDraft] = useState<StoredSound | null>(null);
  const [removing, setRemoving] = useState<StoredSound | null>(null);
  const [editing, setEditing] = useState<SoundEventDef | null>(null);

  const onFile = async (file?: File) => {
    if (!file) return;
    setBusy(true);
    try {
      const { wav, durationMs } = await decodeFile(file);
      const name = file.name.replace(/\.[^.]+$/, '').replace(/[_-]+/g, ' ').trim().slice(0, NAME_MAX) || 'Sonido';
      // Con la placa y su tarjeta SD a mano, el sonido va a la SD; si no, a este aparato.
      const location: SoundLocation = hub.soundTargets.sd ? 'sd' : 'device';
      const d: StoredSound = { id: newId('snd'), name, location, durationMs, createdAt: Date.now(), wav };
      soundPlayer.setLibrary([...sounds, d]);
      setDraft(d);
    } catch (err) {
      toast(err instanceof Error && err.message.startsWith('El sonido') ? err.message : 'No se pudo leer ese sonido');
    }
    setBusy(false);
  };

  const saveDraft = async (d: StoredSound) => {
    // En el chip o la SD: primero se pasa a la placa (si falla, se puede elegir «Este aparato»).
    if (!(await sendToBoard(d))) return;
    try {
      await soundRepo.save({ ...d, name: d.name.trim() || 'Sonido' });
      await reloadSounds();
      toast(`«${d.name.trim() || 'Sonido'}» guardado`);
    } catch {
      toast('No se pudo guardar el sonido (¿memoria llena?)');
    }
    setDraft(null);
  };

  const cancelDraft = () => {
    setDraft(null);
    soundPlayer.setLibrary(sounds);
  };

  const remove = async (s: StoredSound) => {
    try {
      if (s.location !== 'device') hub.deleteBoardSound(s.id);
      await soundRepo.remove(s.id);
      await savePrefs({ ...prefs, soundEvents: withoutSound(prefs.soundEvents ?? {}, s.id) });
      await reloadSounds();
      toast(`«${s.name}» borrado`);
    } catch {
      toast('No se pudo borrar');
    }
    setRemoving(null);
  };

  const choose = async (id: SoundEventDef['id'], c: SoundChoice) => {
    try {
      await savePrefs({ ...prefs, soundEvents: { ...prefs.soundEvents, [id]: c } });
    } catch {
      toast('No se pudo guardar');
    }
  };

  return (
    <div className="snd">
      <div className="card">
        <div className="snd-head">
          <span className="label">Sonidos de la mesa</span>
          <button className="btn btn-primary btn-sm" disabled={busy} onClick={() => fileRef.current?.click()}>
            {busy ? 'Preparando…' : '＋ Subir sonido'}
          </button>
          <input ref={fileRef} type="file" accept="audio/*" hidden onChange={(e) => { void onFile(e.target.files?.[0]); e.target.value = ''; }} />
        </div>
        {sounds.length === 0 ? (
          <p className="muted snd-empty">Sin sonidos propios todavía.</p>
        ) : (
          <div className="list">
            {sounds.map((s) => (
              <div key={s.id} className="row">
                <button className="btn btn-sm btn-icon" aria-label={`Escuchar ${s.name}`} onClick={() => soundPlayer.preview(s.id)}>
                  ▶
                </button>
                <strong className="snd-name">{s.name}</strong>
                <span className="snd-where">{LOCATION_LABEL[s.location]}</span>
                <span className="muted snd-len">{seconds(s.durationMs)}</span>
                {s.location !== 'device' && hub.inventory && !hub.boardHas(s.id) && (
                  <button className="btn btn-sm btn-primary" disabled={!!sending} onClick={() => void sendToBoard(s)}>
                    {sending?.id === s.id ? pct(sending.fraction) : 'Pasar a la placa'}
                  </button>
                )}
                <button className="btn btn-sm btn-danger" disabled={sending?.id === s.id} onClick={() => setRemoving(s)}>
                  Borrar
                </button>
              </div>
            ))}
          </div>
        )}
      </div>

      <div className="card">
        <div className="label">Qué suena en cada momento</div>
        <div className="snd-events">
          {SOUND_EVENTS.map((e) => {
            const c = choiceFor(prefs.soundEvents, e.id);
            return (
              <div key={e.id} className="snd-event">
                <span className="snd-event-name">
                  {e.label}
                  {e.help && <HelpButton title={e.label} text={e.help} className="snd-help" />}
                </span>
                <button
                  className="btn btn-sm btn-icon"
                  aria-label={`Escuchar ${e.label}`}
                  disabled={c.kind === 'none'}
                  onClick={() => {
                    sound.unlock();
                    soundPlayer.play(e.id);
                  }}
                >
                  ▶
                </button>
                <button className={`snd-choice${c.kind === 'custom' ? ' own' : ''}`} onClick={() => setEditing(e)}>
                  {summary(c, sounds)}
                </button>
              </div>
            );
          })}
        </div>
      </div>

      {draft && (
        <DraftModal
          draft={draft}
          progress={sending?.id === draft.id ? sending.fraction : null}
          onChange={setDraft}
          onSave={(d) => void saveDraft(d)}
          onClose={cancelDraft}
        />
      )}
      {editing && (
        <ChoiceModal
          event={editing}
          choice={choiceFor(prefs.soundEvents, editing.id)}
          sounds={sounds}
          onChange={(c) => void choose(editing.id, c)}
          onClose={() => setEditing(null)}
        />
      )}
      {removing && (
        <Modal
          title={`¿Borrar «${removing.name}»?`}
          onClose={() => setRemoving(null)}
          actions={
            <>
              <button className="btn btn-ghost" onClick={() => setRemoving(null)}>No</button>
              <button className="btn btn-danger" onClick={() => void remove(removing)}>Borrar</button>
            </>
          }
        >
          <p style={{ margin: 0 }}>Donde sonaba, vuelve a sonar el de siempre.</p>
        </Modal>
      )}
    </div>
  );
}

/** Sonido recién subido: nombre y dónde se guarda. */
function DraftModal({
  draft,
  progress,
  onChange,
  onSave,
  onClose,
}: {
  draft: StoredSound;
  /** Pasándose a la placa (0–1); null si no. */
  progress: number | null;
  onChange: (d: StoredSound) => void;
  onSave: (d: StoredSound) => void;
  onClose: () => void;
}) {
  const hub = useHardware();
  const can: Record<SoundLocation, boolean> = { device: true, ...hub.soundTargets };
  const busy = progress !== null;
  return (
    <Modal
      title="Nuevo sonido"
      onClose={busy ? undefined : onClose}
      actions={
        busy ? (
          <div className="snd-progress" role="progressbar" aria-valuenow={Math.round(progress * 100)}>
            <span style={{ width: pct(progress) }} />
            <b>Pasando a la placa… {pct(progress)}</b>
          </div>
        ) : (
          <>
            <button className="btn btn-ghost" onClick={onClose}>Cancelar</button>
            <button className="btn btn-primary" onClick={() => onSave(draft)}>Guardar</button>
          </>
        )
      }
    >
      <div className="snd-draft">
        <div className="snd-draft-name">
          <button className="btn btn-icon" aria-label="Escuchar" onClick={() => soundPlayer.preview(draft.id)}>▶</button>
          <input
            className="input"
            aria-label="Nombre"
            value={draft.name}
            maxLength={NAME_MAX}
            onChange={(e) => onChange({ ...draft, name: e.target.value })}
          />
          <span className="muted">{seconds(draft.durationMs)}</span>
        </div>
        <div className="label snd-where-label">
          Dónde se guarda
          <HelpButton
            title="Dónde se guarda"
            text={'En este aparato suena por su altavoz. En la tarjeta SD de la placa suena por los altavoces de la placa, también cuando juega sola; para elegirla, conecta la pantalla de 7" por el cable USB (Ajustes → Mesa) con la tarjeta puesta. Si la placa no está, suena el de siempre.'}
            className="snd-help"
          />
        </div>
        <div className="segmented snd-where-pick">
          {/* La pantalla de 7" guarda los sonidos en la SD: el chip solo sale si una placa lo ofrece. */}
          {(Object.keys(LOCATION_LABEL) as SoundLocation[]).filter((l) => l !== 'chip' || can.chip).map((l) => (
            <button key={l} className="seg" aria-pressed={draft.location === l} disabled={!can[l] || busy} onClick={() => onChange({ ...draft, location: l })}>
              {LOCATION_LABEL[l]}
            </button>
          ))}
        </div>
      </div>
    </Modal>
  );
}

/** Elegir el sonido de un momento: el de siempre, ninguno o uno o varios propios (uno al azar cada vez). */
function ChoiceModal({
  event,
  choice,
  sounds,
  onChange,
  onClose,
}: {
  event: SoundEventDef;
  choice: SoundChoice;
  sounds: StoredSound[];
  onChange: (c: SoundChoice) => void;
  onClose: () => void;
}) {
  const toggle = (id: string) => {
    soundPlayer.preview(id);
    if (choice.kind !== 'custom') return onChange({ kind: 'custom', ids: [id] });
    const ids = choice.ids.includes(id) ? choice.ids.filter((x) => x !== id) : [...choice.ids, id];
    onChange(ids.length ? { kind: 'custom', ids } : { kind: 'default', ids: [] });
  };
  return (
    <Modal title={event.label} onClose={onClose} actions={<button className="btn btn-primary" onClick={onClose}>Listo</button>}>
      <div className="segmented snd-pick">
        <button
          className="seg"
          aria-pressed={choice.kind === 'default'}
          onClick={() => {
            sound.unlock();
            soundPlayer.playDefault(event.id);
            onChange({ ...choice, kind: 'default' });
          }}
        >
          El de siempre
        </button>
        <button className="seg" aria-pressed={choice.kind === 'none'} onClick={() => onChange({ ...choice, kind: 'none' })}>
          Ninguno
        </button>
        {sounds.map((s) => (
          <button key={s.id} className="seg" aria-pressed={choice.kind === 'custom' && choice.ids.includes(s.id)} onClick={() => toggle(s.id)}>
            {s.name}
          </button>
        ))}
      </div>
    </Modal>
  );
}
