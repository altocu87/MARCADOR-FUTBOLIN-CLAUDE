/**
 * Centro de conexiones: gestiona los enlaces activos, traduce líneas de las placas
 * a entradas comunes (inputBus) y reenvía a las placas los cambios aceptados por el motor.
 */
import type { MatchEvent, MatchState } from '../../match-engine';
import { inputBus } from '../inputBus';
import { BluetoothLink, SerialLink, WebSocketLink, type HardwareLink, type LinkKind, type LinkStatus } from './links';
import { findBoard } from './catalog';
import { PROTOCOL_VERSION, outgoingLines, parseBoardLine } from './protocol';

export interface LinkState {
  kind: LinkKind;
  status: LinkStatus;
  detail?: string;
  board?: string;
  /** Modelo anunciado por la placa (id del catálogo si lo reconoce) y lo que sabe hacer. */
  boardId?: string;
  caps?: string[];
}

export interface LogEntry {
  at: number;
  dir: 'in' | 'out' | 'info';
  kind?: LinkKind;
  text: string;
}

type Listener = () => void;

class HardwareHub {
  private links = new Map<LinkKind, HardwareLink>();
  private states = new Map<LinkKind, LinkState>();
  private listeners = new Set<Listener>();
  private lastState: MatchState | null = null;
  /** Brillo pedido desde el panel rápido (0–100); se manda a las placas que saben cambiarlo. */
  private brightness: number | null = null;
  log: LogEntry[] = [];

  subscribe(l: Listener): () => void {
    this.listeners.add(l);
    return () => this.listeners.delete(l);
  }

  private changed(): void {
    for (const l of this.listeners) l();
  }

  private addLog(entry: Omit<LogEntry, 'at'>): void {
    this.log = [...this.log.slice(-39), { ...entry, at: Date.now() }];
    this.changed();
  }

  state(kind: LinkKind): LinkState {
    return this.states.get(kind) ?? { kind, status: 'disconnected' };
  }

  get connectedCount(): number {
    return [...this.states.values()].filter((s) => s.status === 'connected').length;
  }

  async connect(kind: LinkKind, options: { url?: string; reuseGranted?: boolean } = {}): Promise<void> {
    await this.disconnect(kind);
    const link: HardwareLink =
      kind === 'serial'
        ? new SerialLink(115200, options.reuseGranted)
        : kind === 'websocket'
          ? new WebSocketLink(options.url ?? '')
          : new BluetoothLink();
    this.links.set(kind, link);
    const setState = (patch: Partial<LinkState>) => {
      this.states.set(kind, { ...this.state(kind), ...patch, kind });
      this.changed();
    };
    try {
      await link.connect({
        onStatus: (status, detail) => {
          setState({ status, detail });
          if (status === 'connected') {
            link.send(`HELLO MARCADOR_V3 ${PROTOCOL_VERSION}`);
            if (this.lastState) this.sendState(link, null, this.lastState, []);
          }
        },
        onLine: (line) => this.handleLine(kind, link, line, setState),
      });
    } catch (err) {
      setState({ status: 'error', detail: err instanceof Error ? err.message : String(err) });
      this.links.delete(kind);
    }
  }

  async disconnect(kind: LinkKind): Promise<void> {
    const link = this.links.get(kind);
    this.links.delete(kind);
    if (link) await link.disconnect();
    this.states.set(kind, { kind, status: 'disconnected' });
    this.changed();
  }

  private handleLine(kind: LinkKind, link: HardwareLink, line: string, setState: (p: Partial<LinkState>) => void): void {
    this.addLog({ dir: 'in', kind, text: line });
    const msg = parseBoardLine(line);
    if (!msg) return;
    if (msg.kind === 'hello') {
      const known = findBoard(msg.name);
      setState({
        board: `${known?.name ?? msg.name}${msg.version ? ` v${msg.version}` : ''}`,
        boardId: known?.id ?? msg.name,
        caps: msg.caps,
      });
      if (this.brightness !== null && msg.caps.includes('brillo')) link.send(`BRILLO ${this.brightness}`);
    }
    else if (msg.kind === 'ping') link.send('PONG');
    else if (msg.kind === 'command') inputBus.emit({ command: msg.command, source: msg.source });
  }

  /** Llamado por el controlador del partido tras cada cambio aceptado. */
  notifyMatch(next: MatchState, events: MatchEvent[]): void {
    const prev = this.lastState;
    this.lastState = next;
    for (const link of this.links.values()) this.sendState(link, prev, next, events);
  }

  private sendState(link: HardwareLink, prev: MatchState | null, next: MatchState, events: MatchEvent[]): void {
    for (const line of outgoingLines(prev, next, events)) {
      link.send(line);
      this.addLog({ dir: 'out', kind: link.kind, text: line });
    }
  }

  /** ¿Hay conectada una placa que sabe cambiar el brillo de su pantalla? */
  get controlsBrightness(): boolean {
    return [...this.states.values()].some((s) => s.status === 'connected' && !!s.caps?.includes('brillo'));
  }

  /** Brillo de la pantalla (0–100): a las placas que lo saben cambiar; se repite al conectar una. */
  setBrightness(percent: number): void {
    this.brightness = Math.max(0, Math.min(100, Math.round(percent)));
    for (const [kind, link] of this.links) {
      if (!this.state(kind).caps?.includes('brillo')) continue;
      const line = `BRILLO ${this.brightness}`;
      link.send(line);
      this.addLog({ dir: 'out', kind, text: line });
    }
  }

  /** Fuera de partido (inicio, ajustes…). */
  notifyIdle(): void {
    if (!this.lastState) return;
    this.lastState = null;
    for (const link of this.links.values()) link.send('STATE idle');
  }
}

export const hardwareHub = new HardwareHub();

/**
 * ¿La app se está sirviendo desde una placa (ESP32 en la red local)?
 * http + IP privada o nombre .local → conectar por Wi-Fi automáticamente.
 */
export function servedFromBoard(): boolean {
  if (typeof location === 'undefined' || location.protocol !== 'http:') return false;
  const h = location.hostname;
  return /^(192\.168\.|10\.|172\.(1[6-9]|2\d|3[01])\.)/.test(h) || h.endsWith('.local');
}

/** URL WebSocket por defecto: si la app se sirve desde la placa, su misma IP en el puerto 81. */
export function defaultWsUrl(): string {
  const host = typeof location !== 'undefined' ? location.hostname : '';
  const isLocalDev = !host || host === 'localhost' || host === '127.0.0.1';
  return `ws://${isLocalDev ? '192.168.4.1' : host}:81/`;
}
