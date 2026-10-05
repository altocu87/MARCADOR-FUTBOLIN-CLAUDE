import { useEffect, useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import { restoreSnapshot } from '../../app/recovery';
import { getScore, type MatchMode } from '../../match-engine';
import type { ActiveMatchSnapshot } from '../../services/persistence';
import { sound } from '../../services/sound/sound';
import { useHardware } from '../../inputs/hardware/useHardware';
import { AssetImage, assetUrl } from '../components/assets';
import { MODE_LABEL, Modal, formatDate } from '../components/common';

// Textos de una sola frase: se tienen que entender de un vistazo, de pie y jugando.
const MODES: { mode: MatchMode; title: string; text: string; tag: string }[] = [
  { mode: 'quick', title: 'RÁPIDO', text: 'Empieza a jugar', tag: 'XP' },
  { mode: 'chaos', title: 'LOCO', text: 'Un hándicap loco cada minuto', tag: 'XP' },
  { mode: 'ranked', title: 'CLASIFICATORIO', text: 'Partido competitivo', tag: 'ELO + XP' },
];

// Accesos de la cabecera: cada uno con su color e icono de trazo (mismo estilo que el engranaje).
const MENU = [
  {
    route: 'tournament',
    label: 'TORNEO',
    icon: (
      <>
        <path d="M7 4h10v5a5 5 0 0 1-10 0z" />
        <path d="M7 6H4v1.5A3.5 3.5 0 0 0 7.5 11M17 6h3v1.5a3.5 3.5 0 0 1-3.5 3.5M12 14v4M8 20.5h8M9.5 18h5" />
      </>
    ),
  },
  {
    route: 'ranking',
    label: 'RANKING',
    icon: <path d="M3.5 20.5h17M6 20.5v-6h3v6M10.5 20.5V9h3v11.5M15 20.5v-9h3v9" />,
  },
  {
    route: 'challenges',
    label: 'RETOS',
    icon: (
      <>
        <circle cx="12" cy="12" r="8.5" />
        <circle cx="12" cy="12" r="4.8" />
        <circle cx="12" cy="12" r="1.2" />
      </>
    ),
  },
] as const;

/** Sin tocar nada, la tarjeta elegida vuelve a reposo pasado este tiempo. */
const SELECT_TIMEOUT_MS = 8000;

/** Nombre del archivo de cada modo en src/assets/images. */
const MODE_FILE: Record<MatchMode, string> = { quick: 'rapido', chaos: 'caos', ranked: 'clasificatorio' };

/** Tiempo que la tarjeta se ve «pulsada» antes de cambiar de pantalla (confirma el toque). */
const PRESS_FEEDBACK_MS = 180;

export function HomeScreen() {
  const { navigate, repos, persistent } = useApp();
  const [snapshot, setSnapshot] = useState<ActiveMatchSnapshot | null>(null);
  const [pressed, setPressed] = useState<string | null>(null);
  const pressTimer = useRef<number | undefined>(undefined);
  const [selected, setSelected] = useState<MatchMode | null>(null);
  const hub = useHardware();

  useEffect(() => {
    void repos.activeMatch.load().then(setSnapshot);
  }, [repos]);

  useEffect(() => () => window.clearTimeout(pressTimer.current), []);

  // La tarjeta elegida se desactiva sola si nadie confirma.
  useEffect(() => {
    if (!selected) return;
    const id = window.setTimeout(() => setSelected(null), SELECT_TIMEOUT_MS);
    return () => window.clearTimeout(id);
  }, [selected]);

  // El botón tocado resplandece y se hunde un instante antes de cambiar de pantalla; un segundo toque se ignora.
  const press = (key: string, go: () => void) => {
    if (pressed) return;
    setPressed(key);
    pressTimer.current = window.setTimeout(go, PRESS_FEEDBACK_MS);
  };

  // Dos toques: el primero activa la tarjeta (se enciende) y el segundo, sobre la misma, entra en el modo.
  const start = (mode: MatchMode) => {
    if (pressed) return;
    sound.unlock();
    sound.play('ui');
    if (selected !== mode) {
      setSelected(mode);
      return;
    }
    press(mode, () => navigate({ name: 'setup', mode }));
  };

  const resume = () => {
    if (!snapshot) return;
    const state = restoreSnapshot(snapshot, Date.now());
    navigate({ name: 'match', config: state.config, participants: state.participants, resume: state, extras: snapshot.extras });
  };

  const discard = async () => {
    await repos.activeMatch.clear();
    setSnapshot(null);
  };

  return (
    <section className="screen home">
      <div className="home-bg" aria-hidden="true">
        <AssetImage name="fondo-inicio" className="home-bg-img" fallback={<div className="tron-grid" />} />
      </div>
      <header className="home-top">
        <div className="brand">
          <AssetImage name="logo" alt="" className="brand-logo" fallback={<span className="brand-mark" aria-hidden="true" />} />
          <div className="brand-name">
            <span>MARCADOR</span>
            <span>FUTBOLÍN</span>
          </div>
        </div>
        <nav className="home-menu" aria-label="Menú principal">
          {MENU.map((item) => (
            <button
              key={item.route}
              className={`menu-btn menu-${item.route}${pressed === item.route ? ' is-pressed' : ''}`}
              // Torneo empieza eligiendo quién juega.
              onClick={() => press(item.route, () => navigate(item.route === 'tournament' ? { name: 'tournamentNew' } : { name: item.route }))}
            >
              <svg className="menu-icon" viewBox="0 0 24 24" aria-hidden="true">
                {item.icon}
              </svg>
              {item.label}
            </button>
          ))}
        </nav>
        {/* Solo se avisa del almacenamiento cuando hay un problema; el resto vive en Ajustes. */}
        {!persistent && (
          <span className="status-pill home-warn" title="Los datos no se pueden guardar en este dispositivo">
            SIN GUARDADO
          </span>
        )}
        <button className="home-icon-btn" onClick={() => navigate({ name: 'settings' })} aria-label="Ajustes" title="Ajustes">
          <svg viewBox="0 0 24 24" aria-hidden="true">
            <circle cx="12" cy="12" r="3.2" />
            <path d="M12 2.5v2.6M12 18.9v2.6M4.6 4.6l1.9 1.9M17.5 17.5l1.9 1.9M2.5 12h2.6M18.9 12h2.6M4.6 19.4l1.9-1.9M17.5 6.5l1.9-1.9" />
            <circle cx="12" cy="12" r="6.6" />
          </svg>
          {hub.connectedCount > 0 && <span className="home-icon-dot" title="Placa conectada" />}
        </button>
      </header>

      <div className="mode-cards">
        {MODES.map((m) => (
          <button
            key={m.mode}
            className={`mode-card mode-${m.mode}${selected === m.mode ? ' is-selected' : ''}${pressed === m.mode ? ' is-pressed' : ''}`}
            aria-pressed={selected === m.mode}
            onClick={() => start(m.mode)}
            aria-label={`Jugar ${m.title.toLowerCase()}: ${m.text}`}
          >
            {assetUrl(`tarjeta-${MODE_FILE[m.mode]}-off`) ? (
              // Ilustración a tarjeta completa con dos versiones: apagada y encendida (se funden al activar).
              <>
                <AssetImage name={`tarjeta-${MODE_FILE[m.mode]}-off`} className="mode-bg mode-bg-off" fallback={null} />
                <AssetImage name={`tarjeta-${MODE_FILE[m.mode]}-on`} className="mode-bg mode-bg-on" fallback={null} />
                <span className="mode-shade" aria-hidden="true" />
              </>
            ) : (
              <AssetImage
                name={`modo-${MODE_FILE[m.mode]}`}
                className="mode-art"
                fallback={
                  <span className="mode-icon" aria-hidden="true">
                    {m.mode === 'quick' ? '⚡' : m.mode === 'chaos' ? '✦' : '♛'}
                  </span>
                }
              />
            )}
            <span className="mode-title">{m.title}</span>
            <span className="mode-text">{m.text}</span>
            <span className="mode-tag">{m.tag}</span>
          </button>
        ))}
      </div>

      {snapshot && (
        <Modal
          title="Partida sin terminar"
          actions={
            <>
              <button className="btn btn-danger" onClick={discard}>
                Descartar
              </button>
              <button className="btn btn-primary" onClick={resume}>
                Reanudar en pausa
              </button>
            </>
          }
        >
          <p className="muted" style={{ margin: 0 }}>
            {MODE_LABEL[snapshot.state.config.mode]} guardado el {formatDate(snapshot.savedAt)}.
          </p>
          <p style={{ fontSize: 22, fontWeight: 800, margin: '10px 0' }}>
            {snapshot.state.participants.filter((p) => p.team === 'white').map((p) => p.nameSnapshot).join(' + ')}{' '}
            {getScore(snapshot.state).white} – {getScore(snapshot.state).blue}{' '}
            {snapshot.state.participants.filter((p) => p.team === 'blue').map((p) => p.nameSnapshot).join(' + ')}
          </p>
          <p className="muted" style={{ margin: 0, fontSize: 13 }}>
            Se reanuda en pausa con el tiempo que tenía al guardarse. El tiempo con la aplicación cerrada no cuenta.
          </p>
        </Modal>
      )}
    </section>
  );
}
