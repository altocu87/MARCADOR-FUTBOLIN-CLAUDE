import { useMemo, useState } from 'react';
import { useApp } from '../../app/AppContext';
import {
  ACHIEVEMENTS,
  CATEGORY_LABEL,
  RARITY_LABEL,
  availableTitles,
  categoryFor,
  displayTitle,
  levelProgress,
  xpForLevel,
  type AchievementCategory,
} from '../../services/progression';
import { ANTHEMS, sound } from '../../services/sound/sound';
import {
  RIVAL_MIN_MATCHES,
  computePlayerStats,
  formatDuration,
  habits,
  personalGoals,
  resultFor,
  sideStats,
  sortMatches,
  type RivalStat,
  type StatBlock,
} from '../../services/statistics';
import { Avatar, FormChips, MODE_LABEL, ScreenFrame, StatTile, Tabs, formatDate, pct } from '../components/common';
import { AchievementIcon, CategoryBadge, EloChart } from '../components/graphics';
import { resultLine } from '../components/MatchReport';
import { PlayerOptions } from '../components/PlayerQr';
import { TournamentHistory } from '../components/TournamentHistory';

type ProfileTab = 'general' | 'ranked' | 'habits' | 'tournaments' | 'rivals' | 'achievements' | 'history' | 'style';

function Block({ b }: { b: StatBlock }) {
  return (
    <div className="grid-4">
      <StatTile k="Partidos" v={b.played} />
      <StatTile k="Victorias" v={b.wins} />
      <StatTile k="Derrotas" v={b.losses} />
      <StatTile k="% victorias" v={pct(b.winPct)} />
      <StatTile k="Goles equipo a favor" v={b.goalsFor} />
      <StatTile k="Goles equipo en contra" v={b.goalsAgainst} />
      <StatTile k="Diferencia" v={b.goalDiff > 0 ? `+${b.goalDiff}` : b.goalDiff} />
      <StatTile k="Penaltis a favor/contra" v={`${b.penaltyGoalsFor}/${b.penaltyGoalsAgainst}`} />
    </div>
  );
}

function Bar({ label, played, winPct }: { label: string; played: number; winPct: number | null }) {
  return (
    <div className="hbar">
      <span className="hbar-label">{label}</span>
      <span className="hbar-track">
        <span style={{ width: `${winPct ?? 0}%` }} />
      </span>
      <span className="hbar-value">{played ? `${pct(winPct)} · ${played} PJ` : '—'}</span>
    </div>
  );
}

export function ProfileScreen({ playerId }: { playerId: string }) {
  const { players, matches, progression, navigate, prefs, tournaments, savePlayer, toast } = useApp();
  const [tab, setTab] = useState<ProfileTab>('general');
  const [showQr, setShowQr] = useState(false);
  const player = players.find((p) => p.id === playerId);
  const stats = useMemo(() => computePlayerStats(playerId, matches), [playerId, matches]);
  const sides = useMemo(() => sideStats(playerId, matches), [playerId, matches]);
  const hab = useMemo(() => habits(playerId, matches), [playerId, matches]);
  const goals = useMemo(() => personalGoals(matches).get(playerId) ?? 0, [playerId, matches]);
  const prog = progression?.players.get(playerId);
  const myMatches = useMemo(
    () => sortMatches(matches.filter((m) => m.participants.some((p) => p.playerId === playerId))).reverse(),
    [matches, playerId],
  );

  if (!player) {
    return (
      <ScreenFrame title="Perfil" onBack={() => navigate({ name: 'ranking', tab: 'players' })}>
        <div className="empty">Jugador no encontrado.</div>
      </ScreenFrame>
    );
  }

  const title = displayTitle(prog, player.titleId);
  const category = prog?.category ?? categoryFor(prefs.progression.eloInitial);
  const update = async (patch: Partial<typeof player>) => {
    try {
      await savePlayer({ ...player, ...patch, updatedAt: Date.now() });
    } catch {
      toast('No se pudo guardar');
    }
  };

  const rivalRow = (r: RivalStat) => (
    <div key={r.playerId} className="row">
      <span style={{ flex: 1 }}>{r.name}</span>
      <span className="muted">{r.played} PJ</span>
      <span>{r.wins}G{r.draws ? ` · ${r.draws}E` : ''} · {r.losses}P</span>
      <strong style={{ width: 60, textAlign: 'right' }}>{r.winPct.toFixed(0)} %</strong>
    </div>
  );

  return (
    <ScreenFrame
      title="Perfil"
      onBack={() => navigate({ name: 'ranking', tab: 'players' })}
      right={
        <button className="btn btn-sm" onClick={() => setShowQr(true)} title="Para ver este perfil en el móvil">
          ▦ QR del perfil
        </button>
      }
    >
      {showQr && <PlayerOptions player={player} startWithQr onClose={() => setShowQr(false)} />}
      <div className={`player-card cat-${category.id}`} style={{ ['--cat' as string]: category.color }}>
        <div className="pc-photo">
          <Avatar name={player.name} photo={player.photo} size={72} />
          {prog && <span className="pc-level">{prog.level}</span>}
        </div>
        <div style={{ flex: 1, minWidth: 0 }}>
          <div className="pc-name">
            {player.name} {player.alias && <span className="muted" style={{ fontSize: 14 }}>«{player.alias}»</span>}
            {!player.active && <span className="badge" style={{ marginLeft: 8 }}>Inactivo</span>}
          </div>
          {title && <div className="player-title">{title}</div>}
          {prog ? (
            <>
              <div className="muted" style={{ fontSize: 12 }}>
                Nivel {prog.level} · {prog.xp} XP · siguiente a {xpForLevel(prog.level + 1)} XP
              </div>
              <div className="xpbar" style={{ marginTop: 4, maxWidth: 300 }}>
                <span style={{ width: `${levelProgress(prog.xp) * 100}%` }} />
              </div>
            </>
          ) : (
            <div className="muted" style={{ fontSize: 13 }}>Progresión pendiente (desactivada en Ajustes)</div>
          )}
        </div>
        {prog && (
          <div className="pc-elo">
            <CategoryBadge category={category} size={64} />
            <div>
              <div style={{ color: category.color, fontWeight: 800, fontSize: 13 }}>{category.name}</div>
              <div style={{ fontSize: 24, fontWeight: 900 }}>{prog.elo}</div>
              <div className="dim" style={{ fontSize: 10 }}>ELO · máx {prog.maxElo}</div>
            </div>
          </div>
        )}
      </div>
      <Tabs
        label="Secciones del perfil"
        value={tab}
        onChange={setTab}
        tabs={[
          { id: 'general', label: 'General' },
          { id: 'ranked', label: 'Clasificatorio' },
          { id: 'habits', label: 'Hábitos' },
          { id: 'tournaments', label: 'Torneos' },
          { id: 'rivals', label: 'Rivales' },
          { id: 'achievements', label: `Logros ${prog ? prog.achievements.length : 0}/${ACHIEVEMENTS.length}` },
          { id: 'history', label: 'Historial' },
          { id: 'style', label: 'Estilo' },
        ]}
      />
      <div className="scroll" style={{ flex: 1, minHeight: 0, display: 'flex', flexDirection: 'column', gap: 8 }}>
        {tab === 'general' && (
          <>
            <Block b={stats.general} />
            <div className="grid-4">
              <StatTile
                k="Racha actual"
                v={
                  stats.currentStreak.type
                    ? `${stats.currentStreak.count} ${stats.currentStreak.type === 'G' ? 'victoria' : 'derrota'}${stats.currentStreak.count === 1 ? '' : 's'}`
                    : '—'
                }
              />
              <StatTile k="Mejor racha" v={stats.bestWinStreak} />
              <StatTile k="Tiempo jugado" v={formatDuration(stats.totalPlayTimeMs)} />
              <StatTile
                k="Goles apuntados"
                v={goals}
                help="Los goles que se apuntaron a su nombre. En 1 vs 1 se apuntan solos; en 2 vs 2, solo si alguien toca su ficha al marcar."
              />
            </div>
            <div className="muted" style={{ fontSize: 12 }}>
              Últimos partidos: <FormChips form={stats.recent} />
            </div>
          </>
        )}
        {tab === 'ranked' && (
          <>
            <Block b={stats.ranked} />
            <div className="muted" style={{ fontSize: 13 }}>
              Forma (últimas 5 clasificatorias): <FormChips form={stats.form} empty="Sin clasificatorios" />
            </div>
            {prog && (
              <div className="card">
                <div className="label">Evolución del ELO</div>
                <EloChart points={prog.eloHistory} initial={prefs.progression.eloInitial} />
              </div>
            )}
          </>
        )}
        {tab === 'habits' && (
          <div className="grid-2" style={{ alignItems: 'start' }}>
            <div className="card">
              <div className="label">Lado de la mesa</div>
              <Bar label="Blanco (izq.)" played={sides.white.played} winPct={sides.white.winPct} />
              <Bar label="Azul (dcha.)" played={sides.blue.played} winPct={sides.blue.winPct} />
              <div className="label" style={{ marginTop: 10 }}>Franja horaria</div>
              {hab.byDayPart.map((d) => (
                <Bar key={d.label} label={d.label} played={d.rec.played} winPct={d.rec.winPct} />
              ))}
              <div className="dim" style={{ fontSize: 11, marginTop: 4 }}>
                {hab.bestDayPart ? `Rinde mejor por la ${hab.bestDayPart}.` : 'Mejor franja: se necesitan 3 partidos en alguna.'}
              </div>
            </div>
            <div className="card">
              <div className="label">Días de la semana</div>
              {hab.byWeekday.map((d) => (
                <Bar key={d.label} label={d.label} played={d.rec.played} winPct={d.rec.winPct} />
              ))}
              <div className="dim" style={{ fontSize: 11, marginTop: 4 }}>
                {hab.favoriteDay ? `Día favorito: ${hab.favoriteDay}.` : 'Sin partidos aún.'}
              </div>
            </div>
          </div>
        )}
        {tab === 'tournaments' && (
          <TournamentHistory playerId={playerId} tournaments={tournaments} matches={matches} onOpen={(id) => navigate({ name: 'tournamentDetail', id })} />
        )}
        {tab === 'rivals' && (
          <>
            <div className="grid-2">
              <div className="card">
                <div className="label">Rival favorito</div>
                <div style={{ fontSize: 18, fontWeight: 800 }}>
                  {stats.favoriteRival
                    ? `${stats.favoriteRival.name} · le ganas el ${stats.favoriteRival.winPct.toFixed(0)} %`
                    : 'Aún sin rival favorito'}
                </div>
              </div>
              <div className="card">
                <div className="label">Némesis</div>
                <div style={{ fontSize: 18, fontWeight: 800 }}>
                  {stats.nemesis
                    ? `${stats.nemesis.name} · te gana el ${Math.round((stats.nemesis.losses / stats.nemesis.played) * 100)} %`
                    : 'Aún sin némesis'}
                </div>
              </div>
            </div>
            <div className="dim" style={{ fontSize: 11 }}>
              Requiere al menos {RIVAL_MIN_MATCHES} enfrentamientos.
            </div>
            <div className="label">Enfrentamientos directos</div>
            {stats.rivals.length === 0 ? <div className="muted">Sin enfrentamientos.</div> : <div className="list">{stats.rivals.map(rivalRow)}</div>}
            {stats.teammates.length > 0 && (
              <>
                <div className="label">Compañeros (2 vs 2)</div>
                <div className="list">{stats.teammates.map(rivalRow)}</div>
              </>
            )}
          </>
        )}
        {tab === 'achievements' && (
          <>
            <div className="dim" style={{ fontSize: 11 }}>Los secretos se revelan al conseguirlos.</div>
            {(Object.keys(CATEGORY_LABEL) as AchievementCategory[]).map((cat) => (
              <div key={cat}>
                <div className="label" style={{ margin: '4px 0' }}>{CATEGORY_LABEL[cat]}</div>
                <div className="ach-grid">
                  {ACHIEVEMENTS.filter((a) => a.category === cat).map((a) => {
                    const got = prog?.achievements.find((x) => x.id === a.id);
                    const hidden = a.secret && !got;
                    return (
                      <div key={a.id} className={`ach ${got ? 'got' : ''} rarity-${a.rarity}`}>
                        <AchievementIcon id={a.id} glyph={a.icon} rarity={a.rarity} locked={hidden} />
                        <span style={{ minWidth: 0 }}>
                          <strong>{hidden ? 'Logro secreto' : a.name}</strong>
                          {a.title && !hidden && <span className="player-title" style={{ marginLeft: 6 }}>«{a.title}»</span>}
                          <div className="muted" style={{ fontSize: 11 }}>
                            {hidden ? '???' : a.description} · {RARITY_LABEL[a.rarity]}
                            {got && ` · ${formatDate(got.at)}`}
                          </div>
                        </span>
                      </div>
                    );
                  })}
                </div>
              </div>
            ))}
          </>
        )}
        {tab === 'history' &&
          (myMatches.length === 0 ? (
            <div className="muted">Sin partidos guardados.</div>
          ) : (
            <div className="list">
              {myMatches.slice(0, 50).map((m) => {
                const res = resultFor(m, playerId) ?? 'P';
                return (
                  <button key={m.id} className="row" onClick={() => navigate({ name: 'matchDetail', matchId: m.id })}>
                    <span className={`form-chip ${res}`}>{res}</span>
                    <span className="dim" style={{ width: 120, fontSize: 12 }}>{formatDate(m.finishedAt)}</span>
                    <span className="muted" style={{ width: 110, fontSize: 12 }}>{MODE_LABEL[m.config.mode]}</span>
                    <span style={{ flex: 1 }}>{resultLine(m)}</span>
                  </button>
                );
              })}
            </div>
          ))}
        {tab === 'style' && (
          <div className="grid-2" style={{ alignItems: 'start' }}>
            <div className="card">
              <div className="label">Título visible</div>
              <p className="dim" style={{ fontSize: 12, margin: '4px 0 8px' }}>Se desbloquean con algunos logros y aparecen bajo el nombre.</p>
              <div className="chip-wrap">
                <button className={`pick-chip ${!player.titleId ? 'pick-blue' : ''}`} onClick={() => update({ titleId: undefined })}>Automático</button>
                <button className={`pick-chip ${player.titleId === 'none' ? 'pick-blue' : ''}`} onClick={() => update({ titleId: 'none' })}>Sin título</button>
                {availableTitles(prog).map((t) => (
                  <button key={t.id} className={`pick-chip ${player.titleId === t.id ? 'pick-blue' : ''}`} onClick={() => update({ titleId: t.id })}>
                    {t.title}
                  </button>
                ))}
              </div>
              {availableTitles(prog).length === 0 && <div className="muted" style={{ fontSize: 12, marginTop: 6 }}>Aún no tiene títulos.</div>}
            </div>
            <div className="card">
              <div className="label">Melodía de victoria</div>
              <p className="dim" style={{ fontSize: 12, margin: '4px 0 8px' }}>Suena en la pantalla de victoria cuando gana.</p>
              <div className="chip-wrap">
                {Object.entries(ANTHEMS).map(([id, a]) => (
                  <button
                    key={id}
                    className={`pick-chip ${(player.anthem ?? 'fanfare') === id ? 'pick-blue' : ''}`}
                    onClick={() => {
                      sound.unlock();
                      sound.playAnthem(id);
                      void update({ anthem: id });
                    }}
                  >
                    ♪ {a.label}
                  </button>
                ))}
              </div>
            </div>
          </div>
        )}
      </div>
    </ScreenFrame>
  );
}
