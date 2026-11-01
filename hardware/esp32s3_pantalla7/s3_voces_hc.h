// MARCADOR FUTBOLÍN V3 · voces de los hándicaps del Partido Loco (guion en docs/GUION_VOCES_HANDICAPS.md).
// ---------------------------------------------------------------------------------------------------
// Cada voz es un WAV suelto de la SD, fuera del catálogo de comentarios, con el código FNV-1a de
// «handicap/<voz>» (A1, Q2, F07b, R4c, N_alex…), el mismo que da voces-handicaps/subir-sd.py. Aquí solo se decide
// qué voces suenan y en qué orden: el aviso, «quién» + la frase al enseñar el hándicap y el final. Si falta la
// frase no suena nada (mejor callar que decir solo un nombre); si falta el nombre de un jugador, dice su equipo.

#ifndef MFV3_S3_VOCES_HC_H
#define MFV3_S3_VOCES_HC_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mfv3_engine.h"

namespace voceshc {

using mfv3::Handicap;
using mfv3::HKind;

inline uint32_t code(const char* voice) {
  uint32_t h = 2166136261u;
  for (const char* p = "handicap/"; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
  for (const char* p = voice; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
  return h;
}

// Voz del nombre: «Álvaro» → «N_alvaro» (minúsculas, sin tildes ni eñes, espacios como «_»).
inline void nameVoice(const char* name, char* out, size_t n) {
  size_t k = 0;
  if (n < 3) return;
  out[k++] = 'N';
  out[k++] = '_';
  while (*name == ' ') name++;
  for (const uint8_t* p = (const uint8_t*)name; *p && k + 1 < n; p++) {
    char c = 0;
    if (*p == 0xC3 && p[1]) {
      uint8_t b = (uint8_t)(*++p | 0x20);  // la minúscula de Á, É, Ñ… es su byte + 0x20
      c = b >= 0xA0 && b <= 0xA5 ? 'a' : b >= 0xA8 && b <= 0xAB ? 'e' : b >= 0xAC && b <= 0xAF ? 'i'
        : b >= 0xB2 && b <= 0xB6 ? 'o' : b >= 0xB9 && b <= 0xBC ? 'u' : b == 0xB1 ? 'n' : b == 0xA7 ? 'c' : 0;
    } else if (*p >= 'A' && *p <= 'Z') c = (char)(*p + 32);
    else if ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) c = (char)*p;
    else if (*p == ' ') c = '_';
    if (c) out[k++] = c;
  }
  while (k > 2 && out[k - 1] == '_') k--;
  out[k] = 0;
}

// Afectado por el hándicap: su nombre («» si se juega sin jugadores) y si es chico ('m') o chica ('f').
struct Who {
  char name[28];
  char gender;
};

typedef bool (*HasFn)(uint32_t code);

// Lo que suena seguido (la cola del reproductor admite cuatro).
struct Seq {
  uint32_t c[4];
  uint8_t n = 0;
  void add(uint32_t x) {
    if (x && n < 4) c[n++] = x;
  }
};

// Una al azar entre las que están en la SD (0 si ninguna).
inline uint32_t pick(const char* const* voices, uint8_t count, HasFn has, uint32_t rnd) {
  uint32_t ok[4];
  uint8_t k = 0;
  for (uint8_t i = 0; i < count && k < 4; i++)
    if (has(code(voices[i]))) ok[k++] = code(voices[i]);
  return k ? ok[rnd % k] : 0;
}

inline uint32_t aviso(HasFn has, uint32_t rnd) {
  static const char* const A[] = {"A1", "A2", "A3"};
  return pick(A, 3, has, rnd);
}

// Cuenta atrás: «¡Tres!», «¡Dos!», «¡Uno!» (C3, C2, C1; voz de Windows, la pidió el dueño). 0 si no está.
inline uint32_t count(uint8_t n, HasFn has) {
  char v[4] = {'C', (char)('0' + n), 0, 0};
  return n >= 1 && n <= 3 && has(code(v)) ? code(v) : 0;
}

// «Todo vuelve a la normalidad» (o el del cambio de posiciones o del traspaso). 0 si no lleva final: los que se
// gastan (gol triple, robo, penalti) se acaban con su gol.
inline uint32_t ending(const Handicap& h, HasFn has, uint32_t rnd) {
  static const char* const R1[] = {"R1a", "R1b", "R1c", "R1d"};
  static const char* const R4[] = {"R4a", "R4b", "R4c", "R4d"};
  static const char* const R5[] = {"R5a", "R5b", "R5c", "R5d"};
  if (h.kind == HKind::SwapPositions) return pick(R4, 4, has, rnd);
  if (h.kind == HKind::Transfer) return pick(R5, 4, has, rnd);
  return h.durationMs ? pick(R1, 4, has, rnd) : 0;
}

inline uint32_t teamVoice(uint8_t team, HasFn has) {
  uint32_t c = code(team == 0 ? "Q1" : team == 1 ? "Q2" : "Q3");
  return has(c) ? c : 0;
}

// Su nombre o, si no está grabado, su equipo.
inline uint32_t personVoice(const Who& w, uint8_t team, HasFn has) {
  char v[40];
  if (w.name[0]) {
    nameVoice(w.name, v, sizeof(v));
    if (has(code(v))) return code(v);
  }
  return teamVoice(team, has);
}

// Al enseñar el hándicap: quién + la frase («¡Vicky!» + «y» + «¡Alex!» + «¡Congelados! 15 segundos sin moveros.»).
inline Seq reveal(const Handicap& h, const Who& white, const Who& blue, HasFn has) {
  Seq s;
  uint8_t team = h.team > 2 ? 2 : h.team;
  bool both = h.playerW >= 0 && h.playerB >= 0;
  const char* phrase = nullptr;
  char bar[8];
  switch (h.kind) {
    case HKind::DoubleAll: phrase = "F01"; break;
    case HKind::DoubleTeam: phrase = "F02"; break;
    case HKind::TripleNext: phrase = "F03"; break;
    case HKind::Steal: phrase = "F04"; break;
    case HKind::FreezeScore: phrase = "F05"; break;
    case HKind::SwapPositions: phrase = "F06"; break;
    case HKind::FrozenPlayer: {
      bool girl = both ? white.gender == 'f' && blue.gender == 'f'
                       : (h.playerW >= 0 ? white.gender : blue.gender) == 'f';
      phrase = both ? (girl ? "F07d" : "F07c") : (girl ? "F07b" : "F07a");
      break;
    }
    case HKind::BarLock:
      snprintf(bar, sizeof(bar), "F08%c", (char)((both ? 'e' : 'a') + ((int)h.bar & 3)));
      phrase = bar;
      break;
    case HKind::Penalty: phrase = team == 1 ? "F09b" : "F09a"; break;
    case HKind::NoSpin: phrase = "F11"; break;
    case HKind::LongShots: phrase = "F12"; break;
    case HKind::OneHand: phrase = "F13"; break;
    case HKind::Transfer: {  // «¡Vicky!» «¡Te vas al equipo azul!» «¡Alex!» «¡Y tú, al blanco! Durante 30 segundos.»
      uint32_t a = code("F10a"), b = code("F10b");
      if (!has(a) || !has(b)) return s;
      s.add(personVoice(white, 0, has));
      s.add(a);
      s.add(personVoice(blue, 1, has));
      s.add(b);
      return s;
    }
    default: return s;  // «Mano mala» ya no sale
  }
  uint32_t p = code(phrase);
  if (!has(p)) return s;
  switch (h.kind) {
    case HKind::DoubleTeam:
    case HKind::Steal:
    case HKind::SwapPositions:
      s.add(teamVoice(team, has));
      break;
    case HKind::FrozenPlayer:
    case HKind::BarLock:
      if (both) {
        s.add(personVoice(white, 0, has));
        if (has(code("Q4"))) s.add(code("Q4"));
        s.add(personVoice(blue, 1, has));
      } else {
        s.add(h.playerW >= 0 ? personVoice(white, 0, has) : personVoice(blue, 1, has));
      }
      break;
    default:
      break;
  }
  s.add(p);
  return s;
}

}  // namespace voceshc

#endif  // MFV3_S3_VOCES_HC_H
