/**
 * Premios de los eventos (Halloween, Navidad…): quién ha conseguido cada pieza del catálogo.
 * No se guarda nada aparte: se deduce de los partidos, los torneos, los logros y los días en que se abrió
 * la app, igual que el ELO o los logros. Así, si se borra un partido, el premio que dio desaparece solo.
 */
import type { StoredMatch, Tournament, TournamentFormat } from '../persistence';
import type { UnlockedAchievement } from '../progression';
import {
  OBTENCION_DE_EVENTO,
  disponibleAhora,
  obtencionDe,
  type CatalogCollection,
  type CatalogItem,
  type ModoPartido,
  type Obtencion,
} from './model';

export interface Catalog {
  collections: CatalogCollection[];
  items: CatalogItem[];
}

export interface RewardInputs {
  matches: StoredMatch[];
  tournaments: Tournament[];
  /** Logros de cada jugador con su fecha. */
  achievements?: Map<string, UnlockedAchievement[]>;
  /** Días (YYYY-MM-DD) en que se abrió la app en este aparato. */
  visitDays?: string[];
}

export interface RewardState {
  /** Por jugador: pieza → cuándo la consiguió. */
  byPlayer: Map<string, Map<string, number>>;
  /** Del aparato (por entrar en la app): pieza → cuándo. */
  device: Map<string, number>;
}

/** Piezas publicadas y activas de colecciones que se pueden mostrar en la app. */
export function visibleItems(catalog: Catalog): { item: CatalogItem; col: CatalogCollection }[] {
  const cols = new Map(catalog.collections.filter((c) => c.estado !== 'borrador').map((c) => [c.id, c]));
  return catalog.items
    .filter((i) => i.activo && cols.has(i.coleccionId))
    .map((item) => ({ item, col: cols.get(item.coleccionId)! }))
    .sort((a, b) => a.col.orden - b.col.orden || a.item.orden - b.item.orden);
}

const dayKey = (t: number) => {
  const d = new Date(t);
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
};
const dayStart = (key: string) => {
  const [y, m, d] = key.split('-').map(Number);
  return new Date(y, m - 1, d, 12).getTime();
};

/** ¿Este partido cuenta para ese tipo? Los de modalidad son partidos sueltos; los de liga/copa/pool, de torneo. */
export function matchCounts(modo: ModoPartido, m: StoredMatch, formatOf: (tournamentId: string) => TournamentFormat | undefined): boolean {
  if (modo === 'cualquiera') return true;
  const fmt = m.tournament ? formatOf(m.tournament.id) : undefined;
  if (modo === 'league' || modo === 'bracket' || modo === 'pool') return fmt === modo;
  return !m.tournament && m.config.mode === modo;
}

const winnersOf = (m: StoredMatch) => (m.result.winner ? m.participants.filter((p) => p.team === m.result.winner).map((p) => p.playerId) : []);

/** Quién ha conseguido cada pieza (con la fecha en que la consiguió). */
export function computeRewards(catalog: Catalog, data: RewardInputs): RewardState {
  const byPlayer = new Map<string, Map<string, number>>();
  const device = new Map<string, number>();
  const give = (pid: string, itemId: string, at: number) => {
    const m = byPlayer.get(pid) ?? new Map<string, number>();
    if (!m.has(itemId) || m.get(itemId)! > at) m.set(itemId, at);
    byPlayer.set(pid, m);
  };
  const fmt = new Map(data.tournaments.map((t) => [t.id, t.format]));
  const formatOf = (id: string) => fmt.get(id);
  const matches = [...data.matches].sort((a, b) => a.finishedAt - b.finishedAt);

  for (const { item, col } of visibleItems(catalog)) {
    const o: Obtencion = obtencionDe(item, col);
    // Retirada: ya no se consigue (quien la consiguió antes la conserva por las fechas de sus partidos).
    const inWindow = (t: number) => disponibleAhora(col.disponibilidad, t);
    switch (o.tipo) {
      case 'gratis':
        if (col.disponibilidad.modo === 'siempre') device.set(item.id, 0);
        else for (const d of data.visitDays ?? []) if (inWindow(dayStart(d))) { device.set(item.id, Math.min(device.get(item.id) ?? Infinity, dayStart(d))); }
        break;
      case 'entrar':
        for (const d of data.visitDays ?? []) {
          const t = dayStart(d);
          if (inWindow(t)) device.set(item.id, Math.min(device.get(item.id) ?? Infinity, t));
        }
        break;
      case 'jugarPartido':
      case 'ganarPartido':
        for (const m of matches) {
          if (!inWindow(m.finishedAt) || !matchCounts(o.modo, m, formatOf)) continue;
          const who = o.tipo === 'ganarPartido' ? winnersOf(m) : m.participants.map((p) => p.playerId);
          who.forEach((pid) => give(pid, item.id, m.finishedAt));
        }
        break;
      case 'evento': {
        // N partidos dentro de la MISMA ventana del evento (cada año cuenta de nuevo).
        const count = new Map<string, number>();
        for (const m of matches) {
          if (!inWindow(m.finishedAt)) continue;
          for (const p of m.participants) {
            const k = `${p.playerId}|${windowKey(col, m.finishedAt)}`;
            count.set(k, (count.get(k) ?? 0) + 1);
            if (count.get(k)! >= o.partidos) give(p.playerId, item.id, m.finishedAt);
          }
        }
        break;
      }
      case 'ganarTorneo':
        for (const t of data.tournaments) {
          if (t.status !== 'finished' || !t.finishedAt || !inWindow(t.finishedAt)) continue;
          if (o.formato !== 'cualquiera' && t.format !== o.formato) continue;
          const champ = t.teams.find((x) => x.id === t.winnerTeamId);
          champ?.playerIds.forEach((pid) => give(pid, item.id, t.finishedAt!));
        }
        break;
      case 'logro':
        for (const [pid, list] of data.achievements ?? []) {
          for (const a of list) if (a.id === o.logro && inWindow(a.at)) give(pid, item.id, a.at);
        }
        break;
      default:
        // Nivel, rango, compras, códigos…: todavía no se dan en la app.
        break;
    }
  }
  return { byPlayer, device };
}

/** Cada vuelta del evento anual (o la única, con fechas) tiene su propia cuenta. */
function windowKey(col: CatalogCollection, t: number): string {
  if (col.disponibilidad.modo !== 'anual') return 'unica';
  const d = new Date(t);
  const md = dayKey(t).slice(5);
  // Si cruza el año y estamos en enero, la vuelta empezó el año anterior.
  const cross = (col.disponibilidad.desdeDia ?? '') > (col.disponibilidad.hastaDia ?? '');
  return String(cross && md <= (col.disponibilidad.hastaDia ?? '') ? d.getFullYear() - 1 : d.getFullYear());
}

/** ¿La tiene alguno de estos jugadores o el aparato? */
export function ownedBy(state: RewardState, itemId: string, playerIds: string[]): boolean {
  if (state.device.has(itemId)) return true;
  return playerIds.some((pid) => state.byPlayer.get(pid)?.has(itemId));
}

/** Piezas que cada jugador ha conseguido con ESTE partido (para avisarlo al acabar). */
export function unlockedByMatch(catalog: Catalog, data: RewardInputs, matchId: string): { playerId: string; itemId: string }[] {
  const before = computeRewards(catalog, { ...data, matches: data.matches.filter((m) => m.id !== matchId) });
  const after = computeRewards(catalog, data);
  const out: { playerId: string; itemId: string }[] = [];
  for (const [pid, items] of after.byPlayer) for (const id of items.keys()) if (!before.byPlayer.get(pid)?.has(id)) out.push({ playerId: pid, itemId: id });
  return out;
}

export interface ActiveEvent {
  /** Colecciones del mismo evento (mismo nombre de temporada) que se pueden conseguir ahora. */
  title: string;
  rewards: { item: CatalogItem; col: CatalogCollection; obtencion: Obtencion }[];
}

/**
 * Lo que se puede conseguir ahora mismo por un evento: piezas publicadas, con forma de evento y dentro de
 * sus fechas. Se agrupan por la primera palabra del nombre de la colección («Halloween 2026 · Escudos» →
 * «Halloween 2026»), que es como se nombran en el panel.
 */
export function activeEvents(catalog: Catalog, now: number): ActiveEvent[] {
  const groups = new Map<string, ActiveEvent>();
  for (const { item, col } of visibleItems(catalog)) {
    if (col.estado !== 'publicada') continue;
    const o = obtencionDe(item, col);
    if (!OBTENCION_DE_EVENTO.includes(o.tipo) || !disponibleAhora(col.disponibilidad, now)) continue;
    const title = col.nombre.split(/\s[·\-–]\s/)[0].trim() || col.nombre;
    const g = groups.get(title) ?? { title, rewards: [] };
    g.rewards.push({ item, col, obtencion: o });
    groups.set(title, g);
  }
  return [...groups.values()];
}

export { dayKey };
