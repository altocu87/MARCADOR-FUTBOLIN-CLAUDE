# Plan de mejora de la pantalla · 07/11/2026

Base: pantalla **0.28** (`main` 828f33f). Rama de trabajo: `claude/rendimiento-penaltis-arranque`.
Auditoría hecha por un equipo de tres agentes (audio de penaltis, arranque y respuesta, fallos pendientes),
coordinada y comprobada en el código. Las cifras «medidas» son de la placa; las «deducidas», de leer el código
o del simulador del PC.

## 1. Qué está hecho (código hecho y probado en el PC; **sin instalar en la placa ni publicar**)

| Problema | Causa encontrada | Arreglo |
|---|---|---|
| **Sonido cortado en la tanda de penaltis** | La pantalla de penaltis se repintaba entera en **cada vuelta** del programa (75 zonas por vuelta frente a 0 en el partido), y el sonido solo se alimenta entre vuelta y vuelta. Con fotogramas de más de ~110 ms el altavoz se queda sin datos y la voz sale a trozos. | Solo se repinta si algo cambia; el foco de luz late a saltos (8 fotogramas por cada 200 vueltas en vez de 200). |
| Final de las voces de la tanda cortado | El aviso («lanza el blanco…») paraba el altavoz y tiraba lo que quedaba por sonar de la voz anterior (hasta ~120 ms). | El aviso entra en cola detrás. |
| **Final de un torneo: la pantalla de victoria se bloquea** (no hace caso a «VER CAMPEÓN» ni a «INICIO»; había que reiniciar) | Al rehacer la pantalla, el aviso de «borrada» de la vieja olvidaba la nueva, y se rehacía en cada vuelta. | Corregido; prueba nueva que falla sin el arreglo. |
| Arranque: barra «Cargando» | Cada 1 % esperaba un repintado completo del panel (deducido: 1,5–2 s en total). | Repinta cada 5 %. |
| Red mala: minutos con la radio fuera del canal del mando | La conexión segura esperaba hasta **120 s** por intento (peor caso de la búsqueda de versiones: ~10 min). | Tope de 15 s en buscar versiones, pitido y nube (no en descargar el programa). |
| «SINCRONIZANDO · NO TOQUES» al despertar | La franja seguía aunque tocar ya había cancelado la subida. | Se quita al tocar. |
| Pantalla atascada tras sincronizar fotos | Cada foto nueva rehacía todas las fichas (~1,4 s cada vez, medido el total al arrancar). | Se rehacen una sola vez al terminar la sincronización. |
| Mando: una pulsación perdida con la cola llena | Se apuntaba como vista aunque no entrara, y su repetición se descartaba. | Solo se apunta si entra. |
| Toque fantasma a los 24,8 días encendida | Cuenta de tiempo que da la vuelta. | Corregido. |
| `placa.py probar` podía escribir sobre el programa en uso | No ignoraba entradas «inválida/abortada» del arranque. | Las ignora, como el propio chip. |

Medidas nuevas para la placa (sin coste de memoria fija): la línea `ARRANQUE` del registro dice cuánto tarda cada
fase del encendido, y la orden `AUDIO?` dice el mayor hueco sin alimentar el sonido (`hueco_max_ms`, con más de ~110
hay cortes) y cuántas veces se llenó el búfer.

## 2. Qué falta comprobar en la placa (necesita permiso del dueño para grabar)

Grabar esta versión en el **hueco de prueba** (`placa.py probar`; la 0.28 se queda en el otro y se vuelve con
`placa.py volver`) y:

1. **Penaltis:** jugar una tanda escuchando; al acabar, `AUDIO?`. Éxito: sin cortes y `hueco_max_ms` < 100.
2. **Arranque:** apagar y encender de verdad (no por USB); leer `ARRANQUE`. Sabremos si el tiempo se va en las voces
   (806 archivos comprobados uno a uno), las letras o las fichas.
3. **El «primer minuto sin responder» NO está reproducido.** La prueba por USB no sirve (cada orden despierta la
   placa). Hace falta saber del dueño: ¿al enchufarla o al despertarla del reloj? ¿Con el dedo o con el mando? ¿Se ve
   «Cargando» o el reloj? Candidatos, de más a menos probable:
   - Los 14 s de «Cargando» (confirmado: en ese rato no se lee el táctil).
   - El **mando** sordo mientras la placa busca versiones por Wi-Fi al arrancar (la radio sale de su canal); con red
     lenta podía durar minutos (ahora mucho menos, por el tope de 15 s).
   - Tras una sincronización en reposo: tirones y la franja «NO TOQUES» (arreglados arriba).
   - Bloqueos del reloj de reposo: 3,65 s al entrar y ~1 s por cada cifra nueva (medido el 02/11).
4. Medir memoria (`MEMORIA`) tras una tanda y tras un torneo completo.

## 3. Siguiente fase de mejoras (por orden; cada una, pequeña y con su prueba)

**Hechas el 07/11/2026:** la 2 (voces después de arrancar), la 3 (reloj de reposo a trozos) y la 4 (sonido con su
propia tarea), probadas en la placa; y después la 1, 6, 7, 8, 9 y 10 (código y pruebas del PC). Queda la 5, que decide
el dueño. Además, a petición del dueño: en la tanda, el
pulsador del portero cuenta como fallo del que tira.

| # | Mejora | Qué gana el dueño | Riesgo |
|---|---|---|---|
| 1 | **Inicio primero:** enseñar Inicio tras sus capas y letras, y cargar el resto poco a poco después (parando si se toca). | Inicio en ~7 s en vez de 14 s (deducido). | Medio: hay que vigilar que no cargue durante un partido. |
| 2 | Comprobar las 806 voces después de arrancar (ahora se abre cada archivo antes de enseñar nada). | Lo que diga la medida `ARRANQUE`. | Bajo. |
| 3 | Reloj de reposo sin bloqueos: guardar las cifras ya dibujadas (o dibujarlas por partes). | Sin los 3,65 s al entrar en reposo ni el segundo por minuto. | Medio (memoria PSRAM, que sobra). |
| 4 | Sonido con su propia tarea (no depender del dibujo). | Ninguna pantalla lenta volverá a cortar el sonido. | Medio: unos KB de memoria interna, que va justa. |
| 5 | No buscar versiones al encender si se va a jugar: hacerlo en el primer reposo. | El mando responde desde el primer segundo. | Bajo; **decide el dueño** (el aviso de versión nueva tardaría más). |
| 6 | Guardado seguro de partidos: si falla el índice o un borrado en la SD, no dar éxito falso ni perder la fila (MD-02, MD-03). | Ningún partido «desaparece» del historial. | Medio. |
| 7 | Sincronización con la nube por cursor en vez de por páginas (MD-05). | No se salta partidos si otro aparato cambia datos a la vez. | Medio. |
| 8 | Pitido dañado: comprobar tamaño y CRC y volver a bajarlo (MA02). | Siempre suena el pitido o, si no, el tono de respaldo. | Bajo. |
| 9 | Cancelar de verdad la búsqueda de versiones al empezar un partido (PA01) y recuperar el panel tras un fallo al escribir la memoria (PA02). | Menos red durante el partido. | Bajo/medio. |
| 10 | `placa.py`: comprobar el archivo de programa antes de grabarlo (TO02). | Herramienta más segura. | Bajo. |

## 4. Decisiones del dueño

- **Voz del tiro anterior:** cada tiro nuevo corta la voz que esté sonando (hecho así a propósito en la 0.16). ¿Se
  deja, o la voz del tiro espera si al aviso le queda poco?
- **«¡Equipo blanco!» al volver** del pronóstico o al despertar en ELEGIR JUGADORES: hoy vuelve a BLANCO aunque ya
  haya jugadores. ¿Debe seguir donde estaba?
- **Orden de partidos del mismo segundo** (MD-01): ordenar por milisegundos como la app; el ELO de algunos partidos
  importados puede cambiar un poco.
- Buscar versiones al encender o en el primer reposo (mejora 5).

## 5. Lo que no se ha podido comprobar

Fugas de memoria al abrir y cerrar pantallas (la prueba larga no terminó en el PC); cómo suenan los arreglos en la
mesa; el «primer minuto» real; las interferencias de imagen del 02/11, que siguen sin causa confirmada. No se ha
tocado el reloj del panel (fijo a 12,5 MHz).
