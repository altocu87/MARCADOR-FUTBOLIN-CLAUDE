/**
 * Cuenta del dueño de la mesa y sincronización con la nube mientras la app está abierta.
 * Todo se guarda en este aparato al momento; con la nube solo se habla en reposo (ver idle.ts): los cambios
 * marcan «pendiente» y se suben al quedar la app quieta; en reposo también se mira la nube cada 10 min.
 * Un toque o empezar un partido cancela la sincronización automática. Inmediato, siempre: entrar en la cuenta,
 * «Sincronizar ahora» y vincular la placa.
 * Sin Supabase configurado, en modo prueba o sin cuenta, no hace nada (la app sigue en local como siempre).
 */
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { IdleMonitor } from './idle';
import { useAutoSync } from './useIdle';
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
  mergeAfterSync,
  syncOnce,
  writeSession,
  writeState,
  type MesaData,
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
  /** Vigilante de reposo de la app. */
  idle: IdleMonitor;
}

/** Lo que hay guardado ahora mismo en este aparato (no lo que se ve en pantalla, que puede ir un paso por detrás). */
async function readLocal(repos: Repositories): Promise<MesaData> {
  const [players, matches, tournaments, prefs] = await Promise.all([
    repos.players.list(),
    repos.matches.list(),
    repos.tournaments.list(),
    repos.preferences.load(),
  ]);
  return { players, matches, tournaments, prefs };
}

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
  const running = useRef<Promise<boolean | null> | null>(null);
  const queued = useRef<Promise<boolean | null> | null>(null);
  const lastSyncRef = useRef<number | null>(null);

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
      const current = sessionRef.current;
      // readSession crea un objeto nuevo incluso si los tokens no cambian: no se publica otra vez si son iguales.
      if (next.accessToken !== current.accessToken || next.refreshToken !== current.refreshToken ||
          next.expiresAt !== current.expiresAt || next.userId !== current.userId || next.email !== current.email) keep(next);
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

  /**
   * Una sincronización. Con `signal` es automática: se cancela al tocar o al empezar a jugar, y lo bajado solo se
   * guarda si no se ha cancelado. Sin él la ha pedido el usuario y llega hasta el final.
   * Devuelve true si fue bien, false si falló y null si no se hizo (o se canceló).
   */
  const syncJob = useCallback(async (signal?: AbortSignal): Promise<boolean | null> => {
    const cur = latest.current;
    if (!conn || !cur.enabled || !cur.loaded || !sessionRef.current) return null;
    const version = generation.current;
    const { repos, store } = cur;
    const aborted = () => new CloudError('Sincronización aplazada', 'aborted');
    setStatus('syncing');
    try {
      let s = sessionRef.current;
      const api = restApi(conn, async () => {
        s = await currentSession();
        return s.accessToken;
      }, undefined, { signal });
      const userId = s.userId;
      const start = await readLocal(repos);
      const r = await syncOnce(api, start, readState(store, userId), { signal });
      if (version !== generation.current) return null;
      if (signal?.aborted) throw aborted();
      if (r.data) {
        // Se vuelve a leer lo de aquí justo antes de guardar: lo guardado mientras tanto (un partido recién
        // terminado…) se conserva y se sube en la siguiente. Luego se vuelve a cargar todo (ELO, XP… se recalculan solos).
        const merged = mergeAfterSync(start, r.data, await readLocal(repos));
        if (version !== generation.current) return null;
        if (signal?.aborted) throw aborted();
        await repos.players.saveAll(merged.players);
        await repos.matches.saveAll(merged.matches);
        await repos.tournaments.saveAll(merged.tournaments);
        await repos.preferences.save(merged.prefs);
        await latest.current.refresh();
      }
      if (version !== generation.current) return null;
      writeState(store, userId, r.state);
      lastSyncRef.current = Date.now();
      setLastSync(lastSyncRef.current);
      setError(null);
      setStatus('ok');
      return true;
    } catch (err) {
      if (version !== generation.current) return null;
      if (err instanceof CloudError && err.code === 'aborted') {
        // Aplazada (se ha tocado la pantalla o se juega): se repite en el próximo reposo, sin avisar de nada.
        setStatus(lastSyncRef.current ? 'ok' : 'idle');
        return null;
      }
      const msg = err instanceof Error ? err.message : 'No se pudo sincronizar';
      if (err instanceof CloudError && (err.code === '401' || err.code === 'session_expired')) {
        keep(null);  // la sesión ya no vale: hay que volver a entrar
        setStatus('off');
        setError('La sesión ha caducado: vuelve a entrar');
        return false;
      }
      setError(err instanceof CloudError && err.code === 'offline' ? 'Sin conexión: se sincronizará al volver' : msg);
      setStatus('error');
      return false;
    }
  }, [conn, keep, currentSession]);

  // Una sola a la vez. Si el usuario pide otra mientras hay una en marcha, se hace al acabar esa; si es automática, se deja.
  const runSync = useCallback((signal?: AbortSignal): Promise<boolean | null> => {
    if (running.current) {
      if (signal) return Promise.resolve(null);
      queued.current ??= running.current.then(() => {
        queued.current = null;
        return runSyncRef.current();
      });
      return queued.current;
    }
    const job = syncJob(signal).finally(() => {
      if (running.current === job) running.current = null;
    });
    running.current = job;
    return job;
  }, [syncJob]);
  const runSyncRef = useRef(runSync);
  runSyncRef.current = runSync;

  // Automática, solo en reposo (idle.ts): al quedar la app quieta si hay algo pendiente y cada 10 min a mirar la nube.
  const { players, matches, tournaments, prefs, loaded, enabled } = d;
  const userId = session?.userId ?? null;
  const auto = useAutoSync(d.idle, !!conn && enabled && loaded && !!userId, (signal) => runSync(signal));

  // Cada cambio de los datos de la mesa (y al abrir o entrar) solo deja la subida pendiente.
  useEffect(() => {
    if (!userId || !loaded || !enabled) return;
    auto.markPending();
  }, [players, matches, tournaments, prefs, loaded, enabled, userId, auto]);

  // Pedida por el usuario: en el momento y sin cancelarse por tocar la pantalla.
  const syncNow = useCallback(async (): Promise<void> => {
    const mark = auto.begin();
    const ok = await runSync();
    auto.end(mark, ok === true);
  }, [auto, runSync]);

  const signIn = useCallback(
    async (email: string, password: string) => {
      if (!conn) throw new CloudError('La nube no está configurada');
      const s = await cloudSignIn(conn, email, password);
      ++generation.current;
      refreshing.current = null;
      keep(s);
      setStatus('idle');
      setError(null);
      void syncNow();  // al entrar, en el momento (lo ha pedido el usuario)
    },
    [conn, keep, syncNow],
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
        void syncNow();
      }
      return r.needsConfirm;
    },
    [conn, keep, syncNow],
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
    lastSyncRef.current = null;
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
    syncNow,
    pendingBoard,
    setPendingBoard,
    linkBoard,
  };
}
