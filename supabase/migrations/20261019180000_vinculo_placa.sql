-- Entrar en la pantalla de 7" con un QR (sin escribir la contraseña en la placa).
--
--  1. La placa inventa un código aleatorio largo (no se puede adivinar) y lo enseña en un QR con la dirección de la
--     app: …/app/#vincular=<código>.
--  2. El dueño lo lee con el móvil; la app (con su sesión) llama a la función `vincular-placa`
--     (supabase/functions/vincular-placa): el servidor prepara un enlace mágico de un solo uso para su cuenta y lo
--     deja aquí, bajo ese código. No se manda ningún correo.
--  3. La placa pregunta cada 2 s por su código con `vinculo_placa(código)`: la primera vez que hay algo se lo lleva
--     (y se borra) y con él abre su PROPIA sesión (/auth/v1/verify). La contraseña nunca pasa por la placa.
-- Cada vínculo vale 5 minutos y una sola vez.

create table if not exists public.vinculos_placa (
  codigo     text primary key check (codigo ~ '^[A-Za-z0-9]{24,64}$'),
  token_hash text not null,
  email      text not null,
  creado     timestamptz not null default now()
);

alter table public.vinculos_placa enable row level security;
-- Sin políticas: nadie la lee ni la escribe directamente (la escribe la función con permisos de servidor).
revoke all on public.vinculos_placa from anon, authenticated;

create or replace function public.vinculo_placa(p_codigo text)
returns jsonb
language plpgsql
security definer
set search_path = public
as $$
declare
  r public.vinculos_placa;
begin
  delete from public.vinculos_placa where creado < now() - interval '10 minutes';
  if p_codigo is null or p_codigo !~ '^[A-Za-z0-9]{24,64}$' then
    return null;
  end if;
  select * into r from public.vinculos_placa where codigo = p_codigo;
  if r.codigo is null then
    return null;
  end if;
  delete from public.vinculos_placa where codigo = p_codigo;  -- un solo uso
  if r.creado < now() - interval '5 minutes' then
    return null;
  end if;
  return jsonb_build_object('token_hash', r.token_hash, 'email', r.email);
end;
$$;

revoke all on function public.vinculo_placa(text) from public;
grant execute on function public.vinculo_placa(text) to anon, authenticated;
