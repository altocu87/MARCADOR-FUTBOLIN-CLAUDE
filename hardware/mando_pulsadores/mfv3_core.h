// MARCADOR FUTBOLÍN V3 · núcleo común para placas (Arduino AVR, ESP32, ESP8266…)
// ---------------------------------------------------------------------------
// C++ portable sin dependencias de Arduino: se prueba en el PC con g++.
// Contiene:
//   - Debouncer:   antirrebote de pulsadores y sensores (lectura estable).
//   - EdgeTrigger: dispara una vez por pulsación con un tiempo mínimo entre disparos.
//   - LineBuffer:  junta caracteres recibidos en líneas completas.
//   - parseAppLine: interpreta las órdenes que manda la app (GOAL, LOCK, STATE…).
//
// IMPORTANTE: la placa solo envía pulsaciones. Las reglas (bloqueo de 3 s,
// turnos de penaltis, estados) las valida SIEMPRE el motor de la app.
// Copia idéntica en cada carpeta de sketch (el IDE de Arduino solo compila
// archivos de la propia carpeta); `hardware/test/check_copies.sh` lo comprueba.

#ifndef MFV3_CORE_H
#define MFV3_CORE_H

#include <stdint.h>
#include <string.h>

#define MFV3_PROTOCOL_VERSION "1"

namespace mfv3 {

// Antirrebote: la lectura solo cambia si se mantiene estable `stableMs`.
class Debouncer {
 public:
  explicit Debouncer(uint16_t stableMs = 25) : stableMs_(stableMs) {}

  // `raw` = true si está activo (pulsado / haz cortado). Devuelve el estado estable.
  bool update(bool raw, uint32_t nowMs) {
    if (raw != candidate_) {
      candidate_ = raw;
      changedAt_ = nowMs;
    } else if (raw != stable_ && (uint32_t)(nowMs - changedAt_) >= stableMs_) {
      stable_ = raw;
    }
    return stable_;
  }

  bool stable() const { return stable_; }

 private:
  uint16_t stableMs_;
  bool stable_ = false;
  bool candidate_ = false;
  uint32_t changedAt_ = 0;
};

// Disparo por flanco: true una sola vez al activarse, y nunca antes de `holdOffMs`
// desde el disparo anterior (filtro local contra rebotes del balón en el sensor).
class EdgeTrigger {
 public:
  EdgeTrigger(uint16_t stableMs = 25, uint16_t holdOffMs = 400) : deb_(stableMs), holdOffMs_(holdOffMs) {}

  bool update(bool raw, uint32_t nowMs) {
    bool s = deb_.update(raw, nowMs);
    bool fired = false;
    if (s && !last_) {
      if (!firedOnce_ || (uint32_t)(nowMs - lastFire_) >= holdOffMs_) {
        fired = true;
        firedOnce_ = true;
        lastFire_ = nowMs;
      }
    }
    last_ = s;
    return fired;
  }

 private:
  Debouncer deb_;
  uint16_t holdOffMs_;
  bool last_ = false;
  bool firedOnce_ = false;
  uint32_t lastFire_ = 0;
};

// Sensor de gol de CUALQUIER tipo (barrera de infrarrojos, láser, microinterruptor, inductivo…):
// el nivel que tiene al arrancar se toma como «sin balón», y cualquier cambio estable respecto a él es un gol.
// Así da igual si el sensor da nivel alto o bajo, o si es normalmente abierto o cerrado.
// Un pin sin sensor (con INPUT_PULLUP) nunca cambia, así que tenerlo activado no molesta.
class GoalSensor {
 public:
  explicit GoalSensor(uint16_t stableMs = 5, uint16_t holdOffMs = 800) : trig_(stableMs, holdOffMs) {}

  void begin(bool level) {
    idle_ = level;
    ready_ = true;
  }

  bool update(bool level, uint32_t nowMs) {
    if (!ready_) begin(level);
    return trig_.update(level != idle_, nowMs);
  }

 private:
  EdgeTrigger trig_;
  bool idle_ = true;
  bool ready_ = false;
};

// Pulsador arcade con dos gestos: toque corto (gol) y toque largo (anular gol).
//  - Corto: se suelta antes de `longMs` → se notifica AL SOLTAR.
//  - Largo: se mantiene `longMs` → se notifica EN ESE MOMENTO (sin esperar a soltar) y al soltar no pasa nada.
enum class Press : uint8_t { None, Short, Long };

class PressClassifier {
 public:
  explicit PressClassifier(uint16_t longMs = 800, uint16_t stableMs = 25) : deb_(stableMs), longMs_(longMs) {}

  Press update(bool raw, uint32_t nowMs) {
    bool s = deb_.update(raw, nowMs);
    Press out = Press::None;
    if (s && !down_) {
      down_ = true;
      longSent_ = false;
      downAt_ = nowMs;
    } else if (s && down_ && !longSent_ && (uint32_t)(nowMs - downAt_) >= longMs_) {
      longSent_ = true;
      out = Press::Long;
    } else if (!s && down_) {
      down_ = false;
      if (!longSent_) out = Press::Short;
    }
    return out;
  }

  bool held() const { return down_; }

 private:
  Debouncer deb_;
  uint16_t longMs_;
  bool down_ = false;
  bool longSent_ = false;
  uint32_t downAt_ = 0;
};

// Junta caracteres en líneas terminadas en '\n' (ignora '\r'). Tamaño fijo, sin memoria dinámica.
// N = longitud máxima de una línea (64 en las placas pequeñas; la pantalla de 7" usa más para los sonidos).
template <uint16_t N>
class LineBufferN {
 public:
  // Devuelve true cuando hay una línea completa disponible en line().
  bool push(char c) {
    if (c == '\r') return false;
    if (c == '\n') {
      bool has = len_ > 0 && !overflow_;
      buf_[len_] = '\0';
      ready_ = has;
      len_ = 0;
      overflow_ = false;
      return has;
    }
    if (len_ < N - 1) buf_[len_++] = c;
    else overflow_ = true;  // línea demasiado larga: se descarta entera
    ready_ = false;
    return false;
  }

  const char* line() const { return buf_; }

 private:
  char buf_[N] = {0};
  uint16_t len_ = 0;
  bool ready_ = false;
  bool overflow_ = false;
};

typedef LineBufferN<64> LineBuffer;

enum class AppCmd : uint8_t { None, Hello, State, Score, Goal, Lock, Win, Pong };
enum class TeamId : uint8_t { None, White, Blue };

struct AppMessage {
  AppCmd cmd = AppCmd::None;
  TeamId team = TeamId::None;
  int32_t a = 0;  // SCORE blanco | LOCK ms
  int32_t b = 0;  // SCORE azul
  char state[16] = {0};
};

inline bool startsWith(const char* s, const char* p) { return strncmp(s, p, strlen(p)) == 0; }

inline TeamId parseTeam(const char* s) {
  if (startsWith(s, "BLANCO")) return TeamId::White;
  if (startsWith(s, "AZUL")) return TeamId::Blue;
  return TeamId::None;
}

inline int32_t parseInt(const char*& s) {
  while (*s == ' ') s++;
  int32_t v = 0;
  bool neg = false;
  if (*s == '-') { neg = true; s++; }
  while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
  return neg ? -v : v;
}

// Interpreta una línea enviada por la app.
inline AppMessage parseAppLine(const char* line) {
  AppMessage m;
  if (startsWith(line, "HELLO")) m.cmd = AppCmd::Hello;
  else if (startsWith(line, "PONG")) m.cmd = AppCmd::Pong;
  else if (startsWith(line, "STATE ")) {
    m.cmd = AppCmd::State;
    strncpy(m.state, line + 6, sizeof(m.state) - 1);
  } else if (startsWith(line, "SCORE")) {
    m.cmd = AppCmd::Score;
    const char* p = line + 5;
    m.a = parseInt(p);
    m.b = parseInt(p);
  } else if (startsWith(line, "GOAL ")) {
    m.cmd = AppCmd::Goal;
    m.team = parseTeam(line + 5);
  } else if (startsWith(line, "LOCK")) {
    m.cmd = AppCmd::Lock;
    const char* p = line + 4;
    m.a = parseInt(p);
  } else if (startsWith(line, "WIN ")) {
    m.cmd = AppCmd::Win;
    m.team = parseTeam(line + 4);
  }
  return m;
}

}  // namespace mfv3

#endif  // MFV3_CORE_H
