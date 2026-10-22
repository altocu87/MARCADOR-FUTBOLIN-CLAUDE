# Próximos comentarios de la pantalla nativa · equipo B · 22/10/2026

El dueño anticipa estas familias durante la autorización de instalación 0.5. Es una hoja de trabajo para A/Cloud, no voces entregadas ni funciones nuevas activadas. No inventar frases ni clasificar automáticamente archivos que el dueño no haya enviado. La petición de aproximadamente 800 audios se refiere al conjunto del sistema: comprobar almacenamiento y tiempos con el catálogo definitivo.

| Familia | Momento a concretar y comprobar |
|---|---|
| Inicio de partido | Comienzo efectivo del juego; distinguir cuenta atrás e inicio para evitar dos frases |
| Segunda parte | Entrada efectiva en segunda parte; no repetir por pausa o repintado |
| Final de partido | Fin confirmado del motor, una sola vez; decidir cómo convive con voz del gol ganador |
| Prórroga | Distinguir anuncio de que habrá prórroga y comienzo de esta, según las frases recibidas |
| Penaltis | Distinguir entrada en tanda, lanzamientos y resultado; aún sin clips ni reglas exactas |
| Ganar un partido | Victoria confirmada; coordinar con final de partido y gol ganador para que no se corten entre sí |
| Campeón de torneo | Solo con resultado final de torneo confirmado y soporte nativo comprobado; una victoria de partido no basta |
| Gol anulado | Prioridad para cortar el comentario del gol y sustituirlo. Pool 4 ya previsto, sin voces recibidas |
| Comentario durante una sequía de goles | Ejemplo del dueño: unos 90 segundos sin gol. Umbral, repetición y fases elegibles pendientes; no activado |

## Continuidad de diseño

Mantener índices compactos precargados al arrancar/instalar: evento → grupos elegibles → código de archivo → lectura por bloques. No recorrer directorios, abrir candidatos para sortearlos ni cargar voces completas durante el juego. La variedad crece en SD, no en buffers de reproducción. Guardar la procedencia y las condiciones de cada frase en el catálogo y las pruebas.

La 0.5 usa CMT1 con seis pools (0–5) y una máscara de 8 bits para contextos de GOL. Eso no es un registro universal ya preparado para añadir sin límite todos los eventos de esta tabla. Al ampliarlo, separar evento y contexto o diseñar una representación mayor, con pruebas de límites, persistencia, recuperación y migración. Conservar los índices existentes: anulación=4 y empate con dos previos en 60 s=5. No añadir pools mayores de 7 a la máscara actual ni cambiar el formato en silencio. El límite actual es 1.024 referencias en total, no por cada familia.

Mantener comodines elegibles junto con grupos específicos cuando corresponda, no repetir inmediatamente una variante si hay alternativas y respetar las preferencias explícitas de silencio/sonido. Nunca dejar una cola de narraciones atrasadas. Un futuro comentario ambiente debe ceder inmediatamente ante gol, anulación o cambio de fase y no sonar durante pausa, menús o reposo; definir ese comportamiento y su tiempo con el dueño antes de implementarlo. El ejemplo de 90 s no constituye una configuración instalada.

Medir el tiempo hasta el altavoz, interrupciones, lecturas de SD y memoria en la placa real con todas las familias. Las pruebas del selector actual en PC no validan acústica ni carga de 800 archivos físicos. Seguir en 0.x, registrar las novedades legibles de cada publicación y conservar una entrega verificable para A/Cloud.
