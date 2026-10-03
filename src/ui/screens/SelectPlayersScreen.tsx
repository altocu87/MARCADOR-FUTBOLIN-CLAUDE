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

type Teams = Record<Team, string[]>;

function fromParticipants(parts?: ParticipantRef[]): Teams {
  const out: Teams = { white: [], blue: [] };
  for (const p of [...(parts ?? [])].sort((a, b) => a.slot - b.slot)) out[p.team].push(p.playerId);
  return out;
}

/**
 * Selección por turnos: primero elige el equipo BLANCO y, con «Siguiente», el AZUL.
 * Cada equipo elige de 1 a 4 jugadores, sin tener que ser los mismos (1 contra 2,
 * 2 contra 3…). Un jugador elegido por un equipo aparece desactivado para el otro.
 */
export function SelectPlayersScreen({ config, initial }: { config: MatchConfig; initial?: ParticipantRef[] }) {
  const { players, navigate, progression, demoMode, toast } = useApp();
  const [teams, setTeams] = useState<Teams>(() => fromParticipants(initial));
  // Si se vuelve con los dos equipos hechos, se abre en el turno del azul (listo para continuar).
  const [step, setStep] = useState<Team>(() => (fromParticipants(initial).blue.length > 0 ? 'blue' : 'white'));
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
    if (current.length >= MAX_PER_TEAM) {
      toast(`El equipo ${TEAM_NAME[step]} ya tiene ${MAX_PER_TEAM} jugadores (máximo)`);
      return;
    }
    setTeams({ ...teams, [step]: [...current, playerId] });
  };

  const remove = (team: Team, playerId: string) => {
    setTeams({ ...teams, [team]: teams[team].filter((id) => id !== playerId) });
    setStep(team);
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
      {Array.from({ length: MAX_PER_TEAM }, (_, i) => {
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
              <span className="team-slot-empty">{i === 0 ? 'Plaza 1' : `Plaza ${i + 1} · opcional`}</span>
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
