# Verificación física · pantalla 0.11 · 23/10/2026

**Instalada y verificada en COM6: 0.11, código `63ad580`, 560 WAV en SD (496 activos + 64 de torneo reservados).** Conserva los cuatro partidos presentes justo antes de grabar y los 123 audios anteriores. El dueño autoriza integrar en main al terminar; la solicitud es [PR 21](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/pull/21). La revisión independiente del equipo A de esta entrega sigue pendiente; no se confunde con la de 0.3–0.7 ya realizada.

## Programa

Integra las versiones de A 0.8/0.9 y todos los lotes de sonido de esta entrega. La 0.10 no se instaló por separado. FQBN Waveshare 7C: ESP32-S3, PSRAM OPI, flash 32 MB, CDC, partición custom; build base local 54. Arduino-ESP32 3.3.12, LVGL 9.5.0. Fuentes copiadas por Arduino verificadas contra las del sketch antes de grabar.

- Binario: **1,936,576 bytes**, globales 136.352 bytes.
- SHA256: `28e1876f0e8be5f3abb9358f02e44773b757f70f9e830900de82f77f2888887b`.
- MD5: `dfc73bbc64903df2d68d71d2db3acbac`.
- Destino: **app1, 0x610000**, capacidad 0x600000; confirmada activa por OTA secuencia 4/estado VALID, ambos CRC de OTA válidos. app0 sigue con el programa previo. No escribir siempre 0x10000: la actualización de A 0.8 había pasado a app1.
- Se guardaron en privado los primeros 64 KiB antes de grabar, con tabla de particiones idéntica a la anterior. Solo se escribió el binario de aplicación, nunca una imagen completa de 32 MB: NVS, OTA data, FFat y SD no se borraron ni reformatearon. Es una copia del arranque/configuración, no una copia completa de partidos.
- Esptool confirmó **Hash of data verified**. Después del arranque, HELLO respondió 0.11 y los cuatro partidos seguían iguales, antes de cargar sonidos.

## Paquete SD y comprobaciones

437 WAV nuevos; 123 anteriores conservados, sin sobrescrituras. WAV PCM mono 16 bits / 16 kHz; originales intactos. Tamaño total: 146.192.816 bytes, sin contar clusters/directorios FAT. El nombre GOL EMPATE_0_0 reutilizado con contenido distinto recibió otro código: se conserva la voz antigua reservada.

Se validaron SHA256/CRC/tamaño locales, se consultó identidad y capacidad real de SD y se rechazó cualquier archivo existente con contenido diferente. Tras cada transferencia se releyó todo el archivo para CRC; antes de activar se releyeron todos otra vez, incluidos los anteriores y los reservados: **1120 comprobaciones remotas completas**, al menos dos por cada WAV. Incertidumbres de ACK: 0; reintentos de archivo completo: 0.

Catálogo CMT1: **496 filas, 2.490 bytes, CRC32 `416bfecd`**. Se activó solo después de verificar los 560 WAV. Los 64 exclusivos de torneo quedan en SD y fuera de CMT1. El programa conserva `catalogo.bak` anterior; no se reenvió el catálogo para una mera comprobación.

Tras completar la transferencia se reinició la placa y se consultó de nuevo en solo lectura: versión 0.11, 496 entradas y los 24 grupos correctos. No se enviaron toques, goles, órdenes de ajuste, altas/borrados ni partidos de medición; no se reprodujeron voces deliberadamente para validar.

```
COMENTARIOS total=496 limite=1024 grupos=87,3,11,3,0,6,10,6,4,11,14,15,10,7,6,6,5,5,14,20,2,2,132,117
COMENTARIO_ESTADO pendiente=00000000 ultimo=00000000 espera_ms=0 inicio_ms=0 solicitado=00000000 resultado=idle reproductor=idle fallo=none error=0 errores=0 frame_error=0 vsync_timeout=0
SDINFO disponible=1 partido=0 total=8036253696 usado=149299200 libre=7886954496
```

## Partidos y reloj

Antes y después coinciden los cuatro IDs, fechas almacenadas, modos y marcadores: rápidos 6–3 y 12–6 sin fecha, más 3–1 y 1–0 con fecha. La primera consulta de la mañana veía dos partidos; al consultar inmediatamente antes de grabar ya había cuatro. No se atribuyen los adicionales a esta entrega y no se inventan fechas para los dos antiguos.

El RTC respondió válido inmediatamente antes de grabar y después de reiniciar. En una consulta inicial de la mañana había respondido no válido; B no envió HORA ni modificó el reloj en esta entrega. No se da por diagnosticada su retención o sincronización por estas lecturas.

## Memoria y límites de lo comprobado

Antes (0.8):
```
MEMORIA interna 52284 (mayor 31732, minima 42072) psram 6004880
MEMORIA psram mayor hueco 5898228, cache 7123 KB
```
Después (0.11, catálogo completo y reinicio):
```
MEMORIA interna 51800 (mayor 31732, minima 41420) psram 6004324
MEMORIA psram mayor hueco 5898228, cache 7123 KB
```

Catálogo fijo 4.200 bytes (4.140 en 0.7), buffers de reproducción iguales y sin cargar voces completas. Los comentarios ambiente esperan en RAM, sin lecturas SD ni repintados mientras no toca hablar. Se mantienen memoria libre y contador de fallos observables; esto no demuestra rendimiento acústico durante una partida ni ausencia de fragmentación tras días de uso.

Pruebas locales y GitHub correctas: regresión nativa completa, 315 pruebas habituales de app, paridad QR adicional con 40 partidos, 10 de Python, 33 casos del instalador, tipos y compilación real. Aún pendiente: escuchar jugando con el dueño, medir botón→fotograma→altavoz, mezcla de volúmenes, cortes de intro/ambiente, Wi-Fi real durante el juego y sesiones largas. No se han simulado pulsaciones en la placa real ni usado partidos guardados como banco de pruebas.

Reglas y desglose de las 24 familias: [entrega a equipo A](ENTREGA_SONIDOS_0_11_20261023.md). El firmware OTA solo lleva el programa; las voces están en SD y no se publican en Git. Una placa sin este paquete usa los grupos que tenga o su respaldo, sin inventar voces recibidas.
