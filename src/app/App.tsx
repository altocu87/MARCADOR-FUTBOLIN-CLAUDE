/** Navegación principal: cada ruta pinta su pantalla dentro del lienzo 800 × 480. */
import { useEffect, useRef } from 'react';
import { attachKeyboardAdapter, exposeDevInputs } from '../inputs/inputBus';
import { SleepOverlay } from '../ui/components/SleepOverlay';
import { Stage } from '../ui/layout/Stage';
import { wakeLock } from '../services/system/device';
import { defaultWsUrl, hardwareHub, servedFromBoard } from '../inputs/hardware/hub';
import { linkSupport } from '../inputs/hardware/links';
import { HomeScreen } from '../ui/screens/HomeScreen';
import { MatchScreen } from '../ui/screens/MatchScreen';
import { PrematchScreen } from '../ui/screens/PrematchScreen';
import { ProfileScreen } from '../ui/screens/ProfileScreen';
import { RankingScreen } from '../ui/screens/RankingScreen';
import { SelectPlayersScreen } from '../ui/screens/SelectPlayersScreen';
import { SettingsScreen } from '../ui/screens/SettingsScreen';
import { SetupScreen } from '../ui/screens/SetupScreen';
import { ChallengesScreen } from '../ui/screens/ChallengesScreen';
import { MatchDetailScreen } from '../ui/screens/SimpleScreens';
import {
  TournamentDetailScreen,
  TournamentListScreen,
  TournamentHonoursScreen,
  TournamentNewScreen,
  TournamentTemplateScreen,
} from '../ui/screens/TournamentScreens';
import { SummaryScreen } from '../ui/screens/SummaryScreen';
import { AppProvider, useApp } from './AppContext';

// Clave estable por navegación: cada nueva ruta de partido crea un partido nuevo
// (revancha incluida) sin remontarlo en re-renderizados posteriores.
const routeKeys = new WeakMap<object, number>();
let nextKey = 0;
function routeKey(route: object): number {
  let k = routeKeys.get(route);
  if (k === undefined) {
    k = (nextKey += 1);
    routeKeys.set(route, k);
  }
  return k;
}

function Router() {
  const { route, loaded, prefs, toastMessage, demoMode } = useApp();

  useEffect(() => {
    exposeDevInputs();
    return attachKeyboardAdapter();
  }, []);

  // Placas: avisar fuera de partido y reconectar al abrir si está configurado.
  useEffect(() => {
    if (route.name !== 'match') hardwareHub.notifyIdle();
  }, [route.name]);
  const autoConnected = useRef(false);
  useEffect(() => {
    if (!loaded || autoConnected.current || !(prefs.hardware.autoConnect || servedFromBoard())) return;
    autoConnected.current = true;
    void hardwareHub.connect('websocket', { url: prefs.hardware.wsUrl || defaultWsUrl() });
    if (linkSupport.serial()) void hardwareHub.connect('serial', { reuseGranted: true });
  }, [loaded, prefs.hardware]);

  // Pantalla siempre encendida (si el navegador lo permite).
  useEffect(() => {
    if (!prefs.keepAwake) {
      void wakeLock.disable();
      return;
    }
    void wakeLock.enable();
    const onVis = () => void wakeLock.onVisible();
    document.addEventListener('visibilitychange', onVis);
    return () => document.removeEventListener('visibilitychange', onVis);
  }, [prefs.keepAwake]);

  if (!loaded) return <div className="screen" />;

  let screen;
  switch (route.name) {
    case 'home':
      screen = <HomeScreen />;
      break;
    case 'setup':
      screen = <SetupScreen key={route.mode} mode={route.mode} initial={route.config} />;
      break;
    case 'select':
      screen = <SelectPlayersScreen config={route.config} initial={route.participants} />;
      break;
    case 'prematch':
      screen = <PrematchScreen config={route.config} participants={route.participants} extras={route.extras} />;
      break;
    case 'match':
      screen = <MatchScreen key={routeKey(route)} config={route.config} participants={route.participants} resume={route.resume} extras={route.extras} />;
      break;
    case 'summary':
      screen = <SummaryScreen match={route.match} save={route.save} live={route.live} extras={route.extras} />;
      break;
    case 'ranking':
      screen = <RankingScreen tab={route.tab} />;
      break;
    case 'matchDetail':
      screen = <MatchDetailScreen matchId={route.matchId} fromTournament={route.fromTournament} />;
      break;
    case 'profile':
      screen = <ProfileScreen key={route.playerId} playerId={route.playerId} />;
      break;
    case 'tournament':
      screen = <TournamentListScreen tab={route.tab} />;
      break;
    case 'tournamentNew':
      screen = <TournamentNewScreen key={`${route.templateId ?? ''}|${route.step ?? ''}`} templateId={route.templateId} initialSelected={route.selected} initialStep={route.step} />;
      break;
    case 'tournamentHonours':
      screen = <TournamentHonoursScreen key={route.key} competitionKey={route.key} />;
      break;
    case 'tournamentTemplate':
      screen = <TournamentTemplateScreen key={`${route.templateId ?? ''}|${route.baseId ?? ''}`} templateId={route.templateId} baseId={route.baseId} selected={route.selected} />;
      break;
    case 'tournamentDetail':
      screen = <TournamentDetailScreen key={route.id} id={route.id} view={route.view} from={route.from} ceremony={route.ceremony} />;
      break;
    case 'challenges':
      screen = <ChallengesScreen />;
      break;
    case 'settings':
      screen = <SettingsScreen tab={route.tab} />;
      break;
  }

  return (
    <>
      {screen}
      {/* Reposo solo fuera de una partida activa. */}
      <SleepOverlay minutes={prefs.sleepMinutes} enabled={route.name !== 'match'} />
      {/* En las pantallas del partido ya se ve la etiqueta propia de modo prueba. */}
      {demoMode && !['setup', 'select', 'prematch', 'match', 'summary'].includes(route.name) && (
        <div className="demo-badge" role="note" title="Modo prueba: se borra al desactivarlo en Ajustes → General">
          MODO PRUEBA
        </div>
      )}
      {toastMessage && (
        <div className="toast" role="status">
          {toastMessage}
        </div>
      )}
    </>
  );
}

export function App() {
  return (
    <AppProvider>
      <Stage>{() => <Router />}</Stage>
    </AppProvider>
  );
}
