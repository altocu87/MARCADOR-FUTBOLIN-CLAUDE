// FreeRTOS simulado: las tareas no arrancan solas; la cola guarda lo que se envía (sim::queueItems) y
// sim::runTask() la vacía llamando a la tarea a mano, para probar en el PC lo que sonaría.
#pragma once
#include <stdint.h>
#include <cstring>
#include <deque>
#include <vector>
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xFFFFFFFFu
typedef int BaseType_t;
typedef uint32_t TickType_t;
typedef void (*TaskFunction_t)(void*);
typedef void* TaskHandle_t;
struct QueueSim { size_t item; std::deque<std::vector<uint8_t>> q; };
typedef QueueSim* QueueHandle_t;
namespace sim {
inline TaskFunction_t task = nullptr;
inline std::vector<std::vector<uint8_t>> queueLog;  // todo lo que se ha enviado a una cola
inline bool taskStop = false;
}  // namespace sim
