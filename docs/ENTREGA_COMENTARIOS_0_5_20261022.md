# Comentarios nativos 0.5 · selector por situaciones · equipo B · 22/10/2026

## Estado real de esta entrega

Código implementado en la rama `equipo-b/rendimiento-esp32s3`, sin fusionar main. La pantalla física sigue en **0.4**; únicamente están confirmados en su SD los **seis WAV del grupo 1**. No se ha instalado 0.5 ni activado el catálogo nuevo. El dueño pidió poder crecer hasta unos 800 sonidos y que se avise al terminar la instalación para probar juntos. Se prepara candidato 0.5 y paquete de 55 audios; la grabación se consulta antes, conforme al límite del dueño.

Los originales se localizaron finalmente en `Descargas/SONIDOS FIFA` después de que cambiaran de carpeta durante la preparación. Se conservaron intactos y no se incorporan los WAV a GitHub. Hay otras exportaciones nuevas en Descargas que no forman parte de los 55 archivos enviados: no se han interpretado ni añadido por iniciativa propia.

## Reglas funcionales

| Grupo recibido | Variantes | Pool del catálogo | Condición |
|---|---:|---:|---|
| 1 · After inviting pressure | 6 | 0, comodín | Cualquier gol que suba el marcador; no se mide dominio |
| 2 · Tres o más encajados | 3 | 1 | Interpretación provisional: el receptor acumula al menos tres goles encajados, incluido el actual. Se ha pedido confirmación al dueño por la ambigüedad de quién marca; si aclara que se refiere al atacante que ya había encajado tres, cambiar condición y prueba antes de activar |
| 3 · Con ese gol empatas | 3 | 2 | Marcadores iguales después del gol |
| 4 · Respuesta a mala racha | 3 | 3 | Marca tras dos o más goles válidos SEGUIDOS del rival. Confirmado expresamente; NO se exige que vaya ganando, aunque lo diga el nombre del archivo |
| 5 · Comodín 2 | 20 | 0 | Comodín |
| 6 · Comodín 3 | 20 | 0 | Comodín |
| Anulación | 0 recibidas | 4 | Al bajar por anulación; de momento usa el tono/preferencia anterior |

Son **46 comodines y 3+3+3 contextuales**, 55 en total. Los sufijos `_N_N` son índices de exportación; no se interpretan como marcador. El último criterio del dueño sustituye su petición inicial de sonar al tocar físicamente el botón: el comentario arranca **tras entregar el fotograma con el gol actualizado**; se conserva gol corto al soltar y anulación larga.

`goalMask` contempla los contextos simultáneos y siempre ofrece comodín. `Catalog::pick` elige uniformemente primero entre los grupos elegibles con archivos y después entre sus variantes. Esto permite oír los contextuales sin que los 46 comodines absorban casi todas las elecciones. Con un solo contexto más comodín, el reparto entre esos dos grupos es aproximadamente mitad y mitad; con varios, se reparte entre todos los elegibles. Es la decisión técnica inicial, no una proporción solicitada expresamente por el dueño. Se evita repetir la última variante usada de un mismo pool si tiene alternativas; no se promete un recorrido sin repeticiones de todo el catálogo.

Se excluyen penaltis y goles que no aumentan el marcador, y se omiten registros anulados al contar la racha anterior. Una voz puede acompañar también al gol ganador, sin añadir después tonos de victoria que la corten. Las elecciones explícitas anteriores (`MOMENTO`, incluidos silencio o WAV propio) conservan prioridad sobre el catálogo; el aviso secundario matchPoint no obliga a sustituir la frase elegida. Si un fichero desaparece entre la indexación y su uso, se intenta el tono de respaldo. No hay frases de anulación entregadas: no afirmar que ya suena una voz de anulación.

## Arquitectura, memoria y SD

- `s3_comentarios.h`: catálogo de hasta **1.024 códigos**, en segmentos por pool, con contadores y último índice. Aproximadamente 4,1 KB fijos; staging y verificación se reservan temporalmente fuera del partido (dos bloques adicionales de unos 4,1 KB en el pico de guardado). No se almacenan nombres largos ni voces completas en RAM. La elección recorre cinco grupos como máximo y accede directamente al código: su coste no crece con los 800 archivos. El análisis de la racha recorre historia del motor, acotada por el límite de goles del motor, no por el número de voces.
- Los archivos se validan al cargar/guardar el catálogo (cabecera, tamaño y presencia); hay pausas de servicio cada 16 para no monopolizar el arranque. Los ausentes/corruptos no forman parte de las listas disponibles. La CRC del catálogo comprueba sus registros, no sustituye un hash completo de cada audio en cada arranque.
- Formato persistente **CMT1**: magic de cuatro bytes, cantidad uint16 LE, CRC32 uint32 LE, registros de pool uint8 y código uint32 LE. Los grupos deben estar ordenados, códigos únicos y cantidad 1–1.024 al transferir. Un archivo de 800 entradas ocupa 4.010 bytes. El de 55 ocupa 285 bytes. Carga compatible con copia de recuperación y escritura temporal verificada antes de renombrar; se conserva la copia buena si el principal estaba corrupto.
- Nuevas subidas: `/sonidos/ab/abcdefgh.wav`, carpeta por los dos primeros dígitos del código. Se reduce la búsqueda de nombres de FAT al abrir una pista con cientos de voces. `find/open/list` siguen leyendo la carpeta plana anterior: no se mueven ni duplican automáticamente los seis originales. No hay una garantía matemática de reparto uniforme de todos los códigos entre carpetas; medir el peor grupo con el catálogo final.
- `SONIDOS?` conserva el formato y sus 48 entradas para clientes existentes. **`SONIDOS_PAGINA N`** lista hasta 1.024, en páginas de 32, con total declarado; usa buffers temporales pequeños fuera de la ruta de gol. La web antigua no se ha rediseñado para administrar el nuevo catálogo. No sustituir su límite de cuatro variantes por un número gigante: el catálogo nativo es una capa distinta y compatible.
- Las voces siguen en PCM mono de 16 bits a 16 kHz y se leen por bloques. No se ha creado una tarea adicional ni una petición de nube por gol. La apertura/lectura real de SD y el render siguen costando tiempo: no dar por probado rendimiento idéntico en placa solo por la elección O(5).

## Coordinación y sustitución del audio

`.ino afterChange` elige el comentario y guarda un pendiente; no lo reproduce allí. El bucle actualiza la vista y espera un incremento de `uiport::presented` después del render. Un contador de generación evita arrancar con una imagen antigua cuando el gol llega desde un callback de LVGL dentro del mismo paso. Después se abre el WAV, se alimenta el audio y se registra el tiempo de software. Otro gol o una anulación reemplazan el pendiente, sin encolar voces antiguas.

`Player::stop` deshabilita I2S y vacía la cola de software. Antes de reactivar se rellenan los ocho descriptores DMA (250 tramas cada uno, 125 ms a 16 kHz) con datos de la nueva pista o silencio si es corta. Se reutilizan los buffers anteriores y se leen unos 4 KB de PCM mono antes de habilitar la salida. No se reduce el colchón a costa de introducir cortes. Se han comprobado las APIs de precarga y deshabilitación con la [documentación](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/i2s.html) y el [código oficial de Espressif](https://github.com/espressif/esp-idf/blob/v5.5.1/components/esp_driver_i2s/i2s_common.c).

Esto coordina el comienzo con la entrega de la imagen al controlador; no demuestra aún coincidencia acústica exacta con los píxeles visibles. La apertura del fichero, precarga, retorno vertical y amplificador deben medirse en la placa real. Un WAV de menos de 125 ms puede llevar relleno de silencio al completar la primera DMA; se actualizó esa expectativa concreta del simulador, no la duración de la frase original. Si una operación de I2S falla, se deshabilita el reproductor y se apaga el amplificador en lugar de continuar con datos antiguos; requerirá diagnóstico/reinicio para recuperar el canal.

Se corrige además el timeout de subidas: comparar tiempo firmado y pasar una hora reciente después de procesar USB impide interpretar un timestamp anterior del bucle como ocho segundos sin datos. Se conserva el timeout real y se prueba también el caso de timestamp anterior.

## Herramientas y paquete preparado

`python scripts/preparar-comentarios.py <originales> <salida>` requiere FFmpeg ya instalado y no abre puertos. Reconoce los seis grupos de esta entrega, índices numéricos y genera WAV, catálogo JSON, CMT1 y órdenes de instalación. No promete clasificar futuras frases con nombres nuevos sin definir sus grupos. Rechaza duplicados, cabeceras no compatibles, archivos mudos o mayores de 1 MB. Recorta solo silencio inicial según bloques de 5 ms con umbral RMS −55 dBFS y conserva 20 ms previos; aplica al menos −1 dB y baja más si detecta muestras saturadas después del remuestreo. Se conserva el original.

Paquete actual: **55 WAV, 17.418.566 bytes**. Los seis ya instalados coinciden byte a byte con las copias preparadas; el resto necesita **15.580.688 bytes**, más reserva de estructura y catálogo. El [catálogo de preparación](sonidos/catalogo_fc26_55_preparados_0_5.json) diferencia `upload_verified` (solo seis), `installed_sd_path` de esos seis y `automatic_selection_active=false`. No confundir la ruta preparada de una copia con su ruta instalada. La CRC y cabeceras de los 55 se han comprobado, también con el parser C++ de la placa. Los WAV no se publican en GitHub.

`powershell scripts/subir-comentarios.ps1 -Package <salida> -Port COM6 -Install` es solo instalador de DATOS para firmware 0.5; no graba programa ni inicia partidos. Primero comprueba HELLO y SDINFO? (incluido que no haya partido activo), inventario paginado, hashes locales y espacio para los nuevos WAV + 1 MB conservador de reserva. Conserva los seis confirmados y aborta ante archivos existentes de origen no verificado. Envía bloques de 192 bytes, comprueba cada ACK y CRC al finalizar; registra las confirmaciones para reanudar sus propios archivos. Solo después de tenerlos todos transfiere el catálogo, comprueba su aceptación y el total activo. `-CompileOnly` valida el auxiliar C# sin abrir puertos; ese modo está comprobado. La transferencia nueva de 49 WAV todavía no se ha ejecutado.

La consulta antigua declara `sd=13056` KB; es menor que los nuevos 15,58 MB. **No hay espacio suficiente según ese dato**, y tampoco se ha comprobado si es un valor de FAT almacenado/capacidad efectiva. Instalar primero la aplicación autorizada y consultar `SDINFO?` de 64 bits. Si confirma esa falta, parar y pedir una SD con espacio; no borrar fotos/partidos, no formatear ni degradar voces a 8 kHz por iniciativa propia. El instalador contiene esa protección. No asegurar que 800 voces caben en la tarjeta actual.

## Protocolo nuevo y diagnóstico

`COMENTARIOS_INICIO cantidad` → LISTO; `COMENTARIOS_ITEM pool código` → ITEM_OK; `COMENTARIOS_FIN crc32` → GUARDADO. Cada respuesta lleva el prefijo COMENTARIOS_. `COMENTARIOS_CANCELAR` libera staging; un timeout de 30 s o entrar en partido también lo libera. Mutaciones rechazadas durante partido, sin SD o durante una subida de WAV. Ante error de validación/guardado se mantiene el catálogo activo anterior. La retirada física de SD durante el guardado y un corte real de corriente no se han ensayado; el test de recuperación no sustituye esas pruebas.

Lecturas: `COMENTARIOS?` muestra total disponible y cinco cantidades; `SDINFO?` capacidad/usado/libre en bytes; `COMENTARIO_ESTADO?` pendiente, último código y espera de software hasta la alimentación del audio. La última espera no incluye el tiempo humano de pulsación y no equivale a la latencia acústica. `SONIDOS_PAGINA 0..31` permite auditar presencia más allá de 48.

## Pruebas y revisión para Cloud

Nueva `hardware/test/s3_commentary_test.cpp`: condiciones de 3 encajados, empate, dos seguidos (incluye ir perdiendo, rachas interrumpidas y goles anulados), exclusión de penaltis, 800/1.024 referencias, 100.000 elecciones y presencia de todos los grupos, no repetir dentro de pool, 800 WAV válidos guardados/cargados, duplicados/CRC/cantidad/archivos ausentes, timeout, rechazo durante partido, principal corrupto/copia de recuperación, fallo de renombrado, exclusión de ausentes, sustitución del audio en DMA simulado, comienzo después del frame, anulación y preferencias explícitas, respaldo si falta el fichero, listado después de la entrada 48 y fallo de parada I2S que debe dejar el amplificador apagado.

Los primeros fallos del nuevo test fueron referencias al campo de salida equivocado del simulador y una expectativa de dos registros cuando se había eliminado intencionadamente el segundo WAV; se corrigió el escenario/lectura y se mantuvo la comprobación de exclusión de ausentes. No se rebajó la validación del producto para hacer pasar el caso. Las primeras ejecuciones del preparador fallaron al reconocer los archivos y al dejar de estar en la raíz de Descargas; el paquete definitivo usa la carpeta correcta y el patrón probado. Esos intentos no escribieron datos en la placa.

La regresión nativa completa y los seis tests de versiones han pasado. Tras añadir el último control del canal se repitieron el nuevo test completo y la simulación de compatibilidad de Waveshare 7C, ambos correctos. El instalador C# compila en modo -CompileOnly sin abrir puertos. Se registran abajo compilación y huellas. No considerar instalada esta entrega por estar compilada. La prueba de 100.000 elecciones tarda alrededor de 8–10 ms en este PC e incluye trabajo del test: no es un benchmark de SD, del ESP32 ni de latencia de audio. Tampoco se afirma fragmentación nula por el catálogo fijo: las operaciones de FS siguen usando sus objetos de fichero.

## Instalación y primera prueba autorizada

1. Obtener aprobación para grabar 0.5. Verificar COM/identidad, tabla y app0 actual; copia privada del arranque. Escribir únicamente aplicación en 0x10000, sin NVS/FFat/SD ni imagen merged completa. Conservar binario 0.4 para recuperar.
2. Verificar HELLO, SDINFO?, memoria, reloj e historial. Si falta espacio en SD, detener datos y coordinar con el dueño una tarjeta adecuada, conservando originales.
3. Con espacio, subir los 49 restantes y activar los 55 por el instalador. Consultar catálogo e inventario paginado, revisar que continúan los dos partidos y que el puerto queda libre.
4. Avisar al dueño cuando firmware, datos y catálogo estén verificados. Él iniciará el primer partido de prueba; no generarlo automáticamente. Probar gol normal, tercero recibido, empate, respuesta tras dos seguidos, combinaciones con comodín y anulación durante la frase. Hasta que lleguen voces de anulación se espera el tono de respaldo.
5. Medir pantalla y altavoz juntos (mediana, p95 y peor caso), memoria antes/después, gol después de reposo y durante voz previa; luego repetir con catálogo de 800 archivos reales. El ensayo de corriente/SD retirada requiere consulta adicional. La revisión independiente de Cloud sigue pendiente.

## Candidato final listo (sin instalar)

Compilación Arduino-ESP32 3.3.12 para Waveshare 7C con PSRAM OPI, flash 32 MB, partición custom y build base local 54: correcta. Aplicación **1880944 bytes**, variables globales **127.832 bytes** (4.160 más que 0.4). Sigue cabiendo en app0 de 6 MB; el máximo genérico de 16 MB mostrado por Arduino no cambia esa tabla real. SHA256 `c5718d702bca3e38d471b0651605e2b6b365c74a67e27ea706208157999370ee`; MD5 `a18546180ad41af1a247cc8bc5c2ad68`. Cabecera ESP32-S3 y versión 0.5 verificadas. Se conserva el aviso del enlazador del toolchain sobre .note.GNU-stack; no hubo errores de compilación.

Regresión completa final correcta; después del último control de parada I2S y del campo partido en SDINFO? se repitieron los tests afectados de comentarios y compatibilidad 7C y se compiló de nuevo este binario exacto. Seis pruebas de registro de versiones correctas. El modo de compilación del instalador C# no abrió puertos. Quedan pendientes aprobación de grabación, capacidad real de SD, transferencia de 49 voces y catálogo, y prueba acústica con el dueño.
