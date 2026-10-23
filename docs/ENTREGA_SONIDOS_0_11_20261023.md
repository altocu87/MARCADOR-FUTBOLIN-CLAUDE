# Entrega a equipo A / Cloud · pantalla 0.11 · 23/10/2026

## Resultado y alcance

Desarrollada en `equipo-b/rendimiento-esp32s3`; el dueño ha autorizado integrar 0.11 en main tras completar la instalación y sus comprobaciones ([PR 21](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/pull/21)). Se integra `origin/main` de A hasta `aff9735` (pantalla 0.9). Se conservan sin modificaciones `ui_port.h` y la corrección del Wi-Fi (10 MHz durante red, realineación y retorno a 12,5 MHz), los Ajustes/QR/equipos/modo prueba/copias de 0.8 y el protocolo de mando por modelo. No se regraba la mini. Se conserva también la reorganización concurrente de Cloud (`947ecae`): CLAUDE.md breve e historial en `docs/HISTORIAL_ENTREGAS.md`; la nota de 0.11 se actualiza en ese historial, no se vuelve a alargar CLAUDE.md.

El dueño pidió inicialmente 0.10 para 71 audios, y añadió 79 y después 287 más con autorización para 0.11. **0.10 es el registro del lote intermedio preparado: no se instaló ni se publicó por separado.** La entrega final 0.11 contiene todos los lotes y las mejoras de A. Mantener 0.x; `majorOneAuthorized=false`.

Estado físico: **0.11 instalada y verificada en COM6, 560 WAV (496 activos, 64 reservados), 123 anteriores y cuatro partidos conservados**. Ver [informe físico](VALIDACION_PLACA_0_11_20261023.md), incluido RTC válido al final, su lectura inicial no válida y las pruebas acústicas aún pendientes. No se iniciaron partidos de medida ni se alteraron fechas. La revisión independiente de A de esta 0.11 sigue pendiente.

## Catálogo final instalado

**560 WAV = 496 activos + 64 reservados; 437 nuevos, 123 anteriores conservados byte a byte.** 146,192,816 bytes WAV (sin estimar clusters FAT). PCM mono 16 bits/16 kHz, lectura por bloques; ninguna pista completa se carga en RAM. Catálogo CMT1: 496 filas, 2490 bytes, CRC32 `416bfecd`. Inventario con fuentes, códigos y hashes: [catálogo](sonidos/catalogo_fc26_560_0_11.json). No contiene los audios comerciales ni datos privados de la placa; los originales y el paquete completo están en el PC del dueño.

| Pool | Familia | Voces | Condición |
|---|---|---:|---|
| 0 | Comodín general | 87 | En cualquier gol |
| 1 | Había encajado al menos tres | 3 | Marca quien ya había recibido 3 o más |
| 2 | Empate | 11 | El marcador queda empatado |
| 3 | Respuesta tras dos seguidos | 3 | Rompe dos o más goles consecutivos del rival |
| 4 | Anulación | 0 | Sin voces recibidas: respaldo anterior |
| 5 | Empate con movimiento | 6 | Otros dos goles válidos en los 60 s de juego anteriores |
| 6 | Sigue perdiendo | 10 | Después de marcar sigue por detrás |
| 7 | Ventaja de dos o más | 6 | Después de marcar gana por 2 o más |
| 8 | Más de ocho | 4 | Quien marca lleva al menos 9 goles |
| 9 | Va ganando | 11 | Después de marcar va por delante, cualquier ventaja |
| 10 | Últimos 20 segundos | 14 | Segunda parte o prórroga; nunca primera ni solo goles |
| 11 | Introducción | 15 | Primera cuenta atrás de 3 s; puede terminar mientras se juega |
| 12 | Descanso | 10 | Fin de primera parte; una vez |
| 13 | Final sin prórroga | 7 | Se agota el reloj de la segunda parte con ganador; una vez |
| 14 | Remontada | 6 | Ese gol pone por delante tras haber ido perdiendo, incluso por 1 |
| 15 | Empate tras perder por tres | 6 | Ahora empata y llegó a perder por 3 o más |
| 16 | Pierde por más de tres, último minuto | 5 | Tras sumar pierde por 4 o más y quedan <=60 s de segunda parte/prórroga |
| 17 | Pierde por más de tres | 5 | Después de sumar sigue perdiendo por 4 o más |
| 18 | Penalti fallado | 14 | Tanto en tanda como en penalti del partido |
| 19 | Gol de penalti | 20 | Comodín propio de penaltis; nunca mezcla narraciones de gol normal |
| 20 | Penalti y empate | 2 | Empata el marcador del partido; no se aplica al empate de la tanda |
| 21 | Penalti y empate al final | 2 | Lo anterior y <=20 s de segunda parte/prórroga; nunca tanda |
| 22 | Comentarios ambiente | 132 | Intervalo aleatorio de 30–60 s de juego; solo jugando y en silencio |
| 23 | Primeros 20 segundos | 117 | Una vez tras la introducción, si aún está dentro de 20 s de juego |

Las voces de inicio, descanso y final son eventos independientes: **no se mezclan con comodines de gol**. En goles, si hay grupos específicos elegibles, 70 % a esos grupos (reparto igual por grupo) y 30 % al comodín general. Si falta uno de los lados se utiliza el disponible. Las bolsas Fisher–Yates agotan cada familia y evitan repetir en el borde de ronda cuando hay alternativas; no persisten tras reiniciar. No hay precarga durante la cuenta atrás ni recorrido de directorios al elegir.

### Reservados: guardados en SD, fuera del catálogo activo

| Familia exclusiva | Voces |
|---|---:|
| solo_final_torneo_delante | 19 |
| solo_final_torneo_delante_ultimos_20s | 8 |
| solo_final_torneo_empate | 5 |
| solo_final_torneo_empate_ultimos_20s | 4 |
| solo_final_torneo_ventaja_dos_o_mas | 9 |
| solo_torneo_falta_uno | 6 |
| solo_torneo_falta_uno_ultimos_30s | 5 |
| solo_torneo_mas_de_ocho | 4 |
| solo_torneo_rival_necesita_dos | 4 |

La pantalla todavía no tiene Torneos. Los 64 reservados tienen `pool:null` y nunca entran en el catálogo automático. «Final comodín ir ganando» es exclusivo de final, no comodín general. Las eliminatorias requieren contexto real de agregado/desempates, no inferirlo del marcador de un rápido. Para los últimos 30 s de eliminatoria: estrictamente <30.000 ms. Para los últimos 20 s: <=20.000 ms, solo segunda parte/prórroga; no partidos solo a goles. Ir ganando significa marcador posterior al gol, no obligación de acabar de romper un empate.

## Comportamientos acordados y límites

- **Gol ganador a goles:** cambia inmediatamente a victoria y conserva la voz de gol. No concatena ahora una frase larga de final. El dueño buscará una voz específica «gol y final», o dos clips muy cortos. No prolongar la celebración ni bloquear el motor para terminar una frase. Dejar este trigger como siguiente trabajo, sin inventar audio.
- Introducción solo en la primera cuenta atrás. No repite al reanudar o al empezar la segunda parte. Continúa después de los tres segundos sin retrasar el juego; un gol/anulación la sustituye. Se inicia después del fotograma igual que el gol.
- Final normal solo al agotarse el reloj de segunda parte con ganador; no en empate que pasa a prórroga, gol de oro, penaltis ni gol ganador por objetivo.
- Remontada: pasar a estar por delante después de haber ido perdiendo, incluso por un gol. Empate tras déficit de tres es otra familia. Se reconstruye de goles válidos; una anulación elimina su influencia. No se usan estas dos reconstrucciones históricas en Partido Loco, donde las visitas no conservan su instante.
- Partido Loco mantiene las simplificaciones previas: animal suma→gol comodín, resta→anulación y transferencia→anulación prioritaria; aviso de penalti una vez al señalarlo. No se desarrolla su narrativa animal.
- «Pierde de más de tres» significa diferencia >=4 **después** del gol. Último minuto usa tiempo de juego restante; excluye primera parte y solo goles. Los umbrales 20/60 s son inclusivos.
- Las elecciones explícitas en Ajustes (`Ninguno` o sonido propio) siguen mandando sobre los catálogos automáticos. Volumen cero no se anuncia como reproducción audible.

## Cambios técnicos y coste

`s3_comentarios.h`: índices 0–8 intactos, 9–10 incorporados para el lote 0.10 y 11–23 para 0.11; máscara de 32 bits. `Catalog` ocupa **4.200 B** en la prueba, frente a 4.140 B en 0.7: +60 B por contadores de quince familias. Selector: pocas operaciones en RAM por familia, independencia del número de WAV; reconstrucción histórica de déficit acotada a los 120 goles del motor y sin asignaciones ni SD. No prometer coste cero ni latencia acústica medida.

`.ino`: arranque de intro en `startMatch()`, descanso/final en `afterChange()` y reproducción cooperativa tras un fotograma. Mantiene guardado diferido del resultado, prioridad de anulación y exclusión de gestión SD durante un partido. No se añade cola de narraciones atrasadas. El ambiente revisa su reloj como máximo 10 veces/s, sin SD ni repintados durante la espera. Se activa con el reproductor libre al menos 250 ms (margen para cola DMA), sin otra voz pendiente, y espera 10 s de juego tras cualquier cambio de marcador. Pausas/menús/descanso/prórroga pendiente/tanda quedan fuera. Gol, anulación y cambio de fase cortan ambiente; no se acumulan frases durante interrupciones. Las frases de primeros 20 s son una vez y caducan si la introducción/otra voz ocupa toda la ventana. No se fuerza un repintado para disparar ambiente: la barrera de fotograma se mantiene para goles, penaltis, intro y finales. Si un archivo de ambiente desaparece, queda silencio y error diagnóstico, no un falso tono de gol.

Penaltis: fallo usa pool 18, acierto pool 19; en empate de marcador del partido añade 20 y, si corresponde, 21. 70 % entre los grupos específicos y 30 % al comodín **de penalti** (19), sin comodines de gol normal. La frecuencia 30–60 s fue confirmada expresamente por el dueño. La pregunta sobre permitir también el empate de tanda no recibió respuesta; se adopta la interpretación conservadora, solo marcador del partido, comunicada al dueño. No fingir esa decisión como confirmación explícita. Los penaltis genéricos/fallados sí se usan en la tanda. El anuncio de que se pita penalti conserva su evento anterior; las voces nuevas de resolución no lo repiten. Sin catálogos nuevos, el penalti de hándicap conserva el comportamiento previo.

Preparador: reconoce nombres exactos acordados, incluye INTRO/FINAL además de GOL, conserva originales, recorta silencio inicial y genera WAV pequeño. El nombre **GOL EMPATE_0_0.wav se reutilizó con contenido diferente**: asignación explícita de grupo 31 (`--groups-json`), ID/código nuevo; el anterior de final (grupo 17, `e9ddd095`) permanece reservado e intacto. No reclasificar por nombre todos los antiguos GOL EMPATE. Los namespaces separados evitan pisar variantes con el mismo índice. No se encontraron duplicados de bytes dentro de la misma familia funcional.

Instalador PowerShell/C#: acepta pools 0–23, rechaza 24+, exige >=0.10 para 9/10 y >=0.11 para 11–23 aunque el manifiesto indique menos. Los reservados se verifican y guardan sin activación. Mantiene SHA/CRC local, dos lecturas CRC remotas, rechazo de archivos existentes diferentes, recuperación de ACK perdido, reserva FAT e identidad de placa/volumen. Diario no sustituye la lectura de SD. Catálogo activo se sustituye únicamente después de verificar todos los archivos.

### Protección frente a una actualización de otra rama

`screenOutdated()` comparaba solo el contador de build: una 0.9 publicada podía ofrecerse para sustituir la 0.11 USB, porque su contador de GitHub es independiente del build base 54 local. Ahora descarta una versión de producto anterior, aunque tenga un build mayor. Mantiene la comparación de build para la misma versión, versiones posteriores o manifiestos antiguos sin versión válida; no cambia la selección ni el firmware del mando. Comparación numérica 0.9 < 0.10 < 0.11, con pruebas de patch, valores vacíos y sintaxis incorrecta. No se eleva el build artificialmente ni se bloquean versiones futuras.

## Comprobaciones y revisión

- 10 pruebas Python de clasificación/asignación, 33 casos del instalador con transporte falso, sin puertos reales.
- Regresión nativa completa correcta: tres placas, motor, seguridad/resiliencia de audio, animales, transición con pulsadores, fechas, ajustes 0.8 y Wi-Fi 0.9, máscaras hasta bit 23, persistencia y 800 WAV simulados, 70/30 y no repetición. Nuevas `s3_commentary_events_test.cpp` y `s3_commentary_ambient_test.cpp`: intro que cruza el inicio, prioridad gol/anulación, descanso/final una vez, victoria inmediata, límites y déficits reconstruidos; reloj 30–60 s, pausas, silencio tras gol, ventana inicial, prioridad de penaltis, falta de archivo ambiente y 70/30 de penalti.
- App/herramientas: 315 pruebas correctas; paridad QR se omite en la ejecución habitual y se ejecuta con sus 40 partidos durante la regresión nativa.
- Se corrigió **solo la prueba de paridad**: C `qsort` no garantiza orden entre rivales con igual número de partidos (fallaba en Windows). Comprueba orden descendente real y compara todas las filas empatadas por ID, además de conservar la igualdad de estadísticas, personas, rivales preferidos y partidos. No se modificó el perfil de producción ni se ocultó diferencia de datos.
- Compilación Arduino correcta (1.936.576 bytes de binario; 136.352 bytes globales), SHA256 `28e1876f0e8be5f3abb9358f02e44773b757f70f9e830900de82f77f2888887b`. Fuentes del sketch verificadas contra la copia compilada. La verificación física y los límites están en [su informe](VALIDACION_PLACA_0_11_20261023.md); no sustituyen pruebas acústicas con personas jugando.

La revisión de A de 0.3–0.7 ya está hecha y fusionada; **la revisión de esta 0.11 sigue pendiente**. Revisión recomendada: reglas exactas, eventos y preferencias explícitas, convivencia del gol ganador con el futuro audio «gol y final», límites de memoria/red, y medición acústica con el dueño. No iniciar partidos reales ni cambiar datos para probar sin consultarle.
