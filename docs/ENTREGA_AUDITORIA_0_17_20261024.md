# Auditoría conjunta y correcciones 0.17 · 24/10/2026

## Alcance y procedencia

El dueño autorizó corregir lo factible, integrar en main después de los nuevos sonidos y añadir Torneos a la auditoría. Durante la pausa, A/B publicaron 0.15 con Torneos, sincronización en reposo y tablas de interfaz en PSRAM. Otra sesión instaló 0.16 con 100 WAV más. Por eso esta entrega usa **0.17**, conservando las entradas anteriores y `majorOneAuthorized=false`. El borrador de auditoría 0.13 quedó conservado en `785f33e`, sin publicarlo ni volver a código antiguo.

Base remota revisada: `6c20dbf`. Se incorporó una copia de las fuentes y documentos de 0.16 del directorio `integracion-torneos-sonidos`, cuya sesión estaba terminada, sin modificar esos originales. Huella SHA256 del parche de fuentes: `eac8134a647d568286f76707acb1cf6ec04fa1732675abbe18d64eea47ddb2b7`. Se añadieron sus tres archivos nuevos de entrega, inventario y pruebas. El commit `84ee12a` conserva ese conjunto sobre la base actual y el primer arreglo de empates. No se añaden WAV, copias de flash, NVS ni registros privados a Git.

Los tres agentes especializados contrastaron los hallazgos antes de editar y revisaron la integración. Informes por especialidad:

- [Panel, Wi-Fi y respuesta](AUDITORIA_PANEL_WIFI_20261024.md).
- [Memoria, almacenamiento y audio](AUDITORIA_MEMORIA_AUDIO_20261024.md).
- [Motor, guardado y torneos](AUDITORIA_MOTOR_TORNEOS_20261024.md).

## Estado físico y límites de la evidencia

**Esta sesión prepara código y publicación; no graba ni consulta USB.** Según la entrega de la otra sesión, la placa tiene instalada 0.16, 870 WAV verificados, 806 voces activas y 64 reservadas, con los cinco partidos conservados tras reiniciar. Dos antiguos siguen sin fecha; no se les asigna una inventada. Escucha jugando y torneo completo pendientes.

La confirmación visual del menú con reloj fijo es válida para ese escenario. Capturas internas, archivos íntegros y contadores a cero no prueban estabilidad del panel físico bajo Wi-Fi. La fase 3 con SDK propio **ya se probó y se retiró por falta de margen de RAM interna**: mínimo 3.220 B en la comprobación documentada. Esta entrega conserva el núcleo normal y las reservas explícitas de interfaz de 0.15. No repite ni promueve la variante experimental.

## Diagnóstico contrastado

**Confirmado:** búsqueda inicial de actualizaciones antes de terminar precarga; aviso «conecta Wi-Fi» condicionado por hora no válida; errores de archivo/índice no propagados al contador; exportación de una segunda parte no jugada y tiempos de tanda atrasados; audio de victoria ante empate de liga; éxito de algunas acciones de torneo sin comprobar guardado, y borrado de partido antes de confirmar el torneo al deshacer.

**Probable:** la contención entre pantalla, red y memoria contribuye a las interferencias residuales. La antigua prueba fija cambió frecuencia y reinicios a la vez, por lo que no aisló una causa única. La política fija, recuperación de flash y sincronización en reposo ya estaban incorporadas por A/B.

**Pendiente de medir:** imagen física durante Wi-Fi/descargas, latencias máximas de lectura SD y mando, audio durante guardado y una competición completa. No se atribuyen reinicios a watchdog, alimentación o avería sin registros que lo acrediten.

## Cambios, coste y recuperación

| Cambio | Efecto y coste | Recuperación / límite |
|---|---|---|
| Consulta automática tras el primer Inicio presentado | Evita conexión simultánea con la descompresión inicial. Una bandera; no nuevos buffers | Conserva búsqueda manual, sincronización en reposo y cancelación al tocar |
| Aviso «Hora sin ajustar» y contador de recuperación del panel | Aclara el reloj y registra peticiones rechazadas por el controlador; unos pocos bytes globales y 40 B más de pila solo al consultar diagnóstico | No ajusta fecha ni credenciales; aceptar recuperación no demuestra imagen correcta |
| Guardado comprueba JSON e índice | Reserva la fila antes de escribir; ante fallo no actualiza el contador corto/NVS ni anuncia éxito a la nube. Sigue atendiendo audio | Conserva JSON recuperable si falla índice; sin reintentos automáticos que dupliquen archivos |
| Torneo pendiente y reintento explícito | Si el resultado ya se aplicó en RAM, volver solo reintenta guardarlo. No vuelve a sumar victorias ni confirma ceremonia antes de persistir | Aviso visible; no promete conservar una mutación que siga pendiente si se corta la alimentación |
| Deshacer y cambiar reglas comprueban persistencia | Copia temporal profunda en PSRAM para restaurar el estado al fallar; guardar torneo antes de borrar el partido. Sin reservas en el gol | Si falta PSRAM no se realiza la operación; si falla borrar el historial se avisa, sin anunciar un borrado completo |
| Empates usan aviso neutro | Respeta sonido propio/silencio y evita voces de victoria o final con ganador. Sin nuevas familias ni buffers | Victorias y voces de tanda/campeonato conservan su tratamiento |
| Exportación de partes y tiempos | Solo partes realmente jugadas; la tanda usa todo el tiempo de la prórroga | No migra ni reescribe partidos antiguos; preserva IDs de torneo, cruce y empate |
| Arduino ESP32 3.3.12 fijado en CI normal | Controlador reproducible respecto al núcleo revisado; sin coste en ejecución | Cambiar núcleo exige recompilar y comprobar panel/memoria; fase 3 permanece separada |

No se amplían DMA ni buffers RGB. Se conservan mezcla 70/30, bolsas por familia sin repetición, penaltis separados, prioridad de gol/anulación/fase, ambiente de 30–60 segundos y victoria inmediata. Los archivos WAV se reproducen por bloques. La selección automática no recorre directorios ni abre candidatos al elegir.

## Validación y publicación

- `npm test`: 348 correctas; la prueba de perfil nativo omitida en esa ejecución pasa aparte en la batería de hardware. Tipos correctos.
- `hardware/test/check_copies.sh`: batería completa correcta en Windows con LVGL real en el PC, tres variantes de pantalla, mando, torneos cruzados con la app, fotos/fuentes, sincronización en reposo, audio, USB, prototipos Arduino y las cuatro regresiones nuevas. La primera ejecución detectó una precondición inválida en el fixture sintético de una final tras deshacer; se corrigió el fixture y la batería completa volvió a pasar. No se ocultó la aserción ni se cambió producción para hacerla pasar.
- Instalador: 38 casos con transporte ficticio, sin puertos reales. Preparador: 11 pruebas dentro de la batería nativa. Versiones y derivados: `native-release.mjs check` correcto.
- Compilación real: Arduino ESP32 3.3.12 normal, S3 OPI/32 MB, sin SDK experimental. Programa 2.014.063 B; binario 2.014.208 B, SHA256 `1f4d269451c56b135ad44a54b9889488c940704879c3e8955dfb0d6d1aa69ba0`. Globales 143.272 B: 8 más que la compilación 0.16 documentada. No interpretar el espacio indicado por el enlazador como memoria libre real durante el juego.
- `check-ui-memory.py` sobre el ELF: correcto, conserva 28.112 B internos recuperados; tablas de interfaz diferidas, catálogo y motor en su ubicación prevista. El snapshot de gestión mide 210.200 B en el ensayo del PC y se libera al salir; no es una reserva permanente ni una medida física del ESP32.

Las comprobaciones remotas de GitHub y el resultado de la integración se registrarán al completarse. No se ha instalado este binario ni provocado fallos sobre los datos reales del dueño. Estos resultados no sustituyen la observación física.

## Trabajo posterior que requiere una decisión o ensayo acompañado

1. **Instalar y medir 0.17:** confirmar que nadie juega, puerto y particiones/arranque activos; explicar binario y destino antes de grabar. Observar físicamente carga, conexión, reposo/sincronización, fotos, partido, tanda y torneo. Conservar NVS, FFat, SD y datos.
2. **Capacidad para más sonidos:** 870 archivos dejan 154 plazas del inventario; 806 activos dejan 218 del catálogo. Añadir otros 500 superaría el límite. Diseño propuesto: 2.048 referencias, catálogo y temporales explícitos en PSRAM, cambios coordinados de firmware, inventario e instaladores y pruebas de reserva fallida/rollback. La máscara de 64 bits ya admite los 39 grupos actuales. No ampliar tablas internas sin resolver su ubicación y el guard del ELF.
3. **Integridad avanzada:** reconstrucción y recuperación del índice tras corte eléctrico, operación conjunta de varios archivos y recuperación de JSON huérfanos. Esta entrega detecta fallos y evita falsas confirmaciones; no convierte toda la SD en una base transaccional.
4. **Límites del motor:** 60 lanzamientos de tanda y 120 registros de gol, incluidos anulados; decidir representación y avisos antes de extenderlos. Revisar también anulación después de terminar y cronología detallada del modo Loco como funciones independientes.
5. **Si persiste el fallo físico:** medir una alternativa concreta al controlador normal antes de adoptarla. No repetir fase 3 ya descartada ni atribuir el problema a alimentación/cableado por suposición.

Reversión: commits nuevos que retiren el cambio concreto; una nueva publicación debe conservar numeración creciente 0.x. Para recuperar físicamente, firmware verificado y arranque confirmado, sin borrar toda la flash ni asumir siempre `0x10000`.
