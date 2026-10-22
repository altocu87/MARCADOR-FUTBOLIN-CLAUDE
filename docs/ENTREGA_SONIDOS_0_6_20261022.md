# Sonidos nativos 0.6 · corrección conjunta de tres equipos · 22/10/2026

Entrega del equipo B para A/Cloud. Rama `equipo-b/rendimiento-esp32s3`, sin fusionar main ni publicar OTA. El propietario ha autorizado implementar y grabar esta corrección; 0.6 instalada y 61 voces/dos partidos conservados tras reiniciar. La comprobación de instalación y sus límites se registra en [VALIDACION_PLACA_0_6_20261022.md](VALIDACION_PLACA_0_6_20261022.md); este documento no sustituye una prueba acústica.

## Qué mejora para el dueño

| Cambio | Efecto esperado | Evidencia y límite |
|---|---|---|
| Abrir directamente el archivo elegido y copiar directamente el PCM de 16 kHz | Menos trabajo al arrancar cada frase | Pruebas de apertura/muestras. Latencia acústica aún sin medir |
| Mostrar el gol ganador antes de completar su guardado | Evitar que el acceso a datos retrase el primer aviso de victoria | Prueba de entrada normal y dentro de un callback real de LVGL |
| Validar archivos y conservar el anterior durante una sustitución | No consumir el único respaldo si falla la SD | Fallos simulados de lectura, renombrado, recuperación y checksum; no ensayo físico de corte eléctrico |
| Verificar los bytes reales de cada sonido instalado | No dar por instalado un archivo por un diario antiguo o una confirmación perdida | 21 pruebas del instalador y consulta incremental de CRC en el firmware |
| Bloquear gestión pesada mientras se juega | Una carga, listado o prueba manual USB no interrumpe el partido | Pruebas de órdenes mayúsculas/minúsculas y de inicio durante una carga |
| Simplificar animales y penalti | Un aviso coherente por cambio, sin narraciones duplicadas | Integración gato/ardilla de ambos equipos y penalti por botón/interfaz |

No se modifica la mini C3 ni su cableado. No se han añadido voces que el dueño no haya entregado. Las seis familias actuales y los 61 códigos continúan compatibles. La app web conserva su propio comportamiento: esta entrega trabaja en el reproductor nativo y sus herramientas de SD.

## Trabajo y revisión cruzada

Se han usado tres agentes separados: tiempo real/audio, reglas/eventos y almacenamiento/instalador. Cada uno implementó su ámbito; después revisaron los contratos de los otros y la integración. Los bordes encontrados en el cruce —órdenes minúsculas, victoria recibida dentro del dibujo, RIFF incoherente, ACK atrasado y reproducción durante recuperación ambigua— se corrigieron y añadieron a las pruebas. A/Cloud sigue pendiente de revisión independiente; estos tres agentes pertenecen al trabajo B.

## Reglas acordadas para Partido Loco

- Un animal que suma marcador usa un gol comodín. No se deducen remontadas ni tiempos de gol a partir de eventos animales sin esos datos.
- Si el evento resta marcador, usa anulación. Si transfiere goles y simultáneamente suma/resta, se emite una sola anulación, con prioridad sobre el gol. No se encolan dos voces contrapuestas.
- El penalti de hándicap emite `PenaltyAwarded` una vez al señalarlo, distinguiendo la secuencia. Resolverlo como marcado o fallado no añade otro comentario de gol. Respeta silencio, sonido personalizado y predeterminado; de momento el predeterminado es un tono, porque no se ha entregado la voz correspondiente.
- Esto no rediseña las reglas del juego ni implementa una narración completa de Partido Loco. Las próximas familias siguen en [PLAN_COMENTARIOS_EVENTOS_20261022.md](PLAN_COMENTARIOS_EVENTOS_20261022.md).

Los contextos normales mantienen su significado: marcar habiendo encajado tres o más, empatar, responder tras dos del rival seguidos y empatar con dos goles anteriores en los últimos 60 segundos de juego. Los comodines siguen elegibles. Se omiten registros de valor cero y se usa la diferencia real de marcador en los sonidos anteriores del motor.

## Flujo y coste

`evento del motor → grupos elegibles en RAM → código elegido → fotograma confirmado → una apertura SD/FFat → validación WAV → bloques PCM → DMA/I2S → altavoz`.

La SD conserva las voces completas. En RAM se mantienen códigos/índices compactos y buffers fijos; no se cargan 61 ni 800 frases enteras. Se mantiene el catálogo CMT1 de seis pools y límite total de 1.024 referencias. El sorteo consulta seis grupos; no recorre directorios. Las rutas por prefijo se prueban directamente y, solo si faltan, se prueba la ruta plana antigua.

No se ha implementado la propuesta de abrir 5+2+4 candidatos durante la cuenta atrás: elegir códigos ya cuesta muy poco y abrir varios archivos aumentaría descriptores, memoria y trabajo al empezar. La mejora aplicada reduce la apertura del sonido finalmente elegido y evita el remuestreo de los WAV ya preparados a 16 kHz. Una precarga de PCM futura requiere medir primero el tiempo de apertura real; no se promete carga gratuita.

Compilación real: Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5, Waveshare 7C, PSRAM OPI, flash 32 MB, partición custom y build base 54. Aplicación binaria **1.888.928 bytes**; globales **130.024 bytes**. Frente a 0.5: +7.648 bytes de binario y +2.192 bytes de globales (incluye el bloque fijo de inspección CRC de 2 KB). Cabe en app0 de 6 MB. La cifra de globales no equivale a RAM libre en funcionamiento. Persiste el aviso conocido del enlazador sobre `.note.GNU-stack`, sin error de compilación.

SHA256 del candidato: `417b377f1a5724c8fdc898166dfb8c3d3c9cf915a78ec3a4af8bb6e44093b700`. MD5: `c3c075030a75622af40a11d587d46e68`. Las fuentes copiadas por Arduino se compararon con las actuales, normalizando únicamente la directiva inicial de ruta y los saltos de línea finales añadidos por la herramienta.

## Archivos y decisiones para A/Cloud

| Archivo / función | Cambio |
|---|---|
| `mfv3_audio.h`: `FsStore::open`, commit y recuperación WAV2 | Apertura directa; temporal y respaldo con marcador de 24 bytes, CRC del archivo y del marcador. Antes de retirar el respaldo se valida la copia completa. Ante recuperación ambigua conserva copias, bloquea nuevas subidas y también la reproducción; no formatea FFat automáticamente |
| `mfv3_audio.h`: `validWavFile`, `Player::pump` | Validación consistente de RIFF/tamaño/alineación; lectura e I2S comprobados, fallo explícito y amplificador apagado. Timeout normal de escritura no se confunde con error fatal. No reinicia el dispositivo de audio automáticamente durante partido |
| `common/mfv3_sound.h` y copia S3 | Parser acotado, sin bucles por chunks malformados; remuestreo con intermediario de 64 bits y copia exacta para 16 kHz, incluida la última muestra. `PenaltyAwarded` se añade al final para conservar índices previos |
| `common/mfv3_engine.h` y copia S3 | Evita restar un instante anterior al inicio/reanudación y protege deadlines cortos al desbordar `millis()`. Esto no corrige ni ajusta el RTC físico |
| `.ino`: `afterChange`, `loop`, `saveResult`, `saveMatchFile` | Captura fecha/instante al terminar; difiere guardado hasta presentar victoria y arrancar el aviso, con rescate a 250 ms. Exige resultado pendiente antes de render para no confundir una vista antigua emitida tras un callback. Bloquea Inicio/revancha hasta guardar; servicio cooperativo de audio durante serialización |
| `.ino`: `handleBoardLine`, `inspectSoundTick`, `boardCommand` | `SONIDO_INFO` calcula tamaño/CRC del archivo real en bloques de 2 KB, sin bloquear toda la interfaz. Gestión excluida de partido; comparación sin distinguir mayúsculas de la palabra de orden, sin tocar los argumentos/base64. `SD_IDENTIDAD?` distingue placa y capacidad; no finge disponer del CID de la SD |
| `s3_partidos.h`: `save` | Callback opcional de servicio entre operaciones de escritura. Conserva fecha final capturada y evita doble guardado. No convierte una llamada individual a SD/NVS en asíncrona |
| `s3_comentarios.h`: `validAudio`, guardado | Mismo validador que el reproductor y respaldo de catálogo conservado ante fallo al promover el temporal. Excluye WAV truncados/incoherentes |
| `ui_port.h`: `flush` | Solo cuenta fotograma presentado si funcionan dibujo y VSYNC. Contadores separados de fallos; un comentario sin fotograma se descarta a 500 ms, no se adelanta al marcador |
| `CommentarySdInstaller.cs` y scripts | Preflight local completo, firmware mínimo 0.6, CRC remoto de existentes/nuevos antes de activar catálogo. Una diferencia en archivo existente detiene sin borrar. Pérdida de ACK de bloque reinicia archivo completo; pérdida de ACK final consulta el resultado real. READY/DATOS 12 s, FIN 60 s, checksum 60 s; escritura USB parcial detiene y cierra |
| Pruebas y CI | Nuevo trabajo Windows para instalador ficticio y compilación separada; regresión nativa integra resiliencia, exclusión/ganador y animales/penalti |

`COMENTARIO_ESTADO?` distingue código solicitado, voz realmente iniciada, silencio, respaldo y fallo. Incluye estado/error del reproductor y fallos de dibujo/VSYNC. El instante de inicio es digital (precarga/arranque I2S), no una medición del sonido saliendo del altavoz. `busy()` incluye trabajo pendiente del productor, no garantiza vaciado físico del DMA.

## Verificación local del candidato

- Regresión nativa completa correcta: núcleo, motor, radio, OTA, audio, nube, jugadores, progresión, mando, tres variantes de pantalla, prototipos Arduino y pruebas nuevas. Se usan LVGL real y periféricos simulados.
- Integración de catálogo: 800 WAV simulados / 1.024 referencias, mezcla, CRC, persistencia, ausencia y truncamiento; parser contra los WAV preparados. No equivale a 800 archivos ensayados en la SD real.
- Audio: sustitución y recuperación con fallos, cabeceras truncadas, error de lectura, reloj DMA simulado, timeout y error fatal de I2S; copia exacta y última muestra.
- Ganador: guardado diferido, entrada desde callback LVGL, imposibilidad de pisar resultado con revancha/Inicio, silencio y respaldo sin métricas falsas.
- Instalador: **21 casos** con transporte ficticio; incluye ACK perdidos/atrasados, FIN de 45 s, escritura parcial, corrupción, otra placa, flags antiguos, poco espacio y límites/versiones. `-CompileOnly` en otro proceso correcto, sin puertos reales.
- **314 pruebas** de app/herramientas correctas (25 archivos), incluidas versiones. Compilación real del firmware correcta. No se han modificado componentes web.

La regresión antigua probaba reproducción/borrado USB durante un partido; ahora verifica el rechazo y realiza la gestión en reposo. No se ha relajado la protección para conservar ese escenario. La prueba de historial espera el guardado diferido antes de leer el JSON. Se conservan los casos de integridad y contenido.

## Riesgos y medición pendiente

No prometer cero latencia, cero cortes ni inmunidad absoluta a cortes eléctricos. Una sola operación de SD/NVS puede superar los 125 ms del DMA; cooperar entre llamadas no elimina ese bloqueo. El guardado diferido deja una ventana breve de datos aún no persistidos (normalmente un fotograma, rescate 250 ms); comprobar comportamiento ante alimentación inestable en ensayo separado y autorizado. Un error de lectura durante la primera precarga se informa como fallo y no necesariamente reproduce un tono de respaldo.

El desfase del RTC medido en 0.5 es un asunto independiente y no se ajusta automáticamente en esta entrega. Los dos partidos originales sin fecha se conservan; no inventar sus horas. Faltan voces de anulación/penalti y futuras familias. Conservar originales y no subir los audios del juego al repositorio.

Primera prueba con el dueño: él inicia un partido y comprueba gol normal, grupos específicos mezclados con comodines, anulación durante comentario, gol ganador, animales y penalti. Medir marcador y altavoz conjuntamente (mediana, p95 y peor caso), cortes, memoria/fragmentación y primer gol tras reposo; repetir luego con el catálogo real de unas 800 voces y SD lenta. No se han generado partidos ni goles reales automáticamente para esta entrega.

Revisión de A/Cloud: **pendiente**. Numeración de pantalla en **0.6**, aún 0.x; pasar a 1.0 requiere autorización expresa del dueño.
