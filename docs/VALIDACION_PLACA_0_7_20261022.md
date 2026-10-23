# Validación física de pantalla y paquete 0.7 · equipo B · 22/10/2026

**0.7 instalada en COM6; 123 WAV verificados en SD, 98 activos y 25 reservados, comprobados después de reiniciar.** Rama `equipo-b/rendimiento-esp32s3`, código `642a4a0`, sin fusionar main ni publicar OTA. [Entrega técnica y reglas de cada familia](ENTREGA_VARIEDAD_SONIDOS_0_7_20261022.md) · [Catálogo instalado con procedencia y hashes](sonidos/catalogo_fc26_123_instalados_0_7.json).

## Grabación y datos

Se volvió a identificar la pantalla por USB y se comprobó reposo, versión 0.6, dos partidos y 61 voces previas. Copia privada de los primeros 64 KB, tabla idéntica a la anterior y OTA VALID (secuencias 3/2). App0 activa en 0x10000 de 6 MB. La copia contiene datos privados y no se publica ni se presenta como respaldo completo del historial.

Se escribió únicamente app0: **1,885,664 bytes**, SHA256 `1cc1f1665706d24da9dfa0415e4078664817dd6f5b6529588a0b5110f5f15a4f`, MD5 `37f66a437ee55a78b1e03f3cf4ecb08c`. Esptool confirmó el hash; escritura 110,6 s. NVS, otadata, app1, FFat y SD no se escribieron al grabar firmware. Se conserva el binario 0.6 para recuperación manual. Fuentes copiadas por Arduino comparadas con las actuales antes de grabar. Compilador Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5, PSRAM OPI, flash 32 MB, partición custom, build base 54. Globales 130.048 B, +24 B frente a 0.6. Persiste el aviso previo del enlazador sobre .note.GNU-stack, sin errores.

Después se instaló el paquete autorizado: **61 WAV existentes conservados y 62 nuevos**, sin sobrescribir los anteriores. Se validó SHA256/CRC local antes de abrir el puerto, tamaño/CRC remoto de archivos existentes y nuevas copias después de guardarse, y otra pasada completa de todos antes de activar. El diario registra **246 lecturas completas de CRC32** y confirmación de datos `total=123 activos=98 reservados=25`. No se confió en flags ni inventarios parciales.

Los 123 archivos preparados son distintos por hash de contenido y suman **46.229.236 B**: 34.829.684 B activos y 11.399.552 B reservados. Las rutas conservan los seis antiguos en plano y el resto por prefijo de código. Los originales del dueño siguen intactos y no se subieron audios a GitHub.

Catálogo CMT1 de **98 filas / 500 B**, CRC32 `23e89b56`. Se promovió solo después de verificar todo el paquete y se conservó el catálogo anterior de 61 como respaldo. Tras reiniciar: `COMENTARIOS total=98 limite=1024 grupos=63,3,3,3,0,6,10,6,4`. Los 25 exclusivos de torneo están almacenados y verificados, pero no aparecen en el catálogo automático: el modo Torneos aún no está implementado en la pantalla nativa.

Se conservan exactamente los dos partidos originales 6–3 y 12–6, mismos identificadores y ambos con fin=0. No se inventaron fechas, ajustó la hora, inició partido, envió toque/gol/anulación ni cambió volumen o preferencias para probar. Hubo los reinicios deliberados de instalación y comprobación. Puerto cerrado al terminar.

## Lecturas en reposo después del reinicio

| Medida | Antes (0.6) | Después (0.7) |
|---|---:|---:|
| Interna libre | 58,400 B | 58,408 B |
| Mayor bloque interno | 31,732 B | 31,732 B |
| Mínimo interno desde ese arranque | 48,112 B | 47,948 B |
| PSRAM libre | 6,011,832 B | 6,011,740 B |

Antes del reinicio final, después de toda la transferencia: interna libre 58.408 B, mayor bloque 31.732 B y mínimo de ese arranque 47.508 B; PSRAM libre 6.011.856 B. Sin errores declarados por el reproductor ni por dibujo/VSYNC en esa consulta. Es una observación de la instalación en reposo, no una prueba de audio durante partido.

Capacidad SD por el volumen real: total 8,036,253,696 B; usado 47,890,432 B; libre 7,988,363,264 B. Estos datos no proceden del disco FFat. Las mediciones posteriores al reinicio no incluyen el pico de memoria durante la transferencia anterior ni constituyen un ensayo prolongado de fragmentación o rendimiento durante partido.

Diagnóstico final: `COMENTARIO_ESTADO pendiente=00000000 ultimo=00000000 espera_ms=0 inicio_ms=0 solicitado=00000000 resultado=idle reproductor=idle fallo=none error=0 errores=0 frame_error=0 vsync_timeout=0`. Sin voz pendiente ni error de audio/dibujo/VSYNC observado en estas consultas. Esto no demuestra reproducción acústica ni latencia. El RTC sigue disponible; el desfase cercano a dos minutos documentado en 0.6 no se reajustó ni se considera resuelto aquí.

## Pruebas y límites

Regresión nativa completa, 314 pruebas de aplicación/herramientas, seis de clasificación y 29 de instalador correctas. Los sorteos/rondas/contextos se probaron en PC, incluido catálogo máximo y bit 8 hasta la reproducción pendiente del sketch. Compilación real correcta. En código `642a4a0`, GitHub pasó [Pruebas](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39844847357) y [Firmware](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39844847344), con publicación omitida por ser rama B.

Pendiente con el dueño: escuchar gol normal, combinación 70/30, seguir perdiendo, ventaja de dos, noveno gol y anulación durante voz; medir marcador/altavoz, cortes, botones y memoria bajo juego. No afirmar que la simulación o el CRC miden sonido. Las bolsas viven en RAM y se reinician al arrancar/recargar catálogo. Las frases reservadas no se activarán por aproximaciones de marcador hasta implementar el contexto de torneo. Revisión independiente de A/Cloud pendiente.


## Comprobación posterior de solo lectura · 23/10/2026

El dueño aclara que pide comprobar el paquete instalado, no desarrollar Torneos. COM6 vuelve a identificar pantalla 0.7. Se validaron SHA256/CRC de los 123 archivos locales antes de abrir el puerto y se releen tamaño y CRC32 completos de los **123 WAV de la SD, todos coincidentes**. Catálogo: `COMENTARIOS total=98 limite=1024 grupos=63,3,3,3,0,6,10,6,4`; 98 activos y 25 reservados, con los dos partidos originales intactos. SD: total 8.036.253.696 B, usado 47.890.432 B, libre 7.988.363.264 B, igual a la instalación anterior.

No se enviaron órdenes de firmware, subida, borrado, activación de catálogo, reproducción, toques, goles ni ajustes. Solo consultas; puerto cerrado al finalizar. Diagnóstico final sin errores de audio/dibujo/VSYNC. La comprobación valida integridad y catálogo; no es una prueba de audición ni de latencia durante partido. El registro privado `comprobacion-sonidos-20261023/resultado.json` contiene las 123 coincidencias y los estados antes/después. Sigue pendiente la revisión independiente de A/Cloud y la prueba acústica con el dueño.
