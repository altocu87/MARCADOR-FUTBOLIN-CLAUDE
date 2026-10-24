# Revisión del panel y arranque de red · 24/10/2026

## Base y evidencia

Revisión de la normal 0.15 en `main` (`6c20dbf`), tras releer `CLAUDE.md`, el estado vigente y `INTEGRACION_A_B_0_15_20261024.md`. No se debe aplicar en bloque el antiguo borrador de 0.13: la normal ya tiene reloj fijo de 12,5 MHz, recuperación notificada tras escrituras de flash, tablas de interfaz reservadas en PSRAM y sincronización automática solo en reposo, cancelable al volver a usar la placa.

La documentación de la instalación normal registra menú físicamente estable y mínimo interno de 35.208 bytes. Son evidencias de ese recorrido, no una prueba completa de juego, torneo o Wi-Fi. La fase 3 fue probada y retirada tras alcanzar un mínimo de 3.220 bytes internos; esta revisión no la recupera ni cambia el SDK, la caché, los buffers o las particiones. La versión final, sonidos instalados y estado de publicación los registra el coordinador en la entrega integrada.

## Tres cambios acotados

1. La búsqueda automática de versiones se iniciaba en `setup()` antes de precargar imágenes, fuentes y jugadores. Ahora espera al primer fotograma presentado de Inicio. Se reduce el solapamiento de conexión y precarga; no se promete ausencia de contención durante otras operaciones. Sin credenciales no conecta. El asistente inicial puede terminar antes de esa búsqueda.
2. «Pon la hora: conecta Wi-Fi» provenía exclusivamente de `!rtc.clockOk`. «Hora sin ajustar» aclara su significado. No indica contraseña incorrecta ni fallo de actualización. No ajusta fechas o reloj y utiliza letras existentes.
3. `resyncPanel()` ignoraba el resultado de la solicitud al controlador. Cuenta ahora los retornos fallidos y los expone como `resync_error` en `COMENTARIO_ESTADO?`. Un retorno correcto confirma aceptación de la solicitud; no confirma la imagen física.

Se conserva sin cambios la condición `resting`, `nube::sync(true)`, el plazo de quince minutos, el aviso en reposo, la cancelación al tocar y la sincronización manual. No se introducen `setNetwork`, cambios de frecuencia ni reinicios periódicos. También se conserva el mecanismo `s3flash` incorporado por A.

## Coste y recuperación

Un contador de 32 bits y una bandera de arranque, con posible alineación, sin nuevas reservas de PSRAM, DMA o archivos. El búfer local del diagnóstico pasa de 310 a 350 bytes solo durante esa orden. Las tareas, pilas y rutas del gol mantienen su tamaño. La búsqueda inicial comienza después de la precarga y de presentar Inicio.

No se modifican datos, formato de almacenamiento, particiones ni binarios en la placa. La recuperación sigue el procedimiento vigente de selección de aplicación anterior, conservando NVS, FFat y SD. Una instalación posterior requiere comprobar el arranque activo y coordinarse con el dueño; esta auditoría no abrió USB.

## Pruebas y límites

Se amplía `s3_opciones_test.cpp` con credenciales guardadas: al terminar `setup()` no debe existir intento de conexión; tras presentar Inicio debe iniciarse uno. También comprueba el texto de hora y la exposición del contador de errores con un valor simulado. Ese último caso prueba el protocolo, no provoca un fallo real del controlador.

Se conservan las pruebas existentes de ausencia de reinicios periódicos y recuperación puntual, y las de aviso en reposo/cancelación en `s3_sim_test.cpp` y `nube_memory_test.cpp`. La ejecución integrada y compilación real las documenta el coordinador; este informe no las da por realizadas anticipadamente.

Pendientes físicos: arranque repetido, navegación y escritura de ajustes, búsqueda de versiones, sincronización en reposo con fotos, despertar mientras sincroniza y descarga OTA. Observar la pantalla además de registrar errores y memoria. Un USB que responde o una captura interna correcta no acreditan estabilidad del panel.

## Contraste del equipo

Memoria/audio y motor/calidad coinciden en este alcance. Se respetan sus zonas de guardado, torneo y selección de sonidos durante la implementación. La fijación del núcleo normal de compilación en CI, si se integra, corresponde al coordinador y no habilita opciones de la fase 3.
