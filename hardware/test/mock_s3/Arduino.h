// Simulación de Arduino-ESP32 (S3) para probar el sketch de la pantalla de 7" en el PC.
#pragma once
#include "../mock/Arduino.h"
#include <cctype>
#include <cstring>
#define SERIAL_8N1 0x800001c
inline uint32_t esp_random() { return 0xBEEF; }
namespace sim { inline int restarts = 0; }
struct EspSim { void restart() { sim::restarts++; } };
inline EspSim ESP;
// String de Arduino (solo lo que usan los sketches de la pantalla).
struct String {
  std::string s;
  String() {}
  String(const char* c) : s(c ? c : "") {}
  String(const std::string& x) : s(x) {}
  const char* c_str() const { return s.c_str(); }
  size_t length() const { return s.size(); }
  void toLowerCase() { for (auto& c : s) c = (char)tolower((unsigned char)c); }
  bool startsWith(const char* p) const { return s.rfind(p, 0) == 0; }
  bool endsWith(const char* p) const {
    size_t n = strlen(p);
    return s.size() >= n && s.compare(s.size() - n, n, p) == 0;
  }
  bool operator==(const char* o) const { return s == o; }
  String operator+(const String& o) const { return String(s + o.s); }
  String operator+(const char* o) const { return String(s + o); }
};
namespace sim {
inline std::deque<char> auxIn;
inline std::string auxOut;
}  // namespace sim
struct HardwareSerial {
  explicit HardwareSerial(int) {}
  void begin(unsigned long, uint32_t, int, int) {}
  void print(const char* s) { sim::auxOut += s; }
  void print(char c) { sim::auxOut += c; }
  int available() { return (int)sim::auxIn.size(); }
  int read() {
    if (sim::auxIn.empty()) return -1;
    char c = sim::auxIn.front();
    sim::auxIn.pop_front();
    return c;
  }
};
