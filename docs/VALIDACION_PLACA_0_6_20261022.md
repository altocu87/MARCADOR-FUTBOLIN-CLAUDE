# Validación física de pantalla 0.6 · equipo B · 22/10/2026

**0.6 instalada y verificada en la pantalla Waveshare 7C, COM6.** Código `8b17155`, rama `equipo-b/rendimiento-esp32s3`; sin fusionar main ni publicar OTA. Se conserva la numeración 0.x. [Entrega técnica y límites para A/Cloud](ENTREGA_SONIDOS_0_6_20261022.md).

## Grabación autorizada

Se volvió a enumerar el USB y a identificar el ESP32-S3 real. Antes de grabar: versión 0.5, sin partido activo, 61 voces y dos partidos originales. Se guardó una copia privada de los primeros 64 KB; contiene información privada, no se publica ni se presenta como respaldo completo del historial.

Tabla idéntica a la anterior: app0 `0x10000` de `0x600000`, app1 `0x610000` de `0x600000`, NVS `0x9000`, otadata `0xe000`, FFat `0xc10000`. Selección OTA VALID, secuencias 3 y 2, CRC correctas, app0 activa. Fuentes copiadas por Arduino comparadas con las actuales antes de grabar.

Se escribió únicamente app0, con esptool sin stub, manteniendo NVS, otadata, app1, FFat y SD. **1.888.928 bytes, 110,7 s de escritura, hash verificado por esptool.** SHA256 `417b377f1a5724c8fdc898166dfb8c3d3c9cf915a78ec3a4af8bb6e44093b700`; MD5 `c3c075030a75622af40a11d587d46e68`. Se conserva privadamente el binario 0.5 para recuperación manual. Se comprobó el arranque 0.6 y después se realizó otro reinicio de comprobación.

## Sonidos y datos conservados

El instalador 0.6 validó primero hash SHA256/CRC de los 61 WAV locales y después consultó `SONIDO_INFO` para leer tamaño y CRC32 del contenido completo de cada archivo real. Realizó otra pasada completa antes de activar el catálogo: **122 lecturas de CRC, 61 archivos conservados, cero subidas o sustituciones de WAV**. No confió en las marcas antiguas de transferencia ni en listados de 48/1.024 posiciones. El diario privado corresponde a esta placa/capacidad y registra `CONSERVADO_VERIFICADO` para los 61.

Se reactivó el mismo catálogo CMT1 (315 bytes, CRC `34a5b615`), conservando su respaldo anterior. Después del reinicio:

`COMENTARIOS total=61 limite=1024 grupos=46,3,3,3,0,6`.

Los seis WAV originales siguen en rutas planas y los otros 55 en carpetas por prefijo. Los 61 suman 19.859.874 bytes. No se publican audios ni originales en GitHub. El [registro instalado 0.6](sonidos/catalogo_fc26_61_instalados_0_6.json) distingue esta verificación completa de la realizada en 0.5; la instantánea anterior se conserva intacta.

Historial idéntico antes, después y tras reiniciar: dos partidos, 6–3 y 12–6, mismos identificadores y ambos con `fin=0`. No se inventaron fechas, no se iniciaron partidos y no se enviaron goles, anulaciones, toques ni ajustes de audio. El reinicio y la reactivación del catálogo eran parte de la instalación autorizada. Se cerró el puerto al terminar.

## Memoria y diagnóstico en reposo

| Lectura puntual | Antes, 0.5 | Después de reiniciar, 0.6 |
|---|---:|---:|
| RAM interna libre | 60.576 B | 58.388 B |
| Mayor bloque interno libre | 31.732 B | 31.732 B |
| Mínimo interno desde ese arranque | 50.300 B | 48.184 B |
| PSRAM libre | 6.011.396 B | 6.011.808 B |
| Mayor bloque PSRAM libre | 5.898.228 B | 5.898.228 B |
| Caché gráfica | 7.123 KB | 7.123 KB |

Compilador: globales 130.024 B, +2.192 B frente a 0.5. El incremento está dominado por el buffer fijo de inspección CRC; no crece por cada voz. La ligera diferencia de PSRAM entre lecturas no se presenta como una optimización demostrada. Las lecturas posteriores al reinicio no incluyen los picos del arranque anterior durante las dos pasadas CRC, ni validan fragmentación prolongada durante partido.

SD real: total 8.036.253.696 B; usado 21.184.512 B; libre 8.015.069.184 B. Respecto a antes, +4.096 B de ocupación al conservar también el respaldo de catálogo; no se reescribió ningún WAV.

`COMENTARIO_ESTADO?`: sin pendiente, reproductor `idle`, `fallo=none`, error 0, errores 0, fallos de dibujo 0 y timeouts VSYNC 0. Esto confirma el estado de reposo tras arrancar; no es una prueba acústica de reproducción. No se observó un reinicio inesperado en estas operaciones; hubo los reinicios deliberados de instalación/comprobación.

`RELOJ?` sigue declarando válido y RTC disponible, pero conserva un retraso cercano a dos minutos respecto al PC (lecturas puntuales: ~122,7 s antes y ~116,9 s después). No se ajustó la hora ni se atribuye esa diferencia a una causa demostrada. El arreglo de tiempos del motor evita underflow al reanudar; no es un ajuste del reloj físico.

## Pruebas y lo que falta

Regresión local completa correcta, compilación real correcta, 314 pruebas de app/herramientas y 21 del instalador correctas. En el commit de código `8b17155`, GitHub terminó correctamente tanto [Pruebas](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39839119130) —incluye navegador aislado para QR/sesión, firmware simulado y nuevo trabajo Windows del instalador— como [Firmware](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39839119121), con todas las variantes compiladas. No se publicó la release oficial por trabajar en la rama B.

Pendiente con el dueño: oír y medir gol normal, contextos/comodines, anulación durante voz, gol ganador, animales y penalti. La simulación cubre las reglas y fallos; no mide el tiempo desde el marcador hasta el altavoz. Repetir posteriormente con unas 800 voces físicas y condiciones de SD/red reales. Ensayos de corte de corriente/retirada de SD requieren un plan separado y autorización. Revisión independiente de A/Cloud: pendiente.
