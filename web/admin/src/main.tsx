import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { AdminApp } from './App';
import './admin.css';

// Nunca dentro de otra página (evita que alguien lo meta en un marco para engañar con clics).
if (window.top !== window.self) {
  document.body.textContent = 'El panel de administración no se puede abrir dentro de otra página.';
} else {
  createRoot(document.getElementById('root')!).render(
    <StrictMode>
      <AdminApp />
    </StrictMode>,
  );
}
