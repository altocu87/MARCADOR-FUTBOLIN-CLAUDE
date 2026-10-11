-- =====================================================================================
-- MARCADOR FUTBOLÍN · base de datos del servicio de pago (Supabase / PostgreSQL)
-- Puesta en marcha: Supabase → SQL Editor → pegar y ejecutar (pasos completos en docs/ADMIN.md).
--
-- El equipo (admin y soporte) solo tiene permisos si a la vez:
--   1) su perfil tiene ese rol, 2) su cuenta no está bloqueada y 3) ha entrado con la verificación en dos pasos
--   (código de una app, nivel «aal2» del token). Con solo la contraseña, aunque sea correcta, no ve nada.
--
-- Seguridad: todas las tablas tienen RLS (Row Level Security). Los permisos se deciden aquí, en el
-- servidor, según el rol de la tabla profiles. El panel solo muestra lo que el servidor deja ver.
--   admin   → todo
--   soporte → usuarios, bares, envíos y licencias; NO reembolsos, precios, tienda, avisos ni ajustes
--   bar     → su local y sus pedidos
--   jugador → su perfil, sus pedidos y sus licencias
-- Fechas en milisegundos (bigint) e importes en céntimos (integer), igual que el panel.
-- Las columnas usan snake_case (createdAt ↔ created_at); el panel convierte solo.
-- =====================================================================================

-- ---------------------------------------------------------------- perfiles (1 por usuario de Auth)
create table if not exists public.profiles (
  id            uuid primary key references auth.users (id) on delete cascade,
  email         text not null,
  nombre        text not null default '',
  role          text not null default 'jugador' check (role in ('admin', 'soporte', 'bar', 'jugador')),
  plan          text not null default 'gratis' check (plan in ('gratis', 'pro')),
  pais          text not null default 'ES',
  created_at    bigint not null default (extract(epoch from now()) * 1000)::bigint,
  last_seen_at  bigint not null default (extract(epoch from now()) * 1000)::bigint,
  bloqueado     boolean not null default false,
  notas         text not null default '',
  venue_id      text
);

-- Rol del usuario que hace la petición (security definer: no depende de las políticas de profiles).
-- Sin verificación en dos pasos (aal2) o con la cuenta bloqueada, el equipo no tiene rol: no ve ni toca nada.
create or replace function public.staff_role() returns text
  language sql stable security definer set search_path = public
  as $$
    select role from public.profiles
    where id = auth.uid() and not bloqueado and coalesce(auth.jwt() ->> 'aal', '') = 'aal2'
  $$;
create or replace function public.is_admin() returns boolean
  language sql stable as $$ select coalesce(public.staff_role() = 'admin', false) $$;
create or replace function public.is_staff() returns boolean
  language sql stable as $$ select coalesce(public.staff_role() in ('admin', 'soporte'), false) $$;

-- Al registrarse alguien, se crea su perfil de jugador.
create or replace function public.on_auth_user_created() returns trigger
  language plpgsql security definer set search_path = public as $$
begin
  insert into public.profiles (id, email, nombre) values (new.id, new.email, coalesce(new.raw_user_meta_data->>'nombre', ''));
  return new;
end $$;
drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created after insert on auth.users for each row execute function public.on_auth_user_created();

-- Nadie se sube el rol, el plan ni se desbloquea a sí mismo; solo un admin cambia roles.
create or replace function public.guard_profile() returns trigger language plpgsql as $$
begin
  if new.role is distinct from old.role and not public.is_admin() then
    raise exception 'Solo un administrador puede cambiar roles';
  end if;
  if (new.plan is distinct from old.plan or new.bloqueado is distinct from old.bloqueado or new.notas is distinct from old.notas)
     and not public.is_staff() then
    raise exception 'Sin permiso para cambiar plan, bloqueo o notas';
  end if;
  return new;
end $$;
drop trigger if exists guard_profile on public.profiles;
create trigger guard_profile before update on public.profiles for each row execute function public.guard_profile();

-- ---------------------------------------------------------------- tablas del negocio
create table if not exists public.products (
  id          text primary key,
  nombre      text not null,
  tipo        text not null check (tipo in ('pro', 'tema', 'sonido', 'pack', 'suscripcion', 'kit')),
  precio      integer not null check (precio >= 0),
  activo      boolean not null default false,
  descripcion text not null default '',
  stock       integer check (stock is null or stock >= 0)
);

create table if not exists public.venues (
  id          text primary key,
  nombre      text not null,
  ciudad      text not null default '',
  owner_id    uuid references public.profiles (id) on delete set null,
  mesas       integer not null default 1 check (mesas between 1 and 50),
  plan        text not null check (plan in ('bar-basico', 'bar-pro')),
  estado      text not null check (estado in ('prueba', 'activa', 'impagada', 'cancelada')),
  cuota       integer not null default 0,
  desde       bigint not null,
  renovacion  bigint not null
);

create table if not exists public.orders (
  id             text primary key,
  user_id        uuid references public.profiles (id) on delete set null,
  product_id     text not null references public.products (id),
  importe        integer not null check (importe >= 0),
  estado         text not null check (estado in ('pendiente', 'pagado', 'reembolsado', 'fallido')),
  fecha          bigint not null,
  cupon          text,
  proveedor_ref  text unique,
  envio          jsonb
);
create index if not exists orders_user on public.orders (user_id);
create index if not exists orders_fecha on public.orders (fecha desc);

-- Soporte gestiona envíos, pero el dinero (estado e importe) solo lo toca un admin o el webhook de pagos.
create or replace function public.guard_order() returns trigger language plpgsql as $$
begin
  if (new.estado is distinct from old.estado or new.importe is distinct from old.importe) and not public.is_admin()
     and coalesce(auth.role(), '') <> 'service_role' then
    raise exception 'Solo un administrador cambia el estado o el importe de un pedido';
  end if;
  return new;
end $$;
drop trigger if exists guard_order on public.orders;
create trigger guard_order before update on public.orders for each row execute function public.guard_order();

create table if not exists public.licenses (
  id                text primary key,
  code              text not null unique,
  estado            text not null check (estado in ('libre', 'activa', 'revocada')),
  user_id           uuid references public.profiles (id) on delete set null,
  order_id          text references public.orders (id) on delete set null,
  creada            bigint not null,
  activaciones      integer not null default 0,
  max_activaciones  integer not null default 3
);

create table if not exists public.coupons (
  id        text primary key,
  code      text not null unique,
  tipo      text not null check (tipo in ('porcentaje', 'fijo')),
  valor     integer not null check (valor > 0),
  usos      integer not null default 0,
  max_usos  integer,
  caduca    bigint,
  activo    boolean not null default true
);

create table if not exists public.announcements (
  id      text primary key,
  titulo  text not null,
  texto   text not null default '',
  nivel   text not null default 'info' check (nivel in ('info', 'aviso')),
  activo  boolean not null default false,
  desde   bigint not null,
  hasta   bigint
);

-- Registro de actividad: solo se añade; nadie (ni un admin) puede editarlo ni borrarlo desde la API.
create table if not exists public.audit_log (
  id       text primary key,
  at       bigint not null,
  actor    text not null,
  accion   text not null,
  detalle  text not null default ''
);

create table if not exists public.settings (
  id                 text primary key check (id = 'ajustes'),
  nombre_tienda      text not null,
  email_soporte      text not null,
  moneda             text not null default 'EUR',
  iva_incluido       boolean not null default true,
  proveedor_pagos    text not null default 'ninguno' check (proveedor_pagos in ('ninguno', 'stripe', 'lemonsqueezy', 'paddle')),
  precio_pro         integer not null,
  precio_bar_basico  integer not null,
  precio_bar_pro     integer not null,
  mantenimiento      boolean not null default false
);

-- ---------------------------------------------------------------- RLS: permisos
alter table public.profiles      enable row level security;
alter table public.products      enable row level security;
alter table public.venues        enable row level security;
alter table public.orders        enable row level security;
alter table public.licenses      enable row level security;
alter table public.coupons       enable row level security;
alter table public.announcements enable row level security;
alter table public.audit_log     enable row level security;
alter table public.settings      enable row level security;

-- profiles
create policy "perfil: ver el propio o equipo" on public.profiles for select using (id = auth.uid() or public.is_staff());
create policy "perfil: editar el propio o equipo" on public.profiles for update using (id = auth.uid() or public.is_staff());

-- products: el catálogo activo es público; el equipo ve también lo oculto; solo admin edita
create policy "productos: públicos si activos" on public.products for select using (activo or public.is_staff());
create policy "productos: admin escribe" on public.products for all using (public.is_admin()) with check (public.is_admin());

-- venues
create policy "bares: dueño o equipo ven" on public.venues for select using (owner_id = auth.uid() or public.is_staff());
create policy "bares: equipo edita" on public.venues for update using (public.is_staff());
create policy "bares: admin crea y borra" on public.venues for insert with check (public.is_admin());
create policy "bares: admin borra" on public.venues for delete using (public.is_admin());

-- orders (los pedidos los crea el webhook de pagos con la clave de servicio, que se salta RLS)
create policy "pedidos: propios o equipo" on public.orders for select using (user_id = auth.uid() or public.is_staff());
create policy "pedidos: equipo actualiza (envíos)" on public.orders for update using (public.is_staff());
create policy "pedidos: admin crea" on public.orders for insert with check (public.is_admin());

-- licenses
create policy "licencias: propias o equipo" on public.licenses for select using (user_id = auth.uid() or public.is_staff());
create policy "licencias: equipo gestiona" on public.licenses for all using (public.is_staff()) with check (public.is_staff());

-- coupons (la validación en la compra la hace el servidor de pagos)
create policy "cupones: equipo ve" on public.coupons for select using (public.is_staff());
create policy "cupones: admin escribe" on public.coupons for all using (public.is_admin()) with check (public.is_admin());

-- announcements
create policy "avisos: públicos si activos" on public.announcements for select using (activo or public.is_staff());
create policy "avisos: admin escribe" on public.announcements for all using (public.is_admin()) with check (public.is_admin());

-- audit_log (sin políticas de update/delete: no se puede modificar)
create policy "registro: equipo ve" on public.audit_log for select using (public.is_staff());
create policy "registro: equipo añade" on public.audit_log for insert with check (public.is_staff() and actor = (select email from public.profiles where id = auth.uid()));

-- settings: los precios son públicos (la app los muestra); solo admin cambia
create policy "ajustes: públicos" on public.settings for select using (true);
create policy "ajustes: admin escribe" on public.settings for all using (public.is_admin()) with check (public.is_admin());

-- ---------------------------------------------------------------- datos iniciales
insert into public.settings (id, nombre_tienda, email_soporte, precio_pro, precio_bar_basico, precio_bar_pro)
values ('ajustes', 'Marcador Futbolín', 'soporte@ejemplo.com', 499, 900, 1500)
on conflict (id) do nothing;

insert into public.products (id, nombre, tipo, precio, activo, descripcion, stock) values
  ('p_pro', 'Marcador Pro', 'pro', 499, false, 'Desbloqueo de por vida: torneos avanzados, estadísticas extendidas y temas.', null),
  ('p_bar_basico', 'Bar Básico (por mesa y mes)', 'suscripcion', 900, false, 'Ranking del bar y ligas internas.', null),
  ('p_bar_pro', 'Bar Pro (por mesa y mes)', 'suscripcion', 1500, false, 'Ligas entre bares, marcador en la tele y estadísticas.', null),
  ('p_kit_7c', 'Kit pantalla 7" + mando', 'kit', 12900, false, 'Waveshare 7C grabada y probada + mando inalámbrico con 2 pulsadores.', 0)
on conflict (id) do nothing;

-- ---------------------------------------------------------------- primer administrador
-- 1) Authentication → Sign In / Providers: desactiva «Allow new users to sign up» (nadie puede registrarse solo).
-- 2) Authentication → Users → «Add user» con tu correo y una contraseña larga (16+ caracteres).
-- 3) Ejecuta (con tu correo):
--    update public.profiles set role = 'admin' where email = 'tu-correo@ejemplo.com';
-- 4) Entra en el panel: la primera vez te enseña un código QR para tu app de verificación.
