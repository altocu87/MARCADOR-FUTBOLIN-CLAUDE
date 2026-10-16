# Panel de administración

Dirección: **`/admin/`** de la web (por ejemplo `https://altocu87.github.io/MARCADOR-FUTBOLIN-CLAUDE/admin/`).
También desde la app: **Ajustes → Sistema → Administración → «Abrir panel de administración»**.
No aparece en el menú de la web y pide a los buscadores que no lo indexen.

## Seguridad

- **Entrar pide tres cosas**: correo, contraseña y el **código de 6 cifras** de una app de verificación
  (Google Authenticator, Microsoft Authenticator, 1Password…). La primera vez se da de alta escaneando un QR.
- **Lo comprueba el servidor**, no la página: sin rol admin/soporte, con la cuenta bloqueada o sin haber pasado el
  código (nivel «aal2»), la base de datos no devuelve nada aunque alguien modificase el panel.
- La sesión vive solo en esa pestaña, se cierra sola tras **15 minutos sin actividad** y al salir se revoca en el servidor.
- El panel publicado solo carga su propio código y solo habla con Supabase (política de seguridad de contenidos),
  y no se deja abrir dentro de otra página.
- Con la conexión fijada al publicar (variables de abajo), el modo demostración desaparece.

## Hoy: modo demostración

- El panel funciona **completo** con **datos de ejemplo** (un año ficticio de negocio) guardados **solo en tu navegador**.
- La primera vez te pide crear tu cuenta de administrador (correo + contraseña de 8 caracteres o más).
- **No es seguridad real**: la contraseña solo evita miradas casuales. No hay datos reales que proteger: cada
  navegador tiene sus propios datos de ejemplo y nadie ve los tuyos.
- Ajustes → Datos → «Restablecer datos de demostración» vuelve a empezar.

## Qué se puede hacer

| Sección | Para qué |
|---|---|
| **Resumen** | Ingresos del mes (comparados con los mismos días del mes anterior), ingresos recurrentes de bares, usuarios, % Pro, tareas pendientes, ingresos por mes y por tipo, últimas ventas |
| **Ventas** | Buscar y filtrar pedidos (periodo, estado, tipo), ver detalle, **reembolsar** (revoca sus licencias Pro), exportar CSV |
| **Envíos** | Kits por estado: pendiente → grabando → probado → enviado → entregado. No deja enviar sin marcar placa grabada y probada y sin nº de seguimiento |
| **Usuarios** | Buscar, filtrar por rol y plan, dar o quitar Pro, cambiar rol, bloquear, notas internas, ver compras y licencias, exportar CSV |
| **Bares y clubes** | Plan, mesas, estado de la suscripción y cuota (se calcula con los precios de Ajustes) |
| **Licencias Pro** | Generar códigos (se descargan en CSV), comprobar un código (detecta errores de tecleo), asignar a un cliente, revocar, reactivar, reiniciar activaciones |
| **Tienda y cupones** | Productos (precio, a la venta u oculto, stock de kits) y cupones (porcentaje o importe, usos máximos, caducidad) |
| **Catálogo** | Copas, escudos, avatares, marcos, fondos, podios, títulos, colores y sonidos, en colecciones con rareza, cómo se consiguen (gratis, entrando en la app, jugando o ganando un partido —rápido, loco, clasificatorio, de liga, copa o pool—, ganando una liga, copa o pool, nivel, rango, logro, torneos, partidos, evento, monedas, dinero, código o regalo; la colección entera o pieza a pieza), cuándo (siempre, entre fechas o cada año: Navidad, Halloween…) y, en copas, escudos y banners, para qué torneos (todos o solo liga, copa o pool). Se suben varios archivos a la vez y se ajustan solos al formato. Solo administradores |
| **Reglas del juego** | ELO (inicial, puntos en juego, partidos de colocación, premiar goleadas), ELO mínimo de cada rango, duración de la temporada y retos sí/no, experiencia por partido, logros y retos, y la curva de niveles. Cada dato con qué significa, el valor recomendado y por qué, y debajo de cada grupo un ejemplo de cómo queda. Al publicar, todas las mesas lo descargan y recalculan la progresión desde el historial. Solo administradores |
| **Comportamiento de la app** | Partido suelto por defecto (cómo se gana, goles, minutos, primer lanzador), sonido y efectos de una mesa nueva (volumen, sonido de gol, locutor, efectos) y pantalla y placa (mantener encendida, reposo, reconectar la placa). Se publica junto con las reglas. Solo administradores |
| **Avisos** | Mensajes para los usuarios de la app (novedades, mantenimiento) con fechas y vista previa |
| **Actividad** | Registro de todo lo que se cambia en el panel: quién, cuándo y qué. No se puede editar. Exportar CSV |
| **Ajustes** | Nombre y correo de soporte, precios, IVA incluido, modo mantenimiento, plataforma de pagos, equipo, conexión con la base de datos, exportar todo |

## Roles

| Rol | Panel | Puede |
|---|---|---|
| **Administrador** | Sí | Todo |
| **Soporte** | Sí | Usuarios, bares, envíos y licencias. **No**: reembolsos, cambiar roles, tienda, avisos ni ajustes |
| **Dueño de bar** | No | (Su local, en la app, cuando exista el servicio online) |
| **Jugador** | No | (Su perfil y sus compras) |

En modo real los permisos los comprueba **el servidor** (`supabase/migrations/`), no el panel: aunque alguien
modificara la página, sin el rol adecuado la base de datos no le devuelve nada.

## Puesta en marcha con Supabase

1. **Base de datos**: un proyecto en [Supabase](https://supabase.com) en la UE (p. ej. Irlanda o Frankfurt).
2. **Migraciones automáticas** (crean tablas, roles y permisos; la base de datos debe empezar **vacía**). En GitHub,
   *Settings → Secrets and variables → Actions*:
   - Variable `SUPABASE_PROJECT_ID`: el Project ID (Supabase → *Project Settings → General*).
   - Secreto `SUPABASE_ACCESS_TOKEN`: token personal (Supabase → avatar → *Account preferences → Access Tokens*).
   - Secreto `SUPABASE_DB_PASSWORD`: la contraseña de la base de datos del proyecto.

   Al subir a `main` cualquier archivo nuevo de `supabase/migrations/`, el flujo «Base de datos» lo aplica
   (`supabase db push`). También se puede lanzar a mano desde *Actions*. En una PR solo se comprueba qué aplicaría.
   Sin GitHub: pega `supabase/migrations/20261011000000_inicial.sql` en el **SQL Editor** y ejecútalo.
   **Para cambiar la base de datos** no se edita una migración ya aplicada: se añade otra con fecha posterior.
3. **Cerrar el registro**: *Authentication → Sign In / Providers* → desactiva «Allow new users to sign up».
   *Authentication → Policies/Passwords*: longitud mínima 12 o más.
4. **Tu cuenta**: *Authentication → Users → Add user* con tu correo y una contraseña larga. Después, en el SQL Editor:
   `update public.profiles set role = 'admin' where email = 'tu-correo@ejemplo.com';`
5. **Conectar la web publicada**: en GitHub, *Settings → Secrets and variables → Actions → Variables*, crea
   `SUPABASE_URL` (`https://<id-del-proyecto>.supabase.co`) y `SUPABASE_ANON_KEY` (Supabase → *Project Settings → API*,
   clave **anon / publishable**). Al subir a `main` la web se publica ya conectada.
   La clave **service_role / secret** no se pone **nunca** ahí ni en el repositorio.
6. Entra en el panel: la primera vez escanea el QR con tu app de verificación.
7. **Activa también la verificación en dos pasos** en tu cuenta de Supabase y en la de GitHub.
6. **Pagos** (siguiente bloque de trabajo): elegir plataforma (Lemon Squeezy o Paddle si no quieres gestionar el IVA
   de cada país; Stripe si sí) y crear su *webhook* como función de Supabase. Ese webhook es el que crea los pedidos y
   las licencias al cobrar, con la clave de **servicio** de Supabase. Esa clave y la de la plataforma de pagos
   **nunca** van en la web ni en el repositorio: se guardan como secretos de la función.
7. **Legal antes de cobrar**: alta de la actividad, política de privacidad (RGPD), condiciones de venta y, para los
   kits, marcado CE, RAEE y garantía. Consúltalo con un gestor.

## Notas técnicas

- Código: `web/admin/src/` (React). Reglas de negocio en `logic.ts`, datos de ejemplo en `seed.ts`,
  modo demo en `backend.ts`, conector real en `supabase.ts`. Pruebas: `tests/admin.test.ts`.
- El conector real habla con la API REST de Supabase sin bibliotecas extra. Para modificar usa `PATCH` y solo
  crea con `POST` si no existía (así Soporte puede actualizar envíos sin permiso de crear pedidos).
- Importes en céntimos, fechas en milisegundos, columnas en `snake_case`. Una prueba comprueba que cada columna
  que guarda el panel existe en la migración inicial.
- Los gráficos son SVG propios, de una sola serie y con alternativa en tabla. El color se ha validado sobre el fondo oscuro.

## Eventos en la app (Halloween, Navidad…)

- La app descarga el catálogo publicado (`catalog_collections` y `catalog_items`, sin borradores ni piezas
  inactivas) con las mismas variables `VITE_SUPABASE_URL` / `VITE_SUPABASE_ANON_KEY` y guarda una copia en el
  aparato (`mfv3:catalogo`): sin conexión sigue con lo último descargado.
- Una pieza se enseña si su `archivo` es una URL (o el nombre de una imagen que ya trae la app). Copas, escudos y
  banners salen como una pestaña más del muestrario, con candado hasta conseguirlas.
- Los premios no se guardan aparte: se deducen de los partidos, torneos, logros y días en que se abrió la app
  (como el ELO). Si se borra un partido, el premio que dio desaparece. Los de «entrando en la app» son del aparato;
  el resto, de cada jugador.
- Mientras dura un evento sale una pestaña pequeña arriba en la portada; al tocarla se ve qué se puede
  conseguir y quién lo tiene. El evento es la parte del nombre de la colección antes de « · »
  («Halloween 2026 · Copa» → «Halloween 2026»): así se juntan sus colecciones. En el muestrario la pestaña
  lleva el nombre sin el año («Halloween») y dentro van las piezas de todos los años, la más reciente primero.
- Halloween 2026 viene ya dentro de la app (escudo al entrar, banner jugando y copa ganando un partido el
  31 oct 2026; un juego para ligas, otro para copas y otro para pools), así que funciona aunque el panel no esté conectado.
  La migración `20261012120500_catalogo_halloween_2026.sql` carga esas mismas piezas (mismos ids) en el panel: lo que se
  edite allí (cómo se consigue, fechas, desactivarla) manda sobre lo de la app. Su «archivo» es el nombre de la
  imagen de la app, que el panel sabe enseñar (`urlArchivo` en `web/admin/src/catalogMedia.ts`).
- Código: modelo y reglas en `src/services/events/` (compartido con el panel), pruebas en `tests/events.test.ts`.

## Perfil de jugador por QR

- Cada jugador (no los invitados) tiene un perfil de solo lectura en la web: **`/perfil/#CÓDIGO`**. Se abre leyendo
  su QR, que sale al **dejar pulsado** un jugador en las listas para elegir jugadores (partido o torneo) →
  «Ver QR del perfil», o con el botón «QR del perfil» de su ficha en la app.
- Enseña cabecera (foto, nivel, categoría, ELO), cifras, palmarés, premios de eventos, cómo juega (modalidad, lado,
  franja horaria), los últimos 60 partidos, rivales, compañeros y logros.
- El futbolín calcula esa «foto» del jugador y la sube unos segundos después de cada cambio, solo si ha cambiado
  (`src/app/profileSync.ts`, cálculo en `src/services/profiles/`). En modo prueba no se publica nada.
- Seguridad, sencilla a propósito por ahora: con el código del QR se ve ese perfil y nada más (no se puede listar);
  para cambiarlo hace falta una clave que solo guarda el aparato que lo creó. Tabla `perfiles_jugador` y funciones
  `perfil_jugador`, `publicar_perfil_jugador` y `borrar_perfil_jugador` (migración `20261012120855_perfiles_jugador.sql`).
- Usa las mismas variables `VITE_SUPABASE_URL` / `VITE_SUPABASE_ANON_KEY` que el panel. Sin ellas, el QR sale pero
  avisa de que el perfil no se publica.


## Reglas del juego

- Se guardan en la tabla `game_rules` (una sola fila, `global`, con las reglas en JSON; migración `reglas_juego`). La lee
  cualquiera sin cuenta, como el catálogo; solo un administrador la cambia, y cada publicación queda en Actividad.
- La app las descarga al abrirse con internet y guarda una copia; sin conexión usa la última, y si nunca se ha
  publicado nada, los valores recomendados (`DEFAULT_RULES` en `src/services/progression/gameRules.ts`, el mismo
  fichero que tiene los textos de cada dato).
- Como todo se recalcula desde el historial, cambiar una regla cambia también los partidos ya jugados, y volver a
  publicar los valores de antes lo deja todo como estaba.
- En la app ya no hay nada de esto en Ajustes: la progresión está siempre activa y en Ajustes → Datos solo se ve de cuándo son las reglas en uso.

## Comportamiento de la app

- Se guarda en la misma fila que las reglas del juego (`game_rules`), así que no hace falta otra migración. Cada sección
  publica solo lo suyo: lo de la otra se queda como estaba publicado.
- **Partido suelto y sonido son valores por defecto.** Lo que una mesa cambia (Ajustes → Partido o el panel rápido) queda
  apuntado como suyo y manda sobre el panel; «Volver al del panel» lo devuelve. Lo que la mesa no ha tocado sigue siempre
  lo publicado.
- **Pantalla y placa las fija el panel** en todas las mesas: no aparecen en los ajustes de la mesa.
- Las reglas de los torneos no están aquí: cada torneo lleva las suyas y son iguales en todas las mesas.

## Panel rápido de la mesa

Deslizando el dedo desde el borde de arriba (o tocando la rayita de arriba) baja un panel con volumen y silencio, brillo,
sonido de gol (los de la app y «Propio» si en Ajustes → Sonidos el gol tiene uno de la mesa), efectos, locutor, pantalla completa y un botón a Ajustes. No se abre durante un partido. Todo se guarda en
la mesa. El brillo va a la placa si sabe cambiarlo (orden `BRILLO`, la placa de 7" lo recuerda); si no, la app oscurece la
pantalla con una capa.

## Sonidos propios de la mesa

Ajustes → Sonidos: cada mesa sube sus propios sonidos y elige cuál suena en cada momento del partido (gol, remontada,
victoria, subir de nivel…). Es **solo de la mesa**: el panel de administración no los ve ni los cambia, y «Volver al
del panel» no les afecta. El panel solo sigue decidiendo cuál de los sonidos de gol de la app es «el de siempre».
Los sonidos se guardan en el aparato (IndexedDB, aparte de los demás datos) y van en la copia de seguridad de Datos.
Los que se guardan en la placa de 7" (chip o tarjeta SD) se le pasan por el cable USB; la app guarda siempre una copia
(para escucharlos, volver a pasarlos y la copia de seguridad). Detalles en `HARDWARE.md` §7.2.
