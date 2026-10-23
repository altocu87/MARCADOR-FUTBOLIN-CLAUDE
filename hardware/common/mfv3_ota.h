// MARCADOR FUTBOLÍN V3 · actualización por radio de la pantalla al mando, y datos de las versiones publicadas.
// -----------------------------------------------------------------------------------------------------------
// La pantalla descarga de internet el programa nuevo del mando y se lo pasa por ESP-NOW (la misma radio de
// los goles), así el mando no necesita saber el Wi-Fi de casa. No depende del hardware: se prueba en el PC
// (hardware/test/ota_test.cpp), con pérdidas de paquetes.
//
// Paquetes «MFVO» (distintos de los «MFV3» de los goles: las placas antiguas los ignoran):
//   MFVO · versión · tipo · grupo · datos
//   INFO   mando → todos   versión del mando (build) y si está listo para recibir
//   OFFER  pantalla → mando  build, tamaño y MD5 del programa nuevo
//   ACCEPT mando → pantalla  0 = vale; si no, por qué no (no armado, no cabe, error)
//   DATA   pantalla → mando  trozo de 200 bytes en una posición
//   ACK    mando → pantalla  posición del siguiente trozo que espera
//   END    pantalla → mando  ya está todo: comprobar y terminar
//   DONE   mando → pantalla  0 = comprobado y listo (se reinicia con el programa nuevo)
//   CANCEL pantalla → mando  dejarlo
//
// Seguridad: el mando solo acepta un programa nuevo si alguien mantiene pulsados sus dos botones 3 s (queda
// «armado» 2 minutos). El programa se comprueba con su MD5 antes de arrancarlo; si no cuadra, sigue el antiguo.
// Se avanza de trozo en trozo con confirmación: un paquete perdido se repite, un duplicado se ignora.

#ifndef MFV3_OTA_H
#define MFV3_OTA_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace mfv3 {
namespace ota {

const uint8_t VERSION = 1;
const uint8_t CHUNK = 200;
const uint8_t HEADER = 7;
const uint8_t MAX_PACKET = HEADER + 5 + CHUNK;  // DATA es el más largo (212 ≤ 250 de ESP-NOW)
const uint32_t ARM_MS = 120000;                 // el mando queda armado 2 minutos
const uint32_t ARM_HOLD_MS = 3000;              // mantener los dos botones 3 s
const uint32_t RX_IDLE_MS = 15000;              // sin datos 15 s durante una recepción: se abandona

enum class Type : uint8_t { Info = 1, Offer = 2, Accept = 3, Data = 4, Ack = 5, End = 6, Done = 7, Cancel = 8 };
enum class Status : uint8_t { Ok = 0, NotArmed = 1, TooBig = 2, Failed = 3 };
// Tipo de placa en INFO (por si algún día hay más placas que se actualizan por radio).
const uint8_t KIND_MANDO = 1;

inline void put32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
inline uint32_t get32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

inline uint8_t header(uint8_t* out, Type t, uint8_t group) {
  out[0] = 'M'; out[1] = 'F'; out[2] = 'V'; out[3] = 'O';
  out[4] = VERSION;
  out[5] = (uint8_t)t;
  out[6] = group;
  return HEADER;
}

// ¿Es un paquete MFVO de esta mesa? Devuelve su tipo (o 0 si no).
inline uint8_t typeOf(const uint8_t* p, int len, uint8_t group) {
  if (len < HEADER || memcmp(p, "MFVO", 4) != 0 || p[4] != VERSION || p[6] != group) return 0;
  return p[5];
}

// ---------------------------------------------------------------- INFO (versión del mando)
// Modelo de mini del mando, para descargar el programa que le corresponde (cada uno enciende su LED a su manera).
// Los mandos anteriores no lo dicen (MODEL_UNKNOWN): la pantalla pregunta el modelo en AJUSTES → MANDO.
const uint8_t MODEL_UNKNOWN = 0, MODEL_SUPER_MINI = 1, MODEL_C3_ZERO = 2;

inline uint8_t encodeInfo(uint8_t* out, uint8_t group, uint32_t build, bool armed, uint8_t model = MODEL_UNKNOWN) {
  uint8_t n = header(out, Type::Info, group);
  put32(out + n, build);
  out[n + 4] = KIND_MANDO;
  out[n + 5] = armed ? 1 : 0;
  out[n + 6] = model;  // un byte más al final: las pantallas antiguas lo ignoran
  return n + 7;
}
struct Info {
  bool ok;
  uint32_t build;
  bool armed;
  uint8_t model;
};
inline Info decodeInfo(const uint8_t* p, int len, uint8_t group) {
  Info i = {false, 0, false, MODEL_UNKNOWN};
  if (typeOf(p, len, group) != (uint8_t)Type::Info || len < HEADER + 6 || p[HEADER + 4] != KIND_MANDO) return i;
  i.ok = true;
  i.build = get32(p + HEADER);
  i.armed = p[HEADER + 5] != 0;
  if (len >= HEADER + 7 && p[HEADER + 6] <= MODEL_C3_ZERO) i.model = p[HEADER + 6];
  return i;
}

// ---------------------------------------------------------------- escritura del programa (en el mando)
// La implementa el sketch con la biblioteca Update del ESP32; en el PC, una memoria.
class Flash {
 public:
  virtual bool begin(uint32_t size, const uint8_t md5[16]) = 0;
  virtual bool write(const uint8_t* data, uint8_t len) = 0;
  virtual bool end() = 0;  // comprueba el MD5 y deja el programa nuevo para el próximo arranque
  virtual void abort() = 0;
  virtual ~Flash() {}
};

// ---------------------------------------------------------------- receptor (mando)
class Receiver {
 public:
  enum class State : uint8_t { Idle, Receiving, Finished };

  Receiver(uint8_t group, uint32_t maxSize) : group_(group), max_(maxSize) {}

  void arm(uint32_t now) {
    armedAt_ = now;
    armed_ = true;
  }
  bool armed(uint32_t now) const { return armed_ && (uint32_t)(now - armedAt_) < ARM_MS; }
  State state() const { return state_; }
  uint32_t received() const { return next_; }
  uint32_t size() const { return size_; }

  // Atiende un paquete; si hay que contestar, deja la respuesta en `reply` y devuelve su longitud.
  uint8_t handle(const uint8_t* p, int len, uint32_t now, Flash& f, uint8_t* reply) {
    uint8_t t = typeOf(p, len, group_);
    if (!t) return 0;
    const uint8_t* d = p + HEADER;
    if (t == (uint8_t)Type::Offer && len >= HEADER + 24) {
      uint32_t build = get32(d), size = get32(d + 4);
      if (state_ == State::Receiving && build == build_ && size == size_ && memcmp(md5_, d + 8, 16) == 0)
        return accept(reply, Status::Ok);  // la misma oferta repetida: seguir donde iba
      if (state_ == State::Receiving) f.abort();
      state_ = State::Idle;
      if (!armed(now)) return accept(reply, Status::NotArmed);
      if (size == 0 || size > max_) return accept(reply, Status::TooBig);
      if (!f.begin(size, d + 8)) return accept(reply, Status::Failed);
      state_ = State::Receiving;
      build_ = build;
      size_ = size;
      memcpy(md5_, d + 8, 16);
      next_ = 0;
      lastAt_ = now;
      return accept(reply, Status::Ok);
    }
    if (t == (uint8_t)Type::Data && len >= HEADER + 5) {
      if (state_ != State::Receiving) return 0;
      uint32_t off = get32(d);
      uint8_t n = d[4];
      if (n == 0 || n > CHUNK || len < HEADER + 5 + n) return 0;
      lastAt_ = now;
      Status st = Status::Ok;
      if (off == next_ && next_ + n <= size_) {
        if (f.write(d + 5, n)) next_ += n;
        else {
          f.abort();
          state_ = State::Idle;
          st = Status::Failed;
        }
      }
      return ack(reply, st);  // si no era el esperado (duplicado o salto), se repite dónde va
    }
    if (t == (uint8_t)Type::End) {
      if (state_ == State::Finished) return done(reply, Status::Ok);  // DONE perdido: se repite
      if (state_ != State::Receiving) return 0;
      if (next_ != size_) return ack(reply, Status::Ok);
      bool ok = f.end();
      state_ = ok ? State::Finished : State::Idle;
      armed_ = false;
      return done(reply, ok ? Status::Ok : Status::Failed);
    }
    if (t == (uint8_t)Type::Cancel) {
      if (state_ == State::Receiving) f.abort();
      state_ = State::Idle;
    }
    return 0;
  }

  // Recepción a medias que se queda parada: se abandona (el programa antiguo sigue).
  void tick(uint32_t now, Flash& f) {
    if (state_ == State::Receiving && (uint32_t)(now - lastAt_) > RX_IDLE_MS) {
      f.abort();
      state_ = State::Idle;
    }
    if (armed_ && !armed(now)) armed_ = false;
  }

 private:
  uint8_t group_;
  uint32_t max_;
  State state_ = State::Idle;
  bool armed_ = false;
  uint32_t armedAt_ = 0, lastAt_ = 0;
  uint32_t build_ = 0, size_ = 0, next_ = 0;
  uint8_t md5_[16] = {0};

  uint8_t accept(uint8_t* r, Status s) {
    uint8_t n = header(r, Type::Accept, group_);
    r[n] = (uint8_t)s;
    return n + 1;
  }
  uint8_t ack(uint8_t* r, Status s) {
    uint8_t n = header(r, Type::Ack, group_);
    put32(r + n, next_);
    r[n + 4] = (uint8_t)s;
    return n + 5;
  }
  uint8_t done(uint8_t* r, Status s) {
    uint8_t n = header(r, Type::Done, group_);
    r[n] = (uint8_t)s;
    return n + 1;
  }
};

// ---------------------------------------------------------------- origen de los datos (en la pantalla)
class Source {
 public:
  virtual bool read(uint32_t offset, uint8_t* buf, uint8_t len) = 0;
  virtual ~Source() {}
};

// ---------------------------------------------------------------- emisor (pantalla)
class Sender {
 public:
  enum class State : uint8_t { Idle, Offering, Sending, Ending, Done, Failed };
  enum class Error : uint8_t { None, NoAnswer, Refused, Lost, Verify, Read };

  static const uint32_t OFFER_EVERY_MS = 500;
  static const uint32_t OFFER_WAIT_MS = ARM_MS;  // tiempo para que alguien pulse los botones del mando
  static const uint32_t ACK_WAIT_MS = 120;
  static const uint8_t MAX_RETRIES = 40;
  static const uint32_t END_EVERY_MS = 300;
  static const uint8_t MAX_END = 20;

  void start(uint8_t group, uint32_t build, uint32_t size, const uint8_t md5[16], uint32_t now) {
    group_ = group;
    build_ = build;
    size_ = size;
    memcpy(md5_, md5, 16);
    state_ = State::Offering;
    error_ = Error::None;
    lastAccept_ = Status::NotArmed;
    heard_ = false;
    next_ = 0;
    startedAt_ = now;
    lastSend_ = now - OFFER_EVERY_MS;  // la primera oferta, ya
    retries_ = 0;
  }
  void cancel() {
    if (state_ != State::Done) {
      state_ = State::Failed;
      error_ = Error::Refused;
      cancelPending_ = true;
    }
  }

  State state() const { return state_; }
  Error error() const { return error_; }
  uint32_t sent() const { return next_; }
  uint32_t size() const { return size_; }
  // El mando contestó a la oferta pero aún no está armado (hay que pulsar sus dos botones).
  bool waitingButtons() const { return state_ == State::Offering && heard_ && lastAccept_ == Status::NotArmed; }
  uint8_t percent() const { return size_ ? (uint8_t)((uint64_t)next_ * 100 / size_) : 0; }

  // Lo que hay que enviar ahora (longitud, 0 = nada).
  uint8_t poll(uint32_t now, Source& src, uint8_t* out) {
    if (cancelPending_) {
      cancelPending_ = false;
      return header(out, Type::Cancel, group_);
    }
    switch (state_) {
      case State::Offering: {
        if ((uint32_t)(now - startedAt_) > OFFER_WAIT_MS) return fail(Error::NoAnswer);
        if ((uint32_t)(now - lastSend_) < OFFER_EVERY_MS) return 0;
        lastSend_ = now;
        uint8_t n = header(out, Type::Offer, group_);
        put32(out + n, build_);
        put32(out + n + 4, size_);
        memcpy(out + n + 8, md5_, 16);
        return n + 24;
      }
      case State::Sending: {
        if (retries_ > 0 && (uint32_t)(now - lastSend_) < ACK_WAIT_MS) return 0;
        if (retries_ > 0 && sentAt_ == next_ && retries_ >= MAX_RETRIES) return fail(Error::Lost);
        if (sentAt_ != next_) retries_ = 0;
        uint32_t left = size_ - next_;
        uint8_t len = left < CHUNK ? (uint8_t)left : CHUNK;
        uint8_t n = header(out, Type::Data, group_);
        put32(out + n, next_);
        out[n + 4] = len;
        if (!src.read(next_, out + n + 5, len)) return fail(Error::Read);
        sentAt_ = next_;
        retries_++;
        lastSend_ = now;
        return n + 5 + len;
      }
      case State::Ending: {
        if ((uint32_t)(now - lastSend_) < END_EVERY_MS) return 0;
        if (retries_ >= MAX_END) return fail(Error::Lost);
        retries_++;
        lastSend_ = now;
        return header(out, Type::End, group_);
      }
      default: return 0;
    }
  }

  // Respuesta del mando.
  void onPacket(const uint8_t* p, int len, uint32_t now) {
    uint8_t t = typeOf(p, len, group_);
    const uint8_t* d = p + HEADER;
    if (t == (uint8_t)Type::Accept && len >= HEADER + 1 && state_ == State::Offering) {
      heard_ = true;
      lastAccept_ = (Status)d[0];
      if (lastAccept_ == Status::Ok) {
        state_ = State::Sending;
        retries_ = 0;
        sentAt_ = 0xFFFFFFFF;
      } else if (lastAccept_ != Status::NotArmed) {
        fail(Error::Refused);
      }
    } else if (t == (uint8_t)Type::Ack && len >= HEADER + 5 && (state_ == State::Sending || state_ == State::Ending)) {
      if ((Status)d[4] != Status::Ok) {
        fail(Error::Refused);
        return;
      }
      uint32_t got = get32(d);
      if (got > next_ && got <= size_) {
        next_ = got;
        retries_ = 0;
        lastSend_ = now - ACK_WAIT_MS;  // el siguiente trozo, ya
      }
      if (next_ == size_ && state_ == State::Sending) {
        state_ = State::Ending;
        retries_ = 0;
        lastSend_ = now - END_EVERY_MS;
      }
    } else if (t == (uint8_t)Type::Done && len >= HEADER + 1 && state_ == State::Ending) {
      if ((Status)d[0] == Status::Ok) state_ = State::Done;
      else fail(Error::Verify);
    }
  }

 private:
  uint8_t group_ = 0;
  State state_ = State::Idle;
  Error error_ = Error::None;
  Status lastAccept_ = Status::NotArmed;
  bool heard_ = false, cancelPending_ = false;
  uint32_t build_ = 0, size_ = 0, next_ = 0, sentAt_ = 0xFFFFFFFF;
  uint32_t startedAt_ = 0, lastSend_ = 0;
  uint8_t retries_ = 0;
  uint8_t md5_[16] = {0};

  uint8_t fail(Error e) {
    state_ = State::Failed;
    error_ = e;
    return 0;
  }
};

// ---------------------------------------------------------------- versiones publicadas (ota.json)
// GitHub publica para cada placa firmware/<placa>/ota.json: {"build": 57, "size": 1234567, "md5": "…"}
struct Published {
  uint32_t build = 0;
  uint32_t size = 0;
  char md5[33] = {0};
  bool ok() const { return build > 0 && size > 0 && strlen(md5) == 32; }
};

inline const char* findKey(const char* s, const char* key) {
  char pat[24];
  size_t k = strlen(key);
  if (k + 3 > sizeof(pat)) return nullptr;
  pat[0] = '"';
  memcpy(pat + 1, key, k);
  pat[k + 1] = '"';
  pat[k + 2] = 0;
  const char* p = strstr(s, pat);
  if (!p) return nullptr;
  p += k + 2;
  while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  return p;
}

inline bool parsePublished(const char* json, Published& out) {
  out = Published();
  const char* b = findKey(json, "build");
  const char* s = findKey(json, "size");
  const char* m = findKey(json, "md5");
  if (!b || !s || !m || *m != '"') return false;
  out.build = (uint32_t)strtoul(b, nullptr, 10);
  out.size = (uint32_t)strtoul(s, nullptr, 10);
  m++;
  for (int i = 0; i < 32; i++) {
    char c = m[i];
    bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!hex) return false;
    out.md5[i] = (char)(c >= 'A' && c <= 'F' ? c - 'A' + 'a' : c);
  }
  if (m[32] != '"') return false;
  out.md5[32] = 0;
  return out.ok();
}

inline bool md5FromHex(const char* hex, uint8_t out[16]) {
  for (int i = 0; i < 16; i++) {
    char h[3] = {hex[i * 2], hex[i * 2 + 1], 0};
    if (!h[0] || !h[1]) return false;
    char* e = nullptr;
    out[i] = (uint8_t)strtoul(h, &e, 16);
    if (*e) return false;
  }
  return true;
}

}  // namespace ota
}  // namespace mfv3

#endif  // MFV3_OTA_H
