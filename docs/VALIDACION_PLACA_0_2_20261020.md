# Instalación y comprobación física de la pantalla 0.2

20/10/2026 · Equipo B. El propietario autoriza expresamente: «Sí, instala la nueva versión en la placa». Revisión de Cloud pendiente. No se modifica ni recompila el firmware publicado.

## Instalado y verificado

- Pantalla Waveshare 7C ESP32-S3 en **COM6**, versión **0.2 build 54**, código `369e153`. Binario oficial de [Firmware 39418975919](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39418975919), 1.866.112 bytes, MD5 `45c1c5ccbd991e26bc387fd548e86465`, SHA256 `18ea0c26733b60d74a651aa20a3f4bf1a2880c978bcad157b6b2a682dcf06f02`.
- Antes de grabar se vuelve a leer la tabla real: coincide byte a byte con la de la validación previa. app0: 0x10000, 6 MiB. app1: 0x610000, 6 MiB. Selección OTA válida: secuencia 3 → app0; segundo sector original secuencia 2. No se cambia esa selección.
- Se graba **solo la aplicación en app0**, con el cargador ROM (`--no-stub`). Escritura en 109,4 s y resultado **Hash of data verified**. La herramienta no borra ni graba arranque, tabla, NVS, FFat, SD o app1. Se conservan los respaldos privados anteriores; no existe copia completa de los 32 MiB.
- HELLO solicitado después de arrancar responde `HELLO waveshare-7c 0.2`. La pantalla de Actualizaciones muestra **Instalada 0.2** y **Tienes la última versión publicada**. [Captura real](capturas-placa/actualizaciones-placa-0.2.png).
- Los seis jugadores y sus fotos se ven. La captura de su lista tiene CRC32 **9d209d57**, idéntico a las versiones QR original y 0.1: mismos 768.000 bytes de imagen. No se crean ni borran jugadores, fotos o partidos.

## Comprobaciones y alcance

- Se comprueban visualmente Inicio, Ajustes, Actualizaciones, Jugadores y configuración de Rápido. No se pulsa Continuar para comenzar un partido ni se cambian goles, minutos, volumen, Wi-Fi o cuenta.
- **Cinco recorridos válidos** Inicio → Rápido (sin tocar valores) → Inicio → Ajustes → Inicio. Cada vuelta termina con la imagen de Inicio CRC32 **ff77c0d4**, coincidente con la inicial. No se observan bloqueos prolongados ni reinicios en estos recorridos.
- Los toques son órdenes USB que inyectan eventos LVGL; no sustituyen una prueba con el dedo sobre GT911 ni con los pulsadores físicos. La respuesta PONG observada es de hasta **25 ms**; no se equipara a latencia dedo→imagen.
- Se activa temporalmente RENDIMIENTO y se desactiva antes del recorrido repetido. Tras `RENDIMIENTO 0`, **cero mensajes de rendimiento** en los repintados siguientes. Esto verifica en placa el apagado corregido de los callbacks. La lista de jugadores registra un máximo de dibujo de 154 ms en una muestra, similar a las muestras anteriores; no se afirma una mejora general de FPS.
- Memoria al comienzo: interna libre 64.776 B, mayor bloque 31.732 B, mínimo 54.092 B, PSRAM libre 6.011.820 B, caché 7.123 KiB. Al final: interna libre **64.408 B**, mayor bloque **31.732 B**, mínimo **54.092 B**, PSRAM **5.479.884 B**, mayor bloque PSRAM 5.373.940 B, caché **7.619 KiB**. La caché ha crecido al visitar más pantallas; no se interpretan las diferencias como fuga ni como prueba de ausencia de fugas a largo plazo.
- NUBE sigue indicando `cuenta=0`, `pendientes=0`. No se realiza un login ni una vinculación real por QR.
- Se deja la placa en Inicio, se termina con un toque de despertar en un punto sin botones y se cierra COM6. El reposo automático normal sigue activo. No queda monitor ni instrumentación periódica ejecutándose desde el ordenador.

## Incidencias del diagnóstico USB que debe revisar Cloud

1. Dos intentos de CAPTURA aislada no completaron la recepción; en el segundo llegaron **767.744 de 768.000 bytes**. Al enviar `CAPTURA` seguida de `PING`, llegan los 768.000 bytes en aproximadamente 1,2–1,3 s. Se validan **22 archivos de captura distintos por longitud y CRC32**, sin rellenar bytes ni dar por válidas imágenes truncadas. Falta separar el comportamiento del firmware/controlador/lector de Windows; no se declara corregida esta incidencia por cambiar el lector.
2. Tras esa captura larga más PING, el primer toque puede consumirse como despertar. El código de loop toma `now` antes de atender USB y `wakeScreen()` actualiza `lastActivity` después; `now - lastActivity` puede desbordar y marcar `dimmed`. Esto es una causa probable concreta a revisar con una regresión del sketch real. Para comprobar menús se usa un toque previo en (795,479), fuera de los botones de estas pantallas, y se verifica cada pantalla por imagen. Es un ajuste del ensayo, **no una corrección de ese código**. No se contabiliza como botón perdido en uso físico sin medirlo.
3. Se descartan como recorridos válidos las primeras secuencias automatizadas: confundían la posición de VOLVER entre menús antiguos y pantallas nuevas y llegaron a abrir Prueba de la placa. Se sale con TERMINAR y se verifica Inicio. El indicador de prueba inicial ya estaba completado antes de instalar; no se inicia ningún partido. Los cinco recorridos citados arriba son posteriores, con posiciones correctas y comprobación de imagen en cada vuelta.
4. Continúan los avisos de fuentes ausentes de los menús antiguos. Su corrección sigue separada de esta instalación.

No se ocultan estos límites ni se amplía la entrega a un firmware nuevo sin preparar su versión, pruebas y autorización. La versión 0.2 solicitada queda instalada y con los límites documentados.

## Pendiente de medir

El incidente original de aproximadamente un minuto debe repetirse con el dedo, tocando las flechas de goles/tiempo y volviendo a Inicio, con y sin un lector USB consumiendo datos. En esta instalación no se cambian esos valores para medirlo. Tampoco se prueban partidos/goles, el mando físico, pérdida de alimentación ni login real por QR. La comprobación de límites de sesión de Supabase continúa bloqueada por HTTP 403 del token de Actions (`auth_config_read`).

Los registros, imágenes con jugadores, lecturas de memoria y respaldos quedan privados en el equipo del propietario. La documentación pública incluye únicamente la captura de Actualizaciones, sin nombres, fotos, Wi-Fi o credenciales.
