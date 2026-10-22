# Pantalla 0.4 · fechas e historial · equipo B · 22/10/2026

## Síntomas comunicados y alcance

El dueño confirma que ha jugado varios partidos con 0.3 y han funcionado bien. Tras dos partidos rápidos, aparecen sin fecha en el historial y la pestaña **Resumen del ranking** está vacía. Aclara que no se refiere al detalle individual.

Fechas recordadas por el dueño: el último, **22/10/2026 sobre las 13:30**; el anterior, **probablemente el 21/10/2026 por la tarde-noche**, sin hora concreta. Son datos aproximados, no marcas registradas por el reloj. No se han alterado estos dos partidos ni se han inventado horas. Para una recuperación posterior, primero identificar los registros reales, guardar una copia y distinguir fechas estimadas de fechas medidas.

Esta entrega modifica solo la aplicación nativa de la pantalla, en la rama del equipo B. No modifica la web, Supabase, la mini, el protocolo de goles ni los datos existentes. Se prepara 0.4 con registro de novedades, manteniendo el límite 0.x y conservando las versiones anteriores.

## Diagnóstico y evidencias

Confirmado en código y reproducido en PC:

- `saveMatchFile` guarda `startEpoch/endEpoch` como 0 si no hay hora válida. La lista presenta correctamente ese valor como «Sin fecha».
- Antes, `reloj::State::poll` asignaba `clockOk = rtcRead(c)` cada segundo. Un único fallo de lectura I2C hacía perder el estado de hora válida aunque ya se conociera una hora utilizable.
- `State::set` rechazaba una hora real de USB/Internet si no lograba escribir el RTC. Tampoco había una continuidad de la hora por software.
- `finishVersions` descartaba una hora de Internet ya obtenida si se había cancelado la búsqueda de versiones al iniciar el partido.
- `historial::buildDigest` solo aceptaba registros con fecha dentro de los últimos 7/30 días y exigía tener hora actual. Por tanto, dos registros con `fin=0` producían un resumen vacío. Los partidos rápidos sin participantes también dejaban sin contenido las tarjetas de estadísticas individuales.

Verificación física posterior: 0.4 está instalada por autorización del dueño. Los dos registros reales tienen fin=0 y el resumen ya los incluye (2 partidos / 27 goles). El reloj se ha puesto en hora y devuelve valida=1 rtc=1. Véase el [informe físico](VALIDACION_PLACA_0_4_20261022.md). Sigue sin probarse si originalmente faltó sincronización, se perdió la hora al desconectar o hubo un fallo I2C; no atribuirlo a una avería de hardware sin medirlo.

## Cambios

### Reloj y guardado

`s3_reloj.h` distingue hora válida (`clockOk`) de acceso correcto al RTC (`rtcOk`). Una hora conocida sigue avanzando mediante `millis()` ante un fallo de lectura. Se cubren el cambio de día/mes/año, bisiestos, el wrap de `millis()` y un timestamp anterior del bucle, sin saltar accidentalmente 49 días.

Una hora válida recibida por USB o Internet se acepta en RAM aunque falle la escritura física. En ese caso no se vuelve a sobrescribir con una fecha antigua del RTC: se mantiene temporal hasta otra puesta en hora. **Esto no promete conservarla al apagar:** después de reiniciar se necesita un RTC válido o volver a obtener la hora de una fuente real. Si nunca se conoce la hora, continúa sin inventar la fecha y el Inicio avisa «Pon la hora: conecta Wi-Fi».

Se conserva una hora de Internet ya obtenida aunque se haya cancelado la búsqueda de versiones. No se añaden conexiones Wi-Fi ni peticiones nuevas, ni se bloquea el partido esperando Internet. En `saveMatchFile`, si la hora se conoce después de empezar, se recupera el inicio mediante tiempo real transcurrido, incluidas pausas. No se utiliza solo `totalTimeMs`, que omite las pausas. Se rechaza también el 29 de febrero en años no bisiestos.

La zona horaria y la representación local del RTC siguen siendo las del proyecto (España peninsular). No se ha rediseñado la gestión de cambios de horario del hardware.

### Resumen

Se añade **TODOS**, junto a últimos 7 y 30 días. Sin elección explícita del usuario, si faltan fechas o no hay hora actual, se selecciona TODOS de forma visible. Los registros sin fecha se incluyen en esos totales y se indica su número; no se asignan silenciosamente a la semana o mes. Una selección manual de periodo se conserva y respeta.

Los filtros temporales exigen fecha conocida y hora actual válida. Si no pueden mostrar registros, explican el motivo y remiten a TODOS, en lugar de insinuar que no se ha jugado. Los partidos sin jugadores aportan partidos, goles y resultados; sus tarjetas individuales indican «Sin jugadores elegidos». No se inventan goleadores, victorias de personas ni jugadores.

No se amplía el formato de los JSON/índices ni se migran datos históricos. Se reutiliza la lista temporal del resumen. Coste adicional: unos campos pequeños de reloj y un recorrido del historial al construir la pestaña; no hay tareas, buffers grandes, sondeos adicionales ni trabajo nuevo por fotograma.

### Diagnóstico a petición

- `RELOJ?`: devuelve hora válida, estado de acceso al RTC y fecha actual, sin escribirlo.
- `HISTORIAL?`: devuelve número de partidos, cuántos carecen de fecha y hasta cinco registros recientes con id, fecha, modo y resultado, sin nombres de jugadores.

Son consultas de lectura para verificar la causa real tras instalar y preparar una recuperación segura de fechas. No borran ni fechan partidos, no guardan ajustes ni ponen el reloj. A diferencia de los comandos que pasan por `wakeScreen`, retornan antes de esa llamada.

## Pruebas

Nueva `hardware/test/s3_history_dates_test.cpp`, añadida a `check_copies.sh`:

- RTC intermitente y continuidad de hora; cambio de año y bisiesto; arranque sin RTC que no inventa fecha.
- Hora temporal, recuperación con un RTC antiguo, timestamp anterior y wrap de millis.
- Hora de Internet recibida tras cancelar la búsqueda de versiones.
- Guardado y relectura de JSON con fecha conocida; partido iniciado sin hora y sincronizado después, con una pausa de diez segundos.
- Resumen de dos partidos rápidos sin fecha ni jugadores: 2 partidos y 14 goles; originales comparados byte a byte sin cambios.
- Filtros de 7/30 días con datos mixtos: no incluyen registros sin fecha; TODOS sí.
- Consultas de diagnóstico sin modificar los registros.

La regresión nativa completa se repitió y pasó para el estado final, incluido el último caso de inicio recuperado. Las seis pruebas del registro de versiones también pasan. Durante la ampliación del test se corrigió una variable local que ocultaba la configuración del sketch; no se modificó el producto para ocultar una aserción.

## Estado de instalación y pendientes

La compilación final, las pruebas y la instalación física de 0.4 están completadas. El dueño autorizó expresamente instalar y poner la hora mediante «sí, hazlo». Se conservan los dos partidos; las fechas históricas no se han corregido automáticamente.

Se escribió solo app0 y se consultaron HELLO/RELOJ?/HISTORIAL?, se puso la hora actual de España y se revisaron capturas completas del historial y Resumen. No se generaron partidos extra ni se alteraron sus fechas. Recuperar las fechas antiguas requiere distinguir la información aproximada del dueño y las marcas reales, preservando copia de los originales.

Revisión y aceptación por el equipo A / Cloud pendientes. No fusionar esta entrega con main por iniciativa propia.

## Validación final y candidato listo

La regresión nativa final completa termina correctamente, incluyendo el caso de inicio recuperado con pausas. La compilación real de Waveshare 7C con Arduino-ESP32 3.3.12 también pasa: aplicación 1.870.064 bytes, variables globales 123.672 bytes (24 bytes más que 0.3). Los máximos que muestra Arduino no sustituyen la tabla real de particiones: la aplicación sigue cabiendo en su partición de 6 MB.

Binario local 0.4, build base 54: SHA256 `2b446096cda1f889cc0361a5a3088b706cb942dc477f92de7d072d3ee6933df9`, MD5 `462594b3030888bcd42636de852d9156`. Cabecera ESP32-S3 y presencia de versión verificadas. Instalado y verificado en COM6; véase el informe físico enlazado.

Se ha renderizado y revisado el resumen con los recursos gráficos reales, a 800×480. La [vista previa](capturas-placa/resumen-0.4-simulado.png) usa **dos partidos ficticios 5–2** para comprobar diseño, 2 partidos / 14 goles y aviso de fechas; no representa los resultados del dueño. El programa auxiliar de la primera vista previa se descartó porque renombrar y llamar al main de la prueba dejaba un retorno indefinido; la vista definitiva usa un escenario independiente. No fue un fallo del firmware.

Estado final: validación local y física aprobadas para el alcance medido; Resumen real comprobado con 0.4. No afirmar que se han recuperado fechas antiguas, comprobado la retención del reloj sin alimentación o probado un guardado nuevo en placa: esas comprobaciones siguen pendientes.
