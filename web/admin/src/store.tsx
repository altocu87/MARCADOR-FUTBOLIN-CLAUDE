/**
 * Estado del panel: datos cargados, sesión y acciones. Cada cambio deja una línea en el registro de actividad.
 */
import { createContext, useCallback, useContext, useEffect, useMemo, useState, type ReactNode } from 'react';
import type { AdminBackend } from './backend';
import { can, newId, type Permission } from './logic';
import type { AdminData, AdminSession, AuditEntry, Collection, ItemOf, Settings } from './types';

interface Ctx {
  backend: AdminBackend;
  session: AdminSession;
  data: AdminData;
  now: number;
  can: (p: Permission) => boolean;
  save: <C extends Collection>(col: C, item: ItemOf<C>, accion: string, detalle: string) => Promise<void>;
  remove: (col: Collection, id: string, accion: string, detalle: string) => Promise<void>;
  /** Varios a la vez con una sola línea en el registro de actividad. */
  saveMany: <C extends Collection>(col: C, items: ItemOf<C>[], accion: string, detalle: string) => Promise<void>;
  removeMany: (col: Collection, ids: string[], accion: string, detalle: string) => Promise<void>;
  saveSettings: (s: Settings, detalle: string) => Promise<void>;
  reload: () => Promise<void>;
  toast: (msg: string) => void;
  message: string | null;
}

const AdminCtx = createContext<Ctx | null>(null);

export function useAdmin(): Ctx {
  const c = useContext(AdminCtx);
  if (!c) throw new Error('useAdmin fuera de AdminProvider');
  return c;
}

export function AdminProvider({ backend, session, children }: { backend: AdminBackend; session: AdminSession; children: (ready: boolean, error: string | null) => ReactNode }) {
  const [data, setData] = useState<AdminData | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [message, setMessage] = useState<string | null>(null);
  const now = useMemo(() => Date.now(), [data]); // eslint-disable-line react-hooks/exhaustive-deps

  const reload = useCallback(async () => {
    try {
      setError(null);
      setData(structuredClone(await backend.load()));
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }, [backend]);

  useEffect(() => {
    void reload();
  }, [reload]);

  const toast = useCallback((msg: string) => {
    setMessage(msg);
    window.setTimeout(() => setMessage((m) => (m === msg ? null : m)), 3200);
  }, []);

  const audit = useCallback(
    async (accion: string, detalle: string) => {
      const entry: AuditEntry = { id: newId('au'), at: Date.now(), actor: session.email, accion, detalle };
      await backend.upsert('audit', entry);
      setData((d) => (d ? { ...d, audit: [entry, ...d.audit] } : d));
    },
    [backend, session.email],
  );

  const value = useMemo<Ctx | null>(() => {
    if (!data) return null;
    return {
      backend,
      session,
      data,
      now,
      can: (p) => can(session.role, p),
      toast,
      message,
      reload,
      async save(col, item, accion, detalle) {
        await backend.upsert(col, item);
        setData((d) => {
          if (!d) return d;
          const list = d[col] as ItemOf<typeof col>[];
          const i = list.findIndex((x) => x.id === item.id);
          const next = i >= 0 ? list.map((x, j) => (j === i ? item : x)) : [item, ...list];
          return { ...d, [col]: next };
        });
        await audit(accion, detalle);
      },
      async remove(col, id, accion, detalle) {
        await backend.remove(col, id);
        setData((d) => (d ? { ...d, [col]: (d[col] as { id: string }[]).filter((x) => x.id !== id) } : d));
        await audit(accion, detalle);
      },
      async saveMany(col, items, accion, detalle) {
        for (const item of items) await backend.upsert(col, item);
        setData((d) => {
          if (!d) return d;
          let list = d[col] as ItemOf<typeof col>[];
          for (const item of items) {
            const i = list.findIndex((x) => x.id === item.id);
            list = i >= 0 ? list.map((x, j) => (j === i ? item : x)) : [...list, item];
          }
          return { ...d, [col]: list };
        });
        await audit(accion, detalle);
      },
      async removeMany(col, ids, accion, detalle) {
        for (const id of ids) await backend.remove(col, id);
        setData((d) => (d ? { ...d, [col]: (d[col] as { id: string }[]).filter((x) => !ids.includes(x.id)) } : d));
        await audit(accion, detalle);
      },
      async saveSettings(s, detalle) {
        await backend.saveSettings(s);
        setData((d) => (d ? { ...d, settings: s } : d));
        await audit('Ajustes', detalle);
      },
    };
  }, [audit, backend, data, message, now, reload, session, toast]);

  return value ? <AdminCtx.Provider value={value}>{children(true, error)}</AdminCtx.Provider> : <>{children(false, error)}</>;
}
