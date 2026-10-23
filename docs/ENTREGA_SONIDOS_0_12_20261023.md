# Entrega de pantalla 0.12 · equipo B · 23/10/2026

## Estado y autorización

El dueño autoriza integrar los 210 audios enviados, subir 0.12 a main y grabarla en la placa. Otra sesión audita el mismo repositorio; esta entrega se limita al panel, catálogo, disparadores y pruebas relacionadas. No desarrolla Torneos ni amplía animales/hándicaps. Fuentes preparadas y pruebas en curso; **instalación y publicación pendientes de sus comprobaciones**. No confundir con la prueba privada anterior instalada como 0.11.

## Panel y Wi-Fi

El dueño confirmó que el menú se mantiene normal con la prueba a reloj fijo. 0.12 conserva exactamente ese comportamiento gráfico: 12,5 MHz desde arranque; elimina cambios de reloj y realineaciones periódicas por red de 0.9; mantiene recuperación puntual después de escribir flash y durante instalación. Se retiran los comandos privados PANEL? / PANEL_FIJO / PANEL_AUTO / PANEL_RECUPERAR; no se publican controles de laboratorio.

Evidencia: con 0.11 publicada había carga visible seguida de azul/negro/gris, pese a captura interna y USB correctos; con la prueba fija el dueño confirmó menú normal. Esto implica al conjunto del control dinámico, **no demuestra por separado si era frecuencia, reinicio periódico o interacción**. Pruebas nuevas verifican que conectar/desconectar no genera reinicios temporizados y que la recuperación tras flash sigue ejecutándose una vez. Descargas prolongadas, latencia acústica y uso intensivo real quedan pendientes; no declarar que todos los problemas Wi-Fi estén resueltos. El aviso de conectar Wi-Fi sigue siendo otro punto de diagnóstico y no se han cambiado credenciales.

## Paquete y reglas

**770 WAV = 706 activos + 64 reservados. 210 nuevos; los 560 anteriores conservan código, clasificación y bytes.** 184,945,256 bytes WAV. Sin duplicados de bytes en la misma familia funcional ni colisiones de código. Originales intactos; mono PCM 16 bits/16 kHz, recorte inicial conservando 20 ms y margen contra saturación. CMT1 706 filas, 3540 bytes, CRC32 c9dd6523. Se conserva esquema 2 del manifiesto. El instalador exige >=0.12 para grupos 24–30 aunque el manifiesto indique una versión anterior, y rechaza 31 o superior antes de abrir USB.

| Pool | Familia | Voces |
|---|---|---:|
| 0 | Comodín de gol | 112 |
| 1 | Había encajado tres | 3 |
| 2 | Empate | 11 |
| 3 | Respuesta tras dos seguidos | 3 |
| 4 | Gol anulado | 48 |
| 5 | Empate con dos goles previos en 60 s | 6 |
| 6 | Sigue perdiendo | 10 |
| 7 | Ventaja de dos o más | 6 |
| 8 | Nueve goles o más | 4 |
| 9 | Va ganando | 11 |
| 10 | Últimos 20 s | 14 |
| 11 | Introducción | 15 |
| 12 | Descanso | 10 |
| 13 | Final sin prórroga | 7 |
| 14 | Remontada | 6 |
| 15 | Empate tras déficit de tres | 6 |
| 16 | Pierde por cuatro, último minuto | 5 |
| 17 | Pierde por cuatro | 5 |
| 18 | Penalti fallado | 14 |
| 19 | Gol de penalti | 20 |
| 20 | Penalti y empate | 2 |
| 21 | Penalti y empate al final | 2 |
| 22 | Ambiente | 197 |
| 23 | Primeros 20 s | 117 |
| 24 | Segunda parte comodín | 20 |
| 25 | Segunda parte tras siete goles o más | 5 |
| 26 | Segunda parte tras cero o un gol | 5 |
| 27 | Gol de oro ganador | 5 |
| 28 | Fin de segunda: prórroga | 15 |
| 29 | Fin de prórroga: tanda | 2 |
| 30 | Ventaja exacta de tres | 20 |

Los 64 reservados de Torneos no entran en selección automática. Inventario con procedencia y hashes: [catálogo](sonidos/catalogo_fc26_770_0_12.json); no subir los WAV al repositorio.

Reglas nuevas:

- Anulación: interrumpe el comentario anterior y elige exclusivamente entre las 48 voces de Anulado; nunca mezcla un gol positivo. Animales que restan siguen la simplificación ya acordada. Se mantiene prioridad única de anulación en transferencias.
- Segunda parte: una vez al terminar su cuenta atrás, no al pulsar continuar ni en pausa/reanudación ni al empezar prórroga. Usa marcador válido de primera parte, con anulaciones y cambios del motor aplicados. Más de seis significa >=7; menos de dos significa 0–1; de 2 a 6 usa solo comodín de segunda parte. Si hay específico, 70 % específico / 30 % comodín de segunda parte; no mezcla voces de gol. Usa la preferencia de sonido de inicio ya existente: Ninguno/propio mandan. No se amplía el formato persistido de preferencias ni el protocolo común.
- Prórroga: anuncio una vez al terminar segunda parte empatada y quedar pendiente continuar; no repite al empezar la prórroga. No se usa en partido a goles.
- Tanda: anuncio una vez al terminar prórroga empatada; no repite al pulsar para entrar en penaltis ni cuenta ese botón como lanzamiento.
- Gol de oro: las cinco voces combinadas son elegibles solo en gol ganador de prórroga con Reason::GoldenGoal. Mantiene mezcla acordada con comodines y otros contextos elegibles; no se confunde con gol ganador por objetivo ni añade una voz larga de final. Victoria inmediata, audio después de presentar el marcador.
- Ventaja de tres: **exactamente tres después de sumar**, confirmado expresamente por el dueño. Diferencia de dos o cuatro no activa esta familia.
- Los 65 comentarios nuevos de ambiente se añaden a los 132 anteriores. Conserva intervalo 30–60 s de juego, silencio tras cambio de marcador y prioridad de eventos; ninguna cola de frases atrasadas.

## Coste y validación

Catálogo acotado: 31 grupos de 32 posibles, 1024 referencias; sizeof(Catalog) pasa de 4200 a **4228 B (+28 B)**. Máscara uint32_t hasta bit 30. Segunda parte consulta el marcador del primer periodo, sin asignaciones ni SD; selección por índices y reproducción por bloques. No se precargan los 770 WAV ni se amplía innecesariamente la capacidad del catálogo en esta entrega.

Pruebas específicas cubren 0/1/2/6/7/8 goles de primera parte, anulación 7→6, inicio efectivo y no repetición por pausa, preferencias, mezcla 70/30 sin gol normal, anuncios únicos de prórroga/tanda, gol de oro frente a objetivo, ventaja 1/2/3/4 para ambos equipos, anulación durante reproducción, y guardado/recarga del bit 30. Instalador: 35 casos con transporte falso, incluidos grupos 24–30 con mínimo 0.12 y rechazo de 31. Pruebas habituales de app: 315 correctas; la paridad QR se ejecuta aparte en la regresión nativa. Actualizar aquí resultados completos, binario, instalación, CRC remotos y confirmación física al terminar.

## Para el equipo A y la auditoría paralela

Revisar esta entrega sobre el código final 0.12 y actualizar la referencia del incidente anterior. No restaurar setNetwork/PCLK_NET_HZ sin una prueba física que descarte la regresión. No confundir probar bytes, ACK USB o captura interna con comprobar el panel/altavoces físicos. Mantener pendientes: Wi-Fi, estabilidad con descarga OTA prolongada, Torneos y siguientes módulos/sonidos. Los archivos actuales caben en 1024, pero futuras ~500 voces y activación de reservados pueden exigir ampliar capacidad y representación de eventos; diseñarlo como trabajo separado, sin crecer grandes tablas en RAM interna.
