// Tablas persistentes de la interfaz, nunca DMA ni datos leídos desde interrupciones.
#ifndef MFV3_S3_MEMORY_H
#define MFV3_S3_MEMORY_H
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

// Hook local para simular fallos de memoria en las pruebas del PC.
#ifndef MFV3_REALLOC
#define MFV3_REALLOC realloc
#endif
namespace s3mem {
// Conserva el bloque y su capacidad si no se puede ampliar. Nunca reduce ni reserva de uno en uno.
template <typename T> static bool reserve(T*& data, int& capacity, int needed, int initial = 16) {
  if (needed < 0 || capacity < 0 || initial < 1) return false;
  if (needed <= capacity) return true;
  int next = capacity ? capacity : initial;
  while (next < needed) {
    if (next > INT_MAX / 2) { next = needed; break; }
    next *= 2;
  }
  if ((size_t)next > SIZE_MAX / sizeof(T)) return false;
  T* grown = (T*)MFV3_REALLOC(data, (size_t)next * sizeof(T));
  if (!grown) return false;
  data = grown;
  capacity = next;
  return true;
}
}

#if defined(ESP_PLATFORM) && defined(MFV3_FASE3)
#include <sdkconfig.h>
#include <esp_attr.h>
#if !defined(CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY) || !CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
#error "Fase 3 requiere CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y"
#endif
#define MFV3_UI_DATA EXT_RAM_BSS_ATTR
#else
// Arduino con SDK precompilado y pruebas PC: conserva el almacenamiento original.
#define MFV3_UI_DATA
#endif
#endif
