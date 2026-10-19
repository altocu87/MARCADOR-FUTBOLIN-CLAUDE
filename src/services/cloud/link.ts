/**
 * Entrar en la pantalla de 7" con un QR: la placa enseña un QR con la dirección de la app y un código
 * (…/app/#vincular=<código>). La app, con la sesión del dueño, se lo pasa a la función `vincular-placa` de Supabase
 * y la placa abre su propia sesión sola (docs/NUBE.md). La contraseña nunca pasa por la placa.
 */
import type { SupabaseConn } from '../profiles';
import { CloudError, type Session } from './auth';

const CODE = /^[A-Za-z0-9]{24,64}$/;

/** Código de la placa en una dirección (#vincular=…, ?vincular=…) o en el texto de un QR. null si no hay. */
export function boardCodeFrom(text: string): string | null {
  const m = /[#?&]vincular=([^&#]*)/.exec(text);
  const code = m ? m[1] : text.trim();
  return CODE.test(code) ? code : null;
}

/** Conecta la placa que enseña `code` a la cuenta de esta sesión. */
export async function linkBoard(conn: SupabaseConn, s: Session, code: string, http: typeof fetch = (...a) => fetch(...a)): Promise<void> {
  if (!CODE.test(code)) throw new CloudError('Ese QR no es de una placa');
  let res: Response;
  try {
    res = await http(`${conn.url}/functions/v1/vincular-placa`, {
      method: 'POST',
      headers: { apikey: conn.anonKey, Authorization: `Bearer ${s.accessToken}`, 'Content-Type': 'application/json' },
      body: JSON.stringify({ codigo: code }),
    });
  } catch {
    throw new CloudError('Sin conexión con la nube', 'offline');
  }
  if (res.ok) return;
  let msg = '';
  try {
    msg = ((await res.json()) as { error?: string }).error ?? '';
  } catch {
    /* sin cuerpo */
  }
  throw new CloudError(msg || `No se pudo conectar la placa (${res.status})`, String(res.status));
}
