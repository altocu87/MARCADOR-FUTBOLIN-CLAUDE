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
import { soundPlayer } from '../../services/sound/soundPlayer';
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
  // -1 = el campeón (con sus números, XP y logros); después, un puesto por paso desde el 2º.
  const rest = ranking.filter((p) => !p.champion);
  const total = Math.max(0, ...ranking.map((p) => p.place));
  const [step, setStep] = useState(-1);
  const lastStep = rest.length - 1;

  // Igual que las ventanas: se pinta sobre la pantalla entera.
  const anchor = useRef<HTMLSpanElement>(null);
  const [host, setHost] = useState<Element | null | undefined>(undefined);
  useLayoutEffect(() => {
    if (anchor.current) setHost(anchor.current.closest('.screen'));
  }, []);

  useEffect(() => {
    if (step === -1) {
      const id = window.setTimeout(() => soundPlayer.play('tournamentWin'), 500);
      return () => window.clearTimeout(id);
    }
    sound.play(step === lastStep && ranking.length > 1 ? 'siren' : 'whoosh');
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

  // Los goles de cada jugador solo si casi todos se apuntaron (si no, no serían reales).
  const linesOf = (ids: string[]) =>
    ids
      .map((id) => report.players.find((l) => l.playerId === id))
      .filter((l): l is PlayerLine => !!l)
      .map((l) => (report.scorersReliable ? l : { ...l, scored: 0 }));

  let body;
  if (step === -1 || !rest.length) {
    const ids = champ?.playerIds ?? [];
    const s = linesOf(ids)[0];
    const pair = ids.length > 1;
    // El nombre del equipo solo si no es la suma de sus nombres (equipo guardado con su escudo).
    const showTeam = !!champ && (!!champ.logo || champ.name !== ids.map(nameOf).join(' + '));
    body = (
      <div className="cer-champion" key="champ">
        {prefs.effects !== 'off' && <Confetti count={prefs.effects === 'full' ? 80 : 24} />}
        <div className="cer-kicker">{t.name}</div>
        <div className="cer-champ-head">
          <Cup id={t.cup} size={60} className="cer-cup" />
          <div className="cer-champ-title">¡CAMPEÓN!</div>
          <Cup id={t.cup} size={60} className="cer-cup" />
        </div>
        {showTeam && (
          <div className="cer-champ-name">
            {champ!.logo && <Crest id={champ!.logo} size={30} />} {champ!.name}
          </div>
        )}
        <div className="cer-sub">
          {s && (
            <>
              <b>{s.played}</b> PJ · <b>{s.wins}</b> G{s.draws ? <> · <b>{s.draws}</b> E</> : null} · <b>{s.losses}</b> P · <b>{s.goalsFor}–{s.goalsAgainst}</b> goles
            </>
          )}
          {report.finalScore && <> · Final {report.finalScore}</>}
        </div>
        <div className={`cer-players cer-champ-rewards${pair ? ' is-pair' : ''}`}>
          {ids.map((pid) => (
            <PlayerReward
              key={pid}
              name={nameOf(pid)}
              photo={photoOf(pid)}
              rank={rankOf(pid)}
              reward={rewardOf(pid, matchIds, progression)}
              progressionOn={!!progression}
              photoSize={pair ? 88 : 104}
              compact={pair}
            />
          ))}
        </div>
      </div>
    );
  } else {
    const place = rest[step];
    body = (
      <PlaceCard
        key={place.team.id}
        place={place}
        last={step === lastStep && ranking.length > 1}
        lines={linesOf(place.team.playerIds)}
        rewards={place.team.playerIds.map((id) => rewardOf(id, matchIds, progression))}
        nameOf={nameOf}
        photoOf={photoOf}
        rankOf={rankOf}
        progressionOn={!!progression}
        lastTitle={t.lastTitle}
      />
    );
  }

  const content = (
    <div className={`tn-ceremony ${step === -1 ? 'is-gold' : step === lastStep && ranking.length > 1 ? 'is-last' : ''}`} role="dialog" aria-modal="true" aria-label="Resumen del torneo">
      <div className="victory-rays" aria-hidden="true" />
      <div className="cer-top">
        <span className="cer-count">{step === -1 ? 'Campeón' : `Puesto ${rest[step].place} de ${total}`}</span>
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
          {rest.map((p, i) => (
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
  lastTitle,
}: {
  place: FinalPlace;
  last: boolean;
  /** Título que se lleva el último (el del torneo); sin él, «Patético». */
  lastTitle?: string;
  lines: PlayerLine[];
  rewards: (Reward | null)[];
  nameOf: (id: string) => string;
  photoOf: (id: string) => string | undefined;
  rankOf: (id: string) => string;
  progressionOn: boolean;
}) {
  // Pareja fija: juegan los mismos partidos y los números del equipo son los del primero. Con cambio de
  // pareja (Pool), cada uno ha jugado los suyos: van debajo de cada jugador.
  const s = lines[0];
  const shared = lines.every((l) => l.played === s?.played && l.wins === s.wins && l.goalsFor === s.goalsFor && l.goalsAgainst === s.goalsAgainst);
  const pair = place.team.playerIds.length > 1;
  const shame = last ? (lastTitle?.trim() || 'Patético').toUpperCase() : undefined;
  const tag = place.champion ? 'CAMPEÓN' : (shame ?? (place.place === 2 ? 'SUBCAMPEÓN' : null));
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
        {s && shared && (
          <div className="cer-stats">
            <span><b>{s.played}</b> PJ</span>
            <span><b>{s.wins}</b> G</span>
            {s.draws > 0 && <span><b>{s.draws}</b> E</span>}
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
        <div className={`cer-players${pair ? ' is-pair' : ''}`}>
          {place.team.playerIds.map((pid, i) => (
            <PlayerReward
              key={pid}
              stats={shared ? undefined : lines.find((l) => l.playerId === pid)}
              compact={pair}
              name={nameOf(pid)}
              photo={photoOf(pid)}
              rank={rankOf(pid)}
              reward={rewards[i]}
              shame={shame}
              progressionOn={progressionOn}
              photoSize={pair ? 76 : 112}
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
  stats,
  compact,
}: {
  /** Sus números, si no son los mismos que los de su pareja. */
  stats?: PlayerLine;
  /** En pareja: sin el desglose de la XP, para que quepan los dos. */
  compact?: boolean;
  name: string;
  photo?: string;
  rank: string;
  reward: Reward | null;
  /** Título del último (sello sobre la foto); sin él, no es el último. */
  shame?: string;
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
    if (reward.levelAfter > reward.levelBefore) timers.push(window.setTimeout(() => soundPlayer.play('levelUp', { queue: true }), 1500));
    return () => timers.forEach((x) => window.clearTimeout(x));
  }, [reward]);
  const achs = (reward?.unlocked ?? []).map((id) => ACHIEVEMENTS.find((a) => a.id === id)).filter((a) => !!a);
  return (
    <div className="cer-player">
      <span className="cer-photo">
        <RankPhoto name={name} photo={photo} rank={rank} size={photoSize} />
        {shame && (
          <span className={`cer-stamp ${shame.length > 10 ? 'is-long' : ''}`}>{shame}</span>
        )}
      </span>
      <div className="cer-player-main">
        <strong className="cer-player-name">
          {name}
        </strong>
        {stats && (
          <span className="cer-player-stats">
            <b>{stats.played}</b> PJ · <b>{stats.wins}</b> G{stats.draws > 0 && <> · <b>{stats.draws}</b> E</>} · <b>{stats.losses}</b> P · <b>{stats.goalsFor}–{stats.goalsAgainst}</b>
            {stats.scored > 0 && <> · <b>{stats.scored}</b> marcados</>}
          </span>
        )}
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
            {!compact && <span className="cer-lines">
              {reward.lines.map((l) => (
                <span key={l.label}>
                  {l.label} <b>+{l.xp}</b>
                </span>
              ))}
            </span>}
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
