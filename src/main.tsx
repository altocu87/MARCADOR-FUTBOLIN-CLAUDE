import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { App } from './app/App';
// Letra Inter dentro de la app (la misma que usa la pantalla de 7"): se ve igual en cualquier aparato.
import '@fontsource/inter/latin-400.css';
import '@fontsource/inter/latin-600.css';
import '@fontsource/inter/latin-700.css';
import '@fontsource/inter/latin-800.css';
import '@fontsource/inter/latin-900.css';
import '@fontsource/inter/latin-800-italic.css';
import '@fontsource/inter/latin-900-italic.css';
import './styles/base.css';
import './styles/components.css';
import './styles/screens.css';
import './styles/match.css';
import './styles/celebrations.css';
import './styles/handicaps.css';
import './styles/tournament-phase.css';

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);

// Uso sin conexión: registra el service worker en la versión compilada.
if (import.meta.env.PROD && 'serviceWorker' in navigator) {
  window.addEventListener('load', () => {
    navigator.serviceWorker.register('./sw.js').catch(() => undefined);
  });
}
