/**
 * Reposo: tras inactividad fuera de una partida activa muestra fondo casi negro,
 * un reloj que ocupa toda la pantalla (con barra de segundos) y la fecha. El toque que despierta se consume y no activa lo que hay debajo.
 */
import { useEffect, useMemo, useRef, useState } from 'react';
import { useApp } from '../../app/AppContext';
import { sortMatches } from '../../services/statistics';
import { MatchLine, PlayerTag } from './PlayerFaces';
import { SevenSegment } from './SevenSegment';

export function SleepOverlay({ minutes, enabled }: { minutes: number; enabled: boolean }) {
  const [asleep, setAsleep] = useState(false);
  const [now, setNow] = useState(() => new Date());
  const lastActivity = useRef(Date.now());
  const { players, matches, progression } = useApp();
  const info = useMemo(() => {
    const leader = [...(progression?.players.values() ?? [])]
      .filter((p) => p.rankedPlayed > 0)
      .sort((a, b) => b.elo - a.elo)[0];
    const last = sortMatches(matches).pop();
    return { leader: leader ? { id: leader.playerId, elo: leader.elo } : null, last: last ?? null };
  }, [players, matches, progression]);

  useEffect(() => {
    const mark = () => {
      lastActivity.current = Date.now();
    };
    window.addEventListener('pointerdown', mark, true);
    window.addEventListener('keydown', mark, true);
    return () => {
      window.removeEventListener('pointerdown', mark, true);
      window.removeEventListener('keydown', mark, true);
    };
  }, []);

  useEffect(() => {
    // Cada 250 ms para que el cambio de segundo se vea en el momento; solo se repinta si el segundo cambia.
    const id = window.setInterval(() => {
      const d = new Date();
      setNow((prev) => (prev.getSeconds() === d.getSeconds() && prev.getMinutes() === d.getMinutes() ? prev : d));
      if (!enabled || minutes <= 0) {
        lastActivity.current = Date.now();
        return;
      }
      if (Date.now() - lastActivity.current >= minutes * 60_000) setAsleep(true);
    }, 250);
    return () => window.clearInterval(id);
  }, [enabled, minutes]);

  useEffect(() => {
    if (!asleep) return;
    const wake = (e: Event) => {
      e.preventDefault();
      e.stopPropagation();
      lastActivity.current = Date.now();
      setAsleep(false);
    };
    window.addEventListener('keydown', wake, true);
    return () => window.removeEventListener('keydown', wake, true);
  }, [asleep]);

  if (!asleep) return null;
  const wakeUp = (e: React.SyntheticEvent) => {
    e.preventDefault();
    e.stopPropagation();
    lastActivity.current = Date.now();
    setAsleep(false);
  };
  const hh = String(now.getHours()).padStart(2, '0');
  const mm = String(now.getMinutes()).padStart(2, '0');
  const sec = now.getSeconds();
  return (
    <div
      className="sleep"
      role="button"
      aria-label="Reposo. Toca para despertar"
      tabIndex={0}
      onPointerDown={wakeUp}
      onClick={wakeUp}
    >
      {/* Los dos puntos laten con cada segundo (pares encendidos, impares apagados). */}
      <div className={`sleep-clock${sec % 2 ? ' sec-odd' : ''}`}>
        <SevenSegment text={`${hh}:${mm}`} height={250} color="#4fd8ff" />
      </div>
      {/* Barra de 60 marcas: se van encendiendo una por segundo y la actual da un destello. */}
      <div className="sleep-seconds" aria-hidden="true">
        <div className="sleep-ticks">
          {Array.from({ length: 60 }, (_, i) => (
            <span key={i === sec ? `now-${sec}` : i} className={i < sec ? 'tick on' : i === sec ? 'tick now' : 'tick'} />
          ))}
        </div>
        <span key={sec} className="sleep-sec">
          {String(sec).padStart(2, '0')}
        </span>
      </div>
      <div className="sleep-date">
        {now.toLocaleDateString('es-ES', { weekday: 'long', day: 'numeric', month: 'long' })}
      </div>
      {(info.leader || info.last) && (
        <div className="sleep-info">
          {info.leader && (
            <span className="sleep-line">
              👑 Líder:{' '}
              <strong>
                <PlayerTag id={info.leader.id} size={34}>
                  <span className="muted">{info.leader.elo} ELO</span>
                </PlayerTag>
              </strong>
            </span>
          )}
          {info.last && (
            <span className="sleep-line">
              Último:{' '}
              <strong>
                <MatchLine match={info.last} size={30} />
              </strong>
            </span>
          )}
        </div>
      )}
      <div className="sleep-hint">Toca para despertar</div>
    </div>
  );
}
