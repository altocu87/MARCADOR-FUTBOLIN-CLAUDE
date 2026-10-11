-- Acceso de la API a las tablas. Los proyectos nuevos de Supabase no lo dan solo a las tablas creadas por
-- migraciones («permission denied for table …»). Esto abre la puerta; QUÉ filas ve y toca cada uno lo siguen
-- decidiendo las políticas RLS de la migración inicial (sin política, no hay acceso aunque haya permiso).

grant usage on schema public to anon, authenticated;

-- Usuarios con sesión: todo, filtrado por RLS (y por las guardias de perfiles y pedidos).
grant select, insert, update, delete on all tables in schema public to authenticated;

-- Sin sesión: solo leer lo que es público por política (productos activos, avisos activos y ajustes).
grant select on public.products, public.announcements, public.settings to anon;

grant execute on all functions in schema public to anon, authenticated;

-- Y lo mismo para lo que se cree en migraciones futuras.
alter default privileges in schema public grant select, insert, update, delete on tables to authenticated;
alter default privileges in schema public grant execute on functions to anon, authenticated;
