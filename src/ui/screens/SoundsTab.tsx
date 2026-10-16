/**
 * Ajustes → Sonidos: los sonidos propios de la mesa (subir, escuchar, borrar) y qué suena en cada momento.
 * Cada sonido sabe dónde está guardado (este aparato, chip o tarjeta SD de la placa) y se toca desde allí.
 */
import { useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import { useHardware } from '../../inputs/hardware/useHardware';
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

export function SoundsTab() {
  const { sounds, soundRepo, reloadSounds, prefs, savePrefs, toast } = useApp();
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
      const d: StoredSound = { id: newId('snd'), name, location: 'device', durationMs, createdAt: Date.now(), wav };
      soundPlayer.setLibrary([...sounds, d]);
      setDraft(d);
    } catch (err) {
      toast(err instanceof Error && err.message.startsWith('El sonido') ? err.message : 'No se pudo leer ese sonido');
    }
    setBusy(false);
  };

  const saveDraft = async (d: StoredSound) => {
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
                <button className="btn btn-sm btn-danger" onClick={() => setRemoving(s)}>
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

      {draft && <DraftModal draft={draft} onChange={setDraft} onSave={(d) => void saveDraft(d)} onClose={cancelDraft} />}
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
  onChange,
  onSave,
  onClose,
}: {
  draft: StoredSound;
  onChange: (d: StoredSound) => void;
  onSave: (d: StoredSound) => void;
  onClose: () => void;
}) {
  const hub = useHardware();
  const can: Record<SoundLocation, boolean> = { device: true, ...hub.soundTargets };
  return (
    <Modal
      title="Nuevo sonido"
      onClose={onClose}
      actions={
        <>
          <button className="btn btn-ghost" onClick={onClose}>Cancelar</button>
          <button className="btn btn-primary" onClick={() => onSave(draft)}>Guardar</button>
        </>
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
            text="En este aparato suena por su altavoz. En el chip o en la tarjeta SD de la placa suena por el altavoz de la placa; se pueden elegir cuando hay una placa con altavoz conectada. Si la placa no está, suena el de siempre."
            className="snd-help"
          />
        </div>
        <div className="segmented snd-where-pick">
          {(Object.keys(LOCATION_LABEL) as SoundLocation[]).map((l) => (
            <button key={l} className="seg" aria-pressed={draft.location === l} disabled={!can[l]} onClick={() => onChange({ ...draft, location: l })}>
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
