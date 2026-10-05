/**
 * Cartel de la fase de un torneo a pantalla completa, antes del 3·2·1: «GRAN FINAL»,
 * «SEMIFINAL», «CUARTOS DE FINAL», «JORNADA 3»… Cada nivel tiene su color y su adorno
 * (la final, dorada con el trofeo y destellos). Se salta tocando o con un pulsador.
 */
import type { CSSProperties } from 'react';
import type { ParticipantRef, Team } from '../../match-engine';
import type { PhaseInfo } from '../../services/tournaments';
import { AssetImage } from './assets';

const TIER_ICON: Record<PhaseInfo['tier'], string> = {
  final: '🏆',
  semi: '⚔',
  quarter: '⚡',
  eighth: '🔥',
  round: '⚽',
};

export function PhasePoster({
  info,
  tournamentName,
  participants,
  onSkip,
}: {
  info: PhaseInfo;
  tournamentName: string;
  participants: ParticipantRef[];
  onSkip: () => void;
}) {
  const names = (t: Team) =>
    participants
      .filter((p) => p.team === t)
      .sort((a, b) => a.slot - b.slot)
      .map((p) => p.nameSnapshot)
      .join(' · ');
  return (
    <button className={`overlay phase-poster tier-${info.tier}`} onClick={onSkip} aria-label={`${info.title}. Toca para empezar`}>
      <span className="pp-rays" aria-hidden="true" />
      <span className="pp-sparks" aria-hidden="true">
        {Array.from({ length: 14 }, (_, i) => (
          <i key={i} style={{ '--i': i } as CSSProperties} />
        ))}
      </span>
      <span className="pp-tournament">{tournamentName}</span>
      <span className="pp-emblem" aria-hidden="true">
        {info.tier === 'final' ? <AssetImage name="trofeo" className="pp-trophy" fallback={TIER_ICON.final} /> : TIER_ICON[info.tier]}
      </span>
      <span className="pp-title-wrap">
        <span className="pp-line left" aria-hidden="true" />
        <span className={`pp-title ${info.title.length > 13 ? 'is-xlong' : info.title.length > 9 ? 'is-long' : ''}`}>{info.title}</span>
        <span className="pp-line right" aria-hidden="true" />
      </span>
      {info.sub && <span className="pp-sub">{info.sub}</span>}
      <span className="pp-versus">
        <span className="pp-team white">{names('white')}</span>
        <span className="pp-vs">VS</span>
        <span className="pp-team blue">{names('blue')}</span>
      </span>
      <span className="overlay-hint">Toca para empezar</span>
    </button>
  );
}
