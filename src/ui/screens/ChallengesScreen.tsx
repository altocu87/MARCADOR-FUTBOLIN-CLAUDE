import { useMemo, useState } from 'react';
import { useApp } from '../../app/AppContext';
import { activeChallenges, challengeProgress } from '../../services/progression';
import { sortPlayers } from '../../services/players';
import { Avatar, HelpButton, ScreenFrame } from '../components/common';

/** Retos del día y de la semana con el progreso de cada jugador activo. */
export function ChallengesScreen() {
  const { navigate, players, matches, prefs } = useApp();
  const [now] = useState(() => Date.now());
  const challenges = activeChallenges(now);
  const active = useMemo(() => sortPlayers(players.filter((p) => p.active)), [players]);
  const progress = useMemo(
    () => new Map(active.map((p) => [p.id, challengeProgress(p.id, matches, now)])),
    [active, matches, now],
  );
  const endOfDay = new Date(now);
  endOfDay.setHours(24, 0, 0, 0);
  const hoursLeft = Math.max(0, Math.round((endOfDay.getTime() - now) / 3_600_000));

  return (
    <ScreenFrame
      title="Retos"
      subtitle="diarios y semanales"
      onBack={() => navigate({ name: 'home' })}
      right={<HelpButton title="Retos" text="1 reto diario (+40 XP) y 3 semanales (+100 XP). Solo cuentan los partidos guardados." className="head-help" />}
    >
      {!prefs.challenges && <div className="notice warn">Los retos están desactivados en las reglas del juego: no dan experiencia.</div>}
      <div className="challenge-cards">
        {challenges.map((c) => (
          <div key={c.key} className={`challenge-card scope-${c.scope}`}>
            <span className="challenge-scope">{c.scope === 'daily' ? `HOY · quedan ${hoursLeft} h` : 'ESTA SEMANA'}</span>
            <strong>{c.text}</strong>
            <span className="challenge-xp">+{c.xp} XP</span>
          </div>
        ))}
      </div>
      {active.length === 0 ? (
        <div className="empty">
          <div>
            <strong>Sin jugadores</strong>
            Crea jugadores para seguir su progreso.
          </div>
        </div>
      ) : (
        <div className="list scroll" style={{ flex: 1, minHeight: 0 }}>
          {active.map((p) => (
            <button key={p.id} className="row challenge-row" onClick={() => navigate({ name: 'profile', playerId: p.id })}>
              <Avatar name={p.name} photo={p.photo} size={32} />
              <span className="ellipsis" style={{ width: 110, textAlign: 'left', fontWeight: 800 }}>{p.name}</span>
              {progress.get(p.id)!.map((x) => (
                <span key={x.challenge.key} className={`challenge-progress ${x.done ? 'done' : ''}`} title={x.challenge.text}>
                  <span className="cp-track">
                    <span style={{ width: `${(x.value / x.challenge.target) * 100}%` }} />
                  </span>
                  <span className="cp-text">{x.done ? '✓' : `${x.value}/${x.challenge.target}`}</span>
                </span>
              ))}
            </button>
          ))}
        </div>
      )}
    </ScreenFrame>
  );
}
