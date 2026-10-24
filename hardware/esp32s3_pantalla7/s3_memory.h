// Tablas persistentes de la interfaz, nunca DMA ni datos leídos desde interrupciones.
#ifndef MFV3_S3_MEMORY_H
#define MFV3_S3_MEMORY_H
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
