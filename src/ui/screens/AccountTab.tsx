/**
 * Ajustes → Cuenta: el dueño de la mesa entra con su correo y contraseña y los datos de la mesa (jugadores,
 * partidos, torneos y ajustes) se guardan en su cuenta en la nube. Sin cuenta, la app sigue en local.
 */
import { useState } from 'react';
import { useApp } from '../../app/AppContext';
import { Modal } from '../components/common';

type Mode = 'login' | 'signup' | 'recover';

function ago(ts: number | null): string {
  if (!ts) return 'todavía no';
  const s = Math.round((Date.now() - ts) / 1000);
  if (s < 10) return 'ahora mismo';
  if (s < 60) return `hace ${s} s`;
  const m = Math.round(s / 60);
  return m < 60 ? `hace ${m} min` : `hace ${Math.round(m / 60)} h`;
}

export function AccountTab() {
  const { cloud, demoMode, toast } = useApp();
  const [mode, setMode] = useState<Mode>('login');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [busy, setBusy] = useState(false);
  const [msg, setMsg] = useState<{ text: string; ok?: boolean } | null>(null);
  const [confirmOut, setConfirmOut] = useState(false);

  if (!cloud.available) {
    return (
      <div className="settings-grid">
        <div className="card">
          <div className="label">Cuenta</div>
          <p className="muted" style={{ margin: '8px 0 0' }}>Esta versión de la app no está conectada a la nube. Todo se guarda en este aparato.</p>
        </div>
      </div>
    );
  }

  if (cloud.session) {
    const status =
      cloud.status === 'syncing'
        ? 'Sincronizando…'
        : cloud.status === 'error'
          ? cloud.error ?? 'No se pudo sincronizar'
          : `Sincronizado ${ago(cloud.lastSync)}`;
    return (
      <div className="settings-grid">
        <div className="card">
          <div className="label">Cuenta de la mesa</div>
          <div style={{ fontSize: 22, fontWeight: 800, marginTop: 8 }}>{cloud.session.email}</div>
          <p className="muted" style={{ margin: '6px 0 0', color: cloud.status === 'error' ? 'var(--danger)' : undefined }}>
            {demoMode ? 'En modo prueba no se sincroniza (los datos de prueba no se suben).' : status}
          </p>
          <p className="muted" style={{ margin: '10px 0 0', fontSize: 14 }}>
            Jugadores, partidos, torneos y ajustes de esta mesa se guardan en tu cuenta. En otro aparato o en la placa,
            entra con la misma cuenta y tendrás lo mismo.
          </p>
          <div style={{ display: 'flex', gap: 10, marginTop: 14, flexWrap: 'wrap' }}>
            <button className="btn btn-primary" disabled={demoMode || cloud.status === 'syncing'} onClick={() => void cloud.syncNow()}>
              Sincronizar ahora
            </button>
            <button className="btn" onClick={() => setConfirmOut(true)}>Cerrar sesión</button>
          </div>
        </div>
        {confirmOut && (
          <Modal
            title="¿Cerrar sesión?"
            onClose={() => setConfirmOut(false)}
            actions={
              <>
                <button className="btn btn-ghost" onClick={() => setConfirmOut(false)}>Cancelar</button>
                <button
                  className="btn btn-danger"
                  onClick={async () => {
                    await cloud.signOut();
                    setConfirmOut(false);
                    toast('Sesión cerrada');
                  }}
                >
                  Cerrar sesión
                </button>
              </>
            }
          >
            <p style={{ margin: 0 }}>Los datos se quedan en este aparato, pero dejan de guardarse en la nube hasta que vuelvas a entrar.</p>
          </Modal>
        )}
      </div>
    );
  }

  const submit = async () => {
    setMsg(null);
    if (!email.includes('@')) return setMsg({ text: 'Escribe tu correo' });
    if (mode !== 'recover' && password.length < 6) return setMsg({ text: 'La contraseña tiene que tener al menos 6 caracteres' });
    setBusy(true);
    try {
      if (mode === 'login') {
        await cloud.signIn(email, password);
        toast('Has entrado: sincronizando la mesa');
      } else if (mode === 'signup') {
        const confirm = await cloud.signUp(email, password);
        if (confirm) {
          setMsg({ text: 'Cuenta creada. Te hemos mandado un correo: ábrelo para confirmarla y luego entra aquí.', ok: true });
          setMode('login');
        } else toast('Cuenta creada: sincronizando la mesa');
      } else {
        await cloud.recover(email);
        setMsg({ text: 'Te hemos mandado un correo para poner una contraseña nueva.', ok: true });
        setMode('login');
      }
    } catch (err) {
      setMsg({ text: err instanceof Error ? err.message : 'No se pudo' });
    } finally {
      setBusy(false);
    }
  };

  const title = mode === 'login' ? 'Entra con la cuenta de la mesa' : mode === 'signup' ? 'Crear la cuenta de la mesa' : 'He olvidado la contraseña';
  const action = mode === 'login' ? 'Entrar' : mode === 'signup' ? 'Crear cuenta' : 'Mandar correo';
  return (
    <div className="card" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 20, alignItems: 'start' }}>
      <div>
        <div className="label">Cuenta</div>
        <div style={{ fontSize: 22, fontWeight: 800, margin: '6px 0 6px' }}>{title}</div>
        <p className="muted" style={{ margin: '0 0 14px', fontSize: 14 }}>
          La cuenta es del dueño de la mesa: dentro se guardan sus jugadores, partidos y torneos.
        </p>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
          {mode !== 'login' && <button className="btn" onClick={() => { setMode('login'); setMsg(null); }}>Ya tengo cuenta</button>}
          {mode !== 'signup' && <button className="btn" onClick={() => { setMode('signup'); setMsg(null); }}>Crear cuenta</button>}
          {mode !== 'recover' && <button className="btn btn-ghost" onClick={() => { setMode('recover'); setMsg(null); }}>He olvidado la contraseña</button>}
        </div>
      </div>
      <form
        onSubmit={(e) => {
          e.preventDefault();
          void submit();
        }}
        style={{ display: 'flex', flexDirection: 'column', gap: 10 }}
      >
        <label className="field">
          <span className="label">Correo</span>
          <input className="input" type="email" autoComplete="email" value={email} onChange={(e) => setEmail(e.target.value)} />
        </label>
        {mode !== 'recover' && (
          <label className="field">
            <span className="label">Contraseña</span>
            <input
              className="input"
              type="password"
              autoComplete={mode === 'signup' ? 'new-password' : 'current-password'}
              value={password}
              onChange={(e) => setPassword(e.target.value)}
            />
          </label>
        )}
        {msg && <p style={{ margin: 0, color: msg.ok ? 'var(--ok)' : 'var(--danger)' }}>{msg.text}</p>}
        <button className="btn btn-primary" type="submit" disabled={busy} style={{ minHeight: 56 }}>
          {busy ? 'Un momento…' : action}
        </button>
      </form>
    </div>
  );
}
