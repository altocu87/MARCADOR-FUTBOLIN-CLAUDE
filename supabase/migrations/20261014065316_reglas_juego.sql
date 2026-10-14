-- Reglas del juego (ELO, rangos, experiencia y niveles) que se configuran en el panel → «Reglas del juego».
-- Una sola fila («global») con las reglas en JSON. La app la descarga sin cuenta (como el catálogo) y la aplica
-- en todas las mesas; sin nada publicado, la app usa sus valores recomendados.
-- Solo un administrador (con verificación en dos pasos) la cambia.

create table if not exists public.game_rules (
  id          text primary key check (id = 'global'),
  data        jsonb not null,
  updated_at  bigint not null,
  updated_by  text not null default ''
);

alter table public.game_rules enable row level security;

drop policy if exists "reglas: públicas" on public.game_rules;
create policy "reglas: públicas" on public.game_rules for select using (true);
drop policy if exists "reglas: admin escribe" on public.game_rules;
create policy "reglas: admin escribe" on public.game_rules for all
  using (public.is_admin()) with check (public.is_admin());

grant select on public.game_rules to anon;
grant select, insert, update, delete on public.game_rules to authenticated;
