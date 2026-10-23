/**
 * Reposo: cuándo se puede hablar con la nube sin molestar (decisión del dueño).
 * Lo de este aparato se guarda siempre al momento; la subida y bajada a la nube solo se hace en reposo:
 *   - nadie ha tocado la pantalla ni el teclado en 90 s, o la pestaña está oculta,
 *   - y no se está jugando (pantallas de partido o un partido a medias guardado).
 * Los cambios solo marcan «pendiente». Se sincroniza al entrar en reposo si hay algo pendiente, se mira la nube
 * cada 10 min como mucho (solo en reposo) y, por seguridad, si lleva más de 30 min pendiente y no se juega, se
 * sincroniza aunque se esté usando la app en los menús.
 * Al primer toque o al entrar en un partido se cancela la sincronización automática que esté en marcha.
 *
 * Sin React ni ventana: se prueba suelto (tests/idle.test.ts). Los ganchos para la app están en useIdle.ts.
 */

export const IDLE_MS = 90_000;
export const CHECK_EVERY_MS = 10 * 60_000;
export const OVERDUE_MS = 30 * 60_000;
/** Tras un fallo (sin conexión…), no reintentar enseguida. */
export const RETRY_MS = 2 * 60_000;
/** Cada cuánto se repasa si toca sincronizar (no hace nada si no toca). */
export const TICK_MS = 60_000;
/** Un partido a medias guardado hace más de esto se da por abandonado (no bloquea la nube para siempre). */
export const STALE_ACTIVE_MATCH_MS = 3 * 60 * 60_000;

// ---------------------------------------------------------------- vigilante de reposo
export class IdleMonitor {
  private lastActivity: number;
  private hidden = false;
  private playingNow = false;
  private idleNow = false;
  private timer: ReturnType<typeof setTimeout> | null = null;
  private listeners = new Set<() => void>();

  constructor(
    private readonly idleMs = IDLE_MS,
    private readonly now: () => number = () => Date.now(),
  ) {
    this.lastActivity = now();
    this.arm();
  }

  get idle(): boolean {
    return this.idleNow;
  }

  get playing(): boolean {
    return this.playingNow;
  }

  /** Avisa de cada cambio de reposo o de «se está jugando». */
  subscribe(fn: () => void): () => void {
    this.listeners.add(fn);
    return () => void this.listeners.delete(fn);
  }

  /** Un toque, una tecla… */
  activity(): void {
    this.lastActivity = this.now();
    this.update();
    this.arm();
  }

  setHidden(hidden: boolean): void {
    if (hidden === this.hidden) return;
    this.hidden = hidden;
    // Al volver a la pestaña se cuenta como uso.
    if (!hidden) this.lastActivity = this.now();
    this.update();
    this.arm();
  }

  setPlaying(playing: boolean): void {
    if (playing === this.playingNow) return;
    this.playingNow = playing;
    // Al acabar un partido se empieza a contar desde ese momento.
    if (!playing) this.lastActivity = this.now();
    this.update(true);
    this.arm();
  }

  dispose(): void {
    if (this.timer) clearTimeout(this.timer);
    this.timer = null;
    this.listeners.clear();
  }

  private update(force = false): void {
    const idle = !this.playingNow && (this.hidden || this.now() - this.lastActivity >= this.idleMs);
    if (idle === this.idleNow && !force) return;
    this.idleNow = idle;
    for (const fn of [...this.listeners]) fn();
  }

  /** Un solo temporizador: al vencer mira cuánto falta de verdad (los toques no lo reinician). Jugando no cuenta. */
  private arm(): void {
    if (this.timer || this.idleNow || this.playingNow) return;
    const wait = Math.max(0, this.lastActivity + this.idleMs - this.now());
    this.timer = setTimeout(() => {
      this.timer = null;
      this.update();
      this.arm();
    }, wait);
  }
}

// ---------------------------------------------------------------- ¿toca sincronizar?
export type AutoReason = 'pending' | 'check' | 'overdue';

export function autoSyncReason(s: {
  idle: boolean;
  playing: boolean;
  pendingSince: number | null;
  lastCheck: number | null;
  /** null: no se mira la nube por si acaso (solo se sube lo pendiente). */
  checkEveryMs: number | null;
  now: number;
}): AutoReason | null {
  if (s.playing) return null;
  if (s.idle && s.pendingSince !== null) return 'pending';
  if (s.idle && s.checkEveryMs !== null && (s.lastCheck === null || s.now - s.lastCheck >= s.checkEveryMs)) return 'check';
  if (!s.idle && s.pendingSince !== null && s.now - s.pendingSince >= OVERDUE_MS) return 'overdue';
  return null;
}

/** ¿Hay un partido a medias guardado (reciente)? */
export function activeMatchInProgress(snapshot: { savedAt: number } | null | undefined, now: number): boolean {
  return !!snapshot && now - snapshot.savedAt < STALE_ACTIVE_MATCH_MS;
}

// ---------------------------------------------------------------- lanzador
/** Ejecuta `fn` cuando el navegador esté libre (requestIdleCallback; si no lo hay, un setTimeout). Devuelve cómo cancelarlo. */
export function whenBrowserIdle(fn: () => void): () => void {
  const g = globalThis as { requestIdleCallback?: (cb: () => void, o?: { timeout: number }) => number; cancelIdleCallback?: (id: number) => void };
  if (typeof g.requestIdleCallback === 'function') {
    const id = g.requestIdleCallback(fn, { timeout: 2000 });
    return () => g.cancelIdleCallback?.(id);
  }
  const t = setTimeout(fn, 200);
  return () => clearTimeout(t);
}

/** true: bien · false: falló (se espera un rato) · null: no se hizo (otra en marcha…). */
export type AutoRun = (signal: AbortSignal, reason: AutoReason) => Promise<boolean | null>;

export interface AutoSyncOptions {
  checkEveryMs?: number | null;
  now?: () => number;
  defer?: (fn: () => void) => () => void;
}

/**
 * Decide cuándo lanza una sincronización automática. Los cambios llaman a markPending(); las peticiones del
 * usuario (Sincronizar ahora, entrar…) no pasan por aquí, solo avisan con begin()/end() para borrar lo pendiente.
 */
export class AutoSyncScheduler {
  private pendingSince: number | null = null;
  private seq = 0;
  private lastCheck: number | null = null;
  private notBefore = 0;
  private current: { ctl: AbortController; reason: AutoReason } | null = null;
  private cancelDefer: (() => void) | null = null;
  private tick: ReturnType<typeof setInterval> | null = null;
  private unsubscribe: (() => void) | null = null;
  private readonly checkEveryMs: number | null;
  private readonly now: () => number;
  private readonly defer: (fn: () => void) => () => void;

  constructor(
    private readonly monitor: Pick<IdleMonitor, 'idle' | 'playing' | 'subscribe'>,
    private readonly run: AutoRun,
    opts: AutoSyncOptions = {},
  ) {
    this.checkEveryMs = opts.checkEveryMs === undefined ? CHECK_EVERY_MS : opts.checkEveryMs;
    this.now = opts.now ?? (() => Date.now());
    this.defer = opts.defer ?? whenBrowserIdle;
  }

  get pending(): boolean {
    return this.pendingSince !== null;
  }

  get running(): AutoReason | null {
    return this.current?.reason ?? null;
  }

  start(): void {
    if (this.tick) return;
    this.unsubscribe = this.monitor.subscribe(() => this.onMonitor());
    this.tick = setInterval(() => this.evaluate(), TICK_MS);
    this.evaluate();
  }

  stop(): void {
    this.unsubscribe?.();
    this.unsubscribe = null;
    if (this.tick) clearInterval(this.tick);
    this.tick = null;
    this.cancelDefer?.();
    this.cancelDefer = null;
    this.current?.ctl.abort();
  }

  /** Algo ha cambiado aquí: hay que subirlo cuando toque. */
  markPending(): void {
    this.seq++;
    if (this.pendingSince === null) this.pendingSince = this.now();
    this.evaluate();
  }

  /** Volvió la conexión: se puede reintentar ya (si toca). */
  nudge(): void {
    this.notBefore = 0;
    this.evaluate();
  }

  /** Una sincronización pedida por el usuario empieza: devuelve la marca para end(). */
  begin(): number {
    return this.seq;
  }

  /** …y ha terminado. Si fue bien y no ha cambiado nada desde que empezó, ya no queda nada pendiente. */
  end(mark: number, ok: boolean): void {
    if (!ok) return;
    this.lastCheck = this.now();
    if (mark === this.seq) this.pendingSince = null;
  }

  private onMonitor(): void {
    const c = this.current;
    // Al tocar se cancela la automática; la de «demasiado tiempo pendiente» solo al empezar a jugar.
    if (c && (this.monitor.playing || (!this.monitor.idle && c.reason !== 'overdue'))) c.ctl.abort();
    if (!this.monitor.idle) {
      this.cancelDefer?.();
      this.cancelDefer = null;
    }
    this.evaluate();
  }

  private reason(): AutoReason | null {
    if (this.current || this.now() < this.notBefore) return null;
    return autoSyncReason({
      idle: this.monitor.idle,
      playing: this.monitor.playing,
      pendingSince: this.pendingSince,
      lastCheck: this.lastCheck,
      checkEveryMs: this.checkEveryMs,
      now: this.now(),
    });
  }

  private evaluate(): void {
    if (this.cancelDefer || !this.reason()) return;
    let fired = false;
    const cancel = this.defer(() => {
      fired = true;
      this.cancelDefer = null;
      const reason = this.reason();  // puede haber cambiado mientras se esperaba
      if (reason) void this.launch(reason);
    });
    if (!fired) this.cancelDefer = cancel;
  }

  private async launch(reason: AutoReason): Promise<void> {
    const ctl = new AbortController();
    this.current = { ctl, reason };
    const mark = this.seq;
    this.lastCheck = this.now();
    let ok: boolean | null = false;
    try {
      ok = await this.run(ctl.signal, reason);
    } catch {
      ok = false;
    }
    this.current = null;
    if (ok) {
      if (mark === this.seq) this.pendingSince = null;
    } else if (ctl.signal.aborted) {
      // Cancelada: se repite en el próximo reposo.
    } else if (ok === false) {
      this.notBefore = this.now() + RETRY_MS;
    } else {
      this.notBefore = this.now() + TICK_MS;  // no se hizo (otra en marcha…): se mira en el próximo repaso
    }
    this.evaluate();
  }
}
