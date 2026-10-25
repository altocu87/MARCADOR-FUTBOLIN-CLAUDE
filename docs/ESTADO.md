# Estado del proyecto

## Opciones en la ficha del jugador · 0.22 · equipo B · 25/10/2026

**Hecha y probada en la placa (COM6) con una compilación local, recorrida por USB con `TOQUE`/`captura` (no con el dedo).** En la ficha, «··· OPCIONES» arriba a la derecha abre una ventana como la de la app: VER QR, EDITAR (nombre y foto), TÍTULO Y MELODÍA (título bajo el nombre y melodía de victoria, que suena al tocarla) y DESACTIVAR/ACTIVAR; volver de cada una lleva a la ficha como estaba. Un jugador de baja sigue en la placa con «INACTIVO», fuera de elegir jugadores, torneos y retos. La placa guarda ahora el apodo, el título elegido, la melodía y la baja, y la primera vez baja todos los jugadores de la cuenta: subir un jugador cambiado aquí ya no borra en la cuenta lo puesto en la app. Pruebas cruzadas nuevas: títulos (`esperado-titulos.txt`, 17 jugadores iguales) y melodías. Medido en la placa tras reiniciar: 45 KB libres en el menú; recorriendo cinco veces Opciones, Título y melodía, Editar, cara a cara, Logros e Historial, **mínima interna 28,4 KB**; con el QR publicándose por Wi-Fi bajó una vez a 27,0 KB. En la placa no se tocaron títulos ni melodías de los jugadores reales (sí en el PC); José se dio de baja y de alta y quedó como estaba. Diferencias con la app: sin iconos en los botones; EDITAR no cambia el apodo ni junta jugadores repetidos; la melodía solo suena al elegirla (al ganar suenan las voces de siempre). **Publicada (PR 33 y PR 34, Firmware #115; la Elecrow de 4 MB ya no cabe y se queda en el instalador con la 0.21) e instalada en COM6 con permiso del dueño (25/10/2026): archivo de la web, tamaño y MD5 comprobados; arranca como 0.22, OPCIONES abre bien, mínima interna 29,8 KB tras abrir la ficha.** El otro hueco guarda una compilación local de prueba de la 0.22 (la 0.21 publicada ya no está en la placa). [Notas](HISTORIAL_ENTREGAS.md) · `docs/comparar/opciones-ficha-app-placa.png`, `opciones-placa-real.png`.

## Cara a cara en la placa · 0.21 · equipo B · 24/10/2026

**Fusionada en main y publicada como actualización (PR 30, Firmware #111, autorización del dueño 25/10/2026). Probada en la placa (25/10/2026) con una compilación local idéntica grabada por USB en COM6 (hueco de prueba; la anterior se conserva): abre, vuelve y abre la ficha del rival con los partidos reales; mínima interna 30,5 KB tras 24 aperturas (umbral ~27 KB), sin pérdidas. Después, con permiso del dueño, **0.21 publicada (build 111, archivo de la web, tamaño y MD5 comprobados) instalada por USB en COM6** (hueco app0; app1 guarda la compilación local): arranca como 0.21, cara a cara correcto, mínima interna 30,8 KB.** En la ficha del jugador, pestaña RIVALES, tocar el rival favorito, la némesis o una fila de «Enfrentamientos directos» abre CARA A CARA (`ui_duelo.h`), como `DuelScreen.tsx` de la app: fotos y nombres, victorias de cada uno, enfrentamientos y empates, y ELO, nivel, partidos, victorias, % victorias, mejor racha, diferencia de goles y logros lado a lado (el mejor en verde). Volver lleva a la ficha como estaba (en RIVALES); tocar al rival abre su ficha. Los compañeros no se tocan. La app no lista partidos en esta pantalla, así que la placa tampoco. Sin cuentas nuevas: usa `perfil::stats` (el cara a cara es la fila del rival, que cuenta lo mismo que `duel()` de la app) y la progresión. La tabla es una sola pieza dibujada y sus datos van en la PSRAM. Compilación real correcta en GitHub (Firmware #110, todas las placas; Pruebas #494). [Notas](HISTORIAL_ENTREGAS.md) · `docs/comparar/cara-a-cara-app-placa.png`, `cara-a-cara-placa-real.png`.

## Retos y ficha del jugador · 0.19 y 0.20 · 24/10/2026

**0.20 instalada por USB en COM6 y probada en la placa** (capturas por USB, no visto con el dedo): Retos, ficha desde Retos y desde el Ranking, sus seis pestañas, abrir un partido del Historial y volver; partidos del dueño conservados. RETOS: el reto de hoy y los tres de la semana con el progreso de cada jugador. FICHA de cada jugador: tarjeta con nivel, título, experiencia y rango; pestañas General, Hábitos, Rivales, Historial (abre el detalle del partido), Torneos y Logros. La 0.19 (publicada antes) dejaba la memoria interna casi a cero al recorrer Logros (108 bytes mínimo): en la 0.20 Logros e Historial se dibujan como una sola pieza y la pestaña vieja se borra antes de crear la nueva; medido en la placa: 44 KB libres en Logros e Historial y mínimo de 27,9 KB al abrir la ficha (antes de la ficha el mínimo era ~35 KB). Diferencias con la app: sin «Opciones» (el QR sigue en AJUSTES), sin cara a cara, y rangos, escudos e iconos de logro dibujados. La Elecrow de 4 MB vuelve a caber (sin el dibujo RGB565 intercambiado de LVGL), con unos 10 KB libres: la próxima función grande no cabrá sin otra solución. Publicación e instalación autorizadas por el dueño (24/10/2026). [Notas](HISTORIAL_ENTREGAS.md) · `docs/comparar/retos-app-placa.png`, `ficha-app-placa.png`.

## Mando sin bloquear la pantalla · 0.18 · 24/10/2026

**0.17 publicada instalada por USB en COM6 a petición del dueño (archivo de la web, MD5 comprobado; la anterior queda en el otro hueco). El arreglo de abajo está hecho, probado en el PC y publicado como 0.18; sin instalar en la placa.** Su mando (C3-Zero, programa 54, que no dice su modelo) estaba marcado como Super Mini: cambiado a C3-ZERO en la placa. Diagnóstico en la placa: la descarga del programa del mando y la radio funcionan y llega a «Mantén pulsados los dos botones». Dos fallos corregidos: (1) un fallo del mando paraba también la pantalla; ahora el mando se queda como estaba y la pantalla se actualiza igual; (2) «Necesita una primera actualización por cable» salía en cuanto se perdían saludos del mando durante 90 s; ahora solo sale si en más de un minuto oyéndole nunca ha dicho su versión, y si no se usa la última que dijo. Publicado como 0.18 con autorización del dueño (24/10/2026). Pendiente: probar en la placa la actualización por Wi-Fi y los botones del mando.

## Auditoría conjunta · 0.17 · equipo B · 24/10/2026

**Código de la auditoría preparado; 0.17 sin instalar en la placa.** Reúne Torneos y sincronización en reposo de 0.15, las 870 voces/archivos de la entrega 0.16 (806 activos y 64 reservados) y correcciones de arranque, aviso de hora, audio de empate, errores al guardar, reintento explícito de torneo y deshacer con recuperación del estado anterior. Conserva el núcleo normal; no promueve la fase 3 retirada por poco margen de RAM. Tres agentes contrastaron el diagnóstico y las pruebas. Publicación en main autorizada por el dueño en el chat de auditoría, después de los nuevos sonidos. La evidencia física vigente procede de la entrega 0.16 de abajo; estabilidad jugando con Wi-Fi, acústica y torneo completo siguen pendientes. [Entrega, validación y decisiones pendientes](ENTREGA_AUDITORIA_0_17_20261024.md).

## Sonidos de tanda · 0.16 · 24/10/2026

**Instalada y verificada en COM6 por petición del dueño.** 870 WAV en SD: 100 nuevos y 770 conservados; 806 activos y 64 previamente reservados. Avisos de lanzamiento decisivo, primer tiro de toda la tanda, goles, fallos, victoria y campeonato; 14 comentarios aleatorios añadidos al ambiente del partido. Catálogo recargado tras reinicio, cinco partidos idénticos, sin errores de audio. Escucha jugando pendiente; no publicada en main/OTA. [Entrega y límites](ENTREGA_SONIDOS_0_16_20261024.md).

## Integración A/B · 0.15 · 24/10/2026

**Normal 0.15 corregida instalada y comprobada en COM6; el dueño confirma menú estable.** Contiene Torneos y sincronización en reposo de A, sonidos de B y tablas de interfaz reservadas tras arrancar en PSRAM: ahorra 28.112 bytes internos estáticos, sin añadir reservas al gol. Última lectura: 46.024 bytes internos libres, mínimo 35.208. SD recuperada sin formatear; **770 WAV verificados, 706 activos y 64 de torneo aún reservados**, cinco partidos conservados (dos siguen sin fecha). La fase 3 se instaló y probó, pero se **retiró por mínimo interno de 3.220 bytes**: no promoverla a publicación normal. La placa queda con Arduino normal; pruebas de juego, acústica y torneo completo pendientes. Fuentes probadas `b570c8a`, publicación mediante [PR 25](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/pull/25). Versiones anteriores de A conservadas; el paquete llamado sonidos 0.13 se entrega como 0.15 porque A ya usó 0.14. [Informe para A, binarios, medidas y límites](INTEGRACION_A_B_0_15_20261024.md).

Las secciones fechadas anteriores que siguen debajo son historial; el estado físico vigente es el de esta primera sección.

## Fase 3 · compilación con sdkconfig propio · rama `equipo-a/fase3-sdkconfig` · 23/10/2026

**Compilada en GitHub (workflow «Fase 3 (prueba)»); sin instalar ni probar en la placa; no publicada.** Misma 0.14 y Arduino 3.3.12, con XIP desde PSRAM, ISR del panel en IRAM, realineado en cada retorno y caché de 64 B. Se instala en el hueco libre con `placa.py probar` y se vuelve con el botón «VOLVER A LA ANTERIOR», `placa.py volver` o sola tras tres fallos seguidos. Pasos en [PRUEBA_FASE3](PRUEBA_FASE3.md). Pendiente: probarla en la placa.

## Pantalla 0.14 · aviso «sincronizando» en reposo · equipo A · 23/10/2026

**Código hecho y probado en el PC; sin instalar ni probar en la placa.** Petición del dueño: mientras sube a la nube en reposo, el reloj pone abajo «SINCRONIZANDO · NO TOQUES LA PANTALLA» en lugar de «Toca para despertar»; sin reloj (placa sin hora o sin atenuar), una franja abajo con el mismo texto. Tocar sigue cortando la subida (jugar va primero). Pendiente de autorización para fusionar en main.

## Pantalla 0.13 y app · nube solo en reposo y realineado · equipo A · 23/10/2026

**Código hecho y probado en el PC; sin instalar ni probar en la placa.** Auditoría de cinco perfiles (pantalla, red, datos, electrónica, web) sobre los saltos de imagen con Wi-Fi y cargas. Decisión del dueño: guardar al momento y subir a la nube solo en reposo. Pantalla: la sincronización sola arranca con la pantalla atenuada y sin partido, y se corta al tocar la pantalla o el mando; realineado tras cada escritura en la memoria del chip (`s3_flash.h`) y al descomprimir fotos; la NVS no escribe ni realinea si el valor no cambia; caché de imágenes de 96 a 320. App: sincroniza tras 90 s quieta (o pestaña oculta) y nunca jugando, se cancela al tocar; arreglado el fallo que podía borrar un partido guardado durante una sincronización. Todas las pruebas locales correctas. Pendiente: instalar 0.12 y grabar con el móvil si la imagen aún salta (con Wi-Fi, editando una ficha y en la galería de fotos). Pendiente de decisión del dueño: compilar con configuración propia (arreglo de raíz del panel). Comprobaciones físicas sugeridas: cargador 5 V ≥ 2 A, canal del router (el mando va en el 1), cinta del panel.

## Torneos en la pantalla · 0.12 · en main · 23/10/2026

Liga, copa y pool en la placa como en la app: creador completo, clasificación/cuadro/pistas, cartel de fase, empate en liga a tiempo, ceremonia del campeón, ficha y palmarés casi ilimitado en la SD (10 ediciones en memoria, el resto por páginas; la nube de respaldo). Fusionada en main con autorización del dueño, encima de la 0.11 del equipo B; GitHub la publica como actualización. Pruebas del PC, cruzadas con la app y de GitHub correctas. **Sin instalar ni probar en la placa.** Voces de torneo aún sin activar. [Entrega](ENTREGA_TORNEOS_PLACA_0_12_20261023.md) · [Plan](PLAN_TORNEOS_PLACA.md).

## Pantalla 0.11 instalada y verificada · equipo B · 23/10/2026

**COM6, 560 WAV verificados (496 activos / 64 reservados de torneo), 123 anteriores y cuatro partidos conservados tras reiniciar.** Integra ajustes 0.8 y Wi-Fi 0.9 del equipo A. Todos los tests locales y los dos workflows de GitHub correctos para el código `63ad580`. El dueño autoriza la integración en main al terminar la verificación, preparada en [PR 21](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/pull/21). [Entrega a A](ENTREGA_SONIDOS_0_11_20261023.md) · [Informe físico](VALIDACION_PLACA_0_11_20261023.md). La 0.10 no se instaló por separado. Revisión independiente de A y prueba acústica jugando pendientes. RTC válido al final sin ajuste de B; los dos partidos antiguos siguen sin fecha.

## Pantalla 0.9 · imagen estable con el Wi-Fi · 23/10/2026

El dueño envió un vídeo: con el Wi-Fi activo (actualizar, entrar con QR) la imagen se corría y se partía. Causa: los búferes de red y el cifrado usan la PSRAM a la vez que la pantalla. Arreglo en `ui_port.h`: con red en marcha, el panel baja a 10 MHz (≈ 22 imágenes/s) y se realinea cada 250 ms; durante la descarga, tras cada avance; al acabar vuelve a 12,5 MHz. Pruebas simuladas y compilación real correctas. **Falta confirmarlo en la placa** (no se puede grabar desde la nube: se instala por Wi-Fi desde Actualizaciones).

## Ajustes en la pantalla y mando por modelo · 0.8 · 22/10/2026

Fusionada en main el 23/10/2026 con autorización del dueño; GitHub la publica como actualización oficial. **Sin instalar todavía en la placa.** Nuevo menú de AJUSTES en la pantalla: Partido, Sonido, Pantalla, Mando, Equipos fijos, Datos (modo prueba y copias de seguridad), QR del perfil de cada jugador y los apartados que ya había. El mando se actualiza con el programa de su mini: Super Mini o C3-Zero. Si no dice su modelo, no se le ofrece nada hasta elegirlo en AJUSTES → MANDO. Pruebas simuladas correctas en las tres placas. Falta la prueba física. [Entrega](ENTREGA_AJUSTES_PLACA_0_8_20261022.md).

## Revisión de Cloud y fusión en main · 22/10/2026

**Main contiene la pantalla 0.7** (entregas 0.3–0.7 del equipo B), revisada por Cloud y fusionada con autorización del dueño. GitHub la publica como actualización oficial; la placa del dueño ya la tenía instalada. [Revisión de Cloud](REVISION_CLOUD_0_3_A_0_7_20261022.md). Siguen pendientes: prueba audible jugando, desfase del RTC, voces de anulación, torneos en la pantalla, login real por QR y límites de Supabase.

## Variedad y paquete de sonidos 0.7 · equipo B · 22/10/2026

**Recomprobado el 23/10/2026, solo lectura:** tamaño/CRC completos de los 123 WAV correctos; 98 activos, 25 reservados y ambos partidos conservados. Sin regrabar firmware ni catálogo ni reproducir audio. Ver la comprobación posterior al final del [informe físico](VALIDACION_PLACA_0_7_20261022.md).

**0.7 instalada en COM6; 123 audios verificados en SD, 98 activos y 25 exclusivos de torneo reservados sin selección automática.** Rama `equipo-b/rendimiento-esp32s3`, código `642a4a0`; fusionada en main el 22/10/2026 tras la revisión de Cloud. Reparto 70 % específico / 30 % comodín y rondas completas por grupo sin repetir entre rondas si hay alternativas. Se conservaron 61 WAV y ambos partidos; se añadieron 62 y se comprobaron tamaño/CRC completo y catálogo después de reiniciar. [Entrega para A/Cloud](ENTREGA_VARIEDAD_SONIDOS_0_7_20261022.md) · [Validación física](VALIDACION_PLACA_0_7_20261022.md). La pantalla aún no implementa Torneos. Regresión local y GitHub correctos; pruebas acústicas pendientes; revisión de Cloud hecha. El desfase del RTC cercano a dos minutos no se reajustó.



## Entrega anterior · corrección conjunta de sonidos 0.6 · equipo B · 22/10/2026

**Pantalla 0.6 instalada y verificada en COM6; 61 voces conservadas y dos partidos intactos tras reiniciar.** Código `8b17155`, en `equipo-b/rendimiento-esp32s3`, sin fusionar main ni publicar OTA. Regresión nativa completa, 314 pruebas de app/herramientas, 21 pruebas del instalador y compilación real correctas; Pruebas y Firmware de GitHub también correctos. Dos lecturas de CRC completo por cada WAV, sin sobrescribir ninguno. [Entrega para A/Cloud](ENTREGA_SONIDOS_0_6_20261022.md) · [Validación física](VALIDACION_PLACA_0_6_20261022.md). Se mantienen seis pools y límite 1.024. Revisión independiente de Cloud y latencia acústica pendientes; persiste un desfase del RTC cercano a dos minutos, sin reajuste automático.


## Instalación anterior de la rama B · comentarios 0.5 · 22/10/2026

**Instalada y verificada localmente: pantalla 0.5 en COM6, con 61 voces y catálogo activo tras reiniciar.** Selector nativo hasta 1.024 referencias, mezcla contextual con comodines y sustitución al anular. Corregida la consulta que confundía la capacidad SD con FFat: la tarjeta real es de unos 8 GB y tiene espacio suficiente. [Validación física y catálogo instalado](VALIDACION_PLACA_0_5_20261022.md) · [Entrega técnica](ENTREGA_COMENTARIOS_0_5_20261022.md) · [Próximas familias, aún sin implementar](PLAN_COMENTARIOS_EVENTOS_20261022.md). Main sigue en 0.2, sin fusionar. No se iniciaron partidos ni se reprodujeron frases automáticamente; pendiente prueba acústica y de latencia con el dueño. RELOJ? declara válido, pero se midieron unos 123 s de retraso frente al PC después de grabar: investigar sin suponer la causa, ver informe físico; no se reajustó la hora.

## Estado anterior de la placa · 22/10/2026 · equipo B

**Instalada localmente: pantalla 0.5, build base 54, COM6. Publicada en main: 0.2 build 54.** Las entregas 0.3, 0.4 y 0.5 siguen en la rama `equipo-b/rendimiento-esp32s3`, pendientes de revisión de Cloud y sin fusionar. El dueño confirmó varios partidos correctos con 0.3. Con 0.4 se ha puesto el reloj en hora y verificado el Resumen real (2 partidos / 27 goles), conservando los dos originales sin fecha. Recuperar esas fechas aproximadas y comprobar un nuevo guardado en placa siguen pendientes. [Entrega de código](ENTREGA_FECHAS_RESUMEN_0_4_20261022.md) · [Validación física](VALIDACION_PLACA_0_4_20261022.md).

Sonidos FC26: 61 voces activas en el catálogo nativo, comprobadas después de reiniciar; los seis originales conservan su ruta plana y los otros 55 están en carpetas por prefijo. Pendiente primera prueba audible con el dueño y voces de anulación. [Validación](VALIDACION_PLACA_0_5_20261022.md).

La sección siguiente conserva el estado histórico del 19–20 de octubre.

## Entrega del equipo B · 19–20/10/2026

El propietario ha autorizado integrar en `main` toda la rama `equipo-b/rendimiento-esp32s3`, incluidas las mejoras del QR. La revisión independiente de Cloud sigue pendiente; no se presenta como realizada.

**Pantalla nativa publicada: 0.2 build 54. Instalada y verificada en placa: 0.2 build 54 en COM6, por autorización del propietario.** Mantener 0.x hasta autorización expresa para 1.0. La app web y el motor mantienen su numeración independiente. Ver [actualizaciones y política de versiones](ACTUALIZACIONES_PANTALLA.md).

| Trabajo | Estado del código y comprobaciones | Pendiente |
|---|---|---|
| Rendimiento y memoria de la pantalla | Implementado; pruebas automáticas y veinte ciclos físicos correctos. [Entrega](ENTREGA_EQUIPO_B_RENDIMIENTO_ESP32S3.md) | Cargas de partido/red y revisión de Cloud |
| Cinco mejoras ligeras | Implementadas y probadas; tiempos de menús similares. [Detalle](AMPLIACION_EQUIPO_B_MEJORAS_LIGERAS_ESP32S3.md) | Medición durante partido |
| Acceso mediante QR | Móvil y función publicados en main; QR visible y cancelación probada en placa. OPTIONS 204 y POST sin sesión 401 | Confirmación autenticada desde el móvil |
| Actualizaciones de la pantalla | Instalada 0.2 build 54 y reconocida como última publicada; novedades y guía disponibles. [Captura real](capturas-placa/actualizaciones-placa-0.2.png) | Ensayar la barra durante OTA desde la propia pantalla |
| Integración en main | Entrega USB/sesiones integrada en `369e153`; pruebas, firmware 0.2 y web publicados correctamente | Revisión independiente de Cloud |
| Instalación en la placa | 0.2 build 54 instalada con hash verificado; cinco recorridos válidos y fotos idénticas. [Informe físico](VALIDACION_PLACA_0_2_20261020.md) | Repetir con el dedo las flechas del incidente; revisar límites del diagnóstico USB |
| Bloqueo USB y sesiones | Avisos espontáneos sin espera, diagnóstico apagado completo, recordar sesión y cierre por aparato; [entrega a Cloud](ENTREGA_EQUIPO_B_USB_SESIONES_20261020.md) | Login real por QR; repetición del incidente original y diagnóstico de capturas/toque posterior |
| Límites de acceso en Supabase | Consulta de solo lectura vía Actions rechazada con HTTP 403 | Añadir auth_config_read al token de GitHub y repetir; no se presupone acceso indefinido en el servidor |

Resultados físicos del 20/10/2026: [VALIDACION_PLACA_20261020.md](VALIDACION_PLACA_20261020.md).

## Registro histórico (conserva sus fechas)

Fecha: 16/10/2026 · Versión 0.3.0 · Motor 1.0.0 (`reglas-partido-2`: por goles sin partes; por tiempo dos partes con prórroga y penaltis)

## Fases del documento maestro

| Fase | Estado |
|---|---|
| 0. Preparación | Hecha: plan por bloques, decisiones en `DECISIONES.md` |
| 1. Núcleo local | Hecha: UI 800×480, modos, configuración, selección, motor, partes, prórroga, penaltis, resumen |
| 2. Gestión local | Hecha: jugadores, preferencias, modo prueba, historial, detalle, almacenamiento local |
| 3. Análisis y progresión | Hecha con **parámetros propuestos**: perfiles, estadísticas, XP/niveles, ELO/ranking, previsión |
| 4. Funciones ampliadas | Hecha con propuestas: 54 logros, títulos, récords, Hall of Fame con podios, temporadas, retos, torneos (liguilla/cuadro), sonido, locutor, efectos, reposo, backup |
| 5. Preparación física | Avanzada: protocolo MFV3, conexión USB/Wi-Fi/Bluetooth en la app, firmware Arduino y ESP32, guía de cableado. Sin prueba en placa real |
| 6. Entrega del código | Este repositorio |
| 7. Base de datos y online | **No iniciada** (por indicación expresa) |

## Comprobado

- `npm test`: 79 pruebas (incluye reglas Caos, avisos, torneos, retos, temporadas, parejas, goleadores, pronósticos y títulos) — antes 55 (motor A01–A13 y extras de reloj/pausa, ELO, XP, niveles, estadísticas,
  previsión, repositorio local, almacenamiento lleno, modo prueba, backup).
- `npm run build`: TypeScript estricto sin errores y compilación correcta.
- Recorrido en Chromium a 800×480 (Playwright): menú → modalidad → configuración → jugadores →
  previsión → cuenta atrás → partido → bloqueo → pausa → descanso → 2ª parte → −1 → empate →
  prórroga → penaltis → resumen → ranking/historial/Hall of Fame/récords → perfil → torneo →
  ajustes → reposo. Sin errores de consola y sin scroll general.
- 2v2 en modo prueba con teclado y sensor simulado (bloqueo respetado), abandono de partido,
  recuperación tras recarga (reanuda en pausa), escalado a 400×300 y 1280×900.
- Versión 0.2: recorrido con datos de demostración (45 partidos) por Caos 2v2 con equilibrado, comodín, gol
  doble, racha, bola de partido, victoria, celebración, goleadores, ranking (temporada/histórico, Hall of Fame,
  parejas, resumen, cara a cara), perfil (cromo, ELO, hábitos, logros, estilo), retos, creación y juego de un
  torneo en cuadro, ajustes de audio y penaltis. Sin errores de consola.

- Versión 0.3: `npm test` 88 pruebas (protocolo, lienzo adaptable). `npm run test:hardware`: núcleo del firmware,
  simulación del sketch Arduino (antirrebote, órdenes, LED) y comprobación del sketch ESP32 con bibliotecas simuladas.
  Placa ESP32 simulada por WebSocket: salto de cuenta atrás, goles, bloqueo de 3 s y respuestas de la app correctos.
  Sin barras de desplazamiento en 800×480, 1280×720, 1024×768, 2560×1080, 844×390 y 390×844 (girado y sin girar),
  app servida comprimida como en el ESP32, y arranque sin conexión con el servidor apagado.

- Modo prueba (Ajustes → General, interruptor único): carga 12 jugadores, 137 partidos jugados con el motor real
  (1v1/2v2, Rápido, Caos y Clasificatorio, prórrogas, penaltis, goleadores, pronósticos) y 4 torneos (3 terminados y
  1 en juego). Lo que se juegue o añada en modo prueba también se guarda ahí (`mfv3:v1:demo:`). Al desactivarlo se
  borra todo lo de prueba y vuelven los datos reales intactos. Las preferencias son comunes a ambos modos.

## Cuenta del dueño de la mesa en la nube (19/10/2026, rama `pantalla-lvgl`)

- Ajustes → Cuenta en la app web: entrar, crear cuenta y «he olvidado la contraseña» (Supabase Auth con correo y
  contraseña). La cuenta es del dueño de la mesa: sus jugadores (con foto), partidos, torneos y ajustes se guardan
  en la tabla `mesa_datos` y se sincronizan solos (al entrar, al abrir, tras cada cambio, cada minuto). Sin cuenta
  o en modo prueba, todo sigue en local. Peticiones para la placa: docs/NUBE.md.
- Migración aplicada en Supabase (19/10/2026) y probada en producción: un partido de la app llegó a la nube.
- Pantalla de 7" (fase 3): AJUSTES → CUENTA con teclado (letras, números y símbolos); sube sus partidos y
  jugadores y baja los de la cuenta (con foto); une por nombre los jugadores que la placa tenía sin id de la app.
  Ver docs/NUBE.md.

## Sonidos propios (fases 1 y 3 hechas, 16/10/2026; la 2, en la nube, no se hace por ahora)

- **Ajustes → Sonidos**: subir sonidos de la mesa (mp3, wav, ogg, m4a…), escucharlos con ▶ y borrarlos. Al subirlos la
  app los prepara sola: una pista, sin silencios al principio ni al final, volumen igualado, 10 s como mucho y en WAV
  ligero (16 000 Hz, 16 bits, 32 KB por segundo), la misma frecuencia a la que suena la placa.
- **Dónde se guarda** cada sonido (se elige al subirlo): «Este aparato», «Chip de la placa» o «Tarjeta SD». La app lo
  apunta en cada sonido y lo toca desde ahí: del aparato, de la copia ya cargada en memoria (sin esperas al marcar);
  del chip o la SD, pidiéndoselo a la placa. Si la placa no está o no puede, suena el de siempre. Chip y SD solo se
  pueden elegir con una placa con altavoz conectada (capacidades `sonido` y `sd`).
- **16 momentos** con su sonido: «El de siempre», «Ninguno», uno propio o varios (uno al azar cada vez). Detección
  nueva de 3 seguidos, remontada, empate, goleada, gol de oro, ganar un torneo y reto conseguido
  (`src/services/sound/soundEvents.ts`). Un sonido propio nuevo corta al anterior; la bola de partido y el himno del
  ganador esperan a que acabe.
- Panel rápido: botón «Propio» en «Sonido de gol» cuando el gol tiene uno de la mesa.
- Copia de seguridad de Datos: incluye los sonidos (en base64). Las copias antiguas se siguen pudiendo cargar y, si no
  traen sonidos, no borran los de la mesa. «Borrar todos los datos» también los borra. Son comunes al modo prueba.
- Comprobado: `npx vitest run` 272 pruebas (19 nuevas de sonidos: momentos del partido, preparación del audio, de
  dónde sale cada sonido y copia de seguridad), `tsc` sin errores, `npm run build:web` correcto y recorrido en
  Chromium a 800×480: subir dos sonidos, elegirlos, recargar (se conservan) y partido rápido donde el gol suena con el
  propio y el empate con el de siempre. Sin errores de consola.
- **Fase 3, placa de 7" (Waveshare 7C) con dos altavoces de 8 Ω y 2 W** a sus conectores (chip de sonido ES8389 y
  dos amplificadores NS4150B de la propia placa; esquema en `HARDWARE.md` §7.2):
  - Los sonidos se guardan en la tarjeta micro SD, como `/sonidos/<código>.wav`. Memoria de 32 MB repartida con
    `hardware/esp32s3_pantalla7/partitions.csv`: 2 × 6 MB de programa (actualización por Wi-Fi) y 19,9 MB FAT
    reservados para imágenes (app y fotos de jugadores).
  - La app se los pasa por el cable USB en trozos con CRC32 y barra de progreso; al conectar le pregunta cuáles tiene
    («Pasar a la placa» si falta alguno) y le manda qué suena en cada momento y el volumen. Borrar en la app los borra
    también de la placa. Órdenes nuevas del protocolo `SONIDO_*`, `MOMENTO`, `VOLUMEN` (tabla en `HARDWARE.md` §5).
  - La placa detecta ella sola 13 momentos en sus partidos (los mismos que la app salvo torneo, nivel y reto) y toca el
    sonido elegido; si no lo tiene, unas notas «de siempre». También cuando juega sin la app.
  - Comprobado: `npm run test:hardware` (pruebas nuevas `sound_test.cpp` y la placa simulada con altavoces, memoria y
    SD en `s3_sim_test.cpp`: subir por USB, listar, tocar en el gol, «ninguno», volumen, borrar, recordar tras
    reiniciar) y `npx vitest run` 278 pruebas (6 nuevas del protocolo con una placa simulada). Compilado en GitHub.
    **Sin probar en la placa real.**
- Fase 2 (copia en la nube): no se hace por ahora.

## Capturas de la pantalla de 7" sin la placa (16/10/2026)

- `sh hardware/test/capturas.sh` dibuja las pantallas con la LovyanGFX real en el PC y guarda PNG en
  `docs/capturas-placa/` (en el PC no hay chip de sonido: la prueba de la placa sale con «Altavoz ERROR»).
- Se ve que los números grandes salen pixelados (fuente de mapa de bits ampliada) y sin tildes: lo siguiente es
  usar fuentes suaves con tildes. Pendiente también: jugadores con foto en la placa.

## Actualización por Wi-Fi de la pantalla de 7" y del mando (16/10/2026)

- **AJUSTES** en la pantalla (sustituye a PRUEBA en Inicio): Wi-Fi con lista de redes y teclado táctil, ACTUALIZAR,
  volumen y prueba de la placa (`s3_ajustes.h`, `s3_net.h`).
- Al encender mira si hay versión nueva (pantalla y mando) y lo avisa con un botón dorado en Inicio; se instala
  cuando se pulsa, nunca en partido. El mando se actualiza por radio desde la pantalla (`mfv3_ota.h`), con los dos
  botones mantenidos 3 s como permiso. Todo comprobado por MD5; si falla, cada placa sigue con lo que tenía.
- GitHub: número de versión en cada compilación y `ota.json` + `<placa>-ota.bin` en la web. Particiones con dos
  mitades (la primera vez, instalación por USB borrando la placa).
- Comprobado en PC: `ota_test.cpp` (radio con pérdidas), simulación del mando y de la pantalla (teclado, conexión,
  versiones, instalación completa mando + pantalla, comprobación al encender, sin internet). Capturas 09–14.
  Pendiente: probarlo en las placas reales.

## Reloj y batería en la pantalla de 7" (17/10/2026)

- `s3_reloj.h`: reloj PCF85063A (0x51) y medidor BQ27220 (0x55), en el bus del táctil. Hora en Inicio y reloj grande
  con fecha en reposo; fecha y hora en cada resultado (NVS `hist2`). Batería con su carga, «CARGANDO» y
  «BATERIA BAJA» (≤ 15 % sin cargar). Puesta en hora: orden `HORA` por USB (la app la manda a las placas con
  `reloj`) y hora de Internet al conectarse al Wi-Fi.
- Esquema oficial de la 7C: la pata EXIO6 es `LCD_VDD_EN` (alimentación del panel); ahora se deja alta, como en los
  ejemplos del fabricante (antes quedaba a 0). El módulo es un ESP32-S3-WROOM-2-N32R16V.
- El LED «STA» no se usa: no aparece en el esquema de la 7C.
- Comprobado en PC: simulación de la pantalla (puesta en hora, fechas imposibles, batería baja/cargando/quitada,
  reposo, hora de Internet) y capturas 15–17. Pendiente: probarlo en la placa real.

## Pantalla de 7" igual que la app: LVGL 9 en la Waveshare 7C (17/10/2026, rama `pantalla-lvgl`)

- **Motor gráfico nuevo** (`hardware/esp32s3_pantalla7/ui_port.h`): LVGL 9.5 sobre el panel RGB con dos imágenes de
  pantalla en la PSRAM que se cambian en el retorno vertical (sin parpadeo ni cortes). **Comprobado en la placa real:
  41 imágenes por segundo** (el máximo del panel a 16 MHz), imagen correcta y táctil GT911 (lo confirmó Alex).
- **Igual que la app**: lo que no cambia de cada pantalla (fondos, marcos, brillos, títulos, botones pulsados) se
  fotografía de la propia app a 800 × 480 (`scripts/placa-capas.mjs`) y va a la memoria FAT de la placa (19,9 MB;
  `scripts/placa_recursos.py`). Lo que cambia (números, reloj, textos, cifras) lo dibuja LVGL con la misma letra
  **Inter** de la app (ahora la app también la lleva dentro, para verse igual en cualquier aparato), colocada en la
  misma línea base que el navegador con las medidas de `docs/capturas-app/*.json`.
- **Ver la placa sin tocarla**: órdenes USB `CAPTURA` (la imagen que se ve, en 3 s) y `TOQUE x y`;
  `scripts/placa.py` compila, graba, saca capturas y manda órdenes; `scripts/comparar.py` junta app | placa.
  `scripts/capturas_placa_pc.py` saca las mismas capturas con la placa simulada en el PC (mismo código y LVGL).
- **Pruebas en PC**: el sketch se compila con LVGL de verdad (`hardware/test/lvgl_lib.sh`, `s3_build.sh`) para las
  tres placas; la prueba de partido completo lee los textos de los objetos LVGL.
- **Más ligera (18/10/2026)**, medido en la placa con las órdenes USB `RENDIMIENTO 1` (imágenes por segundo y cuánto
  tarda cada una, con la zona repintada) y `PERFIL` (cuánto cuesta cada efecto de la pantalla que se ve):
  - Repintar una pantalla entera cuesta ~35 ms aunque esté vacía, y una foto de fondo de 800 × 480, ~60 ms más.
  - No se repinta lo que no cambia (antes el partido se repintaba entero 8 veces por segundo sin hacer nada) y las
    imágenes sin transparencias tapan lo de debajo (LVGL 9.5 no lo hacía solo): el cartel de gol sale en 86 ms (343).
  - La interrupción que alimenta la pantalla va en el otro núcleo; las fotos de jugadores se reducen y redondean una
    vez y se guardan; la placa recuerda qué fotos están bien (antes leía los ficheros en cada repaso).
  - Elegir jugadores: fondo en degradado de blancos o de azules según el turno (idea de Alex) y al tocar solo se
    recoloca lo que cambia (0,4 s → 0,15 s por toque).
  - Diferencias con la app para ir fluida: el cartel de gol y los de PARTIDO / 2ª PARTE aparecen y se van de golpe
    (sin fundido ni deslizarse), en la bola de partido solo late el cartel (el aro rojo queda fijo), la barra del
    bloqueo baja a saltos de décima y el confeti de la victoria no gira y son 16 trocitos.
  - Inicio y configuración con fondo en degradado radial de cuatro colores (violeta, cian, rosa y azul noche) en vez
    de la foto de la app (Alex); sus tarjetas y botones encendidos y pulsados se preparan al encender y la tarjeta
    elegida se enciende de golpe. Reloj de reposo nuevo y ligero (hora grande dentro de un aro que hace de
    secundero): cada segundo repinta solo un trocito (11 ms).
  - Carteles en franja horizontal dibujada por la placa (sin imágenes): ¡GOL!, ¡GOLAZO!, ¡OTRO MÁS!, ¡RACHA xN!,
    ¡EMPATE!, GOL ANULADO, PARTIDO, PARTIDO LOCO, 1ª/2ª PARTE, PRÓRROGA, PENALTIS y MUERTE SÚBITA (antes
    imágenes de pantalla completa de la app). Al irse solo se repinta la franja: 0,08 s (0,3). Las tarjetas ya no
    se oscurecen durante el bloqueo (quedan la barra y el aviso).
  - Partido Loco en la placa (antes jugaba como el rápido): los 14 hándicaps de la app con sus efectos en el
    marcador (goles dobles y triples, robo, marcador congelado, penalti con GOL/FALLO…), su sorteo y tiempos
    (cada 30–60 s de juego), «¡VUELTA A LA NORMALIDAD!» y las visitas de la ardilla, el caracol y el gato.
    Motor en mfv3_engine.h (probado en el PC: engine_test), textos en s3_loco.h. La cuenta atrás y la tarjeta
    del hándicap van en un recuadro en medio de la pantalla (no a pantalla completa) y, mientras dura, el botón
    de PAUSA enseña el hándicap con sus segundos. Iconos (los emoji de la app) y animales: scripts/placa-loco.mjs.
    Diferencias: una sola cuenta atrás (la app tiene 12 estilos) y los animales salen en una franja (la app los
    anima por la pantalla).
  - AJUSTES → JUGADORES (18/10/2026): crear jugadores en la propia placa, sin la app. Nombre con teclado en
    pantalla (Ñ y tildes; mayúscula al empezar cada palabra) y foto de la carpeta FOTOS de la tarjeta SD (.jpg;
    se abren reducidas, recortadas al centro a 120 × 120 y enderezadas si el móvil las guardó giradas; se puede
    girar a mano). Tocar un jugador deja cambiarle el nombre o la foto, o borrarlo. En la placa quedan Alex, José,
    Lucía, María, Vicky y Victoria (Alex pidió borrar los demás). Las JPEG «progresivas» no se pueden abrir.
    Código: ui_fichas.h (pantallas), s3_fotos.h (fotos de la SD; lector fotojpg.c, copia del TJpgDec completo).
  - Historial (18/10/2026): la placa guarda TODOS los partidos en la tarjeta SD, cada uno con el mismo formato que
    la app (StoredMatch en JSON, /sdcard/partidos/<id>.json, más un índice para la lista; sin tarjeta, en el chip).
    tests/boardMatches.test.ts comprueba que la app los acepta (copia de seguridad, goles y progresión). Inicio →
    RANKING abre la pestaña HISTORIAL como la app: filtros por modalidad y jugador, páginas de 5, y el detalle
    (cómo acabó, marcador, equipos con foto, goles en orden y ELIMINAR). Fondos en degradado.
  - Ranking completo (19/10/2026): la placa calcula ELO, experiencia, niveles, rangos, logros y retos con una copia
    en C++ de la progresión de la app (s3_progresion.h). tests/progresionPlaca.test.ts guarda lo que calcula la app
    con 154 partidos del modo prueba y hardware/test/progresion_test.cpp comprueba que la placa da lo mismo (si la
    app cambia una regla, la prueba falla). Pestañas como la app: CLASIFICACIÓN (temporada e histórico, con rango y
    forma), JUGADORES, HISTORIAL, SALÓN DE LA FAMA (podios, campeones de temporada y récords), RESUMEN (7 y 30 días)
    y PAREJAS; el detalle enseña rango, ELO y nivel. Sin torneos todavía (la placa aún no los juega).
    Plan acordado con Alex: 2) cuentas del dueño de la mesa en Supabase y login en la app web; 3) login en la placa
    y subir/bajar jugadores y partidos; 4) cálculos de ELO, niveles y categorías en la placa.
  - Memoria: la caché de imágenes baja a 8 MB y las victorias se preparan al llegar a bola de partido (con 10 MB
    la placa se quedaba sin un hueco grande y podía reiniciarse).
  - Pendiente: una comprobación del PC (pantalla de actualizar sin internet, s3_sim_test) falla de vez en cuando
    (2 de unas 15 veces) y luego pasa; sin causa encontrada todavía.
  - La imagen se corría de lado al cambiar mucho la pantalla (cartel de gol): la interrupción del panel vuelve a ir
    en el mismo núcleo que dibuja (en el otro se pisaban la PSRAM). Pendiente de que Alex confirme que ya no pasa.

## Web y compilación

- Web pública (`web/`) con asistente «Monta tu marcador» e instalador desde el navegador (ESP Web Tools). Los
  binarios los genera GitHub Actions (`Firmware`, release `firmware-latest` al subir a `main`).
- Compilación real en GitHub: Arduino Uno/Nano/Mega/Leonardo, Raspberry Pi Pico, ESP32, C3, S3, S2 y mando
  compilan, y las 3 pantallas de 7" también tras corregir las declaraciones automáticas del IDE (`s3_types.h`;
  las pruebas del PC ahora simulan ese paso). Los binarios se recortan (sin relleno 0xFF) para instalar en segundos.

## Limitaciones conocidas

- El firmware compila en GitHub Actions para las 13 placas, pero no se ha probado ninguna placa física:
  primera prueba recomendada por USB con el monitor serie (y la pantalla de prueba en la de 7").
- En http://192.168.4.1 (app servida por el ESP32) el navegador no permite instalarla ni USB/Bluetooth web
  (exigen https); la conexión Wi-Fi con la placa sí funciona.
- ESP32-S3 7": versión nativa activa en `hardware/esp32s3_pantalla7/` (motor C++ + LovyanGFX). Probada en PC con simulación y compilada en GitHub; sin probar en la placa real. Configurada para la Waveshare ESP32-S3-Touch-LCD-7C (sensores en DI0/DI1) y mando inalámbrico de 2 pulsadores (ESP-NOW: toque corto = gol, largo = anular).
- «Hazlo tú mismo»: catálogo único de 13 placas (`src/inputs/hardware/diy-catalog.json`) para la guía de la app y `docs/MANUAL_DIY.md`; pines automáticos por modelo (`mfv3_boards.h`) y saludo con capacidades. Todas las placas figuran como «probada en simulador» hasta probarlas en real.

- Los iconos con emoji (🏆, 🎯, 🃏…) dependen de la fuente de emoji del sistema; las imágenes opcionales los sustituyen.
- La voz del locutor depende de que el navegador tenga una voz en español instalada.
- Los datos viven en el `localStorage` de este navegador: borrar los datos del sitio los elimina.
  Exporta copias desde Ajustes → Sistema.
- El arranque totalmente sin red tras un reinicio (empaquetado offline/PWA) no está garantizado en
  una web alojada sin caché; se debe validar en el dispositivo final.
- ESP32-S3 7": versión nativa activa en `hardware/esp32s3_pantalla7/` (motor C++ + LovyanGFX), probada en PC
  con simulación y compilada en GitHub; sin probar en la placa real. Configurada para la Waveshare ESP32-S3-Touch-LCD-7C (sensores en DI0/DI1) y mando inalámbrico de 2 pulsadores (ESP-NOW: toque corto = gol, largo = anular).
- Fotos de jugador guardadas como imagen reducida (160×160) dentro del almacenamiento local.


## Entrega B · bloqueo USB y sesiones (20/10/2026)

Código integrado en main y firmware 0.2 build 54 publicado: [USB, QR y sesiones 0.2](ENTREGA_EQUIPO_B_USB_SESIONES_20261020.md). Main autorizado por el propietario; aceptación de Cloud pendiente. QR en Ajustes de la web comprobado con nube ficticia; login real con placa pendiente. Se conserva 0.x. Esta entrega no modifica tablas, RLS ni datos de partidas.
