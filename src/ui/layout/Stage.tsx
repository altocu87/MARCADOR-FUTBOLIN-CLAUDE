import { useEffect, useState, type ReactNode } from 'react';
import { PortraitHint } from '../components/PortraitHint';

/** Lienzo de referencia (pantalla ESP32-S3 de 7"): 800 × 480 en horizontal. */
export const STAGE_W = 800;
export const STAGE_H = 480;
/** Límites del lienzo adaptativo: se ensancha en pantallas panorámicas y crece en 4:3. */
const MAX_W = 1100;
const MAX_H = 640;

export interface StageSize {
  width: number;
  height: number;
  scale: number;
  portrait: boolean;
}

/**
 * Calcula el lienzo lógico según la proporción de la ventana:
 * - Panorámica (16:9, 21:9…): alto 480 y ancho hasta 1100 → sin franjas laterales.
 * - Más cuadrada (4:3, 16:10): ancho 800 y alto hasta 640 → sin franjas arriba/abajo.
 * - Vertical: se mantiene 800 × 480 escalado (no se convierte en página vertical).
 * Nunca es menor que 800 × 480, así ningún control se recorta.
 */
export function computeStage(vw: number, vh: number, rotated = false): StageSize {
  // Girado: en vertical el lienzo se dibuja a 90° y usa la pantalla como si fuera horizontal.
  if (rotated && vw < vh) {
    const s = computeStage(vh, vw);
    return { ...s, portrait: true };
  }
  const aspect = vw / Math.max(1, vh);
  const portrait = aspect < 1;
  let width = STAGE_W;
  let height = STAGE_H;
  if (!portrait) {
    if (aspect >= STAGE_W / STAGE_H) width = Math.round(Math.min(MAX_W, STAGE_H * aspect));
    else height = Math.round(Math.min(MAX_H, STAGE_W / aspect));
  }
  const scale = Math.min(vw / width, vh / height);
  return { width, height, scale: scale > 0 ? scale : 1, portrait };
}

function viewportSize(): [number, number] {
  const vv = window.visualViewport;
  return [vv?.width ?? window.innerWidth, vv?.height ?? window.innerHeight];
}

/** Margen mínimo junto al borde superior del móvil (barra de notificaciones), aunque el sistema no lo indique. */
const MIN_TOP_MARGIN = 14;

interface Insets {
  top: number;
  right: number;
  bottom: number;
  left: number;
}

/** Zonas reservadas por el sistema (muesca, barra de estado, barra de gestos) en píxeles. */
function safeInsets(): Insets {
  const el = document.createElement('div');
  el.style.cssText =
    'position:fixed;visibility:hidden;pointer-events:none;' +
    'padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)';
  document.body.appendChild(el);
  const cs = getComputedStyle(el);
  const insets = {
    top: parseFloat(cs.paddingTop) || 0,
    right: parseFloat(cs.paddingRight) || 0,
    bottom: parseFloat(cs.paddingBottom) || 0,
    left: parseFloat(cs.paddingLeft) || 0,
  };
  el.remove();
  return { ...insets, top: Math.max(insets.top, MIN_TOP_MARGIN) };
}

/** Hueco útil: la pantalla menos las zonas del sistema, y su centro. */
function usableArea(): { w: number; h: number; cx: number; cy: number } {
  const [vw, vh] = viewportSize();
  const i = safeInsets();
  const w = Math.max(1, vw - i.left - i.right);
  const h = Math.max(1, vh - i.top - i.bottom);
  return { w, h, cx: i.left + w / 2, cy: i.top + h / 2 };
}

function readRotated(): boolean {
  try {
    return sessionStorage.getItem('mfv3:rotated') === '1';
  } catch {
    return false;
  }
}

export function Stage({ children }: { children: (size: StageSize) => ReactNode }) {
  const [rotated, setRotated] = useState(readRotated);
  const [area, setArea] = useState(usableArea);
  const size = computeStage(area.w, area.h, rotated);

  const chooseRotated = (value: boolean) => {
    try {
      sessionStorage.setItem('mfv3:rotated', value ? '1' : '0');
    } catch {
      // Sin almacenamiento de sesión: solo se aplica ahora.
    }
    setRotated(value);
  };

  useEffect(() => {
    const update = () => setArea(usableArea());
    update();
    window.addEventListener('resize', update);
    window.addEventListener('orientationchange', update);
    window.visualViewport?.addEventListener('resize', update);
    return () => {
      window.removeEventListener('resize', update);
      window.removeEventListener('orientationchange', update);
      window.visualViewport?.removeEventListener('resize', update);
    };
  }, []);

  const turn = size.portrait && rotated;

  return (
    <div className="viewport">
      <div
        className="stage"
        style={{
          // Centrado en el hueco útil (fuera de la barra de notificaciones y la muesca).
          left: area.cx,
          top: area.cy,
          width: size.width,
          height: size.height,
          transform: `translate(-50%, -50%) ${turn ? 'rotate(90deg) ' : ''}scale(${size.scale})`,
        }}
      >
        {children(size)}
      </div>
      {size.portrait && !rotated && <PortraitHint onRotate={() => chooseRotated(true)} />}
    </div>
  );
}
