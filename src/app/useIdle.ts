/**
 * Ganchos de React para el reposo (la lógica está en idle.ts): un único vigilante para toda la app y un lanzador
 * de sincronizaciones automáticas para cada servicio (nube de la mesa, perfiles públicos).
 */
import { useEffect, useRef, useState } from 'react';
import { AutoSyncScheduler, IdleMonitor, type AutoRun, type AutoSyncOptions } from './idle';

/** Vigila toques, teclas y si la pestaña está oculta. `playing`: se está jugando (nunca hay reposo). */
export function useIdleMonitor(playing: boolean): IdleMonitor {
  const [monitor] = useState(() => new IdleMonitor());

  useEffect(() => {
    const touch = () => monitor.activity();
    const vis = () => monitor.setHidden(document.visibilityState === 'hidden');
    const opts = { capture: true, passive: true } as const;
    window.addEventListener('pointerdown', touch, opts);
    window.addEventListener('touchstart', touch, opts);
    window.addEventListener('keydown', touch, opts);
    document.addEventListener('visibilitychange', vis);
    vis();
    return () => {
      window.removeEventListener('pointerdown', touch, opts);
      window.removeEventListener('touchstart', touch, opts);
      window.removeEventListener('keydown', touch, opts);
      document.removeEventListener('visibilitychange', vis);
    };
  }, [monitor]);

  useEffect(() => monitor.setPlaying(playing), [monitor, playing]);
  return monitor;
}

/**
 * Lanzador de sincronizaciones automáticas: solo en reposo (ver idle.ts). Mientras `enabled` sea false no lanza
 * nada. `run` puede cambiar en cada render: se usa siempre el último.
 */
export function useAutoSync(monitor: IdleMonitor, enabled: boolean, run: AutoRun, opts: AutoSyncOptions = {}): AutoSyncScheduler {
  const runRef = useRef(run);
  runRef.current = run;
  const [scheduler] = useState(() => new AutoSyncScheduler(monitor, (signal, reason) => runRef.current(signal, reason), opts));

  useEffect(() => {
    if (!enabled) return;
    scheduler.start();
    // Al volver la conexión se reintenta (si hay reposo o lleva demasiado pendiente).
    const online = () => scheduler.nudge();
    window.addEventListener('online', online);
    return () => {
      window.removeEventListener('online', online);
      scheduler.stop();
    };
  }, [scheduler, enabled]);

  return scheduler;
}
