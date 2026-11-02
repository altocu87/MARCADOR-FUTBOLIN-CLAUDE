/**
 * Ajustes → Cuenta: el dueño de la mesa entra con su correo y contraseña y los datos de la mesa (jugadores,
 * partidos, torneos y ajustes) se guardan en su cuenta en la nube. Sin cuenta, la app sigue en local.
 */
import { useState } from 'react';
import { useApp } from '../../app/AppContext';
import { Modal, Toggle } from '../components/common';
import { BoardQrScanner } from '../components/BoardLink';
import { LoginSheet, type LoginMode } from '../components/LoginSheet';

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
  const [sheet, setSheet] = useState<LoginMode | null>(null);
  const [confirmOut, setConfirmOut] = useState(false);
  const [scanning, setScanning] = useState(false);

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
          <div style={{ marginTop: 14 }}>
            <Toggle checked={cloud.rememberSession} onChange={cloud.setRememberSession} label="Recordar mi sesión"
              help="Mantiene tu acceso al cerrar y volver a abrir la app, hasta que cierres sesión. Sin marcar, dura hasta cerrar esta pestaña. La placa tiene su propia sesión y su propia opción." />
          </div>
          <div style={{ display: 'flex', gap: 10, marginTop: 14, flexWrap: 'wrap' }}>
            <button className="btn btn-primary" disabled={demoMode || cloud.status === 'syncing'} onClick={() => void cloud.syncNow()}>
              Sincronizar ahora
            </button>
            <button className="btn" onClick={() => setConfirmOut(true)}>Cerrar sesión</button>
          </div>
        </div>
        <div className="card">
          <div className="label">Placa</div>
          <div style={{ fontSize: 18, fontWeight: 800, marginTop: 8 }}>Conectar la pantalla del futbolín</div>
          <p className="muted" style={{ margin: '6px 0 0', fontSize: 14 }}>
            En la placa: AJUSTES → CUENTA → ENTRAR CON QR. Lee el QR con el móvil y la placa entra sola con esta cuenta, sin
            escribir nada.
          </p>
          <button className="btn btn-primary" style={{ marginTop: 12 }} onClick={() => setScanning(true)}>
            Conectar una placa (QR)
          </button>
        </div>
        {scanning && <BoardQrScanner onClose={() => setScanning(false)} />}
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
            <p style={{ margin: 0 }}>Se cierra solo en este navegador. La placa y los demás aparatos siguen conectados. Los datos locales se conservan.</p>
          </Modal>
        )}
      </div>
    );
  }

  // Sin cuenta: dos botones grandes que abren la pantalla de entrar (vertical, con el teclado del móvil).
  return (
    <div className="card account-out">
      <div className="label">Cuenta de la mesa</div>
      <div className="account-out-title">Entra para guardar tu mesa en la nube</div>
      <p className="muted account-out-text">
        Jugadores, partidos, torneos y los sonidos de la pantalla, en el móvil, el PC y la placa.
      </p>
      <div className="account-out-actions">
        <button className="btn btn-primary btn-lg" onClick={() => setSheet('login')}>
          Entrar
        </button>
        <button className="btn btn-lg" onClick={() => setSheet('signup')}>
          Crear cuenta
        </button>
      </div>
      {sheet && <LoginSheet initial={sheet} onClose={() => setSheet(null)} />}
    </div>
  );
}
