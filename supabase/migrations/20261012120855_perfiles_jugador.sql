-- Perfil público de cada jugador (no invitados), para verlo desde el móvil leyendo su QR.
-- Cada futbolín sube una «foto» de los datos del jugador (palmarés, historial, estadísticas) ya calculada.
--
-- Seguridad (de momento, sencilla a propósito):
--   · Leer: quien tenga el código del QR (token aleatorio de 22 caracteres) ve ese perfil y nada más. No se
--     puede listar la tabla: solo se llega por la función perfil_jugador(token).
--   · Escribir: solo el aparato que lo creó, que guarda una clave secreta propia de ese perfil. En la tabla
--     se guarda la huella (sha256) de la clave, nunca la clave.
-- Más adelante, con cuentas de jugador, se endurecerá.

create table if not exists public.perfiles_jugador (
  token        text primary key check (length(token) between 16 and 64),
  clave_hash   text not null,
  datos        jsonb not null,
  creado       timestamptz not null default now(),
  actualizado  timestamptz not null default now()
);

alter table public.perfiles_jugador enable row level security;
-- Sin políticas para anon/authenticated: nadie lee ni escribe la tabla directamente, solo por las funciones.
revoke all on public.perfiles_jugador from anon, authenticated;
-- El equipo (panel) sí la puede consultar.
drop policy if exists "perfiles: el equipo los ve" on public.perfiles_jugador;
create policy "perfiles: el equipo los ve" on public.perfiles_jugador for select to authenticated using (public.is_staff());
grant select on public.perfiles_jugador to authenticated;

create or replace function public.perfil_jugador(p_token text)
returns jsonb
language sql
stable
security definer
set search_path = public
as $$
  select datos from public.perfiles_jugador where token = p_token;
$$;

create or replace function public.publicar_perfil_jugador(p_token text, p_clave text, p_datos jsonb)
returns void
language plpgsql
security definer
set search_path = public
as $$
declare
  v_hash text := encode(sha256(convert_to(coalesce(p_clave, ''), 'UTF8')), 'hex');
  v_actual text;
begin
  if p_token is null or length(p_token) < 16 or length(p_token) > 64 then
    raise exception 'Código de perfil no válido';
  end if;
  if p_clave is null or length(p_clave) < 16 then
    raise exception 'Clave no válida';
  end if;
  if p_datos is null or length(p_datos::text) > 2000000 then
    raise exception 'Perfil demasiado grande';
  end if;
  select clave_hash into v_actual from public.perfiles_jugador where token = p_token;
  if v_actual is null then
    insert into public.perfiles_jugador (token, clave_hash, datos) values (p_token, v_hash, p_datos);
  elsif v_actual = v_hash then
    update public.perfiles_jugador set datos = p_datos, actualizado = now() where token = p_token;
  else
    raise exception 'Este perfil es de otro aparato';
  end if;
end;
$$;

create or replace function public.borrar_perfil_jugador(p_token text, p_clave text)
returns void
language sql
security definer
set search_path = public
as $$
  delete from public.perfiles_jugador
  where token = p_token and clave_hash = encode(sha256(convert_to(coalesce(p_clave, ''), 'UTF8')), 'hex');
$$;

revoke all on function public.perfil_jugador(text) from public;
revoke all on function public.publicar_perfil_jugador(text, text, jsonb) from public;
revoke all on function public.borrar_perfil_jugador(text, text) from public;
grant execute on function public.perfil_jugador(text) to anon, authenticated;
grant execute on function public.publicar_perfil_jugador(text, text, jsonb) to anon, authenticated;
grant execute on function public.borrar_perfil_jugador(text, text) to anon, authenticated;
