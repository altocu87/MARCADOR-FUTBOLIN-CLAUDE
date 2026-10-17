# Manual «Hazlo tú mismo» · Marcador Futbolín V3

> Este documento se genera solo a partir del catálogo de placas (`npm run docs:diy`). La misma información está
> en la app: **Ajustes → Hazlo tú mismo**, con el esquema de conexión dibujado para cada placa.

El marcador funciona de tres formas, y puedes empezar por la más sencilla e ir subiendo:

- **Solo con el móvil**: abres la app y marcas tocando la pantalla. No hay que montar nada.
- **Con pulsadores o sensores**: una placa barata (Arduino o ESP32) junto a la mesa avisa a la app de cada gol.
- **Marcador de mesa**: una pantalla táctil con su propia placa hace de marcador completo, sin móvil.

No hace falta saber programar: el programa de cada placa ya está hecho y **reconoce solo el modelo de placa**
que eliges al instalarlo. Solo hay que conectar los cables en los pines indicados y pulsar «Subir».

## 1. Elige tu nivel

| Nivel | Qué es | Qué necesitas | Coste | Dificultad |
|---|---|---|---|---|
| **0 · Solo móvil** | Sin montar nada. Abres la app en el móvil, la tablet o el PC y marcas tocando la pantalla. | Un móvil, tablet o PC con navegador | 0 € | ●○○ |
| **1 · Pulsadores por cable** | Una placa Arduino o ESP32 con 2 pulsadores arcade junto a la mesa, conectada por USB al PC o a un móvil Android. | Arduino Uno/Nano o ESP32 · 2 pulsadores arcade · Cables y cable USB · Chrome o Edge (PC, Mac o Android) | 15–30 € | ●○○ |
| **2 · Sin cables** | Un ESP32 crea su propia red Wi-Fi y sirve la app: cualquier móvil (también iPhone) se conecta y recibe los goles. También por Bluetooth. | ESP32, ESP32-C3 o ESP32-S3 · 2 pulsadores arcade · Alimentación USB (cargador o batería) | 10–25 € | ●●○ |
| **3 · Marcador de mesa** | Pantalla táctil de 7" con ESP32-S3 que es el marcador completo, sin móvil. Mando inalámbrico de 2 pulsadores y sensores de gol opcionales. | Pantalla ESP32-S3 7" (p. ej. Waveshare 7C) · ESP32-C3 Super Mini + 2 pulsadores (mando) · Sensores de gol (opcional) | 60–90 € | ●●● |

## 2. Cómo se detectan los goles

Hay dos formas de avisar al marcador, y se pueden usar a la vez:

- **Pulsadores**: alguien pulsa el botón de su equipo. **Toque corto = gol · toque largo (0,8 s) = anular el último gol**.
- **Sensores**: detectan la bola solos. Cualquier sensor con salida tipo «contacto» vale: el programa aprende al
  encender cómo está «sin balón» y cuenta gol cuando cambia. **Enciende la placa sin bola delante de los sensores.**

💡 El mejor sitio para un sensor es el **canal interior por el que cae la bola** después del gol: la bola siempre pasa
por ahí, y más despacio que en la boca de la portería.

Pase lo que pase, el marcador ignora un segundo gol durante 3 segundos (evita goles dobles por rebotes).

| Opción | Coste | Dificultad | ¿Recomendada? |
|---|---|---|---|
| Pulsadores arcade | 2–4 € cada uno | ●○○ | ✅ Sí |
| Barrera de infrarrojos (haz cortado) | 3–6 € por portería | ●●○ | ✅ Sí |
| Sensor de infrarrojos por reflexión | 2–8 € por portería | ●●○ | Vale |
| Láser + receptor | 3–5 € por portería | ●●○ | Vale |
| Microinterruptor con palanca | 0,50–2 € por portería | ●○○ | Vale |
| Sensor fotoeléctrico industrial (12–24 V) | 10–25 € por portería | ●●● | Vale |
| Sensor de vibración o piezo | 1–2 € | ●●○ | ❌ No |

### Pulsadores arcade

Un jugador pulsa el botón de su equipo después de cada gol. Toque largo para anular un gol mal marcado.

- **Dónde:** En un lateral de la mesa, en una caja o dentro del propio mueble.
- **Conexión:** 2 cables: uno al pin del pulsador y otro a GND.
- **A favor:** El más fácil y barato; Nunca da falsos goles; No hay que tocar la mesa.
- **En contra:** Hay que acordarse de pulsar.

### Barrera de infrarrojos (haz cortado)

Un emisor y un receptor enfrentados: cuando la bola corta el haz invisible, cuenta gol.

- **Dónde:** En el canal interior por el que cae la bola tras el gol (la bola siempre pasa por ahí y más despacio).
- **Conexión:** Receptor: rojo a VCC, negro a GND, señal (blanco/amarillo) al pin del sensor. Emisor: rojo a VCC y negro a GND.
- **A favor:** Automático y sin contacto; Muy fiable bien alineado; Vale para placas de 5 V y de 3,3 V (salida de colector abierto).
- **En contra:** Hay que alinear emisor y receptor; La luz solar directa puede molestar.

### Sensor de infrarrojos por reflexión

Un único módulo que «ve» la bola cuando pasa por delante (E18-D80NK, FC-51, TCRT5000).

- **Dónde:** En una pared del canal de caída de la bola, apuntando al paso de la bola.
- **Conexión:** VCC, GND y señal al pin del sensor. El E18-D80NK va a 5 V y su salida NPN va directa al pin (también en placas de 3,3 V).
- **A favor:** Un solo lado: más fácil de montar; Barato.
- **En contra:** Ajustar la distancia con su tornillo; Bolas oscuras o muy rápidas pueden escaparse.

### Láser + receptor

Como la barrera de infrarrojos, pero con un punto de luz visible (módulo láser KY-008 + receptor láser).

- **Dónde:** Igual que la barrera: en el canal de caída de la bola.
- **Conexión:** Láser: S a 5 V y − a GND. Receptor: VCC, GND y OUT al pin del sensor.
- **A favor:** Muy fácil de alinear (se ve el punto); Barato.
- **En contra:** No mirar el haz directamente; Necesita una caja que lo proteja de golpes.

### Microinterruptor con palanca

La bola empuja una palanquita al caer y cierra el contacto.

- **Dónde:** En el canal de retorno, donde la bola cae con algo de peso.
- **Conexión:** 2 cables: común (C) a GND y normalmente abierto (NO) al pin del sensor. Sin alimentación.
- **A favor:** Lo más barato; Sin electrónica extra ni alimentación.
- **En contra:** Mecánico: se desgasta; Bolas ligeras o rápidas pueden no activarlo.

### Sensor fotoeléctrico industrial (12–24 V)

Sensores de barrera o reflexión de los que se usan en fábricas, más robustos.

- **Dónde:** En el canal de caída de la bola.
- **Conexión:** Solo a placas con entradas aisladas (Waveshare 7C: DI0/DI1 y su común) o con un optoacoplador. ¡Nunca 12–24 V directos a un pin!
- **A favor:** Muy robustos y fiables; Alcance largo.
- **En contra:** Más caros; Necesitan fuente de 12–24 V.

### Sensor de vibración o piezo

Detecta el golpe de la bola contra la portería.

- **Dónde:** Pegado a la portería.
- **Conexión:** —
- **A favor:** Barato.
- **En contra:** Confunde golpes, rebotes y empujones a la mesa con goles; No recomendado.

## 3. Cómo se conecta cada cosa

### Pulsador arcade
Un pulsador arcade tiene dos patas para el botón (a veces marcadas **COM** y **NO**) y, si lleva luz, otras dos
para el LED. No hacen falta resistencias: la placa usa las suyas internas.

```
  PIN del pulsador ───────── pata NO del pulsador
  GND de la placa  ───────── pata COM del pulsador
```

### Sensor de 3 cables (barrera, reflexión, láser)
```
  VCC (5 V o 3,3 V) ──────── cable rojo / VCC del sensor
  GND ────────────────────── cable negro / GND del sensor
  PIN del sensor ─────────── cable de señal (blanco, amarillo u OUT)
```

### Luz o zumbador (opcional)
```
  LED de 5 V:   PIN ── resistencia 220 Ω ── (+) LED (−) ── GND
  LED de 12 V (con un MOSFET IRLZ44N o un módulo de relé):
      PIN ── resistencia 1 kΩ ── G (puerta del MOSFET)
      12 V ── (+) LED (−) ── D (drenador)
      S (fuente) ── GND        ← une el GND de la placa con el (−) de la fuente de 12 V
  Zumbador activo: PIN ── (+) zumbador (−) ── GND
```

## 4. Placas compatibles

| Placa | Para qué | Tensión | Conexión con la app | Precio | Estado |
|---|---|---|---|---|---|
| [Arduino Uno](#arduino-uno) | Pulsadores y sensores | 5 V | USB | ≈ 8–25 € | 🧪 Probada en simulador |
| [Arduino Nano](#arduino-nano) | Pulsadores y sensores | 5 V | USB | ≈ 4–20 € | 🧪 Probada en simulador |
| [Arduino Mega 2560](#arduino-mega-2560) | Pulsadores y sensores | 5 V | USB | ≈ 15–40 € | 🧪 Probada en simulador |
| [Arduino Leonardo / Micro / Pro Micro](#arduino-leonardo--micro--pro-micro) | Pulsadores y sensores | 5 V | USB | ≈ 6–25 € | 🧪 Probada en simulador |
| [Raspberry Pi Pico (RP2040)](#raspberry-pi-pico-rp2040) | Pulsadores y sensores | 3,3 V | USB | ≈ 5–8 € | 🧪 Probada en simulador |
| [ESP32 DevKit](#esp32-devkit) | Pulsadores y sensores | 3,3 V | USB, Wi-Fi, Bluetooth | ≈ 6–12 € | 🧪 Probada en simulador |
| [ESP32-C3 Super Mini](#esp32-c3-super-mini) | Pulsadores y sensores | 3,3 V | USB, Wi-Fi, Bluetooth | ≈ 3–6 € | 🧪 Probada en simulador |
| [ESP32-S3 DevKit](#esp32-s3-devkit) | Pulsadores y sensores | 3,3 V | USB, Wi-Fi, Bluetooth | ≈ 8–15 € | 🧪 Probada en simulador |
| [ESP32-S2 Mini](#esp32-s2-mini) | Pulsadores y sensores | 3,3 V | USB, Wi-Fi | ≈ 4–8 € | 🧪 Probada en simulador |
| [Mando inalámbrico (ESP32-C3 Super Mini)](#mando-inalámbrico-esp32-c3-super-mini) | Mando inalámbrico | 3,3 V | Radio | ≈ 3–6 € + pulsadores | 🧪 Probada en simulador |
| [Waveshare ESP32-S3-Touch-LCD-7C](#waveshare-esp32-s3-touch-lcd-7c) | Pantalla de mesa | Entradas aisladas (consulta la wiki de Waveshare) | USB, Radio | ≈ 50–70 € | 🧪 Probada en simulador |
| [Elecrow CrowPanel 7.0" ESP32-S3](#elecrow-crowpanel-70-esp32-s3) | Pantalla de mesa | 3,3 V | USB, Radio | ≈ 40–60 € | 🧪 Probada en simulador |
| [Waveshare ESP32-S3-Touch-LCD-7](#waveshare-esp32-s3-touch-lcd-7) | Pantalla de mesa | 3,3 V | USB, Radio | ≈ 40–60 € | 🧪 Probada en simulador |

«Probada en simulador» significa que el programa se ha probado en el ordenador simulando la placa (botones, sensores,
radio y protocolo). Cuando alguien la pruebe en una placa real, pasará a «Probada en placa».

## 5. Pulsadores y sensores

### Arduino Uno

*Uno R3 y compatibles (CH340)* · ATmega328P · 5 V · 🧪 Probada en simulador

- La más fácil para empezar. Solo por cable USB (PC, Mac o Android con Chrome/Edge).

| Qué | Pin de la placa | El otro cable |
|---|---|---|
| **Pulsador BLANCO** — Toque corto = gol · toque largo (0,8 s) = anular | `2` | GND |
| **Pulsador AZUL** — Toque corto = gol · toque largo (0,8 s) = anular | `3` | GND |
| **Pulsador PAUSA** — Opcional · pausa y continuar | `4` | GND |
| **Sensor de gol BLANCO** — Opcional · cable de señal del sensor (alimentación aparte) | `5` | GND (y VCC del sensor) |
| **Sensor de gol AZUL** — Opcional · cable de señal del sensor (alimentación aparte) | `6` | GND (y VCC del sensor) |
| **Luz BLANCO** — Opcional · se enciende con cada gol del Blanco | `9` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Luz AZUL** — Opcional · se enciende con cada gol del Azul | `10` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Zumbador** — Opcional · pita con cada gol confirmado | `8` | Zumbador activo (+) · su (−) a GND |

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: Arduino AVR Boards (viene instalado).
3. Abre el programa hardware/arduino_usb/arduino_usb.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «Arduino Uno». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. Con la app abierta en Chrome o Edge: Ajustes → Conexiones → USB → Conectar.
7. La app reconoce la placa sola y muestra su nombre y lo que sabe hacer.

### Arduino Nano

*Nano, Nano Every y clones* · ATmega328P / ATmega4809 · 5 V · 🧪 Probada en simulador

- Pequeño: cabe dentro de la mesa o en una caja con los pulsadores.

| Qué | Pin de la placa | El otro cable |
|---|---|---|
| **Pulsador BLANCO** — Toque corto = gol · toque largo (0,8 s) = anular | `D2` | GND |
| **Pulsador AZUL** — Toque corto = gol · toque largo (0,8 s) = anular | `D3` | GND |
| **Pulsador PAUSA** — Opcional · pausa y continuar | `D4` | GND |
| **Sensor de gol BLANCO** — Opcional · cable de señal del sensor (alimentación aparte) | `D5` | GND (y VCC del sensor) |
| **Sensor de gol AZUL** — Opcional · cable de señal del sensor (alimentación aparte) | `D6` | GND (y VCC del sensor) |
| **Luz BLANCO** — Opcional · se enciende con cada gol del Blanco | `D9` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Luz AZUL** — Opcional · se enciende con cada gol del Azul | `D10` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Zumbador** — Opcional · pita con cada gol confirmado | `D8` | Zumbador activo (+) · su (−) a GND |

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: Arduino AVR Boards (Nano Every: Arduino megaAVR Boards).
3. Abre el programa hardware/arduino_usb/arduino_usb.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «Arduino Nano (en clones baratos: «ATmega328P (Old Bootloader)»)». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. Con la app abierta en Chrome o Edge: Ajustes → Conexiones → USB → Conectar.
7. La app reconoce la placa sola y muestra su nombre y lo que sabe hacer.

### Arduino Mega 2560

*Mega 2560 y clones* · ATmega2560 · 5 V · 🧪 Probada en simulador

- Funciona igual que el Uno; solo tiene sentido si ya lo tienes.

| Qué | Pin de la placa | El otro cable |
|---|---|---|
| **Pulsador BLANCO** — Toque corto = gol · toque largo (0,8 s) = anular | `2` | GND |
| **Pulsador AZUL** — Toque corto = gol · toque largo (0,8 s) = anular | `3` | GND |
| **Pulsador PAUSA** — Opcional · pausa y continuar | `4` | GND |
| **Sensor de gol BLANCO** — Opcional · cable de señal del sensor (alimentación aparte) | `5` | GND (y VCC del sensor) |
| **Sensor de gol AZUL** — Opcional · cable de señal del sensor (alimentación aparte) | `6` | GND (y VCC del sensor) |
| **Luz BLANCO** — Opcional · se enciende con cada gol del Blanco | `9` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Luz AZUL** — Opcional · se enciende con cada gol del Azul | `10` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Zumbador** — Opcional · pita con cada gol confirmado | `8` | Zumbador activo (+) · su (−) a GND |

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: Arduino AVR Boards (viene instalado).
3. Abre el programa hardware/arduino_usb/arduino_usb.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «Arduino Mega or Mega 2560». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. Con la app abierta en Chrome o Edge: Ajustes → Conexiones → USB → Conectar.
7. La app reconoce la placa sola y muestra su nombre y lo que sabe hacer.

### Arduino Leonardo / Micro / Pro Micro

*ATmega32U4* · ATmega32U4 · 5 V · 🧪 Probada en simulador

- El Pro Micro de 3,3 V/8 MHz también vale, pero alimenta los sensores a 3,3 V.

| Qué | Pin de la placa | El otro cable |
|---|---|---|
| **Pulsador BLANCO** — Toque corto = gol · toque largo (0,8 s) = anular | `2` | GND |
| **Pulsador AZUL** — Toque corto = gol · toque largo (0,8 s) = anular | `3` | GND |
| **Pulsador PAUSA** — Opcional · pausa y continuar | `4` | GND |
| **Sensor de gol BLANCO** — Opcional · cable de señal del sensor (alimentación aparte) | `5` | GND (y VCC del sensor) |
| **Sensor de gol AZUL** — Opcional · cable de señal del sensor (alimentación aparte) | `6` | GND (y VCC del sensor) |
| **Luz BLANCO** — Opcional · se enciende con cada gol del Blanco | `9` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Luz AZUL** — Opcional · se enciende con cada gol del Azul | `10` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Zumbador** — Opcional · pita con cada gol confirmado | `8` | Zumbador activo (+) · su (−) a GND |

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: Arduino AVR Boards (viene instalado).
3. Abre el programa hardware/arduino_usb/arduino_usb.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «Arduino Leonardo (o Arduino Micro)». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. Con la app abierta en Chrome o Edge: Ajustes → Conexiones → USB → Conectar.
7. La app reconoce la placa sola y muestra su nombre y lo que sabe hacer.

### Raspberry Pi Pico (RP2040)

*Pico, Pico W y placas RP2040* · RP2040 · 3,3 V · 🧪 Probada en simulador

- Placa de 3,3 V: los sensores de 5 V con salida «push-pull» necesitan un divisor de tensión.

| Qué | Pin de la placa | El otro cable |
|---|---|---|
| **Pulsador BLANCO** — Toque corto = gol · toque largo (0,8 s) = anular | `GP2` | GND |
| **Pulsador AZUL** — Toque corto = gol · toque largo (0,8 s) = anular | `GP3` | GND |
| **Pulsador PAUSA** — Opcional · pausa y continuar | `GP4` | GND |
| **Sensor de gol BLANCO** — Opcional · cable de señal del sensor (alimentación aparte) | `GP5` | GND (y VCC del sensor) |
| **Sensor de gol AZUL** — Opcional · cable de señal del sensor (alimentación aparte) | `GP6` | GND (y VCC del sensor) |
| **Luz BLANCO** — Opcional · se enciende con cada gol del Blanco | `GP9` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Luz AZUL** — Opcional · se enciende con cada gol del Azul | `GP10` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Zumbador** — Opcional · pita con cada gol confirmado | `GP8` | Zumbador activo (+) · su (−) a GND |

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: Raspberry Pi Pico/RP2040 de Earle Philhower (Gestor de placas).
3. Abre el programa hardware/arduino_usb/arduino_usb.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «Raspberry Pi Pico». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. Con la app abierta en Chrome o Edge: Ajustes → Conexiones → USB → Conectar.
7. La app reconoce la placa sola y muestra su nombre y lo que sabe hacer.

### ESP32 DevKit

*ESP32-WROOM-32, DevKit V1, NodeMCU-32S* · ESP32 · 3,3 V · 🧪 Probada en simulador

- Crea la red «MARCADOR-FUTBOLIN» y sirve la app: válido para cualquier móvil, también iPhone.

| Qué | Pin de la placa | El otro cable |
|---|---|---|
| **Pulsador BLANCO** — Toque corto = gol · toque largo (0,8 s) = anular | `GPIO 4` | GND |
| **Pulsador AZUL** — Toque corto = gol · toque largo (0,8 s) = anular | `GPIO 5` | GND |
| **Pulsador PAUSA** — Opcional · pausa y continuar | `GPIO 18` | GND |
| **Sensor de gol BLANCO** — Opcional · cable de señal del sensor (alimentación aparte) | `GPIO 19` | GND (y VCC del sensor) |
| **Sensor de gol AZUL** — Opcional · cable de señal del sensor (alimentación aparte) | `GPIO 21` | GND (y VCC del sensor) |
| **Luz BLANCO** — Opcional · se enciende con cada gol del Blanco | `GPIO 22` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Luz AZUL** — Opcional · se enciende con cada gol del Azul | `GPIO 23` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Zumbador** — Opcional · pita con cada gol confirmado | `GPIO 25` | Zumbador activo (+) · su (−) a GND |

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: esp32 de Espressif (Gestor de placas) + biblioteca «WebSockets» de Markus Sattler.
3. Abre el programa hardware/esp32_marcador/esp32_marcador.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «ESP32 Dev Module · Partition Scheme «Huge APP»». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. Conéctate a la red Wi-Fi «MARCADOR-FUTBOLIN» (clave futbolin123) y abre http://192.168.4.1, o usa Bluetooth o USB.
7. La app reconoce la placa sola y muestra su nombre y lo que sabe hacer.

### ESP32-C3 Super Mini

*ESP32-C3 DevKitM y Super Mini* · ESP32-C3 · 3,3 V · 🧪 Probada en simulador

- Diminuta y barata. No uses GPIO 8 (LED de la placa) ni GPIO 9 (botón BOOT).

| Qué | Pin de la placa | El otro cable |
|---|---|---|
| **Pulsador BLANCO** — Toque corto = gol · toque largo (0,8 s) = anular | `GPIO 3` | GND |
| **Pulsador AZUL** — Toque corto = gol · toque largo (0,8 s) = anular | `GPIO 4` | GND |
| **Pulsador PAUSA** — Opcional · pausa y continuar | `GPIO 5` | GND |
| **Sensor de gol BLANCO** — Opcional · cable de señal del sensor (alimentación aparte) | `GPIO 6` | GND (y VCC del sensor) |
| **Sensor de gol AZUL** — Opcional · cable de señal del sensor (alimentación aparte) | `GPIO 7` | GND (y VCC del sensor) |
| **Luz BLANCO** — Opcional · se enciende con cada gol del Blanco | `GPIO 0` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Luz AZUL** — Opcional · se enciende con cada gol del Azul | `GPIO 1` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Zumbador** — Opcional · pita con cada gol confirmado | `GPIO 10` | Zumbador activo (+) · su (−) a GND |

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: esp32 de Espressif (Gestor de placas) + biblioteca «WebSockets» de Markus Sattler.
3. Abre el programa hardware/esp32_marcador/esp32_marcador.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «ESP32C3 Dev Module · «Huge APP» · «USB CDC On Boot: Enabled»». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. Conéctate a la red Wi-Fi «MARCADOR-FUTBOLIN» (clave futbolin123) y abre http://192.168.4.1, o usa Bluetooth o USB.
7. La app reconoce la placa sola y muestra su nombre y lo que sabe hacer.

### ESP32-S3 DevKit

*ESP32-S3-DevKitC y similares* · ESP32-S3 · 3,3 V · 🧪 Probada en simulador

- No uses GPIO 19/20 (USB) ni 0, 3, 45 y 46 (arranque).

| Qué | Pin de la placa | El otro cable |
|---|---|---|
| **Pulsador BLANCO** — Toque corto = gol · toque largo (0,8 s) = anular | `GPIO 4` | GND |
| **Pulsador AZUL** — Toque corto = gol · toque largo (0,8 s) = anular | `GPIO 5` | GND |
| **Pulsador PAUSA** — Opcional · pausa y continuar | `GPIO 6` | GND |
| **Sensor de gol BLANCO** — Opcional · cable de señal del sensor (alimentación aparte) | `GPIO 7` | GND (y VCC del sensor) |
| **Sensor de gol AZUL** — Opcional · cable de señal del sensor (alimentación aparte) | `GPIO 8` | GND (y VCC del sensor) |
| **Luz BLANCO** — Opcional · se enciende con cada gol del Blanco | `GPIO 9` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Luz AZUL** — Opcional · se enciende con cada gol del Azul | `GPIO 10` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Zumbador** — Opcional · pita con cada gol confirmado | `GPIO 11` | Zumbador activo (+) · su (−) a GND |

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: esp32 de Espressif (Gestor de placas) + biblioteca «WebSockets» de Markus Sattler.
3. Abre el programa hardware/esp32_marcador/esp32_marcador.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «ESP32S3 Dev Module · «Huge APP» · «USB CDC On Boot: Enabled»». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. Conéctate a la red Wi-Fi «MARCADOR-FUTBOLIN» (clave futbolin123) y abre http://192.168.4.1, o usa Bluetooth o USB.
7. La app reconoce la placa sola y muestra su nombre y lo que sabe hacer.

### ESP32-S2 Mini

*Lolin S2 Mini y similares* · ESP32-S2 · 3,3 V · 🧪 Probada en simulador

- Sin Bluetooth (el chip no lo tiene): conecta por Wi-Fi o USB.

| Qué | Pin de la placa | El otro cable |
|---|---|---|
| **Pulsador BLANCO** — Toque corto = gol · toque largo (0,8 s) = anular | `GPIO 4` | GND |
| **Pulsador AZUL** — Toque corto = gol · toque largo (0,8 s) = anular | `GPIO 5` | GND |
| **Pulsador PAUSA** — Opcional · pausa y continuar | `GPIO 6` | GND |
| **Sensor de gol BLANCO** — Opcional · cable de señal del sensor (alimentación aparte) | `GPIO 7` | GND (y VCC del sensor) |
| **Sensor de gol AZUL** — Opcional · cable de señal del sensor (alimentación aparte) | `GPIO 8` | GND (y VCC del sensor) |
| **Luz BLANCO** — Opcional · se enciende con cada gol del Blanco | `GPIO 9` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Luz AZUL** — Opcional · se enciende con cada gol del Azul | `GPIO 10` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Zumbador** — Opcional · pita con cada gol confirmado | `GPIO 11` | Zumbador activo (+) · su (−) a GND |

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: esp32 de Espressif (Gestor de placas) + biblioteca «WebSockets» de Markus Sattler.
3. Abre el programa hardware/esp32_marcador/esp32_marcador.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «ESP32S2 Dev Module · «Huge APP» · «USB CDC On Boot: Enabled»». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. Conéctate a la red Wi-Fi «MARCADOR-FUTBOLIN» (clave futbolin123) y abre http://192.168.4.1, o usa Bluetooth o USB.
7. La app reconoce la placa sola y muestra su nombre y lo que sabe hacer.

## 6. Mando inalámbrico

### Mando inalámbrico (ESP32-C3 Super Mini)

*Mando de 2 pulsadores por radio* · ESP32-C3 · 3,3 V · 🧪 Probada en simulador

- Envía los toques por radio ESP-NOW a la pantalla de mesa: sin Wi-Fi ni router.
- LED de la placa: 1 destello = gol, 3 = anulación.
- Mismo «GRUPO_MESA» en el mando y en la pantalla.

| Qué | Pin de la placa | El otro cable |
|---|---|---|
| **Pulsador BLANCO** — Toque corto = gol · toque largo (0,8 s) = anular | `GPIO 3` | GND |
| **Pulsador AZUL** — Toque corto = gol · toque largo (0,8 s) = anular | `GPIO 4` | GND |

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: esp32 de Espressif (Gestor de placas).
3. Abre el programa hardware/mando_pulsadores/mando_pulsadores.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «ESP32C3 Dev Module («USB CDC On Boot: Enabled»)». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. En la pantalla de mesa aparecerá «MANDO CONECTADO».

## 7. Pantalla de mesa

### Waveshare ESP32-S3-Touch-LCD-7C

*«ESP32-S3 7inch AI Voice Touch Display», 7C-BOX* · ESP32-S3 · Entradas aisladas (consulta la wiki de Waveshare) · 🧪 Probada en simulador

- Marcador completo en la propia pantalla táctil, sin móvil.
- Pulsadores: con el mando inalámbrico. Sensores: entradas aisladas DI0/DI1 (más su común).
- Las salidas DO0/DO1 se activan 1 s con cada gol (luz o relé).
- Sonidos propios: dos altavoces de 8 Ω y 2 W a sus conectores de altavoz (PH 1,25 de 2 pines), sin amplificador aparte. Los sonidos van a una tarjeta micro SD; la memoria del chip queda para imágenes.

| Qué | Pin de la placa | El otro cable |
|---|---|---|
| **Sensor de gol BLANCO** — Opcional · cable de señal del sensor (alimentación aparte) | `DI0` | Común de las entradas (COM) |
| **Sensor de gol AZUL** — Opcional · cable de señal del sensor (alimentación aparte) | `DI1` | Común de las entradas (COM) |
| **Luz BLANCO** — Opcional · se enciende con cada gol del Blanco | `DO0` | LED + resistencia 220 Ω a GND (12 V: con transistor) |
| **Luz AZUL** — Opcional · se enciende con cada gol del Azul | `DO1` | LED + resistencia 220 Ω a GND (12 V: con transistor) |

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: esp32 de Espressif + biblioteca LovyanGFX.
3. Abre el programa hardware/esp32s3_pantalla7/esp32s3_pantalla7.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «ESP32S3 Dev Module · PSRAM «OPI PSRAM» · Flash 32 MB · Partition Scheme «Custom» (usa el partitions.csv del programa: 2 × 6 MB de programa y 19,9 MB de imágenes) · «USB CDC On Boot: Enabled»». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. La pantalla arranca directamente en el marcador. Toca JUGAR.

### Elecrow CrowPanel 7.0" ESP32-S3

*CrowPanel HMI 800×480* · ESP32-S3 · 3,3 V · 🧪 Probada en simulador

- Pulsadores con el mando inalámbrico, o con un Arduino por el conector UART (RX 18 / TX 17).

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: esp32 de Espressif + biblioteca LovyanGFX.
3. Abre el programa hardware/esp32s3_pantalla7/esp32s3_pantalla7.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «ESP32S3 Dev Module · PSRAM «OPI PSRAM» · Partition Scheme «Minimal SPIFFS (1.9MB APP with OTA)» · «USB CDC On Boot: Enabled» (cambia MFV3_BOARD a BOARD_ELECROW_7 y borra partitions.csv de la carpeta del programa)». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. La pantalla arranca directamente en el marcador. Toca JUGAR.

### Waveshare ESP32-S3-Touch-LCD-7

*Modelo sin «C»* · ESP32-S3 · 3,3 V · 🧪 Probada en simulador

- Pulsadores con el mando inalámbrico, o con un Arduino por el UART (RX 44 / TX 43).

**Instalación**

1. Instala el IDE de Arduino (arduino.cc/en/software) en tu PC.
2. Prepara el IDE: esp32 de Espressif + biblioteca LovyanGFX.
3. Abre el programa hardware/esp32s3_pantalla7/esp32s3_pantalla7.ino de este proyecto (la carpeta entera, con sus archivos .h).
4. Herramientas → Placa: «ESP32S3 Dev Module · PSRAM «OPI PSRAM» · Flash 8 MB · Partition Scheme «8M with spiffs (3MB APP/1.5MB SPIFFS)» · «USB CDC On Boot: Enabled» (cambia MFV3_BOARD a BOARD_WAVESHARE_7 y borra partitions.csv de la carpeta del programa)». Herramientas → Puerto: el de tu placa.
5. Pulsa «Subir» (la flecha). Al terminar, la placa arranca sola.
6. La pantalla arranca directamente en el marcador. Toca JUGAR.

## 8. Seguridad

- ⚠️ Nunca conectes 12 V ni 24 V a un pin de la placa: los LED de 12 V de los pulsadores van con un transistor o MOSFET (o un módulo de relé).
- ⚠️ Placas de 3,3 V (ESP32, Pico): un sensor de 5 V cuya salida da 5 V necesita un divisor de tensión (10 kΩ + 20 kΩ). Los de colector abierto (NPN) van directos.
- ⚠️ Enciende la placa sin bola delante de los sensores: al arrancar aprende cómo están «sin balón».
- ⚠️ Todas las masas (GND) unidas: placa, sensores y fuente de los LED.

## 9. Problemas frecuentes

| Problema | Solución |
|---|---|
| La app no encuentra la placa por USB | Usa Chrome o Edge (no Safari ni Firefox) y cierra el monitor serie del IDE de Arduino. |
| Un gol cuenta solo al encender | Había algo delante del sensor al arrancar: apágala y enciéndela sin bola delante. |
| El sensor no detecta la bola | Revisa la alineación (barrera/láser) o la distancia con el tornillo del módulo (reflexión). |
| Al anular se marca un gol | Mantén el pulsador hasta notar el aviso (0,8 s). Se puede cambiar en `PULSACION_LARGA_MS`. |
| En el iPhone no puedo usar USB ni Bluetooth | Safari no lo permite: usa un ESP32 por Wi-Fi (sirve la app en su propia red). |
| La pantalla de 7" se queda negra | Comprueba que el modelo elegido en `board_config.h` es el de tu placa. |

## 10. Para quien quiera ir más allá

- **Otros pines**: en el programa, antes de `#include "mfv3_boards.h"`, escribe `#define MFV3_PINES_PERSONALIZADOS` y
  tus `#define PIN_…`.
- **Otra placa**: añade su bloque en `hardware/common/mfv3_boards.h` y su ficha en
  `src/inputs/hardware/diy-catalog.json`. Las pruebas (`npm test`) comprueban que los pines coinciden en los dos sitios.
  Después ejecuta `npm run docs:diy` para regenerar este manual.
- **Tu propio programa**: la app acepta cualquier placa que envíe una orden por línea por USB (115200), Wi-Fi
  (WebSocket, puerto 81) o Bluetooth (UART de Nordic): `GB`/`GA` gol, `AB`/`AA` anular, `PAUSA`, y al conectar
  `HELLO <modelo> <versión> caps=goles,anular,…`. Detalles del protocolo en `docs/HARDWARE.md`.
