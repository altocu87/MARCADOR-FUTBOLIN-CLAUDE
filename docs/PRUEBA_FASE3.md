# Prueba de la fase 3 en la placa (sin perder la versión de ahora)

**Estado:** compilada en GitHub, sin instalar ni probar en la placa. No está publicada: no sale en ACTUALIZAR.

## Qué es

El mismo programa de la pantalla (0.14), con la misma base de Arduino (3.3.12), pero preparada con cuatro ajustes
que recomienda el fabricante del chip para pantallas como la nuestra (`hardware/fase3/platformio.ini`):

- el programa se ejecuta desde la PSRAM: guardar en la memoria del chip ya no deja a la pantalla sin imagen;
- la interrupción de la pantalla va en la memoria rápida (sigue viva mientras se guarda);
- la pantalla se recoloca sola en cada imagen;
- más ancho de banda de la PSRAM (línea de caché de 64 B).

Lo que se ve y se juega no cambia. En AJUSTES pone «Versión 0.14 · prueba fase 3».

## Cómo no se pierde la versión de ahora

La placa tiene **dos huecos** para el programa. La prueba se graba en el que no está en uso; el de ahora se queda
entero en el otro. Volver es solo decirle a la placa con cuál arrancar. Tres formas de volver:

1. En la pantalla: AJUSTES → ACTUALIZACIONES → «VOLVER A LA ANTERIOR» (dos toques).
2. Sola: si la prueba se reinicia por un fallo tres veces seguidas (antes de 2 minutos encendida), vuelve sola.
3. Por USB desde el PC: `python scripts/placa.py volver`.

Si la pantalla quedara en negro sin llegar a arrancar, la forma 3 sigue funcionando (no necesita el programa).

## Pasos (con la placa por USB en el PC, COM6)

Recomendado: instalar antes la 0.14 por Wi-Fi (ACTUALIZAR), para tener la última normal en uno de los huecos.

1. Descargar el programa: GitHub → Actions → «Fase 3 (prueba)» → la última ejecución correcta → artefacto
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

Si todo va igual o mejor que la 0.14 y la imagen ya no salta, se pasa a la compilación normal en GitHub (cambio en
`firmware.yml`), con permiso del dueño. Si algo va peor: volver (arriba) y apuntar qué falló.

## Riesgos conocidos

- El programa ocupa unos 2 MB más en la PSRAM: la caché de imágenes baja de 8 a 6 MB (`ui_assets.h`).
- Compilar tarda más (unos 10 minutos la primera vez en GitHub; luego queda en caché).
- La base de Arduino la prepara pioarduino: al cambiar de versión de Arduino hay que cambiarla también en
  `platformio.ini`.
