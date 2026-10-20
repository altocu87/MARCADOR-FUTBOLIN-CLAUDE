# Equipo B: cinco mejoras adicionales para revisión de Cloud

> **19/10/2026 · Actualización de estado:** el propietario ha autorizado la integración completa en `main`. La revisión de Cloud y las pruebas físicas siguen pendientes. El estado actual prevalece sobre las notas históricas de esta entrega y se mantiene en [ESTADO.md](ESTADO.md).

Fecha: 19/10/2026. Rama: `equipo-b/rendimiento-esp32s3`.
Parte de la primera entrega `8c8f5c0cfe73a5e8bd5511b5e23c354028fcde6b`. El usuario ha autorizado estas cinco mejoras en la rama, **sin fusionar con main**. Continúa pendiente la aprobación del equipo A y la prueba física.

## Qué cambia para quien juega

1. Durante el partido se reutilizan los nombres, fichas y textos que permanecen iguales. El reloj, los marcadores y los hándicaps siguen actualizándose.
2. La racha deja de recorrer los goles en cada vuelta de dibujo. Se recalcula cuando se notifica un cambio del motor, incluidos goles, anulaciones y deshacer.
3. Preparar una foto para subirla necesita menos memoria temporal. El formato, los colores y la orientación siguen siendo los mismos.
4. Al salir del ranking/historial y regresar se recuerdan pestaña, temporada/histórico, modalidad, jugador y página. Al apagar se olvidan: no se escribe esta preferencia en flash. Volver desde Inicio abre la lista; volver del reposo conserva también el detalle.
5. Inicio muestra «1 cambio por subir» o «N cambios por subir» cuando hay cuenta y pendientes. Se oculta al quedarse vacía la cola o salir de la cuenta. **Son cambios, no un recuento de partidos únicos**: la cola también contiene fichas y eliminaciones de jugadores, y puede contener repeticiones.

## Implementación, coste y revisión

| Cambio | Archivo / función | Coste y límites |
|---|---|---|
| Presentación estable del partido | `esp32s3_pantalla7.ino`: `prepareMatchFixed`, `matchView`, `startMatch`, tratamiento de `jugadores.changed` en `loop` | Copia fija de `partido::View`, 2.300 bytes. No añade reservas dinámicas ni temporizadores. Se invalida al iniciar/repetir partido, cambiar de parte o actualizar jugadores. El coste de copiar la vista sigue existiendo; se evitan búsquedas y formateos repetidos. |
| Racha compartida entre celebración y marcador | `lastStreak`, `afterChange` | Dos bytes de resultado y una marca de invalidación; al inicio se reinicia. También se invalida en notificaciones como pausa: pequeño trabajo ocasional para mantener la corrección, nunca un recorrido por cada fotograma. Toda futura mutación del motor debe pasar por `afterChange` o invalidar la racha. |
| BMP/base64 por filas | `s3_nube.h`: `photoDataUrl` | Desaparece el bloque BMP temporal de **43.254 bytes de heap**. Se usan una cabecera de 54 y una fila de 360 bytes en pila. Sigue existiendo la cadena base64 completa (~57,7 KB), el RGB565 de entrada y las demás copias de la sincronización. No se ha convertido la subida en streaming ni se ha cambiado el servicio remoto. |
| Navegación recordada | `ui_historial.h`: `show` | Reutiliza variables existentes. Conserva filtros y ajusta la página al número de resultados; borra el filtro por jugador si ese jugador ya no existe. Se actualizan los cálculos al entrar con la caché de la primera entrega. |
| Pendientes en Inicio | `ui_inicio.h`: `build`, `setStatus`; `render` | Una etiqueta LVGL, un puntero y el último contador. Solo cambia su texto/visibilidad al cambiar el número o reconstruir Inicio. Usa fuente existente y `nube::pendingCount`; no añade consultas de disco, red ni animaciones. |

La memoria fija adicional no es un ahorro: se intercambia por menos trabajo repetido durante el partido. El ahorro temporal de la foto ocurre durante su conversión y **no se debe multiplicar por todas las fotos de la cola**. No se promete coste cero ni una mejora concreta de FPS sin medir en la placa.

### Compatibilidad del formato de foto

El BMP conserva cabecera de 54 bytes, 120 × 120 píxeles BGR de 24 bits y filas de abajo arriba. Tanto cabecera como filas son múltiplos de tres bytes; por eso sus fragmentos base64 se concatenan sin introducir relleno intermedio. La cadena conserva el prefijo `data:image/bmp;base64,` y su terminador. La entrada nula devuelve fallo.

## Pruebas realizadas

- `sh hardware/test/check_copies.sh`: correcto; incluye las simulaciones existentes, tres variantes de pantalla y comprobación de prototipos de Arduino.
- `hardware/test/s3_lightweight_test.cpp`: fichas y textos estables con reloj en movimiento; rachas contrastadas contra los eventos del motor tras goles, anulaciones, deshacer y pausa; reinicio de racha al repetir; cambio de parte; actualización de nombre por el cauce de `loop`; conservación de pestaña/filtros/página; jugador desaparecido y reducción de páginas; singular/plural, ocultación y conexión real del contador a Inicio.
- `hardware/test/nube_memory_test.cpp`: conserva sus pruebas de cola/concurrencia/fallos y añade tres fotos (negra, blanca y patrón de colores). Verifica todos los bytes de la cabecera y de cada píxel, orientación y vuelta BMP/base64 → RGB565 sin diferencias.
- `npm test`: 304 pruebas correctas, 24 archivos.
- Compilación real ESP32-S3 correcta (ESP32 3.3.12 / LVGL 9.5.0, mismo FQBN que la primera entrega): programa **1.860.675 bytes**; variables globales **121.520 bytes**. Respecto a la primera entrega: +432 bytes de programa y +2.328 bytes de globales. No representa el uso dinámico de memoria en funcionamiento. Persisten las advertencias previas de `hcIcon`, `volatile` y el enlazador.
- Captura de Inicio con LVGL real y recursos locales existentes: revisada visualmente, sin solapamiento con las tarjetas o el aviso de batería. Es una simulación de PC, no una captura USB de la placa.

La prueba nueva queda incluida en el script de hardware y, por tanto, en el workflow «Pruebas». Para ejecutarla sola: `sh hardware/test/s3_build.sh 4 test/s3_lightweight_test.cpp`. Opcionalmente definir `MFV3_ASSETS_VERIFY` con la carpeta FAT existente, en ruta absoluta o relativa a `hardware/`; así exporta `build/inicio-pendientes-equipo-b.ppm` para revisión visual. Los registros locales son `build/hardware-lightweight.log`, `build/lightweight-final.log`, `build/cloud-lightweight-test.log`, `build/app-lightweight.log` y `build/firmware-lightweight.log`.

![Inicio simulado: contador de pendientes junto al aviso de batería](capturas-placa/inicio-pendientes-equipo-b.png)

## Lo que debe comprobar el equipo A antes de aprobar

- Ninguna ruta nueva debe modificar jugadores durante un partido sin señalar `jugadores.changed`, ni mutar goles sin notificar a la presentación. Las copias no guardan punteros al almacén.
- Probar partidas rápidas, por tiempo y Partido Loco; anular/deshacer, cambiar de parte, entrar/salir de reposo y repetir con otros equipos. La batería de simulación general cubre reglas adicionales; la prueba nueva no sustituye una sesión con pulsadores reales.
- Confirmar el margen de RAM interna y de pila. Compilar informa del tamaño estático, no de los picos de TLS, imágenes y tareas durante la ejecución.
- Medir antes/después: máximo intervalo de `loop`, latencia del pulsador y memoria libre/bloque mayor. Verificar que el pequeño coste de la etiqueta no empeora Inicio de forma apreciable.
- Confirmar que el contador de cambios es comprensible. Cero pendientes **no garantiza** que una descarga haya concluido: este indicador representa la cola local de subida.
- Revisar estado final de Actions en la rama; una ejecución en cola no equivale a una prueba superada.

No se ha grabado la placa ni modificado datos reales, Supabase, las reglas del motor o la app web. La recepción por lotes y los límites de almacenamiento/concurrencia descritos en [la primera entrega](ENTREGA_EQUIPO_B_RENDIMIENTO_ESP32S3.md) siguen pendientes y no forman parte de estas cinco mejoras.

Revisión Cloud: pendiente. Validación física: pendiente. Fusión: pendiente de autorización.
