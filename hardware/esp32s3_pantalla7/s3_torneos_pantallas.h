// MARCADOR FUTBOLÍN V3 · lo que las pantallas de torneo (ui_torneo*.h) piden además del contrato.
// -------------------------------------------------------------------------------------------------
// CONTRATO: propuesta del agente 3 para el coordinador. Dos datos que la app enseña y que el contrato (s3_torneos.h)
// no trae, sacados de la progresión que ya calcula la placa (s3_progresion.h). Se pueden pasar a s3_torneos.h al
// unir, o a s3_progresion.h si el agente 2 prefiere llevarlos.
//   - predict(): el pronóstico del siguiente partido. Como predict() de la app, solo con datos de clasificatorios;
//     aquí con el ELO medio de cada lado (la app mezcla además enfrentamientos y forma, que la placa aún no tiene).
//   - reward(): lo que se lleva cada jugador al acabar el torneo (rewardOf() de TournamentCeremony.tsx): la XP, el
//     nivel, el ELO y los logros de sus partidos del torneo (progresion::all.entries). La placa no guarda el desglose
//     de la XP por concepto: `nLines` queda a 0.

#ifndef MFV3_S3_TORNEOS_PANTALLAS_H
#define MFV3_S3_TORNEOS_PANTALLAS_H

#include <math.h>
#include "s3_torneos.h"
#include "s3_progresion.h"

namespace torneos {

// % de victoria de cada lado. false sin datos (nadie ha jugado clasificatorios): la tarjeta no enseña la barra.
inline bool predict(const char (*white)[40], uint8_t nw, const char (*blue)[40], uint8_t nb, uint8_t& wPct, uint8_t& bPct) {
  if (!nw || !nb) return false;
  bool any = false;
  double sum[2] = {0, 0};
  for (int s = 0; s < 2; s++) {
    uint8_t n = s ? nb : nw;
    const char (*ids)[40] = s ? blue : white;
    for (uint8_t i = 0; i < n; i++) {
      const progresion::Player* p = progresion::of(partidos::codeOf(ids[i]));
      if (p && p->rankedPlayed > 0) any = true;
      sum[s] += p ? p->elo : progresion::rules.eloInitial;
    }
    sum[s] /= n;
  }
  if (!any) return false;
  double pw = 1.0 / (1.0 + pow(10.0, (sum[1] - sum[0]) / 400.0));
  int w = (int)floor(pw * 100 + 0.5);
  wPct = (uint8_t)(w < 1 ? 1 : w > 99 ? 99 : w);
  bPct = (uint8_t)(100 - wPct);
  return true;
}

struct Reward {
  uint16_t xp;
  uint8_t levelBefore, levelAfter;
  uint8_t levelFill;            // barra de nivel al acabar, 0–100
  bool hasElo;
  int16_t eloDelta, eloAfter;
  uint8_t nAch;
  const char* ach[4];           // nombres de los logros (textos fijos de s3_progresion)
  uint8_t nLines;               // desglose de la XP por concepto (la placa no lo guarda: 0)
  char line[4][24];
  uint16_t lineXp[4];
};

// false si no llegó a jugar.
inline bool reward(const Tournament& t, const char* playerId, Reward& out) {
  memset(&out, 0, sizeof(out));
  const progresion::Snapshot& s = progresion::all;
  uint32_t code = partidos::codeOf(playerId);
  uint32_t first = 0, last = 0;
  bool any = false;
  for (uint16_t k = 0; k < t.nPlayed && t.played; k++) {
    int ri = -1;
    for (int i = 0; i < partidos::count; i++)
      if (!strcmp(partidos::rows[i].id, t.played[k].r.matchId)) ri = i;
    if (ri < 0 || !s.entries || ri >= s.rowsCap) continue;
    const partidos::Row& m = partidos::rows[ri];
    for (int tm = 0; tm < 2; tm++)
      for (int i = 0; i < m.n[tm]; i++) {
        if (m.code[tm][i] != code) continue;
        const progresion::Entry& e = s.entries[(size_t)ri * 8 + tm * 4 + i];
        if (!any) out.levelBefore = e.levelBefore, first = m.fin;
        any = true;
        last = m.fin;
        out.xp = (uint16_t)(out.xp + e.xpGained);
        out.levelAfter = e.levelAfter;
        if (e.eloDelta != 0x7FFF) {
          out.hasElo = true;
          out.eloDelta = (int16_t)(out.eloDelta + e.eloDelta);
          out.eloAfter = e.eloAfter;
        }
      }
  }
  if (!any) return false;
  const progresion::Player* p = progresion::of(code);
  if (p) {
    out.levelFill = (uint8_t)lround(progresion::levelProgress(p->xp) * 100);
    // Logros conseguidos en los partidos del torneo (por su hora).
    for (int a = 0; a < progresion::ACH_COUNT && out.nAch < 4; a++)
      if (p->ach[a >> 3] & (1 << (a & 7)) && p->achAt[a] && p->achAt[a] >= first && p->achAt[a] <= last)
        out.ach[out.nAch++] = progresion::ACH[a].name;
  }
  return true;
}

}  // namespace torneos

#endif
