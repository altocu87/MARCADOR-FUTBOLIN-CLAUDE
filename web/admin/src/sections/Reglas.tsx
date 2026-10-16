/**
 * Reglas del juego: ELO, rangos, experiencia y niveles. Cada dato con qué significa, el valor recomendado y por qué,
 * y debajo de cada grupo un ejemplo de cómo queda con los valores escritos. Al publicar, todas las mesas las
 * descargan y recalculan la progresión desde el historial.
 */
import { useEffect, useMemo, useState } from 'react';
import {
  DEFAULT_RULES,
  RULE_GROUPS,
  averageMatchXp,
  eloGain,
  rulesDiff,
  rulesProblems,
  sanitizeRules,
  xpForLevelWith,
  type GameRules,
  type RuleField,
} from '../../../../src/services/progression/gameRules';
import type { StoredRules } from '../backend';
import { useAdmin } from '../store';
import { Badge, ConfirmButton, Header } from '../ui';

const num = (v: number) => String(v).replace('.', ',');
type Value = GameRules[keyof GameRules];
const show = (v: Value, f: RuleField) =>
  typeof v === 'boolean'
    ? v
      ? 'Encendido'
      : 'Apagado'
    : typeof v === 'string'
      ? (f.options?.find((o) => o.value === v)?.label ?? v)
      : `${num(v)}${f.unit ? ` ${f.unit}` : ''}`;
const fecha = (ms: number) => new Date(ms).toLocaleString('es-ES', { day: 'numeric', month: 'long', year: 'numeric', hour: '2-digit', minute: '2-digit' });

export function Reglas() {
  const { backend, can, saveRules, session, toast } = useAdmin();
  const editable = can('editar_reglas');
  const [stored, setStored] = useState<StoredRules | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [r, setR] = useState<GameRules>(DEFAULT_RULES);

  useEffect(() => {
    let alive = true;
    backend
      .loadRules()
      .then((s) => {
        if (!alive) return;
        setStored(s);
        setR(s?.rules ?? DEFAULT_RULES);
      })
      .catch((e: unknown) => alive && setError(e instanceof Error ? e.message : String(e)))
      .finally(() => alive && setLoading(false));
    return () => {
      alive = false;
    };
  }, [backend]);

  const published = stored?.rules ?? DEFAULT_RULES;
  const changes = useMemo(() => rulesDiff(published, r), [published, r]);
  const problems = useMemo(() => rulesProblems(r), [r]);
  const offDefault = useMemo(() => rulesDiff(DEFAULT_RULES, r).length, [r]);

  const set = (key: keyof GameRules, value: Value) => setR((cur) => ({ ...cur, [key]: value }));

  const publish = async () => {
    const next: StoredRules = { rules: sanitizeRules(r), updatedAt: Date.now(), updatedBy: session.email };
    try {
      await saveRules(next, changes.join(' · ') || 'sin cambios');
      setStored(next);
      setR(next.rules);
      toast('Reglas publicadas: las mesas las aplicarán al abrir la app con internet');
    } catch (e) {
      toast(`No se pudo publicar: ${e instanceof Error ? e.message : String(e)}`);
    }
  };

  if (loading) return <Header title="Reglas del juego" subtitle="Cargando…" />;

  return (
    <>
      <Header
        title="Reglas del juego"
        subtitle={
          stored
            ? `Publicadas el ${fecha(stored.updatedAt)}${stored.updatedBy ? ` por ${stored.updatedBy}` : ''}`
            : 'Aún sin publicar: las mesas usan los valores recomendados'
        }
        actions={
          editable && (
            <>
              <button className="a-btn ghost" disabled={offDefault === 0} onClick={() => setR(DEFAULT_RULES)}>
                Poner todo lo recomendado
              </button>
              <button className="a-btn ghost" disabled={changes.length === 0} onClick={() => setR(published)}>
                Descartar cambios
              </button>
              <ConfirmButton tone="primary" disabled={(!!stored && changes.length === 0) || problems.length > 0} onConfirm={() => void publish()}>
                Publicar {changes.length ? `(${changes.length} cambio${changes.length === 1 ? '' : 's'})` : ''}
              </ConfirmButton>
            </>
          )
        }
      />

      {error && (
        <section className="a-card rg-alert">
          <b>No se han podido leer las reglas publicadas.</b> {error}. Si la base de datos es nueva, falta aplicar la migración de
          «reglas_juego» (se aplica sola al subir a main).
        </section>
      )}

      <section className="a-card rg-intro">
        <h2>Cómo funciona</h2>
        <ul>
          <li>
            Al <b>publicar</b>, todas las mesas descargan estas reglas la próxima vez que abren la app con internet. Sin internet
            siguen con las últimas que descargaron.
          </li>
          <li>
            La app <b>recalcula todo desde el historial</b>: el ELO, la experiencia y los niveles de todos los jugadores cambian
            también para los partidos ya jugados. No se pierde nada: si vuelves a publicar los valores de antes, todo queda como
            estaba.
          </li>
          <li>
            Mejor cambiar <b>pocas cosas cada vez</b> y dejar pasar unas semanas de partidos para ver el efecto. Cada publicación
            queda apuntada en Actividad.
          </li>
          <li>
            Al lado de cada valor está el <b>recomendado</b>. Si lo cambias, se marca en amarillo y con «Usar» vuelve al
            recomendado.
          </li>
        </ul>
      </section>

      {problems.length > 0 && (
        <section className="a-card rg-alert">
          <b>Hay que corregir esto antes de publicar:</b>
          <ul>
            {problems.map((p) => (
              <li key={p}>{p}</li>
            ))}
          </ul>
        </section>
      )}

      {RULE_GROUPS.map((g) => (
        <section key={g.id} className="a-card rg-group">
          <h2>{g.title}</h2>
          <p className="rg-group-intro">{g.intro}</p>
          <div className="rg-fields">
            {g.fields.map((f) => {
              const v = r[f.key];
              const rec = DEFAULT_RULES[f.key];
              const isRec = v === rec;
              const changed = v !== published[f.key];
              return (
                <div key={f.key} className={`rg-field${isRec ? '' : ' is-off'}${changed ? ' is-changed' : ''}`}>
                  <div className="rg-field-top">
                    <label className="rg-label" htmlFor={`rg-${f.key}`}>
                      {f.label}
                    </label>
                    <span className="rg-control">
                      {f.options ? (
                        <span className="rg-options" role="group" aria-label={f.label}>
                          {f.options.map((o) => (
                            <button
                              key={o.value}
                              id={o.value === f.options![0].value ? `rg-${f.key}` : undefined}
                              className={`a-btn sm${v === o.value ? ' primary' : ' ghost'}`}
                              aria-pressed={v === o.value}
                              disabled={!editable}
                              onClick={() => set(f.key, o.value as Value)}
                            >
                              {o.label}
                            </button>
                          ))}
                        </span>
                      ) : typeof rec === 'boolean' ? (
                        <label className="a-check">
                          <input id={`rg-${f.key}`} type="checkbox" checked={v as boolean} disabled={!editable} onChange={(e) => set(f.key, e.target.checked)} />
                          {v ? 'Encendido' : 'Apagado'}
                        </label>
                      ) : (
                        <>
                          <input
                            id={`rg-${f.key}`}
                            className="a-input narrow rg-input"
                            type="number"
                            inputMode="decimal"
                            value={Number.isFinite(v as number) ? (v as number) : ''}
                            min={f.min}
                            max={f.max}
                            step={f.step ?? 1}
                            disabled={!editable}
                            onChange={(e) => set(f.key, e.target.value === '' ? NaN : Number(e.target.value))}
                          />
                          {f.unit && <span className="a-muted">{f.unit}</span>}
                        </>
                      )}
                    </span>
                    <span className="rg-rec">
                      {isRec ? (
                        <Badge tone="ok">✓ Recomendado</Badge>
                      ) : (
                        <>
                          <Badge tone="warn">Recomendado: {show(rec, f)}</Badge>
                          {editable && (
                            <button className="a-btn sm ghost" onClick={() => set(f.key, rec)}>
                              Usar
                            </button>
                          )}
                        </>
                      )}
                    </span>
                  </div>
                  <p className="rg-text">
                    <b>Qué es:</b> {f.meaning}
                  </p>
                  <p className="rg-text">
                    <b>Por qué {show(rec, f)}:</b> {f.why}
                  </p>
                </div>
              );
            })}
          </div>
          <Preview group={g.id} r={sanitizeRules(r)} />
        </section>
      ))}
    </>
  );
}

/** «Así queda»: ejemplos con los valores escritos, para ver el efecto antes de publicar. */
function Preview({ group, r }: { group: string; r: GameRules }) {
  let rows: [string, string][] = [];
  if (group === 'elo') {
    rows = [
      ['Ganar a un rival de tu nivel', `+${eloGain(r.kEstablished, 0)} puntos (en los partidos de colocación, +${eloGain(r.kProvisional, 0)})`],
      ['Ganar a uno 200 puntos mejor', `+${eloGain(r.kEstablished, 200)} puntos`],
      ['Ganar a uno 200 puntos peor', `+${eloGain(r.kEstablished, -200)} puntos`],
      ['Perder', 'lo mismo, pero restando (lo que gana uno lo pierde el otro)'],
    ];
  } else if (group === 'ranks') {
    const step = eloGain(r.kEstablished, 0) || 1;
    const ranks: [string, number][] = [
      ['Madera', r.rankWood],
      ['Bronce', r.rankBronze],
      ['Plata', r.rankSilver],
      ['Oro', r.rankGold],
      ['Platino', r.rankPlatinum],
      ['Diamante', r.rankDiamond],
    ];
    rows = [
      ['Chatarra', `menos de ${r.rankWood} · donde empieza todo el mundo (${r.eloInitial})`],
      ...ranks.map(([name, min]): [string, string] => {
        const wins = Math.max(0, Math.ceil((min - r.eloInitial) / step));
        return [name, `desde ${min} · unas ${wins} victorias más que derrotas contra rivales de su nivel`];
      }),
    ];
  } else if (group === 'xp') {
    const win = r.xpComplete + r.xpWin;
    rows = [
      ['Partido ganado', `${win} XP (Clasificatorio: ${win + r.xpRankedWinBonus} XP)`],
      ['Partido perdido', `${r.xpComplete + r.xpLoss} XP`],
      ['Partido empatado', `${r.xpComplete + r.xpDraw} XP`],
      ['Ganar un torneo', `${r.xpTournamentWin} XP de premio, más lo de cada partido jugado`],
    ];
  } else if (group === 'extras') {
    const avg = averageMatchXp(r) || 1;
    rows = [
      ['Un reto del día', `como ${num(Math.round((r.xpChallengeDaily / avg) * 10) / 10)} partidos normales`],
      ['Los 3 retos de la semana', `como ${num(Math.round(((r.xpChallengeWeekly * 3) / avg) * 10) / 10)} partidos normales`],
      ['Un logro épico', `como ${num(Math.round((r.xpAchievementEpic / avg) * 10) / 10)} partidos normales`],
    ];
  } else if (group === 'levels') {
    const avg = averageMatchXp(r) || 1;
    rows = [1, 5, 10, 20, 50, 100]
      .filter((l) => l <= r.maxLevel)
      .concat(r.maxLevel > 100 ? [r.maxLevel] : [])
      .map((l): [string, string] => {
        const xp = xpForLevelWith(r, l);
        const n = Math.max(1, Math.round(xp / avg));
        return [`Nivel ${l}`, `${xp.toLocaleString('es-ES')} XP · ${n === 1 ? 'en el primer partido' : `unos ${n} partidos`}`];
      });
  }
  if (!rows.length) return null;
  return (
    <div className="rg-preview">
      <div className="rg-preview-title">Así queda con estos valores</div>
      <table>
        <tbody>
          {rows.map(([k, v]) => (
            <tr key={k}>
              <th>{k}</th>
              <td>{v}</td>
            </tr>
          ))}
        </tbody>
      </table>
      {group === 'levels' && <p className="a-muted small">Contando un partido normal, ganando la mitad: {Math.round(averageMatchXp(r))} XP de media, sin logros ni retos.</p>}
    </div>
  );
}
