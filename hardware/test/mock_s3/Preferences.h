#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include "Arduino.h"
namespace sim { inline std::map<std::string, std::vector<uint8_t>> nvs; }
struct Preferences {
  bool begin(const char*, bool) { return true; }
  template <class T> T get(const char* k, T def) {
    auto it = sim::nvs.find(k);
    if (it == sim::nvs.end()) return def;
    T v; std::memcpy(&v, it->second.data(), sizeof(T)); return v;
  }
  template <class T> void put(const char* k, T v) {
    std::vector<uint8_t> b(sizeof(T)); std::memcpy(b.data(), &v, sizeof(T)); sim::nvs[k] = b;
  }
  uint8_t getUChar(const char* k, uint8_t d) { return get<uint8_t>(k, d); }
  uint32_t getULong(const char* k, uint32_t d) { return get<uint32_t>(k, d); }
  void putUChar(const char* k, uint8_t v) { put(k, v); }
  bool getBool(const char* k, bool d) { return get<uint8_t>(k, d ? 1 : 0) != 0; }
  void putBool(const char* k, bool v) { put<uint8_t>(k, v ? 1 : 0); }
  void putULong(const char* k, uint32_t v) { put(k, v); }
  String getString(const char* k, const char* d) {
    auto it = sim::nvs.find(k);
    if (it == sim::nvs.end()) return String(d);
    return String(std::string(it->second.begin(), it->second.end()));
  }
  size_t putString(const char* k, const char* v) {
    sim::nvs[k] = std::vector<uint8_t>(v, v + strlen(v));
    return strlen(v);
  }
  size_t getBytes(const char* k, void* buf, size_t len) {
    auto it = sim::nvs.find(k);
    if (it == sim::nvs.end()) return 0;
    size_t n = it->second.size() < len ? it->second.size() : len;
    std::memcpy(buf, it->second.data(), n); return n;
  }
  size_t putBytes(const char* k, const void* buf, size_t len) {
    sim::nvs[k] = std::vector<uint8_t>((const uint8_t*)buf, (const uint8_t*)buf + len); return len;
  }
};
