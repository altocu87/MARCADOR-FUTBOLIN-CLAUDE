/**
 * Teclado propio de la app, dibujado DENTRO del juego. Con el móvil en vertical el juego se ve girado
 * (en horizontal) y el teclado del sistema saldría en vertical, de lado: no se puede girar desde la web.
 * Así que, mientras el juego está girado, al tocar un campo de texto se esconde el del sistema
 * (inputmode="none") y sale este, girado con el juego.
 */
import { useEffect, useRef, useState, type PointerEvent } from 'react';

const ROWS = [
  ['1', '2', '3', '4', '5', '6', '7', '8', '9', '0'],
  ['q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p'],
  ['a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'ñ'],
  ['z', 'x', 'c', 'v', 'b', 'n', 'm'],
];
const SYMBOLS = ['.', ',', '-', '_', '@', '/', ':', '¿', '?', '¡', '!', "'", '&', '#'];

/** Campos que escriben texto (no casillas, deslizadores ni archivos). */
export function isTextField(el: EventTarget | null): el is HTMLInputElement | HTMLTextAreaElement {
  if (el instanceof HTMLTextAreaElement) return !el.readOnly && !el.disabled;
  if (!(el instanceof HTMLInputElement) || el.readOnly || el.disabled) return false;
  return ['text', 'search', 'url', 'email', 'tel', 'number', 'password', ''].includes(el.type);
}

/** Cambia el valor como si se hubiera tecleado (React se entera por el evento «input»). */
export function typeInto(el: HTMLInputElement | HTMLTextAreaElement, edit: (value: string, start: number, end: number) => { value: string; caret: number }) {
  let start = el.value.length;
  let end = start;
  try {
    start = el.selectionStart ?? start;
    end = el.selectionEnd ?? start;
  } catch {
    /* number/email no tienen selección */
  }
  const next = edit(el.value, start, end);
  if (el.maxLength > 0 && next.value.length > el.maxLength && next.value.length > el.value.length) return;
  const proto = el instanceof HTMLTextAreaElement ? HTMLTextAreaElement.prototype : HTMLInputElement.prototype;
  Object.getOwnPropertyDescriptor(proto, 'value')?.set?.call(el, next.value);
  el.dispatchEvent(new Event('input', { bubbles: true }));
  try {
    el.setSelectionRange(next.caret, next.caret);
  } catch {
    /* sin selección */
  }
}

const insert = (text: string) => (v: string, s: number, e: number) => ({ value: v.slice(0, s) + text + v.slice(e), caret: s + text.length });
const backspace = (v: string, s: number, e: number) =>
  s !== e ? { value: v.slice(0, s) + v.slice(e), caret: s } : s > 0 ? { value: v.slice(0, s - 1) + v.slice(s), caret: s - 1 } : { value: v, caret: 0 };

export function OnScreenKeyboard({ stage }: { stage: () => HTMLElement | null }) {
  const [target, setTarget] = useState<HTMLInputElement | HTMLTextAreaElement | null>(null);
  const [shift, setShift] = useState(true);
  const [mode, setMode] = useState<'abc' | 'sym' | 'acc'>('abc');
  const [, setTick] = useState(0);
  const repeat = useRef<number | undefined>(undefined);

  useEffect(() => {
    // Antes de que el campo reciba el foco: que no salga el teclado del sistema.
    const down = (e: Event) => {
      if (isTextField(e.target) && stage()?.contains(e.target)) e.target.inputMode = 'none';
    };
    const focusIn = (e: FocusEvent) => {
      const el = e.target;
      if (!isTextField(el) || !stage()?.contains(el)) return;
      if (el.inputMode !== 'none') {
        // Recibió el foco solo (al abrir una ventana): se le quita el del sistema y se vuelve a enfocar.
        el.inputMode = 'none';
        el.blur();
        el.focus({ preventScroll: true });
        return;
      }
      setTarget(el);
      setShift(el.value.length === 0);
      setMode('abc');
    };
    const focusOut = (e: FocusEvent) => {
      if (!isTextField(e.target)) return;
      // Un momento después, por si el foco pasa a otro campo.
      window.setTimeout(() => {
        const a = document.activeElement;
        if (!isTextField(a) || !stage()?.contains(a)) setTarget(null);
      }, 0);
    };
    const input = () => setTick((t) => t + 1);
    document.addEventListener('pointerdown', down, true);
    document.addEventListener('focusin', focusIn, true);
    document.addEventListener('focusout', focusOut, true);
    document.addEventListener('input', input, true);
    // Si ya había un campo con el foco al girar el juego.
    const a = document.activeElement;
    if (isTextField(a) && stage()?.contains(a)) {
      a.inputMode = 'none';
      a.blur();
      a.focus({ preventScroll: true });
    }
    return () => {
      document.removeEventListener('pointerdown', down, true);
      document.removeEventListener('focusin', focusIn, true);
      document.removeEventListener('focusout', focusOut, true);
      document.removeEventListener('input', input, true);
      // Al dejar de estar girado, los campos vuelven a usar el teclado del sistema.
      stage()
        ?.querySelectorAll<HTMLInputElement>('input[inputmode="none"], textarea[inputmode="none"]')
        .forEach((el) => el.removeAttribute('inputmode'));
    };
  }, [stage]);

  useEffect(() => () => window.clearInterval(repeat.current), []);

  if (!target || !target.isConnected) return null;
  const el = target;

  const type = (text: string) => {
    typeInto(el, insert(shift ? text.toLocaleUpperCase('es') : text));
    if (shift) setShift(false);
    if (mode === 'acc') setMode('abc');
  };
  const done = () => {
    el.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
    el.blur();
    setTarget(null);
  };
  // Que tocar una tecla no le quite el foco al campo.
  const keep = (e: PointerEvent) => e.preventDefault();
  const startDelete = (e: PointerEvent) => {
    e.preventDefault();
    typeInto(el, backspace);
    window.clearInterval(repeat.current);
    const t0 = Date.now();
    repeat.current = window.setInterval(() => Date.now() - t0 > 400 && typeInto(el, backspace), 70);
  };
  const stopDelete = () => window.clearInterval(repeat.current);

  const label = el.getAttribute('aria-label') || el.closest('label')?.querySelector('.label')?.textContent || el.placeholder || 'Escribe';
  const letters = mode === 'acc' ? [['á', 'é', 'í', 'ó', 'ú', 'ü', 'ñ']] : mode === 'sym' ? [SYMBOLS.slice(0, 7), SYMBOLS.slice(7)] : ROWS.slice(1);

  return (
    <div className="osk" onPointerDown={keep} role="group" aria-label="Teclado">
      <div className="osk-preview">
        <span className="osk-label">{label}</span>
        <span className="osk-value">
          {el.value}
          <span className="osk-caret" />
        </span>
      </div>
      <div className="osk-row">
        {ROWS[0].map((k) => (
          <button key={k} className="osk-key" onClick={() => type(k)}>
            {k}
          </button>
        ))}
      </div>
      {letters.map((row, i) => (
        <div key={`${mode}${i}`} className="osk-row">
          {mode === 'abc' && i === 2 && (
            <button className={`osk-key osk-wide ${shift ? 'is-on' : ''}`} onClick={() => setShift((s) => !s)} aria-label="Mayúsculas">
              ⇧
            </button>
          )}
          {row.map((k) => (
            <button key={k} className="osk-key" onClick={() => type(k)}>
              {shift ? k.toLocaleUpperCase('es') : k}
            </button>
          ))}
          {i === letters.length - 1 && (
            <button className="osk-key osk-wide" onPointerDown={startDelete} onPointerUp={stopDelete} onPointerLeave={stopDelete} aria-label="Borrar">
              ⌫
            </button>
          )}
        </div>
      ))}
      <div className="osk-row">
        <button className="osk-key osk-wide" onClick={() => setMode(mode === 'sym' ? 'abc' : 'sym')}>
          {mode === 'sym' ? 'abc' : '.,?!'}
        </button>
        <button className="osk-key osk-wide" onClick={() => setMode(mode === 'acc' ? 'abc' : 'acc')} aria-label="Tildes">
          {mode === 'acc' ? 'abc' : 'á é'}
        </button>
        <button className="osk-key osk-space" onClick={() => type(' ')}>
          espacio
        </button>
        <button className="osk-key osk-done" onClick={done}>
          Hecho
        </button>
      </div>
    </div>
  );
}
