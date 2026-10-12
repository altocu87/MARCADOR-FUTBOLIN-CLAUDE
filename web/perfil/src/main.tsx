import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { ProfilePage } from './ProfilePage';
import './perfil.css';

// Otro código en la dirección (otro QR leído con la página abierta): se carga ese perfil.
window.addEventListener('hashchange', () => location.reload());

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <ProfilePage />
  </StrictMode>,
);
