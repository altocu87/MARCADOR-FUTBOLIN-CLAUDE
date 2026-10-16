/**
 * Protocolo de texto «MFV3» entre la app y las placas (Arduino, ESP32…).
 * Una orden por línea, terminada en «\n», igual por USB Serial, WebSocket o Bluetooth.
 *
 * Placa → app:
 *   HELLO <modelo> [versión] [caps=a,b,c]  saludo al conectar: modelo de placa (ver catálogo
 *                                «Hazlo tú mismo») y lo que sabe hacer (goles, anular, sensores, leds…)
 *   GOL_BLANCO [button|sensor]   gol del equipo Blanco (alias: GB)
 *   GOL_AZUL [button|sensor]     gol del equipo Azul   (alias: GA)
 *   ANULAR_BLANCO                anular el último gol del Blanco (−1; pulsación larga) (alias: AB)
 *   ANULAR_AZUL                  anular el último gol del Azul   (−1; pulsación larga) (alias: AA)
 *   PAUSA                        pausa / continuar
 *   SALTAR                       saltar la cuenta atrás
 *   PING                         la app responde PONG
 *
 * App → placa:
 *   HELLO MARCADOR_V3 <versión>
 *   STATE <idle|countdown|playing|paused|periodEnd|penalties|finished>
 *   SCORE <blanco> <azul>
 *   GOAL <BLANCO|AZUL>           gol ACEPTADO por el motor (para LED/zumbador)
 *   LOCK <ms>                    bloqueo de 3 s activo
 *   WIN <BLANCO|AZUL>
 *   BRILLO <0–100>               brillo de la pantalla (solo a placas que anuncian «brillo» en caps)
 *   PONG
 *
 * La placa solo envía pulsaciones: TODAS las reglas (bloqueo de 3 s, turnos de
 * penaltis, estados) las valida el motor de la app.
 */
import { GOAL_LOCK_MS, getScore, type InputSource, type MatchEvent, type MatchState, type Team } from '../../match-engine';
import type { InputCommand } from '../inputBus';

export const PROTOCOL_VERSION = '1';

export type BoardMessage =
  | { kind: 'command'; command: InputCommand; source: InputSource }
  | { kind: 'hello'; name: string; version?: string; caps: string[] }
  | { kind: 'ping' }
  | { kind: 'unknown'; raw: string };

const COMMANDS: Record<string, InputCommand> = {
  GOL_BLANCO: 'GOL_BLANCO',
  GB: 'GOL_BLANCO',
  GOL_AZUL: 'GOL_AZUL',
  GA: 'GOL_AZUL',
  ANULAR_BLANCO: 'ANULAR_BLANCO',
  AB: 'ANULAR_BLANCO',
  ANULAR_AZUL: 'ANULAR_AZUL',
  AA: 'ANULAR_AZUL',
  PAUSA: 'PAUSA',
  PAUSE: 'PAUSA',
  SALTAR: 'SALTAR',
  SKIP: 'SALTAR',
};

export function parseBoardLine(line: string): BoardMessage | null {
  const raw = line.trim();
  if (!raw) return null;
  const [head, ...rest] = raw.split(/\s+/);
  const word = head.toUpperCase();
  if (word === 'HELLO') {
    const capsArg = rest.find((r) => r.toLowerCase().startsWith('caps='));
    const plain = rest.filter((r) => r !== capsArg);
    const caps = capsArg ? capsArg.slice(5).split(',').map((c) => c.trim().toLowerCase()).filter(Boolean) : [];
    return { kind: 'hello', name: plain[0] ?? 'placa', version: plain[1], caps };
  }
  if (word === 'PING') return { kind: 'ping' };
  const command = COMMANDS[word];
  if (command) {
    const src = (rest[0] ?? '').toLowerCase();
    return { kind: 'command', command, source: src === 'sensor' ? 'sensor' : 'button' };
  }
  return { kind: 'unknown', raw };
}

const TEAM_WORD: Record<Team, string> = { white: 'BLANCO', blue: 'AZUL' };

/** Líneas que la app envía a las placas tras un cambio aceptado por el motor. */
export function outgoingLines(prev: MatchState | null, next: MatchState, events: MatchEvent[]): string[] {
  const out: string[] = [];
  if (!prev || prev.phase !== next.phase) out.push(`STATE ${next.phase}`);
  for (const e of events) {
    if (e.type === 'GOAL' && e.team) {
      out.push(`GOAL ${TEAM_WORD[e.team]}`);
      out.push(`LOCK ${GOAL_LOCK_MS}`);
    }
    if (e.type === 'MATCH_END' && e.team) out.push(`WIN ${TEAM_WORD[e.team]}`);
  }
  const score = getScore(next);
  const before = prev ? getScore(prev) : null;
  if (!before || before.white !== score.white || before.blue !== score.blue) out.push(`SCORE ${score.white} ${score.blue}`);
  return out;
}

/** Separa un flujo de texto en líneas completas (los datos pueden llegar troceados). */
export class LineSplitter {
  private buffer = '';

  push(chunk: string): string[] {
    this.buffer += chunk;
    const parts = this.buffer.split(/\r?\n/);
    this.buffer = parts.pop() ?? '';
    // Protección: una placa que nunca manda «\n» no debe llenar la memoria.
    if (this.buffer.length > 512) this.buffer = '';
    return parts.filter((p) => p.trim().length > 0);
  }
}
