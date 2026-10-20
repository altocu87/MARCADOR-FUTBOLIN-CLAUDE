// Simulación mínima de la API de Arduino para probar los sketches en el PC.
#pragma once
#include <stdint.h>
#include <string>
#include <deque>
#include <cstdio>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

namespace sim {
inline uint32_t now = 0;
inline int pins[64] = {0};
inline int modes[64] = {0};
inline std::string out;
inline std::deque<char> in;
inline int serialRoom = 65536;
inline uint32_t serialTxTimeout = 100;
inline bool serialSaturated = false;
inline unsigned serialBlockingWrites = 0;
}  // namespace sim

inline uint32_t millis() { return sim::now; }
inline uint32_t micros() { return sim::now * 1000u; }
inline void delay(uint32_t ms) { sim::now += ms; }
inline void pinMode(uint8_t p, uint8_t m) {
  sim::modes[p] = m;
  if (m == INPUT_PULLUP) sim::pins[p] = HIGH;
}
inline int digitalRead(uint8_t p) { return sim::pins[p]; }
inline void digitalWrite(uint8_t p, uint8_t v) { sim::pins[p] = v; }

struct SerialSim {
  void begin(unsigned long) {}
  size_t setRxBufferSize(size_t n) { return n; }
  void print(const char* s) { write((const uint8_t*)s, std::char_traits<char>::length(s)); }
  void print(char c) { write((const uint8_t*)&c, 1); }
  void println(const char* s) { print(s); print('\n'); }
  void setTxTimeoutMs(uint32_t ms) { sim::serialTxTimeout = ms; }
  int availableForWrite() { return sim::serialSaturated ? 0 : sim::serialRoom; }
  size_t write(const uint8_t* d, size_t n) {
    if (sim::serialSaturated || n > (size_t)sim::serialRoom) {
      if (sim::serialTxTimeout) { ++sim::serialBlockingWrites; sim::now += 20 * sim::serialTxTimeout; }
      return 0;
    }
    sim::out.append((const char*)d, n); return n;
  }
  void flush() {}
  int available() { return (int)sim::in.size(); }
  int read() {
    if (sim::in.empty()) return -1;
    char c = sim::in.front();
    sim::in.pop_front();
    return c;
  }
  explicit operator bool() const { return true; }
};
inline SerialSim Serial;
