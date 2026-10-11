import { useState } from 'react';
import { builtInConnection, saveConnection, loadConnection } from '../backend';
import { ROLE_LABEL, money } from '../logic';
import { useAdmin } from '../store';
import type { PaymentProvider, Settings } from '../types';
import { Badge, ConfirmButton, EuroInput, Field, Header, download } from '../ui';

const PROVIDERS: { value: PaymentProvider; label: string; nota: string }[] = [
  { value: 'ninguno', label: 'Ninguno (todavía no se cobra)', nota: 'Las ventas del panel son de demostración.' },
  { value: 'lemonsqueezy', label: 'Lemon Squeezy', nota: 'Gestiona el IVA europeo por ti (comercio de registro). Recomendado para empezar.' },
  { value: 'paddle', label: 'Paddle', nota: 'Como Lemon Squeezy: se encarga del IVA y las facturas.' },
  { value: 'stripe', label: 'Stripe', nota: 'Comisión menor, pero el IVA de cada país lo declaras tú.' },
];

export function Ajustes() {
  const { data, saveSettings, can, toast, backend, session } = useAdmin();
  const [s, setS] = useState<Settings>(data.settings);
  const editable = can('editar_ajustes');
  const team = data.users.filter((u) => u.role === 'admin' || u.role === 'soporte');
  const conn = loadConnection();
  const [url, setUrl] = useState(conn?.url ?? '');
  const [key, setKey] = useState(conn?.anonKey ?? '');
  const changed = JSON.stringify(s) !== JSON.stringify(data.settings);

  const commit = async () => {
    const diff = (Object.keys(s) as (keyof Settings)[])
      .filter((k) => s[k] !== data.settings[k])
      .map((k) => `${k}: ${String(data.settings[k])} → ${String(s[k])}`)
      .join(', ');
    await saveSettings(s, diff || 'sin cambios');
    toast('Ajustes guardados');
  };

  return (
    <>
      <Header
        title="Ajustes"
        actions={
          editable && (
            <button className="a-btn primary" disabled={!changed} onClick={commit}>
              Guardar cambios
            </button>
          )
        }
      />
      <div className="a-grid-2">
        <section className="a-card">
          <h2>Tienda</h2>
          <Field label="Nombre">
            <input className="a-input" value={s.nombreTienda} disabled={!editable} onChange={(e) => setS({ ...s, nombreTienda: e.target.value })} />
          </Field>
          <Field label="Correo de soporte" hint="Aparece en los correos y en la ayuda de la app.">
            <input className="a-input" type="email" value={s.emailSoporte} disabled={!editable} onChange={(e) => setS({ ...s, emailSoporte: e.target.value })} />
          </Field>
          <label className="a-check">
            <input type="checkbox" checked={s.ivaIncluido} disabled={!editable} onChange={(e) => setS({ ...s, ivaIncluido: e.target.checked })} />
            Los precios incluyen IVA
          </label>
          <label className="a-check">
            <input type="checkbox" checked={s.mantenimiento} disabled={!editable} onChange={(e) => setS({ ...s, mantenimiento: e.target.checked })} />
            Modo mantenimiento (pausa las compras en la app)
          </label>
        </section>

        <section className="a-card">
          <h2>Precios</h2>
          <div className="a-form-grid">
            <Field label="Marcador Pro (pago único)">
              <EuroInput cents={s.precioPro} onChange={(precioPro) => setS({ ...s, precioPro })} />
            </Field>
            <Field label="Bar Básico (por mesa y mes)">
              <EuroInput cents={s.precioBarBasico} onChange={(precioBarBasico) => setS({ ...s, precioBarBasico })} />
            </Field>
            <Field label="Bar Pro (por mesa y mes)">
              <EuroInput cents={s.precioBarPro} onChange={(precioBarPro) => setS({ ...s, precioBarPro })} />
            </Field>
          </div>
          <p className="a-muted small">
            Un bar con 2 mesas en Bar Pro pagaría {money(s.precioBarPro * 2)} al mes. Los precios nuevos se aplican a las altas y renovaciones.
          </p>
        </section>

        <section className="a-card">
          <h2>Cobros</h2>
          <Field label="Plataforma de pagos" hint={PROVIDERS.find((p) => p.value === s.proveedorPagos)?.nota}>
            <select className="a-input" value={s.proveedorPagos} disabled={!editable} onChange={(e) => setS({ ...s, proveedorPagos: e.target.value as PaymentProvider })}>
              {PROVIDERS.map((p) => (
                <option key={p.value} value={p.value}>
                  {p.label}
                </option>
              ))}
            </select>
          </Field>
          <p className="a-muted small">
            Las claves de la plataforma de pagos nunca van en el panel ni en la web: se guardan como secretos en el servidor (ver docs/ADMIN.md).
          </p>
        </section>

        <section className="a-card">
          <h2>Equipo</h2>
          <ul className="a-list">
            {team.map((u) => (
              <li key={u.id}>
                <span>
                  {u.nombre} <span className="a-muted">· {u.email}</span>
                </span>
                <Badge tone="accent">{ROLE_LABEL[u.role]}</Badge>
              </li>
            ))}
          </ul>
          <p className="a-muted small">Para añadir a alguien: búscalo en Usuarios y cámbiale el rol a Administrador o Soporte. Soporte no ve reembolsos, precios ni ajustes.</p>
        </section>

        <section className="a-card">
          <h2>Conexión</h2>
          <p>
            Ahora: {backend.mode === 'demo' ? <Badge tone="warn">Modo demostración</Badge> : <Badge tone="ok">Base de datos real</Badge>}{' '}
            <span className="a-muted">· sesión de {session.email}</span>
          </p>
          {builtInConnection() ? (
            <p className="a-muted small">
              Conexión fijada al publicar la web: el panel siempre trabaja con la base de datos real y pide verificación en dos
              pasos. No se puede cambiar desde aquí.
            </p>
          ) : backend.mode === 'demo' ? (
            <>
              <p className="a-muted small">Cuando el servicio sea de pago: crea el proyecto en Supabase, aplica las migraciones de supabase/migrations/ y pega aquí su dirección y su clave «anon» (pública).</p>
              <Field label="Dirección del proyecto (URL)">
                <input className="a-input mono" value={url} disabled={!editable} onChange={(e) => setUrl(e.target.value)} placeholder="https://xxxx.supabase.co" />
              </Field>
              <Field label="Clave pública «anon»">
                <input className="a-input mono" value={key} disabled={!editable} onChange={(e) => setKey(e.target.value)} placeholder="eyJ…" />
              </Field>
              <button
                className="a-btn"
                disabled={!editable || !/^https:\/\/\S+$/.test(url) || key.length < 20}
                onClick={() => {
                  saveConnection({ url: url.trim(), anonKey: key.trim() });
                  location.reload();
                }}
              >
                Conectar con la base de datos real
              </button>
            </>
          ) : (
            <ConfirmButton
              tone="primary"
              onConfirm={() => {
                saveConnection(null);
                backend.signOut();
                location.reload();
              }}
            >
              Volver al modo demostración
            </ConfirmButton>
          )}
        </section>

        <section className="a-card">
          <h2>Datos</h2>
          <div className="a-row-actions">
            <button className="a-btn" onClick={() => download(`panel-${new Date().toISOString().slice(0, 10)}.json`, JSON.stringify(data, null, 2), 'application/json')}>
              Exportar todo (JSON)
            </button>
            {backend.reset && editable && (
              <ConfirmButton
                onConfirm={async () => {
                  await backend.reset!();
                  toast('Datos de demostración restablecidos');
                  location.reload();
                }}
              >
                Restablecer datos de demostración
              </ConfirmButton>
            )}
          </div>
        </section>
      </div>
    </>
  );
}
