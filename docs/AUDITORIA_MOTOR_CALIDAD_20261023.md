# Auditoría de motor e historial · 23/10/2026

Trabajo en copia aislada para la pantalla 0.13. Sin tocar la placa, SD, partidos reales, fotos ni sonidos del dueño. Los casos descritos se ejecutan con archivos ficticios y motor C++ en el PC. No acreditan estabilidad visual, latencia del mando ni audio en la placa.

## Confirmado y corregido

1. **Falso éxito al guardar si falla el índice.** `partidos::save` ignoraba el resultado de `appendIndex`. Reproducción previa: crear una carpeta llamada `indice.txt`, terminar un partido y guardar; devolvía éxito con una fila en memoria, pero al reiniciar desaparecía de la lista. Ahora comprueba escritura, error y cierre de ambos archivos; reserva antes la memoria de la fila; sólo confirma el guardado tras escribir el índice. Si falla el índice, conserva el JSON para recuperación. No reintenta automáticamente: una escritura parcial no justifica crear copias nuevas.
2. **Contadores y aviso tras fallo.** Antes, `saveResult` actualizaba NVS y el contador aunque el partido no se guardase. Ahora el error se propaga sin cambiar esos contadores ni anunciar el partido a la nube. La victoria se conserva, el error aparece en la pantalla real de victoria y en Inicio hasta empezar otro partido. Se permite continuar jugando. Prueba con más de tres segundos de espera: el aviso sigue en la pantalla aunque ya haya desaparecido el aviso temporal; varias vueltas del bucle no duplican escrituras.
3. **Segunda parte inventada en el historial de «Ambas».** Si se alcanzaban los goles para ganar durante la primera parte, el JSON incluía una segunda parte vacía. Ahora las partes salen de la fase realmente alcanzada por el motor. La prueba falló antes de corregirlo y pasa después.
4. **Eventos de penaltis con tiempo atrasado.** El final de una prórroga de 60 segundos figuraba a 180 segundos de juego, pero la tanda y el final del partido retrocedían a 120 segundos. Se usaba el inicio de la prórroga. Ahora la tanda usa todo el tiempo jugado, sin sumar tiempo ficticio a los penaltis. La prueba falló antes y pasa después.

Los cambios no modifican las reglas, resultados, objetivos de goles, duraciones, hándicaps ni catálogo de audio. Mantienen las llamadas de servicio de audio entre bloques de guardado y el orden victoria → audio → persistencia.

## Verificación propia

- `s3_history_storage_test.cpp`: índice rechazado, conservación del JSON y del partido anterior, recuperación explícita posterior del archivo, segunda parte no inventada y tiempos de tanda/final ordenados. Correcta con GCC en Windows.
- `s3_history_save_feedback_test.cpp`: bucle real simulado, victoria y aviso duradero, NVS intacta cuando falla, ausencia de reintentos y posibilidad de empezar otro partido. Correcta con LVGL real del proyecto, placa simulada 4.
- `s3_sim_test.cpp`: prepara ahora su propio almacén temporal para cada placa. La comprobación anterior de contador podía pasar aunque no existiese la carpeta necesaria para guardar; se conserva la aserción de resultado y se corrige la preparación del entorno.
- La regresión conjunta de motor, pausa, bloqueo de tres segundos, cambios de parte, gol de oro, penaltis, hándicaps y visitas la ejecuta el coordinador. El resultado de esa ejecución se registra en la entrega conjunta.

## Límites confirmados y trabajo posterior

- **Tanda limitada a 60 lanzamientos.** Reproducido: con 60 fallos y empate, el intento 61 devuelve estado inválido y la tanda no puede terminar sin deshacer. No se cambia esta regla de capacidad en 0.13: necesita decidir una representación ampliable de la tanda y cómo mostrar/guardar tandas largas.
- **Máximo de 120 registros de gol.** El motor rechaza goles nuevos al alcanzar ese límite; también cuenta los anulados. Afecta potencialmente a partidos largos por tiempo o con muchas correcciones. Pendiente diseñar capacidad y aviso adecuados, sin aumentar grandes tablas en memoria interna a ciegas.
- **Recuperación tras fallo de disco o corte eléctrico.** El aviso nuevo evita dar por guardado lo que no se confirmó, pero no convierte JSON e índice en una transacción. Un JSON puede quedar huérfano o incompleto; el arranque actual sólo lee el índice. Pendiente recuperación explícita o automática que valide archivos, reconstruya índices y evite duplicados. No se borran archivos del dueño como supuesto arreglo.
- **Corrección del gol que decide el partido.** El motor no permite anular una vez terminada la fase. Cambiarlo requiere decidir cómo reabrir el resultado, el historial y su sincronización; no es una optimización de pantalla.
- **Partido Loco: historial reconstruido.** Las visitas no conservan su instante exacto y se exportan al final de la parte. Los tiempos de pared de eventos se reconstruyen con tiempo jugado, excluyendo pausas. El resumen del resultado puede ser correcto sin que ese historial sea una grabación exacta de cada acción. Pendiente diseñar registro temporal completo si se quiere analizar esa cronología.
- **Torneos nativos aún no implementados.** El menú muestra que llegarán más adelante. No se presenta una prueba de torneos de la app web como una prueba de torneos en la pantalla.

## Pendiente físico

Jugar con mando, golpes reales, sonido y cambios de fase; observar anulación, pausa, tiempo agotado, prórroga y penaltis; comprobar el resultado tras reiniciar. Las pruebas de Wi-Fi y panel corresponden al informe de pantalla. No se atribuyen las interferencias a un fallo del motor sin medición física.
