// MARCADOR FUTBOLÍN V3 · pronóstico antes de un clasificatorio (predict() y lastMeetings() de la app,
// src/services/progression/prediction.ts y src/services/statistics/statistics.ts).
// ---------------------------------------------------------------------------------------------------
// Pesos de la app: ELO 60 %, enfrentamientos directos 25 % (por n/5 si hay menos de 5) y forma 15 %, reajustados
// para sumar 100 %. Enfrentamientos directos: clasificatorios con las mismas alineaciones (en cualquier lado).
// Forma: los 5 últimos clasificatorios de cada jugador del equipo, juntos. Sin clasificatorios de nadie, no hay
// pronóstico (se puede jugar igual). Usa la progresión ya calculada (progresion::all) y la lista de partidos.

#ifndef MFV3_S3_PREVISION_H
#define MFV3_S3_PREVISION_H

#include <math.h>
#include <string.h>
#include "s3_partidos.h"
#include "s3_progresion.h"

namespace prevision {

struct Result {
  bool available = false;
  uint8_t whitePct = 50, bluePct = 50;
  int direct = 0;                      // enfrentamientos directos (clasificatorios)
  uint8_t wElo = 0, wH2h = 0, wForm = 0;  // pesos en %
};

struct Meeting {
  uint64_t at;      // ms (0 sin fecha)
  uint8_t white, blue;  // goles desde el punto de vista de las alineaciones de hoy
  int8_t winner;    // 0 Blanco de hoy, 1 Azul de hoy, -1 empate
};

// ¿Las mismas personas (sin importar el orden)?
inline bool sameLineup(const uint32_t* a, uint8_t na, const uint32_t* b, uint8_t nb) {
  if (na != nb || !na) return false;
  bool used[4] = {false, false, false, false};
  for (uint8_t i = 0; i < na; i++) {
    bool found = false;
    for (uint8_t j = 0; j < nb && !found; j++)
      if (!used[j] && a[i] == b[j]) used[j] = found = true;
    if (!found) return false;
  }
  return true;
}

// Lado de hoy que jugó de Blanco en `r` (0), de Azul (1) o -1 si no son estas alineaciones.
inline int sideOf(const partidos::Row& r, const uint32_t* w, uint8_t nw, const uint32_t* b, uint8_t nb) {
  if (sameLineup(r.code[0], r.n[0], w, nw) && sameLineup(r.code[1], r.n[1], b, nb)) return 0;
  if (sameLineup(r.code[0], r.n[0], b, nb) && sameLineup(r.code[1], r.n[1], w, nw)) return 1;
  return -1;
}

inline double formScore(const char* letters, int n) {
  double s = 0;
  for (int i = 0; i < n; i++) s += letters[i] == 'G' ? 1 : letters[i] == 'E' ? 0.5 : 0;
  return s / n;
}

inline Result predict(const uint32_t* white, uint8_t nw, const uint32_t* blue, uint8_t nb) {
  Result out;
  if (!nw || !nb) return out;
  bool any = false;
  double elo[2] = {0, 0};
  char form[2][24];
  int nf[2] = {0, 0};
  for (int s = 0; s < 2; s++) {
    const uint32_t* ids = s ? blue : white;
    uint8_t n = s ? nb : nw;
    for (uint8_t i = 0; i < n; i++) {
      const progresion::Player* p = progresion::of(ids[i]);
      if (p && p->rankedPlayed > 0) any = true;
      elo[s] += p ? p->elo : 1200;
      for (const char* c = p ? p->rform : ""; *c && nf[s] < 20; c++) form[s][nf[s]++] = *c;
    }
    elo[s] /= n;
  }
  int played = 0, whiteWins = 0;
  for (int i = 0; i < partidos::count; i++) {
    const partidos::Row& r = partidos::rows[i];
    if (r.mode != 'r') continue;
    int side = sideOf(r, white, nw, blue, nb);
    if (side < 0) continue;
    played++;
    if (r.winner == (side ? 'b' : 'w')) whiteWins++;
  }
  out.direct = played;
  if (!any) return out;
  double pElo = 1.0 / (1.0 + pow(10.0, (elo[1] - elo[0]) / 400.0));
  double wElo = 0.6, wH2h = 0.25 * (played < 5 ? played : 5) / 5.0;
  double pH2h = played ? (double)whiteWins / played : 0.5;
  double wForm = nf[0] && nf[1] ? 0.15 : 0;
  double fw = nf[0] ? formScore(form[0], nf[0]) : 0, fb = nf[1] ? formScore(form[1], nf[1]) : 0;
  double pForm = wForm > 0 && fw + fb > 0 ? fw / (fw + fb) : 0.5;
  double total = wElo + wH2h + wForm;
  wElo /= total, wH2h /= total, wForm /= total;
  double p = wElo * pElo + wH2h * pH2h + wForm * pForm;
  int w = (int)floor(p * 100 + 0.5);  // Math.round de la app
  out.available = true;
  out.whitePct = (uint8_t)w;
  out.bluePct = (uint8_t)(100 - w);
  out.wElo = (uint8_t)floor(wElo * 100 + 0.5);
  out.wH2h = (uint8_t)floor(wH2h * 100 + 0.5);
  out.wForm = (uint8_t)floor(wForm * 100 + 0.5);
  return out;
}

// Últimos `max` enfrentamientos de estas alineaciones (cualquier modalidad), del más nuevo al más viejo.
inline int lastMeetings(const uint32_t* white, uint8_t nw, const uint32_t* blue, uint8_t nb, Meeting* out, int max) {
  int n = 0;
  if (!nw || !nb) return 0;
  for (int i = 0; i < partidos::count; i++) {
    const partidos::Row& r = partidos::rows[i];
    int side = sideOf(r, white, nw, blue, nb);
    if (side < 0) continue;
    Meeting m;
    m.at = r.finishedAt ? r.finishedAt : (uint64_t)r.fin * 1000ull;
    m.white = side ? r.b : r.w;
    m.blue = side ? r.w : r.b;
    m.winner = r.winner == '-' ? -1 : (r.winner == 'w') == (side == 0) ? 0 : 1;
    // Insertar ordenado (más nuevo primero); se queda con los `max` más nuevos.
    int k = n < max ? n++ : max;
    if (k == max && (max == 0 || m.at <= out[max - 1].at)) continue;
    if (k == max) k = max - 1;
    while (k > 0 && out[k - 1].at < m.at) {
      out[k] = out[k - 1];
      k--;
    }
    out[k] = m;
  }
  return n;
}

}  // namespace prevision

#endif
