/**
 * Catálogo de personalización: una pestaña por tipo (copas, escudos, avatares…), sus colecciones y, dentro de
 * cada colección, sus piezas. Se suben varios archivos a la vez (se ajustan solos al formato) y se decide
 * cómo se consigue cada cosa y cuándo (eventos de temporada incluidos).
 */
import { useMemo, useRef, useState, type CSSProperties } from 'react';
import { ACHIEVEMENTS } from '../../../../src/services/progression/achievements';
import { CATEGORIES } from '../../../../src/services/progression/rules';
import {
  FORMATOS_TORNEO,
  KINDS,
  KINDS_DE_TORNEO,
  KIND_ORDER,
  MODOS_PARTIDO,
  OBTENCION,
  TORNEOS_AMBITO,
  RAREZAS,
  TEMPORADAS,
  disponibilidadTexto,
  disponibleAhora,
  leerColores,
  leerTitulos,
  nombreDesdeArchivo,
  nuevaColeccion,
  obtencionDe,
  obtencionErrores,
  obtencionPorDefecto,
  obtencionTexto,
  rareza,
  type CatalogCollection,
  type CatalogItem,
  type CatalogKind,
  type Disponibilidad,
  type Obtencion,
  type Rareza,
  type TorneoAmbito,
} from '../catalog';
import { preparar, urlArchivo } from '../catalogMedia';
import { newId } from '../logic';
import { useAdmin } from '../store';
import { Badge, ConfirmButton, EuroInput, Field, Header, Modal } from '../ui';

const nombresObtencion = {
  rango: (id: string) => CATEGORIES.find((c) => c.id === id)?.name ?? id,
  logro: (id: string) => ACHIEVEMENTS.find((a) => a.id === id)?.name ?? id,
};
const textoObtencion = (o: Obtencion) => obtencionTexto(o, nombresObtencion);

const ESTADO: Record<CatalogCollection['estado'], { label: string; tone: 'ok' | 'warn' | 'neutral' }> = {
  borrador: { label: 'Borrador', tone: 'warn' },
  publicada: { label: 'Publicada', tone: 'ok' },
  retirada: { label: 'Retirada', tone: 'neutral' },
};

export function Catalogo() {
  const { data, save, toast } = useAdmin();
  const [tipo, setTipo] = useState<CatalogKind>('copa');
  const [abierta, setAbierta] = useState<string | null>(null);
  const [nueva, setNueva] = useState<string | null>(null);
  const cols = data.catalogCollections.filter((c) => c.tipo === tipo).sort((a, b) => a.orden - b.orden || b.creada - a.creada);
  const piezas = (id: string) => data.catalogItems.filter((i) => i.coleccionId === id);
  const col = data.catalogCollections.find((c) => c.id === abierta);
  if (col) return <ColeccionEditor col={col} onClose={() => setAbierta(null)} />;

  const crear = async () => {
    const nombre = (nueva ?? '').trim();
    if (!nombre) return;
    const c = { ...nuevaColeccion(tipo, newId('cc'), Date.now(), cols.length), nombre };
    await save('catalogCollections', c, 'Catálogo', `Crea la colección «${nombre}» (${KINDS[tipo].plural})`);
    setNueva(null);
    toast('Colección creada como borrador');
    setAbierta(c.id);
  };

  return (
    <>
      <Header
        title="Catálogo"
        subtitle="Todo lo que se puede personalizar en la app: copas, escudos, avatares, marcos, fondos, podios, títulos, colores y sonidos. Cada cosa va en una colección con su rareza, cómo se consigue y cuándo."
        actions={
          <button className="a-btn primary" onClick={() => setNueva('')}>
            + Nueva colección de {KINDS[tipo].plural.toLowerCase()}
          </button>
        }
      />
      <nav className="c-kinds" aria-label="Tipos de personalización">
        {KIND_ORDER.map((k) => {
          const n = data.catalogCollections.filter((c) => c.tipo === k).length;
          return (
            <button key={k} className={`c-kind ${k === tipo ? 'on' : ''}`} aria-pressed={k === tipo} onClick={() => setTipo(k)}>
              <span aria-hidden="true">{KINDS[k].icono}</span> {KINDS[k].plural}
              {n > 0 && <small>{n}</small>}
            </button>
          );
        })}
      </nav>
      <Formato tipo={tipo} />
      {cols.length === 0 ? (
        <div className="a-card c-empty">
          <p>
            Aún no hay colecciones de {KINDS[tipo].plural.toLowerCase()}. Crea una, súbele las piezas y publícala cuando esté lista.
          </p>
        </div>
      ) : (
        <div className="c-grid">
          {cols.map((c) => {
            const ps = piezas(c.id);
            const r = rareza(c.rareza);
            return (
              <button key={c.id} className="c-col" style={{ '--rar': r.color } as CSSProperties} onClick={() => setAbierta(c.id)}>
                <span className="c-col-thumbs">
                  {ps.length ? ps.slice(0, 4).map((p) => <Vista key={p.id} tipo={c.tipo} item={p} size={56} />) : <span className="c-thumb-empty">Sin piezas</span>}
                </span>
                <span className="c-col-name">
                  <b>{c.nombre}</b>
                  <Badge tone={ESTADO[c.estado].tone}>{ESTADO[c.estado].label}</Badge>
                </span>
                <span className="c-col-meta">
                  <span className="c-rar">{r.label}</span> · {ps.length} pieza{ps.length === 1 ? '' : 's'} ·{' '}
                  {c.ambito === 'coleccion' ? 'se consigue entera' : 'pieza a pieza'}
                </span>
                <span className="c-col-meta">
                  {textoObtencion(c.obtencion)}
                  {c.disponibilidad.modo !== 'siempre' && ` · ${disponibilidadTexto(c.disponibilidad)}`}
                </span>
              </button>
            );
          })}
        </div>
      )}
      {nueva !== null && (
        <Modal
          title={`Nueva colección de ${KINDS[tipo].plural.toLowerCase()}`}
          onClose={() => setNueva(null)}
          footer={
            <>
              <button className="a-btn ghost" onClick={() => setNueva(null)}>Cancelar</button>
              <button className="a-btn primary" disabled={!nueva.trim()} onClick={() => void crear()}>Crear</button>
            </>
          }
        >
          <Field label="Nombre" hint="Por ejemplo «Navidad 2026», «Retro 90» o «Leyendas».">
            <input className="a-input" value={nueva} autoFocus onChange={(e) => setNueva(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && void crear()} maxLength={40} />
          </Field>
          <p className="a-muted small">Se crea como borrador: nadie la ve en la app hasta que la publiques.</p>
        </Modal>
      )}
    </>
  );
}

/** Ficha del formato que hay que subir para un tipo. */
function Formato({ tipo }: { tipo: CatalogKind }) {
  const s = KINDS[tipo];
  return (
    <div className="a-card c-spec">
      <div className="c-spec-head">
        <span className="c-spec-icon" aria-hidden="true">{s.icono}</span>
        <div>
          <b>{s.plural}</b>
          <p className="a-muted">{s.uso}</p>
        </div>
      </div>
      <ul className="c-spec-list">
        {s.archivo === 'imagen' && (
          <>
            <li><span>Tamaño final</span><b>{s.ancho} × {s.alto} px</b></li>
            <li><span>Fondo</span><b>{s.transparente ? 'Transparente (PNG)' : 'Lleno'}</b></li>
            <li><span>Formatos</span><b>PNG, WebP o JPG</b></li>
            <li><span>Peso final</span><b>≤ {s.maxKB} KB</b></li>
          </>
        )}
        {s.archivo === 'audio' && (
          <>
            <li><span>Formatos</span><b>MP3, OGG o WAV</b></li>
            <li><span>Duración</span><b>≤ 10 s</b></li>
            <li><span>Peso</span><b>≤ {s.maxKB} KB</b></li>
          </>
        )}
        {!s.archivo && <li><span>Archivo</span><b>No lleva: se escribe</b></li>}
      </ul>
      <p className="c-spec-tip">💡 {s.consejo}</p>
      {s.archivo === 'imagen' && (
        <p className="a-muted small">
          No hace falta que la imagen venga exacta: al subirla se ajusta sola a {s.ancho}×{s.alto}, se pasa a WebP y se comprime. Te avisa si queda borrosa o
          si la proporción no cuadra. Para subir varias a la vez, nómbralas como quieras que se llamen («copa-dorada.png» → «Copa dorada»).
        </p>
      )}
    </div>
  );
}

/** Cómo se ve una pieza (miniatura). */
function Vista({ tipo, item, size = 72 }: { tipo: CatalogKind; item: CatalogItem; size?: number }) {
  const st = { width: size, height: size } as const;
  if (tipo === 'titulo') return <span className="c-v-title" style={{ minHeight: size * 0.6 }}>{item.valor ?? item.nombre}</span>;
  if (tipo === 'color') {
    const [a, b] = (item.valor ?? '#888888').split(' ');
    return <span className="c-v-color" style={{ ...st, background: b ? `linear-gradient(135deg, ${a}, ${b})` : a }} />;
  }
  if (tipo === 'sonido') return <span className="c-v-sound" style={st}>🎵</span>;
  if (!item.archivo) return <span className="c-v-empty" style={st}>{KINDS[tipo].icono}</span>;
  if (tipo === 'fondo') return <img className="c-v-img" src={urlArchivo(item.archivo)} alt="" style={{ width: size * 2, height: size / 2 }} />;
  if (tipo === 'marco')
    return (
      <span className="c-v-frame" style={st}>
        <span className="c-v-frame-photo" />
        <img src={urlArchivo(item.archivo)} alt="" />
      </span>
    );
  return <img className={`c-v-img ${tipo === 'avatar' ? 'round' : ''}`} src={urlArchivo(item.archivo)} alt="" style={st} />;
}

function RarezaPicker({ value, onChange, heredar }: { value: Rareza | undefined; onChange: (r: Rareza | undefined) => void; heredar?: Rareza }) {
  return (
    <div className="c-chips" role="radiogroup" aria-label="Rareza">
      {heredar && (
        <button type="button" className={`c-chip ${value === undefined ? 'on' : ''}`} onClick={() => onChange(undefined)}>
          Como la colección ({rareza(heredar).label})
        </button>
      )}
      {RAREZAS.map((r) => (
        <button
          type="button"
          key={r.id}
          className={`c-chip ${value === r.id ? 'on' : ''}`}
          style={{ '--rar': r.color } as CSSProperties}
          role="radio"
          aria-checked={value === r.id}
          onClick={() => onChange(r.id)}
        >
          <i className="c-dot" /> {r.label}
        </button>
      ))}
    </div>
  );
}

function ObtencionPicker({ value, onChange }: { value: Obtencion; onChange: (o: Obtencion) => void }) {
  const info = OBTENCION.find((o) => o.tipo === value.tipo)!;
  const num = (v: string) => Math.max(0, Math.floor(Number(v) || 0));
  return (
    <>
      <Field label="Forma" hint={info.ayuda + (info.proximamente ? ' (Aún no funciona en la app: queda preparado.)' : '')}>
        <select className="a-input" value={value.tipo} onChange={(e) => onChange(obtencionPorDefecto(e.target.value as Obtencion['tipo']))}>
          {OBTENCION.map((o) => (
            <option key={o.tipo} value={o.tipo}>
              {o.label}
              {o.proximamente ? ' · próximamente' : ''}
            </option>
          ))}
        </select>
      </Field>
      {value.tipo === 'nivel' && (
        <Field label="Nivel">
          <input className="a-input narrow" type="number" min={1} value={value.nivel} onChange={(e) => onChange({ ...value, nivel: num(e.target.value) })} />
        </Field>
      )}
      {value.tipo === 'rango' && (
        <Field label="Rango">
          <select className="a-input" value={value.rango} onChange={(e) => onChange({ ...value, rango: e.target.value })}>
            {CATEGORIES.map((c) => (
              <option key={c.id} value={c.id}>{c.name}</option>
            ))}
          </select>
        </Field>
      )}
      {value.tipo === 'logro' && (
        <Field label="Logro">
          <select className="a-input" value={value.logro} onChange={(e) => onChange({ ...value, logro: e.target.value })}>
            <option value="">— Elige un logro —</option>
            {ACHIEVEMENTS.map((a) => (
              <option key={a.id} value={a.id}>{a.icon} {a.name}</option>
            ))}
          </select>
        </Field>
      )}
      {(value.tipo === 'torneos' || value.tipo === 'partidos') && (
        <Field label={value.tipo === 'torneos' ? 'Torneos ganados' : 'Partidos jugados'}>
          <input className="a-input narrow" type="number" min={1} value={value.cantidad} onChange={(e) => onChange({ ...value, cantidad: num(e.target.value) })} />
        </Field>
      )}
      {(value.tipo === 'jugarPartido' || value.tipo === 'ganarPartido') && (
        <Field label="Qué partido cuenta" hint="Los de rápido, loco y clasificatorio son partidos sueltos; los de liga, copa y pool, de torneo.">
          <select className="a-input" value={value.modo} onChange={(e) => onChange({ ...value, modo: e.target.value as typeof value.modo })}>
            {MODOS_PARTIDO.map((m) => (
              <option key={m.id} value={m.id}>{m.label[0].toUpperCase() + m.label.slice(1)}</option>
            ))}
          </select>
        </Field>
      )}
      {value.tipo === 'ganarTorneo' && (
        <Field label="Qué torneo hay que ganar">
          <select className="a-input" value={value.formato} onChange={(e) => onChange({ ...value, formato: e.target.value as typeof value.formato })}>
            {FORMATOS_TORNEO.map((f) => (
              <option key={f.id} value={f.id}>{f.label[0].toUpperCase() + f.label.slice(1)}</option>
            ))}
          </select>
        </Field>
      )}
      {value.tipo === 'evento' && (
        <Field label="Partidos a jugar durante el evento">
          <input className="a-input narrow" type="number" min={1} value={value.partidos} onChange={(e) => onChange({ ...value, partidos: num(e.target.value) })} />
        </Field>
      )}
      {value.tipo === 'monedas' && (
        <Field label="Precio en monedas">
          <input className="a-input narrow" type="number" min={1} value={value.precio} onChange={(e) => onChange({ ...value, precio: num(e.target.value) })} />
        </Field>
      )}
      {value.tipo === 'pago' && (
        <Field label="Precio">
          <EuroInput cents={value.precio} onChange={(c) => onChange({ ...value, precio: c })} />
        </Field>
      )}
      {value.tipo === 'codigo' && (
        <Field label="Código" hint="4 a 20 letras, números o guiones.">
          <input className="a-input mono" value={value.codigo} onChange={(e) => onChange({ ...value, codigo: e.target.value.toUpperCase().replace(/[^A-Z0-9-]/g, '').slice(0, 20) })} />
        </Field>
      )}
    </>
  );
}

const fechaInput = (t?: number) => (t === undefined ? '' : new Date(t).toISOString().slice(0, 10));
const desdeInput = (v: string, finDelDia = false) => (v ? new Date(`${v}T${finDelDia ? '23:59:59' : '00:00:00'}`).getTime() : undefined);
const diaInput = (md?: string) => (md ? `2000-${md}` : '');

function DisponibilidadPicker({ value, onChange }: { value: Disponibilidad; onChange: (d: Disponibilidad) => void }) {
  const modo = (m: Disponibilidad['modo']) =>
    onChange(m === 'siempre' ? { modo: m } : m === 'anual' ? { modo: m, desdeDia: '12-20', hastaDia: '01-06', soloUsoEnFechas: value.soloUsoEnFechas } : { modo: m, soloUsoEnFechas: value.soloUsoEnFechas });
  return (
    <>
      <div className="c-chips" role="radiogroup" aria-label="Cuándo se consigue">
        {(
          [
            ['siempre', 'Siempre'],
            ['fechas', 'Entre dos fechas'],
            ['anual', 'Cada año (temporada)'],
          ] as const
        ).map(([m, l]) => (
          <button type="button" key={m} className={`c-chip ${value.modo === m ? 'on' : ''}`} role="radio" aria-checked={value.modo === m} onClick={() => modo(m)}>
            {l}
          </button>
        ))}
      </div>
      {value.modo === 'fechas' && (
        <div className="c-row">
          <Field label="Desde">
            <input className="a-input" type="date" value={fechaInput(value.desde)} onChange={(e) => onChange({ ...value, desde: desdeInput(e.target.value) })} />
          </Field>
          <Field label="Hasta">
            <input className="a-input" type="date" value={fechaInput(value.hasta)} onChange={(e) => onChange({ ...value, hasta: desdeInput(e.target.value, true) })} />
          </Field>
        </div>
      )}
      {value.modo === 'anual' && (
        <>
          <div className="c-chips">
            {TEMPORADAS.map((t) => (
              <button
                type="button"
                key={t.nombre}
                className={`c-chip ${value.desdeDia === t.desdeDia && value.hastaDia === t.hastaDia ? 'on' : ''}`}
                onClick={() => onChange({ ...value, desdeDia: t.desdeDia, hastaDia: t.hastaDia })}
              >
                {t.nombre}
              </button>
            ))}
          </div>
          <div className="c-row">
            <Field label="Desde el día" hint="El año no importa: se repite cada año.">
              <input className="a-input" type="date" value={diaInput(value.desdeDia)} onChange={(e) => onChange({ ...value, desdeDia: e.target.value.slice(5) })} />
            </Field>
            <Field label="Hasta el día">
              <input className="a-input" type="date" value={diaInput(value.hastaDia)} onChange={(e) => onChange({ ...value, hastaDia: e.target.value.slice(5) })} />
            </Field>
          </div>
        </>
      )}
      {value.modo !== 'siempre' && (
        <label className="c-check">
          <input type="checkbox" checked={!!value.soloUsoEnFechas} onChange={(e) => onChange({ ...value, soloUsoEnFechas: e.target.checked })} />
          <span>
            <b>Solo se puede usar en esas fechas.</b> Si no lo marcas, quien la consiga se la queda para siempre (como un objeto de temporada de un
            juego). Si lo marcas, fuera de las fechas sigue en su colección pero no se puede elegir (como los temas de Navidad).
          </span>
        </label>
      )}
    </>
  );
}

/** Errores que impiden publicar una colección. */
function erroresPublicar(c: CatalogCollection, items: CatalogItem[]): string[] {
  const e: string[] = [];
  if (!c.nombre.trim()) e.push('Ponle un nombre.');
  if (!items.some((i) => i.activo)) e.push('Necesita al menos una pieza activa.');
  if (KINDS[c.tipo].archivo && items.some((i) => i.activo && !i.archivo)) e.push('Hay piezas activas sin archivo.');
  e.push(...obtencionErrores(c.obtencion, c.disponibilidad));
  if (c.disponibilidad.modo === 'fechas' && (c.disponibilidad.desde === undefined || c.disponibilidad.hasta === undefined)) e.push('Faltan las fechas.');
  if (c.disponibilidad.modo === 'fechas' && c.disponibilidad.desde !== undefined && c.disponibilidad.hasta !== undefined && c.disponibilidad.hasta < c.disponibilidad.desde)
    e.push('La fecha final es anterior a la inicial.');
  if (c.ambito === 'elemento')
    for (const i of items) if (i.obtencion) for (const x of obtencionErrores(i.obtencion, c.disponibilidad)) e.push(`${i.nombre}: ${x}`);
  return [...new Set(e)];
}

interface Subida {
  nombre: string;
  estado: 'esperando' | 'subiendo' | 'hecho' | 'error';
  mensaje?: string;
}

function ColeccionEditor({ col, onClose }: { col: CatalogCollection; onClose: () => void }) {
  const { data, save, saveMany, remove, removeMany, backend, toast } = useAdmin();
  const [f, setF] = useState<CatalogCollection>(col);
  const items = useMemo(() => data.catalogItems.filter((i) => i.coleccionId === col.id).sort((a, b) => a.orden - b.orden), [data.catalogItems, col.id]);
  const spec = KINDS[col.tipo];
  const dirty = JSON.stringify(f) !== JSON.stringify(col);
  const errores = erroresPublicar(f, items);
  const [subidas, setSubidas] = useState<Subida[]>([]);
  const [avisos, setAvisos] = useState<string[]>([]);
  const [texto, setTexto] = useState('');
  const [arrastrando, setArrastrando] = useState(false);
  const [editObt, setEditObt] = useState<CatalogItem | null>(null);
  const fileRef = useRef<HTMLInputElement>(null);
  const set = (p: Partial<CatalogCollection>) => setF((x) => ({ ...x, ...p }));

  const guardar = async (estado?: CatalogCollection['estado']) => {
    const next = { ...f, ...(estado ? { estado } : {}) };
    await save('catalogCollections', next, 'Catálogo', estado && estado !== col.estado ? `${ESTADO[estado].label}: «${next.nombre}»` : `Edita la colección «${next.nombre}»`);
    setF(next);
    toast(estado === 'publicada' ? 'Publicada: ya se ve en la app' : 'Guardado');
  };

  const guardarPieza = (i: CatalogItem, detalle: string) => save('catalogItems', i, 'Catálogo', `${detalle} (${col.nombre})`);

  const subir = async (files: File[]) => {
    if (!files.length) return;
    // La colección tiene que existir con sus datos actuales antes de colgarle piezas.
    if (dirty) await save('catalogCollections', f, 'Catálogo', `Edita la colección «${f.nombre}»`);
    setAvisos([]);
    setSubidas(files.map((x) => ({ nombre: x.name, estado: 'esperando' })));
    const nuevas: CatalogItem[] = [];
    const todos: string[] = [];
    let orden = items.length;
    for (const [i, file] of files.entries()) {
      setSubidas((s) => s.map((x, j) => (j === i ? { ...x, estado: 'subiendo' } : x)));
      try {
        const p = await preparar(col.tipo, file);
        const id = newId('ci');
        const ruta = `${col.tipo}/${col.id}/${id}.${p.extension}`;
        const archivo = await backend.uploadFile(ruta, p.blob);
        nuevas.push({ id, coleccionId: col.id, nombre: nombreDesdeArchivo(file.name), archivo, ruta, orden: orden++, activo: true });
        todos.push(...p.avisos);
        setSubidas((s) => s.map((x, j) => (j === i ? { ...x, estado: 'hecho', mensaje: `${Math.round(p.blob.size / 1024)} KB` } : x)));
      } catch (e) {
        setSubidas((s) => s.map((x, j) => (j === i ? { ...x, estado: 'error', mensaje: e instanceof Error ? e.message : String(e) } : x)));
      }
    }
    if (nuevas.length) await saveMany('catalogItems', nuevas, 'Catálogo', `Sube ${nuevas.length} pieza${nuevas.length === 1 ? '' : 's'} a «${f.nombre}»`);
    setAvisos(todos);
    if (nuevas.length) toast(`${nuevas.length} pieza${nuevas.length === 1 ? '' : 's'} subida${nuevas.length === 1 ? '' : 's'}`);
  };

  const reemplazar = async (item: CatalogItem, file: File) => {
    try {
      const p = await preparar(col.tipo, file);
      const ruta = `${col.tipo}/${col.id}/${item.id}.${p.extension}`;
      const archivo = await backend.uploadFile(ruta, p.blob);
      if (item.ruta && item.ruta !== ruta) await backend.removeFiles([item.ruta]).catch(() => undefined);
      await guardarPieza({ ...item, archivo, ruta }, `Reemplaza el archivo de «${item.nombre}»`);
      setAvisos(p.avisos);
    } catch (e) {
      toast(e instanceof Error ? e.message : String(e));
    }
  };

  const anadirTexto = async () => {
    if (dirty) await save('catalogCollections', f, 'Catálogo', `Edita la colección «${f.nombre}»`);
    let orden = items.length;
    if (col.tipo === 'titulo') {
      const { valores, errores: es } = leerTitulos(texto);
      const nuevas = valores.filter((v) => !items.some((i) => i.valor === v)).map((v) => ({ id: newId('ci'), coleccionId: col.id, nombre: v, valor: v, orden: orden++, activo: true }));
      if (nuevas.length) await saveMany('catalogItems', nuevas, 'Catálogo', `Añade ${nuevas.length} título${nuevas.length === 1 ? '' : 's'} a «${f.nombre}»`);
      setAvisos(es);
    } else {
      const { colores, errores: es } = leerColores(texto);
      const nuevas = colores.map((c) => ({ id: newId('ci'), coleccionId: col.id, nombre: c.nombre, valor: c.valor, orden: orden++, activo: true }));
      if (nuevas.length) await saveMany('catalogItems', nuevas, 'Catálogo', `Añade ${nuevas.length} color${nuevas.length === 1 ? '' : 'es'} a «${f.nombre}»`);
      setAvisos(es);
    }
    setTexto('');
  };

  const borrarPieza = async (item: CatalogItem) => {
    if (item.ruta) await backend.removeFiles([item.ruta]).catch(() => undefined);
    await remove('catalogItems', item.id, 'Catálogo', `Borra «${item.nombre}» de «${col.nombre}»`);
  };

  const borrarColeccion = async () => {
    await backend.removeFiles(items.map((i) => i.ruta).filter((r): r is string => !!r)).catch(() => undefined);
    if (items.length) await removeMany('catalogItems', items.map((i) => i.id), 'Catálogo', `Borra las piezas de «${col.nombre}»`);
    await remove('catalogCollections', col.id, 'Catálogo', `Borra la colección «${col.nombre}»`);
    toast('Colección borrada');
    onClose();
  };

  const mover = async (item: CatalogItem, d: -1 | 1) => {
    const i = items.findIndex((x) => x.id === item.id);
    const otro = items[i + d];
    if (!otro) return;
    await saveMany('catalogItems', [{ ...item, orden: otro.orden }, { ...otro, orden: item.orden }], 'Catálogo', `Reordena «${col.nombre}»`);
  };

  const enEvento = f.disponibilidad.modo !== 'siempre' && disponibleAhora(f.disponibilidad, Date.now());

  return (
    <>
      <div className="a-header">
        <div>
          <button className="a-btn ghost sm" onClick={onClose}>← {spec.plural}</button>
          <h1 className="c-title">
            <span aria-hidden="true">{spec.icono}</span> {f.nombre || 'Sin nombre'} <Badge tone={ESTADO[col.estado].tone}>{ESTADO[col.estado].label}</Badge>
            {enEvento && <Badge tone="accent">Disponible ahora</Badge>}
          </h1>
        </div>
        <div className="a-actions">
          {dirty && (
            <button className="a-btn" onClick={() => void guardar()}>
              Guardar cambios
            </button>
          )}
          {col.estado !== 'publicada' ? (
            <button className="a-btn primary" disabled={errores.length > 0} title={errores.join('\n')} onClick={() => void guardar('publicada')}>
              Publicar
            </button>
          ) : (
            <>
              <button className="a-btn" onClick={() => void guardar('borrador')} title="Deja de verse en la app">
                Pasar a borrador
              </button>
              <button className="a-btn" onClick={() => void guardar('retirada')} title="Ya no se puede conseguir; quien la tenga la conserva">
                Retirar
              </button>
            </>
          )}
        </div>
      </div>
      {col.estado !== 'publicada' && errores.length > 0 && (
        <div className="a-card c-todo">
          <b>Para publicarla falta:</b>
          <ul>
            {errores.map((e) => (
              <li key={e}>{e}</li>
            ))}
          </ul>
        </div>
      )}

      <div className="a-grid-2">
        <section className="a-card">
          <h2>Datos</h2>
          <Field label="Nombre">
            <input className="a-input" value={f.nombre} maxLength={40} onChange={(e) => set({ nombre: e.target.value })} />
          </Field>
          <Field label="Descripción" hint="Se ve en la app al tocar la colección.">
            <textarea className="a-input" rows={2} value={f.descripcion} maxLength={200} onChange={(e) => set({ descripcion: e.target.value })} />
          </Field>
          <Field label="Rareza de sus piezas" hint="Cada pieza puede tener otra distinta.">
            <RarezaPicker value={f.rareza} onChange={(r) => r && set({ rareza: r })} />
          </Field>
          {KINDS_DE_TORNEO.includes(f.tipo) && (
            <Field label="Para qué torneos" hint="Exclusiva de un tipo: solo sale en ese tipo de torneo y nunca en los otros ni en los equipos.">
              <select className="a-input" value={f.torneo ?? 'todos'} onChange={(e) => set({ torneo: e.target.value as TorneoAmbito })}>
                {TORNEOS_AMBITO.map((t) => (
                  <option key={t.id} value={t.id}>{t.label}</option>
                ))}
              </select>
            </Field>
          )}
          <Field label="Orden en la app" hint="Las de número más bajo salen primero.">
            <input className="a-input narrow" type="number" value={f.orden} onChange={(e) => set({ orden: Number(e.target.value) || 0 })} />
          </Field>
        </section>

        <section className="a-card">
          <h2>Cómo se consigue</h2>
          <div className="c-chips" role="radiogroup" aria-label="Qué se desbloquea">
            <button type="button" className={`c-chip ${f.ambito === 'coleccion' ? 'on' : ''}`} role="radio" aria-checked={f.ambito === 'coleccion'} onClick={() => set({ ambito: 'coleccion' })}>
              📦 Toda la colección de una vez
            </button>
            <button type="button" className={`c-chip ${f.ambito === 'elemento' ? 'on' : ''}`} role="radio" aria-checked={f.ambito === 'elemento'} onClick={() => set({ ambito: 'elemento' })}>
              🧩 Cada pieza por separado
            </button>
          </div>
          <p className="a-muted small">
            {f.ambito === 'coleccion'
              ? 'Al cumplir la condición se desbloquean todas sus piezas a la vez.'
              : 'Esta es la forma de cada pieza; en la lista de abajo puedes poner otra distinta a cualquiera (por ejemplo, la última por un logro difícil).'}
          </p>
          <ObtencionPicker value={f.obtencion} onChange={(o) => set({ obtencion: o })} />
          {f.ambito === 'elemento' && (
            <Field label="Premio por completarla (monedas)" hint="Opcional: se da al conseguir todas las piezas. Vacío = sin premio.">
              <input
                className="a-input narrow"
                type="number"
                min={0}
                value={f.premioCompletar ?? ''}
                onChange={(e) => set({ premioCompletar: e.target.value === '' ? undefined : Math.max(0, Math.floor(Number(e.target.value))) })}
              />
            </Field>
          )}
          <h2 className="c-h2-gap">Cuándo</h2>
          <DisponibilidadPicker value={f.disponibilidad} onChange={(d) => set({ disponibilidad: d })} />
          <p className="a-muted small">Ahora: {disponibilidadTexto(f.disponibilidad)}.</p>
        </section>
      </div>

      <section className="a-card">
        <div className="c-items-head">
          <h2>
            Piezas <span className="a-muted">({items.length})</span>
          </h2>
          {spec.archivo && (
            <button className="a-btn" onClick={() => fileRef.current?.click()}>
              Subir archivos
            </button>
          )}
        </div>

        {spec.archivo ? (
          <div
            className={`c-drop ${arrastrando ? 'over' : ''}`}
            onDragOver={(e) => {
              e.preventDefault();
              setArrastrando(true);
            }}
            onDragLeave={() => setArrastrando(false)}
            onDrop={(e) => {
              e.preventDefault();
              setArrastrando(false);
              void subir([...e.dataTransfer.files]);
            }}
            onClick={() => fileRef.current?.click()}
            role="button"
            tabIndex={0}
            onKeyDown={(e) => (e.key === 'Enter' || e.key === ' ') && fileRef.current?.click()}
          >
            <b>Arrastra aquí varios archivos a la vez</b> o pulsa para elegirlos.
            <span>
              {spec.archivo === 'imagen' ? `Se ajustan solos a ${spec.ancho}×${spec.alto} y a WebP.` : 'MP3, OGG o WAV de 10 s como mucho.'} El nombre del archivo será el
              de la pieza.
            </span>
            <input
              ref={fileRef}
              type="file"
              multiple
              hidden
              accept={spec.acepta.join(',')}
              onChange={(e) => {
                void subir([...(e.target.files ?? [])]);
                e.target.value = '';
              }}
            />
          </div>
        ) : (
          <div className="c-text-add">
            <Field
              label={col.tipo === 'titulo' ? 'Títulos (uno por línea)' : 'Colores (uno por línea)'}
              hint={col.tipo === 'titulo' ? 'Máximo 24 caracteres cada uno. Ej.: El Muro' : 'Ej.: Fuego #FF5A1F #FFC400   (un segundo color hace degradado)'}
            >
              <textarea className="a-input mono" rows={4} value={texto} onChange={(e) => setTexto(e.target.value)} />
            </Field>
            <button className="a-btn" disabled={!texto.trim()} onClick={() => void anadirTexto()}>
              Añadir
            </button>
          </div>
        )}

        {subidas.length > 0 && (
          <ul className="c-uploads">
            {subidas.map((s, i) => (
              <li key={i} className={`c-up c-up-${s.estado}`}>
                <span>{s.estado === 'hecho' ? '✓' : s.estado === 'error' ? '✕' : s.estado === 'subiendo' ? '…' : '·'}</span>
                <b>{s.nombre}</b>
                {s.mensaje && <small>{s.mensaje}</small>}
              </li>
            ))}
          </ul>
        )}
        {avisos.length > 0 && (
          <div className="c-warns">
            {avisos.map((a) => (
              <p key={a}>⚠ {a}</p>
            ))}
          </div>
        )}

        {items.length === 0 ? (
          <p className="a-muted">Aún no tiene piezas.</p>
        ) : (
          <div className="c-items">
            {items.map((it, idx) => {
              const r = rareza(it.rareza ?? f.rareza);
              return (
                <div key={it.id} className={`c-item ${it.activo ? '' : 'off'}`} style={{ '--rar': r.color } as CSSProperties}>
                  <div className="c-item-view">
                    <Vista tipo={col.tipo} item={it} size={col.tipo === 'fondo' ? 64 : 88} />
                  </div>
                  {col.tipo === 'sonido' && it.archivo && <audio className="c-audio" src={it.archivo} controls preload="none" />}
                  <input
                    className="a-input c-item-name"
                    defaultValue={it.nombre}
                    aria-label="Nombre de la pieza"
                    maxLength={40}
                    onBlur={(e) => {
                      const nombre = e.target.value.trim();
                      if (nombre && nombre !== it.nombre) void guardarPieza({ ...it, nombre, ...(col.tipo === 'titulo' ? { valor: nombre.slice(0, 24) } : {}) }, `Renombra «${it.nombre}» a «${nombre}»`);
                    }}
                  />
                  {col.tipo === 'color' && (
                    <input
                      className="a-input mono c-item-name"
                      defaultValue={it.valor}
                      aria-label="Color"
                      onBlur={(e) => {
                        const v = e.target.value.trim().toUpperCase();
                        if (/^#[0-9A-F]{6}( #[0-9A-F]{6})?$/.test(v) && v !== it.valor) void guardarPieza({ ...it, valor: v }, `Cambia el color de «${it.nombre}»`);
                      }}
                    />
                  )}
                  <select
                    className="a-input c-item-rar"
                    value={it.rareza ?? ''}
                    aria-label="Rareza"
                    onChange={(e) => void guardarPieza({ ...it, rareza: (e.target.value || undefined) as Rareza | undefined }, `Cambia la rareza de «${it.nombre}»`)}
                  >
                    <option value="">{rareza(f.rareza).label} (colección)</option>
                    {RAREZAS.map((x) => (
                      <option key={x.id} value={x.id}>{x.label}</option>
                    ))}
                  </select>
                  {f.ambito === 'elemento' && (
                    <button className="a-btn sm c-item-obt" onClick={() => setEditObt(it)} title="Cómo se consigue esta pieza">
                      {it.obtencion ? '★ ' : ''}
                      {textoObtencion(obtencionDe(it, f))}
                    </button>
                  )}
                  <div className="c-item-actions">
                    <label className="c-check sm">
                      <input type="checkbox" checked={it.activo} onChange={(e) => void guardarPieza({ ...it, activo: e.target.checked }, `${e.target.checked ? 'Activa' : 'Desactiva'} «${it.nombre}»`)} />
                      Activa
                    </label>
                    <button className="a-btn ghost sm" disabled={idx === 0} onClick={() => void mover(it, -1)} aria-label="Subir">↑</button>
                    <button className="a-btn ghost sm" disabled={idx === items.length - 1} onClick={() => void mover(it, 1)} aria-label="Bajar">↓</button>
                    {spec.archivo && (
                      <label className="a-btn ghost sm" title="Reemplazar el archivo">
                        ⟳
                        <input
                          type="file"
                          hidden
                          accept={spec.acepta.join(',')}
                          onChange={(e) => {
                            const file = e.target.files?.[0];
                            if (file) void reemplazar(it, file);
                            e.target.value = '';
                          }}
                        />
                      </label>
                    )}
                    <ConfirmButton onConfirm={() => void borrarPieza(it)}>🗑</ConfirmButton>
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </section>

      <section className="a-card c-danger">
        <h2>Zona peligrosa</h2>
        <p className="a-muted small">
          Borrar la colección quita también sus piezas y sus archivos. Si ya se ha publicado y la tiene gente, mejor <b>retírala</b>: deja de poder
          conseguirse pero quien la tenga la conserva.
        </p>
        <ConfirmButton onConfirm={() => void borrarColeccion()}>Borrar colección</ConfirmButton>
      </section>

      {editObt && (
        <PiezaObtencion
          item={editObt}
          col={f}
          onClose={() => setEditObt(null)}
          onSave={async (o) => {
            const { obtencion: _old, ...rest } = editObt;
            await guardarPieza(o ? { ...rest, obtencion: o } : rest, `Cambia cómo se consigue «${editObt.nombre}»`);
            setEditObt(null);
          }}
        />
      )}
    </>
  );
}

function PiezaObtencion({ item, col, onClose, onSave }: { item: CatalogItem; col: CatalogCollection; onClose: () => void; onSave: (o: Obtencion | undefined) => Promise<void> }) {
  const [propia, setPropia] = useState(!!item.obtencion);
  const [o, setO] = useState<Obtencion>(item.obtencion ?? col.obtencion);
  const errs = propia ? obtencionErrores(o, col.disponibilidad) : [];
  return (
    <Modal
      title={`Cómo se consigue «${item.nombre}»`}
      onClose={onClose}
      footer={
        <>
          <button className="a-btn ghost" onClick={onClose}>Cancelar</button>
          <button className="a-btn primary" disabled={errs.length > 0} onClick={() => void onSave(propia ? o : undefined)}>Guardar</button>
        </>
      }
    >
      <div className="c-chips">
        <button type="button" className={`c-chip ${!propia ? 'on' : ''}`} onClick={() => setPropia(false)}>
          Como la colección ({textoObtencion(col.obtencion)})
        </button>
        <button type="button" className={`c-chip ${propia ? 'on' : ''}`} onClick={() => setPropia(true)}>
          Otra forma solo para esta pieza
        </button>
      </div>
      {propia && <ObtencionPicker value={o} onChange={setO} />}
      {errs.map((e) => (
        <p key={e} className="a-error">{e}</p>
      ))}
    </Modal>
  );
}
