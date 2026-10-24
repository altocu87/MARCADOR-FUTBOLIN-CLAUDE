# Pantalla 0.16 · 100 sonidos de tanda y ambiente · 24/10/2026

Petición del dueño: añadir el lote a la tabla de 770 e instalarlo. Confirmó que COMENTARIOS RAMDOM 10 es ambiente durante el partido y que el primer lanzamiento corresponde al primer tiro de toda la tanda. No se ha solicitado publicar en main ni OTA.

## Estado

**Instalación física terminada en COM6.** Programa 0.16 grabado y hash comprobado; 100 WAV nuevos, 770 conservados y 870 verificados por tamaño/CRC completo. Catálogo de 806 voces recargado tras reiniciar; 64 reservadas anteriores conservadas. Los cinco partidos siguen idénticos y no hay errores de audio registrados. Prueba acústica jugando pendiente. No publicado en main/OTA.

## Paquete

870 WAV = 770 anteriores conservados + 100 nuevos. Catálogo activo: 806 voces; las 64 voces de torneo previamente reservadas siguen reservadas. 200.330.688 bytes de WAV (sin contar clusters FAT); los nuevos ocupan 15.385.432 bytes. PCM mono, 16 bits, 16 kHz. CMT1: 806 referencias, CRC32 `659f8b3b`.

| Grupo | Voces nuevas | Uso |
|---|---:|---|
| 22 · ambiente | 14 | Durante el partido; total de ambiente 211 |
| 31 · si marca gana | 7 | Antes del tiro cuyo gol decide la tanda |
| 32 · si falla pierde | 7 | Antes del tiro cuyo fallo decide la derrota |
| 33 · previo genérico | 14 | Antes de los restantes tiros de la tanda |
| 34 · fallo en tanda | 22 | Tras fallar en la tanda |
| 35 · gol en tanda | 21 | Tras marcar sin terminar la tanda |
| 36 · primer lanzamiento | 7 | Antes del primer tiro de toda la tanda |
| 37 · gol y victoria | 4 | Gol que gana el partido por penaltis |
| 38 · gol y campeonato | 4 | Gol que gana la final y completa la serie, si la hay |

Los avisos decisivos no se mezclan con el previo genérico. El aviso siguiente espera al resultado anterior; si se tira antes, se recalcula para el turno vigente. No retrasa la pantalla de victoria. Los nombres de las dos familias de goles y de fallos conservan su procedencia, pero comparten el grupo funcional correspondiente. Los penaltis durante el partido mantienen sus grupos anteriores. Las elecciones explícitas de sonido/silencio conservan prioridad.

## Implementación y límites

Máscara de grupos ampliada de 32 a 64 bits, manteniendo índices 0–30 y formato CMT1 (grupo de un byte). 39 grupos, máximo 1024 archivos. `Catalog` mide 4264 bytes en pruebas, 36 más que antes; no amplía el vector de códigos ni carga pistas completas en RAM. Los cuatro audios de campeonato llegan a unos 45 segundos: se mantienen completos y el límite de transferencia pasa de 1 a 2 MiB. No cambian los buffers de reproducción. El instalador exige >=0.16 para grupos 31–38 o archivos mayores de 1 MiB, incluso si el manifiesto declarase una versión inferior.

La predicción de victoria usa los contadores de goles/intentos y se contrasta en pruebas contra el motor real. La voz de campeón consulta la final del torneo y las victorias previas de una serie. Los 64 reservados anteriores no se activan indirectamente.

## Validación

- Pruebas nuevas: predicción contra el motor para ambos primeros equipos, rondas 1–5 y árboles de hasta 12 tiros; muerte súbita; grupos 31–38 guardados y recargados; espera del aviso; resultado final; serie; prioridad de silencio.
- Regresión de comentarios, eventos, segunda parte/prórroga, ambiente, seguridad de audio y Partido Loco: correcta.
- Protocolo de sonido y recuperación de audio: correctos.
- Instalador: 38 casos con transporte simulado, incluidos firmware antiguo, archivos largos, CRC, reintentos y reservados. Clasificador: 11 pruebas previas correctas; las 100 entradas nuevas clasificadas y convertidas en el paquete.
- Compilación Arduino-ESP32 3.3.12 normal: binario 2.011.904 bytes; globales 143.264 bytes. No usa la fase 3 experimental. SHA256 del binario: `e04bbf1046aea63f8b6ad19a5a2f5a5e5115c67752e7a1d890de58a752f755d6`.

Las pruebas del PC y la verificación de archivos no sustituyen escuchar una tanda real. No se crean partidos de prueba en la placa del dueño durante la instalación.

## Comprobación física final

1740 lecturas completas de tamaño/CRC. MEMORIA interna 45964 (mayor 27636, minima 34924) psram 5964892; MEMORIA psram mayor hueco 5898228, cache 7123 KB; MEMORIA tablas_UI psram 0 bytes, 0 tablas.

[Inventario instalado](sonidos/catalogo_fc26_870_0_16.json). Arranque anterior 0.15 conservado en app1; 0.16 en app0. Copias privadas de metadatos y diario USB en la carpeta de trabajo `instalacion-sonidos-0.16/` y en el paquete local; no se publican audios ni copias privadas.
