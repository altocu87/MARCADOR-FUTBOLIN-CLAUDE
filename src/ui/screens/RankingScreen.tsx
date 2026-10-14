import { useMemo, useState, type ReactNode } from 'react';
import { useApp } from '../../app/AppContext';
import type { RankingTab } from '../../app/routes';
import type { MatchMode } from '../../match-engine';
import { computeHallOfFame, computeRecords, seasonChampions, seasonProgression } from '../../services/progression';
import { sortPlayers } from '../../services/players';
import {
  computePlayerStats,
  pairStats,
  periodDigest,
  seasonKey,
  seasonLabel,
  sortMatches,
} from '../../services/statistics';
import { Avatar, FormChips, HelpButton, Modal, ScreenFrame, Tabs, formatDate, pct } from '../components/common';
import { CategoryBadge, Podium } from '../components/graphics';
import { MatchLine, PlayerTag, SideTag } from '../components/PlayerFaces';

const PAGE = 20;
/** Modalidad en la fila del historial: corta, cabe en su columna. */
const MODE_SHORT: Record<MatchMode, string> = { quick: 'Rápido', chaos: 'Loco', ranked: 'Clasificatorio' };

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
          { id: 'players', label: 'Jugadores' },
          { id: 'history', label: 'Historial' },
          { id: 'fame', label: 'Salón de la fama' },
          { id: 'digest', label: 'Resumen' },
          { id: 'pairs', label: 'Parejas' },
        ]}
      />
      {tab === 'standings' && <Standings />}
      {tab === 'history' && <History />}
      {tab === 'players' && <PlayersGrid />}
      {tab === 'fame' && <HallOfFame />}
      {tab === 'pairs' && <Pairs />}
      {tab === 'digest' && <Digest />}
    </ScreenFrame>
  );
}

function Standings() {
  const { players, matches, progression, navigate, prefs } = useApp();
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
        <HelpButton
          title="ELO"
          text="En la temporada, todos empiezan en 1200 y se cuenta solo lo jugado en ella. En «Histórico», el ELO acumulado de siempre."
          className="head-help"
        />
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
    </>
  );
}

function History() {
  const { matches, players, navigate } = useApp();
  const [mode, setMode] = useState<MatchMode | 'all'>('all');
  const [playerId, setPlayerId] = useState<string>('all');
  const [page, setPage] = useState(0);
  // Elegir jugador: ventana con las caras de todos.
  const [picking, setPicking] = useState(false);
  const who = players.find((p) => p.id === playerId);
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

  return (
    <>
      <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
        <div className="segmented hist-modes" role="group" aria-label="Filtrar por modalidad">
          {(
            [
              ['all', 'TODOS'],
              ['quick', 'RÁPIDO'],
              ['chaos', 'LOCO'],
              ['ranked', 'CLASIFICATORIO'],
            ] as const
          ).map(([v, label]) => (
            <button key={v} className="seg" aria-pressed={mode === v} onClick={() => { setMode(v); setPage(0); }}>
              {label}
            </button>
          ))}
        </div>
        <button className="btn hist-who" onClick={() => setPicking(true)} aria-label="Filtrar por jugador">
          {who ? <PlayerTag id={who.id} name={who.name} size={30} /> : <span>👥 Jugador: todos</span>}
        </button>
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
              <span className={`hist-mode mode-${m.config.mode}`}>{MODE_SHORT[m.config.mode]}</span>
              <span className={`hist-name ${m.result.winner === 'white' ? 'win' : ''}`}><SideTag match={m} team="white" /></span>
              <span className="hist-score">
                {m.result.score.white}–{m.result.score.blue}
              </span>
              <span className={`hist-name right ${m.result.winner === 'blue' ? 'win' : ''}`}><SideTag match={m} team="blue" right /></span>
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
      {picking && (
        <Modal
          title="¿De quién?"
          onClose={() => setPicking(false)}
          actions={
            <button className="btn btn-primary" onClick={() => { setPlayerId('all'); setPage(0); setPicking(false); }}>
              Todos los jugadores
            </button>
          }
        >
          <div className="who-grid">
            {sortPlayers(players).map((p) => (
              <button key={p.id} className={`who-cell${p.id === playerId ? ' is-on' : ''}`} onClick={() => { setPlayerId(p.id); setPage(0); setPicking(false); }}>
                <Avatar name={p.name} photo={p.photo} size={56} />
                <span>{p.name}</span>
              </button>
            ))}
          </div>
        </Modal>
      )}
    </>
  );
}

function PlayersGrid() {
  const { players, progression, navigate } = useApp();
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
    <div className="pl-grid scroll" style={{ flex: 1, minHeight: 0 }}>
      {list.map((p) => {
        const prog = progression?.players.get(p.id);
        return (
          <button
            key={p.id}
            className={`pl-card${p.active ? '' : ' is-off'}`}
            style={prog ? { ['--cat' as string]: prog.category.color } : undefined}
            onClick={() => navigate({ name: 'profile', playerId: p.id })}
          >
            <Avatar name={p.name} photo={p.photo} size={64} />
            <span className="pl-info">
              <b className="pl-name">{p.name}</b>
              <span className="pl-meta">{!p.active ? 'Inactivo' : prog ? `Nivel ${prog.level}` : 'Sin partidos'}</span>
              {p.active && prog && prog.rankedPlayed > 0 && (
                <span className="pl-rank">
                  <CategoryBadge category={prog.category} size={22} /> {prog.category.name} · {prog.elo}
                </span>
              )}
            </span>
          </button>
        );
      })}
    </div>
  );
}

function HallOfFame() {
  const { players, matches, progression, navigate, prefs } = useApp();
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
          <strong>Salón de la fama vacío</strong>
          Los líderes aparecerán al guardar partidos.
        </div>
      </div>
    );
  }
  const go = (id: string) => navigate({ name: 'profile', playerId: id });
  return (
    <div className="fame-page scroll" style={{ flex: 1, minHeight: 0 }}>
    <div className="fame-grid">
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
                <PlayerTag id={c.playerId} size={30} />
              </span>
              <span className="dim" style={{ fontSize: 13 }}>{seasonLabel(c.key)}</span>
            </button>
          ))}
        </div>
      )}
    </div>
    <div className="label">Récords de partido</div>
    <Records />
    </div>
  );
}

/** Récords de partido (dentro del Salón de la fama). */
function Records() {
  const { players, matches, progression, navigate } = useApp();
  // Solo los de partido: los de jugador (racha, victorias, ELO…) ya salen en los podios de arriba.
  const records = useMemo(() => computeRecords(players, matches, progression).filter((r) => r.matchId), [players, matches, progression]);
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
    <div className="list">
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
          <span style={{ flex: 1, minWidth: 0 }}>
            <strong>{r.title}</strong>
            <div className="muted rec-holder" style={{ fontSize: 13 }}>
              {r.playerId ? (
                <PlayerTag id={r.playerId} size={26} />
              ) : r.matchId && matches.find((x) => x.id === r.matchId) ? (
                <MatchLine match={matches.find((x) => x.id === r.matchId)!} size={22} />
              ) : (
                r.holder
              )}
              {r.at ? <span className="dim">{formatDate(r.at)}</span> : null}
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
          Juega partidos de 2 vs 2 para ver qué parejas funcionan mejor.
        </div>
      </div>
    );
  }
  return (
    <div className="list scroll" style={{ flex: 1, minHeight: 0 }}>
      {pairs.map((p, i) => (
        <div key={p.key} className="row">
          <span className={`std-pos pos-${i + 1}`} style={{ width: 30 }}>{i + 1}</span>
          <span className="pair-who" style={{ flex: 1, fontWeight: 800 }}>
            <button className="link-btn" onClick={() => navigate({ name: 'profile', playerId: p.playerIds[0] })}><PlayerTag id={p.playerIds[0]} name={p.names.split(' + ')[0]} size={30} /></button>
            <span className="pair-plus">+</span>
            <button className="link-btn" onClick={() => navigate({ name: 'profile', playerId: p.playerIds[1] })}><PlayerTag id={p.playerIds[1]} name={p.names.split(' + ')[1]} size={30} /></button>
          </span>
          <span className="muted">{p.rec.played} PJ</span>
          <span>{p.rec.wins}G{p.rec.draws ? ` · ${p.rec.draws}E` : ''} · {p.rec.losses}P</span>
          <strong style={{ width: 60, textAlign: 'right', color: 'var(--ranked)' }}>{pct(p.rec.winPct)}</strong>
        </div>
      ))}
    </div>
  );
}

function Digest() {
  const { matches, players, navigate } = useApp();
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
  const resultOf = (m: (typeof matches)[number]) => <MatchLine match={m} size={26} />;
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
            <PlayerTag id={d.playerOfPeriod.playerId} name={d.playerOfPeriod.name} size={44}><span className="muted">{d.playerOfPeriod.wins} victorias en {d.playerOfPeriod.played}</span></PlayerTag>
          ) : '—', d.playerOfPeriod ? () => navigate({ name: 'profile', playerId: d.playerOfPeriod!.playerId }) : undefined)}
          {tile('Más activo', d.mostActive ? (<PlayerTag id={d.mostActive.playerId} name={d.mostActive.name} size={44}><span className="muted">{d.mostActive.played} partidos</span></PlayerTag>) : '—')}
          {tile('Partidos · goles', <strong>{d.matches} · {d.goals}</strong>)}
          {tile('Partido más igualado', d.closest ? resultOf(d.closest) : '—', d.closest ? () => navigate({ name: 'matchDetail', matchId: d.closest!.id }) : undefined)}
          {tile('Mayor paliza', d.biggest ? resultOf(d.biggest) : '—', d.biggest ? () => navigate({ name: 'matchDetail', matchId: d.biggest!.id }) : undefined)}
          {tile('Pichichi', d.topScorer ? (<PlayerTag id={d.topScorer.playerId} name={d.topScorer.name} size={44}><span className="muted">{d.topScorer.goals} goles</span></PlayerTag>) : 'Faltan goles por apuntar')}
        </div>
      )}
    </>
  );
}

