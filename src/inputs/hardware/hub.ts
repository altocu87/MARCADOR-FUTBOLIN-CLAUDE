/**
 * Centro de conexiones: gestiona los enlaces activos, traduce líneas de las placas
 * a entradas comunes (inputBus) y reenvía a las placas los cambios aceptados por el motor.
 */
import type { MatchEvent, MatchState } from '../../match-engine';
import { inputBus } from '../inputBus';
import { BluetoothLink, SerialLink, WebSocketLink, type HardwareLink, type LinkKind, type LinkStatus } from './links';
import { findBoard } from './catalog';
import { PROTOCOL_VERSION, outgoingLines, parseBoardLine } from './protocol';
import type { StoredSound } from '../../services/persistence';
import {
  BoardSoundError,
  parseSoundsList,
  soundCode,
  uploadSound,
  type BoardInventory,
  type BoardPlace,
  type BoardTransport,
} from './boardSounds';

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
  /** Respuestas esperadas de una placa (subida de sonidos). */
  private waiters: { match: (line: string) => boolean; resolve: (line: string) => void }[] = [];
  /** Sonidos que tiene la placa con altavoz conectada (null: ninguna conectada o aún no ha contestado). */
  inventory: BoardInventory | null = null;
  /** Qué suena en cada momento y volumen, para la placa (se le repite al conectarla). */
  private soundSetup: string[] = [];
  private soundSetupSent: string[] = [];
  private uploading = false;
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
    if (!this.soundKind()) this.inventory = null;
    this.changed();
  }

  private handleLine(kind: LinkKind, link: HardwareLink, line: string, setState: (p: Partial<LinkState>) => void): void {
    // Las respuestas a cada trozo de un sonido no van al registro (serían cientos).
    if (!/^SONIDO_OK\b/i.test(line)) this.addLog({ dir: 'in', kind, text: line });
    const waiter = this.waiters.find((w) => w.match(line));
    if (waiter) {
      this.waiters = this.waiters.filter((w) => w !== waiter);
      waiter.resolve(line);
    }
    const inv = parseSoundsList(line);
    if (inv) {
      this.inventory = inv;
      this.changed();
      return;
    }
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
      // Placa con altavoz: se le pregunta qué sonidos tiene y se le dice qué suena en cada momento.
      if (msg.caps.includes('sonido')) {
        this.sendLog(link, 'SONIDOS?');
        this.soundSetup.forEach((l) => this.sendLog(link, l));
        this.soundSetupSent = [...this.soundSetup];
      }
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

  /**
   * ¿Dónde puede guardar sonidos la placa con altavoz conectada? `sd`: en su tarjeta SD (cap «sd»); `chip`: en su
   * memoria, solo si su lista de sonidos lo dice (la pantalla de 7" la reserva para imágenes y contesta «chip=-»).
   */
  get soundTargets(): { chip: boolean; sd: boolean } {
    const kind = this.soundKind();
    if (!kind) return { chip: false, sd: false };
    const inv = this.inventory;
    return {
      chip: inv ? inv.chipFreeKB !== null : false,
      sd: !!this.state(kind).caps?.includes('sd') && (inv ? inv.sdFreeKB !== null : true),
    };
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

  private sendLog(link: HardwareLink, line: string): void {
    link.send(line);
    if (!line.startsWith('SONIDO_DATOS')) this.addLog({ dir: 'out', kind: link.kind, text: line });
  }

  /** Conexión con una placa que tiene altavoz (cap «sonido»). */
  private soundKind(): LinkKind | null {
    for (const [kind] of this.links) {
      const st = this.state(kind);
      if (st.status === 'connected' && st.caps?.includes('sonido')) return kind;
    }
    return null;
  }

  /** ¿Hay conectada una placa con altavoz? */
  get soundBoard(): boolean {
    return this.soundKind() !== null;
  }

  private transport(kind: LinkKind): BoardTransport {
    const link = this.links.get(kind)!;
    return {
      send: (line) => this.sendLog(link, line),
      waitFor: (match, timeoutMs) =>
        new Promise((resolve, reject) => {
          const w = { match, resolve: (l: string) => (clearTimeout(timer), resolve(l)) };
          const timer = setTimeout(() => {
            this.waiters = this.waiters.filter((x) => x !== w);
            reject(new Error('sin respuesta'));
          }, timeoutMs);
          this.waiters.push(w);
        }),
    };
  }

  /** ¿Dónde tiene la placa este sonido? (null si no lo tiene o no hay placa con altavoz). */
  boardHas(soundId: string): BoardPlace | null {
    return this.inventory?.sounds.get(soundCode(soundId)) ?? null;
  }

  /** Pasa un sonido a la placa (memoria del chip o tarjeta SD). */
  async uploadSound(sound: StoredSound, place: BoardPlace, onProgress?: (fraction: number) => void): Promise<void> {
    const kind = this.soundKind();
    if (!kind) throw new BoardSoundError('No hay ninguna placa con altavoz conectada');
    if (this.uploading) throw new BoardSoundError('Ya se está pasando otro sonido');
    this.uploading = true;
    try {
      await uploadSound(this.transport(kind), sound, place, onProgress);
      this.inventory?.sounds.set(soundCode(sound.id), place);
      this.changed();
    } finally {
      this.uploading = false;
      this.setSoundSetup(this.soundSetup);
    }
  }

  /** Borra un sonido de la placa (si hay una conectada). */
  deleteBoardSound(soundId: string): void {
    const kind = this.soundKind();
    if (!kind) return;
    const code = soundCode(soundId);
    this.sendLog(this.links.get(kind)!, `SONIDO_BORRAR ${code}`);
    this.inventory?.sounds.delete(code);
    this.changed();
  }

  /** Toca un sonido guardado en la placa. false si no puede (no hay placa con altavoz o no lo tiene). */
  playBoardSound(soundId: string, queue = false): boolean {
    const kind = this.soundKind();
    if (!kind || this.uploading || !this.boardHas(soundId)) return false;
    this.sendLog(this.links.get(kind)!, `SONIDO ${soundCode(soundId)}${queue ? ' cola' : ''}`);
    return true;
  }

  /** Qué suena en cada momento y volumen: se manda lo que haya cambiado (y todo al conectar la placa). */
  setSoundSetup(lines: string[]): void {
    this.soundSetup = lines;
    const kind = this.soundKind();
    if (!kind || this.uploading) return;
    const link = this.links.get(kind)!;
    lines.filter((l) => !this.soundSetupSent.includes(l)).forEach((l) => this.sendLog(link, l));
    this.soundSetupSent = [...lines];
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
