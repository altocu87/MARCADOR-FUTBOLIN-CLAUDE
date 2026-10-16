// MARCADOR FUTBOLÍN V3 · qué suena en cada momento del partido (pantalla de 7").
// -----------------------------------------------------------------------------------------
// Sin hardware: se prueba en el PC (test/sound_test.cpp). La parte que toca el altavoz está en s3_audio.h.
//   - Momentos: los mismos que la app (src/services/sound/soundEvents.ts) que tienen sentido en la placa.
//   - Lectura de la cabecera de un WAV (PCM de 8 o 16 bits, mono o estéreo, cualquier frecuencia).
//   - Los sonidos «de siempre» (los mismos tonos que la app) para cuando la SD no trae uno propio.

#ifndef MFV3_S3_SOUND_H
#define MFV3_S3_SOUND_H

#include <math.h>
#include <stdint.h>
#include <string.h>
#include "mfv3_engine.h"

namespace s3snd {

enum class Ev : uint8_t {
  Goal,
  GoalAnnulled,
  Streak3,
  Comeback,
  Equalizer,
  MatchPoint,
  GoldenGoal,
  PenaltyScored,
  PenaltyMissed,
  MatchStart,
  PeriodEnd,
  Victory,
  Rout,
  Count
};
const uint8_t EV_COUNT = (uint8_t)Ev::Count;

// Carpeta de la SD (/sonidos/<carpeta>/) y nombre del momento en la app (mismo id que SoundEventId).
struct EvInfo {
  const char* folder;
  const char* webId;
};
inline const EvInfo& info(Ev e) {
  static const EvInfo T[EV_COUNT] = {
      {"gol", "goal"},
      {"gol-anulado", "goalAnnulled"},
      {"3-seguidos", "streak3"},
      {"remontada", "comeback"},
      {"empate", "equalizer"},
      {"bola-de-partido", "matchPoint"},
      {"gol-de-oro", "goldenGoal"},
      {"penalti-marcado", "penaltyScored"},
      {"penalti-fallado", "penaltyMissed"},
      {"empieza", "matchStart"},
      {"fin-de-parte", "periodEnd"},
      {"victoria", "victory"},
      {"goleada", "rout"},
  };
  return T[(uint8_t)e < EV_COUNT ? (uint8_t)e : 0];
}

// ---------------------------------------------------------------- momentos
// Se llama tras cada cambio del motor (afterChange del sketch) y devuelve los momentos en orden: el primero
// suena ya y los siguientes esperan a que termine el anterior, igual que soundsForCommit() de la app.
class Tracker {
 public:
  static const uint8_t MAX_OUT = 4;

  // Al empezar un partido.
  void reset(const mfv3::Engine& e) {
    count_ = 0;
    prev_ = take(e);
  }

  uint8_t observe(const mfv3::Engine& e, bool goalAccepted, mfv3::Team goalTeam, Ev* out) {
    State now = take(e);
    uint8_t n = 0;
    bool end = now.phase == mfv3::Phase::Finished && prev_.phase != mfv3::Phase::Finished;
    bool samePeriod = now.period == prev_.period;
    bool down = samePeriod && (now.w < prev_.w || now.b < prev_.b);
    bool up = now.w > prev_.w || now.b > prev_.b;

    if (down) {
      push(out, n, Ev::GoalAnnulled);
      while (count_ > 0 && (hist_[count_ - 1].w > now.w || hist_[count_ - 1].b > now.b)) count_--;
    }
    if (prev_.phase == mfv3::Phase::Countdown && now.phase == mfv3::Phase::Playing && now.period == mfv3::Period::First)
      push(out, n, Ev::MatchStart);
    if (now.phase == mfv3::Phase::PeriodEnd && prev_.phase != mfv3::Phase::PeriodEnd) push(out, n, Ev::PeriodEnd);
    if (now.kicks > prev_.kicks && e.kickCount() > 0)
      push(out, n, e.kick(e.kickCount() - 1).scored ? Ev::PenaltyScored : Ev::PenaltyMissed);

    if (up) {
      // Un gol (o un gol corregido que DESHACER devuelve): queda en el historial para remontadas y rachas.
      mfv3::Team t = now.w > prev_.w ? mfv3::Team::White : mfv3::Team::Blue;
      if (goalAccepted) t = goalTeam;
      bool comeback = goalAccepted && isComeback(t, now);
      if (count_ < MAX_HIST) hist_[count_++] = Entry{t, now.w, now.b};
      if (goalAccepted) {
        if (!end) push(out, n, comeback ? Ev::Comeback : streak(t) == 3 ? Ev::Streak3 : (now.w == now.b && now.w > 0) ? Ev::Equalizer : Ev::Goal);
        else if (prev_.period == mfv3::Period::Overtime) push(out, n, Ev::GoldenGoal);
      }
    }
    if (end) push(out, n, isRout(e) ? Ev::Rout : Ev::Victory);
    uint8_t newMp = (uint8_t)(now.matchPoint & ~prev_.matchPoint);
    if (newMp && !end && now.period != mfv3::Period::Overtime) push(out, n, Ev::MatchPoint);
    prev_ = now;
    return n;
  }

  // Goleada: ganar dejando al rival a 0 o por 4 o más (marcador ordinario; los penaltis no cuentan).
  static bool isRout(const mfv3::Engine& e) {
    mfv3::Score s = e.score();
    uint8_t w = s.of(e.winner()), l = s.of(mfv3::other(e.winner()));
    return w > l && (l == 0 || w - l >= 4);
  }

 private:
  struct State {
    mfv3::Phase phase;
    mfv3::Period period;
    uint8_t w, b, kicks, matchPoint;
  };
  struct Entry {
    mfv3::Team team;
    uint8_t w, b;
  };
  static const uint8_t MAX_HIST = 120;
  Entry hist_[MAX_HIST];
  uint8_t count_ = 0;
  State prev_ = {mfv3::Phase::Countdown, mfv3::Period::First, 0, 0, 0, 0};

  static State take(const mfv3::Engine& e) {
    mfv3::Score s = e.score();
    return State{e.phase(), e.period(), s.white, s.blue, e.kickCount(), e.matchPoint()};
  }
  static void push(Ev* out, uint8_t& n, Ev e) {
    if (n < MAX_OUT) out[n++] = e;
  }
  static uint8_t of(const Entry& x, mfv3::Team t) { return t == mfv3::Team::White ? x.w : x.b; }
  static uint8_t ofState(const State& x, mfv3::Team t) { return t == mfv3::Team::White ? x.w : x.b; }

  // Remontada: este gol pone por delante a un equipo que llegó a ir perdiendo por 2 o más.
  bool isComeback(mfv3::Team t, const State& now) const {
    mfv3::Team o = mfv3::other(t);
    uint8_t mine = ofState(now, t), theirs = ofState(now, o);
    if (mine <= theirs || mine - 1 > theirs) return false;  // antes del gol no iba por delante; ahora sí
    int worst = 0;
    for (uint8_t i = 0; i < count_; i++) {
      int d = (int)of(hist_[i], o) - (int)of(hist_[i], t);
      if (d > worst) worst = d;
    }
    return worst >= 2;
  }

  // Goles seguidos del mismo equipo al final del historial.
  uint8_t streak(mfv3::Team t) const {
    uint8_t n = 0;
    for (int i = (int)count_ - 1; i >= 0 && hist_[i].team == t; i--) n++;
    return n;
  }
};

// ---------------------------------------------------------------- WAV
struct Wav {
  uint32_t rate = 0;
  uint16_t channels = 0, bits = 0;
  uint32_t dataOffset = 0, dataSize = 0;
};

inline uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
inline uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Lee la cabecera (los primeros bytes del archivo). Solo PCM de 8/16 bits, 1–2 canales, 8–48 kHz.
inline bool parseWav(const uint8_t* buf, uint32_t len, uint32_t fileSize, Wav& out) {
  if (len < 12 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0) return false;
  bool fmt = false;
  for (uint32_t p = 12; p + 8 <= len;) {
    uint32_t size = le32(buf + p + 4);
    if (memcmp(buf + p, "fmt ", 4) == 0 && p + 8 + 16 <= len) {
      if (le16(buf + p + 8) != 1) return false;  // solo PCM
      out.channels = le16(buf + p + 10);
      out.rate = le32(buf + p + 12);
      out.bits = le16(buf + p + 22);
      fmt = true;
    } else if (memcmp(buf + p, "data", 4) == 0) {
      out.dataOffset = p + 8;
      out.dataSize = size;
      if (fileSize && out.dataOffset + out.dataSize > fileSize) out.dataSize = fileSize - out.dataOffset;
      return fmt && (out.channels == 1 || out.channels == 2) && (out.bits == 8 || out.bits == 16) && out.rate >= 8000 &&
             out.rate <= 48000;
    }
    p += 8 + size + (size & 1);
  }
  return false;
}

// ---------------------------------------------------------------- sonidos de siempre (tonos de la app)
enum class Wave : uint8_t { Sine, Square, Triangle, Saw };
struct Note {
  uint16_t freq;      // Hz (0 = silencio)
  uint16_t startMs;
  uint16_t durMs;
  Wave wave;
  uint8_t gain;       // 0–100
  uint16_t slideTo;   // Hz al final de la nota (0 = sin deslizar)
};
struct Pattern {
  const Note* notes;
  uint8_t count;
};

// Los mismos tonos que DEFAULT_SOUNDS / sound.ts de la app.
inline Pattern defaultPattern(Ev e) {
  static const Note GOAL[] = {{523, 0, 120, Wave::Square, 50, 0},
                              {659, 100, 120, Wave::Square, 50, 0},
                              {784, 200, 120, Wave::Square, 50, 0},
                              {1047, 300, 350, Wave::Saw, 40, 0}};
  static const Note ERROR[] = {{200, 0, 150, Wave::Square, 25, 140}};
  static const Note MATCH_POINT[] = {{880, 0, 120, Wave::Square, 35, 0}, {880, 180, 120, Wave::Square, 35, 0}};
  static const Note GO[] = {{1320, 0, 300, Wave::Sine, 50, 0}};
  static const Note PERIOD_END[] = {{880, 0, 250, Wave::Triangle, 60, 0}, {660, 300, 450, Wave::Triangle, 60, 0}};
  static const Note MATCH_END[] = {{523, 0, 180, Wave::Triangle, 60, 0},
                                   {659, 180, 180, Wave::Triangle, 60, 0},
                                   {784, 360, 180, Wave::Triangle, 60, 0},
                                   {1047, 540, 600, Wave::Triangle, 60, 0}};
#define S3SND_P(a) Pattern{a, (uint8_t)(sizeof(a) / sizeof(a[0]))}
  switch (e) {
    case Ev::GoalAnnulled:
    case Ev::PenaltyMissed: return S3SND_P(ERROR);
    case Ev::MatchPoint: return S3SND_P(MATCH_POINT);
    case Ev::MatchStart: return S3SND_P(GO);
    case Ev::PeriodEnd: return S3SND_P(PERIOD_END);
    case Ev::Victory:
    case Ev::Rout: return S3SND_P(MATCH_END);
    default: return S3SND_P(GOAL);
  }
#undef S3SND_P
}

inline uint32_t patternLengthMs(const Pattern& p) {
  uint32_t end = 0;
  for (uint8_t i = 0; i < p.count; i++)
    if ((uint32_t)p.notes[i].startMs + p.notes[i].durMs > end) end = (uint32_t)p.notes[i].startMs + p.notes[i].durMs;
  return end;
}

// Dibuja `count` muestras mono (16 bits) del patrón a partir de la muestra `from`. Envolvente corta al
// principio y al final de cada nota para que no chasquee. Las notas que coinciden se suman.
inline void renderPattern(const Pattern& p, uint32_t rate, uint32_t from, int16_t* out, uint32_t count) {
  const float TWO_PI_F = 6.2831853f;
  for (uint32_t i = 0; i < count; i++) {
    float t = (float)(from + i) / (float)rate;  // segundos
    float v = 0;
    for (uint8_t k = 0; k < p.count; k++) {
      const Note& n = p.notes[k];
      float st = n.startMs / 1000.0f, du = n.durMs / 1000.0f;
      if (!n.freq || t < st || t >= st + du) continue;
      float local = t - st;
      float f0 = n.freq, f1 = n.slideTo ? n.slideTo : n.freq;
      // Fase de un tono que se desliza linealmente de f0 a f1 durante la nota.
      float phase = f0 * local + (f1 - f0) * local * local / (2 * du);
      float frac = phase - floorf(phase);
      float s;
      switch (n.wave) {
        case Wave::Square: s = frac < 0.5f ? 1.0f : -1.0f; break;
        case Wave::Triangle: s = frac < 0.5f ? 4 * frac - 1 : 3 - 4 * frac; break;
        case Wave::Saw: s = 2 * frac - 1; break;
        default: s = sinf(TWO_PI_F * frac); break;
      }
      float env = 1;
      if (local < 0.005f) env = local / 0.005f;
      float tail = st + du - t;
      if (tail < 0.03f) env *= tail / 0.03f;
      v += s * env * (n.gain / 100.0f);
    }
    v *= 0.5f;  // margen para que dos notas sumadas no saturen
    if (v > 1) v = 1;
    if (v < -1) v = -1;
    out[i] = (int16_t)(v * 32000);
  }
}

}  // namespace s3snd

#endif  // MFV3_S3_SOUND_H
