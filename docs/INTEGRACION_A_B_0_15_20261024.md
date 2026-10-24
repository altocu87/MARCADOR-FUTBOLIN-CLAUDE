# Integración A y B · 0.15 · 24/10/2026

Petición del dueño: instalar Torneos del equipo A, la rama optimizada y los sonidos nuevos que identifica como 0.13. Main de A ya usa 0.13 para nube y 0.14 para su aviso: la entrega conjunta usa **0.15**, sin alterar las entradas históricas de A. La entrega de audio de B conserva su nombre histórico `ENTREGA_SONIDOS_0_12_20261023.md`; no fue fusionada ni terminó de copiarse a SD.

**Resultado vigente:** instalada la **normal 0.15 corregida**, con Torneos, 770 WAV verificados y tablas de interfaz reservadas explícitamente en PSRAM. El dueño confirma que el menú está estable. **La fase 3 se instaló, se midió y se retiró:** su mínimo de RAM interna fue 3.220 bytes, frente a 35.208 en la comprobación final de la normal. No promover el SDK experimental a firmware oficial. La publicación normal usa Arduino; fase 3 solo produce un artefacto manual de prueba.

Los apartados siguientes registran también las candidatas descartadas, para que A no repita esos experimentos ni confunda sus cifras con las de la placa que queda instalada.

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

Código integrado, compilación, instalación y prueba física son estados distintos. Los resultados completados constan debajo. No publicar WAV ni copias privadas de NVS/flash.

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

La primera compilación normal declaraba 171.312 bytes globales, frente a 136.368 de B 0.12: +34.944. En su ELF de fase 3 seguían las reservas estáticas de torneo (`tor::buildKind()::fits`, 3.480; `tor::draft`, 3.692; `tor::selIds`, 2.560; `tor::buildDraft()::fixed`, 2.560; `tor::opts`, 1.920 bytes). **XIP no trasladaba automáticamente esas tablas a PSRAM**. Este hallazgo llevó a la corrección explícita descrita al final; estas cifras son anteriores a ella.

La copia, relectura, activación y comparación física terminaron después; ver los resultados siguientes.

### Paquete instalado y primera prueba de fase 3

**Copia terminada el 24/10 a las 07:59 UTC:** 770 archivos, 184.945.256 bytes de audio, 706 activos / 64 reservados. Los 560 anteriores conservados; 210 añadidos, cero reintentos. Todos los códigos tienen al menos dos lecturas de tamaño y CRC completo coincidentes con el paquete local. CRC del catálogo `c9dd6523`; diario privado SHA-256 `d11bad438aef63f267e014284c61a740ea5b0695d60af12c5813de6b3af86067`. Tras reiniciar la normal siguen los cinco partidos, la misma SD y los 31 grupos correctos.

**Primera optimizada instalada y rechazada para jugar:** binario SHA-256 `11f5ea8d89df6b57d02432953baef586375ec1fc89f30ea2743bee580769e2b2`, app1, secuencia 6. Arranca, SD e historial correctos, 706 voces cargadas, pero RAM interna libre 5.896 bytes (mayor 5.108, mínimo 596); PSRAM libre 3.945.412 y caché de imágenes observada 7.123 KiB. El presupuesto de caché de 6 MiB es blando, no garantiza que Inicio quede debajo de esa cifra.

Se probó **VOLVER_ANTERIOR** por USB, sin partido: vuelve a la normal de app0; tabla de arranque leída y comprobada (secuencia 7, estado válido). Conserva datos y catálogo. En esa lectura la normal tenía 8.608 bytes libres, mayor 7.668 y mínimo 8.252: el margen varía con el trabajo de arranque, no presentar una sola lectura como garantía de estabilidad.

### Experimento de BSS externa (descartado)

**No usar esta variante.** El ELF confirmó 47.408 bytes de tablas en PSRAM, pero en la placa produjo una excepción doble antes de responder a HELLO. La causa exacta no está aislada; no se atribuye una incompatibilidad general a la técnica. Se recuperó la normal por USB cambiando solo la entrada de arranque a secuencia 9. Las fuentes actuales deshabilitan explícitamente la BSS externa y usan reservas tras arrancar.

El experimento habilitaba `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y` y aplicaba `MFV3_UI_DATA` (`EXT_RAM_BSS_ATTR`) a tablas persistentes. No se trasladaban catálogo/selector de audio, motor, DMA ni datos de interrupciones. La ubicación correcta en el ELF no bastó para acreditar un arranque correcto.

La técnica está descrita en la [documentación de Espressif para IDF 5.5](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32s3/api-guides/external-ram.html#allow-bss-segment-to-be-placed-in-external-memory). El cambio de configuración también permite que IDF coloque en PSRAM las secciones BSS de sus componentes de red previstos para ello.

El guard del ELF de ese experimento detectaba la ubicación pero no la excepción física. Ha sido sustituido por `scripts/check-ui-memory.py`, que rechaza BSS externa en la nueva variante. No publicar los binarios experimentales de los párrafos anteriores.

### Solución actual: reservas explícitas tras el arranque

Fuentes `b570c8a442d5d831172759247394cd4d911b6bdd`. `s3mem::ExternalArray` reserva cada tabla una sola vez con `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` y verifica que el puntero sea externo. Preserva inicializadores y duración de los elementos. No utiliza almacenamiento externo estático, ni reserva tablas antes de arrancar. Mantiene jugadores, motor y selector de audio en RAM interna. Se aplica también a la compilación normal, sin requerir un SDK nuevo para esa mejora.

Se reserva antes de cambiar a Torneos, al abrir Resumen/Parejas y al leer la lista de fotos. Ante falta de PSRAM, se mantiene el puntero vacío y el llamador muestra un aviso; no se cambian datos guardados. Las tablas permanecen para los siguientes usos. No se añade trabajo al botón del gol.

Prueba de unidad: fallo recuperable, inicializadores, matrices de caracteres, reintento y ausencia de nuevas reservas al repetir. Torneos, fases de sonido, menús y fechas/resumen pasan; GitHub Pruebas y Firmware de `b570c8a` correctos. El guard del ELF exige que los antiguos bloques grandes sean ahora manejadores pequeños y que motor/jugadores/catálogo sigan internos. `MEMORIA` añade el número y bytes de tablas ya reservadas en PSRAM.

**Normal corregida instalada:** app1, secuencia inicial 10, 2.009.360 bytes, SHA-256 `af5003704ed6db103f89583d106e13ea47dde01e60a6816b2c948119f0893463`. En Inicio: interna libre **46.048**, mayor **27.636**, mínimo **35.388**; PSRAM libre 5.964.772. Cinco partidos, SD y 706 voces correctos. El recorrido Inicio → Torneos → Resumen → Parejas → Inicio conserva ese mínimo y el mayor bloque de 27.636. Se reservan 24.312 bytes en nueve tablas al recorrer esos menús. No se crea ni modifica un torneo o partido.

**Fase 3 con las mismas reservas, probada y retirada:** 2.074.944 bytes, SHA-256 `f79747ab46b0bb2a716b3fb0a40c5990c3cf3bfc39b53b311b1090d978eaaf8d`, app0, secuencia 11. Arranca, carga SD y catálogo y completa el mismo recorrido sin errores registrados. Sin embargo, el mínimo interno cae a **3.220 bytes**, con 10.928 libres al volver a Inicio. No se produjo un reinicio en este recorrido; el margen se considera insuficiente para recomendar juego o torneos. Se vuelve por `VOLVER_ANTERIOR` a la normal de app1. El efecto exacto de cada ajuste del SDK sigue pendiente de aislar; no atribuir el consumo a una opción concreta sin medirla por separado.

### Estado final comprobado (11:06, hora de Madrid)

| Variante | Interna libre en Inicio | Mayor bloque | Mínimo interno observado | Decisión |
| --- | ---: | ---: | ---: | --- |
| Primera normal, antes de mover tablas | 18.068 B | 8.692 B | 8.080 B | Sustituida |
| Normal corregida, última lectura tras volver | 46.024 B | 27.636 B | 35.208 B | **Instalada y recomendada para la prueba de juego** |
| Fase 3 corregida, después del mismo recorrido | 10.928 B | 7.668 B | 3.220 B | Retirada; solo experimental |

El ahorro estático comprobado en el ELF es **28.112 bytes internos**: 28.152 de contenido pasan a PSRAM y quedan 40 de manejadores. No interpretar las diferencias de memoria libre entre arranques como una medición aislada de ese ahorro: también cambia el trabajo de arranque.

Última lectura de la normal: `HELLO waveshare-7c 0.15`, PSRAM libre 5.964.836 bytes, mayor 5.898.228, caché de imágenes 7.123 KiB. SD disponible, mismo volumen de 8.036.253.696 bytes, usados 188.534.784, libres 7.847.718.912. Cinco partidos con los mismos identificadores, resultados y fechas; dos mantienen fecha cero. Reloj válido por RTC sin ajustarlo. Catálogo: **706 activos**, 31 grupos, límite 1024. Diagnóstico: cero errores de audio, cuadro y espera vertical. Confirmación física del dueño: **«El menú está estable»**.

Validación de las fuentes `b570c8a`: GitHub [Pruebas](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/40079092821), [Pruebas del PR](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/40079095331), [Firmware normal](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/40079092717) y [Fase 3](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/40079092716), correctos. Los cambios posteriores a ese commit son documentación. La publicación desde main recompila y puede tener otro contador de build y hash: el binario realmente instalado es el identificado arriba, no confundirlo con una actualización posterior aún no instalada.

### Pendientes para A y siguiente prueba

- Jugar y escuchar: no se ha medido aún latencia acústica, cortes con goles/anulaciones, un torneo completo, ni estabilidad durante horas con esta 0.15. Los CRC demuestran integridad del archivo, no la pertinencia de cada frase ni su reproducción por el altavoz.
- Activar los **64 reservados de torneo** con un contexto fiable del motor de A, sus pruebas y una nueva versión; siguen en SD pero no se eligen automáticamente.
- Revisar en una rama la memoria del SDK con mapa de enlace y medidas por opción, antes de volver a instalarlo. La reversión automática no cubrió el fallo anterior a `onBoot`; conservar siempre recuperación por USB y una normal comprobada.
- Ampliación futura: 1024 referencias y máscara de 32 bits con 31 grupos usados. Otros 500 archivos superarían el límite actual de referencias; diseñar y probar el cambio antes de importarlos, sin cargar todos los WAV en RAM.
- No volver a tocar el USB en esta entrega: se deja la placa lista para que el dueño pruebe jugando. No se han inventado fechas ni añadido partidas para medir rendimiento.
