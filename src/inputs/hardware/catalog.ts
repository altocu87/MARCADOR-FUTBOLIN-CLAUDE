/**
 * Catálogo «Hazlo tú mismo»: placas compatibles, sus pines, niveles de montaje y formas de detectar goles.
 * Una sola fuente (diy-catalog.json) para la guía de la app, el manual (docs/MANUAL_DIY.md) y las pruebas
 * que comprueban que coincide con los pines reales del firmware (hardware/common/mfv3_boards.h).
 */
import data from './diy-catalog.json';

export type BoardRole = 'entradas' | 'mando' | 'pantalla';
export type BoardLink = 'usb' | 'wifi' | 'bluetooth' | 'radio';
export type PinFunction = 'btnBlanco' | 'btnAzul' | 'pausa' | 'sensorBlanco' | 'sensorAzul' | 'ledBlanco' | 'ledAzul' | 'zumbador';
/** «probada» = en placa real · «simulada» = probada en el PC con la placa simulada. */
export type BoardStatus = 'probada' | 'simulada';

export interface DiyLevel {
  id: number;
  title: string;
  summary: string;
  needs: string[];
  cost: string;
  difficulty: number;
}

export interface PinFunctionInfo {
  label: string;
  detail: string;
  other: string;
  kind: 'button' | 'sensor' | 'output';
}

export interface BoardInfo {
  id: string;
  name: string;
  aka: string;
  role: BoardRole;
  level: number;
  chip: string;
  voltage: string;
  links: BoardLink[];
  sketch: string;
  ideBoard: string;
  core: string;
  price: string;
  status: BoardStatus;
  pins: Partial<Record<PinFunction, string>>;
  notes: string[];
  /** Compilación automática (GitHub Actions). Con chipFamily, se puede instalar desde el navegador. */
  build: { fqbn: string; chipFamily?: string; defines?: string };
}

/** Placas que se pueden instalar desde el navegador (familia ESP32, con Web Serial). */
export const webInstallable = (b: BoardInfo) => Boolean(b.build.chipFamily);

export interface DetectionOption {
  id: string;
  name: string;
  how: string;
  where: string;
  wiring: string;
  pros: string[];
  cons: string[];
  cost: string;
  difficulty: number;
  recommended: boolean;
  supported: boolean;
}

export const DIY_LEVELS = data.levels as DiyLevel[];
export const PIN_FUNCTIONS = data.functions as Record<PinFunction, PinFunctionInfo>;
export const PIN_ORDER: PinFunction[] = ['btnBlanco', 'btnAzul', 'pausa', 'sensorBlanco', 'sensorAzul', 'ledBlanco', 'ledAzul', 'zumbador'];
export const BOARDS = data.boards as BoardInfo[];
export const DETECTION = data.detection as DetectionOption[];
export const SAFETY = data.safety as string[];

/** Pasos para instalar el programa en una placa (los mismos en la app y en docs/MANUAL_DIY.md). */
export function installSteps(b: BoardInfo): string[] {
  const fill = (t: string) => t.replace(/\{(\w+)\}/g, (_, k: string) => String(b[k as keyof BoardInfo] ?? ''));
  const extra =
    b.role === 'pantalla' ? data.install.pantalla : b.role === 'mando' ? data.install.mando : b.links.includes('wifi') ? data.install.wifi : data.install.usb;
  return [...data.install.common, ...extra].map(fill);
}

export const ROLE_LABEL: Record<BoardRole, string> = {
  entradas: 'Pulsadores y sensores',
  mando: 'Mando inalámbrico',
  pantalla: 'Pantalla de mesa',
};

export const LINK_LABEL: Record<BoardLink, string> = { usb: 'USB', wifi: 'Wi-Fi', bluetooth: 'Bluetooth', radio: 'Radio' };

/** Lo que una placa sabe hacer, anunciado en su saludo (HELLO … caps=…). */
export const CAP_LABEL: Record<string, string> = {
  goles: 'Goles',
  anular: 'Anular gol',
  pausa: 'Pausa',
  sensores: 'Sensores de gol',
  leds: 'Luces',
  zumbador: 'Zumbador',
  wifi: 'Wi-Fi',
  bluetooth: 'Bluetooth',
  servidor: 'Sirve la app',
  pantalla: 'Pantalla propia',
  radio: 'Mando por radio',
  salidas: 'Salidas aisladas',
  uart: 'Puerto UART',
  brillo: 'Brillo de pantalla',
};

// Nombres que usaban las placas antes de presentarse con su modelo.
const LEGACY: Record<string, string> = { 'ARDUINO-USB': 'arduino-uno', ESP32: 'esp32', MARCADOR_V3_S3: 'waveshare-7c' };

export function findBoard(idOrHelloName: string | undefined): BoardInfo | undefined {
  if (!idOrHelloName) return undefined;
  const id = LEGACY[idOrHelloName] ?? idOrHelloName.toLowerCase();
  return BOARDS.find((b) => b.id === id);
}
