/**
 * Capacidades del dispositivo: pantalla completa, mantener la pantalla encendida
 * y orientación. Todo es opcional: si el navegador no lo admite, no pasa nada.
 */

export function fullscreenAvailable(): boolean {
  return typeof document !== 'undefined' && !!document.documentElement.requestFullscreen;
}

export async function toggleFullscreen(): Promise<void> {
  try {
    if (document.fullscreenElement) {
      await document.exitFullscreen();
      return;
    }
    await document.documentElement.requestFullscreen({ navigationUI: 'hide' });
    // En móviles, intentar fijar horizontal (solo funciona en pantalla completa).
    const orientation = screen.orientation as ScreenOrientation & { lock?: (o: string) => Promise<void> };
    await orientation.lock?.('landscape').catch(() => undefined);
  } catch {
    // Sin permiso o no soportado.
  }
}

type WakeLockSentinelLike = { release(): Promise<void>; released?: boolean };

class WakeLockService {
  private sentinel: WakeLockSentinelLike | null = null;
  private wanted = false;

  get available(): boolean {
    return typeof navigator !== 'undefined' && 'wakeLock' in navigator;
  }

  get active(): boolean {
    return !!this.sentinel && !this.sentinel.released;
  }

  async enable(): Promise<void> {
    this.wanted = true;
    await this.acquire();
  }

  async disable(): Promise<void> {
    this.wanted = false;
    await this.sentinel?.release().catch(() => undefined);
    this.sentinel = null;
  }

  /** El navegador libera el bloqueo al ocultar la pestaña: se vuelve a pedir al volver. */
  async onVisible(): Promise<void> {
    if (this.wanted && document.visibilityState === 'visible' && !this.active) await this.acquire();
  }

  private async acquire(): Promise<void> {
    if (!this.available || document.visibilityState !== 'visible') return;
    try {
      const nav = navigator as Navigator & { wakeLock: { request(type: 'screen'): Promise<WakeLockSentinelLike> } };
      this.sentinel = await nav.wakeLock.request('screen');
    } catch {
      this.sentinel = null;
    }
  }
}

export const wakeLock = new WakeLockService();
