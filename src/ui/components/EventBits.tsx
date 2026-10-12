/**
 * Eventos (Halloween, Navidad…): el aviso discreto de arriba mientras dura uno, la ventana con lo que
 * se puede conseguir y el aviso de «premio conseguido» cuando alguien lo gana.
 */
import { useEffect, useMemo, useState } from 'react';
import { useApp } from '../../app/AppContext';
import {
  activeEvents,
  cuandoTexto,
  dayKey,
  requisitoTexto,
  visibleItems,
  type CatalogCollection,
  type CatalogItem,
} from '../../services/events';
import { BannerArt, Crest, Cup } from './Crest';
import { Avatar, Modal } from './common';

const TORNEO_PARA = { league: 'ligas', bracket: 'copas', pool: 'pools' } as const;

const HIDDEN_KEY = 'mfv3:evento-oculto';
const SEEN_KEY = 'mfv3:premios-vistos';

const load = <T,>(key: string, fallback: T): T => {
  try {
    const raw = localStorage.getItem(key);
    return raw ? (JSON.parse(raw) as T) : fallback;
  } catch {
    return fallback;
  }
};
const save = (key: string, v: unknown) => {
  try {
    localStorage.setItem(key, JSON.stringify(v));
  } catch {
    /* sin almacenamiento */
  }
};

/** Dibujo de una pieza: escudo, copa o banner con su imagen; lo demás, un regalo. */
export function RewardArt({ item, col, size = 52 }: { item: CatalogItem; col: CatalogCollection; size?: number }) {
  const id = `cat:${item.id}`;
  if (col.tipo === 'escudo') return <Crest id={id} size={size} />;
  if (col.tipo === 'copa') return <Cup id={id} size={size} />;
  if (col.tipo === 'fondo') return <BannerArt id={id} className="reward-banner" />;
  return <span style={{ fontSize: size * 0.7 }}>🎁</span>;
}

/** Franja pequeña arriba durante un evento; al tocarla se ve qué se puede conseguir. */
export function EventNotice({ lower }: { lower: boolean }) {
  const { catalog, rewards, players } = useApp();
  const events = useMemo(() => activeEvents(catalog, Date.now()), [catalog]);
  const today = dayKey(Date.now());
  const [hidden, setHidden] = useState<string[]>(() => load<string[]>(HIDDEN_KEY, []));
  const [open, setOpen] = useState(false);
  const ev = events.find((e) => !hidden.includes(`${e.title}|${today}`));
  if (!ev) return null;
  const hide = () => {
    const next = [...hidden.filter((h) => h.endsWith(`|${today}`)), `${ev.title}|${today}`];
    setHidden(next);
    save(HIDDEN_KEY, next);
  };
  const ownersOf = (itemId: string) => players.filter((p) => rewards.byPlayer.get(p.id)?.has(itemId));
  const left = ev.rewards.filter((r) => !rewards.device.has(r.item.id) && !ownersOf(r.item.id).length).length;
  return (
    <>
      <div className={`event-notice ${lower ? 'is-lower' : ''}`} role="note" style={open ? { display: 'none' } : undefined}>
        <button className="event-notice-main" onClick={() => setOpen(true)}>
          <span aria-hidden="true">✨</span> <b>{ev.title}</b> ·{' '}
          {left ? `${left} premio${left === 1 ? '' : 's'} por conseguir` : 'premios del evento'}
        </button>
        <button className="event-notice-close" aria-label="Ocultar hasta mañana" title="Ocultar hasta mañana" onClick={hide}>
          ×
        </button>
      </div>
      {open && (
        <Modal
          title={ev.title}
          onClose={() => setOpen(false)}
          actions={
            <button className="btn btn-primary" onClick={() => setOpen(false)}>
              Vale
            </button>
          }
        >
          <div className="event-list">
            {ev.rewards.map(({ item, col, obtencion }) => {
              const owners = ownersOf(item.id);
              const mine = rewards.device.has(item.id);
              return (
                <div key={item.id} className={`event-row ${col.tipo === 'fondo' ? 'is-wide' : ''}`}>
                  <div className="event-art">
                    <RewardArt item={item} col={col} />
                  </div>
                  <div className="event-info">
                    <b>{item.nombre || col.nombre}</b>
                    <span>
                      Se consigue {requisitoTexto(obtencion)} · {cuandoTexto(col.disponibilidad)}
                      {col.torneo && col.torneo !== 'todos' ? ` · para ${TORNEO_PARA[col.torneo]}` : ''}
                    </span>
                    {(mine || owners.length > 0) && (
                      <span className="event-owners">
                        ✔ {mine ? 'Ya es vuestra' : 'La tienen'}
                        {owners.map((p) => (
                          <span key={p.id} className="event-owner" title={p.name}>
                            <Avatar name={p.name} photo={p.photo} size={22} />
                          </span>
                        ))}
                      </span>
                    )}
                  </div>
                </div>
              );
            })}
          </div>
        </Modal>
      )}
    </>
  );
}

interface Fresh {
  key: string;
  item: CatalogItem;
  col: CatalogCollection;
  playerIds: string[];
}

const DAY = 24 * 3600 * 1000;

/**
 * «¡Premio conseguido!»: sale cuando alguien gana una pieza (o al entrar el día del evento). Lo ya
 * avisado se apunta para no repetirlo; lo conseguido hace más de un día no se avisa.
 */
export function RewardPop() {
  const { catalog, rewards, players, demoMode } = useApp();
  const [shown, setShown] = useState<Fresh[]>([]);
  useEffect(() => {
    const seenKey = demoMode ? `${SEEN_KEY}:demo` : SEEN_KEY;
    const seen = new Set(load<string[]>(seenKey, []));
    const items = new Map(visibleItems(catalog).map((x) => [x.item.id, x]));
    const now = Date.now();
    const today = dayKey(now);
    const fresh = new Map<string, Fresh>();
    const add = (key: string, itemId: string, pid?: string) => {
      const x = items.get(itemId);
      if (!x) return;
      seen.add(key);
      const f = fresh.get(itemId) ?? {
        key: itemId,
        item: x.item,
        col: x.col,
        playerIds: [],
      };
      if (pid) f.playerIds.push(pid);
      fresh.set(itemId, f);
    };
    for (const [pid, list] of rewards.byPlayer)
      for (const [itemId, at] of list) {
        const key = `${pid}|${itemId}`;
        if (seen.has(key)) continue;
        if (now - at > DAY) seen.add(key);
        else add(key, itemId, pid);
      }
    for (const [itemId, at] of rewards.device) {
      const key = `aparato|${itemId}`;
      if (seen.has(key)) continue;
      if (at && dayKey(at) === today) add(key, itemId);
      else seen.add(key);
    }
    save(seenKey, [...seen]);
    if (fresh.size) setShown((s) => [...s, ...fresh.values()]);
  }, [catalog, rewards, demoMode]);

  useEffect(() => {
    if (!shown.length) return;
    const t = window.setTimeout(() => setShown((s) => s.slice(1)), 6500);
    return () => window.clearTimeout(t);
  }, [shown]);

  const cur = shown[0];
  if (!cur) return null;
  const who = cur.playerIds.map((id) => players.find((p) => p.id === id)).filter((p) => !!p);
  return (
    // No tapa nada: los toques pasan a lo que hay debajo (suele ser el botón de seguir) y se va solo.
    <div className="reward-pop" role="status">
      <span className="reward-pop-art">
        <RewardArt item={cur.item} col={cur.col} size={56} />
      </span>
      <span className="reward-pop-text">
        <small>¡Premio conseguido!</small>
        <b>{cur.item.nombre || cur.col.nombre}</b>
        {who.length > 0 ? (
          <span className="reward-pop-who">
            {who.slice(0, 3).map((p) => (
              <span key={p.id} className="event-owner">
                <Avatar name={p.name} photo={p.photo} size={22} /> {p.name}
              </span>
            ))}
            {who.length > 3 && <span className="event-owner">y {who.length - 3} más</span>}
          </span>
        ) : (
          <span>Ya se puede usar en este aparato</span>
        )}
      </span>
    </div>
  );
}
