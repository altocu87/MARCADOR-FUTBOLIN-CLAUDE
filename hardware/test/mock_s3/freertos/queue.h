#pragma once
#include "FreeRTOS.h"
inline QueueHandle_t xQueueCreate(size_t n, size_t item) { (void)n; QueueSim* q = new QueueSim(); q->item = item; return q; }
inline BaseType_t xQueueSend(QueueHandle_t q, const void* item, TickType_t) {
  std::vector<uint8_t> v((const uint8_t*)item, (const uint8_t*)item + q->item);
  q->q.push_back(v);
  sim::queueLog.push_back(v);
  return pdTRUE;
}
// En el PC, una cola vacía detiene la tarea (sim::taskStop) en lugar de esperar para siempre.
struct SimTaskDone {};
inline BaseType_t xQueueReceive(QueueHandle_t q, void* item, TickType_t) {
  if (q->q.empty()) throw SimTaskDone();
  memcpy(item, q->q.front().data(), q->item);
  q->q.pop_front();
  return pdTRUE;
}
