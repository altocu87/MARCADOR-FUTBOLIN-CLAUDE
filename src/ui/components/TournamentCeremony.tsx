/**
 * Cierre del torneo: primero se nombra al campeón y después se repasa la clasificación final
 * uno a uno, del campeón al último (que se lleva el sobrenombre de PATÉTICO). En cada ficha
 * salen su puesto, sus números en el torneo y la XP, el nivel, el ELO y los logros que gana
 * ahora, porque los partidos de torneo no dan XP ni logros hasta que el torneo termina.
 */
import { useEffect, useLayoutEffect, useRef, useState, type CSSProperties } from 'react';
import { createPortal } from 'react-dom';
import { useApp } from '../../app/AppContext';
import type { Tournament } from '../../services/persistence';
import { ACHIEVEMENTS, levelProgress, type ProgressionSnapshot } from '../../services/progression';
import { sound } from '../../services/sound/sound';
import { finalRanking, tournamentReport, type FinalPlace, type PlayerLine } from '../../services/tournaments';
import { Crest, Cup } from './Crest';
import { AchievementIcon, Confetti } from './graphics';
import { RankPhoto } from './PlayerCards';

/** Lo que un jugador se lleva del torneo (suma de todos sus partidos). */
interface Reward {
  xp: number;
  levelBefore: number;
  levelAfter: number;
  /** Barra de nivel al acabar (0-1). */
  levelFill: number;
  eloDelta?: number;
  eloAfter?: number;
  unlocked: string[];
  /** Desglose de la XP agrupado por concepto. */
  lines: { label: string; xp: number }[];
}

function rewardOf(playerId: string, matchIds: string[], progression: ProgressionSnapshot | null): Reward | null {
  const entries = matchIds.map((id) => progression?.byMatch.get(id)?.get(playerId)).filter((e) => !!e);
  if (!entries.length) return null;
  const first = entries[0];
  const last = entries[entries.length - 1];
  const lines = new Map<string, number>();
  for (const e of entries) for (const l of e.xpBreakdown) lines.set(l.label, (lines.get(l.label) ?? 0) + l.xp);
  const elo = entries.filter((e) => e.eloDelta !== undefined);
  return {
    xp: entries.reduce((s, e) => s + e.xpGained, 0),
    levelBefore: first.levelBefore,
    levelAfter: last.levelAfter,
    levelFill: levelProgress(last.xpAfter),
    eloDelta: elo.length ? elo.reduce((s, e) => s + (e.eloDelta ?? 0), 0) : undefined,
    eloAfter: elo.at(-1)?.eloAfter,
    unlocked: entries.flatMap((e) => e.unlocked),
    lines: [...lines].map(([label, xp]) => ({ label, xp })).sort((a, b) => b.xp - a.xp),
  };
}

export function TournamentCeremony({ t, onClose }: { t: Tournament; onClose: () => void }) {
  const { matches, players, progression, prefs } = useApp();
  const ranking = finalRanking(t, matches);
  const report = tournamentReport(t, matches, players);
  const matchIds = report.matches.map((m) => m.id);
  const champ = t.teams.find((x) => x.id === t.winnerTeamId);
  // -1 = cartel del campeón; después, un puesto por paso.
  const [step, setStep] = useState(-1);
  const lastStep = ranking.length - 1;

  // Igual que las ventanas: se pinta sobre la pantalla entera.
  const anchor = useRef<HTMLSpanElement>(null);
  const [host, setHost] = useState<Element | null | undefined>(undefined);
  useLayoutEffect(() => {
    if (anchor.current) setHost(anchor.current.closest('.screen'));
  }, []);

  useEffect(() => {
    if (step === -1) {
      const id = window.setTimeout(() => sound.playAnthem('champion'), 500);
      return () => window.clearTimeout(id);
    }
    sound.play(step === lastStep && step > 0 ? 'siren' : 'whoosh');
    return undefined;
  }, [step, lastStep]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
      if (e.key === 'ArrowRight' || e.key === 'Enter') setStep((s) => Math.min(s + 1, lastStep));
      if (e.key === 'ArrowLeft') setStep((s) => Math.max(s - 1, -1));
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [onClose, lastStep]);

  const photoOf = (id: string) => players.find((p) => p.id === id)?.photo;
  const nameOf = (id: string) => players.find((p) => p.id === id)?.name ?? '?';
  const rankOf = (id: string) => progression?.players.get(id)?.category.id ?? 'none';

  let body;
  if (step === -1 || !ranking.length) {
    body = (
      <div className="cer-champion" key="champ">
        {prefs.effects !== 'off' && <Confetti count={prefs.effects === 'full' ? 80 : 24} />}
        <div className="cer-kicker">{t.name}</div>
        <div className="cer-champ-title">¡CAMPEÓN!</div>
        <Cup id={t.cup} size={76} className="cer-cup" />
        <div className="cer-champ-photos">
          {(champ?.playerIds ?? []).map((pid) => (
            <span key={pid} className="cer-champ-player">
              <RankPhoto name={nameOf(pid)} photo={photoOf(pid)} rank={rankOf(pid)} size={110} />
              <b>{nameOf(pid)}</b>
            </span>
          ))}
        </div>
        {champ && (
          <div className="cer-champ-name">
            {champ.logo && <Crest id={champ.logo} size={34} />} {champ.name}
          </div>
        )}
        {report.finalScore && <div className="cer-sub">Final: {report.finalScore}</div>}
      </div>
    );
  } else {
    const place = ranking[step];
    body = (
      <PlaceCard
        key={place.team.id}
        place={place}
        last={step === lastStep && ranking.length > 1}
        lines={place.team.playerIds.map((id) => report.players.find((l) => l.playerId === id)).filter((l): l is PlayerLine => !!l)}
        rewards={place.team.playerIds.map((id) => rewardOf(id, matchIds, progression))}
        nameOf={nameOf}
        photoOf={photoOf}
        rankOf={rankOf}
        progressionOn={!!progression}
      />
    );
  }

  const content = (
    <div className={`tn-ceremony ${step === lastStep && step > 0 ? 'is-last' : step <= 0 ? 'is-gold' : ''}`} role="dialog" aria-modal="true" aria-label="Resumen del torneo">
      <div className="victory-rays" aria-hidden="true" />
      <div className="cer-top">
        <span className="cer-count">{step === -1 ? 'Campeón' : `Puesto ${step + 1} de ${ranking.length}`}</span>
        <button className="btn btn-sm btn-ghost" onClick={onClose}>
          Saltar
        </button>
      </div>
      <div className="cer-stage">{body}</div>
      <div className="cer-nav">
        <button className="btn" onClick={() => setStep((s) => s - 1)} disabled={step === -1}>
          ◀
        </button>
        <div className="cer-dots" aria-hidden="true">
          {ranking.map((p, i) => (
            <i key={p.team.id} className={i === step ? 'on' : i < step ? 'seen' : ''} />
          ))}
        </div>
        {step < lastStep ? (
          <button className="btn btn-primary btn-lg" onClick={() => setStep((s) => s + 1)} autoFocus>
            {step === -1 ? 'Ver clasificación ▶' : 'Siguiente ▶'}
          </button>
        ) : (
          <button className="btn btn-primary btn-lg" onClick={onClose} autoFocus>
            Terminar
          </button>
        )}
      </div>
    </div>
  );
  if (host) return createPortal(content, host);
  return host === undefined ? <span ref={anchor} hidden /> : content;
}

function PlaceCard({
  place,
  last,
  lines,
  rewards,
  nameOf,
  photoOf,
  rankOf,
  progressionOn,
}: {
  place: FinalPlace;
  last: boolean;
  lines: PlayerLine[];
  rewards: (Reward | null)[];
  nameOf: (id: string) => string;
  photoOf: (id: string) => string | undefined;
  rankOf: (id: string) => string;
  progressionOn: boolean;
}) {
  // En pareja juegan los mismos partidos: los números del equipo son los del primero.
  const s = lines[0];
  const tag = place.champion ? 'CAMPEÓN' : last ? 'PATÉTICO' : place.place === 2 ? 'SUBCAMPEÓN' : null;
  // El nombre del equipo solo hace falta si es una pareja o un equipo con logo.
  const showTeam = place.team.playerIds.length > 1 || !!place.team.logo;
  return (
    <div className={`cer-card ${place.champion ? 'cer-gold' : last ? 'cer-shame' : ''}`}>
      <div className="cer-place">
        <span className="cer-place-num">{place.place}º</span>
        {tag && <span className="cer-tag">{tag}</span>}
      </div>
      <div className="cer-card-main">
        {showTeam && (
          <div className="cer-team-name">
            {place.team.logo && <Crest id={place.team.logo} size={28} />} {place.team.name}
          </div>
        )}
        {s && (
          <div className="cer-stats">
            <span><b>{s.played}</b> PJ</span>
            <span><b>{s.wins}</b> G</span>
            <span><b>{s.losses}</b> P</span>
            <span>
              <b>{s.goalsFor}–{s.goalsAgainst}</b> goles
            </span>
            {lines.some((l) => l.scored > 0) && (
              <span>
                <b>{lines.reduce((n, l) => n + l.scored, 0)}</b> marcados
              </span>
            )}
          </div>
        )}
        <div className="cer-players">
          {place.team.playerIds.map((pid, i) => (
            <PlayerReward
              key={pid}
              name={nameOf(pid)}
              photo={photoOf(pid)}
              rank={rankOf(pid)}
              reward={rewards[i]}
              shame={last}
              progressionOn={progressionOn}
              photoSize={place.team.playerIds.length > 1 ? 84 : 112}
            />
          ))}
        </div>
      </div>
    </div>
  );
}

function PlayerReward({
  name,
  photo,
  rank,
  reward,
  shame,
  progressionOn,
  photoSize,
}: {
  name: string;
  photo?: string;
  rank: string;
  reward: Reward | null;
  shame: boolean;
  progressionOn: boolean;
  photoSize: number;
}) {
  // La XP cuenta sola y la barra de nivel se llena.
  const [shown, setShown] = useState(0);
  const [fill, setFill] = useState(0);
  useEffect(() => {
    if (!reward) return;
    const timers: number[] = [];
    const steps = 20;
    for (let k = 1; k <= steps; k += 1) {
      timers.push(window.setTimeout(() => setShown(Math.round((reward.xp * k) / steps)), 400 + k * 50));
    }
    timers.push(window.setTimeout(() => setFill(reward.levelFill), 400));
    if (reward.levelAfter > reward.levelBefore) timers.push(window.setTimeout(() => sound.play('levelUp'), 1500));
    return () => timers.forEach((x) => window.clearTimeout(x));
  }, [reward]);
  const achs = (reward?.unlocked ?? []).map((id) => ACHIEVEMENTS.find((a) => a.id === id)).filter((a) => !!a);
  return (
    <div className="cer-player">
      <span className="cer-photo">
        <RankPhoto name={name} photo={photo} rank={rank} size={photoSize} />
        {shame && <span className="cer-stamp">PATÉTICO</span>}
      </span>
      <div className="cer-player-main">
        <strong className="cer-player-name">
          {name}
          {shame && <em className="cer-nick"> «PATÉTICO»</em>}
        </strong>
        {!progressionOn ? (
          <span className="dim">Progresión desactivada</span>
        ) : reward ? (
          <>
            <span className="cer-xp-row">
              <span className="cer-xp">+{shown} XP</span>
              {reward.eloDelta !== undefined && (
                <span className={`vic-elo ${reward.eloDelta >= 0 ? 'up' : 'down'}`}>
                  ELO {reward.eloDelta >= 0 ? '+' : ''}
                  {reward.eloDelta}
                </span>
              )}
            </span>
            <span className="cer-level">
              <span className={`vic-lv ${reward.levelAfter > reward.levelBefore ? 'leveled' : ''}`}>
                NV {reward.levelAfter}
                {reward.levelAfter > reward.levelBefore && ` ⬆`}
              </span>
              <span className="vic-bar">
                <span className="vic-bar-fill" style={{ width: `${Math.round(fill * 100)}%`, transition: 'width 1.2s cubic-bezier(0.3, 0.8, 0.3, 1)' }} />
              </span>
            </span>
            <span className="cer-lines">
              {reward.lines.map((l) => (
                <span key={l.label}>
                  {l.label} <b>+{l.xp}</b>
                </span>
              ))}
            </span>
            {achs.length > 0 && (
              <span className="cer-achs">
                {achs.map((a, i) => (
                  <span key={a.id} className={`cer-ach rarity-${a.rarity}`} style={{ '--d': `${0.8 + i * 0.15}s` } as CSSProperties}>
                    <AchievementIcon id={a.id} glyph={a.icon} rarity={a.rarity} size={26} /> {a.name}
                  </span>
                ))}
              </span>
            )}
          </>
        ) : (
          <span className="dim">No llegó a jugar</span>
        )}
      </div>
    </div>
  );
}
