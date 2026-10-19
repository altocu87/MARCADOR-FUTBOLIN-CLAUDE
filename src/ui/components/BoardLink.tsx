/**
 * Entrar en la pantalla de 7" con un QR (docs/NUBE.md):
 *  - BoardLinkPrompt: si la app se abre desde el QR de la placa (…#vincular=<código>), pregunta si se conecta a la
 *    cuenta (o pide entrar antes).
 *  - BoardQrScanner: Ajustes → Cuenta → «Conectar una placa (QR)»: lee el QR con la cámara del móvil.
 */
import { useEffect, useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import { boardCodeFrom } from '../../services/cloud';
import { Modal } from './common';

export function BoardLinkPrompt() {
  const { cloud, navigate, toast, route } = useApp();
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const code = cloud.pendingBoard;
  if (!code) return null;
  const close = () => {
    cloud.setPendingBoard(null);
    setError(null);
  };
  if (!cloud.session) {
    // Conserva el QR mientras se escribe la contraseña, sin tapar el formulario.
    if (route.name === 'settings' && route.tab === 'account') return null;
    return (
      <Modal
        title="Conectar la placa"
        onClose={close}
        actions={
          <>
            <button className="btn btn-ghost" onClick={close}>Cancelar</button>
            <button className="btn btn-primary" onClick={() => navigate({ name: 'settings', tab: 'account' })}>Entrar</button>
          </>
        }
      >
        <p style={{ margin: 0 }}>Para conectar la placa a tu cuenta, entra primero en la app con tu cuenta. Al entrar, te pediremos confirmar la conexión.</p>
      </Modal>
    );
  }
  const go = async () => {
    setBusy(true);
    setError(null);
    try {
      await cloud.linkBoard(code);
      cloud.setPendingBoard(null);
      toast('Autorización enviada: comprueba que la placa termina de entrar');
    } catch (err) {
      setError(err instanceof Error ? err.message : 'No se pudo conectar la placa');
    } finally {
      setBusy(false);
    }
  };
  return (
    <Modal
      title="¿Conectar la placa?"
      onClose={close}
      actions={
        <>
          <button className="btn btn-ghost" onClick={close}>Cancelar</button>
          <button className="btn btn-primary" disabled={busy} onClick={() => void go()}>{busy ? 'Conectando…' : 'Conectar'}</button>
        </>
      }
    >
      <p style={{ margin: 0 }}>
        La pantalla del futbolín entrará con la cuenta <strong>{cloud.session.email}</strong> y guardará allí sus partidos.
      </p>
      {error && <p style={{ margin: '10px 0 0', color: 'var(--danger)' }}>{error}</p>}
    </Modal>
  );
}

type Detector = { detect(src: CanvasImageSource): Promise<{ rawValue: string }[]> };

export function BoardQrScanner({ onClose }: { onClose(): void }) {
  const { cloud } = useApp();
  const video = useRef<HTMLVideoElement>(null);
  // La sincronización cambia el contexto; no debe volver a abrir la cámara.
  const callbacks = useRef({ setPendingBoard: cloud.setPendingBoard, onClose });
  callbacks.current = { setPendingBoard: cloud.setPendingBoard, onClose };
  const [problem, setProblem] = useState<string | null>(null);

  useEffect(() => {
    const Ctor = (window as unknown as { BarcodeDetector?: new (o: object) => Detector }).BarcodeDetector;
    if (!Ctor || !navigator.mediaDevices?.getUserMedia) {
      setProblem('Este navegador no lee QR desde aquí. Abre la cámara del móvil y apunta al QR de la placa: se abrirá la app y te preguntará.');
      return;
    }
    let stream: MediaStream | null = null;
    let timer = 0;
    let done = false;
    const stop = () => stream?.getTracks().forEach((t) => t.stop());
    void (async () => {
      try {
        const detector = new Ctor({ formats: ['qr_code'] });
        stream = await navigator.mediaDevices.getUserMedia({ video: { facingMode: 'environment' } });
        if (!video.current || done) {
          stop();  // El permiso puede llegar después de cerrar la ventana.
          return;
        }
        video.current.srcObject = stream;
        await video.current.play();
        const look = async () => {
          if (done || !video.current) return;
          try {
            const codes = await detector.detect(video.current);
            if (done) return;
            for (const c of codes) {
              const code = boardCodeFrom(c.rawValue);
              if (code) {
                done = true;
                stop();
                callbacks.current.setPendingBoard(code);  // la misma pregunta que al abrir el enlace
                callbacks.current.onClose();
                return;
              }
            }
          } catch {
            /* la imagen aún no está lista */
          }
          if (!done) timer = window.setTimeout(() => void look(), 300);
        };
        void look();
      } catch {
        stop();
        if (!done) setProblem('No se pudo usar la cámara. Abre la cámara del móvil y apunta al QR de la placa.');
      }
    })();
    return () => {
      done = true;
      window.clearTimeout(timer);
      stop();
    };
  }, []);

  return (
    <Modal title="Lee el QR de la placa" onClose={onClose} actions={<button className="btn" onClick={onClose}>Cerrar</button>}>
      {problem ? (
        <p style={{ margin: 0 }}>{problem}</p>
      ) : (
        <>
          <video ref={video} playsInline muted style={{ width: '100%', maxHeight: 260, borderRadius: 12, background: '#000', objectFit: 'cover' }} />
          <p className="muted" style={{ margin: '8px 0 0', fontSize: 14 }}>En la placa: AJUSTES → CUENTA → ENTRAR CON QR.</p>
        </>
      )}
    </Modal>
  );
}
