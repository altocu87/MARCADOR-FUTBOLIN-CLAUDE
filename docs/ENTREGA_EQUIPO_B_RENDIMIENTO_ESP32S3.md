# Entrega del equipo B al equipo A (Cloud)

> **19/10/2026 · Actualización de estado:** el propietario ha autorizado la integración completa en `main`. La revisión de Cloud y las pruebas físicas siguen pendientes. El estado actual prevalece sobre las notas históricas de esta entrega y se mantiene en [ESTADO.md](ESTADO.md).

Fecha: 19/10/2026. Estado: **pendiente de revisión y validación física por el equipo A**.
Rama: `equipo-b/rendimiento-esp32s3`.
**Ampliación posterior autorizada:** [cinco mejoras ligeras, costes, pruebas y captura](AMPLIACION_EQUIPO_B_MEJORAS_LIGERAS_ESP32S3.md). Este documento describe la primera entrega; consultar también la ampliación antes de aprobar la rama.

Código y pruebas de la primera entrega: commit `0f4323b872d889b67aee92c35b035947b8b4e117`.
Base: `origin/main`, commit `cc064e00612fe1f6099b680db9f8d3990e99c418`.
No se autoriza la fusión automática: el usuario quiere revisar esta entrega antes de incorporar a `main`.

## Explicación para el propietario

- **Partido Loco:** dejamos de ordenar a la pantalla que rehaga el mismo estilo de letra en cada vuelta. Esto elimina trabajo repetido mientras se juega.
- **Ranking:** si no han cambiado los partidos, jugadores, reglas o temporada, reutilizamos el resultado. Se evita volver a calcular y reservar memoria al entrar otra vez.
- **Internet:** buscar redes y consultar versiones/hora deja de ocupar durante toda la espera el trabajo que atiende la pantalla. Las conexiones se coordinan para no competir entre sí ni quitar al mando su canal durante un partido.
- **Memoria y sincronización:** comprobamos reservas, liberamos temporales al terminar y conservamos la cola para reintentar ante los errores cubiertos. La pantalla recibe los datos cuando la tarea de descarga ha terminado de modificarlos.
- **Imágenes:** rechazamos cabeceras LZ4 incoherentes y, si falta espacio para una imagen, intentamos liberar imágenes de pantallas anteriores.

Son mejoras comprobadas en código y simulación. Todavía no se ha medido cuánto se notan en la pantalla física con este firmware. No se promete una cifra de FPS ni ausencia absoluta de fallos.

## Alcance y límites de la entrega

Se modifica la aplicación **nativa LVGL** de `hardware/esp32s3_pantalla7/`, sus pruebas y documentación. No se convierte en la app web ni se incorporan sus efectos o funciones deliberadamente simplificados en la placa. No cambian el motor de goles, reglas, puntuaciones, diseños generados, particiones ni formato de datos compartido.

Se ha trabajado en un checkout separado `MARCADOR-FUTBOLIN-EQUIPO-B`. No se ha grabado firmware, iniciado partidos reales, cambiado ajustes en la placa ni alterado Supabase. Los partidos y conexiones de las pruebas son simulados. Las herramientas y bibliotecas ya estaban instaladas; se han reutilizado.

## Cambios para revisar

| Archivos / funciones | Cambio y motivo | Riesgo o atención del revisor |
|---|---|---|
| `ui_partido.h`: `updateChaos` | Cambiar la fuente del tiempo del hándicap solo al aparecer o pasar entre duración y texto | Verificar hándicaps cronometrados, sin duración y reaparición tras ocultarse |
| `s3_progresion.h`: `refresh`, `refreshSeason`, `compute`; `s3_partidos.h`: `revision` | Caché exacta de cálculos; invalida por altas/bajas, cambio de identidad, jugadores, reglas y mes. Reserva del orden por capacidad y limpieza si falla | Cualquier futura mutación directa de `partidos::rows` debe incrementar `revision` |
| `ui_historial.h` | Retirar reservas redundantes del orden global y comprobar tres reservas para los detalles | Si falta memoria, mostrar mensaje; no desreferenciar un puntero nulo |
| `s3_memory.h` | Ampliación geométrica que conserva puntero y capacidad cuando falla `realloc` | El mecanismo de fallo es inyectable en pruebas; no cambia el asignador del firmware |
| `s3_nube.h`: `sync`, `runJob`, `apply`, `startJob` | Copia privada del estado de sesión, publicación atómica del trabajo y resultado; el consumidor no lee mientras el productor amplía o recorre el array | Se acumula lo recibido hasta terminar la descarga: puede aumentar el pico de fotos respecto a aplicar durante ella |
| `s3_nube.h`: `freeUploads`, `takeRow`, `pushMatch` | Liberar temporales al finalizar; propagar fallos de reserva/lectura/descodificación; no confirmar una descarga fallida ni confundir falta de memoria con partido inexistente | No es una conversión de todo el almacenamiento a transacciones; véanse límites más abajo |
| `s3_nube.h`: `trimPending`, `recoverPending` | Retirar solo líneas de la copia enviada; conservar cambios posteriores incluso del mismo jugador; comprobar escritura y conservar respaldo durante sustitución | Reenvíos posibles e idempotentes; dos renombrados en FAT no garantizan resistencia a toda clase de corte eléctrico |
| `ui_assets.h`: `loadLz4`, `allocImage`, `addCached` | Validar tamaño/formato/stride antes de reservar; reintentar asignación expulsando solo imágenes no activas; comprobar creación de descriptores | Se conserva el límite blando de 8 MiB y la protección de imágenes activas; no se ha reducido su calidad |
| `s3_net.h`: `startScan`, `pollScan`; `s3_ajustes.h`: versiones | Escaneo asíncrono; HTTPS y espera NTP en tarea del núcleo 0; LVGL y escritura del reloj permanecen en `loop()` | Tarea de versiones con pila interna de 8192 bytes: medir margen y fragmentación en placa |
| `esp32s3_pantalla7.ino`, `ui_cuenta.h` | Exclusión entre consulta, escaneo, nube e instalación; cancelar consulta de versiones al comenzar partido; esperar si nube/escaneo están ocupados | Revisar mensajes y navegación al pulsar JUGAR durante una conexión |

### Contrato entre tareas

`loop()` prepara copias → publica `jobReady` → tarea descarga/sube sobre sus copias → publica `jobDone` → `loop()` aplica como máximo cuatro cambios por vuelta en pantallas permitidas → libera temporales.

La tarea no cambia `saved` ni mensajes de LVGL durante la recepción. Las barreras atómicas cubren la entrega de estos datos. Esto no equivale a una auditoría completa de todas las variables compartidas existentes en el firmware.

### Flujo de imágenes conservado

Generador de recursos → LZ4 en FFat → lectura del bloque comprimido → validación y descompresión → caché de píxeles → descriptor estable de LVGL → buffers RGB de pantalla.

Las fotos mantienen su preparación por tamaño y esquinas. Se preserva la expulsión por antigüedad/pantalla y las precargas existentes. La lectura, descompresión inicial, mezcla de transparencias y preparación de fotos siguen siendo síncronas cuando se necesitan.

## Evidencia y pruebas reproducibles

Ejecutar desde la raíz (salvo indicación). Todo usa datos ficticios o fixtures; no consulta servicios de producción.

| Comprobación | Resultado |
|---|---|
| `npm test` | 304 pruebas, 24 archivos, correctas |
| `npm run build` | Correcta; aviso existente de paquete web de más de 500 kB |
| `sh hardware/test/check_copies.sh` | Correcta: motor/protocolo/audio, simulaciones y compatibilidad con prototipos Arduino |
| Simulación LVGL en placas 4, 1 y 2 | Correcta: Waveshare 7C, Elecrow y Waveshare 7 |
| `hardware/test/nube_memory_test.cpp` (incluida en el script) | Fallos de reserva, cambios posteriores en cola, fallo al crear sustituto, recuperación de respaldo, cursor conservado, foto inválida y publicación con dos hilos reales del PC |
| `hardware/test/s3_performance_test.cpp` (incluida en el script) | LZ4 válido/truncado/cabecera imposible; reutilización y protección de caché; 100 actualizaciones idénticas sin eventos de cambio de estilo; cambios de formato y reaparición; escaneo simulado de 2 s con ejecución continuada de `loop()`; descarte de consulta cancelada |
| `hardware/test/progresion_test.cpp` | Coincidencia con la app: 154 partidos y 17 jugadores; reaperturas sin nuevas reservas; invalidación por jugador, reglas, mes, alta/baja de partido; recuperación tras fallo de cálculo |
| Recursos locales existentes | 131 capas LZ4 leídas y descomprimidas con el cargador nuevo; RGB565 y RGB565A8 probados; no se han regenerado |
| Compilación real ESP32-S3 | Correcta sobre el código final: programa 1.860.243 B (11%); variables globales 119.192 B (36%) |
| `git diff --check` | Sin errores de espacios |

Compilación real solicitada con las herramientas ya instaladas: Arduino ESP32 3.3.12 y LVGL 9.5.0. FQBN: `esp32:esp32:esp32s3:PSRAM=opi,FlashSize=32M,PartitionScheme=custom,CDCOnBoot=cdc`. Orden: `arduino-cli compile --fqbn <FQBN> --build-path build/equipo-b-firmware hardware/esp32s3_pantalla7`. Compilar no instala nada en la placa. Estos tamaños son estáticos: no equivalen a la memoria libre mientras se ejecutan LVGL, Wi-Fi y las tareas.

Para comprobar recursos ya generados, definir `MFV3_ASSETS_VERIFY` con su carpeta FAT (ruta absoluta o relativa a `hardware/`) y ejecutar `sh hardware/test/s3_build.sh 4 test/s3_performance_test.cpp`. Solo se leen.

Los registros locales están en `build/` (no versionados). Persisten advertencias previas de conversiones en `hcIcon`, ramas antiguas del `switch` y, en el compilador ESP32, operaciones sobre `volatile`. No se han mezclado arreglos ajenos a esta entrega.

Comprobaciones remotas del commit de código: [Pruebas #39281132705](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39281132705) y [Firmware #39281132677](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39281132677). Consultar allí el estado final; este documento no sustituye los resultados de Actions. El workflow de firmware solo publica en `main` o con su opción manual; Pages filtra `main`. Subir esta rama no publica una actualización para las placas.

## Confirmado, probable y pendiente

**Confirmado en pruebas:** se elimina la invalidación repetida de esa etiqueta; las reaperturas del ranking no asignan de nuevo sus tablas cuando no cambia la clave; los fallos simulados no borran la cola; el consumidor espera la publicación de la descarga; los casos LZ4 inválidos probados no entran en caché. El cálculo deportivo conserva la referencia de la app.

**Probable, no cuantificado en hardware:** menos pausas al volver al ranking, menos repintados en Partido Loco, más respuesta al consultar versiones o buscar Wi-Fi, y menor memoria temporal retenida entre sincronizaciones.

**Pendiente de medir o resolver en otra entrega:**

- Pico de PSRAM durante una descarga grande con fotos. La entrega diferida es más simple y evita la carrera, pero no es una cola acotada. Si el lote no cabe, falla y conserva el cursor; puede necesitar una futura recepción por lotes con confirmación.
- Margen de RAM interna/pila durante TLS y fragmentación después de repetir consultas. La tarea nueva puede no crearse; se informa del fallo y se libera la conexión.
- Los hooks que aplican jugadores/partidos siguen siendo `void`: ciertos errores de FFat/SD posteriores a la recepción no se comunican a la sincronización. `saveState`, índices y otros guardados no tienen todos un protocolo transaccional. Esta rama no asegura ausencia de pérdidas ante cualquier fallo de almacenamiento o corte de energía.
- Preparación de subidas con fotos y consultas de historial todavía pueden bloquear `loop()`. Tampoco se vuelve asíncrona la instalación de firmware.
- Caché de imágenes blanda, copias de fotos/BMP/base64, pila PSRAM de la tarea nube en configuraciones que ya la usaban y otros accesos compartidos existentes requieren revisión física específica.
- El ahorro real en milisegundos/FPS y la respuesta de los pulsadores no se deducen de los tiempos del simulador PC.

## Plan de validación física antes de fusionar

1. El propietario aprueba expresamente grabar y ejecutar el recorrido; conservar firmware y copia de datos originales. Verificar de nuevo el puerto USB.
2. Registrar con la versión base y esta rama: memoria interna libre/mínima/bloque mayor, PSRAM libre/bloque mayor, caché y causa de reinicio. La lectura previa de la conversación, con firmware original, fue: interna libre 71.632 B, bloque mayor 31.732 B, mínimo 67.784 B; PSRAM libre 5.823.128 B, bloque mayor 5.767.156 B; caché indicada 7.123 KB. Es una muestra aislada, no un resultado de esta rama.
3. Recorrer inicio → selección con fotos → partido/goles/pausa → Partido Loco → ranking/historial → ajustes → reposo. Repetir al menos 20 veces, comparando primera apertura y siguientes. No mezclar datos reales con partidos de prueba.
4. Medir latencia toque/pulsador→cambio visible (mediana, p95, máximo), tiempo de cada transición y máximo intervalo de `loop()`; separar tiempo de lectura, descompresión y dibujo. Primero decidir instrumentación y frecuencia para no distorsionar la medida.
5. Repetir con internet lento/ausente, escaneo abierto, consulta cancelada al jugar y sincronización con fotos. Confirmar que el mando conserva su canal al comenzar y que las acciones bloqueadas muestran un mensaje claro.
6. Comprobar una modificación del mismo jugador durante una sincronización, desconexión, SD llena/ausente y fallo de asignación controlado sobre datos de prueba. No retirar una SD con datos reales sin copia y autorización.
7. Aceptar solo con resultados equivalentes a la app, sin reinicios ni corrupción en el recorrido y con mejora medida. Si el nuevo pico/pila es inaceptable, separar primero los cambios de ranking/repintado y rediseñar la descarga por lotes.

## Revisión y devolución al equipo B

El equipo A debe leer el diff contra la base indicada, ejecutar las pruebas y valorar los riesgos anteriores. Dejar aquí su dictamen, incidencias y resultados físicos. **No marcar aprobado por el mero hecho de compilar.**

- Revisión equipo A: pendiente.
- Prueba física: pendiente de autorización y ejecución.
- Decisión de fusión: pendiente del propietario/equipo A.

Para comparar: `git diff cc064e00612fe1f6099b680db9f8d3990e99c418...equipo-b/rendimiento-esp32s3`.
Para descartar antes de fusionar basta con seguir usando `main`: la placa actual no ha sido modificada. Si se instala una versión experimental, su vuelta atrás requiere grabar el binario anterior con permiso; no ejecutar limpiezas de datos ni migraciones por esta rama.

## Contexto independiente: Supabase

La configuración de acceso se trató antes de esta entrega. El reintento de «Base de datos» #39227596449 llegó a enlazar el proyecto pero falló al conectar con la base de datos por IPv6; la función no se desplegó en ese intento. La migración ya había concluido en el intento anterior. Se sospecha un permiso de lectura del pooler ausente en el token nuevo; debe verificarse por separado. Esta rama no modifica permisos, secretos, migraciones ni funciones, y sus pruebas no certifican ese despliegue.

## Continuación: acceso por QR

Ver [entrega específica del acceso QR](ENTREGA_EQUIPO_B_ACCESO_QR.md), con correcciones del móvil, recuperación de publicación y límites pendientes de prueba real.

## Continuación: actualizaciones y versión 0.1

Ver [entrega de Actualizaciones](ACTUALIZACIONES_PANTALLA.md): apartado propio en Ajustes, historial y notas en lenguaje llano, confirmación, barra sin recrear objetos y regla obligatoria de subir versión y registrar cambios antes de publicar. La pantalla permanece en 0.x hasta autorización expresa del propietario. Incluye capturas del simulador, pruebas y límites pendientes de validación física; Cloud debe revisar y valorar esta entrega por separado.

## Validación física posterior · 20/10/2026

Integración e instalación autorizadas y realizadas. Se comparan QR original y optimizada 0.1; se reproduce y corrige el bloqueo de Ajustes. Veinte vueltas de menús mantienen memoria y respuesta. Los tiempos de dibujo comparados son similares y hay un coste de unos 6–7 KiB internos. Ver [informe físico](VALIDACION_PLACA_20261020.md) antes de interpretar las hipótesis de rendimiento como mejoras ya medidas en partido. Revisión de Cloud pendiente.
