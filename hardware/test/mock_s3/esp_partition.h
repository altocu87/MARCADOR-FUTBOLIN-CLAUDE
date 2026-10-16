// La otra mitad de la memoria (partición OTA libre), simulada en memoria.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>
#ifndef ESP_OK
#define ESP_OK 0
typedef int esp_err_t;
#endif
struct esp_partition_t { uint32_t size; };
namespace sim {
inline esp_partition_t otaSlot = {3 * 1024 * 1024};
inline std::vector<uint8_t> otaSlotData;
inline bool hasOtaSlot = true;
}  // namespace sim
inline esp_err_t esp_partition_erase_range(const esp_partition_t* p, size_t off, size_t n) {
  if (off % 4096 || n % 4096 || off + n > p->size) return -1;
  if (sim::otaSlotData.size() < p->size) sim::otaSlotData.assign(p->size, 0xFF);
  for (size_t i = off; i < off + n; i++) sim::otaSlotData[i] = 0xFF;
  return ESP_OK;
}
inline esp_err_t esp_partition_write(const esp_partition_t* p, size_t off, const void* src, size_t n) {
  if (off + n > p->size || sim::otaSlotData.size() < p->size) return -1;
  const uint8_t* s = (const uint8_t*)src;
  for (size_t i = 0; i < n; i++) sim::otaSlotData[off + i] &= s[i];  // como la flash: solo pasa de 1 a 0
  return ESP_OK;
}
inline esp_err_t esp_partition_read(const esp_partition_t* p, size_t off, void* dst, size_t n) {
  if (off + n > p->size || sim::otaSlotData.size() < p->size) return -1;
  for (size_t i = 0; i < n; i++) ((uint8_t*)dst)[i] = sim::otaSlotData[off + i];
  return ESP_OK;
}
