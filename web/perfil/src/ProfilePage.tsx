/**
 * Perfil público de un jugador, para el móvil: se llega leyendo el QR de su ficha en el marcador
 * («…/perfil/#CODIGO»). Solo enseña lo que el futbolín ha publicado; aquí no se cambia nada.
 */
import { useEffect, useMemo, useState, type ReactNode } from 'react';
import { fetchProfile, supabaseConn, tokenFromLocation } from '../../../src/services/profiles/publish';
import type { ProfileSnapshot, SnapshotMatch, SnapshotRival } from '../../../src/services/profiles/snapshot';

const images = import.meta.glob('../../../src/assets/images/{copa,escudo,banner,evento}-*.{webp,png}', {
  eager: true,
  query: '?url',
  import: 'default',
}) as Record<string, string>;
const byName = new Map(Object.entries(images).map(([p, url]) => [p.split('/').pop()!.replace(/\.(webp|png)$/, ''), url]));
const img = (id?: string) => (id ? (/^(https?:|data:)/.test(id) ? id : byName.get(id)) : undefined);

const MODE: Record<string, string> = { quick: 'Rápido', chaos: 'Loco', ranked: 'Clasificatorio' };
const FORMAT: Record<string, string> = { league: 'Liga', bracket: 'Copa', pool: 'Pool' };
const RESULT: Record<string, string> = { G: 'Victoria', P: 'Derrota', E: 'Empate' };
const KIND: Record<string, string> = { copa: 'Copa', escudo: 'Escudo', fondo: 'Banner' };

const pct = (v: number | null | undefined) => (v === null || v === undefined ? '—' : `${Math.round(v)} %`);
const fecha = (t: number) => new Date(t).toLocaleDateString('es-ES', { day: 'numeric', month: 'short', year: 'numeric' });
const fechaHora = (t: number) =>
  new Date(t).toLocaleString('es-ES', { day: 'numeric', month: 'short', hour: '2-digit', minute: '2-digit' });
const duracion = (ms: number) => {
  const h = Math.floor(ms / 3600000);
  const m = Math.round((ms % 3600000) / 60000);
  return h ? `${h} h ${m} min` : `${m} min`;
};
const initials = (name: string) =>
  name
    .split(/\s+/)
    .filter(Boolean)
    .slice(0, 2)
    .map((w) => w[0]!.toUpperCase())
    .join('');

function Avatar({ name, photo, size = 40 }: { name: string; photo?: string; size?: number }) {
  return (
    <span className="av" style={{ width: size, height: size, fontSize: size * 0.38 }} aria-hidden="true">
      {photo ? <img src={photo} alt="" /> : initials(name)}
    </span>
  );
}

function Tile({ k, v, hint }: { k: string; v: ReactNode; hint?: string }) {
  return (
    <div className="tile">
      <span className="tile-v">{v}</span>
      <span className="tile-k">{k}</span>
      {hint && <span className="tile-h">{hint}</span>}
    </div>
  );
}

function Bar({ label, played, winPct }: { label: string; played: number; winPct: number | null }) {
  return (
    <div className="bar">
      <span className="bar-l">{label}</span>
      <span className="bar-t">
        <span style={{ width: `${winPct ?? 0}%` }} />
      </span>
      <span className="bar-v">{played ? `${pct(winPct)} · ${played}` : '—'}</span>
    </div>
  );
}

type Tab = 'resumen' | 'partidos' | 'rivales' | 'logros';

export function ProfilePage() {
  const token = useMemo(() => tokenFromLocation(location.hash, location.search), []);
  const [state, setState] = useState<{ s: 'cargando' } | { s: 'ok'; p: ProfileSnapshot } | { s: 'error'; msg: string }>({ s: 'cargando' });
  const [tab, setTab] = useState<Tab>('resumen');

  useEffect(() => {
    const conn = supabaseConn();
    if (!token) return setState({ s: 'error', msg: 'Este enlace no es de ningún perfil. Vuelve a leer el QR del jugador en el marcador.' });
    if (!conn) return setState({ s: 'error', msg: 'Esta web no está conectada al servidor de perfiles.' });
    fetchProfile(conn, token)
      .then((p) =>
        setState(
          p
            ? { s: 'ok', p }
            : { s: 'error', msg: 'Este perfil todavía no se ha publicado. Se sube solo cuando el marcador tiene internet: prueba dentro de un rato.' },
        ),
      )
      .catch(() => setState({ s: 'error', msg: 'No se ha podido cargar el perfil. Comprueba la conexión e inténtalo de nuevo.' }));
  }, [token]);

  useEffect(() => {
    if (state.s === 'ok') document.title = `${state.p.player.name} · Marcador Futbolín`;
  }, [state]);

  if (state.s !== 'ok') {
    return (
      <main className="page center">
        <div className="brand">MARCADOR FUTBOLÍN</div>
        {state.s === 'cargando' ? <div className="spinner" aria-label="Cargando" /> : <p className="msg">{state.msg}</p>}
      </main>
    );
  }

  const p = state.p;
  const person = (id: string) => p.people[id] ?? { name: '¿?' };
  const g = p.stats.general;
  const champion = p.palmares.filter((t) => t.champion);

  return (
    <main className="page">
      <header className="hero" style={{ ['--cat' as string]: p.player.category?.color ?? '#62d6ff' }}>
        <div className="hero-photo">
          <Avatar name={p.player.name} photo={p.player.photo} size={92} />
          {p.player.level !== undefined && <span className="hero-lvl">{p.player.level}</span>}
        </div>
        <div className="hero-info">
          <h1>{p.player.name}</h1>
          {p.player.alias && <div className="alias">«{p.player.alias}»</div>}
          {p.player.title && <div className="ptitle">{p.player.title}</div>}
          <div className="hero-chips">
            {p.player.category && <span className="chip cat">{p.player.category.name}</span>}
            {p.player.elo !== undefined && <span className="chip">ELO {p.player.elo}</span>}
            {champion.length > 0 && <span className="chip gold">🏆 {champion.length}</span>}
          </div>
        </div>
      </header>

      <nav className="tabs" role="tablist">
        {(
          [
            ['resumen', 'Resumen'],
            ['partidos', 'Partidos'],
            ['rivales', 'Rivales'],
            ['logros', 'Logros'],
          ] as [Tab, string][]
        ).map(([id, label]) => (
          <button key={id} role="tab" aria-selected={tab === id} className={tab === id ? 'on' : ''} onClick={() => setTab(id)}>
            {label}
          </button>
        ))}
      </nav>

      {tab === 'resumen' && (
        <>
          <section className="tiles">
            <Tile k="Partidos" v={g.played} />
            <Tile k="Victorias" v={g.wins} hint={pct(g.winPct)} />
            <Tile k="Goles apuntados" v={p.stats.personalGoals} />
            <Tile k="Mejor racha" v={p.stats.bestWinStreak} hint="victorias seguidas" />
            <Tile
              k="Racha actual"
              v={p.stats.streak.type ? `${p.stats.streak.count} ${p.stats.streak.type === 'G' ? 'G' : 'P'}` : '—'}
            />
            <Tile k="Tiempo jugado" v={duracion(p.stats.playTimeMs)} />
          </section>
          {p.stats.recent.length > 0 && (
            <div className="form">
              Últimos:{' '}
              {p.stats.recent.map((r, i) => (
                <span key={i} className={`fc fc-${r}`}>
                  {r}
                </span>
              ))}
            </div>
          )}

          <h2>Palmarés</h2>
          <Palmares list={p.palmares} />

          {p.rewards.length > 0 && (
            <>
              <h2>Premios de eventos</h2>
              <div className="rewards">
                {p.rewards.map((r, i) => (
                  <div key={i} className={`reward ${r.tipo === 'fondo' ? 'is-wide' : ''}`}>
                    {img(r.archivo) ? <img src={img(r.archivo)} alt="" /> : <span>🎁</span>}
                    <small>
                      {r.name} · {KIND[r.tipo] ?? r.tipo}
                    </small>
                  </div>
                ))}
              </div>
            </>
          )}

          <h2>Cómo juega</h2>
          <div className="card">
            <div className="label">Por modalidad</div>
            {(['quick', 'chaos', 'ranked'] as const).map((m) => (
              <Bar key={m} label={MODE[m]} played={p.stats.byMode[m]?.played ?? 0} winPct={p.stats.byMode[m]?.winPct ?? null} />
            ))}
            <div className="label">Lado de la mesa</div>
            <Bar label="Blanco" played={p.sides.white.played} winPct={p.sides.white.winPct} />
            <Bar label="Azul" played={p.sides.blue.played} winPct={p.sides.blue.winPct} />
            <div className="label">Franja horaria</div>
            {p.habits.byDayPart.map((d) => (
              <Bar key={d.label} label={d.label} played={d.rec.played} winPct={d.rec.winPct} />
            ))}
            {p.habits.favoriteDay && <p className="muted small">Día que más juega: {p.habits.favoriteDay}.</p>}
          </div>
          <div className="tiles">
            <Tile k="Goles equipo a favor" v={g.goalsFor} />
            <Tile k="En contra" v={g.goalsAgainst} />
            <Tile k="Diferencia" v={g.goalDiff > 0 ? `+${g.goalDiff}` : g.goalDiff} />
          </div>
        </>
      )}

      {tab === 'partidos' &&
        (p.matches.length === 0 ? (
          <p className="muted">Sin partidos todavía.</p>
        ) : (
          <div className="matches">
            {p.matches.map((m, i) => (
              <MatchRow key={i} m={m} person={person} />
            ))}
            <p className="muted small">Últimos {p.matches.length} partidos.</p>
          </div>
        ))}

      {tab === 'rivales' && (
        <>
          <div className="duo">
            <Highlight label="Rival favorito" id={p.favoriteRival} list={p.rivals} person={person} empty="Aún sin rival favorito" />
            <Highlight label="Némesis" id={p.nemesis} list={p.rivals} person={person} nemesis empty="Aún sin némesis" />
          </div>
          <h2>Contra quién juega</h2>
          <People list={p.rivals} person={person} empty="Sin enfrentamientos todavía." />
          {p.teammates.length > 0 && (
            <>
              <h2>Con quién juega</h2>
              <People list={p.teammates} person={person} empty="" />
            </>
          )}
        </>
      )}

      {tab === 'logros' &&
        (p.achievements.length === 0 ? (
          <p className="muted">Aún no tiene logros.</p>
        ) : (
          <div className="achs">
            {p.achievements.map((a) => (
              <div key={a.id} className={`ach r-${a.rarity}`}>
                <span className="ach-ico">{a.icon}</span>
                <span>
                  <b>{a.name}</b>
                  <small>{fecha(a.at)}</small>
                </span>
              </div>
            ))}
          </div>
        ))}

      <footer className="foot">
        Actualizado {fechaHora(p.generatedAt)} · Jugando desde {fecha(p.player.since)}
      </footer>
    </main>
  );
}

function Faces({ ids, person }: { ids: string[]; person: (id: string) => { name: string; photo?: string } }) {
  return (
    <span className="faces">
      {ids.map((id) => (
        <span key={id} className="face" title={person(id).name}>
          <Avatar name={person(id).name} photo={person(id).photo} size={26} />
          <small>{person(id).name}</small>
        </span>
      ))}
    </span>
  );
}

function MatchRow({ m, person }: { m: SnapshotMatch; person: (id: string) => { name: string; photo?: string } }) {
  return (
    <div className={`match m-${m.result}`}>
      <div className="match-top">
        <span className={`fc fc-${m.result}`}>{m.result}</span>
        <b className="score">
          {m.mine}–{m.theirs}
        </b>
        <span className="muted small">
          {RESULT[m.result]}
          {m.note ? ` · ${m.note}` : ''}
        </span>
        <span className="muted small right">{fechaHora(m.at)}</span>
      </div>
      <div className="match-meta small muted">
        {m.tournament ? `${FORMAT[m.tournament.format]} · ${m.tournament.name}` : MODE[m.mode] ?? m.mode}
        {m.goals > 0 && ` · ${m.goals} gol${m.goals === 1 ? '' : 'es'} suyo${m.goals === 1 ? '' : 's'}`}
      </div>
      <div className="match-people">
        {m.mates.length > 0 && (
          <span>
            <small className="muted">Con</small> <Faces ids={m.mates} person={person} />
          </span>
        )}
        <span>
          <small className="muted">Contra</small> <Faces ids={m.rivals} person={person} />
        </span>
      </div>
    </div>
  );
}

function Highlight({
  label,
  id,
  list,
  person,
  nemesis = false,
  empty,
}: {
  label: string;
  id: string | null;
  list: SnapshotRival[];
  person: (id: string) => { name: string; photo?: string };
  /** Némesis: se dice cuánto te gana; favorito: cuánto le ganas. */
  nemesis?: boolean;
  empty: string;
}) {
  const r = id ? list.find((x) => x.id === id) : undefined;
  return (
    <div className="card hl">
      <div className="label">{label}</div>
      {id ? (
        <div className="hl-who">
          <Avatar name={person(id).name} photo={person(id).photo} size={48} />
          <span>
            <b>{person(id).name}</b>
            {r && (
              <small>
                {nemesis ? `Te gana el ${Math.round((r.losses / r.played) * 100)} %` : `Le ganas el ${r.winPct} %`} de {r.played} partidos
              </small>
            )}
          </span>
        </div>
      ) : (
        <small className="muted">{empty}</small>
      )}
    </div>
  );
}

function People({ list, person, empty }: { list: SnapshotRival[]; person: (id: string) => { name: string; photo?: string }; empty: string }) {
  if (list.length === 0) return <p className="muted">{empty}</p>;
  return (
    <div className="people">
      {list.map((r) => (
        <div key={r.id} className="prow">
          <Avatar name={person(r.id).name} photo={person(r.id).photo} size={40} />
          <span className="prow-name">
            <b>{person(r.id).name}</b>
            <small>{r.played} partidos</small>
          </span>
          <span className="prow-rec">
            {r.wins}G{r.draws ? ` · ${r.draws}E` : ''} · {r.losses}P
          </span>
          <b className="prow-pct">{r.winPct} %</b>
        </div>
      ))}
    </div>
  );
}

/** Torneos jugados: lo más reciente arriba, con filtros por tipo y «solo ganados» (igual que en la app). */
function Palmares({ list }: { list: ProfileSnapshot['palmares'] }) {
  const [format, setFormat] = useState<string>('all');
  const [onlyWon, setOnlyWon] = useState(false);
  if (list.length === 0) return <p className="muted">Todavía no ha jugado ningún torneo.</p>;
  const count = (f: string) => list.filter((t) => t.format === f).length;
  const won = list.filter((t) => t.champion).length;
  const shown = list.filter((t) => (format === 'all' || t.format === format) && (!onlyWon || t.champion));
  const plural: Record<string, string> = { league: 'Ligas', bracket: 'Copas', pool: 'Pools' };
  return (
    <>
      <div className="pal-filters">
        <button aria-pressed={format === 'all'} onClick={() => setFormat('all')}>
          Todos <b>{list.length}</b>
        </button>
        {['league', 'bracket', 'pool']
          .filter((f) => count(f) > 0)
          .map((f) => (
            <button key={f} aria-pressed={format === f} onClick={() => setFormat(format === f ? 'all' : f)}>
              {plural[f]} <b>{count(f)}</b>
            </button>
          ))}
        {won > 0 && (
          <button className="pal-won" aria-pressed={onlyWon} onClick={() => setOnlyWon((v) => !v)}>
            🏆 Ganados <b>{won}</b>
          </button>
        )}
      </div>
      {shown.length === 0 ? (
        <p className="muted">Ningún torneo con este filtro.</p>
      ) : (
        <div className="palmares">
          {shown.map((t, i) => (
            <div key={i} className={`honor ${t.champion ? 'is-champ' : ''}`}>
              {img(t.banner) && <img className="honor-banner" src={img(t.banner)} alt="" />}
              {t.champion && <span className="honor-ribbon">🏆 CAMPEÓN</span>}
              <span className="honor-art">{img(t.logo) ? <img src={img(t.logo)} alt="" /> : '⚽'}</span>
              <span className="honor-txt">
                <b>{t.name}</b>
                <small>
                  {FORMAT[t.format]} · {t.status === 'active' ? 'En juego' : fecha(t.at)}
                </small>
              </span>
              <span className="honor-place">
                {t.status === 'active' ? (
                  <span className="honor-live">EN JUEGO</span>
                ) : t.place ? (
                  <>
                    <b>{t.place}º</b>
                    <small>de {t.of}</small>
                  </>
                ) : null}
              </span>
              <span className={`honor-cup ${t.champion ? '' : 'is-dim'}`}>{img(t.cup) ? <img src={img(t.cup)} alt="" /> : '🏆'}</span>
            </div>
          ))}
        </div>
      )}
    </>
  );
}
