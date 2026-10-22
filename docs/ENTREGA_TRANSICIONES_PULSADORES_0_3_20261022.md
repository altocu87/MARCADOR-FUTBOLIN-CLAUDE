# Pantalla 0.3 · continuar las partes desde el mando · equipo B

## Petición y comportamiento

El dueño pide que cualquiera de los dos pulsadores arcade permita continuar al terminar la primera parte, al terminar la segunda empatada y al acabar la prórroga sin gol. Autoriza programarlo e instalarlo para una prueba real con partes de un minuto. Esta entrega permanece en la rama del equipo B: no implica fusionar ni publicar en main.

Antes, las órdenes de gol de la radio llegaban a `inputGoal` y `Engine::goal` rechazaba la fase `PeriodEnd`. Solo el botón táctil llamaba a `Engine::next`.

Ahora, `inputButtonGoal` en `hardware/esp32s3_pantalla7/esp32s3_pantalla7.ino` dirige las órdenes manuales de gol (mando ESP-NOW y protocolo USB/UART) a `Engine::next` cuando corresponde. Se actualiza el reloj antes de decidir para cubrir una pulsación justo cuando vence el tiempo. La función retorna tras continuar: esa misma pulsación no suma gol ni cuenta como el primer lanzamiento de penalti. La deduplicación de radio existente descarta las tres copias y sus repeticiones tardías.

Los sensores automáticos y los marcadores táctiles siguen entrando por `inputGoal`; un balón no confirma una pantalla de final de parte. El botón táctil CONTINUAR se conserva. Una pulsación larga conserva su función de anulación y no continúa. Un final con ganador sigue siendo final, sin revancha automática. No cambia la app web, el motor común, los pines ni el programa de la mini.

Las pantallas de final de parte explican que se puede pulsar cualquiera de los botones. Las notas usan el espacio de texto existente. No se añaden buffers, tareas, imágenes ni asignaciones dinámicas para procesar la pulsación.

## Versión y pruebas

- Versión nativa 0.3, registro `releases.json` con fecha 22/10/2026, conservando 0.2 y 0.1. No se autoriza ni se utiliza 1.x. `s3_release.h` regenerado.
- Adaptación local sobre build base 54; no confundirla con una entrega oficial publicada. Las versiones publicadas en la nube permanecen sin cambiar.
- Nueva prueba `hardware/test/s3_button_transitions_test.cpp`, incluida en `check_copies.sh`: ambos colores en las tres transiciones, repeticiones de radio, ausencia de gol/penalti involuntario, separación de sensores/anulación, borde del tiempo, continuidad táctil y final con ganador.
- Pasa `hardware/test/check_copies.sh` completo, incluidas simulaciones de las pantallas, radio, actualización, memoria/sesiones, saturación USB y declaraciones generadas como las del IDE de Arduino. Permanecen advertencias anteriores de estrechamiento de enteros y valores de enum en rutas de dibujo antiguas; no se han corregido fuera de alcance.
- Pasa la prueba del registro de versiones: 6/6 mediante Vitest. El primer intento se lanzó con el ejecutor de pruebas de Node por error y se descartó; no era un fallo del código de versiones.
- Comprobada la selección de arranque actual antes de instalar: app0 0x10000, 6 MB, secuencia OTA 3 y estado válido. Se guardaron localmente los primeros 64 KB (incluyen configuración privada; no subirlos). La instalación debe escribir únicamente la aplicación, conservando app1, NVS, FFat y SD.

## Prueba física prevista

Tras confirmar por USB que arranca 0.3, el dueño inicia un partido rápido con partes de un minuto, sin marcar goles para mantener el empate. Al final de la primera parte pulsa corto el blanco; al final de la segunda, el azul; al final de la prórroga de 60 s, el blanco. Se cotejan las órdenes de la mini con los estados `period_end`, `countdown`, `playing` y `penalties` de la pantalla. La interfaz debe mostrar la parte nueva y las notas; al entrar en penaltis no debe aparecer ningún lanzamiento realizado por esa pulsación de continuación.

La prueba real está pendiente hasta añadir su resultado. No dar por probado en placa lo que solo se ha simulado. El dueño autoriza este partido de prueba; podría afectar al historial si se termina, aunque el abandono desde pausa descarta el partido según el código.

## Contexto confirmado del mando y colores del montaje

Mini real: Waveshare ESP32-C3-Zero, programa build base 54 con WS2812 GPIO10, COM7. Pantalla: Waveshare 7C, COM6. Antes de esta mejora, una prueba real de un gol y una anulación por equipo pasó: 0–0 → 1–0 → 0–0 → 0–1 → 0–0. En otra tanda, la mini detectó 3 cortas y 3 largas por equipo. No se reprodujo el fallo intermitente inicial; el bloqueo global de 3 s explica pulsaciones rápidas para goles, no una anulación fallida.

Datos comunicados por el dueño, sin asumir convenciones de color:

| Pulsador | Cable | Función | Cable de la mini |
|---|---|---|---|
| Blanco | rojo | LED + | blanco, 5V |
| Blanco | gris | LED − | verde, GND |
| Blanco | verde | COM | verde, GND |
| Blanco | blanco | contacto NO/NC inicialmente sin confirmar | naranja, GPIO3, solo si NO |
| Azul | verde | LED + | blanco, 5V |
| Azul | negro | LED − | verde, GND |
| Azul | rojo | COM | verde, GND |
| Azul | blanco | NO | negro, GPIO4 |

Los colores de los dos pulsadores se confundieron al principio de la conversación; la tabla refleja la corrección posterior expresa del dueño. El negro de la mini es GPIO4, no negativo. La regleta de unión no se ha identificado por foto: un cable por entrada salvo especificación expresa del modelo. No afirmar haber medido el consumo de las luces.

## Pendientes para equipo A / Cloud

Revisar y aceptar esta entrega. Completar la selección/persistencia del perfil C3-Zero antes de publicar soporte oficial de su actualización: la pantalla sigue descargando el perfil genérico `mando-c3` para Super Mini. Conservar los límites y las incidencias USB ya documentadas. La captura de pantalla no forma parte del tratamiento de las pulsaciones y puede perturbar una medición: no confundir problemas del capturador con fallos de los botones.

## Instalación física de 0.3 confirmada

Se instaló en COM6 únicamente `esp32s3_pantalla7.ino.bin` a 0x10000, mediante el cargador ROM sin stub. Escritura de 1.868.080 bytes en 109,5 s, seguida de **Hash of data verified** y reinicio. SHA256 `c9882963acbcb84a18d1b8983f67cf1b4e194843570270e810652347c8b87e2c`; MD5 `d49f73cecc5c7758ae631b05e32eb2c0`. Fuente exacta: `a336c92e37893de4f4110cdb29484563d099714f`, compilación local con base 54.

La pantalla responde `HELLO waveshare-7c 0.3` e informa memoria interna libre 64.792 bytes, hueco mayor 31.732, mínimo 54.540; PSRAM libre 6.011.792, hueco mayor 5.898.228 y caché 7.123 KB. Son lecturas de arranque, no una prueba prolongada ni demostración de ausencia de fugas.

Las comprobaciones remotas del mismo commit terminaron correctamente: GitHub Actions [Pruebas](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39769729525) y [Firmware](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39769729510). Esta última compila el catálogo completo. No se ha fusionado ni publicado en main.

**Prueba real de transiciones todavía pendiente:** al abrir la observación, COM7 dejó de estar disponible. La enumeración posterior solo muestra la pantalla COM6 y puertos ajenos. Se pidió al dueño reconectar la mini al PC para poder observar ambas. No se ha iniciado automáticamente el partido de un minuto ni se han simulado goles. No confundir esta instalación y las simulaciones aprobadas con una validación completa de las tres transiciones en la placa real.
