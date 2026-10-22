# Variedad y paquete de comentarios 0.7 · equipo B · 22/10/2026

Rama `equipo-b/rendimiento-esp32s3`, sin fusionar main ni publicar OTA. El dueño aprobó el reparto 70/30, rondas sin repetición y la subida de la tanda completa. Código y paquete preparados; la instalación física se registra al final de esta entrega cuando se complete. A/Cloud debe revisar el cambio; no se presenta como revisado por Cloud.

## Comportamiento de las frases

Primero se elige el grupo y después una variante pendiente de ese grupo. Si hay contextos con voces disponibles y comodines, se reserva aproximadamente **70 % al conjunto de contextos y 30 % a comodines**, sin depender del número de archivos de cada grupo. Con dos contextos: 35/35/30; con tres: 70/3 para cada uno y 30 para comodín. Son probabilidades a largo plazo, no siete elecciones forzadas por cada diez goles. Sin contexto disponible se utiliza solo comodín; sin comodines se reparte entre los contextos disponibles.

Cada grupo lleva su propia bolsa: se elige una variante pendiente y se intercambia su código con el final de la parte pendiente del mismo grupo. Es Fisher–Yates incremental, un intercambio por selección. Al agotar la ronda, su última frase queda en la primera posición y se excluye del primer sorteo nuevo, si hay alternativas. Así se agota cada grupo antes de repetir y se evita repetición inmediata entre rondas. Con una sola voz, repetir es inevitable; con dos, alternan.

No se lee la SD para sortear, no se abre ningún candidato adicional ni se precargan listas de PCM. Se reutiliza el array de códigos existente, conservando los límites de cada grupo y la unicidad. El catálogo persistido CMT1 no se reescribe por cada gol: los intercambios y contadores viven en RAM. La variedad continúa entre partidos mientras no se reinicie/recargue el catálogo. Al reiniciar se inicia una bolsa nueva, sin conservar el orden previo. Se cuentan elecciones, no la audición completa: una frase interrumpida o un fallo posterior de reproducción no devuelve automáticamente su código a la bolsa.

Las anulaciones usan exclusivamente su grupo, nunca frases de gol. Se mantienen silencio/sonidos personalizados, prioridad de anulación de animales y aviso único de penalti de la 0.6. Estos nuevos contextos corresponden a goles aceptados; los animales continúan con la simplificación acordada. No se altera el momento de inicio del comentario tras presentar el marcador.

## Catálogo que se instalará

**123 archivos: 98 activos y 25 reservados.** Son los 61 anteriores y 62 nuevos, todos PCM mono 16 bits/16 kHz. Total preparado: **46.229.236 bytes**, sin duplicados exactos dentro de una misma familia. Se conservan los WAV originales. Los archivos del juego no se incorporan a GitHub; solo código, reglas y metadatos de procedencia/hash.

| Pool activo | Frases | Condición |
|---|---:|---|
| 0 · Comodín | 63 | Cualquier gol normal; sigue elegible junto con contextos |
| 1 · Tres o más encajados | 3 | Marca quien ya había recibido al menos tres antes de ese gol |
| 2 · Empate | 3 | El marcador queda empatado al sumar el gol |
| 3 · Respuesta | 3 | Marca tras dos o más goles válidos seguidos del rival |
| 4 · Anulación | 0 | Aún no hay voces entregadas; se conserva el tono/preferencia de respaldo |
| 5 · Empate con dos previos recientes | 6 | Empata y hubo otros dos goles válidos en los últimos 60 segundos de juego |
| 6 · Sigue perdiendo | 10 | Tras sumar el gol, el equipo que marca sigue por debajo del rival |
| 7 · Ventaja de dos o más | 6 | Tras sumar el gol, el equipo que marca gana por al menos dos, INCLUIDO dos. El criterio del dueño prevalece sobre «más de 2» del nombre |
| 8 · Más de ocho | 4 | Tras sumar el gol, el equipo que marca lleva nueve o más goles en ese partido |

Los 17 comodines nuevos se unen a los 46 existentes; las dos tandas de seguir perdiendo se unen en una bolsa de diez. El grupo de ocho goles NO se dispara al octavo: empieza en el noveno. Se comprueba para ambos equipos y también a través del sketch, evitando truncar el bit 8.

| Familia reservada, sin pool activo | Archivos | Regla del dueño |
|---|---:|---|
| Torneo · el rival necesita dos para avanzar | 4 | El equipo que recibe el gol necesita dos goles para pasar de ronda |
| Torneo · más de ocho | 4 | El equipo que marca lleva nueve o más, exclusivamente en torneo |
| Final de torneo · empate | 5 | El gol empata la final |
| Final de torneo · empate en últimos 20 s | 4 | El gol empata cuando quedan 20 segundos o menos |
| Final de torneo · ir ganando en últimos 20 s | 4 | Marca para ir ganando cuando quedan 20 segundos o menos |
| Final de torneo · ventaja de más de uno | 4 | Tras el gol, gana por al menos dos en la final |

**La pantalla nativa aún no juega torneos:** el flujo `onHomeEvent` muestra «Los torneos llegan más adelante» y el motor no tiene cuadro, ronda ni contexto de clasificación. Por eso los 25 se guardan/verifican en SD pero no se añaden a `catalogo.cmt`; ningún gol normal puede seleccionarlos automáticamente. No se sustituye ese contexto por una suposición de marcador. La app web tiene otro alcance. Al implementar torneos, concretar resultado agregado/desempates para «necesita dos», y si «ir ganando» exige ponerse por delante en ese gol o permite ampliar una ventaja previa. Estas voces están reservadas, no son una función de torneo ya implementada.

## Cambios para A/Cloud

- `s3_comentarios.h`: `Catalog::pick` aplica el reparto, `draw` agota bolsas, `goalMask` añade los tres contextos. `availableOnly` y `clear/add` reinician correctamente las bolsas al cambiar referencias. Las máscaras pasan a `PoolMask` de 32 bits y el sketch conserva ese tipo; permite declarar más grupos hasta 32 sin truncamiento. Solo están registrados los pools 0–8; añadir uno requiere reglas, herramientas, pruebas y nueva versión. El máximo sigue siendo 1.024 referencias activas, no 1.024 por grupo.
- La permutación afecta únicamente a códigos dentro de su pool en RAM; no cambia asociaciones ni rutas. `save` continúa recibiendo el catálogo nuevo sin consumir, con su CRC de filas. No persistir una permutación con una CRC antigua.
- `preparar-comentarios.py`: clasificador explícito de los nombres recibidos; normaliza tildes para reconocer «EQUIPÒ» y conserva el original. Los números de exportación no son marcadores. Las familias exclusivas de torneo tienen `pool=null` en `reserved_files`, nunca un grupo de gol de reserva inventado.
- `subir-comentarios.ps1` y `CommentarySdInstaller.cs`: validan todos los archivos, incluidos reservados, pero la lista de filas activas puede ser un subconjunto único del paquete. La CRC se calcula sobre esas filas; no se admiten códigos ajenos/duplicados ni grupos no registrados. Todos los datos se releen antes de activar; el total esperado del catálogo es el activo, no el almacenado. Los nuevos pools 6–8 exigen firmware >=0.7 incluso si un manifiesto declara una mínima menor. Se mantienen las comprobaciones y recuperación de ACK de 0.6.
- `releases.json` / `s3_release.h`: nueva entrada 0.7 en lenguaje llano; 0.x y `majorOneAuthorized=false`. Sin cambios de mini C3, web, Supabase ni formato de partidos.

## Coste y pruebas

La variedad con los seis grupos anteriores cabía en los mismos 4.124 bytes del catálogo. Con los tres grupos adicionales y máscaras ampliadas, `Catalog` ocupa **4.140 bytes**. No hay otro vector de 1.024 posiciones ni reserva dinámica en `pick/draw`. La compilación final declara **130.048 bytes de globales**, +24 B frente a 0.6; el coste medido incluye alineación del binario completo, no solo la estructura. No aumenta con el número de WAV de cada grupo, hasta el límite previsto. Las voces completas permanecen en SD.

Pruebas específicas: 200.000 sorteos para 70/30 y otros 200.000 para 30/35/35 con semilla reproducible y tolerancias; casillas de distribución uniforme; rondas completas intercaladas, grupos de uno/dos, tres rondas de 1.024, extremos de 32 bits, reinicio de bolsas, falta de grupos y separación de anulaciones. Casos de reglas para ambos equipos: derrota, empate, ventaja de uno/dos/tres y ocho/nueve/diez goles. Recorrido integrado del noveno gol hasta el comentario pendiente. Catálogo 800 WAV simulados, persistencia y recuperación conservados. El tiempo orientativo de 100.000 elecciones en PC incluye trabajo del test y no mide el altavoz ni sirve para comparar latencias entre versiones.

**Regresión nativa completa y compilación real correctas; 314 pruebas de app/herramientas, seis de clasificación y 29 del instalador correctas.** El instalador prueba también almacenar reservados sin activarlos, corrupción local/remota de reservados, referencia ajena y duplicado activo. No se crean partidos ni goles físicos para verificar esta entrega. La escucha y medición de latencia real con el dueño siguen pendientes.

La instalación física debe conservar los dos partidos originales, los 61 WAV previos, preferencias y recursos. Se verifican versión, memoria, archivo completo por CRC y catálogo antes/después de reiniciar. Solo se graba app0, sin formatear ni escribir la imagen completa de flash. El desfase del RTC documentado en 0.6 no se reajusta por esta entrega.
