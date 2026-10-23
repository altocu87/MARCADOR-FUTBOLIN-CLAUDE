/** Reposo: la nube solo se usa con la app quieta y sin partido (src/app/idle.ts). */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import {
  AutoSyncScheduler,
  IDLE_MS,
  IdleMonitor,
  OVERDUE_MS,
  CHECK_EVERY_MS,
  RETRY_MS,
  STALE_ACTIVE_MATCH_MS,
  activeMatchInProgress,
  autoSyncReason,
  type AutoReason,
} from '../src/app/idle';

beforeEach(() => {
  vi.useFakeTimers();
  vi.setSystemTime(new Date('2026-10-23T10:00:00Z'));
});
afterEach(() => vi.useRealTimers());

/** Sincronización de mentira: apunta cada llamada y se queda esperando hasta que se la suelta. */
function fakeRun() {
  const calls: { signal: AbortSignal; reason: AutoReason; finish(ok: boolean): void }[] = [];
  const run = (signal: AbortSignal, reason: AutoReason) =>
    new Promise<boolean | null>((resolve) => {
      calls.push({ signal, reason, finish: resolve });
      signal.addEventListener('abort', () => resolve(null));
    });
  return { calls, run };
}

const now = () => Date.now();
const immediate = (fn: () => void) => {
  fn();
  return () => undefined;
};

describe('vigilante de reposo', () => {
  it('reposo tras 90 s sin tocar; un toque lo quita', () => {
    const m = new IdleMonitor(IDLE_MS, now);
    expect(m.idle).toBe(false);
    vi.advanceTimersByTime(IDLE_MS - 1000);
    expect(m.idle).toBe(false);
    m.activity();  // un toque a los 89 s: vuelve a contar
    vi.advanceTimersByTime(IDLE_MS - 1000);
    expect(m.idle).toBe(false);
    vi.advanceTimersByTime(1000);
    expect(m.idle).toBe(true);
    m.activity();
    expect(m.idle).toBe(false);
  });

  it('jugando nunca hay reposo, ni con la pestaña oculta', () => {
    const m = new IdleMonitor(IDLE_MS, now);
    m.setPlaying(true);
    vi.advanceTimersByTime(IDLE_MS * 3);
    m.setHidden(true);
    expect(m.idle).toBe(false);
    m.setPlaying(false);
    expect(m.idle).toBe(true);  // pestaña oculta y sin partido
  });

  it('pestaña oculta sin partido = reposo al momento', () => {
    const m = new IdleMonitor(IDLE_MS, now);
    const seen: boolean[] = [];
    m.subscribe(() => seen.push(m.idle));
    m.setHidden(true);
    expect(m.idle).toBe(true);
    m.setHidden(false);
    expect(m.idle).toBe(false);
    expect(seen).toEqual([true, false]);
  });

  it('un partido a medias guardado cuenta como «jugando» si es reciente', () => {
    expect(activeMatchInProgress(null, now())).toBe(false);
    expect(activeMatchInProgress({ savedAt: now() - 60_000 }, now())).toBe(true);
    expect(activeMatchInProgress({ savedAt: now() - STALE_ACTIVE_MATCH_MS - 1 }, now())).toBe(false);
  });
});

describe('cuándo toca sincronizar', () => {
  const base = () => ({ idle: true, playing: false, pendingSince: null, lastCheck: now(), checkEveryMs: CHECK_EVERY_MS, now: now() });
  it('en reposo con algo pendiente; nunca jugando', () => {
    expect(autoSyncReason({ ...base(), pendingSince: now() })).toBe('pending');
    expect(autoSyncReason({ ...base(), pendingSince: now(), playing: true })).toBeNull();
    expect(autoSyncReason({ ...base(), pendingSince: now(), idle: false })).toBeNull();
  });
  it('mira la nube cada 10 min como mucho y solo en reposo', () => {
    expect(autoSyncReason(base())).toBeNull();
    expect(autoSyncReason({ ...base(), now: now() + CHECK_EVERY_MS })).toBe('check');
    expect(autoSyncReason({ ...base(), now: now() + CHECK_EVERY_MS, idle: false })).toBeNull();
    expect(autoSyncReason({ ...base(), now: now() + CHECK_EVERY_MS, checkEveryMs: null })).toBeNull();
  });
  it('más de 30 min pendiente: aunque se use la app, pero no jugando', () => {
    const old = { ...base(), idle: false, pendingSince: now() - OVERDUE_MS };
    expect(autoSyncReason(old)).toBe('overdue');
    expect(autoSyncReason({ ...old, playing: true })).toBeNull();
    expect(autoSyncReason({ ...old, pendingSince: now() - OVERDUE_MS + 1000 })).toBeNull();
  });
});

describe('lanzador automático', () => {
  it('los cambios solo marcan pendiente; se sincroniza al entrar en reposo', async () => {
    const m = new IdleMonitor(IDLE_MS, now);
    const f = fakeRun();
    const s = new AutoSyncScheduler(m, f.run, { now, defer: immediate });
    s.start();
    s.markPending();
    s.markPending();
    expect(f.calls).toHaveLength(0);
    vi.advanceTimersByTime(IDLE_MS);
    expect(f.calls).toHaveLength(1);
    expect(f.calls[0].reason).toBe('pending');
    f.calls[0].finish(true);
    await vi.advanceTimersByTimeAsync(0);
    expect(s.pending).toBe(false);
    s.stop();
  });

  it('el primer toque cancela la automática y queda pendiente para el siguiente reposo', async () => {
    const m = new IdleMonitor(IDLE_MS, now);
    const f = fakeRun();
    const s = new AutoSyncScheduler(m, f.run, { now, defer: immediate });
    s.start();
    s.markPending();
    vi.advanceTimersByTime(IDLE_MS);
    expect(f.calls).toHaveLength(1);
    m.activity();
    expect(f.calls[0].signal.aborted).toBe(true);
    await vi.advanceTimersByTimeAsync(0);
    expect(s.pending).toBe(true);
    await vi.advanceTimersByTimeAsync(IDLE_MS);
    expect(f.calls).toHaveLength(2);
    s.stop();
  });

  it('entrar en un partido cancela; jugando no se lanza nada', async () => {
    const m = new IdleMonitor(IDLE_MS, now);
    const f = fakeRun();
    const s = new AutoSyncScheduler(m, f.run, { now, defer: immediate });
    s.start();
    s.markPending();
    m.setHidden(true);
    expect(f.calls).toHaveLength(1);
    m.setPlaying(true);
    expect(f.calls[0].signal.aborted).toBe(true);
    await vi.advanceTimersByTimeAsync(OVERDUE_MS * 2);
    expect(f.calls).toHaveLength(1);
    s.stop();
  });

  it('lleva más de 30 min pendiente: se sincroniza aunque se toque, y no la cancelan los toques', async () => {
    const m = new IdleMonitor(IDLE_MS, now);
    const f = fakeRun();
    const s = new AutoSyncScheduler(m, f.run, { now, defer: immediate });
    s.start();
    s.markPending();
    // Alguien usando los menús sin parar.
    for (let t = 0; t < OVERDUE_MS + 60_000; t += 30_000) {
      m.activity();
      await vi.advanceTimersByTimeAsync(30_000);
    }
    expect(f.calls).toHaveLength(1);
    expect(f.calls[0].reason).toBe('overdue');
    m.activity();
    expect(f.calls[0].signal.aborted).toBe(false);
    m.setPlaying(true);
    expect(f.calls[0].signal.aborted).toBe(true);
    s.stop();
  });

  it('en reposo mira la nube cada 10 min, sin cambios aquí', async () => {
    const m = new IdleMonitor(IDLE_MS, now);
    const f = fakeRun();
    const s = new AutoSyncScheduler(m, f.run, { now, defer: immediate });
    s.start();
    m.setHidden(true);
    expect(f.calls.map((c) => c.reason)).toEqual(['check']);
    f.calls[0].finish(true);
    await vi.advanceTimersByTimeAsync(CHECK_EVERY_MS - 60_000);
    expect(f.calls).toHaveLength(1);
    await vi.advanceTimersByTimeAsync(60_000);
    expect(f.calls).toHaveLength(2);
    s.stop();
  });

  it('si falla (sin conexión) espera un rato antes de reintentar', async () => {
    const m = new IdleMonitor(IDLE_MS, now);
    const f = fakeRun();
    const s = new AutoSyncScheduler(m, f.run, { now, defer: immediate, checkEveryMs: null });
    s.start();
    s.markPending();
    m.setHidden(true);
    f.calls[0].finish(false);
    await vi.advanceTimersByTimeAsync(RETRY_MS - 60_000);
    expect(f.calls).toHaveLength(1);
    await vi.advanceTimersByTimeAsync(60_000);
    expect(f.calls).toHaveLength(2);
    s.stop();
  });

  it('un cambio durante la sincronización sigue pendiente', async () => {
    const m = new IdleMonitor(IDLE_MS, now);
    const f = fakeRun();
    const s = new AutoSyncScheduler(m, f.run, { now, defer: immediate, checkEveryMs: null });
    s.start();
    s.markPending();
    m.setHidden(true);
    s.markPending();  // p. ej. llega un partido de la placa mientras tanto
    f.calls[0].finish(true);
    await vi.advanceTimersByTimeAsync(0);
    expect(s.pending).toBe(true);
    expect(f.calls).toHaveLength(2);  // sigue en reposo: otra vuelta
    s.stop();
  });

  it('«Sincronizar ahora» bien hecho borra lo pendiente', () => {
    const m = new IdleMonitor(IDLE_MS, now);
    const s = new AutoSyncScheduler(m, fakeRun().run, { now, defer: immediate });
    s.markPending();
    const mark = s.begin();
    s.end(mark, true);
    expect(s.pending).toBe(false);
  });
});
