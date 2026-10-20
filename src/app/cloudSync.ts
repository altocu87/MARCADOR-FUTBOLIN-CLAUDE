/**
 * Cuenta del dueño de la mesa y sincronización con la nube mientras la app está abierta: al entrar, al abrir la
 * app, un rato después de cada cambio (jugador, partido, torneo, ajustes), cada minuto y al volver la conexión.
 * Sin Supabase configurado, en modo prueba o sin cuenta, no hace nada (la app sigue en local como siempre).
 */
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { Player, Preferences, Repositories, StoredMatch, Tournament } from '../services/persistence';
import { supabaseConn } from '../services/profiles';
import {
  boardCodeFrom,
  linkBoard as linkBoardCall,
  CloudError,
  freshSession,
  readSession,
  readRememberSession,
  REMEMBER_KEY,
  SESSION_KEY,
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
  rememberSession: boolean;
  setRememberSession(remember: boolean): void;
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
  /** Placa esperando a que se confirme su QR (la app se abrió con …#vincular=<código>). */
  pendingBoard: string | null;
  setPendingBoard(code: string | null): void;
  /** Conecta la placa del QR a esta cuenta. */
  linkBoard(code: string): Promise<void>;
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
  // Desactivado: sessionStorage conserva el acceso al recargar esta pestaña, hasta cerrarla.
  // Nunca copiar esta preferencia a los datos de la mesa en la nube.
  const temporary = useMemo(() => {
    try { return window.sessionStorage; } catch { return null; }
  }, []);
  const [rememberSession, setRemember] = useState(() => readRememberSession(d.store));
  const rememberRef = useRef(rememberSession);
  const [session, setSession] = useState<Session | null>(() =>
    conn ? readSession(rememberSession ? d.store : temporary ?? { getItem: () => null }) : null);
  const generation = useRef(0);
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
      if (rememberRef.current) {
        writeSession(latest.current.store, s);
        if (temporary) writeSession(temporary, null);
      } else {
        writeSession(latest.current.store, null);
        if (temporary) writeSession(temporary, s);
      }
      setSession(s);
      sessionRef.current = s;
    },
    [temporary],
  );

  const setRememberSession = useCallback((remember: boolean) => {
    rememberRef.current = remember;
    try { latest.current.store.setItem(REMEMBER_KEY, remember ? '1' : '0'); } catch { /* sesión en memoria */ }
    keep(sessionRef.current);
    setRemember(remember);
  }, [keep]);

  // Un único intercambio del token por pestaña; Web Locks evita renovaciones simultáneas entre pestañas.
  const refreshing = useRef<Promise<Session> | null>(null);
  const currentSession = useCallback(async (): Promise<Session> => {
    if (refreshing.current) return refreshing.current;
    const version = generation.current;
    const renew = async () => {
      let s = sessionRef.current;
      if (!s || version !== generation.current) throw new CloudError('La cuenta ha cambiado', 'session_changed');
      const stored = readSession(rememberRef.current ? latest.current.store : temporary ?? { getItem: () => null });
      if (stored && stored.userId === s.userId) s = stored;
      const next = await freshSession(conn!, s);
      if (version !== generation.current || !sessionRef.current) throw new CloudError('La cuenta ha cambiado', 'session_changed');
      if (next !== sessionRef.current) keep(next);
      return next;
    };
    const job = typeof navigator !== 'undefined' && navigator.locks
      ? navigator.locks.request('mfv3:auth:refresh', renew) : renew();
    refreshing.current = job;
    try { return await job; } finally { if (refreshing.current === job) refreshing.current = null; }
  }, [conn, keep, temporary]);

  useEffect(() => {
    const changed = (e: StorageEvent) => {
      if (e.key !== SESSION_KEY && e.key !== REMEMBER_KEY && e.key !== null) return;
      if (e.storageArea !== window.localStorage) return;
      const remember = readRememberSession(latest.current.store);
      const s = readSession(remember ? latest.current.store : temporary ?? { getItem: () => null });
      if (!s || s.userId !== sessionRef.current?.userId) {
        ++generation.current;
        refreshing.current = null;
      }
      rememberRef.current = remember;
      setRemember(remember);
      sessionRef.current = s;
      setSession(s);
    };
    window.addEventListener('storage', changed);
    return () => window.removeEventListener('storage', changed);
  }, [temporary]);

  const runSync = useCallback(async (): Promise<void> => {
    const cur = latest.current;
    if (!conn || !cur.enabled || !cur.loaded || !sessionRef.current) return;
    if (running.current) {
      again.current = true;  // ya hay una en marcha: otra al acabar
      return running.current;
    }
    const job = (async () => {
      const version = generation.current;
      setStatus('syncing');
      try {
        let s = sessionRef.current!;
        const api = restApi(conn, async () => {
          s = await currentSession();
          return s.accessToken;
        });
        const userId = s.userId;
        const local = latest.current;
        const r = await syncOnce(api, { players: local.players, matches: local.matches, tournaments: local.tournaments, prefs: local.prefs }, readState(local.store, userId));
        if (version !== generation.current) return;
        if (r.data) {
          // Lo que llega de la nube se guarda aquí y se vuelve a cargar todo (ELO, XP… se recalculan solos).
          await local.repos.players.saveAll(r.data.players);
          await local.repos.matches.saveAll(r.data.matches);
          await local.repos.tournaments.saveAll(r.data.tournaments);
          await local.repos.preferences.save(r.data.prefs);
          await local.refresh();
        }
        if (version !== generation.current) return;
        writeState(local.store, userId, r.state);
        setLastSync(Date.now());
        setError(null);
        setStatus('ok');
      } catch (err) {
        if (version !== generation.current) return;
        const msg = err instanceof Error ? err.message : 'No se pudo sincronizar';
        if (err instanceof CloudError && (err.code === '401' || err.code === 'session_expired')) {
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
  }, [conn, keep, currentSession]);

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
      ++generation.current;
      refreshing.current = null;
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
        ++generation.current;
        refreshing.current = null;
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
    ++generation.current;
    refreshing.current = null;
    keep(null);
    setStatus('off');
    setLastSync(null);
    setError(null);
    // Borrar primero aquí: una petición de renovación que llegue tarde no puede reabrir la cuenta.
    if (conn && s) await cloudSignOut(conn, s);
  }, [conn, keep]);

  // ¿Se abrió la app desde el QR de una placa? (se quita de la dirección para que no vuelva a salir al recargar)
  const [pendingBoard, setPendingBoard] = useState<string | null>(() => {
    try {
      const code = boardCodeFrom(window.location.hash) ?? boardCodeFrom(window.location.search);
      if (code) window.history.replaceState(null, '', window.location.pathname);
      return code;
    } catch {
      return null;
    }
  });
  const linkBoard = useCallback(
    async (code: string) => {
      if (!conn) throw new CloudError('La nube no está configurada');
      if (!sessionRef.current) throw new CloudError('Entra primero con tu cuenta');
      const s = await currentSession();
      await linkBoardCall(conn, s, code);
    },
    [conn, currentSession],
  );

  return {
    available: !!conn,
    session,
    rememberSession,
    setRememberSession,
    status: session ? status : 'off',
    lastSync,
    error,
    signIn,
    signUp,
    recover: doRecover,
    signOut,
    syncNow: runSync,
    pendingBoard,
    setPendingBoard,
    linkBoard,
  };
}
