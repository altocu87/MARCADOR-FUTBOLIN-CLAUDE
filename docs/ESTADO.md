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

## Sonidos propios (fase 1 hecha, 16/10/2026)

- **Ajustes → Sonidos**: subir sonidos de la mesa (mp3, wav, ogg, m4a…), escucharlos con ▶ y borrarlos. Al subirlos la
  app los prepara sola: una pista, sin silencios al principio ni al final, volumen igualado, 10 s como mucho y en WAV
  ligero (22 050 Hz, 16 bits, unos 40 KB por segundo), que también podrá tocar la placa.
- **Dónde se guarda** cada sonido (se elige al subirlo): «Este aparato», «Chip de la placa» o «Tarjeta SD». La app lo
  apunta en cada sonido y lo toca desde ahí: del aparato, de la copia ya cargada en memoria (sin esperas al marcar);
  del chip o la SD, pidiéndoselo a la placa. Si la placa no está o no puede, suena el de siempre. Chip y SD salen
  apagados hasta que haya una placa con altavoz (fase 3, capacidades `sonido` y `sd`).
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
- Pendiente: fase 2 (copia opcional en la nube, a decidir) y fase 3 (altavoz en la placa).

## Placa de 7" (Waveshare ESP32-S3-Touch-LCD-7C / 7C-BOX): sonido y tarjeta SD (16/10/2026)

- **Altavoz:** códec ES8389 + I2S con los pines y la secuencia del ejemplo oficial de Waveshare
  (`hardware/esp32s3_pantalla7/s3_audio.h`). Suenan los mismos momentos que en la app (13 de los 16; torneo, nivel y
  reto no existen en la placa), decididos con las mismas reglas que `soundsForCommit()` (`s3_sound.h`). Sin SD, los
  tonos de siempre de la app. Tarea aparte: la pantalla no espera al altavoz. Órdenes nuevas `VOLUMEN` y `SONIDO`;
  la placa anuncia `altavoz,volumen` en el HELLO (todavía no `sonido`/`sd`: la app aún no sabe pasarle archivos).
- **Tarjeta SD:** SDMMC de 1 bit; sonidos propios en `/sonidos/<momento>/*.wav` (varios = uno al azar, `ninguno` =
  silencio). La pantalla de prueba muestra la SD y suena al tocar cada esquina.
- **Capturas sin la placa:** `sh hardware/test/capturas.sh` dibuja las pantallas con la LovyanGFX real en el PC y
  guarda PNG en `docs/capturas-placa/`. Se ve que los números grandes salen pixelados (fuente de mapa de bits
  ampliada) y sin tildes: lo siguiente es usar fuentes suaves con tildes.
- Comprobado en PC: `npm run test:hardware` (nuevas: `sound_test.cpp` y la parte de sonido de `s3_sim_test.cpp`). En
  la placa real: pendiente (aún no la tenemos). Falta la compilación de verdad en GitHub Actions.
- Pendiente: jugadores con foto en la placa, fuentes con tildes, pasar los sonidos de la app a la SD, sincronizar.

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
