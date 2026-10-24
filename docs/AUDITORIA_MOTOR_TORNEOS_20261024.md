# Auditoría del motor, historial y torneos · 24/10/2026

Preparada para 0.17 sobre la integración 0.15 y los sonidos 0.16. Código y pruebas de PC; no se ha conectado, grabado ni probado la placa durante esta auditoría. Se conservan las reglas actuales de torneos, el empate permitido en liga y los identificadores de torneo/cruce de cada partido.

## Fallos confirmados y correcciones

- El historial podía comunicar éxito aunque fallase el índice. Ahora se reserva capacidad antes de escribir y se comprueban escritura y cierre del JSON y del índice. Si falla el índice, se devuelve error y se conserva el JSON para su recuperación; no se suma el contador NVS ni se encola una subida como si hubiera funcionado.
- La victoria conserva el resultado deportivo al fallar el guardado. El aviso permanece en la vista real de victoria y en Inicio, más allá del aviso breve. No hay bucle de reintentos automáticos. Un partido normal posterior puede iniciarse.
- Un torneo podía avanzar en RAM aunque su archivo siguiese con el cruce pendiente. La prueba reproduce ese desacuerdo bloqueando la ruta temporal de guardado. Ahora queda un aviso persistente y se reintenta únicamente al pulsar volver al torneo o Inicio. Si el resultado ya se aplicó, se vuelve a guardar sin aplicarlo otra vez, sin crear otro partido ni aumentar NVS otra vez. Hasta guardar, no se muestra la ceremonia de campeón ni se permite iniciar otro partido que sustituya el pendiente. La voz de victoria sigue expresando el resultado deportivo inmediato.
- Si falla el propio JSON del partido de torneo, no se anota el resultado en el cruce ni se incrementan los contadores. Se muestra el fallo; este caso no incorpora recuperación automática del partido perdido.
- Anular un cruce ya no borra primero el partido. Se guarda el estado del torneo con el cruce pendiente y después se solicita borrar el partido; un fallo de borrado se avisa. La notificación de borrado a la nube depende del resultado de la operación local.
- Anular, cambiar reglas o marcar abandono conservan una copia profunda temporal del torneo antes de mutar. Si el guardado falla, se recuperan metadatos, equipos, cruces y resultados anteriores. Si no se puede reservar esa copia, la operación no comienza. Crear un torneo tampoco abre su ficha ni confirma éxito si falla el guardado.
- En partidos por tiempo o goles que terminan en la primera parte, el historial ya no inventa una segunda parte. Los eventos de tanda parten del tiempo total anterior a la tanda, sin retroceder por la duración de la prórroga. No se inventan fechas para partidos sin reloj válido.

## Memoria y alcance

La copia temporal utiliza una reserva explícita en PSRAM en el firmware, sin alternativa en RAM interna. El ensayo nativo mide 210.200 bytes (aproximadamente 205,3 KiB); el tamaño exacto en ESP32 depende de su ABI de 32 bits. Solo existe durante operaciones explícitas de gestión, nunca al registrar el gol. Su destructor la libera también al fallar. `undoLast`, `updateRules` y `markAbandoned` no liberan ni sustituyen los punteros de las tablas originales; por eso restaurar el estado y las filas copiadas recupera los mismos almacenes.

No se cambian las reglas del motor, del cuadro, de la liga o del Pool, ni los sonidos instalados. Los límites de eventos y los casos más amplios de recuperación requieren trabajo separado.

## Verificación específica

Ejecutados correctamente con los mocks del sketch y LVGL:

- `s3_history_storage_test.cpp`: fallo de índice detectable, JSON anterior intacto y huérfano recuperable explícitamente, fase realmente jugada, tiempos de tanda, empate con `allowDraw`, ganador nulo y conservación de identificadores y fechas al recargar.
- `s3_history_save_feedback_test.cpp`: fallo real de archivo, victoria conservada, aviso visible después de tres segundos y en Inicio, sin contador NVS falso ni reintento periódico, siguiente partido permitido.
- `s3_torneo_storage_failure_test.cpp`: archivo de torneo bloqueado, estado anterior persistido, resultado pendiente visible, reintentos fallidos desde volver e Inicio, recuperación por ambas salidas sin duplicación; fallo de reserva; restauración exacta de cruce al fallar anulación y de reglas al fallar cambio; comprobación de que el torneo ya está persistido antes del borrado del partido; fallo del JSON sin avanzar el torneo.

Las tres pruebas están incluidas en `check_copies.sh`. El simulador general usa un directorio de historial propio por ejecución para comprobar guardados reales sin depender de una carpeta de compilación preexistente. La compilación ESP32, la batería completa y la publicación las coordina la entrega 0.17; estos ensayos no demuestran estabilidad física del panel o de la tarjeta.

## Riesgos y propuestas pendientes

- JSON de partido e índice siguen siendo dos archivos: un fallo parcial del índice puede dejar un JSON huérfano o una línea incompleta. Se detecta el error, pero no hay transacción con diario ni reparación automática tras reinicio. No se hacen reintentos ciegos que puedan duplicar filas.
- El resultado de torneo pendiente permanece en RAM. Cortar alimentación antes de reintentar puede dejar el partido guardado y el torneo anterior. Una recuperación durable e idempotente al arrancar requiere diseño y pruebas adicionales.
- Guardar primero el torneo al anular protege el partido frente al fallo de ese guardado, pero no convierte el borrado del historial en una transacción. `partidos::remove` conserva limitaciones previas ante fallos parciales del índice; el booleano no garantiza todos los escenarios de pérdida de alimentación. No se promete atomicidad entre torneo, JSON, índice y nube.
- La creación de un torneo puede haber actualizado equipos con nombre antes del fallo del archivo del torneo. Se evita el falso éxito del torneo, sin convertir ambas operaciones en una transacción conjunta.
- Continúan los límites actuales de 60 lanzamientos de tanda y 120 registros de gol. No se ha añadido una nueva regla deportiva para resolver su agotamiento. La anulación después del final y la reconstrucción completa de la cronología de Partido Loco quedan pendientes de una especificación y pruebas ampliadas.
- Hace falta comprobar en la placa un torneo completo, fallos físicos de tarjeta y tiempos de gestión con tablas grandes. No se deben provocar fallos sobre los partidos reales del dueño.
