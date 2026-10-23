# Incidente de imagen tras 0.11 · equipo B · 23/10/2026

## Estado: fallo físico reportado; prueba instalada; no confirmado resuelto

El dueño ve la carga y, al terminar, alternancia de azul/negro/gris. La captura interna de Inicio es normal (CRC32 b31d151f) y USB responde, **lo cual no demuestra que el panel físico funcione**. Las comprobaciones de instalación anteriores acreditan firmware, archivos e integridad; queda invalidada cualquier interpretación de que acreditasen estabilidad visual o acústica jugando.

La última grabación de producto previa al incidente fue 0.11 local build base 54, fuente 63ad580, incorporando 0.9 de A; no se instaló 0.9 por separado. No hay causalidad demostrada contra los nuevos audios ni contra el cambio de A. B revisa el paso de carga a loop: la comprobación automática de actualizaciones se inicia antes de precargar imágenes; después setNetwork puede cambiar de 12,5 a 10 MHz y solicitar realineaciones cada 250 ms (aff9735). Es una hipótesis de investigación.

## Evidencias antes de intervenir

- COM6, HELLO waveshare-7c 0.11, PONG. Cuatro partidos y catálogo 496/1024.
- Memoria libre interna entre 42 y 52 KB, mayor bloque 31.732 B; PSRAM libre ~5,95–6,00 MB. Sin evidencia registrada de agotamiento de memoria.
- frame_error=0, vsync_timeout=0, reproductor idle y errores de audio=0. Estos contadores no detectan todos los fallos físicos del panel.
- Se observó USB_UART_CHIP_RESET durante operaciones de diagnóstico; además el dueño desconectó/reconectó. No afirmar watchdog, brownout o reinicio espontáneo sin registro que lo pruebe.

## Prueba privada autorizada e instalada

Autorización del dueño: «haz lo que tengas que hacer para solucionarlo». Copia privada del sketch 0.11, sin publicar OTA ni nueva versión, con dos fuentes distintas: ui_port.h y esp32s3_pantalla7.ino. Arranca a 12,5 MHz y evita los cambios de frecuencia y reinicios periódicos por red; conserva realineaciones solicitadas tras escritura de flash. Añade PANEL? y controles temporales no persistentes PANEL_FIJO/PANEL_AUTO/PANEL_RECUPERAR, bloqueados durante partido. Aísla el conjunto de control dinámico: no distingue por sí sola frecuencia frente a reinicio periódico.

Binario de prueba: 1.937.408 bytes, SHA256 fccda9b07a6084f7d83e0fc0224c57693623c2bb95f68a583f3e61882147c8e5. Compilación real correcta, fuentes contrastadas con copia compilada, +24 B de globales para diagnóstico. Primeros 64 KB guardados privadamente; tabla sin cambios, OTA seq3/seq4 válidas/confirmadas, activa app1. Escrito solo app1 (0x610000) y hash remoto verificado. Conservados app0, NVS, otadata, FFat y SD. Binario original 0.11 guardado para recuperación.

Primera consulta tras instalar: PANEL fijo=1 hz=12500000 red=1 reinicios=0 error_reinicio=0 error_reloj=0 frames=70 frame_error=0 vsync_timeout=0. Memoria interna libre 51.464 B y PSRAM 5.957.100 B. Conservados los cuatro IDs/resultados/fechas y catálogo con los 24 recuentos originales. No se inició partido ni se simuló toque/gol ni se ajustó fecha.

## Pendientes y límites

1. Confirmación visual del dueño, solicitada después de grabar: no dar por resuelto por recibir respuestas USB.
2. Si el fijo funciona, contrastar recuperación/reproducibilidad y decidir corrección mínima; revisar carga de red y descarga OTA para no reintroducir el problema que intentaba resolver 0.9.
3. Si falla, continuar diagnóstico de señal/control/alimentación; no diagnosticar avería física ni mala alimentación sin evidencia.
4. Sustituir la prueba privada por una versión de producto validada, sin publicar comandos de laboratorio accidentalmente. Mantener 0.x, novedades y revisión de A. Main sigue en la 0.11 publicada, sin esta prueba.

Los registros y copias privadas están en la carpeta diagnostico-panel-0.11 del proyecto local de Codex; no subir NVS, audios ni copias privadas al repositorio.
