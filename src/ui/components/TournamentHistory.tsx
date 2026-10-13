/**
 * Torneos de un jugador (pestaña «Torneos» del perfil): del más reciente al más antiguo, con filtros por
 * tipo y «solo ganados». Cada torneo con su banner, escudo, copa y el puesto en que quedó; el campeón,
 * con marco dorado.
 */
import { useMemo, useState } from 'react';
import type { StoredMatch, Tournament, TournamentFormat } from '../../services/persistence';
import { FORMAT_LABEL, competitionName, playerTournaments } from '../../services/tournaments';
import { BannerArt, Crest, Cup, defaultBanner, defaultCup, defaultLogo } from './Crest';

const FORMATS: TournamentFormat[] = ['league', 'bracket', 'pool'];
const PLURAL: Record<TournamentFormat, string> = { league: 'Ligas', bracket: 'Copas', pool: 'Pools' };

const shortDate = (ts: number) => new Date(ts).toLocaleDateString('es-ES', { day: 'numeric', month: 'short', year: 'numeric' });

export function TournamentHistory({
  playerId,
  tournaments,
  matches,
  onOpen,
}: {
  playerId: string;
  tournaments: Tournament[];
  matches: StoredMatch[];
  onOpen: (id: string) => void;
}) {
  const all = useMemo(() => playerTournaments(playerId, tournaments, matches), [playerId, tournaments, matches]);
  const [format, setFormat] = useState<TournamentFormat | 'all'>('all');
  const [onlyWon, setOnlyWon] = useState(false);
  const list = all.filter((x) => (format === 'all' || x.tournament.format === format) && (!onlyWon || x.champion));
  const count = (f: TournamentFormat) => all.filter((x) => x.tournament.format === f).length;
  const won = all.filter((x) => x.champion).length;

  if (all.length === 0) return <div className="muted">No ha participado en torneos.</div>;

  return (
    <div className="th">
      <div className="th-filters" role="group" aria-label="Filtrar torneos">
        <button className="th-filter" aria-pressed={format === 'all'} onClick={() => setFormat('all')}>
          Todos <b>{all.length}</b>
        </button>
        {FORMATS.filter((f) => count(f) > 0).map((f) => (
          <button key={f} className="th-filter" aria-pressed={format === f} onClick={() => setFormat(format === f ? 'all' : f)}>
            {PLURAL[f]} <b>{count(f)}</b>
          </button>
        ))}
        <button className="th-filter th-filter-won" aria-pressed={onlyWon} onClick={() => setOnlyWon((v) => !v)} disabled={won === 0}>
          🏆 Ganados <b>{won}</b>
        </button>
      </div>
      {list.length === 0 ? (
        <div className="muted">Ningún torneo con este filtro.</div>
      ) : (
        <div className="th-list">
          {list.map(({ tournament: t, at, place, of, champion }) => {
            const name = competitionName(t);
            const banner = t.banner ?? defaultBanner(name, t.format);
            return (
              <button key={t.id} className={`th-card${champion ? ' is-champion' : ''}${t.status === 'active' ? ' is-live' : ''}`} onClick={() => onOpen(t.id)}>
                <BannerArt id={banner} className="th-banner" />
                {champion && <span className="th-ribbon">🏆 CAMPEÓN</span>}
                <Crest id={t.logo ?? defaultLogo(name, t.format)} size={54} className="th-crest" />
                <span className="th-info">
                  <b className="th-name">{name}</b>
                  <span className="th-sub">
                    {t.edition ? `${t.edition}ª edición · ` : ''}
                    {FORMAT_LABEL[t.format]} · {t.status === 'active' ? 'en juego' : shortDate(at)}
                  </span>
                </span>
                <span className="th-place">
                  {t.status === 'active' ? (
                    <span className="th-live">EN JUEGO</span>
                  ) : place ? (
                    <>
                      <b>{place}º</b>
                      <small>de {of}</small>
                    </>
                  ) : null}
                </span>
                <Cup id={t.cup ?? defaultCup()} size={champion ? 58 : 46} className={`th-cup${champion ? '' : ' is-dim'}`} />
              </button>
            );
          })}
        </div>
      )}
    </div>
  );
}
