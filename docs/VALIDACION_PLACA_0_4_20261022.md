# Validación física de pantalla 0.4 · equipo B · 22/10/2026

## Alcance y autorización

El dueño autorizó instalar la 0.4, poner el reloj en hora y comprobar los dos registros y el Resumen. Se ha completado ese alcance, sin iniciar partidos, asignar fechas históricas ni cambiar resultados. La mini no interviene. No se ha fusionado con main ni publicado un firmware OTA oficial.

## Firmware y conservación

Código instalado: `cb65865f58cd644d564515c12027ef107ab5083c`, rama `equipo-b/rendimiento-esp32s3`. Pantalla Waveshare 7C ESP32-S3 identificada en COM6. HELLO posterior: `waveshare-7c 0.4`.

Aplicación local de 1.870.064 bytes, build base 54; SHA256 `2b446096cda1f889cc0361a5a3088b706cb942dc477f92de7d072d3ee6933df9`. Antes de escribir se guardaron los primeros 64 KB en una copia privada y se verificaron tabla y selección OTA (secuencias 3 y 2, estado VALID y CRC correctos). Esa copia no es un respaldo completo del historial.

Escritura exclusivamente en app0, 0x10000, partición de 6 MB, con esptool 5.3.1 sin stub. Duración de escritura 109,7 s; verificación de hash correcta y reinicio final. No se escribieron NVS, otadata, app1, FFat ni SD. Queda disponible localmente el binario 0.3 para recuperación. No se publican copias privadas que puedan contener credenciales.

## Reloj real

Antes de ponerlo en hora, RELOJ? devolvió `valida=1 rtc=1 fecha=2026-10-22 15:41:27` mientras el ordenador estaba aproximadamente a las 15:42:30 de España. Esa lectura actual no prueba por qué faltó hora en los partidos anteriores.

A las 13:42 UTC se envió `HORA 2026-10-22 15:42:32`, obtenida inmediatamente antes desde UTC del PC y convertida a Europe/Madrid (UTC+2 ese día); se contrastó la fuente UTC con el reloj de la sesión. Respuesta HORA_OK. Lecturas posteriores: 15:42:33, 15:42:37 y, tras cerrar y volver a abrir el puerto, 15:44:48, 15:45:24 y 15:46:17, siempre valida=1 rtc=1. La hora avanza y el acceso al RTC funciona en esta sesión. No se ha ensayado apagar/desconectar ni la pila; no afirmar retención comprobada sin alimentación.

## Historial y Resumen reales

HISTORIAL? antes y después de poner la hora y de navegar: exactamente **2 partidos, 2 sin fecha**; mismos identificadores, modo rápido y resultados **6–3 y 12–6**, ambos con fin=0. No se alteraron esos campos. La captura del historial muestra ambos originales como «Sin fecha».

La pestaña Resumen real selecciona TODOS, indica «Incluye 2 sin fecha» y muestra **2 partidos / 27 goles**, además de las tarjetas de jugadores y resultados. Capturas locales RGB565 de 800×480 completas (768.000 bytes) y CRC32 comprobado: historial `12ac97ef`, resumen `17aaffce`. Se revisaron visualmente sus PNG. Se conservan en el espacio privado de trabajo y no se incorporan al repositorio las fotos y estadísticas personales. La imagen simulada de la entrega de código sigue identificada como ficticia.

Una primera captura con el lector PowerShell anterior llegó incompleta (766.789/768.000 bytes) y se rechazó. Un lector auxiliar C# con lectura binaria directa y un buffer mayor obtuvo ambas capturas completas con CRC correcto; no se modificó el firmware para ello. La primera compilación de ese auxiliar necesitó añadir referencias del sistema, sin instalar herramientas. Esto no demuestra resuelto el problema conocido de capturas USB: `sendCapture` sigue sin comprobar escrituras parciales y permanece pendiente de revisión. No confundir fallos de la captura con pérdida de datos del historial.

Memoria consultada durante estas vistas: interna libre 67.144–67.188 bytes, mayor bloque 31.732, mínimo desde arranque 54.476; PSRAM libre aproximadamente 5,83 MB decimales (5.825.528–5.825.548 bytes), mayor bloque 5.767.156; caché declarada por firmware 7.123 KB. Es una lectura puntual, no una prueba prolongada de fragmentación ni rendimiento durante partido.

## Validación y límites para Cloud / equipo A

Además de la regresión local y compilación Arduino documentadas en la entrega, GitHub completó correctamente para cb65865 las ejecuciones [Pruebas](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39787855183) y [Firmware](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39787855534).

Confirmado: instalación 0.4, hora correcta durante la sesión, dos fechas originales cero, resultados conservados y Resumen real con datos. Probable causa de fechas: falta de hora válida al guardar; los fallos de código asociados están cubiertos por pruebas. No se ha identificado físicamente el desencadenante original ni una avería del RTC.

Pendiente: aceptar la entrega en Cloud; jugar un próximo partido autorizado y comprobar su fecha nueva; comprobar retención tras desconectar; preparar recuperación con respaldo de JSON y fechas expresamente aproximadas si el dueño la solicita. El dueño recuerda el último partido el 22/10 sobre las 13:30 y el otro probablemente el 21 por la tarde-noche. No se deduce una correspondencia inequívoca con los UUID a partir de un índice sin fecha ni se inventa la hora que falta. No hay migración automática de esos originales en 0.4.

Se deja abierta la pestaña Resumen. No hay monitor reteniendo COM6.
