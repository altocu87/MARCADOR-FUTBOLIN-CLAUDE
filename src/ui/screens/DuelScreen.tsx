/**
 * Cara a cara de dos jugadores: cuántas veces se ha ganado cada uno y sus números, uno al lado del otro.
 * Se llega desde Perfil → Rivales tocando un rival.
 */
import { useMemo, type ReactNode } from 'react';
import { useApp } from '../../app/AppContext';
import { computePlayerStats, duel } from '../../services/statistics';
import { Avatar, ScreenFrame, pct } from '../components/common';

export function DuelScreen({ a, b }: { a: string; b: string }) {
  const { players, matches, progression, navigate } = useApp();
  const sa = useMemo(() => computePlayerStats(a, matches), [a, matches]);
  const sb = useMemo(() => computePlayerStats(b, matches), [b, matches]);
  const d = useMemo(() => duel(a, b, matches), [a, b, matches]);
  const pa = progression?.players.get(a);
  const pb = progression?.players.get(b);
  const A = players.find((p) => p.id === a);
  const B = players.find((p) => p.id === b);
  const row = (label: string, va: ReactNode, vb: ReactNode, better?: 'a' | 'b' | null) => (
    <div className="duel-row">
      <span className={better === 'a' ? 'win' : ''}>{va}</span>
      <span className="label">{label}</span>
      <span className={better === 'b' ? 'win' : ''}>{vb}</span>
    </div>
  );
  const cmp = (x: number | null | undefined, y: number | null | undefined) => (x == null || y == null || x === y ? null : x > y ? 'a' : 'b');
  const face = (id: string, name = '?', photo?: string) => (
    <button className="duel-face" onClick={() => navigate({ name: 'profile', playerId: id })}>
      <Avatar name={name} photo={photo} size={72} />
      <b>{name}</b>
    </button>
  );
  return (
    <ScreenFrame title="Cara a cara" onBack={() => navigate({ name: 'profile', playerId: a })}>
      <div className="duel scroll">
        <div className="duel-head">
          {face(a, A?.name, A?.photo)}
          <div className="duel-vs">
            <strong>{d.aWins}</strong> – <strong>{d.bWins}</strong>
            <div className="dim" style={{ fontSize: 13 }}>
              {d.played} enfrentamiento{d.played === 1 ? '' : 's'}
              {d.draws ? ` · ${d.draws} empate${d.draws === 1 ? '' : 's'}` : ''}
            </div>
          </div>
          {face(b, B?.name, B?.photo)}
        </div>
        <div className="duel-table">
          {progression && row('ELO', pa?.elo ?? '—', pb?.elo ?? '—', cmp(pa?.elo, pb?.elo))}
          {progression && row('Nivel', pa?.level ?? '—', pb?.level ?? '—', cmp(pa?.level, pb?.level))}
          {row('Partidos', sa.general.played, sb.general.played, cmp(sa.general.played, sb.general.played))}
          {row('Victorias', sa.general.wins, sb.general.wins, cmp(sa.general.wins, sb.general.wins))}
          {row('% victorias', pct(sa.general.winPct), pct(sb.general.winPct), cmp(sa.general.winPct, sb.general.winPct))}
          {row('Mejor racha', sa.bestWinStreak, sb.bestWinStreak, cmp(sa.bestWinStreak, sb.bestWinStreak))}
          {row('Dif. goles equipo', sa.general.goalDiff, sb.general.goalDiff, cmp(sa.general.goalDiff, sb.general.goalDiff))}
          {progression && row('Logros', pa?.achievements.length ?? 0, pb?.achievements.length ?? 0, cmp(pa?.achievements.length, pb?.achievements.length))}
        </div>
      </div>
    </ScreenFrame>
  );
}
