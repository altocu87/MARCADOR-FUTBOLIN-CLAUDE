# Prompt maestro: la app en la pantalla de 7" (Waveshare ESP32-S3-Touch-LCD-7C)

Pega todo lo que hay debajo de la línea en una sesión nueva de **Claude Code en tu PC** (Claude Desktop → pestaña
Code, carpeta del proyecto), con la placa conectada por USB-C.

---

## Objetivo

Llevar la app web del Marcador Futbolín (React, carpeta `src/`) a la pantalla Waveshare ESP32-S3-Touch-LCD-7C
(800 × 480, táctil, 32 MB de flash, 16 MB de PSRAM) para que **se vea y se sienta lo más igual posible a la app**:
mismas pantallas, mismas posiciones, colores, tipografías, imágenes, fotos de jugadores y flujo de toques. La placa
debe funcionar sola, sin PC ni móvil. El propietario no es programador: explícale en español, en corto y sin
tecnicismos lo que haces, y enséñale capturas.

## Contexto que ya existe (léelo antes de empezar)

- `CLAUDE.md`: normas de diseño (fotos de jugador siempre, botones grandes, sin scroll salvo listas, todo cabe en
  800 × 480, nada repetido). Se aplican igual en la placa.
- `src/`: la app de referencia. El lienzo es 800 × 480, el mismo tamaño que la pantalla.
- `hardware/esp32s3_pantalla7/`: el programa actual de la placa (LovyanGFX, pantallas dibujadas a mano). **Se
  conserva lo que funciona**: `mfv3_engine.h` (reglas del partido, portadas de la app y probadas en PC),
  `mfv3_sound.h`/`mfv3_audio.h` (altavoces y SD), `mfv3_radio.h` (mando), `mfv3_ota.h`/`s3_net.h` (Wi-Fi y
  actualizaciones), `s3_reloj.h` (reloj y batería), `board_config.h` (pines), `partitions.csv` (2 × 6 MB de programa
  y 19,9 MB FAT para imágenes; los sonidos van en la SD). Lo que se cambia es **la capa de pantallas**.
- `docs/HARDWARE.md` §7 y `docs/ESTADO.md`: estado y protocolo USB con la app.
- `hardware/test/`: pruebas en PC (`sh hardware/test/check_copies.sh`) y capturas sin placa (`capturas.sh`).

## Paso 0: probar la placa real (antes de escribir nada)

1. Instala `arduino-cli`, el núcleo esp32 3.x y LovyanGFX. Compila `hardware/esp32s3_pantalla7` con
   `esp32:esp32:esp32s3:PSRAM=opi,FlashSize=32M,PartitionScheme=custom,CDCOnBoot=cdc` y grábalo por USB
   (borrando la placa la primera vez).
2. Comprueba con el propietario: ¿arranca?, ¿se ve bien?, ¿responde el táctil en las 4 esquinas?, ¿suena?, ¿lee la
   SD?, ¿hora y batería? Lee el puerto serie para ver los mensajes. Si no arranca con 32 MB, prueba `FlashMode=opi`
   (el módulo es un WROOM-2 N32R16V, de flash octal) y anótalo.
3. Arregla lo que falle antes de seguir. Cualquier pin dudoso se comprueba con el esquema oficial
   (github.com/waveshareteam/ESP32-S3-Touch-LCD-7C, carpeta `schematic/`) y con sus ejemplos.

## Paso 1: base para ver y comparar

1. **Captura desde la placa**: añade una orden por USB (`CAPTURA`) que mande el contenido de la pantalla por el
   puerto serie, y un script en el PC que lo guarde como PNG. Así ves exactamente lo que enseña la placa, sin fotos.
2. **Capturas de la app**: arranca la app (`npm run dev`) y saca con Playwright capturas a 800 × 480 de cada
   pantalla, con los datos de demostración.
3. **Comparación lado a lado** (app | placa) de cada pantalla, en un solo PNG, para enseñárselas al propietario.

## Paso 2: motor gráfico

- **LVGL 9** sobre el panel RGB de la placa (búfer de pantalla en PSRAM, doble búfer o bounce buffer para que no
  haya parpadeo ni tirones). Táctil GT911 conectado a LVGL. Objetivo: 30 fps o más en las animaciones.
- **Tema de la app**: copia los colores (variables CSS de `src/`), radios de esquina, sombras y espaciados.
- **Fuentes**: genera con el conversor de LVGL las mismas tipografías de la app, con tildes y ñ, en los tamaños que
  se usan.
- **Imágenes**: un script (`npm run build:placa` o similar) que convierta las imágenes de la app (webp) al formato
  de LVGL o PNG, con el tamaño exacto al que se dibujan, y las meta en la partición FAT (19,9 MB) o en la SD.

## Paso 3: pantallas, por fases (una pantalla cada vez)

Para cada pantalla: léela en `src/ui/screens/` y sus componentes, reprodúcela en la placa con las mismas medidas,
saca la comparación app | placa, enséñasela al propietario y **no pases a la siguiente hasta que la apruebe**.

1. Lo que se usa jugando: Inicio, elegir modo y configuración, cuenta atrás, partido (marcador, reloj, bloqueo,
   pausa, −1, deshacer), descanso, prórroga, penaltis, victoria y reposo (reloj de 7 segmentos con barra de segundos,
   fecha, líder y último partido, igual que `SleepOverlay`).
2. Jugadores: lista y selección con foto (o iniciales), parejas. La app pasa jugadores y fotos a la placa (USB y
   después Wi-Fi). Las fotos se guardan en la FAT.
3. Resumen del partido, ranking, historial, perfiles, torneos y ajustes.
4. Sonidos: los momentos ya funcionan; revisa que suenen igual que en la app.

## Reglas de trabajo

- Rama nueva desde `claude/esp32-s3-capabilities-space-27mt3e`. Commits pequeños y claros, en español.
- Las pruebas en PC (`npm test`, `sh hardware/test/check_copies.sh`) siempre en verde antes de subir. Añade pruebas
  para lo nuevo que no sea dibujo.
- Las reglas del partido salen siempre de `mfv3_engine.h`: no se reescriben en la capa de pantallas.
- Si algo de la app no se puede hacer igual en la placa (un efecto, una animación), dilo, propón la versión más
  parecida y espera el visto bueno.
- Nunca digas que algo funciona en la placa sin haberlo visto en la placa (captura por USB o confirmación del
  propietario).
- Al terminar cada fase, actualiza `docs/ESTADO.md` y `docs/HARDWARE.md`.
