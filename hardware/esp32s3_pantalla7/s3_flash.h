// MARCADOR FUTBOLÍN V3 · aviso de escritura en la memoria del chip (FFat, NVS).
// ------------------------------------------------------------------------------
// Mientras se escribe en la flash, el ESP32-S3 no puede rellenar los búferes de la pantalla y la imagen puede quedar
// descolocada. Quien escribe llama a s3flash::wrote(); el sketch la realinea en el siguiente retorno (ui_port.h,
// needResync). Sin aviso, se quedaba corrida hasta la próxima vez (auditoría del 23/10/2026).

#ifndef MFV3_S3_FLASH_H
#define MFV3_S3_FLASH_H

#include <string.h>

namespace s3flash {
static void (*hook)() = nullptr;
static inline void wrote() {
  if (hook) hook();
}
// Lo mismo, solo si la ruta es de la memoria del chip (en la tarjeta SD no hace falta).
static inline void wroteTo(const char* path) {
  if (path && !strncmp(path, "/ffat", 5)) wrote();
}
}  // namespace s3flash

#endif  // MFV3_S3_FLASH_H
