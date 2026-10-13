/**
 * Opciones de un jugador al dejarlo pulsado (en las listas para elegir jugadores) y el QR de su perfil:
 * se lee con la cámara del móvil y abre su perfil en la web (solo para ver; no hace falta contraseña).
 */
import qrcode from 'qrcode-generator';
import { useEffect, useMemo, useRef, useState, type MouseEvent, type PointerEvent } from 'react';
import { useApp } from '../../app/AppContext';
import type { Player } from '../../services/persistence';
import { Avatar, Modal } from './common';

/** Código QR en SVG (nítido a cualquier tamaño). */
export function QrCode({ text, size = 220 }: { text: string; size?: number }) {
  const path = useMemo(() => {
    const qr = qrcode(0, 'M');
    qr.addData(text);
    qr.make();
    const n = qr.getModuleCount();
    let d = '';
    for (let r = 0; r < n; r++) for (let c = 0; c < n; c++) if (qr.isDark(r, c)) d += `M${c + 4} ${r + 4}h1v1h-1z`;
    return { d, n: n + 8 };
  }, [text]);
  return (
    <svg className="qr-code" width={size} height={size} viewBox={`0 0 ${path.n} ${path.n}`} role="img" aria-label="Código QR del perfil" shapeRendering="crispEdges">
      <rect width={path.n} height={path.n} fill="#fff" />
      <path d={path.d} fill="#000" />
    </svg>
  );
}

/**
 * Pulsación larga: devuelve las propiedades para un botón que hace `onClick` al tocarlo y `onLong` al
 * dejarlo pulsado (y entonces no hace el toque normal).
 */
export function useLongPress(ms = 550) {
  const timer = useRef<number | undefined>(undefined);
  const fired = useRef(false);
  const start = useRef<{ x: number; y: number } | null>(null);
  const cancel = () => {
    window.clearTimeout(timer.current);
    start.current = null;
  };
  useEffect(() => () => window.clearTimeout(timer.current), []);
  return (onLong: () => void, onClick: (e: MouseEvent<HTMLElement>) => void) => ({
    onPointerDown: (e: PointerEvent<HTMLElement>) => {
      fired.current = false;
      start.current = { x: e.clientX, y: e.clientY };
      window.clearTimeout(timer.current);
      timer.current = window.setTimeout(() => {
        fired.current = true;
        start.current = null;
        navigator.vibrate?.(25);
        onLong();
      }, ms);
    },
    onPointerMove: (e: PointerEvent<HTMLElement>) => {
      const s = start.current;
      if (s && Math.hypot(e.clientX - s.x, e.clientY - s.y) > 12) cancel();
    },
    onPointerUp: cancel,
    onPointerLeave: cancel,
    onPointerCancel: cancel,
    onContextMenu: (e: MouseEvent<HTMLElement>) => e.preventDefault(),
    onClick: (e: MouseEvent<HTMLElement>) => {
      if (fired.current) {
        fired.current = false;
        e.preventDefault();
        e.stopPropagation();
        return;
      }
      onClick(e);
    },
  });
}

type Estado = 'subiendo' | 'listo' | 'sin-conexion' | 'local';

/** Opciones del jugador: ver el QR de su perfil o su perfil en la app. */
export function PlayerOptions({ player, onClose, startWithQr = false }: { player: Player; onClose: () => void; startWithQr?: boolean }) {
  const { navigate, profileLink, demoMode } = useApp();
  const [qr, setQr] = useState(startWithQr);
  const link = useMemo(() => profileLink(player.id), [player, profileLink]);
  const [estado, setEstado] = useState<Estado>(link?.online ? 'subiendo' : 'local');

  useEffect(() => {
    if (!qr || !link?.online) return;
    let alive = true;
    void link.publish().then((ok) => alive && setEstado(ok ? 'listo' : 'sin-conexion'));
    return () => {
      alive = false;
    };
  }, [qr, link]);

  return (
    <Modal
      title={player.name}
      onClose={onClose}
      actions={
        <>
          {qr && (
            <button className="btn" onClick={() => setQr(false)}>
              Atrás
            </button>
          )}
          <button className="btn btn-primary" onClick={onClose}>
            Cerrar
          </button>
        </>
      }
    >
      {!qr ? (
        <div className="po">
          <div className="po-head">
            <Avatar name={player.name} photo={player.photo} size={64} />
            <div>
              <b>{player.name}</b>
            </div>
          </div>
          <div className="po-actions">
            <button className="btn btn-lg po-btn" onClick={() => setQr(true)}>
              <span className="po-ico">▦</span> Ver QR del perfil
            </button>
            <button
              className="btn btn-lg po-btn"
              onClick={() => {
                onClose();
                navigate({ name: 'profile', playerId: player.id });
              }}
            >
              <span className="po-ico">👤</span> Ver perfil aquí
            </button>
          </div>
        </div>
      ) : (
        link && (
          <div className="po-qr">
            <QrCode text={link.url} size={230} />
            <div className="po-qr-info">
              <div className="po-qr-who">
                <Avatar name={player.name} photo={player.photo} size={44} />
                <b>{player.name}</b>
              </div>
              <p>Apunta con la cámara del móvil al código y se abre su perfil: palmarés, partidos y estadísticas.</p>
              <p className={`po-qr-state is-${estado}`}>
                {estado === 'subiendo' && 'Actualizando el perfil…'}
                {estado === 'listo' && '✔ Perfil al día'}
                {estado === 'sin-conexion' && 'Sin conexión ahora: el perfil se verá con los datos de la última vez que hubo internet.'}
                {estado === 'local' &&
                  (demoMode ? 'Modo prueba: los perfiles de prueba no se publican.' : 'Este aparato no está conectado al servidor: el perfil no se publica.')}
              </p>
              <small className="po-qr-url">{link.url}</small>
            </div>
          </div>
        )
      )}
    </Modal>
  );
}
