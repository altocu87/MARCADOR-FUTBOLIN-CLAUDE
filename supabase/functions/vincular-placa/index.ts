// Entrar en la pantalla de 7" con un QR: la app (con la sesión del dueño de la mesa) manda el código que enseña la
// placa y aquí se prepara un enlace mágico de un solo uso para esa cuenta, que la placa recoge con
// `vinculo_placa(código)` para abrir su propia sesión. No se manda ningún correo. Ver
// supabase/migrations/20261019180000_vinculo_placa.sql y docs/NUBE.md.
import { createClient } from 'npm:@supabase/supabase-js@2';

const CORS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type',
  'Access-Control-Allow-Methods': 'POST, OPTIONS',
};

const reply = (status: number, body: object) =>
  new Response(JSON.stringify(body), { status, headers: { ...CORS, 'Content-Type': 'application/json' } });

Deno.serve(async (req) => {
  if (req.method === 'OPTIONS') return new Response(null, { status: 204, headers: CORS });
  if (req.method !== 'POST') return reply(405, { error: 'Solo POST' });

  const url = Deno.env.get('SUPABASE_URL')!;
  const service = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!;
  const admin = createClient(url, service, { auth: { persistSession: false, autoRefreshToken: false } });

  // ¿Quién lo pide? (la sesión de la app)
  const jwt = (req.headers.get('Authorization') ?? '').replace(/^Bearer\s+/i, '');
  const { data: who, error: whoErr } = await admin.auth.getUser(jwt);
  const email = who?.user?.email;
  if (whoErr || !email) return reply(401, { error: 'Entra primero con tu cuenta' });

  let codigo = '';
  try {
    codigo = String((await req.json())?.codigo ?? '');
  } catch {
    /* sin cuerpo */
  }
  if (!/^[A-Za-z0-9]{24,64}$/.test(codigo)) return reply(400, { error: 'Código de la placa no válido' });

  const { data: link, error: linkErr } = await admin.auth.admin.generateLink({ type: 'magiclink', email });
  const tokenHash = link?.properties?.hashed_token;
  if (linkErr || !tokenHash) return reply(500, { error: 'No se pudo preparar la entrada de la placa' });

  const { error: saveErr } = await admin.from('vinculos_placa').upsert({ codigo, token_hash: tokenHash, email, creado: new Date().toISOString() });
  if (saveErr) return reply(500, { error: 'No se pudo guardar el vínculo' });
  return reply(200, { ok: true });
});
