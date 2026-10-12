-- Halloween 2026 en el catálogo del panel, para poder editarlo desde allí (cómo se consigue, fechas, quitarlo…).
-- Son las mismas piezas que trae la app dentro (BUNDLED_CATALOG en src/services/events/catalogSource.ts), con
-- los mismos ids: lo que se cambie aquí manda sobre lo de la app. «archivo» es el nombre de la imagen que trae
-- la app (el panel la sabe enseñar); si se sube otra desde el panel, pasa a ser su dirección.
-- «on conflict do nothing»: volver a ejecutarla no pisa lo que ya se haya editado.
-- Fechas: el 31 de octubre de 2026 entero en hora de España.

insert into public.catalog_collections (id, tipo, nombre, descripcion, rareza, estado, ambito, obtencion, disponibilidad, torneo, orden, creada) values
  ('app-halloween-2026-escudo', 'escudo', 'Halloween 2026 · Escudo de liga', '', 'epica', 'publicada', 'coleccion', '{"tipo":"entrar"}'::jsonb, '{"modo":"fechas","desde":1793401200000,"hasta":1793487599000}'::jsonb, 'league', 0, 1793401200000),
  ('app-halloween-2026-banner', 'fondo', 'Halloween 2026 · Banner de liga', '', 'epica', 'publicada', 'coleccion', '{"tipo":"jugarPartido","modo":"cualquiera"}'::jsonb, '{"modo":"fechas","desde":1793401200000,"hasta":1793487599000}'::jsonb, 'league', 1, 1793401200000),
  ('app-halloween-2026-copa', 'copa', 'Halloween 2026 · Copa de liga', '', 'epica', 'publicada', 'coleccion', '{"tipo":"ganarPartido","modo":"cualquiera"}'::jsonb, '{"modo":"fechas","desde":1793401200000,"hasta":1793487599000}'::jsonb, 'league', 2, 1793401200000),
  ('app-halloween-2026-copa-escudo', 'escudo', 'Halloween 2026 · Escudo de copa', '', 'epica', 'publicada', 'coleccion', '{"tipo":"entrar"}'::jsonb, '{"modo":"fechas","desde":1793401200000,"hasta":1793487599000}'::jsonb, 'bracket', 3, 1793401200000),
  ('app-halloween-2026-copa-banner', 'fondo', 'Halloween 2026 · Banner de copa', '', 'epica', 'publicada', 'coleccion', '{"tipo":"jugarPartido","modo":"cualquiera"}'::jsonb, '{"modo":"fechas","desde":1793401200000,"hasta":1793487599000}'::jsonb, 'bracket', 4, 1793401200000),
  ('app-halloween-2026-copa-copa', 'copa', 'Halloween 2026 · Copa de copa', '', 'epica', 'publicada', 'coleccion', '{"tipo":"ganarPartido","modo":"cualquiera"}'::jsonb, '{"modo":"fechas","desde":1793401200000,"hasta":1793487599000}'::jsonb, 'bracket', 5, 1793401200000),
  ('app-halloween-2026-pool-escudo', 'escudo', 'Halloween 2026 · Escudo de pool', '', 'epica', 'publicada', 'coleccion', '{"tipo":"entrar"}'::jsonb, '{"modo":"fechas","desde":1793401200000,"hasta":1793487599000}'::jsonb, 'pool', 6, 1793401200000),
  ('app-halloween-2026-pool-banner', 'fondo', 'Halloween 2026 · Banner de pool', '', 'epica', 'publicada', 'coleccion', '{"tipo":"jugarPartido","modo":"cualquiera"}'::jsonb, '{"modo":"fechas","desde":1793401200000,"hasta":1793487599000}'::jsonb, 'pool', 7, 1793401200000),
  ('app-halloween-2026-pool-copa', 'copa', 'Halloween 2026 · Copa de pool', '', 'epica', 'publicada', 'coleccion', '{"tipo":"ganarPartido","modo":"cualquiera"}'::jsonb, '{"modo":"fechas","desde":1793401200000,"hasta":1793487599000}'::jsonb, 'pool', 8, 1793401200000)
on conflict (id) do nothing;

insert into public.catalog_items (id, coleccion_id, nombre, archivo, orden, activo) values
  ('app-halloween-2026-escudo-1', 'app-halloween-2026-escudo', 'Escudo fantasma 2026', 'evento-halloween-2026-liga-escudo', 0, true),
  ('app-halloween-2026-banner-1', 'app-halloween-2026-banner', 'Banner noche de brujas 2026', 'evento-halloween-2026-liga-banner', 0, true),
  ('app-halloween-2026-copa-1', 'app-halloween-2026-copa', 'Copa calabaza 2026', 'evento-halloween-2026-liga-copa', 0, true),
  ('app-halloween-2026-copa-escudo-1', 'app-halloween-2026-copa-escudo', 'Escudo calabaza 2026', 'evento-halloween-2026-copa-escudo', 0, true),
  ('app-halloween-2026-copa-banner-1', 'app-halloween-2026-copa-banner', 'Banner calabazas 2026', 'evento-halloween-2026-copa-banner', 0, true),
  ('app-halloween-2026-copa-copa-1', 'app-halloween-2026-copa-copa', 'Copa fantasma 2026', 'evento-halloween-2026-copa-copa', 0, true),
  ('app-halloween-2026-pool-escudo-1', 'app-halloween-2026-pool-escudo', 'Escudo futbolín embrujado 2026', 'evento-halloween-2026-pool-escudo', 0, true),
  ('app-halloween-2026-pool-banner-1', 'app-halloween-2026-pool-banner', 'Banner barras fantasma 2026', 'evento-halloween-2026-pool-banner', 0, true),
  ('app-halloween-2026-pool-copa-1', 'app-halloween-2026-pool-copa', 'Copa calabaza negra 2026', 'evento-halloween-2026-pool-copa', 0, true)
on conflict (id) do nothing;
