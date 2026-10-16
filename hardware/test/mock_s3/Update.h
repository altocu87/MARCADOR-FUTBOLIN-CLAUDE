// Biblioteca Update simulada: guarda lo escrito y «comprueba el MD5» con una suma sencilla (sim::fakeMd5),
// la misma que usan las pruebas para preparar el programa.
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <cstdio>
namespace sim {
inline std::vector<uint8_t> updateData;
inline std::string updateMd5;
inline size_t updateSize = 0;
inline bool updateOpen = false, updateDone = false;
inline std::string fakeMd5(const std::vector<uint8_t>& v) {
  uint32_t h = 2166136261u;
  for (uint8_t b : v) h = (h ^ b) * 16777619u;
  char out[33];
  for (int i = 0; i < 16; i++) std::snprintf(out + i * 2, 3, "%02x", (uint8_t)(h >> (8 * (i % 4))) ^ (uint8_t)i);
  return std::string(out, 32);
}
}  // namespace sim
struct UpdateClass {
  bool begin(size_t size) {
    sim::updateSize = size;
    sim::updateData.clear();
    sim::updateOpen = true;
    sim::updateDone = false;
    return true;
  }
  bool setMD5(const char* m) { sim::updateMd5 = m; return true; }
  size_t write(uint8_t* d, size_t n) {
    if (!sim::updateOpen) return 0;
    sim::updateData.insert(sim::updateData.end(), d, d + n);
    return n;
  }
  bool end(bool = false) {
    sim::updateOpen = false;
    sim::updateDone = sim::updateData.size() == sim::updateSize && sim::fakeMd5(sim::updateData) == sim::updateMd5;
    return sim::updateDone;
  }
  void abort() { sim::updateOpen = false; sim::updateData.clear(); }
};
inline UpdateClass Update;
