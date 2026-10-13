/**
 * Cambio de ronda del Pool en escalera, animado:
 * 1. Las pistas de la ronda que acaba, con quién sube (borde verde) y quién baja (borde rojo).
 * 2. Cada uno se desliza a la pista a la que va.
 * 3. Con cambio de pareja, en cada pista las parejas se deshacen y se rehacen.
 * Los jugadores son fichas con posición absoluta: al cambiar su sitio, el navegador anima el viaje.
 */
import { useEffect, useMemo, useRef, useState } from 'react';
import type { Tournament } from '../../services/persistence';
import { arrivalLineup, lineupOfRound, type Lineup } from '../../services/tournaments';
import { Avatar } from './common';

const ROW = 60;
const GAP = 6;
/** Centro de cada hueco de una pista (en % del ancho): dos de Blanco y dos de Azul. */
const SLOT_X = [27, 42, 74, 89];

type Phase = 'start' | 'glow' | 'move' | 'mix' | 'done';

interface Spot {
  row: number;
  slot: number;
}

/** Dónde está cada jugador en unas pistas (el banquillo es la fila de debajo de la última). */
function spots(lu: Lineup): Map<string, Spot> {
  const out = new Map<string, Spot>();
  lu.courts.forEach(([w, b], row) => {
    w.forEach((pid, i) => out.set(pid, { row, slot: i }));
    b.forEach((pid, i) => out.set(pid, { row, slot: 2 + i }));
  });
  lu.bench?.forEach((pid, i) => out.set(pid, { row: lu.courts.length, slot: i }));
  return out;
}

export function LadderRoundShow({
  t,
  round,
  nameOf,
  photoOf,
  onDone,
}: {
  t: Tournament;
  /** Ronda que acaba de terminar. */
  round: number;
  nameOf: (id: string) => string;
  photoOf: (id: string) => string | undefined;
  onDone: () => void;
}) {
  const { from, arrival, to, moves, courts, bench, mixes } = useMemo(() => {
    const from = spots(lineupOfRound(t, round));
    const arr = arrivalLineup(t, round);
    const arrival = spots(arr);
    const to = spots(lineupOfRound(t, round + 1));
    const moves = new Map<string, 'up' | 'down'>();
    from.forEach((s, pid) => {
      const a = arrival.get(pid);
      if (a && a.row !== s.row) moves.set(pid, a.row < s.row ? 'up' : 'down');
    });
    const mixes = [...to].some(([pid, s]) => arrival.get(pid)?.slot !== s.slot);
    return { from, arrival, to, moves, courts: arr.courts.length, bench: !!arr.bench, mixes };
  }, [t, round]);

  const [phase, setPhase] = useState<Phase>('start');
  useEffect(() => {
    const at = (ms: number, p: Phase) => window.setTimeout(() => setPhase(p), ms);
    const timers = [at(350, 'glow'), at(1000, 'move')];
    if (mixes) timers.push(at(2500, 'mix'), at(3700, 'done'));
    else timers.push(at(2600, 'done'));
    return () => timers.forEach((x) => window.clearTimeout(x));
  }, [mixes]);
  // Se avisa una sola vez, acabe sola o con «Saltar».
  const finished = useRef(false);
  const finish = () => {
    if (finished.current) return;
    finished.current = true;
    onDone();
  };
  useEffect(() => {
    if (phase === 'done') finish();
  });

  const where = phase === 'start' || phase === 'glow' ? from : phase === 'move' ? arrival : to;
  const rows = courts + (bench ? 1 : 0);
  const top = (row: number) => row * (ROW + GAP);
  const caption = phase === 'mix' || (phase === 'done' && mixes) ? 'Cambio de pareja…' : phase === 'start' ? `Fin de la ronda ${round}` : 'Los que ganan suben y los que pierden bajan…';

  return (
    <div className="lrs">
      <div className="ladder-head">
        <span className="label">
          Ronda {round} → Ronda {round + 1}
        </span>
        <button className="btn btn-sm" onClick={finish}>
          Saltar
        </button>
      </div>
      <div className="lrs-caption">{caption}</div>
      <div className="lrs-board" style={{ height: rows * (ROW + GAP) - GAP }}>
        {Array.from({ length: rows }, (_, r) => {
          const isBench = bench && r === courts;
          const cls = isBench ? 'ladder-bench' : r === 0 ? 'is-top is-crown' : r === courts - 1 && courts > 1 ? 'is-bottom' : '';
          return (
            <div key={r} className={`ladder-court lrs-row ${cls}`} style={{ top: top(r), height: ROW }}>
              <span className="ladder-badge">{isBench ? 'BANQUILLO' : r === 0 ? <>👑<b>PISTA 1</b></> : `PISTA ${r + 1}`}</span>
              {!isBench && <span className="lrs-vs">vs</span>}
            </div>
          );
        })}
        {[...from.keys()].map((pid) => {
          const s = where.get(pid) ?? from.get(pid)!;
          const mv = phase !== 'start' ? moves.get(pid) : undefined;
          return (
            <span key={pid} className="lrs-token" style={{ left: `${SLOT_X[s.slot]}%`, top: top(s.row) + 3 }}>
              <span className="ladder-player">
                <span className={`fx-photo lrs-photo${mv ? ` mv-${mv}` : ''}`}>
                  <Avatar name={nameOf(pid)} photo={photoOf(pid)} size={34} />
                </span>
                <span className="ladder-name">{nameOf(pid)}</span>
              </span>
            </span>
          );
        })}
      </div>
    </div>
  );
}
