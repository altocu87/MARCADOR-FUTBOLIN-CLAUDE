import { useLayoutEffect, useMemo, useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import { MAX_PER_TEAM, validateParticipants, type MatchConfig, type ParticipantRef, type Slot, type Team } from '../../match-engine';
import type { Player } from '../../services/persistence';
import { balancedTeams, initials, randomTeams, sortPlayers } from '../../services/players';
import { displayTitle } from '../../services/progression';
import { AssetImage } from '../components/assets';
import { MODE_LABEL, ScreenFrame, TestModeBadge } from '../components/common';
import { PlayerEditor } from '../components/PlayerEditor';

const TEAM_NAME: Record<Team, string> = { white: 'BLANCO', blue: 'AZUL' };
const OTHER: Record<Team, Team> = { white: 'blue', blue: 'white' };

type Teams = Record<Team, string[]>;

function fromParticipants(parts?: ParticipantRef[]): Teams {
  const out: Teams = { white: [], blue: [] };
  for (const p of [...(parts ?? [])].sort((a, b) => a.slot - b.slot)) out[p.team].push(p.playerId);
  return out;
}

/**
 * Selección por turnos: primero elige el equipo BLANCO y, con «Siguiente», el AZUL.
 * Cada equipo elige de 1 a 4 jugadores, sin tener que ser los mismos (1 contra 2,
 * 2 contra 3…). Al elegir un jugador, su ficha viaja con suavidad desde la lista de
 * libres hasta la columna de su equipo (y cambia a formato horizontal); al tocarla
 * otra vez vuelve a la lista.
 */
export function SelectPlayersScreen({ config, initial }: { config: MatchConfig; initial?: ParticipantRef[] }) {
  const { players, navigate, progression, demoMode, toast } = useApp();
  const [teams, setTeams] = useState<Teams>(() => fromParticipants(initial));
  // Si se vuelve con los dos equipos hechos, se abre en el turno del azul (listo para continuar).
  const [step, setStep] = useState<Team>(() => (fromParticipants(initial).blue.length > 0 ? 'blue' : 'white'));
  const [creating, setCreating] = useState(false);

  const available = useMemo(() => sortPlayers(players.filter((p) => p.active)), [players]);
  const byId = useMemo(() => new Map(players.map((p) => [p.id, p])), [players]);

  // Animación de movimiento (técnica «FLIP»): antes de cambiar los equipos se apunta dónde
  // está cada ficha; después del cambio, cada ficha arranca desde su sitio anterior y se
  // desliza hasta el nuevo.
  const layoutRef = useRef<HTMLDivElement>(null);
  const before = useRef(new Map<string, DOMRect>());
  const updateTeams = (next: Teams) => {
    const m = new Map<string, DOMRect>();
    layoutRef.current?.querySelectorAll<HTMLElement>('[data-flip]').forEach((el) => m.set(el.dataset.flip!, el.getBoundingClientRect()));
    before.current = m;
    setTeams(next);
  };
  useLayoutEffect(() => {
    const prev = before.current;
    before.current = new Map();
    if (prev.size === 0 || window.matchMedia?.('(prefers-reduced-motion: reduce)').matches) return;
    layoutRef.current?.querySelectorAll<HTMLElement>('[data-flip]').forEach((el) => {
      const a = prev.get(el.dataset.flip!);
      if (!a) return;
      const b = el.getBoundingClientRect();
      if (!b.width || !b.height) return;
      // La pantalla puede estar escalada: se pasa de píxeles de pantalla a píxeles del diseño.
      const k = el.offsetWidth ? b.width / el.offsetWidth : 1;
      const dx = (a.left + a.width / 2 - (b.left + b.width / 2)) / k;
      const dy = (a.top + a.height / 2 - (b.top + b.height / 2)) / k;
      const changed = Math.abs(a.width - b.width) > 1 || Math.abs(a.height - b.height) > 1;
      if (Math.abs(dx) < 1 && Math.abs(dy) < 1 && !changed) return;
      const scale = Math.sqrt((a.width * a.height) / (b.width * b.height));
      el.style.zIndex = '5';
      el.animate(
        [
          { transform: `translate(${dx}px, ${dy}px) scale(${scale})`, opacity: changed ? 0.55 : 1 },
          { transform: 'none', opacity: 1 },
        ],
        { duration: changed ? 480 : 360, easing: 'cubic-bezier(0.2, 0.8, 0.2, 1)' },
      ).onfinish = () => {
        el.style.zIndex = '';
      };
    });
  }, [teams]);

  // Tocar un jugador libre: entra en el equipo que elige.
  const pick = (playerId: string) => {
    const current = teams[step];
    if (current.includes(playerId) || teams[OTHER[step]].includes(playerId)) return;
    if (current.length >= MAX_PER_TEAM) {
      toast(`El equipo ${TEAM_NAME[step]} ya tiene ${MAX_PER_TEAM} jugadores (máximo)`);
      return;
    }
    updateTeams({ ...teams, [step]: [...current, playerId] });
  };

  // Tocar una ficha de un equipo: vuelve a la lista de libres.
  const remove = (team: Team, playerId: string) => {
    updateTeams({ ...teams, [team]: teams[team].filter((id) => id !== playerId) });
  };

  const participants: ParticipantRef[] = (['white', 'blue'] as Team[]).flatMap((team) =>
    teams[team].map((id, i) => ({
      playerId: id,
      team,
      slot: (i + 1) as Slot,
      nameSnapshot: byId.get(id)?.name ?? '?',
    })),
  );
  const whiteReady = teams.white.length > 0;
  const valid = whiteReady && teams.blue.length > 0 && validateParticipants(participants).length === 0;
  const count = teams[step].length;
  const status =
    step === 'blue' && valid
      ? `${teams.white.length} contra ${teams.blue.length} listo`
      : count === 0
        ? `Equipo ${TEAM_NAME[step]}: elige de 1 a ${MAX_PER_TEAM} jugadores`
        : `Equipo ${TEAM_NAME[step]}: ${count} de ${MAX_PER_TEAM} jugadores`;
  // Equilibrar/Aleatorio reparten a partes iguales: solo con 4, 6 u 8 elegidos.
  const chosenCount = teams.white.length + teams.blue.length;
  const canSplit = step === 'blue' && valid && chosenCount >= 4 && chosenCount % 2 === 0;

  // Sorteo con todos los elegidos, a partes iguales.
  const applySplit = (kind: 'balanced' | 'random') => {
    const chosen = [...teams.white, ...teams.blue];
    const split =
      kind === 'balanced'
        ? balancedTeams(chosen, (id) => progression?.players.get(id)?.elo ?? 1200)
        : randomTeams(chosen);
    updateTeams({ white: split.white, blue: split.blue });
  };

  const go = () => {
    if (!valid) return;
    if (config.mode === 'ranked') navigate({ name: 'prematch', config, participants });
    else navigate({ name: 'match', config, participants });
  };

  const metaOf = (p: Player) => {
    const prog = progression?.players.get(p.id);
    return {
      meta: prog ? `Nv ${prog.level}${prog.rankedPlayed ? ` · ELO ${prog.elo}` : ''}` : '\u00a0',
      title: displayTitle(prog, p.titleId),
    };
  };
  const photoOf = (p: Player) =>
    p.photo ? <img src={p.photo} alt="" draggable={false} /> : <span className="pick-initials">{initials(p.name)}</span>;

  // Columna de un equipo: 4 plazas; las ocupadas muestran la ficha en horizontal.
  const teamColumn = (team: Team) => (
    <div
      className={`team-col team-col-${team} ${step === team ? 'is-active' : ''}`}
      onClick={() => setStep(team)}
      role="button"
      aria-pressed={step === team}
      aria-label={`Elegir jugadores del equipo ${TEAM_NAME[team]}`}
    >
      <div className="team-col-label">{TEAM_NAME[team]}</div>
      {Array.from({ length: MAX_PER_TEAM }, (_, i) => {
        const id = teams[team][i];
        const p = id ? byId.get(id) : undefined;
        if (!p) {
          return (
            <div key={`empty-${i}`} className="team-slot">
              <span className="team-slot-empty">{i === 0 ? 'Plaza 1' : `Plaza ${i + 1} · opcional`}</span>
            </div>
          );
        }
        const { meta, title } = metaOf(p);
        return (
          <button
            key={p.id}
            data-flip={p.id}
            className={`team-card team-card-${team}`}
            onClick={(e) => {
              e.stopPropagation();
              remove(team, p.id);
            }}
            aria-label={`${p.name}: devolver a la lista de jugadores`}
          >
            <span className="team-card-photo">{photoOf(p)}</span>
            <span className="team-card-info">
              <span className="team-card-name">{p.name}</span>
              <span className="pick-meta">{meta}</span>
              {title && <span className="pick-title">{title}</span>}
            </span>
          </button>
        );
      })}
    </div>
  );

  // Ficha de jugador libre: la imagen ocupa toda la tarjeta y debajo va su información.
  const playerCard = (p: Player) => {
    const { meta, title } = metaOf(p);
    return (
      <button key={p.id} data-flip={p.id} className="pick-card" onClick={() => pick(p.id)} aria-label={`Elegir a ${p.name}`}>
        <span className="pick-photo">{photoOf(p)}</span>
        <span className="pick-info">
          <span className="pick-name">{p.name}</span>
          <span className="pick-meta">{meta}</span>
          {title && <span className="pick-title">{title}</span>}
        </span>
      </button>
    );
  };

  const free = available.filter((p) => !teams.white.includes(p.id) && !teams.blue.includes(p.id));

  return (
    <ScreenFrame
      title={`Equipo ${TEAM_NAME[step]}`}
      subtitle={MODE_LABEL[config.mode]}
      className={`select-screen select-${step}`}
      background={
        <>
          <AssetImage name="fondo-equipo-blanco" className={`select-bg ${step === 'white' ? 'is-on' : ''}`} fallback={null} />
          <AssetImage name="fondo-equipo-azul" className={`select-bg ${step === 'blue' ? 'is-on' : ''}`} fallback={null} />
        </>
      }
      onBack={() => (step === 'blue' ? setStep('white') : navigate({ name: 'setup', mode: config.mode, config }))}
      right={
        <>
          {demoMode && <TestModeBadge />}
          <button className="btn btn-sm" onClick={() => setCreating(true)}>
            + Nuevo
          </button>
        </>
      }
      footer={
        <>
          <span className={`notice ${valid ? '' : 'warn'} select-status`} role="status">
            {status}
          </span>
          {canSplit && (
            <>
              <button className="btn btn-sm" onClick={() => applySplit('balanced')} title="Reparte por ELO para que los equipos estén igualados">
                ⚖ Equilibrar
              </button>
              <button className="btn btn-sm" onClick={() => applySplit('random')}>
                🎲 Aleatorio
              </button>
            </>
          )}
          {step === 'white' ? (
            <button className="btn btn-primary btn-lg" disabled={!whiteReady} onClick={() => setStep('blue')}>
              Siguiente: Azul
            </button>
          ) : (
            <button className="btn btn-primary btn-lg" disabled={!valid} onClick={go}>
              Continuar
            </button>
          )}
        </>
      }
    >
      <div className="select-layout" ref={layoutRef}>
        {teamColumn('white')}
        <div className="pick-grid scroll">
          {available.length === 0 ? (
            <div className="empty" style={{ gridColumn: '1 / -1' }}>
              <div>
                <strong>Sin jugadores</strong>
                Crea al menos dos jugadores para empezar.
                <div style={{ marginTop: 10 }}>
                  <button className="btn btn-primary" onClick={() => setCreating(true)}>
                    Crear jugador
                  </button>
                </div>
              </div>
            </div>
          ) : free.length === 0 ? (
            <div className="empty" style={{ gridColumn: '1 / -1' }}>
              <div>Todos los jugadores están elegidos.</div>
            </div>
          ) : (
            free.map(playerCard)
          )}
        </div>
        {teamColumn('blue')}
      </div>
      {creating && <PlayerEditor onClose={() => setCreating(false)} onSaved={(p) => pick(p.id)} />}
    </ScreenFrame>
  );
}
