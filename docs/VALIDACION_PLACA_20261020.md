# Validación física · QR original y versión optimizada 0.1

Fecha: 20/10/2026. Equipo B. El propietario autorizó integrar toda la rama en main e instalar primero el trabajo QR y después las optimizaciones. La revisión independiente de Cloud sigue pendiente.

## Versiones y publicaciones

- QR original: fuente exacta `cc064e0`, la rama `pantalla-lvgl` que ya contenía el QR nativo. Compilación local con ESP32 3.3.12 y LVGL 9.5.0. Binario de 1.856.208 bytes. La etiqueta antigua `1.0` de HELLO es histórica; no se publica una versión 1.0 nueva.
- Optimizada: `main` en `569098a`, versión de producto **0.1**, build **49**. Binario oficial de [Firmware 39391954909](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39391954909): 1.864.256 bytes, MD5 `9cd0d178ac3461546313a4c6e6379f09`, SHA256 `05028a60cb4506cb2a2c3456ad495d4543cd9e0969418f15210b7e939f65aa2d`. Se comprueban el tamaño, MD5 y cabecera del archivo antes de grabarlo.
- Todas las comprobaciones de ese código en main son correctas: [Pruebas](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39391954449), [publicación QR](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39391954658) y [web con firmware](https://github.com/altocu87/MARCADOR-FUTBOLIN-CLAUDE/actions/runs/39392432113). Se mantiene la regla 0.x hasta autorización expresa para 1.0.

## Grabación y conservación de datos

USB comprobado de nuevo: **COM6**, ESP32-S3 Waveshare 7C. La tabla real coincide con `hardware/esp32s3_pantalla7/partitions.csv`: app0 en 0x10000, app1 en 0x610000, 6 MiB por programa, FFat en 0xc10000 y NVS en 0x9000.

La lectura completa con el lector temporal de esptool falla en transferencias grandes; no existe una copia local íntegra de los 32 MiB. El cargador original de ROM sí permite guardar los primeros 64 KiB, incluidos arranque, tabla, NVS y selección OTA. Estas copias son privadas: pueden contener configuración o credenciales y no se incorporan a Git.

Se conserva **intacto el programa previo en app1**. Su selección original era secuencia 2, estado válido, CRC comprobado. La versión QR y luego la optimizada se graban en app0. Para arrancar en app0 se escribe únicamente el primer sector de selección OTA con secuencia 3 y CRC comprobado; el segundo sector original permanece intacto. No se reescriben arranque, tabla de particiones, NVS, FFat ni SD. La herramienta verifica el contenido grabado mediante su hash. Se usa el cargador ROM (`--no-stub`) que permite escribir y verificar sin el fallo del lector rápido.

La copia de configuración y recuperación está en el directorio local privado `backup-placa-20261020` de esta conversación. Volver al programa anterior exige restaurar su primer sector de selección OTA original y reiniciar; **no hay vuelta atrás automática**. No restaurar NVS ni borrar datos para cambiar de programa.

## Método y límites

Se recorren las pantallas mediante órdenes USB `TOQUE` (inyectan el evento en LVGL; no prueban el sensor táctil físico), se guardan capturas `CAPTURA` con CRC32 verificado y se consulta `MEMORIA`. `RENDIMIENTO 1/0` se usa temporalmente para observar el dibujo; al terminar se desactiva. Los comandos de consulta y las capturas quedan en registros privados. Las imágenes que contienen jugadores, nombres, Wi-Fi o el código QR no se publican en el repositorio.

`CAPTURA` transmite 768.000 bytes y bloquea la aplicación unos 1,2–1,3 segundos. Los máximos de loop producidos por capturar se excluyen de las conclusiones de uso normal. El valor «pintar max» mide dibujo, no la latencia completa dedo → imagen. Los FPS de un menú parado no son una medida de fluidez en un partido.

No se crean ni borran partidos, jugadores ni fotos, ni se cambian volumen o Wi-Fi. Se ven seis jugadores con sus fotografías. La clasificación consultada no tiene partidos clasificatorios de la temporada, por lo que no representa el coste de calcular estadísticas con un historial grande.

## Confirmado con la versión QR original

- Grabación y verificación del programa y la selección OTA correctas; arranca y responde HELLO/PING.
- Inicio, ranking, historial, lista de seis jugadores y sus seis fotos, Cuenta y QR visibles.
- Ranking: máximos de dibujo 86–87 ms en las aperturas observadas; volver al inicio, 84 ms. Jugadores con fotos: 154 ms. Son muestras de esta sesión, sin distribución p95 ni medida de botón físico.
- Memoria al comienzo: interna libre 71.600 B, mayor bloque 31.732 B, mínimo 70.460 B; PSRAM libre 6.011.992 B. Tras ranking y tres vueltas: interna libre 71.420 B, mínimo 65.264 B; PSRAM libre 5.823.024 B. Al mostrar jugadores: interna libre 71.280 B, mínimo 63.912 B; PSRAM libre 5.558.000 B.
- Durante la espera QR: interna libre 72.564 B, mínimo 54.472 B, PSRAM libre 5.182.172 B. El estado es «Esperando al móvil», sin cuenta vinculada. La confirmación autenticada requiere al propietario.
- **Fallo físico reproducido:** al volver del QR a Cuenta y después a Ajustes, los toques en VOLVER y en las filas dejan de navegar; PING y MEMORIA siguen respondiendo. La captura permanece en Ajustes. Coincide con el fallo del simulador Linux: el adaptador `LegacyGfx` retenía la dirección de un objeto LVGL ya liberado y podía confundir otra pantalla con la suya.
- Los menús antiguos piden fuentes que no están en FFat (`inter-700-*`, `inter-400-20`): aparecen avisos repetidos y letra de reserva. La interfaz nueva de Actualizaciones reutiliza fuentes existentes. Corregir los demás menús es una mejora separada pendiente.

## Resultado de la versión optimizada

La segunda instalación termina con hash verificado. HELLO responde `waveshare-7c 0.1`; el binario instalado es exactamente el artefacto build 49 indicado arriba. La placa queda en Inicio, sin la instrumentación periódica activada y con el puerto serie liberado.

**Confirmado:**

- Se repite Cuenta → QR → Cuenta → Ajustes y ahora sí se puede abrir Actualizaciones y volver a Inicio. La corrección de `ui_legacy.h` evita el bloqueo reproducido con el original.
- La lista de seis jugadores es **idéntica píxel a píxel** a la de la versión QR original: ambas capturas RGB565 de 768.000 bytes tienen CRC32 `9d209d57`. No se han sustituido ni perdido fotos en el recorrido.
- Historial mantiene la pestaña seleccionada al salir y volver; aparece el partido rápido que ya estaba guardado. No se inicia ningún partido durante estas pruebas.
- Actualizaciones muestra **0.1 instalada** y reconoce que es la última publicada. Novedades se desplaza y permite leer hasta la última entrada; Cómo actualizar se abre y se puede volver. Capturas reales: [novedades](capturas-placa/actualizaciones-novedades-placa-0.1.png), [desplazamiento](capturas-placa/actualizaciones-scroll-placa-0.1.png), [guía](capturas-placa/actualizaciones-guia-placa-0.1.png).
- Se abre la configuración de Partido Loco y su selección de jugadores con las seis fotos; se vuelve sin comenzar el partido. Se comprueban reposo y despertar al Inicio.
- Veinte ciclos consecutivos Inicio → Ajustes → Jugadores → Ajustes → Inicio: veinte respuestas de vuelta a Inicio, **cero reinicios inesperados** y sin pérdida progresiva apreciable de memoria en ese recorrido.

| Medida comparable | QR original | Optimizada 0.1 | Lectura del resultado |
|---|---:|---:|---|
| Dibujo al abrir ranking, primera vez | 87 ms | 87 ms | Sin mejora medible en esta muestra |
| Dibujo al reabrir clasificación | 86–87 ms | 87–88 ms | Diferencia pequeña; no demuestra aceleración |
| Dibujo de lista con seis fotos | 154 ms | 153 ms | Prácticamente igual; imagen final idéntica |
| RAM interna libre en Inicio al comienzo | 71.600 B | 64.784 B | El conjunto de cambios usa 6.816 B más en esta muestra |
| Mayor bloque libre interno al comienzo | 31.732 B | 31.732 B | El bloque grande disponible se conserva |
| PSRAM libre al comienzo | 6.011.992 B | 6.011.820 B | Diferencia pequeña |
| RAM interna libre al mostrar jugadores | 71.280 B | 64.648 B | Coste adicional similar, unos 6–7 KiB |
| PSRAM libre al mostrar jugadores | 5.558.000 B | 5.557.964 B | Prácticamente igual |

La memoria mínima registrada no se toma como comparación de un paso idéntico: las tareas de consulta y TLS tienen momentos distintos. En el recorrido QR original llega a 54.472 B internos y en el optimizado a 47.816 B. No hay fallos de asignación o reinicios observados, pero no se deduce de ello seguridad ante cualquier carga.

Estabilidad de la versión optimizada, en Inicio y con las cachés ya preparadas:

| Momento | Interna libre | Mayor bloque interno | Mínimo interno desde arranque | PSRAM libre | Mayor bloque PSRAM |
|---|---:|---:|---:|---:|---:|
| Antes de 20 ciclos | 64.144 B | 31.732 B | 47.816 B | 5.260.428 B | 5.111.796 B |
| Después de 10 | 64.184 B | 31.732 B | 47.816 B | 5.260.412 B | 5.111.796 B |
| Después de 20 | 64.196 B | 31.732 B | 47.816 B | 5.260.412 B | 5.111.796 B |

La caché crece al visitar por primera vez más escenas y llega a 7.651 KiB. Después permanece en ese valor durante los veinte ciclos. La apertura del Historial recordado tarda 110 ms en una muestra, pero muestra contenido diferente de la clasificación: no se mezcla con la comparación de reaperturas de clasificación.

**Probable, pendiente de cuantificar:** las eliminaciones de trabajo repetido y las copias más pequeñas reducen el coste de estados concretos del partido y de la sincronización. Las pruebas automáticas respaldan el comportamiento, pero este recorrido no mide goles ni carga grande de fotos en red.

**Conclusión:** se confirma una mejora de robustez concreta y las funciones nuevas de historial/actualizaciones; no una ganancia general de FPS ni ahorro de RAM interna. Los menús comparados dibujan en tiempos semejantes y el conjunto de funciones añade aproximadamente 6–7 KiB internos. La memoria se mantiene estable en el ensayo repetido.

La mejora pequeña siguiente sería ajustar las fuentes solicitadas por los menús antiguos a las que existen: los avisos repetidos de fuentes ausentes continúan. Se deja anotada para una entrega posterior; no se amplía este firmware después de publicarlo como 0.1.

## Qué no se acredita con estas pruebas

Quedan pendientes: confirmar el QR con la cuenta real (en ambos firmwares se muestra y espera al móvil, pero `cuenta=0` durante el ensayo); partido/goles/Partido Loco en juego y pulsadores físicos; carga con historial grande; transferencia OTA desde la nueva interfaz, su barra durante escritura de flash y pérdida de red/alimentación controlada. Las pruebas automáticas de estados, cancelación y porcentajes son correctas, pero no sustituyen estos ensayos físicos.

## Entrega al equipo A

La integración y las dos instalaciones están realizadas por autorización del propietario. Cloud debe valorar este informe, los cambios y las limitaciones; no se marca su revisión como aprobada. La documentación de las entregas anteriores conserva su contexto histórico y este informe actualiza su estado físico.
