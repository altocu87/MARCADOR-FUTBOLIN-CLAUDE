# Auditoría y correcciones 0.13 · equipo B · 23/10/2026

## Situación de la entrega

Trabajo preparado en `equipo-b/auditoria-panel-0.13`, copia aislada de la sesión que añade sonidos. La autorización del dueño es integrar en main como 0.13 **después** de la entrega de sonidos 0.12. La integración y las comprobaciones finales se registrarán aquí al realizarlas. Esta auditoría no abre el puerto serie, no instala firmware ni crea partidos en la placa.

Última evidencia física comunicada al comenzar: prueba privada de 0.11 instalada; el dueño confirmó Inicio estable con reloj fijo. También comunica interferencias intermitentes, especialmente con Wi-Fi. No está cerrada la validación visual bajo carga. Una captura interna correcta y contadores a cero no certifican el panel.

## Resultado conjunto antes de modificar código

Tres agentes especializados revisaron pantalla/respuesta, memoria/audio y motor/calidad; intercambiaron hallazgos y acordaron el alcance antes de editar. El coordinador asignó archivos por separado y revisa integración y pruebas. Informes completos:

- [Panel, Wi-Fi y respuesta](AUDITORIA_PANEL_WIFI_20261023.md).
- [Memoria, almacenamiento y audio](AUDITORIA_MEMORIA_AUDIO_20261023.md).
- [Motor, historial y calidad](AUDITORIA_MOTOR_CALIDAD_20261023.md).

**Confirmado en código:** 0.11 cambiaba 12,5/10 MHz y solicitaba realineación cada 250 ms de red; la comprobación inicial coincidía con precarga de recursos. El aviso inferior dependía de la hora no válida, no de un diagnóstico de conexión. El guardado no propagaba fallos del índice. El historial podía inventar una segunda parte al ganar por goles en modo mixto y hacer retroceder el tiempo de eventos al entrar en penaltis.

**Probable:** el conjunto de control dinámico del panel participa en el fallo tras cargar. La prueba privada cambió frecuencia y periodicidad a la vez; no aisla cada causa. Contención de PSRAM y escrituras flash pueden contribuir a las interferencias restantes, según las limitaciones documentadas por [Espressif](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html).

**Pendiente de medir:** estabilidad física en Wi-Fi/OTA, tiempos máximos de lectura SD y audio, fluidez con fotos, y recuperación visual tras escrituras. No se ha diagnosticado alimentación defectuosa, avería de panel ni reinicios espontáneos.

## Cambios acotados, coste y recuperación

| Cambio | Efecto esperado y coste | Recuperación y límite |
|---|---|---|
| Reloj fijo, sin realineaciones periódicas de red | Evita alternancias de política; sin nuevos buffers. Conserva recuperación existente tras flash y progreso OTA | Incluido también en la entrega de sonidos; no duplicar ni atribuir dos arreglos distintos. Pendiente carga física |
| Consulta inicial después del primer Inicio presentado | Evita conexión simultánea con descompresión inicial; una bandera, unos segundos más tarde para novedades/hora | Revertible; siguen disponibles búsqueda y sincronización manuales |
| «Hora sin ajustar» | Aclara la advertencia; no modifica fecha ni credenciales, sin recursos de fuentes nuevos | Comprobar RTC y sincronización si persiste; no inventar fechas antiguas |
| Fallo de guardado visible y propagado | No confirmar contador/historial corto si falla archivo/índice; mantiene victoria inmediata y servicio de audio | Conserva JSON recuperable si falla índice; sin reintentos automáticos ni borrado de datos. No garantiza integridad ante corte de alimentación |
| Partes y tiempos del historial | Guarda solo partes jugadas; penaltis después de toda la prórroga | No migra ni reescribe partidos anteriores; comprobar fixtures |
| Núcleo ESP32 3.3.12 fijado en CI | Publicación con controlador conocido; sin coste de RAM en ejecución | Actualizar núcleo requiere recompilar y revalidar, especialmente panel; otras familias de placas conservan su configuración |

No se amplían buffers RGB/DMA ni se introduce una tarea de audio. No se alteran reglas 70/30, rondas por familia, prioridad de gol/anulación/fase, penalti separado, ambiente 30–60 s ni victoria inmediata. Los catálogos y WAV pertenecen a la entrega de sonidos; no se añaden audios privados a Git.

## Validación

Registro provisional, sustituir por resultados finales de la integración:

- Pruebas de aplicación: 315 correctas y una omitida que se ejecuta con el perfil nativo preparado. Tipos correctos.
- Prueba nativa de Ajustes con LVGL real en PC: correcta, incluye arranque sin conexión antes de Inicio, red sin reinicios periódicos, recuperación por NVS y aviso de hora.
- Compilación real ESP32 y suite nativa completa en curso. Simulación no equivale a prueba física.

## Decisiones y ensayos pendientes para el dueño

1. **Ensayo físico acompañado:** instalar tras comprobar que no está jugando, identificar puerto y arranque/particiones activas; observar carga, Inicio, búsquedas repetidas, fotos, QR y descarga. No basta USB/CAPTURA. Conservar NVS, FFat, SD y ambos historiales; no asumir dirección 0x10000.
2. **Si persisten interferencias:** preparar variante de núcleo con caché de 64 B y XIP de PSRAM, o comparar frecuencia fija menor. Son experimentos alternativos, con coste de PSRAM/CPU/fluidez y compilación del núcleo; no activar macros aisladas sobre bibliotecas precompiladas. Solicitar aprobación antes de instalar variantes.
3. **Capacidad de sonidos:** ampliar catálogo e inventario por separado, con almacenamiento explícito en PSRAM y pruebas de frontera; máscara de familias revisada junto con nuevas reglas, nunca por el nombre del WAV. Ver informe de memoria.
4. **Integridad avanzada:** recuperación automática del índice, persistencia transaccional de equipos/jugadores y protección ante cortes requieren diseño y pruebas adicionales. Esta entrega detecta fallos, no promete reparar todos los datos automáticamente.
5. **Torneos y Partido Loco:** no portar módulos completos sin presupuesto de memoria y pruebas. Partido Loco ya tiene reglas parciales; comparar lo implementado con la app. Existe trabajo de A en rama de torneos; no fusionarlo como parte de esta auditoría sin revisar su estado. La tanda tiene límite de 60 lanzamientos: decidir almacenamiento/continuación antes de extenderlo.

La reversión de código se hace con commits nuevos sobre esta rama o main; no resetear ramas de otros equipos. Una reversión publicada debe llevar una nueva versión 0.x. Para recuperación física, usar copia de firmware verificada y partición activa confirmada, sin borrar datos.
