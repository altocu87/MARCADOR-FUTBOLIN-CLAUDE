// MARCADOR FUTBOLÍN V3 · sonidos propios en la placa (lógica común, sin hardware)
// -------------------------------------------------------------------------------
//   - Momentos del partido con sonido (los mismos nombres que la app) y su detección con el motor de la placa.
//   - Qué sonido va con cada momento: «el de siempre», «ninguno» o uno de varios propios al azar.
//   - Órdenes del protocolo para recibir sonidos de la app por el cable (en trozos base64 con CRC32),
//     listarlos, borrarlos y tocarlos.
//   - WAV, cambio de frecuencia y los sonidos «de siempre» generados con notas.
// Cada sonido se conoce en la placa por un código de 8 cifras hexadecimales (FNV-1a de su id en la app) y se guarda
// como /sonidos/<código>.wav en la memoria del chip o en la tarjeta SD. Se prueba en PC (hardware/test/sound_test.cpp).
// Copia idéntica en la carpeta del sketch de la pantalla de 7" (lo comprueba hardware/test/check_copies.sh).

#ifndef MFV3_SOUND_H
#define MFV3_SOUND_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mfv3_engine.h"

namespace mfv3snd {

using mfv3::Engine;
using mfv3::Period;
using mfv3::Phase;
using mfv3::Score;
using mfv3::Team;

// ---------------------------------------------------------------- momentos
enum class Event : uint8_t {
  Goal, GoalAnnulled, Streak3, Comeback, Equalizer, MatchPoint, GoldenGoal, PenaltyScored, PenaltyMissed,
  MatchStart, PeriodEnd, Victory, Rout, Count
};
static const uint8_t EVENT_COUNT = (uint8_t)Event::Count;
static const char* const EVENT_NAMES[EVENT_COUNT] = {
    "goal",          "goalAnnulled",  "streak3",    "comeback",  "equalizer", "matchPoint", "goldenGoal",
    "penaltyScored", "penaltyMissed", "matchStart", "periodEnd", "victory",   "rout"};

inline int eventFromName(const char* s, size_t len) {
  for (uint8_t i = 0; i < EVENT_COUNT; i++)
    if (strlen(EVENT_NAMES[i]) == len && !strncmp(EVENT_NAMES[i], s, len)) return i;
  return -1;
}

// Estado justo antes de un cambio del motor.
struct Before {
  Phase phase;
  Period period;
  Score score;
  uint8_t kicks;
  uint8_t matchPoint;
};

inline Before before(const Engine& e) { return Before{e.phase(), e.period(), e.score(), e.kickCount(), e.matchPoint()}; }

struct EventList {
  Event ev[4];
  uint8_t n = 0;
  void add(Event e) {
    if (n < 4) ev[n++] = e;
  }
  bool has(Event e) const {
    for (uint8_t i = 0; i < n; i++)
      if (ev[i] == e) return true;
    return false;
  }
};

// Goleada: ganar dejando al rival a 0 o por 4 o más (marcador ordinario; los penaltis no cuentan).
inline bool isRout(const Engine& e) {
  Score s = e.score();
  uint8_t w = s.of(e.winner()), l = s.of(e.winner() == Team::White ? Team::Blue : Team::White);
  return w > l && (l == 0 || w - l >= 4);
}

// Sonido del último gol válido cuando no termina el partido: el momento más especial gana
// (remontada > 3 seguidos > empate > gol), igual que en la app.
inline Event goalEvent(const Engine& e) {
  int last = -1;
  for (uint8_t i = 0; i < e.goalCount(); i++)
    if (!e.goalAt(i).annulled) last = i;
  if (last < 0) return Event::Goal;
  Team team = e.goalAt((uint8_t)last).team;
  int sw = 0, sb = 0, worstW = 0, worstB = 0, beforeT = 0, beforeO = 0, worst = 0;
  for (int i = 0; i <= last; i++) {
    const mfv3::Goal& g = e.goalAt((uint8_t)i);
    if (g.annulled) continue;
    if (i == last) {
      beforeT = team == Team::White ? sw : sb;
      beforeO = team == Team::White ? sb : sw;
      worst = team == Team::White ? worstW : worstB;
    }
    (g.team == Team::White ? sw : sb)++;
    if (sb - sw > worstW) worstW = sb - sw;
    if (sw - sb > worstB) worstB = sw - sb;
  }
  int afterT = beforeT + 1, afterO = beforeO;
  if (beforeT <= beforeO && afterT > afterO && worst >= 2) return Event::Comeback;
  int streak = 0;
  for (int i = last; i >= 0; i--) {
    const mfv3::Goal& g = e.goalAt((uint8_t)i);
    if (g.annulled) continue;
    if (g.team != team) break;
    streak++;
  }
  if (streak == 3) return Event::Streak3;
  if (afterT == afterO && afterT > 0) return Event::Equalizer;
  return Event::Goal;
}

// Momentos de un cambio del motor, en orden: el primero suena ya y los demás, en cola.
inline EventList detect(const Engine& e, const Before& b, bool goalAccepted) {
  EventList out;
  Score s = e.score();
  Phase ph = e.phase();
  bool finished = ph == Phase::Finished && b.phase != Phase::Finished;
  bool scoreChanged = s.white != b.score.white || s.blue != b.score.blue;
  if (b.period == e.period() && (s.white < b.score.white || s.blue < b.score.blue)) out.add(Event::GoalAnnulled);
  if (b.phase == Phase::Countdown && b.period == Period::First && ph != Phase::Countdown) out.add(Event::MatchStart);
  if (ph == Phase::PeriodEnd && b.phase != Phase::PeriodEnd) out.add(Event::PeriodEnd);
  if (e.kickCount() > b.kicks) out.add(e.kick((uint8_t)(e.kickCount() - 1)).scored ? Event::PenaltyScored : Event::PenaltyMissed);
  if (goalAccepted) {
    if (!finished) out.add(goalEvent(e));
    else if (b.period == Period::Overtime) out.add(Event::GoldenGoal);
  }
  if (finished) out.add(isRout(e) ? Event::Rout : Event::Victory);
  if (!finished && e.matchPoint() && e.period() != Period::Overtime &&
      (b.matchPoint == 0 || scoreChanged || b.period != e.period()))
    out.add(Event::MatchPoint);
  return out;
}

// ---------------------------------------------------------------- códigos, CRC y base64
// Código de un sonido: FNV-1a de 32 bits del id de la app (la app calcula el mismo).
inline uint32_t soundCode(const char* id) {
  uint32_t h = 2166136261u;
  for (const char* p = id; *p; p++) {
    h ^= (uint8_t)*p;
    h *= 16777619u;
  }
  return h;
}

inline bool parseCode(const char* s, size_t len, uint32_t& out) {
  if (len != 8) return false;
  uint32_t v = 0;
  for (size_t i = 0; i < len; i++) {
    char c = s[i];
    uint8_t d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 255;
    if (d == 255) return false;
    v = (v << 4) | d;
  }
  out = v;
  return true;
}

inline uint32_t crc32Update(uint32_t crc, const uint8_t* d, size_t n) {
  crc = ~crc;
  for (size_t i = 0; i < n; i++) {
    crc ^= d[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

// Descodifica base64 (sin saltos de línea). Devuelve los bytes escritos o -1 si no es válido.
inline int base64Decode(const char* s, size_t len, uint8_t* out, size_t cap) {
  if (len % 4) return -1;
  size_t n = 0;
  for (size_t i = 0; i < len; i += 4) {
    uint32_t v = 0;
    int pad = 0;
    for (int k = 0; k < 4; k++) {
      char c = s[i + k];
      int d = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52
            : c == '+' ? 62 : c == '/' ? 63 : c == '=' ? -2 : -1;
      if (d == -1 || (d == -2 && i + 4 < len)) return -1;
      if (d == -2) {
        pad++;
        d = 0;
      } else if (pad) return -1;
      v = (v << 6) | (uint32_t)d;
    }
    if (pad > 2) return -1;
    uint8_t b[3] = {(uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
    for (int k = 0; k < 3 - pad; k++) {
      if (n >= cap) return -1;
      out[n++] = b[k];
    }
  }
  return (int)n;
}

// ---------------------------------------------------------------- qué suena en cada momento
enum class Kind : uint8_t { Default, None, Custom };
enum class Place : uint8_t { Chip, Sd };

struct Choice {
  Kind kind = Kind::Default;
  uint8_t n = 0;
  uint32_t codes[4] = {0, 0, 0, 0};
};

struct Choices {
  Choice c[EVENT_COUNT];

  // «<momento> def» · «<momento> no» · «<momento> 1a2b3c4d,5e6f7a8b» (hasta 4, uno al azar).
  bool set(const char* args) {
    while (*args == ' ') args++;
    const char* sp = strchr(args, ' ');
    if (!sp) return false;
    int ev = eventFromName(args, (size_t)(sp - args));
    if (ev < 0) return false;
    const char* v = sp + 1;
    while (*v == ' ') v++;
    Choice ch;
    if (!strncmp(v, "def", 3)) ch.kind = Kind::Default;
    else if (!strncmp(v, "no", 2)) ch.kind = Kind::None;
    else {
      ch.kind = Kind::Custom;
      while (*v && ch.n < 4) {
        const char* end = v;
        while (*end && *end != ',' && *end != ' ' && *end != ';') end++;
        uint32_t code;
        if (!parseCode(v, (size_t)(end - v), code)) return false;
        ch.codes[ch.n++] = code;
        if (*end != ',') break;
        v = end + 1;
      }
      if (!ch.n) return false;
    }
    c[ev] = ch;
    return true;
  }

  // Texto para guardar en la memoria (NVS): «goal 1a2b3c4d;rout no;…» (solo lo que no es «de siempre»).
  size_t serialize(char* buf, size_t cap) const {
    size_t n = 0;
    if (cap) buf[0] = 0;
    for (uint8_t i = 0; i < EVENT_COUNT; i++) {
      if (c[i].kind == Kind::Default) continue;
      char item[64];
      int k = snprintf(item, sizeof(item), "%s ", EVENT_NAMES[i]);
      if (c[i].kind == Kind::None) k += snprintf(item + k, sizeof(item) - k, "no");
      for (uint8_t j = 0; c[i].kind == Kind::Custom && j < c[i].n; j++)
        k += snprintf(item + k, sizeof(item) - k, "%s%08lx", j ? "," : "", (unsigned long)c[i].codes[j]);
      if (n + (size_t)k + 2 > cap) break;
      n += (size_t)snprintf(buf + n, cap - n, "%s%s", n ? ";" : "", item);
    }
    return n;
  }

  void parse(const char* text) {
    *this = Choices();
    char item[64];
    while (text && *text) {
      const char* end = strchr(text, ';');
      size_t len = end ? (size_t)(end - text) : strlen(text);
      if (len < sizeof(item)) {
        memcpy(item, text, len);
        item[len] = 0;
        set(item);
      }
      text = end ? end + 1 : nullptr;
    }
  }
};

// ---------------------------------------------------------------- WAV
struct WavInfo {
  uint32_t rate = 0;
  uint16_t channels = 0;
  uint32_t dataOffset = 0;
  uint32_t dataBytes = 0;
};

// Lee la cabecera de un WAV PCM de 16 bits (en los primeros bytes del archivo).
inline bool parseWav(const uint8_t* d, size_t len, WavInfo& out) {
  if (len < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return false;
  size_t p = 12;
  bool fmt = false;
  while (p + 8 <= len) {
    uint32_t sz = (uint32_t)d[p + 4] | ((uint32_t)d[p + 5] << 8) | ((uint32_t)d[p + 6] << 16) | ((uint32_t)d[p + 7] << 24);
    if (!memcmp(d + p, "fmt ", 4)) {
      if (p + 24 > len) return false;
      uint16_t format = (uint16_t)(d[p + 8] | (d[p + 9] << 8));
      out.channels = (uint16_t)(d[p + 10] | (d[p + 11] << 8));
      out.rate = (uint32_t)d[p + 12] | ((uint32_t)d[p + 13] << 8) | ((uint32_t)d[p + 14] << 16) | ((uint32_t)d[p + 15] << 24);
      uint16_t bits = (uint16_t)(d[p + 22] | (d[p + 23] << 8));
      if (format != 1 || bits != 16 || out.channels < 1 || out.channels > 2 || out.rate < 4000 || out.rate > 48000) return false;
      fmt = true;
    } else if (!memcmp(d + p, "data", 4)) {
      if (!fmt) return false;
      out.dataOffset = (uint32_t)(p + 8);
      out.dataBytes = sz;
      return true;
    }
    p += 8 + sz + (sz & 1);
  }
  return false;
}

// Cambio de frecuencia por interpolación lineal, por trozos (una pista).
class Resampler {
 public:
  void begin(uint32_t from, uint32_t to) {
    step_ = (uint32_t)(((uint64_t)from << 16) / to);
    pos_ = 0;
    prev_ = 0;
    primed_ = false;
  }
  // Consume `in` (n muestras) y escribe en `out` como mucho `cap`. Devuelve las escritas; `used` = consumidas.
  size_t process(const int16_t* in, size_t n, int16_t* out, size_t cap, size_t& used) {
    size_t w = 0;
    used = 0;
    if (!primed_ && n) {
      prev_ = in[0];
      primed_ = true;
      used = 1;
    }
    while (w < cap) {
      // pos_ (16.16) está entre prev_ (0) y la siguiente muestra (1).
      while (pos_ >= 0x10000u) {
        if (used >= n) return w;
        prev_ = in[used++];
        pos_ -= 0x10000u;
      }
      if (used >= n) return w;
      int32_t next = in[used];
      out[w++] = (int16_t)(prev_ + (((next - prev_) * (int32_t)pos_) >> 16));
      pos_ += step_;
    }
    return w;
  }

 private:
  uint32_t step_ = 0x10000u, pos_ = 0;
  int32_t prev_ = 0;
  bool primed_ = false;
};

// ---------------------------------------------------------------- sonidos «de siempre» (notas)
struct Note {
  uint16_t freq;  // 0 = silencio
  uint16_t ms;
};

static const Note TONE_GOAL[] = {{523, 110}, {659, 110}, {784, 110}, {1047, 330}};
static const Note TONE_ERROR[] = {{200, 80}, {150, 90}};
static const Note TONE_MATCH_POINT[] = {{880, 120}, {0, 60}, {880, 120}};
static const Note TONE_START[] = {{1320, 300}};
static const Note TONE_PERIOD_END[] = {{880, 250}, {0, 50}, {660, 450}};
static const Note TONE_VICTORY[] = {{523, 180}, {659, 180}, {784, 180}, {1047, 600}};

struct Tune {
  const Note* notes;
  uint8_t n;
};

// El de siempre de cada momento (el mismo reparto que la app).
inline Tune defaultTune(Event e) {
  switch (e) {
    case Event::GoalAnnulled:
    case Event::PenaltyMissed: return Tune{TONE_ERROR, 2};
    case Event::MatchPoint: return Tune{TONE_MATCH_POINT, 3};
    case Event::MatchStart: return Tune{TONE_START, 1};
    case Event::PeriodEnd: return Tune{TONE_PERIOD_END, 3};
    case Event::Victory:
    case Event::Rout: return Tune{TONE_VICTORY, 4};
    default: return Tune{TONE_GOAL, 4};
  }
}

// Genera las notas como onda cuadrada suave (una pista).
class ToneGen {
 public:
  void start(Tune t, uint32_t rate) {
    tune_ = t;
    rate_ = rate;
    note_ = 0;
    left_ = t.n ? (uint32_t)t.notes[0].ms * rate / 1000 : 0;
    phase_ = 0;
  }
  bool active() const { return note_ < tune_.n; }
  size_t render(int16_t* out, size_t cap) {
    size_t w = 0;
    while (w < cap && note_ < tune_.n) {
      if (!left_) {
        if (++note_ >= tune_.n) break;
        left_ = (uint32_t)tune_.notes[note_].ms * rate_ / 1000;
        continue;
      }
      uint16_t f = tune_.notes[note_].freq;
      int16_t v = 0;
      if (f) {
        phase_ += (uint32_t)(((uint64_t)f << 32) / rate_);
        uint32_t total = (uint32_t)tune_.notes[note_].ms * rate_ / 1000;
        uint32_t done = total - left_;
        uint32_t edge = rate_ / 200;  // 5 ms de subida y bajada (sin chasquidos)
        int32_t amp = 6000;
        if (done < edge) amp = amp * (int32_t)done / (int32_t)edge;
        if (left_ < edge) amp = amp * (int32_t)left_ / (int32_t)edge;
        v = (int16_t)((phase_ & 0x80000000u) ? amp : -amp);
      }
      out[w++] = v;
      left_--;
    }
    return w;
  }

 private:
  Tune tune_ = {nullptr, 0};
  uint32_t rate_ = 16000, left_ = 0, phase_ = 0;
  uint8_t note_ = 0;
};

// ---------------------------------------------------------------- almacén de sonidos (lo pone cada placa)
class Store {
 public:
  virtual ~Store() {}
  virtual bool available(Place p) = 0;
  virtual uint32_t freeKB(Place p) = 0;
  // Recepción de un archivo: se escribe aparte y solo pasa a ser el sonido al terminar bien.
  virtual bool begin(Place p, uint32_t code) = 0;
  virtual bool write(const uint8_t* d, size_t n) = 0;
  virtual bool commit() = 0;
  virtual void abort() = 0;
  virtual bool remove(uint32_t code) = 0;
  // ¿Dónde está? (la tarjeta SD primero).
  virtual bool find(uint32_t code, Place& where) = 0;
  // Lista hasta `cap` sonidos guardados.
  virtual size_t list(uint32_t* codes, Place* places, size_t cap) = 0;
};

static const uint32_t MAX_SOUND_BYTES = 1048576;  // 1 MB (10 s a 44 kHz con margen)
static const uint32_t UPLOAD_TIMEOUT_MS = 8000;

// Lo que pide una orden al sketch después de contestarla.
struct Action {
  bool play = false;
  bool queue = false;  // «SONIDO <código> cola»: espera a que acabe lo que suena
  uint32_t code = 0;
  bool choicesChanged = false;
  bool volumeChanged = false;
  uint8_t volume = 0;
};

typedef void (*ReplyFn)(const char* line);

// Órdenes de sonido del protocolo (app → placa):
//   SONIDOS?                               → «SONIDOS chip=<KB libres> sd=<KB libres|-> <código>@c,<código>@s,…»
//   SONIDO_SUBIR <chip|sd> <código> <bytes> → «SONIDO_LISTO <código>» o «SONIDO_ERROR <código> <motivo>»
//   SONIDO_DATOS <base64>                  → «SONIDO_OK <bytes recibidos>» (la app espera esa respuesta)
//   SONIDO_FIN <crc32>                     → «SONIDO_GUARDADO <código>» o «SONIDO_ERROR <código> crc»
//   SONIDO_BORRAR <código>                 → «SONIDO_BORRADO <código>»
//   SONIDO <código> [cola]                 → lo toca (sin respuesta); «cola»: al acabar lo que suena
//   MOMENTO <momento> <def|no|códigos>     → «MOMENTO_OK <momento>»
//   VOLUMEN <0–100>                        → sin respuesta
class Commands {
 public:
  Commands(Store& store, Choices& choices) : store_(store), choices_(choices) {}

  bool receiving() const { return receiving_; }

  // Cancela una recepción a medias si la app deja de mandar datos.
  void tick(uint32_t now, ReplyFn reply) {
    if (receiving_ && now - lastData_ > UPLOAD_TIMEOUT_MS) fail(reply, "tiempo");
  }

  // Devuelve true si la línea era una orden de sonido.
  bool handle(const char* line, uint32_t now, ReplyFn reply, Action& act) {
    act = Action();
    const char* sp = strchr(line, ' ');
    size_t wl = sp ? (size_t)(sp - line) : strlen(line);
    const char* args = sp ? sp + 1 : "";
    char buf[160];
    if (is(line, wl, "SONIDOS?")) {
      listReply(reply);
    } else if (is(line, wl, "SONIDO_SUBIR")) {
      char where[8] = {0}, code[16] = {0};
      unsigned long size = 0;
      if (receiving_) {
        store_.abort();
        receiving_ = false;
      }
      if (sscanf(args, "%7s %15s %lu", where, code, &size) != 3 || !parseCode(code, strlen(code), code_)) {
        reply("SONIDO_ERROR - orden");
        return true;
      }
      Place p = !strcmp(where, "sd") ? Place::Sd : Place::Chip;
      const char* why = nullptr;
      if (!store_.available(p)) why = p == Place::Sd ? "sin_sd" : "sin_memoria";
      else if (size == 0 || size > MAX_SOUND_BYTES) why = "tamano";
      else if ((uint64_t)store_.freeKB(p) * 1024u < (uint64_t)size + 8192u) why = "sin_espacio";
      else if (!store_.begin(p, code_)) why = "escritura";
      if (why) {
        snprintf(buf, sizeof(buf), "SONIDO_ERROR %08lx %s", (unsigned long)code_, why);
        reply(buf);
        return true;
      }
      receiving_ = true;
      expected_ = (uint32_t)size;
      got_ = 0;
      crc_ = 0;
      lastData_ = now;
      snprintf(buf, sizeof(buf), "SONIDO_LISTO %08lx", (unsigned long)code_);
      reply(buf);
    } else if (is(line, wl, "SONIDO_DATOS")) {
      if (!receiving_) {
        reply("SONIDO_ERROR - sin_subida");
        return true;
      }
      uint8_t bin[256];
      int n = base64Decode(args, strlen(args), bin, sizeof(bin));
      if (n < 0) return fail(reply, "datos");
      if (got_ + (uint32_t)n > expected_) return fail(reply, "tamano");
      if (n && !store_.write(bin, (size_t)n)) return fail(reply, "escritura");
      got_ += (uint32_t)n;
      crc_ = crc32Update(crc_, bin, (size_t)n);
      lastData_ = now;
      snprintf(buf, sizeof(buf), "SONIDO_OK %lu", (unsigned long)got_);
      reply(buf);
    } else if (is(line, wl, "SONIDO_FIN")) {
      if (!receiving_) {
        reply("SONIDO_ERROR - sin_subida");
        return true;
      }
      uint32_t crc;
      if (!parseCode(args, strlen(args), crc) || crc != crc_ || got_ != expected_) return fail(reply, "crc");
      receiving_ = false;
      if (!store_.commit()) {
        snprintf(buf, sizeof(buf), "SONIDO_ERROR %08lx escritura", (unsigned long)code_);
        reply(buf);
        return true;
      }
      snprintf(buf, sizeof(buf), "SONIDO_GUARDADO %08lx", (unsigned long)code_);
      reply(buf);
    } else if (is(line, wl, "SONIDO_BORRAR")) {
      uint32_t code;
      if (!parseCode(args, strlen(args), code)) return true;
      store_.remove(code);
      snprintf(buf, sizeof(buf), "SONIDO_BORRADO %08lx", (unsigned long)code);
      reply(buf);
    } else if (is(line, wl, "SONIDO")) {
      const char* end = strchr(args, ' ');
      if (parseCode(args, end ? (size_t)(end - args) : strlen(args), act.code)) {
        act.play = true;
        act.queue = end && !strncmp(end + 1, "cola", 4);
      }
    } else if (is(line, wl, "MOMENTO")) {
      if (choices_.set(args)) {
        act.choicesChanged = true;
        const char* e = strchr(args, ' ');
        snprintf(buf, sizeof(buf), "MOMENTO_OK %.*s", e ? (int)(e - args) : 0, args);
        reply(buf);
      }
    } else if (is(line, wl, "VOLUMEN")) {
      int v = atoi(args);
      act.volume = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
      act.volumeChanged = true;
    } else {
      return false;
    }
    return true;
  }

 private:
  static bool is(const char* w, size_t wl, const char* name) {
    size_t n = strlen(name);
    if (n != wl) return false;
    for (size_t i = 0; i < n; i++)
      if (toupperAscii(w[i]) != name[i]) return false;
    return true;
  }
  static char toupperAscii(char c) { return c >= 'a' && c <= 'z' ? (char)(c - 32) : c; }

  bool fail(ReplyFn reply, const char* why) {
    store_.abort();
    receiving_ = false;
    char buf[64];
    snprintf(buf, sizeof(buf), "SONIDO_ERROR %08lx %s", (unsigned long)code_, why);
    reply(buf);
    return true;
  }

  void listReply(ReplyFn reply) {
    uint32_t codes[48];
    Place places[48];
    size_t n = store_.list(codes, places, 48);
    char buf[48 * 11 + 64];
    int k = snprintf(buf, sizeof(buf), "SONIDOS");
    if (store_.available(Place::Chip)) k += snprintf(buf + k, sizeof(buf) - k, " chip=%lu", (unsigned long)store_.freeKB(Place::Chip));
    else k += snprintf(buf + k, sizeof(buf) - k, " chip=-");
    if (store_.available(Place::Sd)) k += snprintf(buf + k, sizeof(buf) - k, " sd=%lu", (unsigned long)store_.freeKB(Place::Sd));
    else k += snprintf(buf + k, sizeof(buf) - k, " sd=-");
    for (size_t i = 0; i < n; i++)
      k += snprintf(buf + k, sizeof(buf) - k, "%s%08lx@%c", i ? "," : " ", (unsigned long)codes[i], places[i] == Place::Sd ? 's' : 'c');
    reply(buf);
  }

  Store& store_;
  Choices& choices_;
  bool receiving_ = false;
  uint32_t code_ = 0, expected_ = 0, got_ = 0, crc_ = 0, lastData_ = 0;
};

}  // namespace mfv3snd

#endif  // MFV3_SOUND_H
