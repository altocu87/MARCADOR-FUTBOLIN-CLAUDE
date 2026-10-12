-- Eventos del catálogo: cada colección de copas, escudos o fondos (banners) puede ser exclusiva de un tipo
-- de torneo (liga, copa o pool). Las nuevas formas de conseguir (entrar en la app, jugar o ganar un partido
-- de un tipo, ganar un torneo) viajan en la columna jsonb «obtencion», que no necesita cambios.
alter table public.catalog_collections
  add column if not exists torneo text not null default 'todos';

do $$
begin
  if not exists (select 1 from pg_constraint where conname = 'catalog_collections_torneo_check') then
    alter table public.catalog_collections
      add constraint catalog_collections_torneo_check check (torneo in ('todos', 'league', 'bracket', 'pool'));
  end if;
end $$;
