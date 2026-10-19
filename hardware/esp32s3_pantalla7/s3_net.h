// MARCADOR FUTBOLÍN V3 · Wi-Fi de casa y actualizaciones de la pantalla de 7" (y de su mando).
// ----------------------------------------------------------------------------------------------
// - Wi-Fi: buscar redes y conectarse (la red y la clave se escriben en la pantalla y se guardan en la NVS).
//   Mientras no hay que descargar nada la placa NO está conectada: así la radio de los goles (ESP-NOW, canal
//   fijo) no se mueve de canal. Se conecta un momento al encender (para ver si hay versión nueva) y al instalar.
// - Versiones publicadas: GitHub compila y deja en la web del proyecto, para cada placa,
//   firmware/<placa>/ota.json ({"build", "size", "md5"}) y firmware/<placa>/<placa>-ota.bin. Se descargan por
//   HTTPS comprobando el certificado (certificados raíz que trae el ESP32).
// - Programa del mando: se descarga a la mitad libre de la memoria de la pantalla (la que usará su propia
//   actualización), se comprueba su MD5 y desde ahí se le pasa por radio (mfv3_ota.h).
// - Programa de la pantalla: biblioteca HTTPUpdate (comprueba el MD5); arranca con él al reiniciar.

#ifndef MFV3_S3_NET_H
#define MFV3_S3_NET_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <MD5Builder.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_wifi.h>
#include "mfv3_ota.h"
#include "mfv3_radio.h"

namespace s3net {

inline const char* BASE_URL = "https://altocu87.github.io/MARCADOR-FUTBOLIN-CLAUDE/firmware/";

struct Network {
  char ssid[33];
  int8_t rssi;
  bool open;
};

// La radio de los goles vuelve a su canal (al desconectar o tras buscar redes).
inline void restoreRadio() { esp_wifi_set_channel(mfv3::RADIO_CHANNEL, WIFI_SECOND_CHAN_NONE); }

// Redes a la vista, de más a menos señal y sin repetir nombres (tarda 2–4 s).
inline uint8_t scanResults(Network* out, uint8_t max, int n) {
  uint8_t k = 0;
  for (int i = 0; i < n && k < max; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;  // redes ocultas
    bool dup = false;
    for (uint8_t j = 0; j < k; j++)
      if (!strcmp(out[j].ssid, s.c_str())) dup = true;
    if (dup) continue;
    strncpy(out[k].ssid, s.c_str(), 32);
    out[k].ssid[32] = 0;
    out[k].rssi = (int8_t)WiFi.RSSI(i);
    out[k].open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    k++;
  }
  WiFi.scanDelete();
  restoreRadio();
  return k;
}

inline bool scanning = false;
inline int scanResult = WIFI_SCAN_RUNNING;
inline void startScan() {
  if (scanning) return;
  scanning = true;
  scanResult = WiFi.scanNetworks(true);
}
inline bool pollScan(Network* out, uint8_t max, uint8_t& count) {
  if (!scanning) return false;
  int n = scanResult == WIFI_SCAN_RUNNING ? WiFi.scanComplete() : scanResult;
  if (n == WIFI_SCAN_RUNNING) return false;
  count = scanResults(out, max, n);
  scanning = false;
  return true;
}

// ---------------------------------------------------------------- conexión (sin bloquear)
enum class Link : uint8_t { Off, Connecting, Up, Failed };
inline Link link = Link::Off;
inline uint32_t connectAt = 0;
const uint32_t CONNECT_TIMEOUT_MS = 15000;

inline void connect(const char* ssid, const char* pass) {
  WiFi.persistent(false);  // la red la guarda el marcador, no el sistema
  WiFi.begin(ssid, pass);
  connectAt = millis();
  link = Link::Connecting;
}

inline void stop() {
  if (link != Link::Off) WiFi.disconnect();
  link = Link::Off;
  restoreRadio();
}

inline Link poll() {
  if (link != Link::Connecting) return link;
  wl_status_t st = WiFi.status();
  uint32_t t = millis() - connectAt;
  if (st == WL_CONNECTED) link = Link::Up;
  else if (st == WL_CONNECT_FAILED || (st == WL_NO_SSID_AVAIL && t > 8000) || t > CONNECT_TIMEOUT_MS) {
    WiFi.disconnect();
    restoreRadio();
    link = Link::Failed;
  }
  return link;
}

// Conecta y espera (para instalar). `idle` se llama mientras tanto (la pantalla sigue viva).
inline bool connectWait(const char* ssid, const char* pass, void (*idle)()) {
  connect(ssid, pass);
  while (poll() == Link::Connecting) {
    if (idle) idle();
    delay(50);
  }
  return link == Link::Up;
}

// ---------------------------------------------------------------- versiones publicadas
inline String url(const char* board, const char* file) { return String(BASE_URL) + board + "/" + file; }

inline bool fetchPublished(const char* board, mfv3::ota::Published& out) {
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url(board, "ota.json"))) return false;
  int code = http.GET();
  bool ok = code == 200 && mfv3::ota::parsePublished(http.getString().c_str(), out);
  http.end();
  return ok;
}

typedef void (*Progress)(uint32_t done, uint32_t total);

// ---------------------------------------------------------------- programa del mando → mitad libre de la memoria
inline const esp_partition_t* slot() { return esp_ota_get_next_update_partition(nullptr); }

inline bool downloadToSlot(const char* board, const mfv3::ota::Published& pub, Progress progress) {
  const esp_partition_t* p = slot();
  if (!p || pub.size > p->size) return false;
  if (esp_partition_erase_range(p, 0, (pub.size + 4095) & ~4095u) != ESP_OK) return false;
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();
  HTTPClient http;
  http.setTimeout(10000);
  String name = String(board) + "-ota.bin";
  if (!http.begin(client, url(board, name.c_str()))) return false;
  if (http.GET() != 200) {
    http.end();
    return false;
  }
  NetworkClient* s = http.getStreamPtr();
  MD5Builder md5;
  md5.begin();
  static uint8_t buf[1024];
  uint32_t got = 0, lastData = millis();
  bool ok = true;
  while (got < pub.size && ok) {
    size_t av = s->available();
    if (!av) {
      if (millis() - lastData > 10000) ok = false;  // la descarga se quedó parada
      delay(2);
      continue;
    }
    size_t want = pub.size - got;
    if (want > sizeof(buf)) want = sizeof(buf);
    if (av < want) want = av;
    int n = s->readBytes(buf, want);
    if (n <= 0) continue;
    if (esp_partition_write(p, got, buf, n) != ESP_OK) ok = false;
    md5.add(buf, n);
    got += n;
    lastData = millis();
    if (progress) progress(got, pub.size);
  }
  http.end();
  md5.calculate();
  return ok && got == pub.size && md5.toString() == pub.md5;
}

// Lee el programa del mando desde la mitad libre para pasárselo por radio.
struct SlotSource : mfv3::ota::Source {
  bool read(uint32_t off, uint8_t* buf, uint8_t len) override {
    const esp_partition_t* p = slot();
    return p && esp_partition_read(p, off, buf, len) == ESP_OK;
  }
};

// ---------------------------------------------------------------- programa de la pantalla
inline Progress selfProgress = nullptr;

inline bool selfUpdate(const char* board, const mfv3::ota::Published& pub, Progress progress) {
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();
  selfProgress = progress;
  httpUpdate.rebootOnUpdate(false);  // reinicia el sketch, después de enseñar «Listo»
  httpUpdate.setMD5sum(pub.md5);
  httpUpdate.onProgress([](int done, int total) {
    if (selfProgress) selfProgress((uint32_t)done, (uint32_t)total);
  });
  String name = String(board) + "-ota.bin";
  return httpUpdate.update(client, url(board, name.c_str())) == HTTP_UPDATE_OK;
}

}  // namespace s3net

#endif  // MFV3_S3_NET_H
