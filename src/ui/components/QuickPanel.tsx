/**
 * Panel rápido: se baja deslizando el dedo desde el borde de arriba (como en los móviles) o tocando la pestaña
 * de arriba. Volumen, brillo, efectos, locutor, sonido de gol y pantalla completa; lo que se cambia se guarda en
 * esta mesa. No se abre durante un partido, para no tocarlo sin querer jugando.
 */
import { useEffect, useRef, useState, useSyncExternalStore } from 'react';
import { useApp } from '../../app/AppContext';
import { hardwareHub } from '../../inputs/hardware/hub';
import type { EffectsLevel, GoalSoundId, Preferences } from '../../services/persistence';
import { GOAL_SOUNDS, sound } from '../../services/sound/sound';
import { choiceFor } from '../../services/sound/soundEvents';
import { soundPlayer } from '../../services/sound/soundPlayer';
import { fullscreenAvailable, toggleFullscreen } from '../../services/system/device';

/** Hasta dónde llega el «borde de arriba» y cuánto hay que bajar el dedo (en píxeles del lienzo). */
const EDGE = 28;
const PULL = 60;

const EFFECTS: { id: EffectsLevel; label: string }[] = [
  { id: 'full', label: 'Completos' },
  { id: 'reduced', label: 'Suaves' },
  { id: 'off', label: 'Sin efectos' },
];

export function QuickPanel() {
  const { prefs, savePrefs, route, navigate, toast } = useApp();
  const [open, setOpen] = useState(false);
  const enabled = route.name !== 'match';
  const board = useSyncExternalStore(
    (l) => hardwareHub.subscribe(l),
    () => hardwareHub.controlsBrightness,
  );
  const brightness = prefs.brightness ?? 100;
  // Sonido de gol: los de la app o «Propio» si en Ajustes → Sonidos se le ha puesto uno de la mesa.
  const goal = choiceFor(prefs.soundEvents, 'goal');

  // El brillo pedido viaja a la placa (si sabe cambiarlo) también al conectarla más tarde.
  useEffect(() => hardwareHub.setBrightness(brightness), [brightness]);
  useEffect(() => {
    if (!enabled) setOpen(false);
  }, [enabled]);

  // Gesto: dedo que empieza pegado al borde de arriba del lienzo y baja.
  const start = useRef<{ y: number; scale: number } | null>(null);
  useEffect(() => {
    if (!enabled) return;
    const stage = () => document.querySelector<HTMLElement>('.stage');
    const down = (e: PointerEvent) => {
      const el = stage();
      if (!el) return;
      const r = el.getBoundingClientRect();
      const scale = r.height / (el.offsetHeight || 1);
      start.current = (e.clientY - r.top) / scale <= EDGE ? { y: e.clientY, scale } : null;
    };
    const move = (e: PointerEvent) => {
      const s = start.current;
      if (s && (e.clientY - s.y) / s.scale >= PULL) {
        start.current = null;
        setOpen(true);
      }
    };
    const up = () => (start.current = null);
    window.addEventListener('pointerdown', down, true);
    window.addEventListener('pointermove', move, true);
    window.addEventListener('pointerup', up, true);
    window.addEventListener('pointercancel', up, true);
    return () => {
      window.removeEventListener('pointerdown', down, true);
      window.removeEventListener('pointermove', move, true);
      window.removeEventListener('pointerup', up, true);
      window.removeEventListener('pointercancel', up, true);
    };
  }, [enabled]);

  const update = (patch: Partial<Preferences>) =>
    void savePrefs({ ...prefs, ...patch }).catch(() => toast('No se pudo guardar'));

  // Sin placa que lo controle, el brillo se hace con una capa oscura encima de todo.
  const dim = board ? 0 : (100 - brightness) / 100 * 0.85;

  return (
    <>
      {dim > 0 && <div className="qp-dim" style={{ opacity: dim }} aria-hidden />}
      {enabled && !open && <button className="qp-handle" aria-label="Ajustes rápidos" onClick={() => setOpen(true)} />}
      {open && (
        <div className="qp-layer" onClick={() => setOpen(false)}>
          <div className="qp-panel" role="dialog" aria-label="Ajustes rápidos" onClick={(e) => e.stopPropagation()}>
            <div className="qp-row">
              <button
                className={`qp-icon${prefs.muted ? ' off' : ''}`}
                aria-pressed={prefs.muted}
                aria-label={prefs.muted ? 'Quitar silencio' : 'Silenciar'}
                onClick={() => update({ muted: !prefs.muted })}
              >
                {prefs.muted ? '🔇' : '🔊'}
              </button>
              <input
                className="range qp-range"
                type="range"
                min={0}
                max={100}
                value={Math.round(prefs.volume * 100)}
                onChange={(e) => update({ volume: Number(e.target.value) / 100, muted: false })}
                onPointerUp={() => {
                  sound.unlock();
                  sound.play('periodEnd');
                }}
                aria-label="Volumen"
              />
              <span className="qp-value">{prefs.muted ? '—' : `${Math.round(prefs.volume * 100)}%`}</span>
            </div>
            <div className="qp-row">
              <span className="qp-icon" aria-hidden>☀</span>
              <input
                className="range qp-range"
                type="range"
                min={10}
                max={100}
                value={brightness}
                onChange={(e) => update({ brightness: Number(e.target.value) })}
                aria-label="Brillo"
              />
              <span className="qp-value">{brightness}%</span>
            </div>
            <div className="qp-label">Sonido de gol</div>
            <div className="segmented qp-seg">
              {(Object.keys(GOAL_SOUNDS) as GoalSoundId[]).map((id) => (
                <button
                  key={id}
                  className="seg"
                  aria-pressed={goal.kind === 'default' && prefs.goalSound === id}
                  onClick={() => {
                    sound.unlock();
                    sound.playGoalVariant(id);
                    update({ goalSound: id, soundEvents: { ...prefs.soundEvents, goal: { ...goal, kind: 'default' } } });
                  }}
                >
                  {GOAL_SOUNDS[id].label}
                </button>
              ))}
              {goal.ids.length > 0 && (
                <button
                  className="seg"
                  aria-pressed={goal.kind === 'custom'}
                  onClick={() => {
                    const next = { ...goal, kind: 'custom' as const };
                    soundPlayer.setChoices({ ...prefs.soundEvents, goal: next });
                    sound.unlock();
                    soundPlayer.play('goal');
                    update({ soundEvents: { ...prefs.soundEvents, goal: next } });
                  }}
                >
                  Propio
                </button>
              )}
            </div>
            <div className="qp-label">Efectos de gol</div>
            <div className="segmented qp-seg">
              {EFFECTS.map((l) => (
                <button key={l.id} className="seg" aria-pressed={prefs.effects === l.id} onClick={() => update({ effects: l.id })}>
                  {l.label}
                </button>
              ))}
            </div>
            <div className="segmented qp-buttons">
              <button className="seg qp-big" aria-pressed={prefs.voice} onClick={() => update({ voice: !prefs.voice })}>
                🎙 Locutor
              </button>
              {fullscreenAvailable() && (
                <button className="seg qp-big" onClick={() => void toggleFullscreen()}>
                  ⛶ Pantalla completa
                </button>
              )}
              <button
                className="seg qp-big"
                onClick={() => {
                  setOpen(false);
                  navigate({ name: 'settings' });
                }}
              >
                ⚙ Ajustes
              </button>
            </div>
            <button className="qp-close" aria-label="Cerrar" onClick={() => setOpen(false)}>
              ︽
            </button>
          </div>
        </div>
      )}
    </>
  );
}
