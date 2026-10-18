// MARCADOR FUTBOLÍN V3 · textos del Partido Loco en la placa (los mismos de src/match-engine/handicaps.ts).
// ---------------------------------------------------------------------------------------------------
// Título (cartel), nombre corto (botón del partido), explicación con nombres y duración de cada hándicap; el aviso
// al terminar y los de las visitas de los animales. `name(team, idx)` da el nombre del jugador elegido (o
// «un jugador del Blanco» si se juega sin jugadores).

#ifndef MFV3_S3_LOCO_H
#define MFV3_S3_LOCO_H

#include <stdio.h>
#include <string.h>
#include "mfv3_engine.h"

namespace loco {

using mfv3::Handicap;
using mfv3::HKind;

typedef void (*NameFn)(uint8_t team, int8_t idx, char* out, size_t n);

static const char* const TEAM_UP[2] = {"BLANCO", "AZUL"};
static const char* const TEAM_LOW[2] = {"blanco", "azul"};
static const char* const BAR_UP[4] = {"PORTERO", "DEFENSA", "MEDIO", "DELANTERO"};
static const char* const BAR_TEXT[4] = {"la barra del portero", "la barra de la defensa", "la barra del medio campo",
                                        "la barra de la delantera"};

struct Texts {
  char title[48];
  char shortT[24];
  char detail[200];
  char duration[32];
};

// Nombres de los afectados: «Alex» o «Alex y José».
static void players(const Handicap& h, NameFn name, char* out, size_t n, bool* both) {
  char a[32] = "", b[32] = "";
  if (h.playerW >= 0) name(0, h.playerW, a, sizeof(a));
  if (h.playerB >= 0) name(1, h.playerB, b, sizeof(b));
  *both = a[0] && b[0];
  if (*both) snprintf(out, n, "%s y %s", a, b);
  else snprintf(out, n, "%s", a[0] ? a : b);
}

static void texts(const Handicap& h, NameFn name, Texts& t) {
  memset(&t, 0, sizeof(t));
  unsigned secs = (unsigned)(h.durationMs / 1000);
  uint8_t team = h.team > 1 ? 0 : h.team;
  char who[72];
  bool both = false;
  switch (h.kind) {
    case HKind::DoubleAll:
      snprintf(t.title, sizeof(t.title), "¡GOLES DOBLES!");
      snprintf(t.shortT, sizeof(t.shortT), "GOLES x2");
      snprintf(t.detail, sizeof(t.detail), "Todos los goles valen doble durante %u segundos.", secs);
      break;
    case HKind::DoubleTeam:
      snprintf(t.title, sizeof(t.title), "GOLES DOBLES PARA %s", TEAM_UP[team]);
      snprintf(t.shortT, sizeof(t.shortT), "x2 %s", TEAM_UP[team]);
      snprintf(t.detail, sizeof(t.detail), "Los goles del equipo %s valen doble durante %u segundos.", TEAM_LOW[team], secs);
      break;
    case HKind::TripleNext:
      snprintf(t.title, sizeof(t.title), "¡GOL TRIPLE!");
      snprintf(t.shortT, sizeof(t.shortT), "GOL x3");
      snprintf(t.detail, sizeof(t.detail), "El próximo gol, lo marque quien lo marque, vale 3.");
      break;
    case HKind::Steal:
      snprintf(t.title, sizeof(t.title), "¡ROBO!");
      snprintf(t.shortT, sizeof(t.shortT), "ROBO %s", TEAM_UP[team]);
      snprintf(t.detail, sizeof(t.detail), "El próximo gol del equipo %s además le quita un gol al equipo %s.", TEAM_LOW[team],
               TEAM_LOW[1 - team]);
      break;
    case HKind::FreezeScore:
      snprintf(t.title, sizeof(t.title), "MARCADOR CONGELADO");
      snprintf(t.shortT, sizeof(t.shortT), "CONGELADO");
      snprintf(t.detail, sizeof(t.detail), "Durante %u segundos los goles no cuentan.", secs);
      break;
    case HKind::SwapPositions:
      snprintf(t.title, sizeof(t.title), "CAMBIO DE POSICIONES");
      snprintf(t.shortT, sizeof(t.shortT), "CAMBIO DE SITIO");
      if (h.team == 2)
        snprintf(t.detail, sizeof(t.detail), "En los dos equipos: el de portero y defensa pasa al ataque y el de ataque pasa atrás.");
      else
        snprintf(t.detail, sizeof(t.detail), "Equipo %s: el de portero y defensa pasa al ataque y el de ataque pasa atrás.",
                 TEAM_LOW[team]);
      break;
    case HKind::FrozenPlayer:
      players(h, name, who, sizeof(who), &both);
      snprintf(t.title, sizeof(t.title), both ? "JUGADORES CONGELADOS" : "JUGADOR CONGELADO");
      snprintf(t.shortT, sizeof(t.shortT), "CONGELADO");
      snprintf(t.detail, sizeof(t.detail), "%s no se %s mover durante %u segundos.", who, both ? "pueden" : "puede", secs);
      break;
    case HKind::BarLock:
      players(h, name, who, sizeof(who), &both);
      snprintf(t.title, sizeof(t.title), "BARRA BLOQUEADA");
      snprintf(t.shortT, sizeof(t.shortT), "SIN %s", BAR_UP[(int)h.bar & 3]);
      snprintf(t.detail, sizeof(t.detail), "%s no %s mover %s durante %u segundos.", who, both ? "pueden" : "puede",
               BAR_TEXT[(int)h.bar & 3], secs);
      break;
    case HKind::Penalty:
      snprintf(t.title, sizeof(t.title), "¡PENALTI PARA %s!", TEAM_UP[team]);
      snprintf(t.shortT, sizeof(t.shortT), "PENALTI");
      snprintf(t.detail, sizeof(t.detail), "Penalti a favor del equipo %s. Un solo lanzamiento.", TEAM_LOW[team]);
      break;
    case HKind::Transfer: {
      char a[32] = "", b[32] = "";
      name(0, h.playerW, a, sizeof(a));
      name(1, h.playerB, b, sizeof(b));
      snprintf(t.title, sizeof(t.title), "¡TRASPASO!");
      snprintf(t.shortT, sizeof(t.shortT), "TRASPASO");
      snprintf(t.detail, sizeof(t.detail), "%s se va al equipo azul y %s al blanco durante %u segundos. El gol es para el equipo donde se juega.",
               a, b, secs);
      break;
    }
    case HKind::WeakHand:
      snprintf(t.title, sizeof(t.title), "MANO MALA");
      snprintf(t.shortT, sizeof(t.shortT), "MANO MALA");
      snprintf(t.detail, sizeof(t.detail), "Todos juegan con la mano que no usan normalmente durante %u segundos.", secs);
      break;
    case HKind::NoSpin:
      snprintf(t.title, sizeof(t.title), "PROHIBIDO EL MOLINILLO");
      snprintf(t.shortT, sizeof(t.shortT), "SIN MOLINILLO");
      snprintf(t.detail, sizeof(t.detail), "Durante %u segundos, si alguien gira la barra entera, el gol no vale: anuladlo.", secs);
      break;
    case HKind::LongShots:
      snprintf(t.title, sizeof(t.title), "SOLO DESDE LEJOS");
      snprintf(t.shortT, sizeof(t.shortT), "DESDE LEJOS");
      snprintf(t.detail, sizeof(t.detail),
               "Durante %u segundos solo valen los goles marcados desde la defensa o el medio campo: anulad los demás.", secs);
      break;
    default:  // OneHand
      snprintf(t.title, sizeof(t.title), "UNA SOLA MANO");
      snprintf(t.shortT, sizeof(t.shortT), "UNA MANO");
      snprintf(t.detail, sizeof(t.detail), "Cada jugador juega con una sola mano durante %u segundos.", secs);
      break;
  }
  if (h.kind == HKind::Penalty) snprintf(t.duration, sizeof(t.duration), "Un lanzamiento");
  else if (!h.durationMs)
    snprintf(t.duration, sizeof(t.duration), h.kind == HKind::SwapPositions ? "Hasta el siguiente hándicap" : "Hasta que se use");
  else snprintf(t.duration, sizeof(t.duration), "Dura %u s", secs);
}

// Aviso al terminar («Fin del cambio de posiciones: cada uno a su sitio.», «Se acabó: goles dobles.»…).
static void endText(const Handicap& h, NameFn name, char* out, size_t n) {
  if (h.kind == HKind::SwapPositions) {
    snprintf(out, n, "Fin del cambio de posiciones: cada uno a su sitio.");
    return;
  }
  if (h.kind == HKind::Transfer) {
    snprintf(out, n, "Fin del traspaso: cada uno a su equipo.");
    return;
  }
  Texts t;
  texts(h, name, t);
  // Sin ¡ ! y en minúsculas salvo la primera (en UTF-8: las tildes en mayúscula llevan 0xC3 + 0x80–0x9E).
  char c[48];
  size_t k = 0;
  for (const char* q = t.title; *q && k < sizeof(c) - 2; q++) {
    if ((uint8_t)q[0] == 0xC2 && (uint8_t)q[1] == 0xA1) {  // ¡
      q++;
      continue;
    }
    if (*q == '!') continue;
    c[k++] = *q;
  }
  c[k] = 0;
  for (size_t i = 1; c[i]; i++) {
    if (c[i] >= 'A' && c[i] <= 'Z') c[i] = (char)(c[i] + 32);
    else if ((uint8_t)c[i] == 0xC3 && (uint8_t)c[i + 1] >= 0x80 && (uint8_t)c[i + 1] <= 0x9E) c[i + 1] = (char)(c[i + 1] + 0x20);
  }
  snprintf(out, n, "Se acabó: %s.", c);
}

// Visitas: título y la línea de debajo (corta, para la franja).
static void visitTexts(const mfv3::Visitor& v, char* title, size_t tn, char* sub, size_t sn) {
  if (v.animal == mfv3::Animal::Squirrel) {
    snprintf(title, tn, "¡ARDILLA!");
    snprintf(sub, sn, "LE ROBA UN GOL AL %s", TEAM_UP[v.from == 1 ? 1 : 0]);
  } else if (v.animal == mfv3::Animal::Snail) {
    snprintf(title, tn, "¡CARACOL!");
    snprintf(sub, sn, v.time ? "UN MINUTO MÁS DE PARTIDO" : "AHORA HACE FALTA UN GOL MÁS");
  } else {
    snprintf(title, tn, "¡GATO!");
    snprintf(sub, sn, "VOMITA Y DA UN GOL AL %s", TEAM_UP[v.to == 1 ? 1 : 0]);
  }
}

}  // namespace loco

#endif  // MFV3_S3_LOCO_H
