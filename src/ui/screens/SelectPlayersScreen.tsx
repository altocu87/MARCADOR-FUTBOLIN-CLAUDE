import { useMemo, useState } from 'react';
import { useApp } from '../../app/AppContext';
import { MAX_PER_TEAM, validateParticipants, type MatchConfig, type ParticipantRef, type Slot, type Team } from '../../match-engine';
import type { Player } from '../../services/persistence';
import { balancedTeams, initials, randomTeams, sortPlayers } from '../../services/players';
import { displayTitle } from '../../services/progression';
import { AssetImage } from '../components/assets';
import { Avatar, MODE_LABEL, ScreenFrame, TestModeBadge } from '../components/common';
import { PlayerEditor } from '../components/PlayerEditor';

const TEAM_NAME: Record<Team, string> = { white: 'BLANCO', blue: 'AZUL' };
const OTHER: Record<Team, Team> = { white: 'blue', blue: 'white' };
const SIZES = Array.from({ length: MAX_PER_TEAM }, (_, i) => i + 1);
/** Lo más habitual en la mesa: 2 contra 2. */
const DEFAULT_SIZE = 2;

type Teams = Record<Team, string[]>;

function fromParticipants(parts?: ParticipantRef[]): Teams {
  const out: Teams = { white: [], blue: [] };
  for (const p of [...(parts ?? [])].sort((a, b) => a.slot - b.slot)) out[p.team].push(p.playerId);
  return out;
}

/**
 * Selección por turnos: primero elige el equipo BLANCO y después el AZUL.
 * Cada equipo lleva de 1 a 4 jugadores (por defecto 2); un jugador elegido
 * por un equipo aparece desactivado para el otro.
 */
export function SelectPlayersScreen({ config, initial }: { config: MatchConfig; initial?: ParticipantRef[] }) {
  const { players, navigate, progression, demoMode, toast } = useApp();
  const [teams, setTeams] = useState<Teams>(() => fromParticipants(initial));
  const [size, setSize] = useState(() => {
    const n = fromParticipants(initial).white.length;
    return n >= 1 && n <= MAX_PER_TEAM ? n : DEFAULT_SIZE;
  });
  // Si se vuelve con los dos equipos hechos, se abre en el turno del azul (listo para continuar).
  const [step, setStep] = useState<Team>(() => (fromParticipants(initial).white.length >= size ? 'blue' : 'white'));
  const [creating, setCreating] = useState(false);

  const available = useMemo(() => sortPlayers(players.filter((p) => p.active)), [players]);
  const byId = useMemo(() => new Map(players.map((p) => [p.id, p])), [players]);

  const teamOf = (id: string): Team | undefined =>
    teams.white.includes(id) ? 'white' : teams.blue.includes(id) ? 'blue' : undefined;

  // Tocar un jugador: entra en el equipo que elige; si ya estaba, sale.
  const toggle = (playerId: string) => {
    const current = teams[step];
    if (current.includes(playerId)) {
      setTeams({ ...teams, [step]: current.filter((id) => id !== playerId) });
      return;
    }
    if (teams[OTHER[step]].includes(playerId)) return;
    if (current.length >= size) {
      toast(`El equipo ${TEAM_NAME[step]} ya tiene ${size} jugador${size === 1 ? '' : 'es'}`);
      return;
    }
    const next = { ...teams, [step]: [...current, playerId] };
    setTeams(next);
    // Al completar el blanco, pasa solo al turno del azul.
    if (step === 'white' && next.white.length === size) setStep('blue');
  };

  const remove = (team: Team, playerId: string) => {
    setTeams({ ...teams, [team]: teams[team].filter((id) => id !== playerId) });
    setStep(team);
  };

  const changeSize = (n: number) => {
    setSize(n);
    setTeams({ white: teams.white.slice(0, n), blue: teams.blue.slice(0, n) });
    if (teams.white.length < n) setStep('white');
  };

  const participants: ParticipantRef[] = (['white', 'blue'] as Team[]).flatMap((team) =>
    teams[team].map((id, i) => ({
      playerId: id,
      team,
      slot: (i + 1) as Slot,
      nameSnapshot: byId.get(id)?.name ?? '?',
    })),
  );
  const whiteFull = teams.white.length === size;
  const allFull = whiteFull && teams.blue.length === size;
  const valid = allFull && validateParticipants(participants).length === 0;
  const missing = size - teams[step].length;
  const status = valid
    ? `${size} contra ${size} listo`
    : missing > 0
      ? `Equipo ${TEAM_NAME[step]}: elige ${missing} jugador${missing === 1 ? '' : 'es'} más`
      : 'Completa el otro equipo';

  // Sorteo con todos los elegidos (2v2 en adelante).
  const applySplit = (kind: 'balanced' | 'random') => {
    const chosen = [...teams.white, ...teams.blue];
    const split =
      kind === 'balanced'
        ? balancedTeams(chosen, (id) => progression?.players.get(id)?.elo ?? 1200)
        : randomTeams(chosen);
    setTeams({ white: split.white, blue: split.blue });
  };

  const go = () => {
    if (!valid) return;
    if (config.mode === 'ranked') navigate({ name: 'prematch', config, participants });
    else navigate({ name: 'match', config, participants });
  };

  // Columna de un equipo: tantas plazas como jugadores por equipo.
  const teamColumn = (team: Team) => (
    <div
      className={`team-col team-col-${team} ${step === team ? 'is-active' : ''}`}
      onClick={() => setStep(team)}
      role="button"
      aria-pressed={step === team}
      aria-label={`Elegir jugadores del equipo ${TEAM_NAME[team]}`}
    >
      <div className="team-col-label">{TEAM_NAME[team]}</div>
      {Array.from({ length: size }, (_, i) => {
        const id = teams[team][i];
        const p = id ? byId.get(id) : undefined;
        return (
          <div key={i} className={`team-slot ${p ? 'filled' : ''}`}>
            {p ? (
              <>
                <Avatar name={p.name} photo={p.photo} size={28} />
                <span className="team-slot-name">{p.name}</span>
                <button
                  className="team-slot-clear"
                  onClick={(e) => {
                    e.stopPropagation();
                    remove(team, p.id);
                  }}
                  aria-label={`Quitar a ${p.name}`}
                >
                  ×
                </button>
              </>
            ) : (
              <span className="team-slot-empty">Plaza {i + 1}</span>
            )}
          </div>
        );
      })}
    </div>
  );

  // Ficha del jugador: la imagen ocupa toda la tarjeta y debajo va su información.
  const playerCard = (p: Player) => {
    const team = teamOf(p.id);
    const mine = team === step;
    const taken = team !== undefined && !mine;
    const prog = progression?.players.get(p.id);
    const title = displayTitle(prog, p.titleId);
    const slot = mine ? teams[step].indexOf(p.id) + 1 : 0;
    return (
      <button
        key={p.id}
        className={`pick-card ${mine ? `picked picked-${step}` : ''} ${taken ? 'taken' : ''}`}
        onClick={() => toggle(p.id)}
        disabled={taken}
        aria-pressed={mine}
        aria-label={taken ? `${p.name}, ya está en el equipo ${TEAM_NAME[team!]}` : p.name}
      >
        <span className="pick-photo">
          {p.photo ? <img src={p.photo} alt="" draggable={false} /> : <span className="pick-initials">{initials(p.name)}</span>}
          {mine && <span className="pick-badge">{slot}</span>}
          {taken && <span className="pick-taken">{TEAM_NAME[team!]}</span>}
        </span>
        <span className="pick-info">
          <span className="pick-name">{p.name}</span>
          <span className="pick-meta">{prog ? `Nv ${prog.level}${prog.rankedPlayed ? ` · ELO ${prog.elo}` : ''}` : ' '}</span>
          {title && <span className="pick-title">{title}</span>}
        </span>
      </button>
    );
  };

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
          <div className="team-size" role="group" aria-label="Jugadores por equipo">
            <span className="team-size-label">POR EQUIPO</span>
            {SIZES.map((n) => (
              <button key={n} className="team-size-btn" aria-pressed={size === n} onClick={() => changeSize(n)}>
                {n}
              </button>
            ))}
          </div>
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
          {step === 'blue' && allFull && size >= 2 && (
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
            <button className="btn btn-primary btn-lg" disabled={!whiteFull} onClick={() => setStep('blue')}>
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
      <div className="select-layout">
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
          ) : (
            available.map(playerCard)
          )}
        </div>
        {teamColumn('blue')}
      </div>
      {creating && <PlayerEditor onClose={() => setCreating(false)} onSaved={(p) => toggle(p.id)} />}
    </ScreenFrame>
  );
}
