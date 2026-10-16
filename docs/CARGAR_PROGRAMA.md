# Cómo cargar el programa en una placa

Guía rápida para hacerlo tú solo. Es **igual para todas las placas ESP32** (la pantalla de 7", el mando de
pulsadores y las ESP32 sueltas): se instala **desde la web, con un clic**. Solo las placas Arduino clásicas
(Uno, Nano, Mega, Leonardo, Pico) necesitan el programa «IDE de Arduino» (camino B).

## Qué necesitas

- Un **ordenador** (Windows, Mac o Linux) con **Chrome o Edge**. No vale el móvil ni Safari.
- Un **cable USB de datos**. Muchos cables solo cargan: si la placa no aparece, prueba con otro cable.

## Camino A · Desde la web (todas las ESP32) — recomendado

1. Abre **https://altocu87.github.io/MARCADOR-FUTBOLIN-CLAUDE/#/monta** en Chrome o Edge.
2. Entra en tu placa (o directamente con estos enlaces):

   | Placa | Enlace |
   |---|---|
   | Pantalla 7" Waveshare 7C / 7C-BOX | `…/#/placa/waveshare-7c` |
   | Mando de 2 pulsadores (ESP32-C3 Super Mini) | `…/#/placa/mando-c3` |
   | Pantalla 7" Waveshare (sin «C») | `…/#/placa/waveshare-7` |
   | Pantalla 7" Elecrow CrowPanel | `…/#/placa/elecrow-7` |
   | ESP32 / C3 / S3 / S2 sueltas | `…/#/placa/esp32`, `esp32c3`, `esp32s3`, `esp32s2` |

3. Conecta la placa al ordenador por USB.
4. Pulsa **«Instalar en mi placa»** → elige el puerto (suele llamarse «USB JTAG/serial» o «USB Serial») →
   **Instalar**. Si pregunta si quieres **borrar el dispositivo**, márcalo la primera vez (placa nueva). Tarda
   alrededor de un minuto. No desconectes el cable mientras tanto.
5. Al terminar, la placa arranca sola.

**Si la placa no aparece en la lista:** otro cable; y si sigue sin salir, **mantén pulsado el botón BOOT** de la
placa mientras la conectas al USB, suéltalo y vuelve a pulsar «Instalar».

### Lo que pasa después, según la placa

| Placa | Al encender por primera vez |
|---|---|
| Pantalla 7" | Sale **«PRUEBA DE LA PLACA»**: toca las 4 esquinas (cada una suena), pulsa los dos botones del mando y TERMINAR. |
| Mando de pulsadores | Nada a la vista. En la pantalla de 7" aparecerá **«MANDO CONECTADO»** (tarda hasta 20 s). |
| ESP32 sueltas | Crea la red Wi-Fi «MARCADOR-FUTBOLIN» (clave `futbolin123`) o se conecta por USB/Bluetooth a la app. |

### Orden recomendado para la mesa

1. Pantalla de 7" → 2. Mando → 3. Enciende los dos y comprueba en la pantalla de prueba que el mando se oye.

## Camino B · Con el IDE de Arduino (Arduino Uno, Nano, Mega, Leonardo, Pico)

También sirve para cualquier ESP32 si el camino A falla.

1. Instala el **IDE de Arduino** (arduino.cc/en/software).
2. Descarga el proyecto: en GitHub, botón verde **Code → Download ZIP**, y descomprímelo.
3. La página de tu placa en la web (camino A, punto 2) dice **qué paquete instalar y qué opciones elegir** en
   Herramientas: cópialas tal cual.
4. Abre `hardware/<programa>/<programa>.ino` (la página de la placa te dice cuál) → elige placa y puerto →
   pulsa **Subir** (la flecha).

## Tarjeta SD de la pantalla de 7" (sonidos propios, opcional)

1. Formatea la microSD en **FAT32** en el ordenador.
2. Crea la carpeta `sonidos` y, dentro, una carpeta por momento: `gol`, `victoria`, `remontada`… (la lista
   completa está en `docs/HARDWARE.md`, apartado 7).
3. Mete dentro tus `.wav` (si hay varios, suena uno al azar). Un archivo llamado `ninguno` deja ese momento sin
   sonido. Sin tarjeta, suenan los sonidos de siempre.
4. Mete la tarjeta con la placa **apagada** y enciéndela: los sonidos se leen al arrancar.

## Actualizar a una versión nueva

Igual que la primera vez (camino A). La web siempre tiene **la última versión publicada**: cada cambio que se
aprueba en la rama principal (`main`) se compila solo en GitHub y aparece en el instalador unos minutos después.
Cuando pregunte si quieres **borrar el dispositivo**, déjalo **sin marcar**: así se conservan la configuración y
el historial guardados en la placa.

## Problemas típicos

| Pasa esto | Prueba esto |
|---|---|
| El botón dice que el navegador no puede | Usa Chrome o Edge en un ordenador (no iPhone, no Safari, no Firefox). |
| «El programa de esta placa todavía no está publicado» | Aún no se ha compilado en `main`: usa el camino B o espera a la próxima versión. |
| No aparece ningún puerto | Otro cable USB; cierra el IDE de Arduino si está abierto; mantén BOOT al conectar. |
| La pantalla de 7" se queda en negro | Desconecta y vuelve a conectar. Si sigue igual, apunta qué ves y pásalo para revisarlo. |
| El mando no conecta | Que los dos tengan el mismo número de mesa (`GRUPO_MESA`, viene igual de fábrica) y estén a menos de unos metros. |
