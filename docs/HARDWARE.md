# Hardware y dispositivos · MARCADOR FUTBOLÍN V3

La app es una web: funciona en **cualquier PC, Mac, tablet o móvil** con un navegador moderno y se puede
**instalar como aplicación** (PWA). Las placas **Arduino / ESP32** se usan para los **pulsadores y sensores de gol**,
LEDs y zumbador, y el ESP32 además puede **servir la app sin Internet** desde su propia red Wi-Fi.

> **¿Quieres montarlo tú?** Empieza por el [Manual «Hazlo tú mismo»](MANUAL_DIY.md) (también en la app:
> Ajustes → Hazlo tú mismo): niveles, placas compatibles, qué pin va a cada pulsador y cómo detectar los goles.
> Este documento es la referencia técnica. Las placas ESP32 también se pueden instalar **desde la web**
> (asistente «Monta tu marcador» → tu placa → «Instalar en mi placa»), sin IDE de Arduino.
> Guía rápida paso a paso para cargar el programa: [`CARGAR_PROGRAMA.md`](CARGAR_PROGRAMA.md).

> Regla de oro: la placa solo avisa de pulsaciones. **Todas las reglas** (bloqueo de 3 s, turnos de penaltis,
> estados del partido) las aplica el motor de la app, igual que si tocaras la pantalla.

---

## 1. Pantallas y dispositivos

| Dispositivo | Cómo usarlo | Notas |
|---|---|---|
| PC / Mac (Chrome, Edge, Firefox, Safari) | Abrir la web o instalarla (icono «Instalar» en la barra) | ⛶ pantalla completa en Inicio o Ajustes |
| Tablet / móvil Android | Chrome → menú → «Instalar aplicación» | Se abre a pantalla completa y en horizontal |
| iPhone / iPad | Safari → Compartir → «Añadir a pantalla de inicio» | Sin USB ni Bluetooth web (usa Wi-Fi con ESP32) |
| Raspberry Pi + pantalla táctil 7" | Chromium en modo quiosco (ver §6) | Opción recomendada para dejarlo fijo en la mesa |
| ESP32-S3 7" (800×480) | **Marcador completo nativo** en la propia placa (táctil, sin PC ni móvil), ver §7. También puede hacer de servidor de la app | |

**Resolución:** el diseño base es 800×480 (la pantalla del ESP32-S3). En pantallas panorámicas (16:9, 21:9) el
lienzo se ensancha hasta 1100 px lógicos y en 4:3/16:10 crece en alto hasta 640, así que **no quedan franjas**.
En un móvil en vertical aparece un aviso para girarlo (se puede seguir en vertical). La app mantiene la pantalla
encendida mientras está abierta (Ajustes → General → Pantalla) y funciona **sin conexión** una vez cargada.

---

## 2. Formas de conectar una placa

| Vía | Placas | Navegadores | Cuándo usarla |
|---|---|---|---|
| **USB (Web Serial)** | Arduino Uno, Nano, Mega, Leonardo, Micro · ESP32 por cable | Chrome / Edge en PC, Mac, Android | Lo más sencillo: placa barata y un cable |
| **Wi-Fi (WebSocket)** | ESP32, ESP32-C3, ESP32-S3 (y ESP8266 adaptando el sketch) | **Todos**, incluido iPhone | Sin cables; la placa crea su red y sirve la app |
| **Bluetooth LE (UART de Nordic)** | ESP32, ESP32-C3, ESP32-S3 | Chrome / Edge en PC, Mac, Android | Sin cables y sin cambiar de red Wi-Fi |

En la app: **Ajustes → Conexiones**. Se ven el estado, el nombre de la placa y un registro de mensajes.
«Reconectar al abrir» vuelve a conectar Wi-Fi y USB automáticamente. Si la app se abre **desde la propia
placa** (http://192.168.4.1), se conecta por Wi-Fi sola.

> Seguridad del navegador: USB y Bluetooth solo funcionan en páginas seguras (https o localhost). Una web en
> **https** no puede conectar con **ws://** (Wi-Fi sin cifrar) salvo que la app la sirva la propia placa por http.

---

## 3. Programas para las placas (`hardware/`)

| Carpeta | Placa | Qué hace |
|---|---|---|
| `hardware/arduino_usb/` | Arduino Uno/Nano/Mega/Leonardo | Pulsadores + sensores → USB; LEDs y zumbador al confirmar gol |
| `hardware/esp32_marcador/` | ESP32 / C3 / S3 | Red propia «MARCADOR-FUTBOLIN», sirve la app, WebSocket + Bluetooth + USB, LEDs y zumbador |
| `hardware/esp32s3_pantalla7/` | ESP32-S3 con pantalla táctil 7" 800×480 | **Marcador activo** en la propia pantalla: configuración, partido, prórroga, penaltis, victoria e historial |
| `hardware/mando_pulsadores/` | ESP32-C3 Super Mini (u otra ESP32) | **Mando inalámbrico** con 2 pulsadores arcade: toque corto = gol, toque largo = anular gol; radio ESP-NOW hasta la pantalla |
| `hardware/common/mfv3_boards.h` | — | **Pines de cada modelo**, elegidos solos al compilar según la placa del IDE (Uno, Nano, Mega, Leonardo, Pico, ESP32, C3, S3, S2) |
| `hardware/common/mfv3_core.h` | — | Antirrebote, disparo por flanco, toque corto/largo y protocolo (compartido, probado en PC) |
| `hardware/common/mfv3_radio.h` | — | Paquete de radio del mando (grupo de mesa, secuencia, suma de control, descarte de repeticiones) |
| `hardware/common/mfv3_engine.h` | — | Motor del partido en C++ (mismas reglas que la app, criterios A01–A13 probados en PC) |

### Arduino (USB)
1. Instala el **IDE de Arduino**. Abre `hardware/arduino_usb/arduino_usb.ino`.
2. Ajusta los pines al principio del archivo (y `USE_SENSORS = true` si pones sensores).
3. Herramientas → Placa y Puerto → **Subir**.
4. Conecta la placa al PC/Android, abre la app en Chrome/Edge → Ajustes → Conexiones → USB → **Conectar**.

### ESP32 (Wi-Fi + Bluetooth + USB, y servidor de la app)
1. IDE de Arduino → Gestor de placas → instala **esp32 (Espressif)**.
   Gestor de bibliotecas → instala **WebSockets** (Markus Sattler).
2. Abre `hardware/esp32_marcador/esp32_marcador.ino`, revisa pines, red y `USE_BLE`/`USE_SENSORS`.
3. Compila y sube.
4. Copia la app a la placa: en la carpeta del proyecto ejecuta **`npm run build:esp32`** (compila y deja la
   app comprimida en `hardware/esp32_marcador/data/`, unos 170 KB). Después sube esa carpeta con el plugin
   **«LittleFS Upload»** del IDE (Ctrl+Mayús+P → *Upload LittleFS to Pico/ESP8266/ESP32*).
   Usa un esquema de particiones con espacio para LittleFS (p. ej. *Default 4MB with spiffs*).
5. Conecta el móvil/tablet a la red **MARCADOR-FUTBOLIN** (clave `futbolin123`) y abre **http://192.168.4.1**
   (o http://marcador.local). Opcional: rellena `STA_SSID/STA_PASS` para que también se una a tu Wi-Fi.

> Estado de verificación: todos los programas se **prueban en PC** con una simulación de pines y tiempo
> (`npm run test:hardware`) y se **compilan de verdad en GitHub Actions** para las 13 placas del catálogo
> (flujo «Firmware»). Ninguna placa física se ha probado todavía: haz una primera prueba por USB con el monitor serie.

---

## 4. Cableado

### Pines por defecto

| Señal | Arduino Uno/Nano | ESP32 DevKit | ESP32-C3 SuperMini (sugerido) |
|---|---|---|---|
| Pulsador gol Blanco | D2 | GPIO 4 | GPIO 2 |
| Pulsador gol Azul | D3 | GPIO 5 | GPIO 3 |
| Pulsador pausa (opcional) | D4 | GPIO 18 | GPIO 4 |
| Sensor portería Blanco (opcional) | D5 | GPIO 19 | GPIO 5 |
| Sensor portería Azul (opcional) | D6 | GPIO 21 | GPIO 6 |
| LED Blanco | D9 | GPIO 22 | GPIO 7 |
| LED Azul | D10 | GPIO 23 | GPIO 10 |
| Zumbador | D8 | GPIO 25 | GPIO 1 |

### Pulsadores arcade (100 mm, con LED de 5/12 V)
- El **interruptor** del botón es un contacto seco: un terminal al **pin**, el otro a **GND**. El programa usa
  `INPUT_PULLUP` (activo al pulsar = LOW). No hace falta resistencia.
- El **LED** del botón va a otra tensión (5 V o 12 V): **nunca** directo al pin. Usa un transistor/MOSFET
  de nivel lógico (p. ej. IRLZ44N, AO3400) o un ULN2003: pin → puerta/entrada; el LED entre +12 V y el drenador.
- ESP32 = 3,3 V. **Ningún pin del ESP32 admite 5 V o 12 V.** Arduino Uno/Nano = 5 V.

### Sensores de gol (opcionales)
- Recomendado: **barrera infrarroja** (emisor + receptor, p. ej. 3 mm/5 mm «IR break beam») en la boca o en el
  canal de bajada de la portería. La salida del receptor suele ser de colector abierto → al pin con `INPUT_PULLUP`;
  alimenta el receptor a 3,3 V con ESP32.
- El sketch filtra rebotes (5 ms) y no repite antes de 800 ms; el motor de la app aplica además el bloqueo de 3 s.
- Alternativas: sensor de vibración/microrruptor en la bandeja de bolas. Hay que probar latencia y fiabilidad.

### Alimentación
- Arduino por USB del PC/tablet. ESP32 con fuente USB de 5 V/1 A; si los LED son de 12 V, fuente de 12 V
  aparte con **masa común** (GND unidas) y regulador a 5 V para la placa.

---

## 5. Protocolo MFV3 (para quien quiera hacer su propio firmware)

Texto UTF-8, **una orden por línea** terminada en `\n`, igual por USB (115200 baudios), WebSocket o Bluetooth.

| Placa → app | Significado |
|---|---|
| `HELLO <modelo> <versión> caps=a,b,…` | Saludo con el modelo de placa (id del catálogo, p. ej. `arduino-uno`, `esp32c3`, `waveshare-7c`) y lo que sabe hacer (`goles,anular,pausa,sensores,leds,zumbador,wifi,bluetooth,servidor,pantalla,radio`). La app muestra el nombre, las capacidades y el esquema de esa placa. Se repite cada 2 s por USB hasta recibir respuesta |
| `GOL_BLANCO [button\|sensor]` / `GB` | Gol de Blanco |
| `GOL_AZUL [button\|sensor]` / `GA` | Gol de Azul |
| `ANULAR_BLANCO` / `AB` | Anular el último gol del Blanco (−1). En penaltis deshace el último lanzamiento |
| `ANULAR_AZUL` / `AA` | Anular el último gol del Azul (−1). En penaltis deshace el último lanzamiento |
| `PAUSA` | Pausa / continuar |
| `SALTAR` | Saltar la cuenta atrás |
| `PING` | La app responde `PONG` |

| App → placa | Significado |
|---|---|
| `HELLO MARCADOR_V3 1` | La app está conectada |
| `STATE <fase>` | `idle`, `countdown`, `playing`, `paused`, `periodEnd`, `penalties`, `finished` |
| `SCORE <blanco> <azul>` | Marcador ordinario |
| `GOAL BLANCO\|AZUL` | Gol **aceptado** (para LED/zumbador) |
| `LOCK 3000` | Bloqueo de gol activo (ms) |
| `WIN BLANCO\|AZUL` | Fin del partido |
| `VOLUMEN <0–100>` | Volumen del altavoz de la placa (solo placas con `altavoz`; 0 = sin sonido). Se guarda |
| `SONIDO <momento>` | Toca un momento en la placa (`gol`, `remontada`… o el id de la app: `goal`, `comeback`…), para probar |

**Sonidos propios (previsto, fase 3).** Cada sonido de la mesa sabe dónde está guardado: en el aparato, en la memoria
del chip o en la tarjeta SD de la placa. La app ya reconoce dos capacidades reservadas para cuando la placa tenga
altavoz: `sonido` (guarda y toca sonidos en su memoria) y `sd` (además, en su tarjeta SD). Solo con ellas se pueden
elegir «Chip de la placa» o «Tarjeta SD» al subir un sonido. La orden para tocarlo y la forma de pasarle los archivos
se definirán en la fase 3. Mientras tanto, o si la placa no está conectada, suena el de siempre en la app.

Durante la cuenta atrás, un gol de pulsador **solo la salta** (no suma). En penaltis, un gol del equipo que
lanza cuenta como acierto; si no es su turno, el motor lo rechaza.

---

## 6. Mesa fija con Raspberry Pi (pantalla táctil completa)

Una Raspberry Pi 4/5 (o Zero 2 W) con pantalla táctil de 7" 800×480 ejecuta la app completa:

```bash
# En la Raspberry Pi OS con escritorio
chromium-browser --kiosk --noerrdialogs --disable-infobars --check-for-update-interval=31536000 \
  http://192.168.4.1   # o la URL donde esté publicada / un servidor local con dist/
```

Los pulsadores van a un Arduino por USB o a un ESP32 por Wi-Fi/Bluetooth, igual que con un PC.

---

## 7. ESP32-S3 con pantalla de 7" (versión activa)

Placa de referencia: **Waveshare ESP32-S3-Touch-LCD-7C** (se vende como «ESP32-S3 7inch AI Voice Touch Display,
800×480, 5-Point Touch, Wi-Fi & BLE 5», también en caja: 7C-BOX). Los pines salen del repositorio oficial del
fabricante (github.com/waveshareteam/ESP32-S3-Touch-LCD-7C). El sketch
`hardware/esp32s3_pantalla7/` convierte la placa en **el marcador completo**, sin PC, móvil ni Internet:

- **Inicio:** POR GOLES / POR TIEMPO / AMBAS, goles y minutos por parte con − / +, botón JUGAR. Muestra el último
  resultado y los partidos jugados.
- **Partido:** el número grande de cada equipo es el botón de gol; −1, DESHACER, PAUSA, reloj, bloqueo de 3 s
  visible, «BOLA DE PARTIDO», cuenta atrás (toca para saltar), 1ª y 2ª parte, prórroga con gol de oro, penaltis
  con muerte súbita y pantalla de victoria con REVANCHA.
- **Mismas reglas que la app:** usa `mfv3_engine.h`, el motor portado a C++ y probado en PC con los criterios A01–A13.
- **Memoria:** guarda la configuración y los 8 últimos resultados aunque se apague.
- **Ahorro:** fuera de partido baja el brillo a los 5 min; el primer toque solo la despierta.
- **Sonido (7C):** por el altavoz de la placa suenan los mismos momentos que en la app: empieza el partido, gol,
  gol anulado, empate, 3 seguidos, remontada, bola de partido, gol de oro, penalti marcado/fallado, fin de la parte,
  victoria y goleada. Sin tarjeta SD suenan los tonos de siempre de la app. El sonido va aparte: la pantalla nunca
  espera al altavoz, y un momento nuevo corta el que suena. Volumen con la orden `VOLUMEN` (se guarda).
- **Tarjeta SD (7C), sonidos propios:** microSD en FAT32 con una carpeta por momento dentro de `/sonidos/`:

  | Carpeta | Momento | Carpeta | Momento |
  |---|---|---|---|
  | `gol` | Gol | `penalti-marcado` | Penalti marcado |
  | `gol-anulado` | Gol anulado | `penalti-fallado` | Penalti fallado |
  | `3-seguidos` | 3 seguidos | `empieza` | Empieza el partido |
  | `remontada` | Remontada | `fin-de-parte` | Fin de la parte |
  | `empate` | Empate | `victoria` | Victoria |
  | `bola-de-partido` | Bola de partido | `goleada` | Goleada |
  | `gol-de-oro` | Gol de oro | | |

  Dentro, uno o varios `.wav` (hasta 8; si hay varios, uno al azar). Un archivo llamado `ninguno` deja ese momento
  en silencio; una carpeta vacía o que no existe, el de siempre. Formato: WAV PCM de 8 o 16 bits, mono o estéreo,
  8–48 kHz (los que prepara la app en Ajustes → Sonidos, 22 050 Hz, valen tal cual). Se leen al encender la placa.
- **Pantalla de prueba:** el primer arranque abre «PRUEBA DE LA PLACA»: toca las 4 esquinas, pulsa los dos botones
  del mando y (opcional) pasa la bola por los sensores. Muestra también si la radio funciona y el último toque.
  Se repite cuando quieras con el botón PRUEBA de la pantalla de inicio. En la 7C, cada esquina tocada suena (si se
  oye, el altavoz va) y abajo se ve si hay tarjeta SD, su tamaño y cuántos momentos traen sonido propio.
- **Mando inalámbrico de 2 pulsadores** (ver §7.1): toque corto = gol, toque largo = anular gol. En la pantalla
  de inicio aparece «MANDO CONECTADO» cuando lo oye.
- **Sensores de gol (Waveshare 7C):** van directamente a las **entradas aisladas** de la placa, sin Arduino:
  **DI0 = gol del Blanco**, **DI1 = gol del Azul** (más su borne común). El estado que tienen al encender se toma
  como «sin balón», así que vale cualquier sensor (NPN/PNP, normalmente abierto o cerrado): **enciende la placa
  sin balón delante de los sensores**. Las salidas **DO0 / DO1** se activan 1 segundo con cada gol del Blanco /
  Azul (luz, tira LED o relé). Comprueba en la wiki de Waveshare la tensión admitida por DI/DO antes de cablear.
- **Otras placas:** conecta un Arduino o ESP32-C3 con el sketch `arduino_usb` al UART de la placa
  (TX del Arduino → RX de la placa). La placa contesta `GOAL/LOCK/WIN` para que el Arduino encienda LEDs y zumbador.
  ⚠ Un Arduino Uno/Nano trabaja a 5 V: pon un divisor (1 kΩ + 2 kΩ) en su TX antes de entrar al RX de 3,3 V.
  También acepta las mismas órdenes por el USB de la placa (`GB`, `GA`, `AB`, `AA`, `PAUSA`, `SALTAR`, `PING`, `HELLO`).

### 7.1 Mando inalámbrico con 2 pulsadores arcade (`hardware/mando_pulsadores/`)

Una mini placa **ESP32-C3 Super Mini** (unos 3–5 €; vale cualquier ESP32) con los dos pulsadores arcade. Envía
cada pulsación por radio **ESP-NOW** (2,4 GHz, la radio que ya llevan las dos placas): sin Wi-Fi, sin router y sin
emparejar. Alcance típico de decenas de metros; de sobra para una mesa.

| Gesto | Qué hace | LED del mando |
|---|---|---|
| Toque corto (se suelta antes de 0,8 s) | **Gol** de ese equipo (en la cuenta atrás, la salta; en penaltis, gol del lanzamiento) | 1 destello |
| Toque largo (mantener 0,8 s) | **Anular** el último gol de ese equipo (−1); en penaltis, deshace el último lanzamiento | 3 destellos |

- **Cableado:** pulsador BLANCO entre **GPIO 3** y **GND**; pulsador AZUL entre **GPIO 4** y **GND** (sin
  resistencias: la placa usa las internas). Si tus pulsadores llevan LED, ese LED se alimenta aparte (5/12 V).
- **Alimentación:** USB-C (cargador o batería externa) o batería LiPo de 3,7 V con un módulo cargador.
- **Las mismas reglas:** el bloqueo de 3 s, los turnos de penaltis, etc. los decide la pantalla; el mando solo
  envía pulsaciones. Cada orden se envía 3 veces y la pantalla cuenta solo una.
- **Dos mesas cerca:** cambia `GRUPO_MESA` (mismo número en el mando y en `esp32s3_pantalla7.ino`).
- **Ajustes** al principio del sketch: pines, LED, duración del toque largo (`PULSACION_LARGA_MS`).
- **Cargar:** IDE de Arduino → placa **ESP32C3 Dev Module**, «USB CDC On Boot: Enabled» → Subir. En el monitor
  serie verás «Enviado: GOL_BLANCO button», etc.

### Cómo cargarlo
1. IDE de Arduino con el paquete **esp32 de Espressif** y la biblioteca **LovyanGFX** (Gestor de bibliotecas).
2. Abre `hardware/esp32s3_pantalla7/esp32s3_pantalla7.ino`.
3. `board_config.h` ya viene con **`BOARD_WAVESHARE_7C`**. Otras opciones: `BOARD_ELECROW_7` (CrowPanel 7.0"),
   `BOARD_WAVESHARE_7` (ESP32-S3-Touch-LCD-7, sin «C») o `BOARD_CUSTOM`.
4. Herramientas → Placa **ESP32S3 Dev Module**, PSRAM **OPI PSRAM**, Flash **16 MB** (la 7C trae 32 MB: 16 MB
   basta), particiones **Huge APP**, USB CDC On Boot **Enabled** (para ver los mensajes por el USB).
5. **Subir.**

### Limitaciones conocidas
- Compila en GitHub (núcleo esp32 3.3 + LovyanGFX 1.2), pero no se ha probado en la placa real: la lógica
  (motor, botones, protocolo, memoria, sonido) se prueba en PC (`npm run test:hardware`) y el dibujo se ve en las
  capturas del PC (abajo), pero falta comprobarlo en una pantalla de verdad.
- Los pines de pantalla y táctil **cambian según el fabricante**: si la pantalla sale en negro o desplazada, revisa
  el perfil en `board_config.h`.
- Los textos de la placa van sin tildes (las fuentes integradas no las incluyen).
- En la 7C no queda un UART libre (el audio usa GPIO43/44): los sensores van a DI0/DI1 y el PC por USB.
- El altavoz y la SD (códec ES8389, pines y secuencia del ejemplo del fabricante) solo se han probado en el PC: el
  primer encendido real se comprueba con la pantalla de prueba. Aún no usa el micrófono ni el Bluetooth, y la app
  todavía no le pasa sus sonidos (de momento se copian a la SD desde el ordenador). La radio Wi-Fi se usa para el
  mando (ESP-NOW); sincronizar con la app queda para una versión futura.

### Ver las pantallas sin la placa
`sh hardware/test/capturas.sh` compila el sketch en el PC con la **misma LovyanGFX** que usa la placa, dibujando en
un lienzo de 800×480, recorre las pantallas (prueba, inicio, cuenta atrás, partido, pausa, victoria) y guarda un
PNG de cada una en `docs/capturas-placa/`. Necesita g++, zlib y git (la primera vez descarga LovyanGFX).

---

## 8. Problemas frecuentes

| Problema | Solución |
|---|---|
| «USB no disponible aquí» | Usa Chrome o Edge (no Firefox/Safari). En Android, cable OTG. |
| No aparece el puerto | Cierra el monitor serie del IDE de Arduino (solo un programa puede usar el puerto). |
| Wi-Fi «Reintentando…» | Comprueba que estás en la red MARCADOR-FUTBOLIN y la IP (ws://192.168.4.1:81/). |
| Bluetooth no encuentra la placa | Activa Bluetooth y ubicación en Android; la placa se anuncia como «MFV3-xxxx». |
| El mando no hace nada | Mira que en la pantalla ponga «MANDO CONECTADO» (el mando saluda cada 20 s); revisa que `GRUPO_MESA` sea el mismo en los dos sketches. |
| Un toque corto anula el gol | Sube `PULSACION_LARGA_MS` en el mando (por defecto 800 ms). |
| Goles dobles por rebote | Sube el antirrebote o el `holdOff` en el sketch; el bloqueo de 3 s de la app ya evita dobles goles. |
| En http://192.168.4.1 no se instala como app | Los navegadores solo instalan/cachean en https o localhost; la placa ya la sirve sin Internet. |
