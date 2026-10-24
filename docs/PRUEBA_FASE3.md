# Prueba de la fase 3 en la placa (sin perder la versión de ahora)

**Estado actualizado 24/10: probada y retirada; no recomendada para jugar.** Incluso con las tablas de interfaz en PSRAM, la fase 3 baja a 3.220 bytes internos mínimos al recorrer menús. La normal 0.15 corregida queda instalada, con 35.208 de mínimo en la última comprobación y menú estable confirmado por el dueño. Ver [informe A/B y binarios concretos](INTEGRACION_A_B_0_15_20261024.md). El workflow de fase 3 solo genera artefactos manuales: **no publica firmware ni sustituye la compilación normal**. No repetir la instalación sin resolver y medir antes el margen interno.

## Qué es

El mismo programa de la pantalla, con la misma base de Arduino (3.3.12), pero preparado con cinco ajustes
que se han evaluado para esta pantalla (`hardware/fase3/platformio.ini`):

- ejecución de código y constantes desde la PSRAM, para reducir interferencias al escribir flash;
- interrupción de pantalla en memoria interna rápida, para atenderla con la caché de flash deshabilitada;
- realineado del panel en el retorno vertical;
- más ancho de banda de la PSRAM (línea de caché de 64 B).
- tablas de ranking, fotos y selección de torneo reservadas en PSRAM después de arrancar, con comprobación de memoria. Catálogo de audio, jugadores, motor e interrupciones siguen internos. No se usa BSS externa.

Las reglas del juego son las mismas. La candidata actual se identifica como «Versión 0.15 · prueba fase 3». La reserva explícita de tablas del último punto también está en la normal 0.15; no requiere este SDK. No se ha demostrado aún una mejora física global de fluidez de fase 3: su consumo interno impide recomendarla.

## Cómo no se pierde la versión de ahora

La placa tiene **dos huecos** para el programa. La prueba se graba en el que no está en uso; el de ahora se queda
entero en el otro. Volver es solo decirle a la placa con cuál arrancar. Tres formas de volver:

1. En la pantalla: AJUSTES → ACTUALIZACIONES → «VOLVER A LA ANTERIOR» (dos toques).
2. Sola: si el programa alcanza el contador de arranques y registra tres fallos seguidos (antes de 2 minutos encendida), vuelve sola. **No cubre todo fallo anterior a ese contador**: el experimento descartado de BSS externa requirió recuperar por USB.
3. Por USB desde el PC: `python scripts/placa.py volver`.

Si la pantalla quedara en negro sin llegar a arrancar, la forma 3 sigue funcionando (no necesita el programa).

## Procedimiento reservado para futuras candidatas corregidas

Antes de otra prueba autorizada: resolver el margen interno, identificar puerto y binario exactos, comprobar que el otro hueco contiene una normal validada (actualmente 0.15) y conservar los datos. **No instalar automáticamente el último artefacto:** los experimentos descartados también pueden haber compilado correctamente.

1. Descargar una candidata corregida e identificada: GitHub → Actions → «Fase 3 (prueba)» → ejecución concreta → artefacto
   `fase3-waveshare-7c` → dentro, `waveshare-7c-fase3-ota.bin`.
2. Ver qué hay en cada hueco: `python scripts/placa.py ranuras`.
3. Grabar la prueba en el hueco libre: `python scripts/placa.py probar waveshare-7c-fase3-ota.bin`.
4. Comprobar que arranca y que en AJUSTES pone «prueba fase 3».

## Qué mirar (grabarlo con el móvil si salta la imagen)

1. Wi-Fi: AJUSTES → ACTUALIZACIONES → BUSCAR, varias veces.
2. Editar la ficha de un jugador y cambiar su foto.
3. La galería de fotos de la tarjeta.
4. Un partido entero con el mando: goles, sonidos y el cartel de victoria.
5. Dejarla en reposo con la cuenta: que sincronice y que el aviso se vea.
6. `python scripts/placa.py orden "MEMORIA"` antes y después de jugar (memoria libre).

Antes de proponer publicarla, comparar memoria y fluidez con la normal 0.15, incluido juego y torneo completo, carga de fotos y reposo con nube. No basta con compilar y ver Inicio. Cambiar el workflow oficial requiere permiso del dueño y resultados favorables; **la candidata probada el 24/10 no cumple el margen requerido**. Si algo va peor: volver (arriba) y apuntar qué falló.

## Riesgos conocidos

- La RAM interna mínima observada fue 3.220 bytes frente a más de 35.000 de la normal corregida: falta aislar el coste de cada opción del SDK. No se ha medido jugando.
- El programa ocupa unos 2 MB más en la PSRAM: el presupuesto de caché de imágenes baja de 8 a 6 MiB (`ui_assets.h`), pero es blando; se observaron 7.123 KiB en Inicio.
- Compilar tarda más (unos 10 minutos la primera vez en GitHub; luego queda en caché).
- La base de Arduino la prepara pioarduino: al cambiar de versión de Arduino hay que cambiarla también en
  `platformio.ini`.
