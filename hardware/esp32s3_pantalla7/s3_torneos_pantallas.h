// MARCADOR FUTBOLÍN V3 · lo que las pantallas de torneo (ui_torneo*.h) piden además del contrato.
// -------------------------------------------------------------------------------------------------
// CONTRATO: propuesta del agente 3 para el coordinador. Son dos datos que la app enseña y que el contrato
// (s3_torneos.h) aún no trae; los implementa quien lleve la progresión (s3_progresion.h · agente 2). Mientras, los
// da el falso de las pruebas (hardware/test/torneos_fake.h). Se pueden pasar tal cual a s3_torneos.h al unir.

#ifndef MFV3_S3_TORNEOS_PANTALLAS_H
#define MFV3_S3_TORNEOS_PANTALLAS_H

#include "s3_torneos.h"

namespace torneos {

// Pronóstico del siguiente partido (predict() de progression.ts): % de victoria de cada lado. false sin datos
// (la tarjeta no enseña la barra).
bool predict(const char (*white)[40], uint8_t nw, const char (*blue)[40], uint8_t nb, uint8_t& wPct, uint8_t& bPct);

// Lo que se lleva un jugador al acabar el torneo (rewardOf() de TournamentCeremony.tsx): la XP de todos sus
// partidos del torneo, el nivel antes y después, el ELO y los logros que gana ahora. false si no llegó a jugar.
struct Reward {
  uint16_t xp;
  uint8_t levelBefore, levelAfter;
  uint8_t levelFill;            // barra de nivel al acabar, 0–100
  bool hasElo;
  int16_t eloDelta, eloAfter;
  uint8_t nAch;
  const char* ach[4];           // nombres de los logros (textos fijos de s3_progresion)
  uint8_t nLines;               // desglose de la XP por concepto
  char line[4][24];
  uint16_t lineXp[4];
};
bool reward(const Tournament& t, const char* playerId, Reward& out);

// Palmarés de una competición por páginas (decisión del dueño: en memoria, solo las 10 últimas ediciones de cada
// competición; las demás, en la SD sin límite). Lo implementa el guardado (agente 2); firmas provisionales hasta unir.
// editionCount: cuántas ediciones (no descartadas) tiene. editionsPage: `max` resúmenes desde la `from`-ésima,
// de la más nueva a la más antigua; devuelve cuántas ha puesto.
uint16_t editionCount(const char* key);
uint16_t editionsPage(const char* key, uint16_t from, Summary* out, uint16_t max);

}  // namespace torneos

#endif
