-- Las guardias de perfiles y pedidos solo se aplican a quien entra por la API (la app, la web y el panel:
-- roles «anon» y «authenticated»). El dueño del proyecto desde el SQL Editor de Supabase (rol «postgres»)
-- y el servidor de pagos (clave de servicio) quedan fuera: si no, nadie podría nombrar al primer administrador.

create or replace function public.guard_profile() returns trigger language plpgsql as $$
begin
  if current_user not in ('anon', 'authenticated') then
    return new;
  end if;
  if new.role is distinct from old.role and not public.is_admin() then
    raise exception 'Solo un administrador puede cambiar roles';
  end if;
  if (new.plan is distinct from old.plan or new.bloqueado is distinct from old.bloqueado or new.notas is distinct from old.notas)
     and not public.is_staff() then
    raise exception 'Sin permiso para cambiar plan, bloqueo o notas';
  end if;
  return new;
end $$;

create or replace function public.guard_order() returns trigger language plpgsql as $$
begin
  if current_user not in ('anon', 'authenticated') then
    return new;
  end if;
  if (new.estado is distinct from old.estado or new.importe is distinct from old.importe) and not public.is_admin() then
    raise exception 'Solo un administrador cambia el estado o el importe de un pedido';
  end if;
  return new;
end $$;
