// Generado por scripts/native-release.mjs. Editar releases.json, no este archivo.
#ifndef MFV3_S3_RELEASE_H
#define MFV3_S3_RELEASE_H
#include <stddef.h>
namespace nativeRelease {
static const char VERSION[] = "0.3";
struct Entry { const char* version; const char* date; const char* notes; };
static const Entry HISTORY[] = {
  {"0.3", "2026-10-22", "Continúa el partido desde los pulsadores\n\n• Al terminar cada parte, una pulsación corta de cualquiera de los dos botones permite continuar.\n\n• También puedes pasar a la prórroga y a los penaltis sin tocar la pantalla.\n\n• La pulsación que permite continuar no suma un gol ni cuenta como el primer penalti."},
  {"0.2", "2026-10-20", "Pantalla sin esperas del USB y acceso recordado\n\n• La pantalla sigue respondiendo aunque el ordenador deje de recoger los mensajes por USB.\n\n• Al desactivar las mediciones dejan de enviarse los avisos de dibujo.\n\n• Puedes elegir recordar la cuenta al apagar y encender, o entrar solo hasta apagar la placa.\n\n• La sesión guardada se conserva con una copia de recuperación al renovarla."},
  {"0.1", "2026-10-19", "Primera versión de la pantalla\n\n• Actualizaciones en su propio apartado, con instrucciones y progreso visible.\n\n• Historial de novedades para saber qué cambia en cada versión.\n\n• Menos trabajo repetido al mostrar el partido y preparar las fotos.\n\n• El historial recuerda tus filtros y el inicio avisa de los cambios por subir.\n\n• Los botones de Ajustes siguen respondiendo al volver de otras pantallas."},
};
static const size_t COUNT = sizeof(HISTORY) / sizeof(HISTORY[0]);
}
#endif
