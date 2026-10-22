# Ajustes en la pantalla y actualización del mando por modelo · 0.8 · 22/10/2026

Equipo A (Cloud), por encargo del dueño: llevar a la placa la lógica de Ajustes de la app web, con un estilo austero (botones y letras grandes, sin imágenes nuevas) y resolver la actualización del mando C3-Zero. Rama `claude/ajustes-placa`. **Sin fusionar en main, sin publicar y sin grabar en la placa**: falta la autorización del dueño.

## Qué cambia para quien juega

AJUSTES (engranaje de Inicio) es ahora un menú de 4 filas de botones grandes, cada uno con su valor debajo. Lo que pide atención sale en dorado (sin Wi-Fi, sin cuenta, versión nueva, mando sin modelo).

| Botón | Qué tiene | Origen en la app |
|---|---|---|
| PARTIDO | Cómo se gana (GOLES/TIEMPO/AMBAS, con «?»), goles para ganar, minutos por parte y **primero en los penaltis** (BLANCO/AZUL) | Ajustes → Partido |
| SONIDO | Volumen y **qué suena en cada momento**: 14 momentos, cada uno con OÍR, EL DE SIEMPRE, NINGUNO y PROPIOS (solo si la app le puso sonidos propios). «?» en los que no se entienden solos | Ajustes → Sonidos |
| PANTALLA | Brillo, **reloj de reposo** (1, 2, 5, 10 min o NUNCA, con «?») y la hora con **PONER EN HORA** por Wi-Fi | Nuevo: antes solo desde la app (brillo) o fijo (reposo de 2 min) |
| MANDO | Si se oye y su programa, **modelo de la mini** (SUPER MINI / C3-ZERO, con «?») y el estado de su actualización | Nuevo |
| EQUIPOS | **Equipos fijos**: lista, nuevo (nombre con teclado → elegir los dos jugadores), cambiar nombre o jugadores y borrar | Ajustes → Jugadores → Equipos |
| DATOS | **Modo prueba** (activar / quitar) y **copias de seguridad** en la SD (hacer, restaurar, borrar) | Ajustes → Datos |
| JUGADORES → un jugador | Botón nuevo **QR DEL PERFIL** | Menú del jugador → Ver QR |
| JUGADORES, CUENTA, WI-FI, ACTUALIZACIONES, PRUEBA DE LA PLACA | Las pantallas que ya había | — |

- OÍR en «Gol» con EL DE SIEMPRE reproduce una voz de comentario de la SD: sirve para comprobar que los comentarios suenan sin jugar un partido.
- Todo se guarda al tocarlo, sin botón de guardar, y se conserva al apagar. El primer lanzador de penaltis se aplica a los partidos siguientes (`cfg.penaltyFirst`, que el motor ya usaba).
- La pantalla antigua de Ajustes (lista dibujada a mano) desaparece. Wi-Fi, Actualizaciones e Instalación siguen con el adaptador antiguo, pero con letras que sí existen en la placa: se acaban los avisos de «Falta la letra».

## Equipos fijos (`s3_equipos.h`)

- Una pareja con nombre. Si esos dos juegan juntos en el mismo lado, el **marcador y la victoria ponen el nombre del equipo** en vez de BLANCO o AZUL, como `TeamName` de la app.
- Se guardan en `/jugadores/equipos.txt` (FFat), así que van con el modo prueba y con las copias. En la PSRAM, hasta 32.
- **Nube:** en la cuenta, los equipos van en la fila `prefs`/`mesa`, campo `clubs`, como en la app. La placa baja ese campo. Si tiene cambios propios (`equipos.pend`), lee la fila, cambia **solo** `clubs` y la guarda: el resto de preferencias de la app no se toca.
- Al subir se **unen** las dos listas: los de la placa más los de la cuenta que la placa no tiene. No vuelven los que se borraron en la placa (`equipos.del`) ni los de una pareja que la placa ya tiene. Así un equipo creado en la app mientras tanto no se pierde. Sin cambios en la placa, lo de la cuenta sustituye a lo de la placa.
- La placa no dibuja los logos de la app: enseña una insignia con las iniciales en el color que sale de su logo. Los equipos creados en la placa llevan un escudo genérico de la app.

## QR del perfil (`s3_perfil.h`)

- Cada jugador tiene un código (22 letras, el que va en el QR) y una clave (32) que solo conoce la placa, en `/jugadores/perfiles.txt`, como `credentialsFor` de la app.
- Al abrir el QR, la placa calcula el perfil con **las mismas cuentas que `buildProfileSnapshot`** y lo publica con la RPC `publicar_perfil_jugador`. Usa la clave pública, así que no hace falta cuenta. Lo calculado incluye:
  - general, clasificatorio y por modo;
  - racha actual y mejor racha;
  - últimos 5 resultados, tiempo jugado y goles propios;
  - blanco/azul y hábitos por día y por franja;
  - 12 rivales y 12 compañeros, el rival favorito y la bestia negra;
  - últimos 60 partidos;
  - nivel, XP, ELO, categoría y logros con su fecha.

  Lo envía la tarea de la nube (trabajo `PROFILE`), sin bloquear la pantalla.
- **Comprobado contra la app:** `tests/perfil-placa.test.ts` calcula el perfil con el código de la app sobre los 40 partidos de prueba que juega la placa. Coincide **exactamente** con el de la placa (estadísticas, lados, hábitos, rivales, compañeros, favorito, bestia negra, partidos y gente).
- Diferencias con la app:
  - Sin torneos en la pantalla, el palmarés va vacío.
  - «Jugador desde» es la fecha de su primer partido en la placa.
  - De los demás jugadores no se envía la foto; del propio jugador, sí (en BMP, como al subirlo a la cuenta).
  - En modo prueba no se publica.

## Modo prueba y copias (`s3_datos.h`)

- **Modo prueba:**
  - Las carpetas reales `/jugadores` (FFat) y la de partidos (SD) se renombran a `….real`. Se crean 12 jugadores (los nombres de la app) y 40 partidos jugados con el motor de la placa durante los últimos 40 días.
  - Lo que se juegue va a la prueba. La nube queda en pausa (`nube::paused`): no sube ni baja nada.
  - Inicio avisa con «MODO PRUEBA».
  - Al quitarlo se borran las carpetas de prueba y vuelven las reales, con el último resultado y el contador de partidos de Inicio.
- **Copia de seguridad:** se guarda en `/sdcard/copias/AAAAMMDD-HHMMSS/` e incluye:
  - jugadores (con fotos, equipos y códigos de QR);
  - partidos;
  - `ajustes.txt` (partido, volumen, brillo, reposo, mando y sonidos; **no** el Wi-Fi ni la cuenta);
  - `info.txt`, que se escribe al final: una copia sin él está incompleta y no se restaura.
- **Restaurar:**
  1. Se copia a `….new`.
  2. Solo al final se cambia por lo actual (`….old`) y se borra lo apartado.
  3. Con cuenta, lo restaurado se marca para subir.
- **Cortes de luz:** al encender, `datos::recover()` deja todo coherente:
  - un restaurar a medias se descarta o se completa;
  - un modo prueba a medio poner se deshace.

  La prueba lo simula con las carpetas a medio mover.
- Son tareas de unos segundos. Se hacen seguidas, con barra de progreso y sin aceptar toques, como al actualizar. No durante un partido ni con la nube trabajando.
- Las copias son para la placa: la app no las importa (su copia es un archivo JSON propio).

## Actualización del mando por modelo

**Problema:** GitHub solo publicaba el programa de la Super Mini (`mando-c3`). La pantalla lo habría ofrecido al C3-Zero del dueño: los botones habrían seguido funcionando, pero su luz no.

**Solución:**
1. **Catálogo:** nueva placa `mando-c3zero` con `-DMFV3_MANDO_RGB_PIN=10`. GitHub compila y publica los dos programas del mando.
2. **El mando dice su modelo:** el saludo de radio (INFO) lleva un byte más al final: 1 = Super Mini, 2 = C3-Zero. Las pantallas antiguas lo ignoran.
3. **La pantalla elige:** consulta lo publicado de los dos modelos y usa el que dice el mando. Si el mando tiene un programa anterior y no dice su modelo, como el del dueño (build 54), **no se le ofrece ninguna actualización** hasta elegirlo una vez en AJUSTES → MANDO. Se recuerda en la NVS (`mmod`). Si el mando sí lo dice, no se puede elegir otro.
4. Instala desde `firmware/mando-c3zero/` o `firmware/mando-c3/` según ese modelo, con las mismas comprobaciones de siempre: MD5, armado con los dos botones y confirmación del mando.

**Para el dueño:** después de instalar 0.8, entrar en AJUSTES → MANDO y tocar **C3-ZERO**. A partir de ahí la actualización del mando es segura. Tras la primera actualización, el mando lo dirá él solo.

## Rendimiento

- Cada apartado es una pantalla LVGL que se crea una vez al entrar. Al tocar una opción solo cambian el color de sus botones y el texto de su valor. La prueba comprueba que la pantalla sigue siendo la misma después de tocar.
- Los valores que cambian solos (mando, versiones, hora) se repasan cada 500 ms, y solo se redibuja lo que ha cambiado.
- No usa imágenes ni letras nuevas: solo tamaños que ya están en la memoria FAT de la placa (`fuentes.txt`). Por eso basta con actualizar por Wi-Fi, sin volver a grabar los recursos.
- La lista de sonidos usa scroll, permitido en Ajustes. Lo demás cabe en 800 × 480 sin scroll.

## Archivos

- `s3_opciones.h` (nuevo): menú y apartados PARTIDO, SONIDO, PANTALLA y MANDO.
- `s3_opciones_datos.h` (nuevo): pantallas de EQUIPOS, DATOS y QR (dentro de `opciones`).
- `s3_equipos.h`, `s3_perfil.h` y `s3_datos.h` (nuevos): equipos fijos, perfil público y modo prueba / copias.
- `s3_nube.h`:
  - Baja también `prefs` (solo usa `clubs`) y sube los equipos uniendo las dos listas.
  - Trabajo `PROFILE`.
  - `paused` para el modo prueba.
- `s3_progresion.h`: fecha de cada logro (`achAt`).
- `ui_partido.h` y `ui_victoria.h`: nombre del equipo fijo.
- `ui_fichas.h`:
  - Botón QR DEL PERFIL.
  - Iniciales con letras que existen: corrige que en JUGADORES salían diminutas.
- `s3_ajustes.h`:
  - Fuera el menú antiguo.
  - `mandoId(modelo)`, `pubMandoBy[]`, `MandoUpd::NeedsModel` y `syncMandoModel()`.
  - La instalación descarga el programa del modelo.
- `ui_layout.h`: nuevos sitios de `SETTINGS`, en el mismo orden de índices.
- `esp32s3_pantalla7.ino`:
  - Reposo configurable (`slp`, 0 = nunca), `pfirst` y `mmod` en la NVS.
  - Modelo recibido por radio.
  - Rama de dibujo del menú nuevo.
- `mfv3_ota.h` (común y copias): `MODEL_*`, y `encodeInfo`/`decodeInfo` con el modelo, compatible con los dos formatos.
- `mando_pulsadores.ino`: envía `MFV3_MANDO_MODEL`.
- `ui_legacy.h`: letras existentes.
- `diy-catalog.json`, `docs/MANUAL_DIY.md`: placa `mando-c3zero`.
- `releases.json` / `s3_release.h`: **0.8**.

## Pruebas

- Nueva `hardware/test/s3_opciones_test.cpp`, con LVGL real y toques:
  - Menú y valores.
  - Partido: condición, goles, minutos y penaltis, guardados y recuperados al encender.
  - Sonido: volumen, NINGUNO/EL DE SIEMPRE, la ayuda «?» y PROPIOS.
  - Pantalla: brillo, NUNCA (12 minutos sin reposo), 1 MIN (reposo a los 61 s) y despertar en el mismo apartado.
  - Mando: un mando antiguo sin modelo no recibe nada; al elegir C3-ZERO se ofrece su programa y la instalación descarga el binario de `mando-c3zero`; un mando que dice «Super Mini» manda sobre lo elegido.
- La misma prueba, además:
  - **Equipos:** crear con el teclado y elegir la pareja; el nombre en el marcador; con la nube simulada, unir con un equipo de la app conservando las demás preferencias, no recuperar uno borrado aquí y recibir uno nuevo de la cuenta.
  - **QR:** código estable, perfil publicado y «Listo».
  - **Modo prueba:** activar (12 jugadores y 40 partidos, nada sube a la cuenta y no se pueden hacer copias) y quitar (vuelven los datos exactos).
  - **Copias:** hacer, cambiar cosas, restaurar (vuelven jugadores, equipos y ajustes) y borrar.
  - **Cortes:** modo prueba a medio poner y restaurar a medio cambiar.
- `tests/perfil-placa.test.ts`: el perfil de la placa es igual que el de la app.
- `ota_test`: modelo en INFO, formato antiguo y modelo desconocido.
- `mando_sim_test`: los dos perfiles anuncian su modelo.
- `catalog.test.ts`: los dos programas del mando se compilan con el perfil de su mini.
- `s3_sim_test` y `s3_updates_test` adaptados al menú nuevo (sitios de los botones, botones que actúan al levantar el dedo y modelo del mando).
- Resultados:
  - `bash hardware/test/check_copies.sh`: todo correcto, en las tres placas (7C, Elecrow y Waveshare 7).
  - `vitest`: 315/315.
  - `npm run build` correcto.
  - `native-release.mjs check`: «0.8 correcto».

## Pendiente

- La compilación real con Arduino-ESP32 la hace GitHub al subir la rama.
- Probar en la placa y con el dedo. Es la parte que falta.
- Primera actualización real del mando C3-Zero por radio.
- Primera publicación real de un perfil desde la placa: la RPC es la misma que usa la app, pero aquí solo se ha probado con la nube simulada.
- No se ha llevado «borrar todos los datos»: con cuenta, la nube los volvería a bajar. Para empezar de cero, mejor el modo prueba o restaurar una copia.
- La subida de sonidos y la creación o recuperación de la cuenta se quedan en la app.
