# Auditoría del panel y Wi-Fi · 23/10/2026

## Estado y alcance

Revisión de código sobre `0922d06`, contrastada con los agentes de memoria/audio y motor/calidad. El dueño confirma el menú estable con la prueba privada 0.11 de reloj fijo; comunica interferencias intermitentes, especialmente con Wi-Fi. No se ha aislado si el fallo original depende del cambio de frecuencia, de los reinicios periódicos o de su interacción. Capturas internas, respuestas USB y contadores sin errores no acreditan estabilidad del panel físico.

Este cambio prepara código de producto. No instala firmware, no abre USB, no cambia datos ni ajusta la hora. La publicación e integración de sonidos las coordina la sesión principal.

## Evidencias y corrección acotada

- `ui_port.h` alternaba 12,5/10 MHz y solicitaba reinicios cada 250 ms de red. Ahora conserva 12,5 MHz; `setNetwork()` solo registra actividad. Se mantienen las solicitudes existentes tras flash y las de progreso OTA, sin controles temporales de laboratorio. Se cuenta el retorno fallido de la petición de recuperación; aceptar una petición no demuestra recuperación visual.
- `setup()` conectaba antes de precargar imágenes, fuentes y jugadores. Ahora la búsqueda inicial espera a un fotograma presentado de Inicio. La sincronización automática de cuenta también espera ese primer Inicio. Se conservan las acciones manuales posteriores y el asistente inicial de la placa.
- La advertencia inferior procedía de `!rtc.clockOk`, después de las prioridades de batería y modo prueba. «Hora sin ajustar» hace explícito su significado. No indica contraseña incorrecta, pérdida de red ni fallo de actualización. Solo usa letras ya presentes; no modifica fuentes ni requiere nuevos recursos.
- No se aumentan buffers. Los dos fotogramas RGB565 ocupan 1.536.000 bytes de PSRAM; los dos buffers intermedios de veinte líneas ocupan aproximadamente 64.000 bytes internos, aparte de descriptores. La evidencia del incidente recoge solo 42–52 KB internos libres.

## Núcleo y recuperación

El Arduino ESP32 instalado es 3.3.12. Su `esp32s3-libs/3.3.12/qio_opi/include/sdkconfig.h` activa `CONFIG_LCD_RGB_RESTART_IN_VSYNC`, `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP` y `CONFIG_ESP32S3_DATA_CACHE_LINE_32B`. La configuración del núcleo desactiva XIP de PSRAM y `LCD_RGB_ISR_IRAM_SAFE`. Por tanto, esta compilación ya incorpora recuperación del controlador en VSYNC; las peticiones periódicas adicionales de la aplicación no son necesarias para habilitarla.

NVS mediante `PrefsW` y recepción USB de jugadores notifican escrituras. Varias rutas FFat de equipos, jugadores locales y estado de cuenta no tienen notificación explícita. Se deja documentado para una revisión separada, conservando la recuperación automática del núcleo: no se reemplaza el reinicio periódico por reinicios indiscriminados desde la nube. Un único aviso en `hooks.finished` tampoco cubriría cancelaciones, inicio/cierre de sesión y preferencias de cuenta.

La configuración efectiva del núcleo debe quedar reproducible en CI. Cambiar macros con `-D` no recompila las bibliotecas precompiladas. Si continúan las interferencias, estudiar por separado un núcleo con líneas de caché de 64 bytes y XIP, midiendo memoria, arranque y Wi-Fi/OTA; no forma parte de esta corrección.

Espressif documenta la contención de PSRAM entre núcleos, el efecto de escrituras flash sobre buffers intermedios y el carácter diferido de `esp_lcd_rgb_panel_restart()` al siguiente VSYNC. Es un mecanismo plausible, no una atribución causal de este incidente: [RGB LCD](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html), [FAQ oficial](https://documentation.espressif.com/api/resource/path/docs%2Fprojects%2Fesp-faq%2Fen%2Flatest%2Fesp-faq-en-master.pdf).

## Coste

- Sin nuevas reservas de imágenes, audio, DMA o PSRAM; sin archivos persistentes adicionales.
- Un indicador de comprobación inicial y un contador de fallos de recuperación; se elimina el temporizador de reinicio por red. Medir tamaño final con la compilación integrada, sin atribuir diferencias de binario solo a este cambio.
- Se eliminan cuatro solicitudes de reinicio por segundo durante red y los cambios de reloj. No se promete una tasa de imágenes ni latencia física hasta medir.
- La comprobación inicial comienza más tarde: después de la precarga y de presentar Inicio. Durante el asistente inicial espera a que se salga a Inicio.

## Pruebas y límites

Se actualiza `hardware/test/s3_opciones_test.cpp` para comprobar credenciales guardadas sin conexión durante `setup()`, conexión después del primer Inicio, reloj fijo y ausencia de reinicios durante actividad de red prolongada, conservación de recuperación NVS y significado del aviso de hora. El coordinador ejecuta las pruebas y compilaciones integradas para no compartir salidas temporales con la sesión de sonidos. Este documento no presenta esas pruebas como ejecutadas ni como validación física.

Validación física pendiente: arranque repetido, navegación, búsquedas de redes/versiones, QR/cancelación, sincronización con fotos, pérdida de conexión y descarga OTA. Observar el panel y la respuesta táctil además de registrar contadores. No iniciar partidos ni generar goles reales sin coordinación con el dueño. Comprobar pulsadores y sonido jugando en una prueba acordada.

## Recuperación

El cambio no altera particiones ni almacenamiento. Antes de cualquier instalación, comprobar versión, partición activa y conservación de datos según el procedimiento del incidente. Conservar el firmware anterior y la prueba privada local. Si aparece degradación, restaurar únicamente la aplicación mediante el procedimiento autorizado; nunca borrar NVS, FFat o SD. No publicar copias privadas de memoria ni comandos de laboratorio.
