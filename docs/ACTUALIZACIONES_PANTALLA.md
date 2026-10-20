# Actualizaciones de la pantalla ESP32-S3 · versión 0.1

> **19/10/2026 · Actualización de estado:** el propietario ha autorizado la integración completa en `main`. La revisión de Cloud y las pruebas físicas siguen pendientes. El estado actual prevalece sobre las notas históricas de esta entrega y se mantiene en [ESTADO.md](ESTADO.md).

Decisión del dueño del 19/10/2026: la numeración visible de la aplicación **nativa** empieza en **0.1** y continúa en **0.x**. Pasar a 1.0 requiere que él lo autorice expresamente. La app web, el protocolo y el motor tienen numeraciones independientes.

## Para quien usa el futbolín

En **Ajustes → Actualizaciones**:

- **Versiones:** muestra la versión instalada de la pantalla, si hay otra disponible y el estado del mando. Buscar consulta Internet; no instala nada.
- **Novedades:** explica los cambios de cada versión registrada. Se desliza con el dedo para leer y se pasa de versión con Anterior/Siguiente. Funciona sin conexión para el historial incluido en el programa instalado. Una nueva versión disponible muestra sus notas antes de instalarla.
- **Cómo actualizar:** explica los pasos. Se necesita corriente y Wi-Fi; el mando debe estar cerca si también se actualiza.
- **Continuar → Instalar ahora:** permite revisar las instrucciones y confirmar. Ahora no vuelve atrás. La instalación no se inicia durante un partido ni mientras otro trabajo utiliza la red.
- Durante la instalación se muestra qué equipo y qué paso se están realizando. El porcentaje corresponde a ese paso; puede volver a cero al empezar la descarga o el envío de otro equipo. No es una estimación de tiempo.
- La pantalla comprueba el archivo antes de reiniciarse. El mando debe confirmar su nueva versión; si no lo hace se explica el fallo. Cancelar mientras se espera al mando no se presenta como una instalación terminada.

La actualización de programa no borra partidos para instalarse. No apagues los equipos mientras trabajan. La comprobación técnica no sustituye a la prueba final en hardware real ni garantiza recuperación ante cualquier corte eléctrico.

## Registro único y regla de publicación

Editar `hardware/esp32s3_pantalla7/releases.json` antes de **cada publicación**:

1. Subir `version` dentro de 0.x; añadir la nueva entrada al principio con esa misma versión, fecha, título y cambios en lenguaje llano. No editar las notas ya publicadas.
2. Mantener todas las entradas anteriores. No inventar versiones para reconstruir un pasado no registrado: 0.1 es el punto de partida acordado.
3. Mantener `majorOneAuthorized: false` hasta autorización expresa del dueño. No deducirla de que una función esté terminada.
4. Ejecutar `node scripts/native-release.mjs generate`, `node scripts/native-release.mjs check` y las pruebas.
5. Pedir la revisión de Cloud y la autorización de publicación/grabación que corresponda. Un commit en la rama no es una publicación ni una instalación.

El archivo generado `s3_release.h` alimenta la versión del firmware y todo su historial local. La herramienta genera también `version` y `notes` en `ota.json`, junto al tamaño y MD5 del binario real. El instalador web toma la misma versión para las variantes de pantalla nativa. El contador `MFV3_BUILD` sigue distinguiendo compilaciones a efectos de OTA; no es la versión que lee el usuario.

GitHub comprueba que el registro y el código generado coincidan. Antes de publicar descarga el manifiesto de la release anterior, exige una versión mayor y conserva sus notas en el historial. Las publicaciones se serializan. Se permite reintentar la subida del mismo artefacto si coinciden exactamente versión, notas, build, tamaño y MD5; un binario nuevo exige otra versión. La primera publicación 0.1 admite el manifiesto antiguo que solo llevaba build/tamaño/MD5. Un fallo al consultar la release anterior detiene la publicación; no se interpreta como permiso para sobrescribirla.

## Implementación y coste

- `s3_update_ui.h`: interfaz nativa LVGL, confirmación, historial y barra persistente; reutiliza fuentes Inter ya incluidas en FFat. No añade imágenes ni requiere regenerar recursos.
- `s3_ajustes.h`: publicación de metadatos desde la tarea de consulta, estados y comprobación del saludo del mando. El plazo para autorizar el mando empieza después de descargar; el aviso de reinicio permanece 1,5 segundos después de terminar, aunque la descarga haya sido lenta. Durante la descarga se refresca el dibujo sin ejecutar temporizadores de entrada que puedan cambiar de pantalla.
- `s3_update_info.h`: dos buffers de 1.040 bytes para intercambiar versión/notas entre consulta e interfaz (2.080 bytes estáticos en total). Texto de la barra fijo de 40 bytes. El historial completo queda en constantes del programa; solo se crea la vista de una entrada cada vez.
- La barra y su texto se reutilizan; no se recrean objetos por porcentaje ni se redibuja la pantalla completa por cada paquete de radio. La prueba recorre 101 porcentajes y 1.000 repeticiones sin aumentar objetos.
- `s3_net.h`: consulta metadatos de hasta 8 KiB con longitud conocida. Cada entrada de notas admite menos de 1.024 bytes UTF-8, validado antes de compilar/publicar. Una publicación anterior sin notas se identifica como tal.

## Límites y revisión del equipo A (Cloud)

Entrega en `equipo-b/rendimiento-esp32s3`, pendiente de revisión. No se ha fusionado con main, publicado este firmware ni grabado la placa.

El historial registra versiones del producto, no es un registro técnico de cada intento de instalación. Antes de actualizar se muestran las notas de la próxima versión publicada; las entradas intermedias que no existían en el firmware instalado estarán disponibles al instalar el nuevo firmware, que contiene todo el historial.

La numeración 0.x corresponde a la pantalla. Los firmwares antiguos del mando no ofrecen notas ni una versión legible: se indica honestamente esa ausencia, sin asignarles una versión inventada.

La descarga/escritura del programa continúa siendo síncrona; esta entrega mantiene el progreso visible con refresco directo de LVGL, pero no convierte OTA en una tarea paralela. Falta verificar en la placa real la imagen durante escritura de flash, los tiempos, los botones del mando y la recuperación ante fallo de red. La simulación usa red, radio, flash y reinicio ficticios.

## Pruebas y capturas

- Pruebas de registro, orden, límites, autorización de 1.x, conservación de notas, manifiesto y MD5 del binario: `tests/native-release.test.mjs`.
- Pruebas nativas con LVGL: `hardware/test/s3_updates_test.cpp` y recorrido de pantalla/mando en `s3_sim_test.cpp`. Incluyen confirmación, sin Wi-Fi, bloqueo en partido, progreso estable, cancelación y error sin reinicio.
- Las capturas adjuntas se generan en el PC con LVGL y las fuentes reales; no son fotografías de la placa. La versión 0.2 usada para simular una disponibilidad no es una versión publicada.

### Comprobaciones de esta entrega (19/10/2026)

- 310 pruebas de la aplicación y herramientas pasan (25 archivos), incluidas las seis del registro de versiones.
- Construcción de la app web correcta; persiste su aviso previo de paquete grande.
- Suite completa de hardware correcta: núcleo, motor, radio, OTA, sonido, jugadores, nube, progresión, simuladores y declaraciones automáticas de Arduino. La pantalla se comprueba en las variantes 4, 1 y 2.
- Tras los últimos ajustes se repiten el recorrido de la variante 4 y la prueba específica con LVGL/fuentes reales. Se verifica el gesto de deslizar, 101 porcentajes y 1.000 repeticiones sin nuevos objetos, límites de las notas, errores sin reinicio, cancelación y saludo del mando. Una descarga ficticia larga verifica que los plazos comiencen al terminarla.
- Siete bloques de consola del flujo de firmware pasan la comprobación de sintaxis de Bash. No se ha ejecutado una publicación.
- Compilación final real para ESP32-S3 correcta con ESP32 3.3.12 y LVGL 9.5.0. Programa: **1.865.383 bytes**; variables globales: **123.648 bytes**. Frente a la ampliación anterior del equipo B: +4.708 bytes de programa y +2.128 bytes de globales. FQBN: `esp32:esp32:esp32s3:PSRAM=opi,FlashSize=32M,PartitionScheme=custom,CDCOnBoot=cdc`. Solo compilación: no se ha grabado ni reiniciado ningún equipo.

Persisten avisos previos sobre conversiones en `hcIcon`, casos del motor y el enlazador. No se interpreta la cifra de variables globales del compilador como RAM libre con Wi-Fi, fotos y pantalla funcionando.

### Capturas del simulador

[Historial](capturas-placa/actualizaciones-historial-equipo-b.png) · [Confirmación](capturas-placa/actualizaciones-confirmar-equipo-b.png) · [Progreso](capturas-placa/actualizaciones-progreso-equipo-b.png) · [Error explicado](capturas-placa/actualizaciones-error-equipo-b.png).

### Antes de darlo por aprobado en la placa

Cloud debe revisar el código, las notas y la regla de publicación, y dejar su dictamen. Con autorización del propietario y datos respaldados: instalar en la placa de prueba, comprobar lectura y deslizamiento con el dedo, medir memoria al buscar novedades y recorrer las tres pestañas, y ensayar actualización de pantalla/mando con conexión normal y fallo de red controlado. Comprobar que el aviso de reinicio se ve y que el mando confirma su nueva versión. No realizar estas pruebas sobre partidos reales ni cortar alimentación durante escritura sin un plan de recuperación.

Revisión de Cloud: **pendiente**. Validación física: **pendiente de autorización**.

## Validación física posterior · 20/10/2026

La versión 0.1 se ha integrado y publicado en main por autorización expresa del propietario e instalado en su placa, después de probar el firmware QR original. Novedades, deslizamiento y guía funcionan en la pantalla real. Ver [resultados físicos, comparación y límites](VALIDACION_PLACA_20261020.md); esta prueba no acredita todavía la barra durante una actualización OTA real.
