/**
 * Cuenta del dueño de la mesa y sincronización con la nube mientras la app está abierta: al entrar, al abrir la
 * app, un rato después de cada cambio (jugador, partido, torneo, ajustes), cada minuto y al volver la conexión.
 * Sin Supabase configurado, en modo prueba o sin cuenta, no hace nada (la app sigue en local como siempre).
 */
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { Player, Preferences, Repositories, StoredMatch, Tournament } from '../services/persistence';
import { supabaseConn } from '../services/profiles';
import {
  CloudError,
  freshSession,
  readSession,
  readState,
  recover,
  restApi,
  signIn as cloudSignIn,
  signOut as cloudSignOut,
  signUp as cloudSignUp,
  syncOnce,
  writeSession,
  writeState,
  type Session,
} from '../services/cloud';

export type CloudStatus = 'off' | 'idle' | 'syncing' | 'ok' | 'error';

export interface CloudSync {
  /** ¿Hay nube configurada en esta versión de la app? */
  available: boolean;
  session: Session | null;
  status: CloudStatus;
  /** Última sincronización correcta (epoch ms). */
  lastSync: number | null;
  error: string | null;
  signIn(email: string, password: string): Promise<void>;
  /** Devuelve true si hay que confirmar el correo antes de entrar. */
  signUp(email: string, password: string): Promise<boolean>;
  recover(email: string): Promise<void>;
  signOut(): Promise<void>;
  syncNow(): Promise<void>;
}

interface Data {
  enabled: boolean;
  store: Storage | { getItem(k: string): string | null; setItem(k: string, v: string): void; removeItem(k: string): void };
  repos: Repositories;
  players: Player[];
  matches: StoredMatch[];
  tournaments: Tournament[];
  /** Preferencias tal como están guardadas en este aparato. */
  prefs: Preferences;
  loaded: boolean;
  refresh(): Promise<void>;
}

const AFTER_CHANGE_MS = 3000;
const EVERY_MS = 60_000;

export function useCloudSync(d: Data): CloudSync {
  const conn = useMemo(() => supabaseConn(), []);
  const [session, setSession] = useState<Session | null>(() => (conn ? readSession(d.store) : null));
  const [status, setStatus] = useState<CloudStatus>(session ? 'idle' : 'off');
  const [lastSync, setLastSync] = useState<number | null>(null);
  const [error, setError] = useState<string | null>(null);
  const latest = useRef(d);
  latest.current = d;
  const sessionRef = useRef(session);
  sessionRef.current = session;
  const running = useRef<Promise<void> | null>(null);
  const again = useRef(false);

  const keep = useCallback(
    (s: Session | null) => {
      writeSession(latest.current.store, s);
      setSession(s);
      sessionRef.current = s;
    },
    [],
  );

  const runSync = useCallback(async (): Promise<void> => {
    const cur = latest.current;
    if (!conn || !cur.enabled || !cur.loaded || !sessionRef.current) return;
    if (running.current) {
      again.current = true;  // ya hay una en marcha: otra al acabar
      return running.current;
    }
    const job = (async () => {
      setStatus('syncing');
      try {
        let s = sessionRef.current!;
        const api = restApi(conn, async () => {
          s = await freshSession(conn, s);
          if (s !== sessionRef.current) keep(s);
          return s.accessToken;
        });
        const userId = s.userId;
        const local = latest.current;
        const r = await syncOnce(api, { players: local.players, matches: local.matches, tournaments: local.tournaments, prefs: local.prefs }, readState(local.store, userId));
        if (r.data) {
          // Lo que llega de la nube se guarda aquí y se vuelve a cargar todo (ELO, XP… se recalculan solos).
          await local.repos.players.saveAll(r.data.players);
          await local.repos.matches.saveAll(r.data.matches);
          await local.repos.tournaments.saveAll(r.data.tournaments);
          await local.repos.preferences.save(r.data.prefs);
          await local.refresh();
        }
        writeState(local.store, userId, r.state);
        setLastSync(Date.now());
        setError(null);
        setStatus('ok');
      } catch (err) {
        const msg = err instanceof Error ? err.message : 'No se pudo sincronizar';
        if (err instanceof CloudError && (err.code === '401' || err.code === '403')) {
          keep(null);  // la sesión ya no vale: hay que volver a entrar
          setStatus('off');
          setError('La sesión ha caducado: vuelve a entrar');
          return;
        }
        setError(err instanceof CloudError && err.code === 'offline' ? 'Sin conexión: se sincronizará al volver' : msg);
        setStatus('error');
      }
    })();
    running.current = job;
    try {
      await job;
    } finally {
      running.current = null;
      if (again.current) {
        again.current = false;
        void runSync();
      }
    }
  }, [conn, keep]);

  // Al abrir (con la sesión guardada) y cada vez que cambian los datos de la mesa (un rato después).
  const { players, matches, tournaments, prefs, loaded, enabled } = d;
  useEffect(() => {
    if (!session || !loaded || !enabled) return;
    const t = window.setTimeout(() => void runSync(), AFTER_CHANGE_MS);
    return () => window.clearTimeout(t);
  }, [players, matches, tournaments, prefs, loaded, enabled, session, runSync]);

  // Cada minuto (lo que se haya jugado en la placa u otro aparato) y al volver la conexión o la pestaña.
  useEffect(() => {
    if (!session || !enabled) return;
    const t = window.setInterval(() => void runSync(), EVERY_MS);
    const now = () => void runSync();
    window.addEventListener('online', now);
    window.addEventListener('focus', now);
    return () => {
      window.clearInterval(t);
      window.removeEventListener('online', now);
      window.removeEventListener('focus', now);
    };
  }, [session, enabled, runSync]);

  const signIn = useCallback(
    async (email: string, password: string) => {
      if (!conn) throw new CloudError('La nube no está configurada');
      const s = await cloudSignIn(conn, email, password);
      keep(s);
      setStatus('idle');
      setError(null);
    },
    [conn, keep],
  );

  const signUp = useCallback(
    async (email: string, password: string) => {
      if (!conn) throw new CloudError('La nube no está configurada');
      const r = await cloudSignUp(conn, email, password);
      if (r.session) {
        keep(r.session);
        setStatus('idle');
      }
      return r.needsConfirm;
    },
    [conn, keep],
  );

  const doRecover = useCallback(
    async (email: string) => {
      if (!conn) throw new CloudError('La nube no está configurada');
      await recover(conn, email);
    },
    [conn],
  );

  const signOut = useCallback(async () => {
    const s = sessionRef.current;
    if (conn && s) await cloudSignOut(conn, s);
    keep(null);
    setStatus('off');
    setLastSync(null);
    setError(null);
  }, [conn, keep]);

  return { available: !!conn, session, status: session ? status : 'off', lastSync, error, signIn, signUp, recover: doRecover, signOut, syncNow: runSync };
}
