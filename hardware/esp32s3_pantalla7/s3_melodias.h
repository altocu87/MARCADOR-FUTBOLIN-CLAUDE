// MARCADOR FUTBOLÍN V3 · melodías de victoria de cada jugador (ANTHEMS de src/services/sound/sound.ts).
// -------------------------------------------------------------------------------------------------
// Se eligen en la ficha → OPCIONES → TÍTULO Y MELODÍA y se guardan en la cuenta (`anthem`). La placa las toca con su
// generador de notas (onda cuadrada) para oírlas al elegir. Cada nota: frecuencia y tiempos × paso de la melodía,
// en ms (melody() de la app). tests/melodiasPlaca.test.ts comprueba que son las mismas que las de la app.

#ifndef MFV3_S3_MELODIAS_H
#define MFV3_S3_MELODIAS_H

#include <string.h>
#include "mfv3_sound.h"

namespace melodias {

static const mfv3snd::Note FANFARE[] = {{523, 160}, {523, 160}, {523, 160}, {698, 480}, {880, 160}, {784, 160}, {1047, 640}};
static const mfv3snd::Note HEROIC[] = {{392, 300}, {523, 150}, {587, 150}, {659, 300}, {587, 150}, {659, 150}, {784, 600}};
static const mfv3snd::Note FUNKY[] = {{330, 130}, {0, 130}, {392, 130}, {440, 130}, {0, 130},
                                      {494, 130}, {587, 260}, {494, 130}, {587, 390}};
static const mfv3snd::Note CHAMPION[] = {{523, 320}, {659, 320}, {784, 320}, {1047, 480}, {988, 160}, {1047, 640}};
static const mfv3snd::Note ARCADE[] = {{659, 100}, {784, 100}, {988, 100}, {1319, 100}, {988, 100}, {1319, 300}};

struct Anthem {
  const char* id;
  const char* label;
  mfv3snd::Tune tune;
};
#define MFV3_N(a) (uint8_t)(sizeof(a) / sizeof(a[0]))
static const Anthem LIST[] = {
    {"fanfare", "Fanfarria", {FANFARE, MFV3_N(FANFARE)}}, {"heroic", "Épica", {HEROIC, MFV3_N(HEROIC)}},
    {"funky", "Funky", {FUNKY, MFV3_N(FUNKY)}},           {"champion", "Campeón", {CHAMPION, MFV3_N(CHAMPION)}},
    {"arcade", "Arcade", {ARCADE, MFV3_N(ARCADE)}},
};
#undef MFV3_N
static const int COUNT = sizeof(LIST) / sizeof(LIST[0]);

// La del jugador («» o desconocida = la Fanfarria, como la app).
static int indexOf(const char* id) {
  for (int i = 0; id && i < COUNT; i++)
    if (!strcmp(LIST[i].id, id)) return i;
  return 0;
}

}  // namespace melodias

#endif  // MFV3_S3_MELODIAS_H
