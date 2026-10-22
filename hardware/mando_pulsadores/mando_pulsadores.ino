// MARCADOR FUTBOLÍN V3 · mando inalámbrico con 2 pulsadores arcade
// ------------------------------------------------------------------
// Una mini placa ESP32 (recomendada: ESP32-C3 Super Mini) con dos pulsadores arcade:
//   - Toque corto  → GOL del equipo de ese pulsador.
//   - Toque largo (0,8 s) → ANULAR el último gol de ese equipo (−1). En penaltis deshace el último lanzamiento.
// Envía las órdenes por radio (ESP-NOW, 2,4 GHz) directamente a la pantalla ESP32-S3: sin Wi-Fi ni router.
// Cada orden se repite 3 veces; la pantalla descarta las repeticiones.
//
// Cableado (cada pulsador arcade tiene 2 terminales; no hacen falta resistencias):
//   Pulsador BLANCO: un terminal → GPIO 3,  el otro → GND
//   Pulsador AZUL:   un terminal → GPIO 4,  el otro → GND
//   Alimentación: USB-C (cargador o batería externa), o batería LiPo de 3,7 V con un módulo cargador.
// LED de la placa: 1 destello = gol enviado, 3 destellos = anulación enviada.
//
// Actualización sin cables: la pantalla de 7" se descarga el programa nuevo del mando y se lo pasa por radio.
// Para aceptarlo hay que mantener pulsados LOS DOS botones 3 segundos (el LED parpadea rápido 2 minutos).
// El programa nuevo se comprueba antes de arrancar; si algo falla, el mando sigue con el que tenía.
//
// Placa en el IDE de Arduino: «ESP32C3 Dev Module» (con «USB CDC On Boot: Enabled»), o la que uses.

#include <Arduino.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_arduino_version.h>
#include "mfv3_core.h"
#include "mfv3_ota.h"
#include "mfv3_radio.h"

// Número de compilación: lo pone GitHub al compilar (-DMFV3_BUILD=…); compilado a mano vale 0.
#ifndef MFV3_BUILD
#define MFV3_BUILD 0
#endif

// Waveshare ESP32-C3-Zero: compilar con -DMFV3_MANDO_RGB_PIN=10.
// Sin esa opción conserva el LED azul GPIO8 de la Super Mini. Los pulsadores no cambian.
#ifndef MFV3_MANDO_RGB_PIN
#define MFV3_MANDO_RGB_PIN -1
#endif
// Lo dice en cada saludo: la pantalla descarga el programa de este modelo («mando-c3zero» o «mando-c3»).
#define MFV3_MANDO_MODEL (MFV3_MANDO_RGB_PIN >= 0 ? mfv3::ota::MODEL_C3_ZERO : mfv3::ota::MODEL_SUPER_MINI)

// ---- Ajustes
const uint8_t GRUPO_MESA = 1;          // MISMO número que en la pantalla (cámbialo si hay dos mesas cerca)
const int PIN_BTN_BLANCO = 3;
const int PIN_BTN_AZUL = 4;
const int PIN_LED = 8;                 // LED azul de la ESP32-C3 Super Mini (-1 si no hay)
const bool LED_ACTIVO_BAJO = true;     // la Super Mini enciende su LED con nivel bajo
const uint16_t PULSACION_LARGA_MS = 800;
const uint32_t SALUDO_CADA_MS = 20000; // aviso periódico para que la pantalla muestre «mando conectado»

using mfv3::Press;
using mfv3::RadioCmd;

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static mfv3::PressClassifier btnBlanco(PULSACION_LARGA_MS), btnAzul(PULSACION_LARGA_MS);
static uint16_t seq = 0;
static uint32_t lastHello = 0;
static uint32_t bothSince = 0;   // desde cuándo están pulsados los dos botones (0 = no)
static uint32_t restartAt = 0;   // programa nuevo recibido: reiniciar en este momento

// LED sin bloquear: n destellos de 80 ms.
static uint8_t ledBlinks = 0;
static uint32_t ledNext = 0;
static bool ledOn = false;

static void ledWrite(bool on) {
#if MFV3_MANDO_RGB_PIN >= 0
  // WS2812 integrado: blanco tenue, sin bibliotecas adicionales ni tocar GPIO8.
  const uint8_t brightness = on ? 8 : 0;
  rgbLedWrite(MFV3_MANDO_RGB_PIN, brightness, brightness, brightness);
#else
  if (PIN_LED < 0) return;
  digitalWrite(PIN_LED, (on != LED_ACTIVO_BAJO) ? HIGH : LOW);
#endif
}

static void blink(uint8_t n) {
  ledBlinks = n;
  ledNext = 0;
}

static void ledTick(uint32_t now) {
  if (ledBlinks == 0 || now < ledNext) return;
  ledOn = !ledOn;
  ledWrite(ledOn);
  ledNext = now + 80;
  if (!ledOn) ledBlinks--;
}

// ---- Actualización por radio (ver mfv3_ota.h)
// Escritura con la biblioteca Update del ESP32: el programa nuevo va a la otra mitad de la memoria y solo se
// arranca si su MD5 cuadra.
struct UpdateFlash : mfv3::ota::Flash {
  bool begin(uint32_t size, const uint8_t md5[16]) override {
    if (!Update.begin(size)) return false;
    char hex[33];
    for (int i = 0; i < 16; i++) snprintf(hex + i * 2, 3, "%02x", md5[i]);
    Update.setMD5(hex);
    return true;
  }
  bool write(const uint8_t* d, uint8_t n) override { return Update.write((uint8_t*)d, n) == n; }
  bool end() override { return Update.end(); }
  void abort() override { Update.abort(); }
};
static UpdateFlash flash;
static mfv3::ota::Receiver otaRx(GRUPO_MESA, 0x1F0000);  // como mucho la mitad de un ESP32-C3 de 4 MB

// Los paquetes llegan en otra tarea del sistema: se copian a una cola y loop() los atiende.
struct RxPacket {
  uint8_t len;
  uint8_t data[mfv3::ota::MAX_PACKET];
};
static RxPacket rxQueue[6];
static volatile uint8_t rxHead = 0, rxTail = 0;

static void rxPush(const uint8_t* data, int len) {
  if (len < mfv3::ota::HEADER || len > mfv3::ota::MAX_PACKET || memcmp(data, "MFVO", 4) != 0) return;
  uint8_t next = (uint8_t)((rxHead + 1) % 6);
  if (next == rxTail) return;  // llena: la pantalla repetirá
  rxQueue[rxHead].len = (uint8_t)len;
  memcpy(rxQueue[rxHead].data, data, len);
  rxHead = next;
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void onRadio(const esp_now_recv_info_t*, const uint8_t* data, int len) { rxPush(data, len); }
#else
static void onRadio(const uint8_t*, const uint8_t* data, int len) { rxPush(data, len); }
#endif

static void sendInfo() {
  uint8_t pk[mfv3::ota::MAX_PACKET];
  uint8_t n = mfv3::ota::encodeInfo(pk, GRUPO_MESA, MFV3_BUILD, otaRx.armed(millis()), MFV3_MANDO_MODEL);
  esp_now_send(BROADCAST, pk, n);
}

static void otaPoll(uint32_t now) {
  uint8_t reply[mfv3::ota::MAX_PACKET];
  while (rxTail != rxHead) {
    const RxPacket& p = rxQueue[rxTail];
    uint8_t n = otaRx.handle(p.data, p.len, now, flash, reply);
    rxTail = (uint8_t)((rxTail + 1) % 6);
    if (n) esp_now_send(BROADCAST, reply, n);
    if (otaRx.state() == mfv3::ota::Receiver::State::Receiving) ledWrite((otaRx.received() / 4096) % 2);
  }
  otaRx.tick(now, flash);
  if (otaRx.state() == mfv3::ota::Receiver::State::Finished && !restartAt) {
    Serial.println("Programa nuevo recibido: reiniciando");
    restartAt = now + 1500;  // da tiempo a repetir la confirmación si se perdió
  }
  if (restartAt && now >= restartAt) ESP.restart();
}

static void sendCmd(RadioCmd c) {
  uint8_t pk[mfv3::RADIO_PACKET_SIZE];
  mfv3::radioEncode(pk, GRUPO_MESA, ++seq, c);
  for (uint8_t i = 0; i < mfv3::RADIO_REPEATS; i++) {
    esp_now_send(BROADCAST, pk, sizeof(pk));
    if (i + 1 < mfv3::RADIO_REPEATS) delay(i == 0 ? 8 : 20);
  }
  Serial.print("Enviado: ");
  Serial.println(c == RadioCmd::Hello ? "HELLO" : mfv3::radioLine(c));
  if (c == RadioCmd::Hello) sendInfo();  // con el saludo, la versión (para saber si hay que actualizarlo)
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN_BLANCO, INPUT_PULLUP);
  pinMode(PIN_BTN_AZUL, INPUT_PULLUP);
#if MFV3_MANDO_RGB_PIN < 0
  if (PIN_LED >= 0) pinMode(PIN_LED, OUTPUT);
#endif
  ledWrite(false);
  char identity[96];
  snprintf(identity, sizeof(identity), "MANDO ESP32-C3 build %lu RGB pin %d", (unsigned long)MFV3_BUILD, MFV3_MANDO_RGB_PIN);
  Serial.println(identity);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(mfv3::RADIO_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: no se pudo iniciar ESP-NOW");
    blink(10);
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = mfv3::RADIO_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
  esp_now_register_recv_cb(onRadio);

  seq = (uint16_t)esp_random();  // tras reiniciar, nunca repite la secuencia anterior
  sendCmd(RadioCmd::Hello);
  lastHello = millis();
  blink(2);
}

void loop() {
  uint32_t now = millis();
  // Pulsadores a GND con resistencia interna: pulsado = LOW.
  Press w = btnBlanco.update(digitalRead(PIN_BTN_BLANCO) == LOW, now);
  Press b = btnAzul.update(digitalRead(PIN_BTN_AZUL) == LOW, now);
  // Los dos a la vez no anulan nada: es el gesto para aceptar una actualización (3 s).
  bool both = btnBlanco.held() && btnAzul.held();
  if (w == Press::Short) { sendCmd(RadioCmd::GoalWhite); blink(1); }
  if (w == Press::Long && !both) { sendCmd(RadioCmd::AnnulWhite); blink(3); }
  if (b == Press::Short) { sendCmd(RadioCmd::GoalBlue); blink(1); }
  if (b == Press::Long && !both) { sendCmd(RadioCmd::AnnulBlue); blink(3); }
  if (both) {
    if (!bothSince) bothSince = now;
    else if (now - bothSince >= mfv3::ota::ARM_HOLD_MS && !otaRx.armed(now)) {
      otaRx.arm(now);
      Serial.println("Listo para recibir una actualizacion (2 min)");
      sendInfo();
    }
  } else {
    bothSince = 0;
  }
  otaPoll(now);
  // Armado y esperando: parpadeo rápido continuo.
  if (otaRx.armed(now) && otaRx.state() == mfv3::ota::Receiver::State::Idle && ledBlinks == 0) blink(1);

  if (now - lastHello >= SALUDO_CADA_MS && !btnBlanco.held() && !btnAzul.held()) {
    lastHello = now;
    sendCmd(RadioCmd::Hello);
  }
  ledTick(now);
  delay(2);
}
