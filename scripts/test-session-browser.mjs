// App real y API ficticia: sesiones por aparato, varias pestañas, QR y cierre durante renovación.
// No usa cuentas reales, cámara real ni servicios de producción.
import assert from 'node:assert/strict';
import { realpathSync } from 'node:fs';
import { chromium } from 'playwright';
import { createServer } from 'vite';
process.env.VITE_SUPABASE_URL = 'https://nube.test';
process.env.VITE_SUPABASE_ANON_KEY = 'solo-prueba';
const server = await createServer({ server: { host: '127.0.0.1', port: 0, fs: { allow: [process.cwd(), realpathSync('node_modules')] } }, clearScreen: false });
let browser;
try {
  await server.listen(); const base = server.resolvedUrls.local[0];
  browser = await chromium.launch({ channel: process.env.QR_BROWSER_CHANNEL ?? 'msedge', headless: true });
  const context = await browser.newContext({ viewport: { width: 1000, height: 650 } });
  const errors = [], calls = [];
  let refreshes = 0, holdRefresh = false, releaseRefresh;
  await context.route('**/*', async route => {
    const req = route.request(), url = new URL(req.url());
    if (url.origin === new URL(base).origin) return route.continue();
    if (url.hostname !== 'nube.test') return route.abort();
    calls.push({ path: url.pathname, query: url.search, auth: req.headers().authorization, body: req.postDataJSON() });
    let body = [];
    if (url.pathname === '/auth/v1/token') {
      if (url.searchParams.get('grant_type') === 'refresh_token') {
        ++refreshes;
        if (holdRefresh) await new Promise(resolve => { releaseRefresh = resolve; });
      }
      body = { access_token: `token-${refreshes}`, refresh_token: `permiso-${refreshes}`, expires_in: 3600,
        user: { id: 'cuenta-prueba', email: 'prueba@example.invalid' } };
    }
    if (url.pathname === '/functions/v1/vincular-placa') body = { ok: true };
    await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify(body) });
  });
  const account = async page => {
    await page.getByRole('button', { name: 'Ajustes', exact: true }).click();
    await page.getByRole('tab', { name: 'Cuenta', exact: true }).click();
  };
  const open = async () => {
    const page = await context.newPage(); page.setDefaultTimeout(10000); page.on('pageerror', e => errors.push(e.message));
    await page.goto(base); await account(page); return page;
  };
  const login = async page => {
    await page.getByLabel('Correo', { exact: true }).fill('prueba@example.invalid');
    await page.getByLabel('Contraseña', { exact: true }).fill('ficticia123');
    await page.getByRole('button', { name: 'Entrar', exact: true }).click();
    await page.getByRole('button', { name: 'Sincronizar ahora', exact: true }).waitFor();
  };
  let page = await open();
  assert.equal(await page.getByRole('switch', { name: 'Recordar mi sesión' }).getAttribute('aria-checked'), 'true');
  // El interruptor dentro del formulario no debe enviarlo aunque las credenciales estén completas.
  await page.getByLabel('Correo', { exact: true }).fill('prueba@example.invalid');
  await page.getByLabel('Contraseña', { exact: true }).fill('ficticia123');
  await page.getByRole('switch', { name: 'Recordar mi sesión' }).click();
  await page.getByRole('switch', { name: 'Recordar mi sesión' }).click();
  assert.equal(calls.filter(c => c.path === '/auth/v1/token').length, 0, 'Recordar no envía el formulario');
  await login(page);
  // Leer tokens ya válidos no debe provocar sincronizaciones nuevas cada tres segundos.
  await page.waitForTimeout(4500);
  const syncCalls = () => calls.filter(c => c.path === '/rest/v1/mesa_datos').length;
  const idleCalls = syncCalls(); assert.ok(idleCalls > 0);
  await page.waitForTimeout(3500); assert.equal(syncCalls(), idleCalls, 'Sin bucle de sincronización en reposo');
  await page.reload(); await account(page);
  await page.getByText('prueba@example.invalid', { exact: true }).waitFor();
  let other = await open(); await other.getByText('prueba@example.invalid', { exact: true }).waitFor(); await other.close();
  // No recordar: recargar conserva esta pestaña; una pestaña nueva independiente no hereda la cuenta.
  await page.getByRole('switch', { name: 'Recordar mi sesión' }).click();
  assert.equal(await page.evaluate(() => localStorage.getItem('mfv3:nube:sesion')), null);
  assert.ok(await page.evaluate(() => sessionStorage.getItem('mfv3:nube:sesion')));
  await page.reload(); await account(page); await page.getByText('prueba@example.invalid', { exact: true }).waitFor();
  other = await open(); await other.getByLabel('Correo', { exact: true }).waitFor(); await other.close();
  await page.close(); page = await open(); await page.getByLabel('Correo', { exact: true }).waitFor();
  await page.getByRole('switch', { name: 'Recordar mi sesión' }).click(); await login(page);
  // Renovaciones en dos pestañas: Web Locks + releer el permiso recién guardado evitan repetir el intercambio.
  await page.evaluate(() => {
    const s = JSON.parse(localStorage.getItem('mfv3:nube:sesion')); s.expiresAt = 0;
    localStorage.setItem('mfv3:nube:sesion', JSON.stringify(s));
  });
  await page.reload(); await account(page); other = await open();
  await Promise.all([page.getByRole('button', { name: 'Sincronizar ahora', exact: true }).click(),
    other.getByRole('button', { name: 'Sincronizar ahora', exact: true }).click()]);
  await page.waitForFunction(() => JSON.parse(localStorage.getItem('mfv3:nube:sesion')).accessToken === 'token-1');
  await other.getByRole('button', { name: 'Sincronizar ahora', exact: true }).waitFor({ state: 'visible' });
  assert.equal(refreshes, 1); await other.close();
  // Leer con el lector de Ajustes y confirmar usa el mismo usuario y su token renovado.
  const code = 'Ab3dEf6hIj9kLm2nOp5qRs8t';
  await page.evaluate(code => {
    window.BarcodeDetector = class { async detect() { return [{ rawValue: code }]; } };
    HTMLMediaElement.prototype.play = async () => {};
    navigator.mediaDevices.getUserMedia = async () => new MediaStream();
  }, code);
  await page.getByRole('button', { name: 'Conectar una placa (QR)', exact: true }).click();
  const confirm = page.getByRole('dialog', { name: '¿Conectar la placa?', exact: true });
  await confirm.waitFor(); assert.match(await confirm.innerText(), /prueba@example.invalid/);
  await confirm.getByRole('button', { name: 'Conectar', exact: true }).click(); await confirm.waitFor({ state: 'hidden' });
  const linked = calls.find(c => c.path.includes('/functions/'));
  assert.equal(linked.auth, 'Bearer token-1'); assert.equal(linked.body.codigo, code);
  // Cierre mientras una renovación está pendiente: al llegar la respuesta no debe reabrirse la cuenta.
  await page.evaluate(() => {
    const s = JSON.parse(localStorage.getItem('mfv3:nube:sesion')); s.expiresAt = 0;
    localStorage.setItem('mfv3:nube:sesion', JSON.stringify(s));
  });
  holdRefresh = true; await page.reload(); await account(page);
  await page.getByRole('button', { name: 'Sincronizar ahora', exact: true }).click();
  const until = Date.now() + 10000;
  while (!releaseRefresh && Date.now() < until) await new Promise(resolve => setTimeout(resolve, 20));
  assert.ok(releaseRefresh);
  await page.getByRole('button', { name: 'Cerrar sesión', exact: true }).click();
  await page.getByRole('dialog', { name: '¿Cerrar sesión?', exact: true }).getByRole('button', { name: 'Cerrar sesión', exact: true }).click();
  await page.getByLabel('Correo', { exact: true }).waitFor(); releaseRefresh();
  await page.waitForTimeout(300);
  assert.equal(await page.evaluate(() => localStorage.getItem('mfv3:nube:sesion')), null);
  await page.getByLabel('Correo', { exact: true }).waitFor();
  assert.ok(calls.some(c => c.path === '/auth/v1/logout' && c.query === '?scope=local'));
  assert.deepEqual(errors, []);
  console.log('Sesiones navegador: recordar, recargar, pestaña temporal, renovación entre pestañas, lector QR y cierre durante renovación OK');
} finally { await browser?.close(); await server.close(); }
