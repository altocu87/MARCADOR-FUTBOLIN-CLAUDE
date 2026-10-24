# Auditoría de memoria y audio · 24/10/2026

## Base y alcance

Revisión iniciada sobre la normal 0.15 (`6c20dbf`) y actualizada después de incorporar el snapshot 0.16 (`84ee12a`) a la rama de auditoría 0.17. La 0.16 añade voces de tanda: el informe del día 23 y su límite de 31 familias ya no describen el código vigente.

Se han leído fuentes y documentación, sin abrir WAV privados ni consultar la placa. Las comprobaciones siguientes son del PC. El estado físico comunicado por la entrega 0.16 es 870 WAV, 806 activos y 64 reservados; esta auditoría no vuelve a verificar esos archivos. La normal 0.15 documentó mínimo interno de 35.208 B; la fase 3 llegó a 3.220 B y fue retirada. No se modifica SDK, DMA ni tamaño de búferes.

## Fallo confirmado y corrección

Al terminar empatada una liga con `allowDraw`, el motor informa `Finished`, segunda parte y resultado de empate. El detector de sonido generaba `Victory` para cualquier final; además, la condición de `FinalNormal` no comprobaba que hubiera ganador. Podían sonar una victoria personalizada o voces de final con ganador en un empate real.

La corrección usa el evento neutro existente `PeriodEnd` cuando `eng.draw()` es verdadero y excluye ese resultado de `FinalNormal`. Respeta las opciones de ese evento (predeterminado, ninguno o propio). No añade familias, preferencias, tablas ni esperas. Gol ganador, victoria, gol de oro, tanda y voz de campeón conservan sus reglas; el cambio no espera a que termine una narración ni altera el guardado.

Archivos de producción: `hardware/common/mfv3_sound.h`, su copia en `hardware/esp32s3_pantalla7/`, y una condición de `afterChange` en el sketch. La copia común y la de la pantalla deben permanecer idénticas.

## Memoria y límites vigentes

| Elemento | Estado auditado |
|---|---|
| Catálogo activo | Global interno; 1.024 códigos de 32 bits, contadores y máscara. `sizeof(Catalog)=4.264 B` confirmado en pruebas del PC; comprobar ELF objetivo antes de atribuir cifra al binario |
| Familias | 39, índices 0–38; máscara `uint64_t`, 25 posiciones libres |
| Selección | Códigos y barajado en RAM; no recorre SD ni abre candidatos del catálogo al elegir |
| Carga/recepción | Valida cabeceras fuera del partido; catálogos temporales con `new(nothrow)`. La política general favorece PSRAM, pero no expresa una obligación de usarla |
| Respuesta `COMENTARIOS?` | Búfer de 320 B desde 0.16; ya no aplica el antiguo riesgo de 160 B al ampliar familias |
| Audio | DMA nominal 8.000 B para 125 ms; buffers del productor 2.560 B y bloque temporal 1.024 B. Sin WAV completos en memoria |
| Pantalla | Dos imágenes RGB565: 1.536.000 B PSRAM; dos reservas RGB de 20 líneas: 64.000 B internos, sin contar descriptores |
| Tablas de interfaz | La entrega 0.15 trasladó explícitamente 28.152 B de contenido a PSRAM tras arrancar, dejando 40 B de manejadores; no trasladó audio ni motor |

Los tamaños de datos anteriores no incluyen sobrecostes del asignador o del driver y no deben sumarse como si fueran el consumo total del firmware. La corrección del empate no cambia las reservas permanentes.

La 0.16 convirtió correctamente los desplazamientos del selector a `PoolMask(1) << p`, incluidos los bits superiores a 31. Quedan algunos `1u << pool` en el sketch, pero sus dominios actuales son fases hasta el índice 29 o ambiente 22/23; no alcanzan los nuevos índices altos. Cualquier futura ampliación de esos caminos deberá mantener el tipo de 64 bits.

## Diseño de capacidad: pendiente, sin modificar en esta entrega

Actualmente quedan 154 plazas para el total de WAV del paquete y 218 referencias activas. Añadir otros 500 produciría aproximadamente 1.370 WAV y, si todos fueran activos, 1.306 referencias: superaría ambos límites. No confundir el número de archivos con el número de familias; la ampliación a 64 bits ya está hecha.

Los límites de 1.024 están duplicados en:

- `s3_comentarios.h`: catálogo activo, carga y recepción.
- `mfv3_sound.h`: inventario de 32 páginas de 32 códigos; dos reservas de 1.025 elementos para detectar truncamiento.
- `preparar-comentarios.py`, `subir-comentarios.ps1` y `CommentarySdInstaller.cs`: total del paquete, incluidos reservados. El instalador exige además literalmente `limite=1024` en la respuesta.
- Pruebas y guardas: `check-ui-memory.py` exige conscientemente un catálogo grande interno. Cambiar su ubicación requiere actualizar ese contrato, no eliminar la comprobación.

Propuesta para una entrega independiente: capacidad 2.048, catálogo y temporales asignados explícitamente en PSRAM después del arranque, sin fallback a RAM interna. Con las 39 familias actuales, el objeto sería 8.360 B bajo el mismo ABI de la prueba; activo, staging y comprobación simultáneos sumarían 25.080 B, sin PCM. El inventario de 2.049 códigos/ubicaciones requeriría 10.245 B temporales, más metadatos. No ampliar DMA, pila ni buffers por tener más archivos.

La propiedad de los bloques debe quedar explícita: no copiar superficialmente un puntero al sustituir el catálogo. Preparar y validar el nuevo catálogo por separado, y reemplazar el activo solo si todo termina bien; fallo de reserva debe conservar el anterior. Mantener asignaciones fuera del gol y del selector. El formato CMT1 conserva su fila de un byte de familia y cuatro bytes de código, y su cuenta de 16 bits permite 2.048 sin cambiar el formato. Una versión antigua seguirá rechazando un catálogo mayor: el instalador debe exigir la versión nueva antes de copiarlo y explicar la limitación al volver a firmware antiguo.

También debe revisarse el listado: cada página recorre de nuevo la SD y la deduplicación compara con los códigos acumulados. Duplicar la capacidad sin cambiar esto puede aumentar las pausas de gestión; está prohibido durante partido, pero no hay una medida que garantice fluidez fuera de él. La capacidad adicional se deja como diseño porque no hay un paquete mayor de 1.024 en esta entrega y la normal recién validada conservó deliberadamente el selector interno.

Pruebas para esa futura ampliación: 1.024/1.025/2.048/2.049 archivos, reservados frente a activos, última página, truncamiento explícito, respuesta completa de contadores, fallo de PSRAM, conservación del catálogo anterior, respaldo CMT1, firmware antiguo, distribución 70/30 y rondas sin repetición. Verificar ELF y probar la placa antes de declarar estable la nueva ubicación.

## Contraste entre agentes

Motor confirma que `draw()` identifica el empate y `winner()` no tiene valor en ese caso; sonido neutro y exclusión de `FinalNormal` son coherentes. Pantalla confirma la corrección y conserva sus cambios en arranque/diagnóstico sin tocar esta selección.

La voz `ShootoutChampion` de 0.16 expresa el resultado deportivo, seleccionado antes del guardado diferido para mantener victoria inmediata. Esta auditoría no cambia esa política. El flujo de guardado debe comunicar cualquier fallo de persistencia y no presentar como guardado un torneo que sigue pendiente; anunciar el resultado y confirmar el almacenamiento son hechos distintos.

Para la recuperación de cambios administrativos de torneo, un snapshot en PSRAM solo es aceptable fuera del partido, reservado antes de mutar, sin fallback interno y liberado en todas las salidas. El fallo de reserva debe dejar los datos intactos. No se incorpora ese bloque al gol ni a la reproducción; lo implementa el agente de motor.

## Validación de esta corrección

- `sound_test`: correcto; incluye empate 0–0/1–1 con tiempo y ambas condiciones, aviso único y regresiones de gol de oro/victoria.
- Nueva `s3_commentary_draw_test` con LVGL: correcta. Comprueba preferencias Default/None/Custom, ignora la voz personalizada de victoria en empate, no consume `FinalNormal`, prórroga/tanda ni `ShootoutChampion`, y conserva la voz normal con ganador. No persiste los partidos sintéticos.
- `s3_commentary_events_test` y `s3_commentary_012_test`: correctas; introducción, gol/anulación, victoria inmediata y reparto contextual conservados.
- `s3_commentary_016_test`: correcta; predicciones contrastadas con el motor, persistencia de bits 31–38, avisos/resultados de tanda, muerte súbita y series.

La prueba nueva queda incorporada en `hardware/test/check_copies.sh`. No se atribuye a estas pruebas ninguna medida acústica o visual de la placa.
