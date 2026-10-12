/**
 * Mantiene al día en internet el perfil de cada jugador (no invitados): un rato después de cada cambio
 * (partido, torneo, nombre, foto…) se calcula su «foto» y se sube solo si ha cambiado. Sin conexión o sin
 * Supabase configurado no hace nada: se reintenta en el siguiente cambio o al volver a abrir la app.
 */
import { useCallback, useEffect, useMemo, useRef } from 'react';
import { visibleItems, type Catalog, type RewardState } from '../services/events';
import type { Player, StoredMatch, Tournament } from '../services/persistence';
import {
  buildProfileSnapshot,
  credentialsFor,
  deleteProfile,
  hasPublicProfile,
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
}

const DELAY_MS = 4000;

export function useProfileSync(d: Data): { conn: SupabaseConn | null; publishNow(playerId: string): Promise<boolean> } {
  // Sin conexión en modo prueba (sus jugadores son ficticios) ni sin Supabase configurado.
  const conn = useMemo(() => (d.enabled ? supabaseConn() : null), [d.enabled]);
  const latest = useRef(d);
  latest.current = d;
  const running = useRef(false);

  const publishOne = useCallback(
    async (player: Player, force = false): Promise<boolean> => {
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
        await publishProfile(conn, c, snap);
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

  const syncAll = useCallback(async () => {
    if (!conn || running.current) return;
    running.current = true;
    try {
      const { players, store } = latest.current;
      for (const p of players) if (hasPublicProfile(p)) await publishOne(p);
      // Jugadores que ya no están (borrados o fusionados): su perfil se retira.
      const all = readCredentials(store);
      const alive = new Set(players.filter(hasPublicProfile).map((p) => p.id));
      for (const [id, c] of Object.entries(all)) {
        if (alive.has(id)) continue;
        try {
          await deleteProfile(conn, c);
          delete all[id];
        } catch {
          /* se reintenta otro día */
        }
      }
      writeCredentials(store, all);
    } finally {
      running.current = false;
    }
  }, [conn, publishOne]);

  useEffect(() => {
    if (!conn) return;
    const t = window.setTimeout(() => void syncAll(), DELAY_MS);
    return () => window.clearTimeout(t);
  }, [conn, syncAll, d.players, d.matches, d.tournaments, d.progression, d.rewards]);

  const publishNow = useCallback(
    async (playerId: string) => {
      const p = latest.current.players.find((x) => x.id === playerId);
      return p && hasPublicProfile(p) ? publishOne(p) : false;
    },
    [publishOne],
  );

  return { conn, publishNow };
}
