// Generado por scripts/native-release.mjs. Editar releases.json, no este archivo.
#ifndef MFV3_S3_RELEASE_H
#define MFV3_S3_RELEASE_H
#include <stddef.h>
namespace nativeRelease {
static const char VERSION[] = "0.1";
struct Entry { const char* version; const char* date; const char* notes; };
static const Entry HISTORY[] = {
  {"0.1", "2026-10-19", "Primera versión de la pantalla\n\n• Actualizaciones en su propio apartado, con instrucciones y progreso visible.\n\n• Historial de novedades para saber qué cambia en cada versión.\n\n• Menos trabajo repetido al mostrar el partido y preparar las fotos.\n\n• El historial recuerda tus filtros y el inicio avisa de los cambios por subir.\n\n• Los botones de Ajustes siguen respondiendo al volver de otras pantallas."},
};
static const size_t COUNT = sizeof(HISTORY) / sizeof(HISTORY[0]);
}
#endif
