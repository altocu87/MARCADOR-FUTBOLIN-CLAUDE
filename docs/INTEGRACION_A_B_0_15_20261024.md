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
