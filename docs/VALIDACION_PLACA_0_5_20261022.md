# Validación física de pantalla 0.5 · equipo B · 22/10/2026

## Resultado

El dueño autorizó instalar la 0.5 con los sonidos y el selector rápido. **Firmware final 0.5 instalado en COM6; 61 voces presentes y catálogo activo, comprobados otra vez después de reiniciar.** Código de firmware: `dfb1a98`, rama `equipo-b/rendimiento-esp32s3`, sin fusionar main ni publicar OTA. Las próximas familias solo se documentan en [el plan de eventos](PLAN_COMENTARIOS_EVENTOS_20261022.md); no se han añadido voces inexistentes ni activado comentarios de inactividad.

## Grabación y conservación

Se identificó la Waveshare 7C ESP32-S3 real y se guardaron los primeros 64 KB en una copia privada. Tabla igual a la anterior; app0 en 0x10000 de 6 MB, selección OTA VALID con secuencias 3 y 2 y CRC correctas. La copia contiene información privada y no se publica; no es un respaldo completo del historial.

Se escribió únicamente app0, sin grabar NVS, otadata, app1, FFat ni SD con esptool. Se conservó el binario 0.4 para recuperación. Hubo dos escrituras locales de 0.5: la primera permitió comprobar el error heredado de capacidad; la segunda contiene su corrección. Ambas verificaron el hash después de escribir. Cada escritura tardó unos 110 s. No se publicó una entrega oficial entre ambas revisiones internas.

Binario final: **1.881.280 bytes**, SHA256 `7e4425317ade700568c64ecc311b3da41252d9d2c817d33330b13c0150132f99`, MD5 `c2f205a1368e2938e79cb138d7f2f1a7`. Compilado con Arduino-ESP32 3.3.12, PSRAM OPI, flash 32 MB, partición custom, build base 54. Globales 127.832 bytes. Se conserva el aviso conocido del enlazador sobre .note.GNU-stack, sin errores.

Antes y después, incluidos carga y reinicio, HISTORIAL? conserva exactamente los dos originales: 6–3 y 12–6, mismos identificadores, ambos con fin=0. No se repararon fechas ni se iniciaron partidos. El RTC responde valida=1 rtc=1, pero eso NO prueba exactitud: la consulta registrada a las 17:09:42 UTC (19:09:42 local) devolvió 19:07:39, unos 123 s de retraso respecto al PC. Se deja pendiente investigar ese desfase tras la sesión de grabación; no atribuirlo a una causa sin medir. No se cambió la hora ni se ensayó desconexión eléctrica. No se enviaron toques, goles, anulaciones, preferencias de audio ni cambios de ajustes para esta validación.

## Error de capacidad identificado y corregido

El valor previo de 13.056 KB libres pertenecía a FFat, no a la SD. En Arduino 3.3.12, SD_MMC.totalBytes/usedBytes llaman a f_getfree("0:"). FFat se monta primero, por lo que ese disco era la flash interna: total 20.623.360, usado 7.254.016, libre 13.369.344 bytes. La primera SDINFO? de 0.5 reprodujo el mismo dato; imprimir uint64_t por sí solo no lo corregía.

Ahora FsStore::sdSpace consulta esp_vfs_fat_info("/sdcard", ...) y valida el resultado. Lo usan SDINFO? y el espacio declarado por SONIDOS?. Al terminar: **total 8,036,253,696; usado 21,180,416; libre 8,015,073,280 bytes**. La SD real tiene capacidad suficiente; no se compró, formateó ni sustituyó tarjeta y no se borraron otros archivos.

La regresión prueba un volumen de 32 GB distinto del devuelto por SD_MMC, errores de consulta y free>total; ante fallo devuelve no disponible/cero. La regresión nativa completa y la compilación real pasaron tras la corrección. No se consulta capacidad en cada gol.

## Transferencia y catálogo

Se conservaron los seis WAV del grupo 1 en sus rutas planas antiguas. Se copiaron los otros 55 (18.021.996 bytes) en carpetas por prefijo de código, comprobando tamaño esperado y CRC32 en la recepción de cada archivo. Los 61 suman 19.859.874 bytes PCM mono, 16 bits, 16 kHz. Los originales exportados siguen intactos y los WAV no se publican en GitHub.

Después de verificar la presencia de todos mediante inventario paginado, se transfirió y aceptó el catálogo CMT1: 61 registros, 315 bytes, CRC32 `34a5b615`. Resultado antes y después del reinicio: `COMENTARIOS total=61 limite=1024 grupos=46,3,3,3,0,6`. Los dos listados paginados posteriores contienen exactamente los 61 códigos esperados; la recarga valida cabeceras y excluye ausentes. No equivale a releer y calcular SHA256 de todo el contenido físico de cada WAV.

El [catálogo instalado](sonidos/catalogo_fc26_61_instalados_0_5.json) distingue las rutas realmente instaladas, la activación y el método de comprobación. Los catálogos de 55 y 61 preparados son instantáneas anteriores. No confundir la ruta por prefijo preparada para los primeros seis con su ruta plana conservada.

## Memoria, límites y primera prueba

Lectura puntual después de reiniciar: `MEMORIA interna 60544 (mayor 31732, minima 50300) psram 6011380; MEMORIA psram mayor hueco 5898228, cache 7123 KB`. Catálogo disponible de 61, sin comentario pendiente y sin partido activo. No se observó un reinicio inesperado en estas operaciones; esto no es un ensayo prolongado de fragmentación o de audio durante juego.

El selector por grupo y código ya está instalado: no busca en todos los archivos de SD para sortear una frase. La reproducción sigue abriendo el WAV elegido y precargando DMA tras presentar el marcador. La simulación cubre 800/1.024 referencias, anulación y sustitución DMA; no demuestra latencia acústica de la placa. No se ha medido todavía el sonido contra el momento visible del gol ni probado 800 voces físicas.

No se reprodujeron frases ni se iniciaron partidos automáticamente. Primera prueba con el dueño: gol normal, gol tras haber encajado tres, empate normal, empate con dos goles previos en 60 s, respuesta tras dos goles seguidos del rival y anulación durante una frase. Los específicos alternan con comodines; una situación concreta no obliga a elegir su grupo. Aún faltan voces de anulación: se espera el respaldo configurado. Comprobar audición, cortes, respuesta táctil y botones, mediana/p95/peor latencia y memoria bajo carga. El puerto se cierra al terminar.

La revisión independiente de A/Cloud continúa pendiente. Main sigue en 0.2; no confundir la instalación local de 0.5 con publicación oficial.
