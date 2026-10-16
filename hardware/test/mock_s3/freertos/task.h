#pragma once
#include "FreeRTOS.h"
#include "queue.h"
inline BaseType_t xTaskCreatePinnedToCore(TaskFunction_t f, const char*, uint32_t, void*, int, TaskHandle_t*, int) {
  sim::task = f;
  return pdPASS;
}
namespace sim {
// Ejecuta la tarea hasta que su cola se vacía (lo que sonaría ahora mismo, de una vez).
inline void runTask() {
  if (!task) return;
  try { task(nullptr); } catch (const SimTaskDone&) {}
}
}  // namespace sim
