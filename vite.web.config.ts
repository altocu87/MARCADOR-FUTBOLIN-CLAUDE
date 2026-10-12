import { defineConfig, type Plugin } from 'vite';
import react from '@vitejs/plugin-react';

/**
 * Política de seguridad del panel (solo al publicar; en desarrollo Vite necesita scripts en línea):
 * solo carga código propio, solo habla con su propia web y con Supabase, sin formularios ni marcos ajenos.
 */
const ADMIN_CSP = [
  "default-src 'self'",
  "script-src 'self'",
  "style-src 'self' 'unsafe-inline'",
  "img-src 'self' data: blob: https://*.supabase.co",
  "media-src 'self' data: blob: https://*.supabase.co",
  "font-src 'self'",
  "connect-src 'self' https://*.supabase.co",
  "object-src 'none'",
  "base-uri 'none'",
  "form-action 'self'",
].join('; ');

function adminSecurity(): Plugin {
  return {
    name: 'admin-security',
    apply: 'build',
    transformIndexHtml(html, ctx) {
      if (!ctx.path.includes('admin')) return html;
      return html.replace(
        '<meta charset="UTF-8" />',
        `<meta charset="UTF-8" />\n    <meta http-equiv="Content-Security-Policy" content="${ADMIN_CSP}" />\n    <meta name="referrer" content="no-referrer" />`,
      );
    },
  };
}

/**
 * Web pública (portada, «Monta tu marcador», instalador y ayuda).
 * Se publica junto a la app: la web en la raíz y la app en la carpeta «app/» (ver scripts/assemble-web.mjs).
 */
export default defineConfig({
  root: 'web',
  base: './',
  plugins: [react(), adminSecurity()],
  build: {
    outDir: '../dist-web',
    emptyOutDir: true,
    chunkSizeWarningLimit: 900,
    rollupOptions: {
      // Tres páginas: la web pública, el panel de administración (en /admin/, fuera del menú y sin indexar) y el
      // perfil de jugador que se abre leyendo su QR (en /perfil/, tampoco se indexa).
      input: { web: 'web/index.html', admin: 'web/admin/index.html', perfil: 'web/perfil/index.html' },
    },
  },
  preview: { port: 4180 },
});
