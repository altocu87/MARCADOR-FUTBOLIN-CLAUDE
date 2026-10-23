# Auditoría de memoria, almacenamiento y audio · 23/10/2026

Revisión de solo lectura del código base 0.11 en la copia aislada `auditoria-0.13`. Este documento no acredita instalación ni prueba física. No se han abierto audios, fuentes privadas, copias de la placa ni puertos; tampoco se han cambiado catálogos o versiones. Otra sesión prepara sonidos 0.12: las propuestas de capacidad deben contrastarse con su entrega antes de aplicarse.

## Conclusión

No se ha identificado una causa demostrada del fallo visual dentro del selector de audio. El reproductor está inactivo al llegar a Inicio; el catálogo valida las cabeceras antes de la pantalla de carga. Aumentar la cantidad de archivos en SD no aumenta por sí mismo las reservas estáticas actuales. Sí existen dos límites diferentes de 1.024 y una reserva del catálogo en RAM interna que conviene resolver antes del crecimiento previsto.

La memoria libre registrada en el incidente, 42–52 KB internos y un mayor bloque de 31.732 B, deja poco margen para aumentar búferes internos. Son mediciones históricas, no una lectura nueva ni prueba de agotamiento. Los aproximadamente 6 MB libres de PSRAM tampoco demuestran que su ancho de banda sea suficiente durante pantalla y Wi-Fi.

## Mapa de reservas y costes

Las cifras siguientes son tamaños de datos deducibles del código o del test existente; no son una suma del consumo total del firmware. No incluyen metadatos del asignador, descriptores de DMA, objetos del driver ni reservas de red.

| Reserva | Datos | Ubicación / observación |
|---|---:|---|
| Dos imágenes RGB565 de 800 × 480 | 1.536.000 B | PSRAM explícita, `ui_port.h`: `num_fbs=2`, `fb_in_psram=1` |
| Dos búferes RGB de 20 líneas | 64.000 B = 2 × 800 × 20 × 2 | RAM interna del driver; además hay descriptores y control |
| DMA I2S, 8 × 250 tramas estéreo de 16 bits | 8.000 B | Reserva del driver para 125 ms a 16 kHz; no incluye descriptores |
| Productor de audio: `in_[512]`, `mono_[256]`, `stereo_[512]` | 2.560 B | Miembros del reproductor estático; no incluye sus demás campos |
| Bloque temporal `fill`: `raw[BLOCK*4]` | 1.024 B | Pila, solo al rellenar; no es otra pista completa |
| `Catalog` activo con 1.024 referencias y 24 familias | 4.200 B | Global estática en RAM interna: 4.096 B códigos + 96 B contadores + contador/alineación/máscara |
| Catálogo temporal de carga / recepción / comprobación | 4.200 B cada uno | `new(nothrow)`; configuración general dirige reservas >32 B a PSRAM, sin garantía explícita de capacidad de memoria |
| Inventario paginado: 1.025 códigos + 1.025 `Place:uint8_t` | 5.125 B | Dos reservas dinámicas durante cada petición; no persisten tras responder |
| Inspección CRC USB: `static block[2048]` | 2.048 B | RAM interna estática; inspección prohibida durante partido |
| Pila de la tarea principal | 32.768 B solicitados | `SET_LOOP_TASK_STACK_SIZE(32*1024)`; no se propone moverla sin revisar llamadas flash |
| Recepción USB | 8.192 B solicitados | `Serial.setRxBufferSize(8192)`; tamaño solicitado, no medida de reserva efectiva |
| Caché de imágenes | Tope lógico 8.388.608 B | No equivale a memoria siempre ocupada; imágenes se asignan y expulsan según uso |

Los 560 WAV documentados ocupan 146.192.816 B de archivo en SD, sin contar clústeres FAT. El catálogo CMT1 actual ocupa 2.490 B (10 + 5 × 496). Ningún WAV completo se precarga en RAM. El lector mantiene bloques y convierte a salida de 16 kHz.

## Hallazgos confirmados

1. **Capacidad activa e inventario total son límites separados.** `s3_comentarios.h:6,162,179,230` limita el catálogo activo a 1.024 referencias. `mfv3_sound.h:514–519` rechaza página >=32 y trunca el inventario total a 1.024 WAV. Por tanto, ~1.060 WAV ya superan el inventario aunque los reservados mantengan el catálogo activo por debajo del límite. No basta con cambiar una constante.
2. **Ampliar solo `MAX_FILES` consume más RAM interna.** El catálogo activo es `static Catalog catalog`. Con las mismas 24 familias, 2.048 referencias ocuparían 8.296 B y 4.096 ocuparían 16.488 B: respectivamente +4.096/+12.288 B internos. La recomendación es asignar explícitamente en PSRAM después de habilitarla, con fallo controlado que preserve el tono de respaldo. No inicializar ese objeto mediante una reserva global anterior a `setup`.
3. **Listado de archivos bloqueante, fuera del partido.** Cada página llama de nuevo a `Store::list`; `FsStore::listDir` recorre directorios y busca duplicados recorriendo los códigos acumulados. El trabajo de deduplicación puede crecer cuadráticamente y se repite por página. `delay(1)` cede CPU al sistema pero no retorna al bucle que actualiza la interfaz. Las órdenes de inventario están prohibidas durante partido; no hay medición de cuánto se detiene una pantalla de gestión.
4. **La selección automática no recorre SD.** `Catalog::pick/draw` opera con códigos en RAM y contadores por familia. Solo después del fotograma se abre la pista elegida. La carga inicial y el cierre de una transferencia sí validan todos los candidatos, fuera de partido. Excepción existente: elecciones manuales `Custom` en `playEvents` comprueban hasta cuatro códigos con `find/exists` antes de elegir; no se confunde ese camino con el catálogo automático.
5. **Audio no escribe FFat en la configuración actual.** `SOUNDS_ON_CHIP=false`; WAV, recuperación y catálogo se almacenan en SD. `FsStore::begin` monta FFat sin formatearla. La capacidad SD consulta `/sdcard` mediante `esp_vfs_fat_info`, sin regresar al error de confundirla con FFat.
6. **Hay escrituras flash fuera del aviso de realineación actual.** `equipos::save` escribe y renombra en FFat sin `needResync`. La nube usa rutas `/ffat` con escrituras POSIX y tampoco un aviso general de ese tipo. Confirmado y contrastado con el agente de pantalla. Se propusieron señales puntuales; el coordinador las difiere por la autorrecuperación del SDK y para acotar el cambio. Queda pendiente comprobar estas escrituras físicamente sin el reinicio periódico; no añadir reinicios en cada iteración.

## Reglas de audio revisadas

El código conserva 70 % total para familias específicas y 30 % para el comodín cuando ambos existen, repartiendo equitativamente entre familias específicas, no entre todos sus archivos. Fisher–Yates agota cada familia y evita repetir la última frase al comenzar otra ronda cuando hay más de una. La anulación tiene prioridad y no se mezcla con gol.

Los penaltis usan su comodín propio, no el gol general; empate se refiere al marcador del partido, no al de la tanda. Introducción, descanso y final son familias independientes. El gol ganador cambia a victoria sin esperar el final de una narración ni concatenar una frase larga. No se ha encontrado un fallo nuevo concreto en estas reglas durante esta revisión; las pruebas existentes deben seguir pasando tras integrar sonidos 0.12.

La máscara actual es `uint32_t` con 24 familias y comprobación estática <=32: quedan ocho posiciones. No hace falta ampliar a 64 bits por la cantidad de archivos. Si nuevas familias superan 32, cambiar solo el typedef sería incorrecto: hay desplazamientos `1u << p` que deben convertirse, junto con protocolos, preparador, instalador y pruebas.

## Contraste de propuestas y riesgos

| Propuesta | Valoración | Coste / riesgo / verificación |
|---|---|---|
| Reloj fijo a 12,5 MHz y retirar reinicios cada 250 ms | Corrección experimental acotada, coherente con la prueba privada | Sin aumentar búferes. El agente de pantalla verificó autorrecuperación por VSYNC en el SDK instalado. Falta panel físico con Wi-Fi/OTA: no afirmar que resuelve el incidente |
| Iniciar `startCheck` después de la precarga y primer Inicio presentado | Cambio acotado razonable | Reduce solape inicial entre precarga PSRAM y red/TLS; no elimina la presión durante red posterior. Sin tablas adicionales |
| Señalar escrituras FFat reales para realineación | Diferido por coordinación: primero validar autorrecuperación del SDK | Si hiciera falta, cubrir equipos/nube y errores/cierres; respetar qué tarea ejecuta el driver. Evitar nuevas dependencias circulares |
| Aclarar aviso del RTC como «Hora sin ajustar» | Seguro como cambio de texto | Una lectura válida no acredita hora correcta. No ajustar reloj ni reconstruir fechas |
| Comprobar errores de JSON/índice al guardar partido | Respaldado por reproducción del agente de motor | Reservar capacidad antes de I/O; comprobar `fprintf/ferror/fclose`, conservar JSON si falla índice y devolver error. Mantener `serviceAudio` y victoria inmediata; sin reintentos largos automáticos |
| Propagar fallo de guardado al llamador | Necesario para cerrar el problema de persistencia | Devolver false desde `save` no basta: el llamador original continúa NVS/contador. Acordar comportamiento y mensaje sin borrar datos ni presentar éxito |
| Catálogo activo en PSRAM + límite 2.048 | Diseño para coordinar con sonidos 0.12 | Libera los 4.200 B estáticos actuales, reserva 8.296 B PSRAM más metadatos. En recepción pueden coexistir activo, staging y comprobación: hasta 24.888 B de objetos, sin WAV |
| Inventario hasta 2.048 | Diseño para coordinar con sonidos 0.12 | 10.245 B temporales para 2.049 códigos/ubicaciones. Actualizar número de páginas/clientes y probar el límite; no resolver bloqueo aumentando solo el búfer |
| Aumentar DMA/búferes de panel o audio | No recomendado sin medida | Consume RAM interna escasa y no prueba la causa. La cantidad de WAV no exige más DMA |
| Tarea nueva de audio o cambio de núcleos | No justificado por esta auditoría | Añade pila, sincronización y competencia SD/PSRAM; requiere medidas previas de pausas y underrun |

Los cambios de código y documentación autorizados pueden prepararse sin nueva confirmación. Las mediciones e instalación física quedan fuera de esta revisión y deben seguir la autorización y coordinación del responsable de la placa; ninguna propuesta implica por sí sola permiso de grabación.

## Pruebas recomendadas y pendientes

- Regresión local de `s3_commentary_test`, `s3_commentary_events_test`, `s3_commentary_ambient_test`, `s3_sound_safety_test`, `s3_audio_resilience_test`, `s3_sound_chaos_test` y preparador/instalador, tras la integración de 0.12. Esta auditoría no los ha reejecutado ni atribuye resultados nuevos a la placa.
- Para capacidad: inventario con 1.024/1.025/1.060/2.048 WAV, última página y truncamiento explícito; activos/reservados separados; catálogo máximo y uno de más; reserva fallida; máscara de última familia; reinicio con catálogo antiguo y principal corrupto conservando respaldo.
- Verificar tamaño real del binario/DRAM y ubicación de reservas en la compilación objetivo; no deducir PSRAM de un comentario o de una prueba de escritorio.
- Medir máximo tiempo entre llamadas a `pump`, duración de apertura/lectura SD y guardado/NVS, bajo las cargas permitidas. Los 125 ms DMA son una reserva nominal, no una garantía ante un bloqueo mayor. Los timeouts de escritura no equivalen por sí mismos a cortes acústicos.
- Validación física conjunta: inicio, Wi-Fi/consulta, pantalla de actualización, gol/anulación y victoria; observar panel y escuchar audio. Capturas internas, CRC y respuesta USB no acreditan estabilidad visual o acústica.

## Estado de coordinación

Hallazgos compartidos y contrastados con pantalla y motor antes de cambios. El agente de motor conserva la propiedad del guardado e índice; el de pantalla, reloj/arranque/realineación. Capacidad, catálogo y herramientas de sonidos se dejan para contraste con la sesión 0.12, evitando modificar archivos compartidos durante su entrega.
