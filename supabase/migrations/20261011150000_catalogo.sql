-- Catálogo de personalización: colecciones (copas, escudos, avatares, marcos, fondos, podios, títulos, colores
-- y sonidos) y sus piezas, con rareza, cómo se consiguen y cuándo. Los archivos van al almacén público «catalogo».
-- Lo publicado lo puede leer cualquiera (la app lo descarga); solo un administrador (con 2FA) crea y cambia.

create table if not exists public.catalog_collections (
  id                 text primary key,
  tipo               text not null check (tipo in ('copa', 'escudo', 'avatar', 'marco', 'fondo', 'podio', 'titulo', 'color', 'sonido')),
  nombre             text not null,
  descripcion        text not null default '',
  rareza             text not null default 'comun' check (rareza in ('comun', 'rara', 'epica', 'legendaria', 'mitica')),
  estado             text not null default 'borrador' check (estado in ('borrador', 'publicada', 'retirada')),
  ambito             text not null default 'coleccion' check (ambito in ('coleccion', 'elemento')),
  obtencion          jsonb not null default '{"tipo": "gratis"}',
  disponibilidad     jsonb not null default '{"modo": "siempre"}',
  premio_completar   integer check (premio_completar is null or premio_completar >= 0),
  orden              integer not null default 0,
  creada             bigint not null
);

create table if not exists public.catalog_items (
  id            text primary key,
  coleccion_id  text not null references public.catalog_collections (id) on delete cascade,
  nombre        text not null,
  rareza        text check (rareza is null or rareza in ('comun', 'rara', 'epica', 'legendaria', 'mitica')),
  archivo       text,
  ruta          text,
  valor         text,
  obtencion     jsonb,
  orden         integer not null default 0,
  activo        boolean not null default true
);
create index if not exists catalog_items_coleccion on public.catalog_items (coleccion_id);

alter table public.catalog_collections enable row level security;
alter table public.catalog_items       enable row level security;

-- Leer: lo publicado (o retirado, para quien ya lo tenga) cualquiera; los borradores, solo el equipo.
create policy "catálogo: colecciones visibles" on public.catalog_collections for select
  using (estado <> 'borrador' or public.is_staff());
create policy "catálogo: admin escribe colecciones" on public.catalog_collections for all
  using (public.is_admin()) with check (public.is_admin());

create policy "catálogo: piezas visibles" on public.catalog_items for select
  using (
    public.is_staff()
    or (activo and exists (select 1 from public.catalog_collections c where c.id = coleccion_id and c.estado <> 'borrador'))
  );
create policy "catálogo: admin escribe piezas" on public.catalog_items for all
  using (public.is_admin()) with check (public.is_admin());

grant select on public.catalog_collections, public.catalog_items to anon;
grant select, insert, update, delete on public.catalog_collections, public.catalog_items to authenticated;

-- ---------------------------------------------------------------- almacén de archivos
insert into storage.buckets (id, name, public, file_size_limit, allowed_mime_types)
values (
  'catalogo', 'catalogo', true, 1048576,
  array['image/webp', 'image/png', 'image/jpeg', 'audio/mpeg', 'audio/ogg', 'audio/wav', 'audio/x-wav']
)
on conflict (id) do nothing;

-- Cualquiera descarga (es público); solo un administrador sube, reemplaza o borra.
create policy "catálogo: archivos públicos" on storage.objects for select
  using (bucket_id = 'catalogo');
create policy "catálogo: admin sube archivos" on storage.objects for insert to authenticated
  with check (bucket_id = 'catalogo' and public.is_admin());
create policy "catálogo: admin reemplaza archivos" on storage.objects for update to authenticated
  using (bucket_id = 'catalogo' and public.is_admin()) with check (bucket_id = 'catalogo' and public.is_admin());
create policy "catálogo: admin borra archivos" on storage.objects for delete to authenticated
  using (bucket_id = 'catalogo' and public.is_admin());
