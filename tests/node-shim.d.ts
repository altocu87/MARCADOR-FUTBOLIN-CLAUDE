// Lo poco de Node que usan las pruebas que leen archivos de tests/fixtures (el proyecto no instala @types/node).
declare module 'node:fs' {
  export function readFileSync(path: string, encoding: 'utf8'): string;
  export function writeFileSync(path: string, data: string): void;
  export function existsSync(path: string): boolean;
  export function mkdirSync(path: string, options?: { recursive?: boolean }): void;
  export function readdirSync(path: string): string[];
}
declare module 'node:path' {
  export function join(...parts: string[]): string;
}
declare const process: { env: Record<string, string | undefined> };
declare const __dirname: string;
