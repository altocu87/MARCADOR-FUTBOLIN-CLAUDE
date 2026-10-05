import { useMemo, useState, type ReactNode } from 'react';
import { useApp } from '../../app/AppContext';
import type { RankingTab } from '../../app/routes';
import type { MatchMode } from '../../match-engine';
import { computeHallOfFame, computeRecords, seasonChampions, seasonProgression } from '../../services/progression';
import { sortPlayers } from '../../services/players';
import {
  computePlayerStats,
  duel,
  pairStats,
  periodDigest,
  seasonKey,
  seasonLabel,
  sortMatches,
} from '../../services/statistics';
import { Avatar, FormChips, MODE_LABEL, ScreenFrame, Tabs, formatDate, pct } from '../components/common';
import { CategoryBadge, Podium } from '../components/graphics';

const PAGE = 20;

export function RankingScreen({ tab: initialTab }: { tab?: RankingTab }) {
  const { navigate } = useApp();
  const [tab, setTab] = useState<RankingTab>(initialTab ?? 'standings');
  return (
    <ScreenFrame title="Ranking" onBack={() => navigate({ name: 'home' })}>
      <Tabs
        label="Secciones de ranking"
        value={tab}
        onChange={setTab}
        tabs={[
          { id: 'standings', label: 'Clasificación' },
          { id: 'history', label: 'Historial' },
          { id: 'players', label: 'Jugadores' },
          { id: 'fame', label: 'Hall of Fame' },
          { id: 'records', label: 'Récords' },
          { id: 'pairs', label: 'Parejas' },
          { id: 'digest', label: 'Resumen' },
          { id: 'duel', label: 'Cara a cara' },
        ]}
      />
      {tab === 'standings' && <Standings />}
      {tab === 'history' && <History />}
      {tab === 'players' && <PlayersGrid />}
      {tab === 'fame' && <HallOfFame />}
      {tab === 'records' && <Records />}
      {tab === 'pairs' && <Pairs />}
      {tab === 'digest' && <Digest />}
      {tab === 'duel' && <Duel />}
    </ScreenFrame>
  );
}

function Standings() {
  const { players: allPlayers, matches, progression, navigate, prefs } = useApp();
  // Los invitados no salen en el ranking.
  const players = useMemo(() => allPlayers.filter((p) => !p.guest), [allPlayers]);
  const [scope, setScope] = useState<'season' | 'all'>('season');
  const now = Date.now();
  const currentSeason = seasonKey(now, prefs.seasonLength);
  const source = useMemo(() => {
    if (!progression) return null;
    return scope === 'all'
      ? progression
      : seasonProgression(players, matches, prefs.progression, prefs.seasonLength, currentSeason);
  }, [scope, progression, players, matches, prefs.progression, prefs.seasonLength, currentSeason]);
  const rows = useMemo(() => {
    if (!source) return [];
    const ranked = matches.filter(
      (m) => m.config.mode === 'ranked' && (scope === 'all' || seasonKey(m.finishedAt, prefs.seasonLength) === currentSeason),
    );
    return players
      .map((p) => ({ p, prog: source.players.get(p.id)!, form: computePlayerStats(p.id, ranked).form }))
      .filter((r) => r.prog && r.prog.rankedPlayed > 0)
      // Desempate propuesto: ELO, luego partidos clasificatorios, luego nombre.
      .sort((a, b) => b.prog.elo - a.prog.elo || b.prog.rankedPlayed - a.prog.rankedPlayed || a.p.name.localeCompare(b.p.name, 'es'));
  }, [players, matches, source, scope, prefs.seasonLength, currentSeason]);

  if (!progression) {
    return (
      <div className="empty">
        <div>
          <strong>Clasificación pendiente</strong>
          La progresión (ELO/XP) está desactivada en Ajustes → Progresión.
        </div>
      </div>
    );
  }
  return (
    <>
      <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
        <div className="segmented" style={{ width: 360 }}>
          <button className="seg" style={{ minHeight: 44 }} aria-pressed={scope === 'season'} onClick={() => setScope('season')}>
            {seasonLabel(currentSeason).toUpperCase()}
          </button>
          <button className="seg" style={{ minHeight: 44 }} aria-pressed={scope === 'all'} onClick={() => setScope('all')}>
            HISTÓRICO
          </button>
        </div>
        <span className="dim" style={{ fontSize: 11 }}>
          {scope === 'season' ? 'ELO de temporada: todos empiezan en 1200.' : 'ELO acumulado de siempre.'}
        </span>
      </div>
      {rows.length === 0 ? (
        <div className="empty">
          <div>
            <strong>Sin partidos clasificatorios</strong>
            {scope === 'season' ? 'Nadie ha jugado un Clasificatorio esta temporada.' : 'Juega un Clasificatorio (sin modo prueba) para aparecer.'}
          </div>
        </div>
      ) : (
        <>
          <div className="std-head">
            <span>#</span>
            <span>Jugador</span>
            <span>ELO</span>
            <span>Categoría</span>
            <span>PJ</span>
            <span>Forma</span>
          </div>
          <div className="list scroll" style={{ flex: 1, minHeight: 0 }}>
            {rows.map((r, i) => (
              <button key={r.p.id} className={`row std-row ${i < 3 ? `top-${i + 1}` : ''}`} onClick={() => navigate({ name: 'profile', playerId: r.p.id })}>
                <span className={`std-pos pos-${i + 1}`}>{i + 1}</span>
                <span style={{ display: 'flex', alignItems: 'center', gap: 8, minWidth: 0 }}>
                  <Avatar name={r.p.name} photo={r.p.photo} size={32} />
                  <span className="ellipsis">{r.p.name}</span>
                </span>
                <span style={{ fontWeight: 800, fontVariantNumeric: 'tabular-nums' }}>{r.prog.elo}</span>
                <span style={{ color: r.prog.category.color, fontWeight: 700, display: 'flex', alignItems: 'center', gap: 6 }}>
                  <CategoryBadge category={r.prog.category} size={30} />
                  {r.prog.category.name}
                </span>
                <span className="muted">{r.prog.rankedPlayed}</span>
                <FormChips form={r.form} />
              </button>
            ))}
          </div>
        </>
      )}
      <div className="dim" style={{ fontSize: 11 }}>
        Reglas propuestas pendientes de aprobación · temporada {prefs.seasonLength === 'month' ? 'mensual' : 'trimestral'} (Ajustes → General).
      </div>
    </>
  );
}

function History() {
  const { matches, players: allPlayers, navigate } = useApp();
  // Los invitados no salen en el ranking.
  const players = useMemo(() => allPlayers.filter((p) => !p.guest), [allPlayers]);
  const [mode, setMode] = useState<MatchMode | 'all'>('all');
  const [playerId, setPlayerId] = useState<string>('all');
  const [page, setPage] = useState(0);
  const list = useMemo(
    () =>
      sortMatches(matches)
        .reverse()
        .filter((m) => mode === 'all' || m.config.mode === mode)
        .filter((m) => playerId === 'all' || m.participants.some((p) => p.playerId === playerId)),
    [matches, mode, playerId],
  );
  const pages = Math.max(1, Math.ceil(list.length / PAGE));
  const current = list.slice(page * PAGE, page * PAGE + PAGE);
  const names = (m: (typeof matches)[number], t: 'white' | 'blue') =>
    m.participants.filter((p) => p.team === t).sort((a, b) => a.slot - b.slot).map((p) => p.nameSnapshot).join(' + ');

  return (
    <>
      <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
        <select className="input" style={{ minHeight: 44, fontSize: 14 }} value={mode} onChange={(e) => { setMode(e.target.value as MatchMode | 'all'); setPage(0); }} aria-label="Filtrar por modalidad">
          <option value="all">Todas las modalidades</option>
          <option value="quick">{MODE_LABEL.quick}</option>
          <option value="chaos">{MODE_LABEL.chaos}</option>
          <option value="ranked">{MODE_LABEL.ranked}</option>
        </select>
        <select className="input" style={{ minHeight: 44, fontSize: 14 }} value={playerId} onChange={(e) => { setPlayerId(e.target.value); setPage(0); }} aria-label="Filtrar por jugador">
          <option value="all">Todos los jugadores</option>
          {sortPlayers(players).map((p) => (
            <option key={p.id} value={p.id}>{p.name}</option>
          ))}
        </select>
        <span style={{ flex: 1 }} />
        <button className="btn btn-sm" disabled={page === 0} onClick={() => setPage(page - 1)} aria-label="Página anterior">‹</button>
        <span className="muted" style={{ fontSize: 13 }}>{page + 1}/{pages}</span>
        <button className="btn btn-sm" disabled={page >= pages - 1} onClick={() => setPage(page + 1)} aria-label="Página siguiente">›</button>
      </div>
      {list.length === 0 ? (
        <div className="empty">
          <div>
            <strong>Historial vacío</strong>
            Los partidos jugados sin modo prueba aparecerán aquí.
          </div>
        </div>
      ) : (
        <div className="list scroll" style={{ flex: 1, minHeight: 0 }}>
          {current.map((m) => (
            <button key={m.id} className="row hist-row" onClick={() => navigate({ name: 'matchDetail', matchId: m.id })}>
              <span className="dim hist-date">{formatDate(m.finishedAt)}</span>
              <span className={`hist-mode mode-${m.config.mode}`}>{MODE_LABEL[m.config.mode]}</span>
              <span className={`hist-name ${m.result.winner === 'white' ? 'win' : ''}`}>{names(m, 'white')}</span>
              <span className="hist-score">
                {m.result.score.white}–{m.result.score.blue}
              </span>
              <span className={`hist-name right ${m.result.winner === 'blue' ? 'win' : ''}`}>{names(m, 'blue')}</span>
              <span className="hist-tags">
                {m.result.reason === 'golden_goal' && <span className="badge">Prórroga</span>}
                {m.result.penaltyScore && (
                  <span className="badge">P {m.result.penaltyScore.white}–{m.result.penaltyScore.blue}</span>
                )}
              </span>
            </button>
          ))}
        </div>
      )}
    </>
  );
}

function PlayersGrid() {
  const { players: allPlayers, progression, navigate } = useApp();
  // Los invitados no salen en el ranking.
  const players = useMemo(() => allPlayers.filter((p) => !p.guest), [allPlayers]);
  const list = sortPlayers(players);
  if (list.length === 0) {
    return (
      <div className="empty">
        <div>
          <strong>Sin jugadores</strong>
          Créalos en Ajustes → Jugadores.
        </div>
      </div>
    );
  }
  return (
    <div className="pgrid pgrid-wide scroll" style={{ flex: 1, minHeight: 0 }}>
      {list.map((p) => {
        const prog = progression?.players.get(p.id);
        return (
          <button key={p.id} className={`pcard ${p.active ? '' : 'inactive'}`} onClick={() => navigate({ name: 'profile', playerId: p.id })}>
            <Avatar name={p.name} photo={p.photo} size={44} />
            <span className="pcard-name">{p.name}</span>
            <span className="pcard-meta">
              {!p.active ? 'Inactivo' : prog ? `Nv ${prog.level}${prog.rankedPlayed ? ` · ${prog.elo}` : ''}` : ' '}
            </span>
          </button>
        );
      })}
    </div>
  );
}

function HallOfFame() {
  const { players: allPlayers, matches, progression, navigate, prefs } = useApp();
  // Los invitados no salen en el ranking.
  const players = useMemo(() => allPlayers.filter((p) => !p.guest), [allPlayers]);
  const rows = useMemo(() => computeHallOfFame(players, matches, progression), [players, matches, progression]);
  const champions = useMemo(
    () => seasonChampions(players, matches, prefs.progression, prefs.seasonLength, Date.now()),
    [players, matches, prefs.progression, prefs.seasonLength],
  );
  const nonEmpty = rows.filter((r) => r.leaders.length > 0);
  if (nonEmpty.length === 0) {
    return (
      <div className="empty">
        <div>
          <strong>Hall of Fame vacío</strong>
          Los líderes aparecerán al guardar partidos.
        </div>
      </div>
    );
  }
  const go = (id: string) => navigate({ name: 'profile', playerId: id });
  return (
    <div className="fame-grid scroll" style={{ flex: 1, minHeight: 0 }}>
      {nonEmpty.map((row) => (
        <div key={row.id} className="card fame-card">
          <div className="label">{row.title}</div>
          <Podium leaders={row.leaders} onPick={go} />
        </div>
      ))}
      {champions.length > 0 && (
        <div className="card fame-card">
          <div className="label">Campeones de temporada</div>
          {champions.map((c) => (
            <button key={c.key} className="fame-leader" onClick={() => go(c.playerId)}>
              <span aria-hidden="true">🏆</span>
              <span className="ellipsis" style={{ flex: 1, textAlign: 'left' }}>
                {players.find((p) => p.id === c.playerId)?.name ?? '?'}
              </span>
              <span className="dim" style={{ fontSize: 11 }}>{seasonLabel(c.key)}</span>
            </button>
          ))}
        </div>
      )}
    </div>
  );
}

function Records() {
  const { players: allPlayers, matches, progression, navigate } = useApp();
  // Los invitados no salen en el ranking.
  const players = useMemo(() => allPlayers.filter((p) => !p.guest), [allPlayers]);
  const records = useMemo(() => computeRecords(players, matches, progression), [players, matches, progression]);
  if (records.length === 0) {
    return (
      <div className="empty">
        <div>
          <strong>Sin récords</strong>
          Se calculan automáticamente a partir del historial.
        </div>
      </div>
    );
  }
  return (
    <div className="list scroll" style={{ flex: 1, minHeight: 0 }}>
      {records.map((r) => (
        <button
          key={r.id}
          className="row"
          onClick={() =>
            r.matchId
              ? navigate({ name: 'matchDetail', matchId: r.matchId })
              : r.playerId && navigate({ name: 'profile', playerId: r.playerId })
          }
        >
          <span style={{ flex: 1 }}>
            <strong>{r.title}</strong>
            <div className="muted" style={{ fontSize: 12 }}>
              {r.holder}
              {r.at ? ` · ${formatDate(r.at)}` : ''}
            </div>
          </span>
          <span style={{ fontSize: 20, fontWeight: 800, color: 'var(--ranked)' }}>{r.value}</span>
        </button>
      ))}
    </div>
  );
}

function Pairs() {
  const { matches, navigate } = useApp();
  const pairs = useMemo(() => pairStats(matches).filter((p) => p.rec.played >= 1), [matches]);
  if (pairs.length === 0) {
    return (
      <div className="empty">
        <div>
          <strong>Sin parejas todavía</strong>
          Juega partidos de 2 contra 2 para ver qué parejas funcionan mejor.
        </div>
      </div>
    );
  }
  return (
    <div className="list scroll" style={{ flex: 1, minHeight: 0 }}>
      {pairs.map((p, i) => (
        <div key={p.key} className="row">
          <span className={`std-pos pos-${i + 1}`} style={{ width: 30 }}>{i + 1}</span>
          <span style={{ flex: 1, fontWeight: 800 }}>
            <button className="link-btn" onClick={() => navigate({ name: 'profile', playerId: p.playerIds[0] })}>{p.names.split(' + ')[0]}</button>
            {' + '}
            <button className="link-btn" onClick={() => navigate({ name: 'profile', playerId: p.playerIds[1] })}>{p.names.split(' + ')[1]}</button>
          </span>
          <span className="muted">{p.rec.played} PJ</span>
          <span>{p.rec.wins}G · {p.rec.played - p.rec.wins}P</span>
          <strong style={{ width: 60, textAlign: 'right', color: 'var(--ranked)' }}>{pct(p.rec.winPct)}</strong>
        </div>
      ))}
      <div className="dim" style={{ fontSize: 11 }}>Ordenado por % de victorias; con pocos partidos es orientativo.</div>
    </div>
  );
}

function Digest() {
  const { matches, players: allPlayers, navigate } = useApp();
  // Los invitados no salen en el ranking.
  const players = useMemo(() => allPlayers.filter((p) => !p.guest), [allPlayers]);
  const [span, setSpan] = useState<'week' | 'month'>('week');
  const now = Date.now();
  const from = now - (span === 'week' ? 7 : 30) * 86_400_000;
  const d = useMemo(() => periodDigest(matches, players, from, now + 1), [matches, players, from, now]);
  const tile = (title: string, body: ReactNode, onClick?: () => void) => (
    <button className="card digest-tile" onClick={onClick} disabled={!onClick}>
      <div className="label">{title}</div>
      <div className="digest-body">{body}</div>
    </button>
  );
  const resultOf = (m: (typeof matches)[number]) =>
    `${m.participants.filter((p) => p.team === 'white').map((p) => p.nameSnapshot).join(' + ')} ${m.result.score.white}–${m.result.score.blue} ${m.participants.filter((p) => p.team === 'blue').map((p) => p.nameSnapshot).join(' + ')}`;
  return (
    <>
      <div className="segmented" style={{ width: 400 }}>
        <button className="seg" style={{ minHeight: 44 }} aria-pressed={span === 'week'} onClick={() => setSpan('week')}>ÚLTIMOS 7 DÍAS</button>
        <button className="seg" style={{ minHeight: 44 }} aria-pressed={span === 'month'} onClick={() => setSpan('month')}>ÚLTIMOS 30 DÍAS</button>
      </div>
      {d.matches === 0 ? (
        <div className="empty">
          <div>
            <strong>Sin partidos en este periodo</strong>
            El resumen se rellena solo al jugar.
          </div>
        </div>
      ) : (
        <div className="digest-grid scroll">
          {tile(span === 'week' ? 'Jugador de la semana' : 'Jugador del mes', d.playerOfPeriod ? (
            <><strong>{d.playerOfPeriod.name}</strong><span className="muted"> · {d.playerOfPeriod.wins} victorias en {d.playerOfPeriod.played}</span></>
          ) : '—', d.playerOfPeriod ? () => navigate({ name: 'profile', playerId: d.playerOfPeriod!.playerId }) : undefined)}
          {tile('Más activo', d.mostActive ? (<><strong>{d.mostActive.name}</strong><span className="muted"> · {d.mostActive.played} partidos</span></>) : '—')}
          {tile('Partidos · goles', <strong>{d.matches} · {d.goals}</strong>)}
          {tile('Partido más igualado', d.closest ? resultOf(d.closest) : '—', d.closest ? () => navigate({ name: 'matchDetail', matchId: d.closest!.id }) : undefined)}
          {tile('Mayor paliza', d.biggest ? resultOf(d.biggest) : '—', d.biggest ? () => navigate({ name: 'matchDetail', matchId: d.biggest!.id }) : undefined)}
          {tile('Pichichi', d.topScorer ? (<><strong>{d.topScorer.name}</strong><span className="muted"> · {d.topScorer.goals} goles</span></>) : 'Sin goleadores asignados')}
        </div>
      )}
    </>
  );
}

function Duel() {
  const { players: allPlayers, matches, progression, navigate } = useApp();
  // Los invitados no salen en el ranking.
  const players = useMemo(() => allPlayers.filter((p) => !p.guest), [allPlayers]);
  const list = sortPlayers(players);
  const [a, setA] = useState(list[0]?.id ?? '');
  const [b, setB] = useState(list[1]?.id ?? '');
  const sa = useMemo(() => (a ? computePlayerStats(a, matches) : null), [a, matches]);
  const sb = useMemo(() => (b ? computePlayerStats(b, matches) : null), [b, matches]);
  const d = useMemo(() => (a && b && a !== b ? duel(a, b, matches) : null), [a, b, matches]);
  if (list.length < 2) {
    return <div className="empty"><div><strong>Faltan jugadores</strong>Se necesitan al menos dos.</div></div>;
  }
  const pa = progression?.players.get(a);
  const pb = progression?.players.get(b);
  const row = (label: string, va: ReactNode, vb: ReactNode, better?: 'a' | 'b' | null) => (
    <div className="duel-row">
      <span className={better === 'a' ? 'win' : ''}>{va}</span>
      <span className="label">{label}</span>
      <span className={better === 'b' ? 'win' : ''}>{vb}</span>
    </div>
  );
  const cmp = (x: number | null | undefined, y: number | null | undefined) =>
    x == null || y == null || x === y ? null : x > y ? 'a' : 'b';
  const pa_ = players.find((p) => p.id === a);
  const pb_ = players.find((p) => p.id === b);
  return (
    <div className="duel scroll">
      <div className="duel-head">
        <select className="input" value={a} onChange={(e) => setA(e.target.value)} aria-label="Jugador A">
          {list.map((p) => <option key={p.id} value={p.id}>{p.name}</option>)}
        </select>
        <div className="duel-vs">
          {d ? <><strong>{d.aWins}</strong> – <strong>{d.bWins}</strong></> : 'VS'}
          <div className="dim" style={{ fontSize: 11 }}>{d ? `${d.played} enfrentamientos` : 'Elige dos jugadores distintos'}</div>
        </div>
        <select className="input" value={b} onChange={(e) => setB(e.target.value)} aria-label="Jugador B">
          {list.map((p) => <option key={p.id} value={p.id}>{p.name}</option>)}
        </select>
      </div>
      <div className="duel-faces">
        <button className="link-btn" onClick={() => navigate({ name: 'profile', playerId: a })}><Avatar name={pa_?.name ?? '?'} photo={pa_?.photo} size={52} /></button>
        <button className="link-btn" onClick={() => navigate({ name: 'profile', playerId: b })}><Avatar name={pb_?.name ?? '?'} photo={pb_?.photo} size={52} /></button>
      </div>
      {sa && sb && (
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
      )}
    </div>
  );
}
