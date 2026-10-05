/**
 * Crear o editar un equipo fijo: dos jugadores, nombre y logo. Se reconoce solo siempre que esos
 * dos jueguen juntos (en partidos y torneos). Con `fixedPlayers` la pareja ya viene dada.
 */
import { useState } from 'react';
import { useApp } from '../../app/AppContext';
import { clubNameTaken, findClub, type Club } from '../../services/clubs';
import { sortPlayers } from '../../services/players';
import { Avatar, Modal } from './common';
import { Crest, IconGrid, defaultLogo } from './Crest';

export function ClubEditor({
  club,
  fixedPlayers,
  initial,
  onSave,
  onClose,
}: {
  /** Equipo que se edita (en Ajustes). */
  club?: Club;
  /** Pareja ya elegida (en la creación del torneo). */
  fixedPlayers?: string[];
  initial?: { name: string; logo: string } | null;
  onSave: (v: { name: string; logo: string; playerIds: string[] }) => void;
  onClose: () => void;
}) {
  const { players, prefs } = useApp();
  const [picked, setPicked] = useState<string[]>(fixedPlayers ?? club?.playerIds ?? []);
  const [name, setName] = useState(initial?.name ?? club?.name ?? '');
  const [logo, setLogo] = useState(initial?.logo ?? club?.logo ?? defaultLogo(picked.join('|') || String(Date.now())));
  const others = prefs.clubs.filter((c) => c.id !== club?.id);
  const taken = !!name.trim() && clubNameTaken(others, name, picked);
  // Esa pareja ya tiene otro equipo.
  const pairTaken = picked.length === 2 && !fixedPlayers && !!findClub(others, picked);
  const nameOf = (id: string) => players.find((p) => p.id === id);
  const toggle = (id: string) => setPicked((p) => (p.includes(id) ? p.filter((x) => x !== id) : p.length < 2 ? [...p, id] : [p[1], id]));
  const ok = picked.length === 2 && !!name.trim() && !taken && !pairTaken;
  return (
    <Modal
      title={club ? 'Editar equipo' : 'Equipo'}
      onClose={onClose}
      actions={
        <>
          <button className="btn btn-ghost" onClick={onClose}>
            Cancelar
          </button>
          <button
            className="btn btn-primary"
            disabled={!ok}
            onClick={() => {
              onSave({ name: name.trim(), logo, playerIds: [...picked].sort() });
              onClose();
            }}
          >
            Guardar equipo
          </button>
        </>
      }
    >
      <div className="team-editor">
        <div className="team-editor-head">
          <Crest id={logo} size={72} />
          <div className="team-editor-fields">
            <input className="input" value={name} maxLength={22} placeholder="Nombre del equipo" aria-label="Nombre del equipo" onChange={(e) => setName(e.target.value)} autoFocus />
            <div className="team-editor-players">
              {picked.length === 0 && <span className="muted">Elige los dos jugadores</span>}
              {picked.map((id) => {
                const p = nameOf(id);
                return (
                  <span key={id}>
                    <Avatar name={p?.name ?? '?'} photo={p?.photo} size={30} /> {p?.name ?? '?'}
                  </span>
                );
              })}
            </div>
          </div>
        </div>
        {taken && <div className="notice warn">Ya hay otro equipo con ese nombre.</div>}
        {pairTaken && <div className="notice warn">Esta pareja ya tiene un equipo: «{findClub(others, picked)!.name}».</div>}
        {!fixedPlayers && (
          <>
            <div className="label">Jugadores ({picked.length}/2)</div>
            <div className="club-pick">
              {sortPlayers(players)
                .filter((p) => p.active || picked.includes(p.id))
                .map((p) => (
                  <button key={p.id} className={`club-pick-cell ${picked.includes(p.id) ? 'is-on' : ''}`} aria-pressed={picked.includes(p.id)} onClick={() => toggle(p.id)}>
                    <Avatar name={p.name} photo={p.photo} size={40} />
                    <span>{p.name}</span>
                  </button>
                ))}
            </div>
          </>
        )}
        <div className="label">Logo</div>
        <IconGrid kind="logo" value={logo} onPick={setLogo} playerIds={picked} size={48} />
        <small className="muted">Siempre que estos dos jueguen juntos saldrán con este nombre y logo, y en los torneos de parejas jugarán juntos.</small>
      </div>
    </Modal>
  );
}
