/**
 * Mantiene al día en internet el perfil de cada jugador: cada cambio (partido, torneo, nombre, foto…) deja la
 * subida pendiente y, cuando la app queda en reposo (ver idle.ts), se calcula su «foto» y se sube solo si ha
 * cambiado. Un toque o empezar un partido la cancela. Pedido a mano (QR del perfil), se sube en el momento.
 * Sin conexión o sin Supabase configurado no hace nada: se reintenta en el siguiente reposo.
 */
import { useCallback, useEffect, useMemo, useRef } from 'react';
import { abortableHttp } from '../services/cloud';
import type { IdleMonitor } from './idle';
import { useAutoSync } from './useIdle';
import { visibleItems, type Catalog, type RewardState } from '../services/events';
import type { Player, StoredMatch, Tournament } from '../services/persistence';
import {
  buildProfileSnapshot,
  credentialsFor,
  deleteProfile,
  publishProfile,
  readCredentials,
  snapshotHash,
  supabaseConn,
  writeCredentials,
  type SupabaseConn,
} from '../services/profiles';
import type { ProgressionSnapshot } from '../services/progression';

interface Data {
  enabled: boolean;
  store: Pick<Storage, 'getItem' | 'setItem'>;
  players: Player[];
  matches: StoredMatch[];
  tournaments: Tournament[];
  progression: ProgressionSnapshot | null;
  catalog: Catalog;
  rewards: RewardState;
  /** Vigilante de reposo de la app. */
  idle: IdleMonitor;
}

const fetchAll: typeof fetch = (...a) => fetch(...a);

export function useProfileSync(d: Data): { conn: SupabaseConn | null; publishNow(playerId: string): Promise<boolean> } {
  // Sin conexión en modo prueba (sus jugadores son ficticios) ni sin Supabase configurado.
  const conn = useMemo(() => (d.enabled ? supabaseConn() : null), [d.enabled]);
  const latest = useRef(d);
  latest.current = d;
  const running = useRef(false);

  /** Sube el perfil de un jugador si ha cambiado. `signal`: subida automática (se cancela al tocar o al jugar). */
  const publishOne = useCallback(
    async (player: Player, force = false, signal?: AbortSignal): Promise<boolean> => {
      if (!conn) return false;
      const { store, players, matches, tournaments, progression, catalog, rewards } = latest.current;
      const items = new Map(visibleItems(catalog).map((x) => [x.item.id, x]));
      const own = rewards.byPlayer.get(player.id) ?? new Map<string, number>();
      const snap = buildProfileSnapshot({
        player,
        players,
        matches,
        tournaments,
        progress: progression?.players.get(player.id),
        rewards: [...own].flatMap(([id, at]) => {
          const x = items.get(id);
          return x ? [{ name: x.item.nombre || x.col.nombre, tipo: x.col.tipo, archivo: x.item.archivo, at }] : [];
        }),
        now: Date.now(),
      });
      const hash = snapshotHash(snap);
      const c = credentialsFor(store, player.id);
      if (!force && c.hash === hash) return true;
      try {
        await publishProfile(conn, c, snap, abortableHttp(fetchAll, { signal }));
        const all = readCredentials(store);
        all[player.id] = { ...c, hash };
        writeCredentials(store, all);
        return true;
      } catch {
        return false;
      }
    },
    [conn],
  );

  const syncAll = useCallback(async (signal: AbortSignal): Promise<boolean | null> => {
    if (!conn || running.current) return null;
    running.current = true;
    let ok = true;
    try {
      const { players, store } = latest.current;
      for (const p of players) {
        if (signal.aborted) return null;
        if (!(await publishOne(p, false, signal))) ok = false;
        // Un respiro entre jugador y jugador: calcular cada perfil cuesta.
        await new Promise((r) => setTimeout(r, 0));
      }
      if (signal.aborted) return null;
      // Jugadores que ya no están (borrados o fusionados): su perfil se retira.
      const all = readCredentials(store);
      const alive = new Set(players.map((p) => p.id));
      for (const [id, c] of Object.entries(all)) {
        if (alive.has(id)) continue;
        if (signal.aborted) break;
        try {
          await deleteProfile(conn, c, abortableHttp(fetchAll, { signal }));
          delete all[id];
        } catch {
          ok = false;  // se reintenta otro día
        }
      }
      writeCredentials(store, all);
      return signal.aborted ? null : ok;
    } finally {
      running.current = false;
    }
  }, [conn, publishOne]);

  // Cada cambio solo deja la subida pendiente; se hace en reposo.
  const auto = useAutoSync(d.idle, !!conn, syncAll, { checkEveryMs: null });
  useEffect(() => {
    if (conn) auto.markPending();
  }, [conn, auto, d.players, d.matches, d.tournaments, d.progression, d.rewards]);

  const publishNow = useCallback(
    async (playerId: string) => {
      const p = latest.current.players.find((x) => x.id === playerId);
      return p ? publishOne(p) : false;
    },
    [publishOne],
  );

  return { conn, publishNow };
}
