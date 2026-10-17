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
}  // namespace sim

inline uint32_t millis() { return sim::now; }
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
  void print(const char* s) { sim::out += s; }
  void print(char c) { sim::out += c; }
  void println(const char* s) { sim::out += s; sim::out += '\n'; }
  size_t write(const uint8_t* d, size_t n) { sim::out.append((const char*)d, n); return n; }
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
