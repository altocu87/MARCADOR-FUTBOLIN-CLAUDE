# Estado de la entrega local

Fecha: 16/10/2026 · Versión 0.3.0 · Motor 1.0.0 (`reglas-partido-2`: por goles sin partes; por tiempo dos partes con prórroga y penaltis)

## Fases del documento maestro

| Fase | Estado |
|---|---|
| 0. Preparación | Hecha: plan por bloques, decisiones en `DECISIONES.md` |
| 1. Núcleo local | Hecha: UI 800×480, modos, configuración, selección, motor, partes, prórroga, penaltis, resumen |
| 2. Gestión local | Hecha: jugadores, preferencias, modo prueba, historial, detalle, almacenamiento local |
| 3. Análisis y progresión | Hecha con **parámetros propuestos**: perfiles, estadísticas, XP/niveles, ELO/ranking, previsión |
| 4. Funciones ampliadas | Hecha con propuestas: 54 logros, títulos, récords, Hall of Fame con podios, temporadas, retos, torneos (liguilla/cuadro), sonido, locutor, efectos, reposo, backup |
| 5. Preparación física | Avanzada: protocolo MFV3, conexión USB/Wi-Fi/Bluetooth en la app, firmware Arduino y ESP32, guía de cableado. Sin prueba en placa real |
| 6. Entrega del código | Este repositorio |
| 7. Base de datos y online | **No iniciada** (por indicación expresa) |

## Comprobado

- `npm test`: 79 pruebas (incluye reglas Caos, avisos, torneos, retos, temporadas, parejas, goleadores, pronósticos y títulos) — antes 55 (motor A01–A13 y extras de reloj/pausa, ELO, XP, niveles, estadísticas,
  previsión, repositorio local, almacenamiento lleno, modo prueba, backup).
- `npm run build`: TypeScript estricto sin errores y compilación correcta.
- Recorrido en Chromium a 800×480 (Playwright): menú → modalidad → configuración → jugadores →
  previsión → cuenta atrás → partido → bloqueo → pausa → descanso → 2ª parte → −1 → empate →
  prórroga → penaltis → resumen → ranking/historial/Hall of Fame/récords → perfil → torneo →
  ajustes → reposo. Sin errores de consola y sin scroll general.
- 2v2 en modo prueba con teclado y sensor simulado (bloqueo respetado), abandono de partido,
  recuperación tras recarga (reanuda en pausa), escalado a 400×300 y 1280×900.
- Versión 0.2: recorrido con datos de demostración (45 partidos) por Caos 2v2 con equilibrado, comodín, gol
  doble, racha, bola de partido, victoria, celebración, goleadores, ranking (temporada/histórico, Hall of Fame,
  parejas, resumen, cara a cara), perfil (cromo, ELO, hábitos, logros, estilo), retos, creación y juego de un
  torneo en cuadro, ajustes de audio y penaltis. Sin errores de consola.

- Versión 0.3: `npm test` 88 pruebas (protocolo, lienzo adaptable). `npm run test:hardware`: núcleo del firmware,
  simulación del sketch Arduino (antirrebote, órdenes, LED) y comprobación del sketch ESP32 con bibliotecas simuladas.
  Placa ESP32 simulada por WebSocket: salto de cuenta atrás, goles, bloqueo de 3 s y respuestas de la app correctos.
  Sin barras de desplazamiento en 800×480, 1280×720, 1024×768, 2560×1080, 844×390 y 390×844 (girado y sin girar),
  app servida comprimida como en el ESP32, y arranque sin conexión con el servidor apagado.

- Modo prueba (Ajustes → General, interruptor único): carga 12 jugadores, 137 partidos jugados con el motor real
  (1v1/2v2, Rápido, Caos y Clasificatorio, prórrogas, penaltis, goleadores, pronósticos) y 4 torneos (3 terminados y
  1 en juego). Lo que se juegue o añada en modo prueba también se guarda ahí (`mfv3:v1:demo:`). Al desactivarlo se
  borra todo lo de prueba y vuelven los datos reales intactos. Las preferencias son comunes a ambos modos.

## Sonidos propios (fases 1 y 3 hechas, 16/10/2026; la 2, en la nube, no se hace por ahora)

- **Ajustes → Sonidos**: subir sonidos de la mesa (mp3, wav, ogg, m4a…), escucharlos con ▶ y borrarlos. Al subirlos la
  app los prepara sola: una pista, sin silencios al principio ni al final, volumen igualado, 10 s como mucho y en WAV
  ligero (16 000 Hz, 16 bits, 32 KB por segundo), la misma frecuencia a la que suena la placa.
- **Dónde se guarda** cada sonido (se elige al subirlo): «Este aparato», «Chip de la placa» o «Tarjeta SD». La app lo
  apunta en cada sonido y lo toca desde ahí: del aparato, de la copia ya cargada en memoria (sin esperas al marcar);
  del chip o la SD, pidiéndoselo a la placa. Si la placa no está o no puede, suena el de siempre. Chip y SD solo se
  pueden elegir con una placa con altavoz conectada (capacidades `sonido` y `sd`).
- **16 momentos** con su sonido: «El de siempre», «Ninguno», uno propio o varios (uno al azar cada vez). Detección
  nueva de 3 seguidos, remontada, empate, goleada, gol de oro, ganar un torneo y reto conseguido
  (`src/services/sound/soundEvents.ts`). Un sonido propio nuevo corta al anterior; la bola de partido y el himno del
  ganador esperan a que acabe.
- Panel rápido: botón «Propio» en «Sonido de gol» cuando el gol tiene uno de la mesa.
- Copia de seguridad de Datos: incluye los sonidos (en base64). Las copias antiguas se siguen pudiendo cargar y, si no
  traen sonidos, no borran los de la mesa. «Borrar todos los datos» también los borra. Son comunes al modo prueba.
- Comprobado: `npx vitest run` 272 pruebas (19 nuevas de sonidos: momentos del partido, preparación del audio, de
  dónde sale cada sonido y copia de seguridad), `tsc` sin errores, `npm run build:web` correcto y recorrido en
  Chromium a 800×480: subir dos sonidos, elegirlos, recargar (se conservan) y partido rápido donde el gol suena con el
  propio y el empate con el de siempre. Sin errores de consola.
- **Fase 3, placa de 7" (Waveshare 7C) con dos altavoces de 8 Ω y 2 W** a sus conectores (chip de sonido ES8389 y
  dos amplificadores NS4150B de la propia placa; esquema en `HARDWARE.md` §7.2):
  - Los sonidos se guardan en la tarjeta micro SD, como `/sonidos/<código>.wav`. Memoria de 32 MB repartida con
    `hardware/esp32s3_pantalla7/partitions.csv`: 2 × 6 MB de programa (actualización por Wi-Fi) y 19,9 MB FAT
    reservados para imágenes (app y fotos de jugadores).
  - La app se los pasa por el cable USB en trozos con CRC32 y barra de progreso; al conectar le pregunta cuáles tiene
    («Pasar a la placa» si falta alguno) y le manda qué suena en cada momento y el volumen. Borrar en la app los borra
    también de la placa. Órdenes nuevas del protocolo `SONIDO_*`, `MOMENTO`, `VOLUMEN` (tabla en `HARDWARE.md` §5).
  - La placa detecta ella sola 13 momentos en sus partidos (los mismos que la app salvo torneo, nivel y reto) y toca el
    sonido elegido; si no lo tiene, unas notas «de siempre». También cuando juega sin la app.
  - Comprobado: `npm run test:hardware` (pruebas nuevas `sound_test.cpp` y la placa simulada con altavoces, memoria y
    SD en `s3_sim_test.cpp`: subir por USB, listar, tocar en el gol, «ninguno», volumen, borrar, recordar tras
    reiniciar) y `npx vitest run` 278 pruebas (6 nuevas del protocolo con una placa simulada). Compilado en GitHub.
    **Sin probar en la placa real.**
- Fase 2 (copia en la nube): no se hace por ahora.

## Capturas de la pantalla de 7" sin la placa (16/10/2026)

- `sh hardware/test/capturas.sh` dibuja las pantallas con la LovyanGFX real en el PC y guarda PNG en
  `docs/capturas-placa/` (en el PC no hay chip de sonido: la prueba de la placa sale con «Altavoz ERROR»).
- Se ve que los números grandes salen pixelados (fuente de mapa de bits ampliada) y sin tildes: lo siguiente es
  usar fuentes suaves con tildes. Pendiente también: jugadores con foto en la placa.

## Actualización por Wi-Fi de la pantalla de 7" y del mando (16/10/2026)

- **AJUSTES** en la pantalla (sustituye a PRUEBA en Inicio): Wi-Fi con lista de redes y teclado táctil, ACTUALIZAR,
  volumen y prueba de la placa (`s3_ajustes.h`, `s3_net.h`).
- Al encender mira si hay versión nueva (pantalla y mando) y lo avisa con un botón dorado en Inicio; se instala
  cuando se pulsa, nunca en partido. El mando se actualiza por radio desde la pantalla (`mfv3_ota.h`), con los dos
  botones mantenidos 3 s como permiso. Todo comprobado por MD5; si falla, cada placa sigue con lo que tenía.
- GitHub: número de versión en cada compilación y `ota.json` + `<placa>-ota.bin` en la web. Particiones con dos
  mitades (la primera vez, instalación por USB borrando la placa).
- Comprobado en PC: `ota_test.cpp` (radio con pérdidas), simulación del mando y de la pantalla (teclado, conexión,
  versiones, instalación completa mando + pantalla, comprobación al encender, sin internet). Capturas 09–14.
  Pendiente: probarlo en las placas reales.

## Reloj y batería en la pantalla de 7" (17/10/2026)

- `s3_reloj.h`: reloj PCF85063A (0x51) y medidor BQ27220 (0x55), en el bus del táctil. Hora en Inicio y reloj grande
  con fecha en reposo; fecha y hora en cada resultado (NVS `hist2`). Batería con su carga, «CARGANDO» y
  «BATERIA BAJA» (≤ 15 % sin cargar). Puesta en hora: orden `HORA` por USB (la app la manda a las placas con
  `reloj`) y hora de Internet al conectarse al Wi-Fi.
- Esquema oficial de la 7C: la pata EXIO6 es `LCD_VDD_EN` (alimentación del panel); ahora se deja alta, como en los
  ejemplos del fabricante (antes quedaba a 0). El módulo es un ESP32-S3-WROOM-2-N32R16V.
- El LED «STA» no se usa: no aparece en el esquema de la 7C.
- Comprobado en PC: simulación de la pantalla (puesta en hora, fechas imposibles, batería baja/cargando/quitada,
  reposo, hora de Internet) y capturas 15–17. Pendiente: probarlo en la placa real.

## Pantalla de 7" igual que la app: LVGL 9 en la Waveshare 7C (17/10/2026, rama `pantalla-lvgl`)

- **Motor gráfico nuevo** (`hardware/esp32s3_pantalla7/ui_port.h`): LVGL 9.5 sobre el panel RGB con dos imágenes de
  pantalla en la PSRAM que se cambian en el retorno vertical (sin parpadeo ni cortes). **Comprobado en la placa real:
  41 imágenes por segundo** (el máximo del panel a 16 MHz), imagen correcta y táctil GT911 (lo confirmó Alex).
- **Igual que la app**: lo que no cambia de cada pantalla (fondos, marcos, brillos, títulos, botones pulsados) se
  fotografía de la propia app a 800 × 480 (`scripts/placa-capas.mjs`) y va a la memoria FAT de la placa (19,9 MB;
  `scripts/placa_recursos.py`). Lo que cambia (números, reloj, textos, cifras) lo dibuja LVGL con la misma letra
  **Inter** de la app (ahora la app también la lleva dentro, para verse igual en cualquier aparato), colocada en la
  misma línea base que el navegador con las medidas de `docs/capturas-app/*.json`.
- **Ver la placa sin tocarla**: órdenes USB `CAPTURA` (la imagen que se ve, en 3 s) y `TOQUE x y`;
  `scripts/placa.py` compila, graba, saca capturas y manda órdenes; `scripts/comparar.py` junta app | placa.
  `scripts/capturas_placa_pc.py` saca las mismas capturas con la placa simulada en el PC (mismo código y LVGL).
- **Pruebas en PC**: el sketch se compila con LVGL de verdad (`hardware/test/lvgl_lib.sh`, `s3_build.sh`) para las
  tres placas; la prueba de partido completo lee los textos de los objetos LVGL.

## Web y compilación

- Web pública (`web/`) con asistente «Monta tu marcador» e instalador desde el navegador (ESP Web Tools). Los
  binarios los genera GitHub Actions (`Firmware`, release `firmware-latest` al subir a `main`).
- Compilación real en GitHub: Arduino Uno/Nano/Mega/Leonardo, Raspberry Pi Pico, ESP32, C3, S3, S2 y mando
  compilan, y las 3 pantallas de 7" también tras corregir las declaraciones automáticas del IDE (`s3_types.h`;
  las pruebas del PC ahora simulan ese paso). Los binarios se recortan (sin relleno 0xFF) para instalar en segundos.

## Limitaciones conocidas

- El firmware compila en GitHub Actions para las 13 placas, pero no se ha probado ninguna placa física:
  primera prueba recomendada por USB con el monitor serie (y la pantalla de prueba en la de 7").
- En http://192.168.4.1 (app servida por el ESP32) el navegador no permite instalarla ni USB/Bluetooth web
  (exigen https); la conexión Wi-Fi con la placa sí funciona.
- ESP32-S3 7": versión nativa activa en `hardware/esp32s3_pantalla7/` (motor C++ + LovyanGFX). Probada en PC con simulación y compilada en GitHub; sin probar en la placa real. Configurada para la Waveshare ESP32-S3-Touch-LCD-7C (sensores en DI0/DI1) y mando inalámbrico de 2 pulsadores (ESP-NOW: toque corto = gol, largo = anular).
- «Hazlo tú mismo»: catálogo único de 13 placas (`src/inputs/hardware/diy-catalog.json`) para la guía de la app y `docs/MANUAL_DIY.md`; pines automáticos por modelo (`mfv3_boards.h`) y saludo con capacidades. Todas las placas figuran como «probada en simulador» hasta probarlas en real.

- Los iconos con emoji (🏆, 🎯, 🃏…) dependen de la fuente de emoji del sistema; las imágenes opcionales los sustituyen.
- La voz del locutor depende de que el navegador tenga una voz en español instalada.
- Los datos viven en el `localStorage` de este navegador: borrar los datos del sitio los elimina.
  Exporta copias desde Ajustes → Sistema.
- El arranque totalmente sin red tras un reinicio (empaquetado offline/PWA) no está garantizado en
  una web alojada sin caché; se debe validar en el dispositivo final.
- ESP32-S3 7": versión nativa activa en `hardware/esp32s3_pantalla7/` (motor C++ + LovyanGFX), probada en PC
  con simulación y compilada en GitHub; sin probar en la placa real. Configurada para la Waveshare ESP32-S3-Touch-LCD-7C (sensores en DI0/DI1) y mando inalámbrico de 2 pulsadores (ESP-NOW: toque corto = gol, largo = anular).
- Fotos de jugador guardadas como imagen reducida (160×160) dentro del almacenamiento local.
