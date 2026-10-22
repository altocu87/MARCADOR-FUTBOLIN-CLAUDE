# Ajustes en la pantalla y actualización del mando por modelo · 0.8 · 22/10/2026

Equipo A (Cloud), por encargo del dueño: llevar a la placa la lógica de Ajustes de la app web, con un estilo austero (botones y letras grandes, sin imágenes nuevas) y resolver la actualización del mando C3-Zero. Rama `claude/ajustes-placa`. **Sin fusionar en main, sin publicar y sin grabar en la placa**: falta la autorización del dueño.

## Qué cambia para quien juega

AJUSTES (engranaje de Inicio) es ahora un menú de 3 × 3 botones grandes, cada uno con su valor debajo. Lo que pide atención sale en dorado (sin Wi-Fi, sin cuenta, versión nueva, mando sin modelo).

| Botón | Qué tiene | Origen en la app |
|---|---|---|
| PARTIDO | Cómo se gana (GOLES/TIEMPO/AMBAS, con «?»), goles para ganar, minutos por parte y **primero en los penaltis** (BLANCO/AZUL) | Ajustes → Partido |
| SONIDO | Volumen y **qué suena en cada momento**: 14 momentos, cada uno con OÍR, EL DE SIEMPRE, NINGUNO y PROPIOS (solo si la app le puso sonidos propios). «?» en los que no se entienden solos | Ajustes → Sonidos |
| PANTALLA | Brillo, **reloj de reposo** (1, 2, 5, 10 min o NUNCA, con «?») y la hora con **PONER EN HORA** por Wi-Fi | Nuevo: antes solo desde la app (brillo) o fijo (reposo de 2 min) |
| MANDO | Si se oye y su programa, **modelo de la mini** (SUPER MINI / C3-ZERO, con «?») y el estado de su actualización | Nuevo |
| JUGADORES, CUENTA, WI-FI, ACTUALIZACIONES, PRUEBA DE LA PLACA | Las pantallas que ya había | — |

- OÍR en «Gol» con EL DE SIEMPRE reproduce una voz de comentario de la SD: sirve para comprobar que los comentarios suenan sin jugar un partido.
- Todo se guarda al tocarlo, sin botón de guardar, y se conserva al apagar. El primer lanzador de penaltis se aplica a los partidos siguientes (`cfg.penaltyFirst`, que el motor ya usaba).
- La pantalla antigua de Ajustes (lista dibujada a mano) desaparece. Wi-Fi, Actualizaciones e Instalación siguen con el adaptador antiguo, pero con letras que sí existen en la placa: se acaban los avisos de «Falta la letra».

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
- No se han llevado a la placa los equipos fijos, el QR del jugador, el modo prueba, la copia de seguridad ni el borrado de datos. Se explica en la conversación del 22/10: poco uso en la mesa o riesgo con la sincronización de la nube.
- La subida de sonidos y la creación o recuperación de la cuenta se quedan en la app.
