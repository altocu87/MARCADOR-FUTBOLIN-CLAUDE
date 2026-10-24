// MARCADOR FUTBOLÍN V3 · tipos de la pantalla de 7".
// Van en un archivo aparte porque el IDE de Arduino declara todas las funciones del .ino al principio,
// antes de que el .ino defina sus propios tipos (y «Snapshot» no existiría todavía).

#ifndef MFV3_S3_TYPES_H
#define MFV3_S3_TYPES_H

#include <stdint.h>
#include "mfv3_engine.h"

using mfv3::Period;
using mfv3::Phase;

// Resultado guardado en la memoria interna (NVS).
struct LastResult {
  uint8_t white, blue, penW, penB, winner, reason;  // winner 0 = Blanco, 1 = Azul
  uint32_t at;  // cuándo terminó (reloj::stamp); 0 = la placa no tenía hora
};

// Estado del partido justo antes de un cambio (para avisar a las placas y sonar).
struct Snapshot {
  Phase phase;
  Period period;
  uint8_t w, b;
  uint8_t kicks;
  bool sudden;  // penaltis en muerte súbita
};

// Pantalla de prueba del primer arranque.
struct TestState {
  bool active;
  bool corner[4];
  bool remoteWhite, remoteBlue;
  bool sensorWhite, sensorBlue;
  bool usbSeen;
  int16_t lastX, lastY;
  uint16_t touches;
};

// Pantallas fuera del partido (Inicio y las de AJUSTES, ver s3_ajustes.h).
enum class Screen : uint8_t { Home, Settings, Wifi, Keyboard, Connecting, Update, Install, Players, Ranking, Account, Tournament, Challenges };

// Paquete de actualización por radio recibido (se atiende en loop()).
struct OtaPacket {
  uint8_t len;
  uint8_t data[212];  // mfv3::ota::MAX_PACKET
};
typedef void (*OtaHandler)(const uint8_t* data, int len);

#endif  // MFV3_S3_TYPES_H
