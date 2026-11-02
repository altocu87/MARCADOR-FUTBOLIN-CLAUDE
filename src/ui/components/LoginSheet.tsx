/**
 * Entrar en la cuenta (o crearla, o recuperar la contraseña) en una pantalla aparte, en vertical y FUERA del marcador:
 * así se escribe con el teclado del propio móvil (con sus sugerencias, el correo y la contraseña guardados…), sin el
 * teclado dibujado dentro del juego girado ni el lienzo de 800 × 480 encogido detrás del teclado.
 *
 * Se pinta encima de todo con un portal en <body> (el lienzo está girado y escalado: dentro de él, «fixed» no ocupa la
 * pantalla). En los móviles que lo permiten (Android a pantalla completa o instalada) se pone en vertical sola mientras
 * está abierta; si no, se pide girar el móvil y, aun en horizontal, la página se desplaza para que el campo se vea.
 */
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { createPortal } from 'react-dom';
import { useApp } from '../../app/AppContext';
import { Toggle } from './common';

export type LoginMode = 'login' | 'signup' | 'recover';

type Orientation = ScreenOrientation & { lock?: (o: string) => Promise<void>; unlock?: () => void };

/** Vertical mientras está abierta (si el navegador deja), y al cerrar, como estaba. */
function usePortraitWhileOpen() {
  useEffect(() => {
    const o = (typeof screen !== 'undefined' ? screen.orientation : undefined) as Orientation | undefined;
    void o?.lock?.('portrait').catch(() => undefined);
    return () => {
      // A pantalla completa la app va fija en horizontal (device.ts); si no, que gire libre.
      if (document.fullscreenElement) void o?.lock?.('landscape').catch(() => undefined);
      else {
        try {
          o?.unlock?.();
        } catch {
          /* no se puede */
        }
      }
    };
  }, []);
}

export function LoginSheet({ initial = 'login', onClose }: { initial?: LoginMode; onClose: () => void }) {
  const { cloud, toast } = useApp();
  const [mode, setMode] = useState<LoginMode>(initial);
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [show, setShow] = useState(false);
  const [busy, setBusy] = useState(false);
  const [msg, setMsg] = useState<{ text: string; ok?: boolean } | null>(null);
  const emailRef = useRef<HTMLInputElement>(null);
  const passRef = useRef<HTMLInputElement>(null);
  usePortraitWhileOpen();

  // Sin desplazar la página del juego que queda detrás; al abrir, el cursor en el correo.
  useEffect(() => {
    const prev = document.body.style.overflow;
    document.body.style.overflow = 'hidden';
    const t = window.setTimeout(() => emailRef.current?.focus(), 250);
    const esc = (e: KeyboardEvent) => e.key === 'Escape' && onClose();
    window.addEventListener('keydown', esc);
    return () => {
      document.body.style.overflow = prev;
      window.clearTimeout(t);
      window.removeEventListener('keydown', esc);
    };
  }, [onClose]);

  const go = (m: LoginMode) => {
    setMode(m);
    setMsg(null);
  };

  const submit = async (e?: FormEvent) => {
    e?.preventDefault();
    setMsg(null);
    const mail = email.trim();
    if (!/^\S+@\S+\.\S+$/.test(mail)) {
      setMsg({ text: 'Escribe bien tu correo (por ejemplo, nombre@gmail.com)' });
      emailRef.current?.focus();
      return;
    }
    if (mode !== 'recover' && password.length < 6) {
      setMsg({ text: 'La contraseña tiene que tener al menos 6 letras o números' });
      passRef.current?.focus();
      return;
    }
    setBusy(true);
    try {
      if (mode === 'login') {
        await cloud.signIn(mail, password);
        toast('Has entrado: sincronizando la mesa');
        onClose();
      } else if (mode === 'signup') {
        const confirm = await cloud.signUp(mail, password);
        if (confirm) {
          setMsg({ text: 'Cuenta creada. Te hemos mandado un correo: ábrelo para confirmarla y luego entra aquí.', ok: true });
          setMode('login');
        } else {
          toast('Cuenta creada: sincronizando la mesa');
          onClose();
        }
      } else {
        await cloud.recover(mail);
        setMsg({ text: 'Te hemos mandado un correo para poner una contraseña nueva.', ok: true });
        setMode('login');
      }
    } catch (err) {
      setMsg({ text: err instanceof Error ? err.message : 'No se pudo. Prueba otra vez.' });
    } finally {
      setBusy(false);
    }
  };

  const title = mode === 'login' ? 'Entra en tu cuenta' : mode === 'signup' ? 'Crea tu cuenta' : 'Recuperar la contraseña';
  const action = mode === 'login' ? 'Entrar' : mode === 'signup' ? 'Crear cuenta' : 'Mandarme el correo';
  const sub =
    mode === 'recover'
      ? 'Te mandamos un correo para poner una contraseña nueva.'
      : 'La cuenta de la mesa: jugadores, partidos, torneos y sonidos, en todos tus aparatos.';

  return createPortal(
    <div className="login-sheet" role="dialog" aria-modal="true" aria-label={title}>
      <div className="login-turn" aria-hidden="true">
        <span className="login-turn-icon">📱</span>
        Pon el móvil en vertical para escribir más cómodo
      </div>
      <div className="login-body">
        <div className="login-top">
          <button type="button" className="login-close" onClick={onClose} aria-label="Volver">
            ‹ Volver
          </button>
        </div>
        <h1 className="login-title">{title}</h1>
        <p className="login-sub">{sub}</p>

        {mode !== 'recover' && (
          <div className="login-tabs" role="tablist">
            <button type="button" role="tab" aria-selected={mode === 'login'} className={mode === 'login' ? 'is-on' : ''} onClick={() => go('login')}>
              Entrar
            </button>
            <button type="button" role="tab" aria-selected={mode === 'signup'} className={mode === 'signup' ? 'is-on' : ''} onClick={() => go('signup')}>
              Crear cuenta
            </button>
          </div>
        )}

        <form className="login-form" onSubmit={(e) => void submit(e)} noValidate>
          <label className="login-field">
            <span>Correo</span>
            <input
              ref={emailRef}
              type="email"
              inputMode="email"
              name="email"
              autoComplete={mode === 'signup' ? 'email' : 'username'}
              autoCapitalize="none"
              autoCorrect="off"
              spellCheck={false}
              enterKeyHint={mode === 'recover' ? 'send' : 'next'}
              placeholder="nombre@gmail.com"
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter' && mode !== 'recover') {
                  e.preventDefault();
                  passRef.current?.focus();
                }
              }}
            />
          </label>
          {mode !== 'recover' && (
            <label className="login-field">
              <span>Contraseña</span>
              <div className="login-pass">
                <input
                  ref={passRef}
                  type={show ? 'text' : 'password'}
                  name="password"
                  autoComplete={mode === 'signup' ? 'new-password' : 'current-password'}
                  autoCapitalize="none"
                  autoCorrect="off"
                  spellCheck={false}
                  enterKeyHint="go"
                  placeholder={mode === 'signup' ? 'Al menos 6 letras o números' : 'Tu contraseña'}
                  value={password}
                  onChange={(e) => setPassword(e.target.value)}
                />
                <button type="button" className="login-eye" onClick={() => setShow((s) => !s)} aria-label={show ? 'Ocultar la contraseña' : 'Ver la contraseña'}>
                  {show ? 'Ocultar' : 'Ver'}
                </button>
              </div>
            </label>
          )}
          {mode !== 'recover' && (
            <div className="login-remember">
              <Toggle
                checked={cloud.rememberSession}
                onChange={cloud.setRememberSession}
                label="Recordarme en este aparato"
                help="Sigues dentro al cerrar y volver a abrir la app, hasta que cierres sesión. Sin marcar, dura hasta cerrar esta pestaña."
              />
            </div>
          )}
          {msg && (
            <p className={`login-msg ${msg.ok ? 'ok' : 'bad'}`} role="alert">
              {msg.text}
            </p>
          )}
          <button className="login-submit" type="submit" disabled={busy}>
            {busy ? 'Un momento…' : action}
          </button>
        </form>

        <div className="login-links">
          {mode === 'login' && (
            <button type="button" onClick={() => go('recover')}>
              He olvidado la contraseña
            </button>
          )}
          {mode === 'recover' && (
            <button type="button" onClick={() => go('login')}>
              Volver a entrar
            </button>
          )}
        </div>
      </div>
    </div>,
    document.body,
  );
}
