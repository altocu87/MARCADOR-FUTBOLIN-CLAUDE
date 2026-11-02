// MARCADOR FUTBOLÍN V3 · pitido del árbitro al acabar una parte, la prórroga o el partido (petición del dueño,
// 02/11/2026).
// ---------------------------------------------------------------------------------------------------
// Es un WAV suelto de la SD, fuera del catálogo de comentarios, con el código FNV-1a de «arbitro/pitido_final»
// (lo baja la placa sola de la web, ver abajo, o se sube con `python scripts/placa.py pitido <archivo>`). Suena primero y lo demás (voz del descanso, de la
// prórroga, de la victoria…) va detrás, en la cola. Sin el archivo en la SD todo suena como antes.
//
// Cuándo: al acabar la 1ª parte, la 2ª y la prórroga (por tiempo o por goles), y al acabar el partido, salvo
// si lo decide la tanda de penaltis.

#ifndef MFV3_S3_PITIDO_H
#define MFV3_S3_PITIDO_H

#include <stdint.h>
#include <stdlib.h>
#include "mfv3_engine.h"

namespace pitido {

inline uint32_t fnv(const char* s) {
  uint32_t h = 2166136261u;
  for (const char* p = s; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
  return h;
}

inline uint32_t code() { return fnv("arbitro/pitido_final"); }

// ---------------------------------------------------------------- bajarlo de la web (0.27)
// Si a la SD le falta el pitido, la placa lo baja sola de la web del proyecto al buscar actualizaciones (al
// encender y en ACTUALIZACIONES), comprueba tamaño y CRC y loop() lo guarda en la SD. Es el mismo archivo que
// prepara `scripts/placa.py pitido` (web/public/sonidos/pitido.wav: WAV mono 16 kHz, 1,5 s).
static const char* const URL = "https://altocu87.github.io/MARCADOR-FUTBOLIN-CLAUDE/sonidos/pitido.wav";
const uint32_t FILE_SIZE = 48070, FILE_CRC = 0xac7d72f0;
// Descarga: escribe hasta `cap` bytes en `out` y dice cuántos (`n`). La pone el .ino (HTTPClient) o la prueba.
typedef bool (*FetchFn)(const char* url, uint8_t* out, size_t cap, size_t& n);
static FetchFn fetch = nullptr;
static bool want = false;       // lo decide loop() antes de lanzar la tarea de red
static uint8_t tries = 0;       // como mucho 3 intentos por encendido
static uint8_t* got = nullptr;  // descargado y comprobado: lo guarda loop() (y lo libera)

inline uint32_t crc32(const uint8_t* d, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

// En la tarea de red (no toca la pantalla ni la SD).
inline void downloadWork() {
  if (!want || !fetch || got) return;
  want = false;
  tries++;
  uint8_t* b = (uint8_t*)malloc(FILE_SIZE);
  if (!b) return;
  size_t n = 0;
  if (fetch(URL, b, FILE_SIZE, n) && n == FILE_SIZE && crc32(b, n) == FILE_CRC) got = b;
  else free(b);
}

// before/after: fase y periodo antes y después del cambio del motor.
inline bool due(mfv3::Phase beforePhase, mfv3::Phase afterPhase, mfv3::Period afterPeriod) {
  using mfv3::Phase;
  if (afterPhase == beforePhase) return false;
  if (afterPhase == Phase::PeriodEnd) return true;
  if (afterPhase == Phase::Finished) return afterPeriod != mfv3::Period::Shootout;
  return false;
}

}  // namespace pitido

#endif
