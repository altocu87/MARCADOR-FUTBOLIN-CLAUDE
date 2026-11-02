// MARCADOR FUTBOLÍN V3 · pitido del árbitro al acabar una parte, la prórroga o el partido (petición del dueño,
// 02/11/2026).
// ---------------------------------------------------------------------------------------------------
// Es un WAV suelto de la SD, fuera del catálogo de comentarios, con el código FNV-1a de «arbitro/pitido_final»
// (se sube con `python scripts/placa.py pitido <archivo>`). Suena primero y lo demás (voz del descanso, de la
// prórroga, de la victoria…) va detrás, en la cola. Sin el archivo en la SD todo suena como antes.
//
// Cuándo: al acabar la 1ª parte, la 2ª y la prórroga (por tiempo o por goles), y al acabar el partido, salvo
// si lo decide la tanda de penaltis.

#ifndef MFV3_S3_PITIDO_H
#define MFV3_S3_PITIDO_H

#include <stdint.h>
#include "mfv3_engine.h"

namespace pitido {

inline uint32_t fnv(const char* s) {
  uint32_t h = 2166136261u;
  for (const char* p = s; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
  return h;
}

inline uint32_t code() { return fnv("arbitro/pitido_final"); }

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
