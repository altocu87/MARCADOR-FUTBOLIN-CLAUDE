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
| `HELLO <modelo> <versión> caps=a,b,…` | Saludo con el modelo de placa (id del catálogo, p. ej. `arduino-uno`, `esp32c3`, `waveshare-7c`) y lo que sabe hacer (`goles,anular,pausa,sensores,leds,zumbador,wifi,bluetooth,servidor,pantalla,radio,reloj`). La app muestra el nombre, las capacidades y el esquema de esa placa. Se repite cada 2 s por USB hasta recibir respuesta |
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
| `HORA AAAA-MM-DD HH:MM:SS` | Hora local para el reloj con pila (placas con `reloj`; la app la manda al conectar). Respuesta `HORA_OK` o `HORA_ERROR` |
| `STATE <fase>` | `idle`, `countdown`, `playing`, `paused`, `periodEnd`, `penalties`, `finished` |
| `SCORE <blanco> <azul>` | Marcador ordinario |
| `GOAL BLANCO\|AZUL` | Gol **aceptado** (para LED/zumbador) |
| `LOCK 3000` | Bloqueo de gol activo (ms) |
| `WIN BLANCO\|AZUL` | Fin del partido |
| `VOLUMEN <0–100>` | Volumen del altavoz de la placa (solo placas con `altavoz`; 0 = sin sonido). Se guarda |
| `SONIDO <momento>` | Toca un momento en la placa (`gol`, `remontada`… o el id de la app: `goal`, `comeback`…), para probar |

**Sonidos propios** (pantalla de 7" con altavoces; ver §7.2). La placa los anuncia en su saludo con `sonido`
(tiene altavoz y guarda sonidos en la memoria del chip) y `sd` (además, en la tarjeta SD). Cada sonido se conoce en la
placa por un **código** de 8 cifras hexadecimales: el FNV-1a de 32 bits del id del sonido en la app.

| App → placa | Placa → app | Significado |
|---|---|---|
| `SONIDOS?` | `SONIDOS chip=<KB libres\|-> sd=<KB libres\|-> <código>@c,<código>@s,…` | Qué sonidos tiene y dónde (`c` chip, `s` SD). La app lo pide al conectar |
| `SONIDO_SUBIR <chip\|sd> <código> <bytes>` | `SONIDO_LISTO <código>` o `SONIDO_ERROR <código> <motivo>` | Empieza a recibir un sonido (WAV PCM de 16 bits, 1 MB como mucho) |
| `SONIDO_DATOS <base64>` | `SONIDO_OK <bytes recibidos>` | Un trozo de 192 bytes. La app espera la respuesta antes del siguiente |
| `SONIDO_FIN <crc32>` | `SONIDO_GUARDADO <código>` o `SONIDO_ERROR <código> crc` | Termina: si el CRC32 cuadra, el sonido queda guardado (si no, se descarta) |
| `SONIDO_BORRAR <código>` | `SONIDO_BORRADO <código>` | Lo borra (del chip y de la SD) |
| `SONIDO <código> [cola]` | — | Lo toca ya (corta lo que suene) o, con `cola`, al acabar lo que suena |
| `MOMENTO <momento> <def\|no\|código,código…>` | `MOMENTO_OK <momento>` | Qué suena en ese momento cuando la placa juega sola: el de siempre, nada o uno al azar de hasta 4 |
| `VOLUMEN <0–100>` | — | Volumen de los altavoces (se guarda) |

Motivos de error: `sin_sd`, `sin_memoria`, `sin_espacio`, `tamano`, `escritura`, `crc`, `tiempo` (8 s sin datos).
Momentos: `goal`, `goalAnnulled`, `streak3`, `comeback`, `equalizer`, `matchPoint`, `goldenGoal`, `penaltyScored`,
`penaltyMissed`, `matchStart`, `periodEnd`, `victory`, `rout` (los mismos nombres que en la app).

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
- **Reloj con pila (Waveshare 7C):** la hora sale arriba a la izquierda en Inicio y, en reposo, en grande con la
  fecha; cada resultado guarda su fecha y hora. Chip PCF85063A con pila **CR927** en su soporte (sin pila, la hora
  se pierde al apagar). Se pone en hora sola: la app se la manda al conectarla por USB, y por Internet cada vez que
  la placa se conecta al Wi-Fi a mirar si hay versión nueva (hora de España, con el cambio de verano/invierno).
- **Batería (Waveshare 7C):** con una LiPo de 3,7 V en su conector PH2.0, Inicio enseña la carga arriba a la derecha
  («CARGANDO» si está enchufada) y «BATERIA BAJA» en rojo por debajo del 15 % (también en reposo). Medidor BQ27220:
  el porcentaje es orientativo las primeras cargas, hasta que aprende la capacidad de la batería.
- **Pantalla de prueba:** el primer arranque abre «PRUEBA DE LA PLACA»: toca las 4 esquinas, pulsa los dos botones
  del mando y (opcional) pasa la bola por los sensores. Muestra también si la radio funciona y el último toque.
  Se repite cuando quieras en AJUSTES → PRUEBA DE LA PLACA. En la 7C, cada esquina tocada suena (si se
  oye, el altavoz va) y abajo se ve si hay tarjeta SD.
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

### 7.2 Altavoces y sonidos propios (Waveshare 7C)

La 7C trae de serie el chip de sonido **ES8389** y **dos amplificadores NS4150B** (uno por altavoz), con sus
conectores de altavoz. No hace falta ningún módulo más: los altavoces se enchufan directamente.

**Altavoces de referencia:** 2 × «2030 cavity speaker» tipo B, **8 Ω y 2 W**, con conector **PH 1,25 de 2 pines**.
Vale cualquier altavoz de **4–8 Ω y 2–3 W**. Con uno solo también funciona: la placa manda el mismo sonido a los dos.

```
                    Waveshare ESP32-S3-Touch-LCD-7C (por detrás)
   ┌───────────────────────────────────────────────────────────────────────┐
   │  [SPK L]  ◄── conector PH 1,25 (2 pines, + y −)                       │
   │     ▲        amplificador NS4150B ◄─┐                                 │
   │     │                               ├── chip de sonido ES8389 ◄── ESP32-S3 (I2S)
   │     │        amplificador NS4150B ◄─┘                                 │
   │  [SPK R]  ◄── conector PH 1,25 (2 pines, + y −)                       │
   │                                                                       │
   │  [ micro SD ]  ◄── tarjeta FAT32 (opcional)          [USB-C] ── app   │
   └───────────────────────────────────────────────────────────────────────┘
        │                                   │
   ┌────┴─────┐                        ┌────┴─────┐
   │ Altavoz 1│ 8 Ω · 2 W              │ Altavoz 2│ 8 Ω · 2 W
   │  (2030)  │                        │  (2030)  │
   └──────────┘                        └──────────┘
```

1. **Altavoces:** enchufa cada uno a un conector de altavoz de la placa (serigrafía `SPK`/`SPEAKER` o `L`/`R`; mira la
   wiki de Waveshare si dudas). El conector solo entra en una posición. Si tu altavoz no trae conector, el cable rojo
   es `+`. **No unas los dos altavoces al mismo conector** (bajaría la carga del amplificador).
2. **Tarjeta micro SD (para tus sonidos):** formateada en **FAT32**, en la ranura de la placa **con la placa
   apagada**. La app crea sola la carpeta `/sonidos` (cada sonido ocupa unos 32 KB por segundo). Sin tarjeta, la
   placa toca los sonidos de siempre: la memoria del chip se reserva para imágenes.
3. **Cargar el programa** con el reparto de memoria propio de la 7C (`partitions.csv`, ver «Cómo cargarlo»).
4. **Pasar los sonidos:** conecta la placa a la app por el **cable USB** (Ajustes → Mesa → USB). En Ajustes → Sonidos,
   al subir un sonido sale ya elegida «Tarjeta SD»: la app lo prepara y se lo pasa (unos segundos, con
   barra de progreso). Si un sonido de la placa le falta (placa nueva, tarjeta cambiada), sale «Pasar a la placa».
5. **Qué suena en cada momento:** lo de Ajustes → Sonidos se le manda a la placa al conectarla y con cada cambio, y
   lo recuerda. Así, **jugando sola** (sin la app), la placa toca tus sonidos en el gol, la remontada, la victoria…
   Si un momento no tiene sonido en la placa (o era del aparato), suena el de siempre (unas notas).

**Cómo suena:** todo a 16 kHz y 16 bits (la app ya los prepara así; los antiguos de 22 kHz se convierten al vuelo).
Un sonido nuevo corta al anterior; la bola de partido espera a que acabe el gol. Los amplificadores se apagan
solos en reposo para que no se oiga soplido. Volumen: el de la app (panel rápido), de −30 dB a 0 dB.

**Pines (para quien adapte otra placa):** I2S MCLK GPIO6, BCLK GPIO44, LRCK GPIO16, DOUT GPIO15; chip de sonido en
I2C 0x10 (mismo bus que el táctil, GPIO47/48); amplificadores con la pata EXIO3 del expansor; SD en modo SD_MMC de
1 bit: CLK GPIO12, CMD GPIO11, D0 GPIO13. Todo en `hardware/esp32s3_pantalla7/mfv3_audio.h`.

### Cómo cargarlo
1. IDE de Arduino con el paquete **esp32 de Espressif** y la biblioteca **LovyanGFX** (Gestor de bibliotecas).
2. Abre `hardware/esp32s3_pantalla7/esp32s3_pantalla7.ino`.
3. `board_config.h` ya viene con **`BOARD_WAVESHARE_7C`**. Otras opciones: `BOARD_ELECROW_7` (CrowPanel 7.0"),
   `BOARD_WAVESHARE_7` (ESP32-S3-Touch-LCD-7, sin «C») o `BOARD_CUSTOM`.
4. Herramientas → Placa **ESP32S3 Dev Module**, PSRAM **OPI PSRAM**, Flash **32 MB**, Partition Scheme **«Custom»**
   y USB CDC On Boot **Enabled** (para ver los mensajes por el USB). El reparto lo pone el archivo `partitions.csv`
   de la carpeta del programa (el IDE lo usa solo):

   | Parte | Tamaño | Para qué |
   |---|---|---|
   | `app0` / `app1` | 6 MB + 6 MB | El programa que funciona y el nuevo de la actualización por Wi-Fi |
   | `ffat` | 19,9 MB | Imágenes (app y fotos de jugadores) |
   | `nvs`, `otadata`, `coredump` | 92 KB | Ajustes, qué mitad arranca y registro de fallos |

   Los sonidos van en la tarjeta SD. **Elecrow y Waveshare 7 sin «C»:** borra `partitions.csv` (es para 32 MB) y
   elige **Minimal SPIFFS** (Elecrow) u **8M with spiffs** (Waveshare 7), Flash 4 MB / 8 MB.
5. **Subir.**

### Limitaciones conocidas
- Compila en GitHub (núcleo esp32 3.3 + LovyanGFX 1.2), pero no se ha probado en la placa real: la lógica
  (motor, botones, protocolo, memoria, sonido) se prueba en PC (`npm run test:hardware`) y el dibujo se ve en las
  capturas del PC (abajo), pero falta comprobarlo en una pantalla de verdad.
- Los pines de pantalla y táctil **cambian según el fabricante**: si la pantalla sale en negro o desplazada, revisa
  el perfil en `board_config.h`.
- Los textos de la placa van sin tildes (las fuentes integradas no las incluyen).
- En la 7C no queda un UART libre (el audio usa GPIO43/44): los sensores van a DI0/DI1 y el PC por USB.
- Los altavoces (§7.2), el reloj y la batería solo en la 7C: la Elecrow y la Waveshare 7 sin «C» no los tienen. El
  micrófono, el Bluetooth y el LED «STA» no se usan (el esquema de la 7C no dice a qué pata va ese LED, y la que
  apuntan los ejemplos, GPIO6, es el reloj del sonido). La radio Wi-Fi es para el mando (ESP-NOW): los sonidos se pasan por el **cable USB**.
- El sonido (chip ES8389, registros copiados del ejemplo oficial) está probado en PC con la placa simulada, no en la
  real: si no suena, mira §8.

### Actualizar por Wi-Fi (pantalla y mando)
- **AJUSTES** (botón de Inicio): Wi-Fi (lista de redes y teclado en pantalla; se guarda en la NVS), ACTUALIZAR,
  volumen y PRUEBA DE LA PLACA.
- La placa solo se conecta al Wi-Fi un momento: al encender (para ver si hay versión nueva) y al instalar. El
  resto del tiempo no, para que la radio del mando (ESP-NOW, canal 1) no cambie de canal.
- GitHub compila cada placa con su número de versión (`-DMFV3_BUILD=<nº de ejecución>`) y publica en la web
  `firmware/<placa>/ota.json` (`build`, `size`, `md5`) y `firmware/<placa>/<placa>-ota.bin`. La placa los descarga
  por HTTPS comprobando el certificado. Compilado a mano, `MFV3_BUILD` vale 0 («versión de prueba»).
- **Mando** (`mfv3_ota.h`): la pantalla descarga su programa a su mitad de memoria libre, comprueba el MD5 y se
  lo pasa por radio en trozos de 200 bytes con confirmación. El mando solo lo acepta si se mantienen sus dos
  botones 3 s (queda listo 2 minutos) y lo comprueba antes de arrancarlo. Anuncia su versión con cada saludo.
- **Particiones con dos mitades** (hacen falta para esto): 7C `partitions.csv` (2 × 6 MB), Waveshare 7
  `8M with spiffs`, Elecrow `Minimal SPIFFS`; el mando, la de serie. Al pasar a ellas la primera vez hay que
  instalar por USB borrando la placa: la actualización por Wi-Fi no cambia el reparto de la memoria.
- Guía para el usuario: [`CARGAR_PROGRAMA.md`](CARGAR_PROGRAMA.md).

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
| La placa de 7" no suena | Mira que en Ajustes → Mesa la placa anuncie «sonido»; si no, el chip de sonido no respondió (apaga y enciende). Comprueba el volumen de la app (no en silencio) y que el altavoz esté en el conector de altavoz. |
| «La memoria del chip no está preparada» | En la 7C los sonidos van a la tarjeta SD: elige «Tarjeta SD» al subirlos. |
| «La placa no tiene tarjeta SD» | Tarjeta en FAT32, metida con la placa apagada. |
| En http://192.168.4.1 no se instala como app | Los navegadores solo instalan/cachean en https o localhost; la placa ya la sirve sin Internet. |
