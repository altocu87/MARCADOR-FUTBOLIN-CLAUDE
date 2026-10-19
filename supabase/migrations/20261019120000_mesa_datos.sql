-- Datos de cada mesa en la nube, en la cuenta de su dueño (app web y pantalla de 7").
--
-- Modelo (decidido por Alex): la cuenta es del DUEÑO DE LA MESA. Dentro están sus jugadores (fichas con su foto),
-- sus partidos, sus torneos y sus preferencias. Los demás jugadores no tienen cuenta: su ficha pública es la del QR
-- (perfiles_jugador).
--
-- Una fila por cosa: tipo ('player', 'match', 'tournament', 'prefs') + su id (el de la app) + el JSON tal cual lo
-- guarda la app (Player, StoredMatch, Tournament, Preferences). Así la placa, con peticiones HTTP sencillas, sube un
-- partido o baja lo que ha cambiado desde una fecha (docs/NUBE.md).
--   · `actualizado` lo pone siempre el servidor (al crear y al cambiar): sirve para «dame lo nuevo desde…».
--   · Borrar = `borrado = true` (la fila se queda, para que los demás aparatos se enteren y lo borren también).
--   · Las fotos de los jugadores van dentro del JSON (data URL JPEG de 160 × 160, unos 10 KB).
--
-- Seguridad: cada dueño solo ve y toca sus filas (RLS con auth.uid()). Sin sesión, nada.

create table if not exists public.mesa_datos (
  owner_id    uuid not null default auth.uid() references auth.users (id) on delete cascade,
  tipo        text not null check (tipo in ('player', 'match', 'tournament', 'prefs')),
  id          text not null check (length(id) between 1 and 80),
  datos       jsonb not null,
  borrado     boolean not null default false,
  actualizado timestamptz not null default now(),
  primary key (owner_id, tipo, id)
);

create index if not exists mesa_datos_cambios on public.mesa_datos (owner_id, actualizado);

-- Límite de tamaño por fila (un partido largo ocupa ~60 KB; un torneo grande, algo más).
alter table public.mesa_datos drop constraint if exists mesa_datos_tamano;
alter table public.mesa_datos add constraint mesa_datos_tamano check (pg_column_size(datos) < 1500000);

-- La hora del cambio la pone el servidor (no la del aparato, que puede ir mal).
create or replace function public.mesa_datos_actualizado() returns trigger
  language plpgsql as $$
begin
  new.actualizado := now();
  new.owner_id := coalesce(new.owner_id, auth.uid());
  return new;
end;
$$;

drop trigger if exists mesa_datos_actualizado on public.mesa_datos;
create trigger mesa_datos_actualizado before insert or update on public.mesa_datos
  for each row execute function public.mesa_datos_actualizado();

alter table public.mesa_datos enable row level security;

drop policy if exists "mesa: el dueño ve lo suyo" on public.mesa_datos;
drop policy if exists "mesa: el dueño añade lo suyo" on public.mesa_datos;
drop policy if exists "mesa: el dueño cambia lo suyo" on public.mesa_datos;
drop policy if exists "mesa: el dueño borra lo suyo" on public.mesa_datos;
create policy "mesa: el dueño ve lo suyo" on public.mesa_datos for select to authenticated using (owner_id = auth.uid());
create policy "mesa: el dueño añade lo suyo" on public.mesa_datos for insert to authenticated with check (owner_id = auth.uid());
create policy "mesa: el dueño cambia lo suyo" on public.mesa_datos for update to authenticated
  using (owner_id = auth.uid()) with check (owner_id = auth.uid());
create policy "mesa: el dueño borra lo suyo" on public.mesa_datos for delete to authenticated using (owner_id = auth.uid());

revoke all on public.mesa_datos from anon;
grant select, insert, update, delete on public.mesa_datos to authenticated;
