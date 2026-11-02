-- Sonidos de la SD de la pantalla, comprimidos (MP3), para que la app los baje una vez y los use sin internet.
-- Almacén privado: solo lo leen quienes han entrado con una cuenta de la app; nadie lo ve sin entrar.
-- Solo se sube con la clave de servicio (scripts/subir-sonidos-nube.py, desde el PC del dueño): nadie escribe desde la app.
-- Guía: docs/SONIDOS_NUBE.md.

insert into storage.buckets (id, name, public)
values ('sonidos', 'sonidos', false)
on conflict (id) do update set public = false;

drop policy if exists "sonidos: leen las cuentas" on storage.objects;
create policy "sonidos: leen las cuentas" on storage.objects
  for select to authenticated
  using (bucket_id = 'sonidos');
