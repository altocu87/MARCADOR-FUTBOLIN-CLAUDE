# Sonidos FC26 · grupo 1, gol comodín · equipo B · 22/10/2026

## Instrucciones del dueño (vigentes)

Los seis archivos exportados de FC26 en español pertenecen a «gol después de dejarse dominar», pero la placa no mide dominio. Por instrucción expresa se clasifican como **GOL COMODÍN**, elegibles también cuando exista un grupo específico de gol. No detectar dominio ni inferir un marcador a partir de los sufijos 0_0, 1_1, etc.: son las variantes exportadas, no condiciones de activación.

La selección específica no debe ser obligatoria. Cuando corresponda un contexto especial, deben poder salir tanto una variante de ese contexto como una comodín. No se ha fijado aún una proporción; no asumir que 50/50 está aprobado ni que concatenar todos los archivos da ese reparto entre grupos. El dueño anuncia cuatro grupos de gol; solo ha entregado este primero.

Última aclaración de sincronización: el audio puede esperar a la animación; **debe arrancar cuando el gol suba al marcador visible**. No se requiere reproducir al bajar físicamente el pulsador antes de que se confirme el gol. Mantener pulsación corta al soltar y larga para anular. Si se anula durante la frase, interrumpir el comentario y sustituirlo por sonido de gol anulado, sin dejarlo al final de una cola. Ese grupo de anulación aún no se ha recibido.

## Lo realizado físicamente (no confundir con funciones pendientes)

Los seis WAV están guardados en la SD de la pantalla COM6 con firmware **0.4**, sin grabar firmware ni cambiar MOMENTO, volumen o las opciones de audio. Cada recepción terminó en SONIDO_GUARDADO tras verificar tamaño y CRC32; SONIDOS? lista los seis con @s. Antes no había sonidos en esa lista; la pantalla se encontraba en Ranking → Resumen y no se inició ningún partido. Los originales de Descargas permanecen intactos.

**Todavía no se seleccionan automáticamente en los goles.** No se han activado cuatro dejando fuera dos ni simulado una mezcla ya operativa. La ampliación del selector y la interrupción/sincronización requieren trabajo de firmware posterior, con su nueva versión, pruebas y autorización de instalación. No se ha realizado una escucha del altavoz ni una medición de latencia física en esta entrega.

## Organización y catálogo

El firmware 0.4 exige `/sonidos/<código hexadecimal>.wav`, en una carpeta plana: `FsStore::path`, `find`, `listIn` y el protocolo trabajan así. No mover los WAV a subcarpetas por grupos sin adaptar el lector. La clasificación legible está en [el catálogo](sonidos/catalogo_fc26_gol_comodin_01.json), que relaciona nombre original, id estable, grupo, ruta SD, duración, hashes y verificación. **Ese JSON está en el repositorio/PC, no en la tarjeta**; el protocolo actual solo recibe WAV.

Id: `fc26/es/gol/comodin/after_inviting_pressure/NN`; código FNV-1a de 32 bits, compatible con `mfv3snd::soundCode`. Los códigos de este lote no colisionan entre sí y no existían en la placa antes de subirlos. La copia local de preparación conserva el árbol `sd/sonidos/` y su catálogo. No se incorporan los audios del juego a GitHub.

| Archivo exportado | Archivo dentro de /sonidos | Duración preparada | Bytes | CRC32 |
|---|---|---|---:|---|
| GOL_1_AFTER INVITING PRESSURE_0_0.wav | `fd0284b7.wav` | 9.408 s | 301,098 | fd61808d |
| GOL_1_AFTER INVITING PRESSURE_1_1.wav | `fc028324.wav` | 9.966 s | 318,972 | 13c0ef99 |
| GOL_1_AFTER INVITING PRESSURE_2_2.wav | `ff0287dd.wav` | 12.358 s | 395,500 | 7230b7d8 |
| GOL_1_AFTER INVITING PRESSURE_3_3.wav | `fe02864a.wav` | 9.258 s | 296,312 | 4ada3464 |
| GOL_1_AFTER INVITING PRESSURE_4_4.wav | `f9027e6b.wav` | 7.394 s | 236,636 | 33805316 |
| GOL_1_AFTER INVITING PRESSURE_5_5.wav | `f8027cd8.wav` | 9.041 s | 289,360 | 9ca9be24 |

Total: **5.515.986 bytes originales → 1.837.878 bytes preparados**, reducción aproximada del 66,7 %. PCM WAV RIFF de cabecera de 44 bytes, mono, 16 bits, 16 kHz. El parser real `mfv3snd::parseWav` de 0.4 acepta los seis en una validación C++ de PC; todos están por debajo del límite de subida de 1.048.576 bytes. El original de la variante 2 supera ese límite antes de convertir.

Conversión con FFmpeg ya instalado: remuestreo filtrado a 16 kHz, metadatos retirados, sin compresión en la placa. Se detectó el primer bloque de 5 ms con RMS superior a −55 dBFS y se conservaron 20 ms previos; los recortes iniciales fueron de 0–10 ms. No había un silencio inicial largo que explicara grandes retardos. Se aplicó −1 dB antes del remuestreo para evitar nuevos picos saturados: en la primera preparación aparecieron 1–2 muestras a escala completa en tres variantes; la revisión final tiene cero muestras saturadas. Sin normalizar al alza ni cambiar el volumen de la placa. Duraciones 7,39–12,36 s: próximos goles/anulaciones no deben acumular frases antiguas.

## Incidencias de subida y alcance de verificación

El primer intento terminó con tiempo de espera del receptor durante el sexto archivo; cinco ya estaban confirmados. Se actualizaron únicamente esos cinco archivos propios con la revisión final de audio y se completó el sexto. El envío final terminó correctamente para los seis, unos 5,1–8,5 s por archivo. Se usaron bloques de 192 bytes codificados en base64, ACK exacto por bloque, tamaño esperado y CRC32 al finalizar. El receptor escribe temporal y renombra al confirmar. No se borraron sonidos ajenos ni se tocó FFat. La protección de reemplazo del auxiliar solo permite códigos de este lote previamente confirmados por el propio registro.

Posible causa del timeout a investigar: `loop()` toma now antes de leer USB; `handleSoundLine` usa un millis más reciente para lastData, pero `sndCmd.tick(now)` recibe el valor antiguo. `Commands::tick` resta sin signo y puede interpretar ese desfase como timeout. Es una ruta comprobable en código y compatible con el fallo observado; no hay traza que pruebe que fuera el desencadenante exacto. No se modificó firmware para ocultarlo. El auxiliar admite hasta tres reintentos por archivo únicamente ante ese error; en la transferencia final no hicieron falta. SONIDOS? confirma presencia en SD; no hay lectura remota completa de los WAV para un segundo hash del fichero ya almacenado. No equiparar el CRC de recepción a una comprobación acústica.

## Trabajo de firmware pendiente para equipo A / Cloud

- `mfv3_sound.h`: Choice.codes[4], parser hasta cuatro, item[64]; .ino: have[4], persistencia text[700]. Ampliar de forma coherente, validar límites y no truncar silenciosamente listas de seis o más. Hay copias compartidas y un cliente web con el modelo actual: mantener compatibilidad, no tocar solo una constante. Revisar también el listado máximo de 48 sonidos antes de cargar los cuatro grupos completos.
- Definir catálogo/pools para que cualquier grupo contextual de gol pueda mezclar comodines; elegir una sola frase válida por gol, sin atribuir condiciones que el motor no mide. Reservar decisión sobre porcentajes hasta acordarla. No sustituir voces de descanso/victoria por frases de gol accidentalmente.
- `playEvents` se llama al final de `afterChange`; Player.start abre SD y lee cabecera, pero las muestras se alimentan en Player.pump desde loop. El marcador se dibuja más tarde mediante render/uiport::step. Medir el desfase real y coordinar el primer audio con la actualización visible, sin bloquear LVGL ni esperar que termine la frase anterior.
- `Player::stop` cierra archivo y vacía la cola de software, pero no vacía explícitamente el DMA I2S (8×250 tramas = 125 ms a 16 kHz). Al anular, sustituir la frase y revisar la cola física para no dejar una cola audible del comentario anulado. No prometer latencia cero a partir de esa capacidad de buffer: medirla.
- Preferir audio ya convertido y, si las medidas lo justifican, preparar lecturas fuera del instante del gol con presupuesto explícito de RAM. No cargar todos los futuros audios en RAM sin medir; la SD conserva los ficheros y el código actual reproduce por bloques.

## Pruebas de aceptación siguientes (con permiso para partido)

Comprobar los seis archivos en selección; contexto con específico y comodín, ausencia de contexto y de archivos, compatibilidad de configuración anterior, lista demasiado grande rechazada, persistencia y ausencia de repeticiones excesivas. Verificar que una anulación corta el audio y no añade gol ni deja reproducción antigua en cola. Si falta su frase, acordar un sonido breve de respaldo.

En placa: grabación conjunta de pantalla y altavoz con referencia temporal; medir instante del marcador visible frente al comienzo audible, varios goles tras reposo y con audio previo, y anulación durante la frase. Separar tiempo humano de pulsación, antirrebote/radio, apertura de SD y DMA. Registrar mediana, percentil 95 y peor caso; el dueño pide coincidencia perceptiva, no ha aprobado una cifra de milisegundos. No iniciar partidos para medir sin autorización. La escucha de las frases y su adecuación semántica siguen pendientes; la categoría se basa en la instrucción del dueño y los nombres exportados, no en una transcripción revisada.

Entrega en rama del equipo B, sin fusión con main. No hay nueva versión nativa porque solo se han transferido datos y documentado requisitos; la pantalla sigue en 0.4.
