/**
 * Composición de servicios de la aplicación: repositorios locales, datos cargados,
 * progresión derivada y navegación. La interfaz consume este contexto.
 */
import { mergeClubPlayer } from '../services/clubs';
import { createContext, useCallback, useContext, useEffect, useMemo, useState, type ReactNode } from 'react';
import {
  DEFAULT_PREFERENCES,
  DEMO_FLAG_KEY,
  DEMO_NAMESPACE,
  browserStore,
  createLocalRepositories,
  type Player,
  type Preferences,
  type Repositories,
  type StoredMatch,
  type Tournament,
} from '../services/persistence';
import { generateDemoData } from '../services/demo';
import { mergePlayers } from '../services/players';
import { computeProgression, type ProgressionSnapshot } from '../services/progression';
import { sound } from '../services/sound/sound';
import { voice } from '../services/sound/voice';
import {
  BUNDLED_CATALOG,
  computeRewards,
  dayKey,
  fetchRemoteCatalog,
  mergeCatalogs,
  readCachedCatalog,
  writeCachedCatalog,
  type Catalog,
  type RewardState,
} from '../services/events';
import { applyCatalogIcons } from './catalogIcons';
import type { Route } from './routes';

/** Días en que se ha abierto la app en este aparato (para los premios por entrar en un evento). */
const VISITS_KEY = 'mfv3:visitas';
const MAX_VISITS = 400;

interface AppContextValue {
  repos: Repositories;
  persistent: boolean;
  loaded: boolean;
  players: Player[];
  matches: StoredMatch[];
  tournaments: Tournament[];
  prefs: Preferences;
  /** null cuando la progresión está desactivada («Clasificación pendiente»). */
  progression: ProgressionSnapshot | null;
  refresh(): Promise<void>;
  savePlayer(player: Player): Promise<void>;
  saveMatch(match: StoredMatch): Promise<void>;
  deleteMatch(id: string): Promise<void>;
  saveTournament(tournament: Tournament): Promise<void>;
  /** Descarta un torneo: se borran el torneo y todos sus partidos (no cuentan para nada). */
  discardTournament(id: string): Promise<void>;
  /** Pasa todo lo de un jugador (p. ej. un invitado) a otro y borra el primero. */
  mergePlayers(fromId: string, intoId: string): Promise<{ movedMatches: number; movedTournaments: number }>;
  savePrefs(prefs: Preferences): Promise<void>;
  /** Modo prueba: la app trabaja sobre datos ficticios y guarda ahí lo que se juegue. */
  demoMode: boolean;
  /** Activa (carga datos ficticios) o desactiva (borra todo lo de prueba y vuelve a los reales). */
  setDemoMode(on: boolean): Promise<void>;
  route: Route;
  navigate(route: Route): void;
  toast(message: string): void;
  toastMessage: string | null;
  /** Catálogo de eventos (lo que trae la app + lo descargado del panel). */
  catalog: Catalog;
  /** Quién tiene cada pieza del catálogo. */
  rewards: RewardState;
  /** Días (YYYY-MM-DD) en que se ha abierto la app aquí. */
  visitDays: string[];
}

const Ctx = createContext<AppContextValue | null>(null);

export function useApp(): AppContextValue {
  const v = useContext(Ctx);
  if (!v) throw new Error('useApp fuera de AppProvider');
  return v;
}

export function AppProvider({ children, repos: injected }: { children: ReactNode; repos?: Repositories }) {
  const [{ store, persistent }] = useState(() => browserStore());
  const readDemoFlag = () => {
    try {
      return !injected && store.getItem(DEMO_FLAG_KEY) === '1';
    } catch {
      return false;
    }
  };
  const [demoMode, setDemoFlag] = useState(readDemoFlag);
  // Modo prueba: jugadores, partidos y torneos en su propio espacio (los reales quedan intactos).
  // Las preferencias son siempre las mismas en ambos modos.
  const repos = useMemo<Repositories>(() => {
    if (injected) return injected;
    const real = createLocalRepositories(store);
    return demoMode ? { ...createLocalRepositories(store, DEMO_NAMESPACE), preferences: real.preferences } : real;
  }, [injected, store, demoMode]);
  const [loaded, setLoaded] = useState(false);
  const [players, setPlayers] = useState<Player[]>([]);
  const [matches, setMatches] = useState<StoredMatch[]>([]);
  const [tournaments, setTournaments] = useState<Tournament[]>([]);
  const [prefs, setPrefs] = useState<Preferences>(DEFAULT_PREFERENCES);
  const [route, setRoute] = useState<Route>({ name: 'home' });
  const [toastMessage, setToastMessage] = useState<string | null>(null);

  // Catálogo de eventos: lo último descargado (guardado aquí) y, si hay conexión, se pide otra vez.
  const [catalog, setCatalog] = useState<Catalog>(() => {
    let cached: Catalog | null = null;
    try {
      cached = readCachedCatalog(store);
    } catch {
      /* sin almacenamiento */
    }
    const c = mergeCatalogs(BUNDLED_CATALOG, cached);
    applyCatalogIcons(c);
    return c;
  });
  useEffect(() => {
    if (injected) return;
    const url = (import.meta.env.VITE_SUPABASE_URL as string | undefined)?.trim();
    const key = (import.meta.env.VITE_SUPABASE_ANON_KEY as string | undefined)?.trim();
    if (!url || !key) return;
    let alive = true;
    fetchRemoteCatalog(url, key)
      .then((remote) => {
        if (!alive) return;
        try {
          writeCachedCatalog(store, remote);
        } catch {
          /* sin almacenamiento */
        }
        const c = mergeCatalogs(BUNDLED_CATALOG, remote);
        applyCatalogIcons(c);
        setCatalog(c);
      })
      .catch(() => {
        /* sin conexión: se queda lo guardado */
      });
    return () => {
      alive = false;
    };
  }, [injected, store]);

  // Apunta el día de hoy (una vez por día) entre los días en que se ha abierto la app.
  const [visitDays] = useState<string[]>(() => {
    let list: string[] = [];
    try {
      const raw = store.getItem(VISITS_KEY);
      list = raw ? (JSON.parse(raw) as string[]).filter((d) => typeof d === 'string') : [];
    } catch {
      list = [];
    }
    const today = dayKey(Date.now());
    if (!list.includes(today)) {
      list = [...list, today].slice(-MAX_VISITS);
      try {
        store.setItem(VISITS_KEY, JSON.stringify(list));
      } catch {
        /* sin almacenamiento */
      }
    }
    return list;
  });

  const refresh = useCallback(async () => {
    const [p, m, pr, t] = await Promise.all([
      repos.players.list(),
      repos.matches.list(),
      repos.preferences.load(),
      repos.tournaments.list(),
    ]);
    setPlayers(p);
    setMatches(m);
    setTournaments(t);
    setPrefs(pr);
    setLoaded(true);
  }, [repos]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  useEffect(() => {
    sound.configure(prefs.volume, prefs.muted, prefs.goalSound);
    voice.configure(prefs.voice && !prefs.muted);
  }, [prefs.volume, prefs.muted, prefs.goalSound, prefs.voice]);

  const savePlayer = useCallback(
    async (player: Player) => {
      await repos.players.save(player);
      setPlayers(await repos.players.list());
    },
    [repos],
  );

  const saveMatch = useCallback(
    async (match: StoredMatch) => {
      await repos.matches.save(match);
      setMatches(await repos.matches.list());
    },
    [repos],
  );

  // Borrar un partido: estadísticas, ELO, XP, logros y récords se recalculan solos.
  const deleteMatch = useCallback(
    async (id: string) => {
      const list = await repos.matches.list();
      await repos.matches.saveAll(list.filter((m) => m.id !== id));
      setMatches(await repos.matches.list());
    },
    [repos],
  );

  const saveTournament = useCallback(
    async (t: Tournament) => {
      await repos.tournaments.save(t);
      setTournaments(await repos.tournaments.list());
    },
    [repos],
  );

  const discardTournament = useCallback(
    async (id: string) => {
      const [m, t, active] = await Promise.all([repos.matches.list(), repos.tournaments.list(), repos.activeMatch.load()]);
      await repos.matches.saveAll(m.filter((x) => x.tournament?.id !== id));
      await repos.tournaments.saveAll(t.filter((x) => x.id !== id));
      // Si había un partido suyo a medias, también se va.
      if (active?.extras?.tournament?.id === id) await repos.activeMatch.clear();
      await refresh();
    },
    [repos, refresh],
  );

  const mergePlayersCb = useCallback(
    async (fromId: string, intoId: string) => {
      const [p, m, t] = await Promise.all([repos.players.list(), repos.matches.list(), repos.tournaments.list()]);
      const r = mergePlayers(fromId, intoId, p, m, t, Date.now());
      await repos.matches.saveAll(r.matches);
      await repos.tournaments.saveAll(r.tournaments);
      await repos.players.saveAll(r.players);
      // Sus equipos guardados pasan al jugador que se queda.
      const pr = await repos.preferences.load();
      const clubs = mergeClubPlayer(pr.clubs, fromId, intoId);
      if (clubs !== pr.clubs) {
        await repos.preferences.save({ ...pr, clubs });
        setPrefs({ ...pr, clubs });
      }
      await refresh();
      return { movedMatches: r.movedMatches, movedTournaments: r.movedTournaments };
    },
    [repos, refresh],
  );

  const savePrefs = useCallback(
    async (next: Preferences) => {
      await repos.preferences.save(next);
      setPrefs(next);
    },
    [repos],
  );

  const progression = useMemo(
    () =>
      prefs.progression.enabled
        ? computeProgression(
            players.map((p) => p.id),
            matches,
            prefs.progression,
            { tournaments, challenges: prefs.challenges },
          )
        : null,
    [players, matches, tournaments, prefs.progression, prefs.challenges],
  );

  const rewards = useMemo(
    () =>
      computeRewards(catalog, {
        matches,
        tournaments,
        achievements: progression ? new Map([...progression.players].map(([id, p]) => [id, p.achievements])) : undefined,
        visitDays,
      }),
    [catalog, matches, tournaments, progression, visitDays],
  );

  const toast = useCallback((message: string) => {
    setToastMessage(message);
    window.setTimeout(() => setToastMessage((m) => (m === message ? null : m)), 2600);
  }, []);

  const navigate = useCallback((next: Route) => setRoute(next), []);

  const setDemoMode = useCallback(
    async (on: boolean) => {
      const demoRepos = createLocalRepositories(store, DEMO_NAMESPACE);
      if (on) {
        const data = generateDemoData();
        await demoRepos.wipe();
        await demoRepos.players.saveAll(data.players);
        await demoRepos.matches.saveAll(data.matches);
        await demoRepos.tournaments.saveAll(data.tournaments);
        store.setItem(DEMO_FLAG_KEY, '1');
      } else {
        await demoRepos.wipe();
        store.removeItem(DEMO_FLAG_KEY);
      }
      setLoaded(false);
      setDemoFlag(on);
      setRoute({ name: 'home' });
    },
    [store],
  );

  const value: AppContextValue = {
    repos,
    persistent: injected ? true : persistent,
    loaded,
    players,
    matches,
    tournaments,
    prefs,
    progression,
    refresh,
    savePlayer,
    saveMatch,
    deleteMatch,
    saveTournament,
    discardTournament,
    mergePlayers: mergePlayersCb,
    savePrefs,
    demoMode,
    setDemoMode,
    route,
    navigate,
    toast,
    toastMessage,
    catalog,
    rewards,
    visitDays,
  };
  return <Ctx.Provider value={value}>{children}</Ctx.Provider>;
}
