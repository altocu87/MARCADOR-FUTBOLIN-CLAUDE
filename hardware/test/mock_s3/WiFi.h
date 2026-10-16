// Wi-Fi simulado: redes a la vista (sim::networks) y conexión a la red de casa (sim::homePass).
#pragma once
#include <string>
#include <vector>
#include "Arduino.h"
#define WIFI_STA 1
enum wl_status_t { WL_IDLE_STATUS = 0, WL_NO_SSID_AVAIL = 1, WL_CONNECTED = 3, WL_CONNECT_FAILED = 4, WL_DISCONNECTED = 6 };
enum wifi_auth_mode_t { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WPA2_PSK = 3 };
namespace sim {
struct Net {
  std::string ssid, pass;
  int rssi;
};
inline std::vector<Net> networks;
inline std::string joinedSsid, joinedPass;  // última red a la que se intentó conectar
inline bool wifiUp = false;
inline int wifiBegins = 0, scans = 0;
inline uint32_t connectDelayMs = 300;  // lo que tarda en conectar
inline uint32_t beganAt = 0;
}  // namespace sim
struct WiFiSim {
  void mode(int) {}
  void persistent(bool) {}
  bool disconnect(bool = false, bool = false, unsigned long = 100) { sim::wifiUp = false; sim::joinedSsid.clear(); return true; }
  wl_status_t begin(const char* ssid, const char* pass = nullptr) {
    sim::wifiBegins++;
    sim::joinedSsid = ssid;
    sim::joinedPass = pass ? pass : "";
    sim::beganAt = millis();
    return WL_DISCONNECTED;
  }
  wl_status_t status() {
    if (sim::joinedSsid.empty()) return WL_DISCONNECTED;
    for (auto& n : sim::networks)
      if (n.ssid == sim::joinedSsid) {
        if (n.pass != sim::joinedPass) return millis() - sim::beganAt > 2000 ? WL_CONNECT_FAILED : WL_DISCONNECTED;
        sim::wifiUp = millis() - sim::beganAt >= sim::connectDelayMs;
        return sim::wifiUp ? WL_CONNECTED : WL_DISCONNECTED;
      }
    return WL_NO_SSID_AVAIL;
  }
  int16_t scanNetworks() { sim::scans++; return (int16_t)sim::networks.size(); }
  String SSID(uint8_t i) { return String(sim::networks[i].ssid); }
  int32_t RSSI(uint8_t i) { return sim::networks[i].rssi; }
  wifi_auth_mode_t encryptionType(uint8_t i) { return sim::networks[i].pass.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK; }
  void scanDelete() {}
};
inline WiFiSim WiFi;
