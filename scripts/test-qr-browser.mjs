// Prueba del recorrido QR con navegador aislado y nube ficticia. No toca una placa ni Supabase.
// Requiere las dependencias existentes y Edge instalado (o QR_BROWSER_CHANNEL=chrome).
import assert from 'node:assert/strict';
import { realpathSync } from 'node:fs';
import { chromium } from 'playwright';
import { createServer } from 'vite';

process.env.VITE_SUPABASE_URL = 'https://nube.test';
process.env.VITE_SUPABASE_ANON_KEY = 'solo-prueba';
const server = await createServer({ server: { host: '127.0.0.1', port: 0, fs: { allow: [process.cwd(), realpathSync('node_modules')] } }, clearScreen: false });
let browser;
try {
  await server.listen();
  const base = server.resolvedUrls.local[0];
  browser = await chromium.launch({ channel: process.env.QR_BROWSER_CHANNEL ?? 'msedge', headless: true });
  const context = await browser.newContext({ viewport: { width: 1000, height: 650 } });
  const requests = [];
  const errors = [];
  let linkFails = true;
  await context.route('**/*', async route => {
    const url = new URL(route.request().url());
    if (url.origin === new URL(base).origin) return route.continue();
    if (url.hostname !== 'nube.test') return route.abort();
    requests.push({ path: url.pathname, method: route.request().method(), body: route.request().postDataJSON() });
    let data = [];
    let status = 200;
    if (url.pathname === '/auth/v1/token') {
      data = { access_token: 'prueba', refresh_token: 'refresh-prueba', expires_in: 3600,
        user: { id: 'cuenta-prueba', email: 'prueba@example.invalid' } };
    } else if (url.pathname === '/functions/v1/vincular-placa') {
      status = linkFails ? 503 : 200;
      data = linkFails ? { error: 'Servicio temporalmente no disponible' } : { ok: true };
    }
    return route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(data) });
  });
  const page = await context.newPage();
  page.setDefaultTimeout(10000);
  page.on('pageerror', e => errors.push(e.message));
  const code = 'Ab3dEf6hIj9kLm2nOp5qRs8t';
  await page.goto(`${base}#vincular=${code}`);
  const prompt = page.getByRole('dialog', { name: 'Conectar la placa', exact: true });
  await prompt.getByRole('button', { name: 'Entrar', exact: true }).click();
  await page.getByLabel('Correo', { exact: true }).fill('prueba@example.invalid');
  await page.getByLabel('Contraseña', { exact: true }).fill('ficticia123');
  assert.equal(await page.getByRole('dialog').count(), 0, 'El aviso no tapa el formulario');
  await page.getByRole('button', { name: 'Entrar', exact: true }).click();
  const confirm = page.getByRole('dialog', { name: '¿Conectar la placa?', exact: true });
  await confirm.waitFor();
  assert.match(await confirm.innerText(), /prueba@example.invalid/);
  assert.equal(requests.filter(r => r.path.includes('/functions/')).length, 0, 'Entrar no autoriza automáticamente');
  await confirm.getByRole('button', { name: 'Conectar', exact: true }).click();
  await confirm.getByText('Servicio temporalmente no disponible', { exact: true }).waitFor();
  linkFails = false;
  await confirm.getByRole('button', { name: 'Conectar', exact: true }).click();
  await confirm.waitFor({ state: 'hidden' });
  const sent = requests.filter(r => r.path.includes('/functions/'));
  assert.equal(sent.length, 2);
  assert.ok(sent.every(r => r.method === 'POST' && r.body.codigo === code));

  // Un permiso de cámara que llega después de cerrar debe liberar el dispositivo.
  await page.evaluate(() => {
    window.__qrTest = { stopped: 0, opened: 0, resolve: [] };
    window.BarcodeDetector = class { async detect() { return []; } };
    Object.defineProperty(navigator.mediaDevices, 'getUserMedia', { configurable: true, value: () => {
      window.__qrTest.opened++;
      return new Promise(resolve => { window.__qrTest.resolve.push(() => resolve({
        getTracks: () => [{ stop: () => { window.__qrTest.stopped++; } }],
      })); });
    } });
  });
  await page.getByRole('button', { name: 'Conectar una placa (QR)', exact: true }).click();
  await page.waitForFunction(() => window.__qrTest.resolve.length > 0);
  await page.getByRole('dialog', { name: 'Lee el QR de la placa' }).getByRole('button', { name: 'Cerrar' }).click();
  await page.evaluate(() => window.__qrTest.resolve.forEach(resolve => resolve()));
  await page.waitForFunction(() => window.__qrTest.stopped > 0);
  assert.equal(await page.getByRole('dialog').count(), 0);
  assert.equal(await page.evaluate(() => window.__qrTest.stopped), await page.evaluate(() => window.__qrTest.opened));

  // Una detección pendiente al cerrar tampoco debe reabrir la confirmación.
  await page.evaluate(() => {
    window.__qrTest = { stopped: 0, opened: 0, resolve: [] };
    window.BarcodeDetector = class { detect() { return new Promise(resolve => { window.__qrTest.detect = resolve; }); } };
    HTMLMediaElement.prototype.play = async () => {};
    Object.defineProperty(navigator.mediaDevices, 'getUserMedia', { configurable: true, value: async () => {
      window.__qrTest.opened++;
      const stream = new MediaStream();
      stream.getTracks = () => [{ stop: () => { window.__qrTest.stopped++; } }];
      return stream;
    } });
  });
  await page.getByRole('button', { name: 'Conectar una placa (QR)', exact: true }).click();
  await page.waitForFunction(() => !!window.__qrTest.detect);
  await page.getByRole('dialog', { name: 'Lee el QR de la placa' }).getByRole('button', { name: 'Cerrar' }).click();
  await page.evaluate(code => window.__qrTest.detect([{ rawValue: code }]), code);
  await page.waitForTimeout(100);
  assert.equal(await page.getByRole('dialog').count(), 0, 'No reaparece un QR detectado después de cerrar');
  assert.deepEqual(errors, []);
  console.log('QR navegador: entrar sin sesión, confirmar, reintentar, conservar código y liberar cámara tardía: OK');
} catch (error) {
  console.error(error);
  throw error;
} finally {
  await browser?.close();
  await server.close();
}
