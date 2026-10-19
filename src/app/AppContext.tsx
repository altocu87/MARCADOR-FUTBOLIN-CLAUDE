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
  type StoredSound,
  type Tournament,
} from '../services/persistence';
import { browserSoundRepository, memorySoundRepository, type SoundRepository } from '../services/sound/library';
import { soundPlayer } from '../services/sound/soundPlayer';
import { hardwareHub } from '../inputs/hardware/hub';
import { momentLines } from '../inputs/hardware/boardSounds';
import { boardPlayer, photoForBoard } from '../inputs/hardware/boardPlayers';

// Los sonidos guardados en la placa se le piden a ella (si no puede, suena el de siempre en la app).
soundPlayer.board = { play: (s, queue) => hardwareHub.playBoardSound(s.id, queue) };
import { generateDemoData } from '../services/demo';
import { mergePlayers, sortPlayers } from '../services/players';
import {
  DEFAULT_RULES,
  applyGameRules,
  computeProgression,
  displayTitle,
  eloSettings,
  fetchRemoteRules,
  readCachedRules,
  writeCachedRules,
  type GameRules,
  type ProgressionSnapshot,
  type PublishedRules,
} from '../services/progression';
import { applyPanelPrefs, markOverrides, resetToPanel } from '../services/panelPrefs';
import type { OverridableKey } from '../services/persistence/types';
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
import { useProfileSync } from './profileSync';
import { useCloudSync, type CloudSync } from './cloudSync';
import { credentialsFor, profileUrl } from '../services/profiles';
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
  /** Pasa todo lo de un jugador a otro (el mismo, apuntado dos veces) y borra el primero. */
  mergePlayers(fromId: string, intoId: string): Promise<{ movedMatches: number; movedTournaments: number }>;
  savePrefs(prefs: Preferences): Promise<void>;
  /** Sonidos propios de la mesa (en este aparato, aparte de los demás datos). */
  soundRepo: SoundRepository;
  sounds: StoredSound[];
  reloadSounds(): Promise<void>;
  /** Esos campos vuelven a seguir lo del panel de administración. */
  followPanel(keys: OverridableKey[]): Promise<void>;
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
  /** Reglas del juego en uso (ELO, rangos, XP, niveles) y cuándo se publicaron en el panel (null: las de la app). */
  rules: GameRules;
  rulesUpdatedAt: number | null;
  /** Quién tiene cada pieza del catálogo. */
  rewards: RewardState;
  /** Días (YYYY-MM-DD) en que se ha abierto la app aquí. */
  visitDays: string[];
  /**
   * Enlace del perfil público de un jugador (para su QR) y si se publica de verdad: no en modo prueba ni sin
   * conexión con el servidor configurada. Al pedirlo se sube en el momento si hace falta.
   */
  profileLink(playerId: string): { url: string; online: boolean; publish(): Promise<boolean> };
  /** Cuenta del dueño de la mesa y sincronización con la nube (Ajustes → Cuenta). */
  cloud: CloudSync;
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
  const [soundRepo] = useState<SoundRepository>(() => (injected ? memorySoundRepository() : browserSoundRepository()));
  const [sounds, setSounds] = useState<StoredSound[]>([]);
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

  // Reglas del juego: las publicadas en el panel (copia guardada aquí y, si hay conexión, se piden otra vez).
  const [published, setPublished] = useState<PublishedRules | null>(() => {
    try {
      return readCachedRules(store);
    } catch {
      return null;
    }
  });
  useEffect(() => {
    if (injected) return;
    const url = (import.meta.env.VITE_SUPABASE_URL as string | undefined)?.trim();
    const key = (import.meta.env.VITE_SUPABASE_ANON_KEY as string | undefined)?.trim();
    if (!url || !key) return;
    let alive = true;
    fetchRemoteRules(url, key)
      .then((remote) => {
        if (!alive || !remote) return;
        writeCachedRules(store, remote);
        setPublished((cur) => (cur && cur.updatedAt === remote.updatedAt ? cur : remote));
      })
      .catch(() => {
        /* sin conexión o sin reglas publicadas: se quedan las guardadas */
      });
    return () => {
      alive = false;
    };
  }, [injected, store]);
  const rules = published?.rules ?? DEFAULT_RULES;
  // Se aplican antes de calcular nada: tablas de XP, rangos y niveles. Lo del ELO va en los ajustes de progresión.
  useMemo(() => applyGameRules(rules), [rules]);
  // Lo que deciden las reglas del panel manda sobre lo guardado aquí: ELO, temporadas y retos. La progresión, siempre activa.
  const effectivePrefs = useMemo<Preferences>(
    () => ({
      ...applyPanelPrefs(prefs, rules),
      progression: { ...prefs.progression, ...eloSettings(rules), enabled: true },
      seasonLength: rules.seasonLength,
      challenges: rules.challenges,
    }),
    [prefs, rules],
  );

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

  const reloadSounds = useCallback(async () => {
    try {
      setSounds(await soundRepo.list());
    } catch {
      setSounds([]);
    }
  }, [soundRepo]);

  const refresh = useCallback(async () => {
    void reloadSounds();
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
  }, [repos, reloadSounds]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const ep = effectivePrefs;
  useEffect(() => {
    sound.configure(ep.volume, ep.muted, ep.goalSound);
    voice.configure(ep.voice && !ep.muted);
  }, [ep.volume, ep.muted, ep.goalSound, ep.voice]);
  // Los sonidos propios se dejan cargados en memoria: al marcar no se lee nada.
  useEffect(() => soundPlayer.setLibrary(sounds), [sounds]);
  useEffect(() => soundPlayer.setChoices(ep.soundEvents), [ep.soundEvents]);
  // La placa con altavoz juega también sola: se le dice qué suena en cada momento y a qué volumen.
  useEffect(
    () => hardwareHub.setSoundSetup([...momentLines(ep.soundEvents, sounds), `VOLUMEN ${ep.muted ? 0 : Math.round(ep.volume * 100)}`]),
    [ep.soundEvents, sounds, ep.volume, ep.muted],
  );

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

  // Lo que se guarda desde la mesa y difiere de lo que se veía pasa a ser de esta mesa (deja de seguir al panel).
  const savePrefs = useCallback(
    async (next: Preferences) => {
      const stored = markOverrides(effectivePrefs, next);
      await repos.preferences.save(stored);
      setPrefs(stored);
    },
    [repos, effectivePrefs],
  );

  const followPanel = useCallback(
    async (keys: OverridableKey[]) => {
      const stored = resetToPanel(prefs, keys);
      await repos.preferences.save(stored);
      setPrefs(stored);
    },
    [repos, prefs],
  );

  const progression = useMemo(
    () =>
      effectivePrefs.progression.enabled
        ? computeProgression(
            players.map((p) => p.id),
            matches,
            effectivePrefs.progression,
            { tournaments, challenges: effectivePrefs.challenges },
          )
        : null,
    [players, matches, tournaments, effectivePrefs.progression, effectivePrefs.challenges],
  );

  // Pantalla de 7": la lista de jugadores activos con su «Nv · ELO», su título y su foto ya preparada para la placa.
  // Las fotos se convierten una vez (por jugador y versión) y se guardan aquí mientras la app esté abierta.
  const [boardPhotos] = useState(() => new Map<string, Promise<Uint8Array | null>>());
  useEffect(() => {
    let cancelled = false;
    void (async () => {
      const active = sortPlayers(players.filter((p) => p.active));  // en el mismo orden que la lista de la app
      const list = await Promise.all(
        active.map(async (p) => {
          const prog = progression?.players.get(p.id);
          const meta = prog ? `Nv ${prog.level}${prog.rankedPlayed ? ` · ELO ${prog.elo}` : ''}` : '';
          let photo: Promise<Uint8Array | null> = Promise.resolve(null);
          if (p.photo) {
            const key = `${p.id}:${p.updatedAt}`;
            if (!boardPhotos.has(key)) boardPhotos.set(key, photoForBoard(p.photo));
            photo = boardPhotos.get(key)!;
          }
          return boardPlayer(p, meta, displayTitle(prog, p.titleId), await photo);
        }),
      );
      if (!cancelled) hardwareHub.setPlayers(list);
    })();
    return () => {
      cancelled = true;
    };
  }, [players, progression, boardPhotos]);

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

  const cloud = useCloudSync({ enabled: !injected && !demoMode, store, repos, players, matches, tournaments, prefs, loaded, refresh });
  const sync = useProfileSync({ enabled: !injected && !demoMode, store, players, matches, tournaments, progression, catalog, rewards });
  const profileLink = useCallback(
    (playerId: string) => {
      const c = credentialsFor(store, playerId);
      return { url: profileUrl(c.token), online: !!sync.conn && !demoMode, publish: () => sync.publishNow(playerId) };
    },
    [store, sync, demoMode],
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
    prefs: effectivePrefs,
    soundRepo,
    sounds,
    reloadSounds,
    progression,
    refresh,
    savePlayer,
    saveMatch,
    deleteMatch,
    saveTournament,
    discardTournament,
    mergePlayers: mergePlayersCb,
    savePrefs,
    followPanel,
    demoMode,
    setDemoMode,
    route,
    navigate,
    toast,
    toastMessage,
    catalog,
    rules,
    rulesUpdatedAt: published?.updatedAt ?? null,
    rewards,
    visitDays,
    profileLink,
    cloud,
  };
  return <Ctx.Provider value={value}>{children}</Ctx.Provider>;
}
