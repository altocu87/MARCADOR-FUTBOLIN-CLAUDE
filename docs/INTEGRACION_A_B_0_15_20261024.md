# Integración A y B · 0.15 · 24/10/2026

Petición del dueño: instalar Torneos del equipo A, la rama optimizada y los sonidos nuevos que identifica como 0.13. Main de A ya usa 0.13 para nube y 0.14 para su aviso: la entrega conjunta usa **0.15**, sin alterar las entradas históricas de A. La entrega de audio de B conserva su nombre histórico `ENTREGA_SONIDOS_0_12_20261023.md`; no fue fusionada ni terminó de copiarse a SD.

## Fuentes que se reúnen

- A: `9f55ee5ecca8a18be2702af0a4ffa05c9112e520`, fase 3, que contiene main `ab72266` y los torneos `a9f36e1`.
- B: `d9c82dcf5adbf3c269ca1c2f91739e10820b6582`: 210 WAV nuevos preparados y siete grupos nuevos, más las pruebas y el ritmo fijo del panel confirmado por el dueño en la prueba privada de 0.11.
- Se conservan Torneos, guardado y sincronización en reposo de A, los ganchos de realineado tras flash/fotos y la reversión de fase 3. Se retiran los cambios dinámicos de frecuencia y reinicios de panel cada 250 ms, coincidentes con el fallo anterior; no se da por demostrada una causa única.
- La compilación normal y la fase 3 comparten reglas y sonidos. Solo fase 3 recompila el SDK con XIP/ISR en IRAM/realineado vertical/caché 64 B y reduce la caché de imágenes de 8 a 6 MB.

## Sonidos y límites

Paquete preparado: **770 WAV, 706 activos y 64 reservados de torneo**. Los 560 anteriores conservan códigos y contenido; 210 añadidos: 48 anulaciones, 65 ambiente, 30 segunda parte, 20 ventaja exacta de tres, 25 goles comodín, cinco gol de oro, 15 paso a prórroga y dos paso a tanda. La incorporación de Torneos no activa por sí sola los 64 reservados: el selector de contexto de torneo aún está pendiente en A; no se hacen sonar frases de eliminatoria en situaciones sin verificar.

Se mantienen las rondas sin repetición, 70 % contexto / 30 % comodín cuando corresponde, ambiente cada 30–60 s de juego y prioridad de gol/anulación/fase. El límite sigue siendo 1024 referencias y 31 grupos. El instalador exige al menos 0.15 para grupos 24–30: la 0.12 de A es Torneos y no entiende esos grupos.

## Estado físico inicial

COM6 consultado el 24/10: firmware de B 0.12, SD no detectada, catálogo e historial no visibles. El 23/10 antes de ese fallo se leyeron cinco partidos y 560 WAV. No se ha formateado ni borrado la SD; no afirmar que esos datos se han comprobado conservados hasta montarla y verificarlos. El dueño deja para después desconectar toda la alimentación durante 10 s.

Código integrado, compilación, instalación y prueba física son estados distintos. Completar debajo con los resultados reales antes de dar la entrega por instalada o comprobada. No publicar WAV ni copias privadas de NVS/flash.

## Comprobaciones del 24/10

- Fuentes de programa y configuración congeladas en `d18fd295876b42bb97e2ccfcf5b0e25e8875ae95`.
- GitHub: Pruebas `40073548231`, Firmware normal `40073548218` y Fase 3 `40073548263`, correctos. El artefacto SDK de fase 3 confirma las cuatro opciones activas, no solo los indicadores del código.
- PC: 348 pruebas de aplicación, tipos, 11 de clasificación y 35 escenarios del instalador correctos. También pasan las pruebas nativas de las fases nuevas, ajustes, sonidos seguros, Partido Loco, diagnóstico USB, perfil QR y pantallas de torneo. El recorrido completo de Linux pasó en GitHub. En Windows se adaptó la llamada `mkdir` del test de torneos; el cambio no toca el programa de la placa.
- Historial de versiones anterior de A comparado como datos: idéntico. No se sustituyen las entradas 0.12, 0.13 ni 0.14.
- La lectura USB grande de la flash se interrumpe. Se guardaron privadamente los primeros 64 KiB y se verificó contra la placa el binario anterior de B 0.12 completo mediante su resumen MD5. **No hay copia nueva completa de toda la flash**. No se modifica el cargador, la tabla de particiones, NVS ni FFat al instalar: solo un programa y la entrada de arranque más antigua, conservando la otra.

### Primera instalación: normal con Torneos

Normal 0.15 build 90, 2.005.296 bytes, SHA-256 `83d2886aaa08fd031562ca8d26fc613a00c310c12f79c42370ea271ba29d7e98`. Grabada en app0 (`0x10000`), verificación de escritura correcta y arranque secuencia 5. El programa previo de B quedó inicialmente entero en app1.

Tras arrancar responde como 0.15 y **vuelve a detectar la misma SD de 8.036.253.696 bytes**. Los cinco identificadores, fechas y marcadores del historial coinciden con la lectura anterior al incidente; dos siguen sin fecha. Catálogo anterior: 496 activos, sin activar aún los nuevos. No fue necesario formatear ni se da por demostrada la causa de la recuperación de la SD.

Lectura en Inicio: RAM interna libre 18.068 bytes, mayor bloque 8.692 y mínimo 8.080; PSRAM libre 5.964.384. Cero errores de audio/cuadro/espera vertical. **La memoria interna es ajustada**: comparar con fase 3 y medir en torneos reales; estas cifras no prueban ausencia de agotamiento durante un torneo largo. La captura/USB no sustituye la confirmación visual del dueño.

La compilación normal declara 171.312 bytes globales, frente a 136.368 de B 0.12: +34.944. En el ELF de fase 3 siguen existiendo reservas estáticas de torneo (`tor::buildKind()::fits`, 3.480; `tor::draft`, 3.692; `tor::selIds`, 2.560; `tor::buildDraft()::fixed`, 2.560; `tor::opts`, 1.920 bytes). **XIP no traslada automáticamente esas tablas a PSRAM**. Primera mejora posterior sugerida a A: revisar cuáles pueden reservarse explícitamente en PSRAM con fallo controlado. No se modifica su almacenamiento durante esta instalación; antes se mide la candidata optimizada.

En curso: copia y relectura de los 770 WAV, activación de 706, instalación de la optimizada en app1 y comprobación posterior. Hasta anotarlo expresamente, no dar esos pasos por terminados.
