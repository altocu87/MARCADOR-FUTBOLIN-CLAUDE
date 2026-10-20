# Mando físico Waveshare ESP32-C3-Zero · equipo B · 20/10/2026

## Identificación y alcance

El dueño ha aportado fotos por ambas caras. La placa real es una **Waveshare ESP32-C3-Zero**, no una C3 Super Mini. Consulta previa a la grabación: COM7, ESP32-C3 QFN32 revisión 0.4, flash XMC integrada de 4 MB, MAC 9c:cc:01:a8:14:80. La pantalla grande continúa en COM6.

Se autoriza instalar el mando. No se autoriza crear partidos ni modificar marcadores para probarlo. Esta entrega adapta únicamente el LED de estado del mando; no cambia la pantalla, el protocolo de radio ni la base de datos.

## Programa y reproducción

Fuente: `hardware/mando_pulsadores/mando_pulsadores.ino`. El perfil original sigue siendo el predeterminado: LED sencillo GPIO8, activo bajo. El perfil opcional `MFV3_MANDO_RGB_PIN=10` usa el WS2812 integrado de la Waveshare C3-Zero, con blanco tenue y apagado real, sin utilizar GPIO8 ni añadir bibliotecas.

Compilar con Arduino-ESP32 3.3.12 y placa `esp32:esp32:esp32c3:CDCOnBoot=cdc`; propiedades C++: `-DMFV3_BUILD=54 -DMFV3_MANDO_RGB_PIN=10`. El número 54 es la base de la adaptación local, **no una nueva publicación oficial**. La versión 0.2 corresponde a la pantalla grande y no se modifica en esta entrega.

La instalación inicial emplea el binario combinado a dirección 0, con su arranque y particiones para C3. Nunca usar el binario de la pantalla S3 ni grabar COM6. Se ha guardado una copia local completa previa de 4.194.304 bytes, fuera del repositorio: SHA256 `dfd2f3bda2973c309894cab7a394778e8fbc9adfff04db1d71b54696cbee9191`.

## Cableado de los contactos del pulsador

Desconectar la alimentación antes de cablear. Los contactos del interruptor no tienen polaridad. Usar **COM y NO** (normalmente abierto); dejar NC sin conectar si existe. No deducir NO/NC por el número de terminal de otra marca: leer su etiqueta o medir continuidad; COM–NO debe estar abierto en reposo y cerrar al pulsar.

| Contacto | Destino en la mini |
|---|---|
| COM del blanco | GND |
| NO del blanco | GPIO3, marcado 3 |
| COM del azul | GND |
| NO del azul | GPIO4, marcado 4 |

Ambos COM comparten el único GND mediante una regleta o derivación segura. No introducir dos cables sueltos mal sujetos en un conector. Las resistencias de entrada son internas; no conectar 5 V al interruptor ni a GPIO3/4.

```text
ESP32-C3-Zero                    Interruptores
3 ---------------------------- NO blanco
4 ---------------------------- NO azul
GND --------+----------------- COM blanco
            +----------------- COM azul
```

Gesto corto: gol del equipo al soltar antes de 0,8 s. Gesto largo: anular el gol de ese equipo. La pantalla decide las reglas y descarta las repeticiones; conserva el control táctil. Grupo de mesa 1 y canal de radio 1 en ambos programas.

## Luces de los pulsadores: pendientes de identificar

El dueño indica que son conjuntos de luz de 5 V y dos bornes separados de los del interruptor. La foto del portalámparas blanco **no permite identificar + y − ni confirmar su consumo o resistencia interna**. No asignar polaridad por izquierda/derecha, color del plástico, tornillos o por ensayo a 5 V invirtiendo conexiones.

Una vez identificados consumo y polaridad, la conexión para luz permanente sería ambos positivos al pin 5V y ambos negativos a GND, siempre que la alimentación USB y las pistas/conexiones admitan la carga total. En la foto posterior de la placa, la columna derecha empieza por 5V, GND y 3V3; usar las etiquetas. No usar 3V3 ni ningún GPIO para alimentar estas luces. No confundir alimentación USB de la placa con los bornes de las luces: el pulsador no tiene USB.

Para identificar la polaridad: referencia o ficha del producto, marcas en la bombilla desmontada o medición adecuada. Una lectura OL en modo diodo puede ser inconcluyente si la luz incorpora resistencia/circuito; no interpretar automáticamente como avería. El programa no controla las luces externas; el perfil RGB solo controla el LED pequeño de la mini.

## Verificación y pendientes para Cloud

### Instalación física comprobada

Se ha grabado correctamente el binario combinado de 4 MB en COM7 y el grabador ha confirmado **Hash of data verified**. Aplicación: 965.744 bytes, 73 % del espacio de programa; variables globales: 38.088 bytes (11 %). SHA256 de la aplicación `d93bcf2b15823ffb7d8ac7d3fce4fe49c11d61eb620bedb14917e10871971c0a`; del combinado `aad24bf51c3f1284efe0ca211eb4b0ad9957915d52873848f537cca9ba3d01e0`.

La lectura del USB del mando confirma `MANDO ESP32-C3 build 54 RGB pin 10` y tres líneas `Enviado: HELLO`; no aparecen órdenes de gol en esa observación. La pantalla grande recibe la señal: captura íntegra 800×480 RGB565, 768.000 bytes, CRC32 `4ce32a76`, con punto verde junto al engranaje de Inicio. En `ui_inicio.h`, ese punto verde corresponde a `mandoLinked`; una actualización pendiente lo volvería dorado. Ver [captura](capturas-placa/mando-c3-zero-conectado.png).

La primera captura mostraba el reloj de reposo. Se despertó mediante un toque en una esquina sin botones y se obtuvo Inicio con el indicador; no se inició ningún partido ni se enviaron goles. Puertos cerrados al acabar. Esto confirma el saludo y su recepción, **no los contactos físicos aún sin cablear ni las luces de los pulsadores**.

- Pasan las pruebas simuladas del mando en ambos perfiles: botones, triple envío, anulación, gesto de armado y transferencia OTA. Se comprueba también la salida RGB y que el perfil Zero no configure GPIO8 como salida.
- Pendientes de prueba física tras el cableado: contactos de ambos botones, luz de cada pulsador y gestos cortos/largos. No afirmar que ya se han probado.
- **Actualizaciones futuras:** el catálogo y el actualizador de la pantalla aún descargan `mando-c3`, perfil Super Mini. La actualización genérica conservaría GPIO3/4 y la radio, pero perdería el control correcto del LED integrado de esta Zero. Antes de publicar soporte oficial, Cloud debe decidir cómo conservar/seleccionar el perfil y verificar la actualización completa. Mientras tanto, mantener este programa y actualizar esta mini por USB con el perfil correcto.
- El límite nominal del receptor OTA es mayor que la partición real predeterminada de la C3; `Update.begin()` es quien rechaza una imagen que no cabe. La adaptación no modifica ese comportamiento.
- La evaluación y aceptación de Cloud siguen pendientes. Esta entrega permanece en la rama del equipo B y no publica firmware ni cambia main.

Fuentes de hardware: [documentación Waveshare](https://www.waveshare.com/wiki/ESP32-C3-Zero) y [esquema oficial](https://files.waveshare.com/wiki/ESP32-C3-Zero/ESP32-C3-Zero-Sch.pdf).
