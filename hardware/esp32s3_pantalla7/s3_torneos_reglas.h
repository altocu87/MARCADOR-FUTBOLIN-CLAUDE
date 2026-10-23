// MARCADOR FUTBOLÍN V3 · reglas de los torneos en la pantalla de 7" (copia de la app).
// -------------------------------------------------------------------------------------------------
// Copia en C++ de src/services/tournaments/tournaments.ts, ladder.ts y templates.ts: la placa crea, apunta, deshace y
// clasifica igual que la app. Lo comprueba la prueba cruzada: tests/torneosPlaca.test.ts guarda lo que calcula la app
// (tests/fixtures/torneos) y hardware/test/torneos_test.cpp mira que la placa dé exactamente lo mismo.
//
// - Liga: todos contra todos por el método del círculo (lados alternos), a una vuelta o ida y vuelta (la vuelta, con
//   los lados cambiados). Empates solo en la fase regular. Sin final, gana el líder (y el premio va en su último
//   partido); con final, la juegan el 1º y el 2º.
// - Copa: cuadro de 4 u 8 con la siembra 1-8 / 4-5 / 2-7 / 3-6; los huecos dan pase directo; la final, a 1 o al mejor
//   de 3.
// - Pool: escalera de pistas (ver ladder.ts). Los sorteos usan seededRandom con la misma semilla que la app
//   («<id>-r<ronda>»), así que las rondas salen iguales en los dos lados y deshacer da las mismas rondas.
// Los índices de `teams` y `fix` siguen el orden de los arrays de la app (los mismos que el JSON).
// Memoria: lo grande en la PSRAM (calloc), nada de tablas globales; lo de paso, en reservas que se sueltan al acabar.

#ifndef MFV3_S3_TORNEOS_REGLAS_H
#define MFV3_S3_TORNEOS_REGLAS_H

#include <stdio.h>
#include "s3_torneos.h"

namespace torneos {

// Solo para las pruebas del PC: ids fijos («t_1», «tt_3»…) para comparar con la app. En la placa, nullptr.
static void (*makeId)(const char* prefix, char* out, size_t n) = nullptr;

namespace rg {

static void cpy(char* d, size_t n, const char* s) { snprintf(d, n, "%s", s ? s : ""); }
#define TORNEO_CPY(d, s) ::torneos::rg::cpy((d), sizeof(d), (s))

// «t_» + UUID v4, como newId de la app (src/services/ids.ts).
static void newId(const char* prefix, char* out, size_t n) {
  if (makeId) {
    makeId(prefix, out, n);
    return;
  }
  uint8_t b[16];
  for (int i = 0; i < 16; i += 4) {
#ifdef ARDUINO
    uint32_t r = esp_random();
#else
    uint32_t r = (uint32_t)rand() * 2654435761u ^ (uint32_t)rand();
#endif
    memcpy(b + i, &r, 4);
  }
  b[6] = (uint8_t)((b[6] & 0x0F) | 0x40);
  b[8] = (uint8_t)((b[8] & 0x3F) | 0x80);
  snprintf(out, n, "%s_%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", prefix, b[0], b[1], b[2], b[3],
           b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

// Siguiente letra de un texto UTF-8 (código Unicode).
static uint32_t nextCp(const char*& p) {
  uint8_t c = (uint8_t)*p++;
  if (c < 0x80) return c;
  int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
  uint32_t cp = c & (0x3F >> extra);
  for (; extra && ((uint8_t)*p & 0xC0) == 0x80; extra--) cp = (cp << 6) | ((uint8_t)*p++ & 0x3F);
  return cp;
}

// seededRandom de la app (statistics/calendar.ts): FNV-1a con Math.imul sobre charCodeAt(0) de cada letra y luego
// xorshift. Los desplazamientos de JS trabajan con 32 bits: con uint32_t salen los mismos bits.
struct Rng {
  uint32_t x = 0;
  explicit Rng(const char* seed) {
    uint32_t h = 2166136261u;
    for (const char* p = seed; *p;) {
      uint32_t cp = nextCp(p);
      uint32_t unit = cp < 0x10000 ? cp : 0xD800 + ((cp - 0x10000) >> 10);  // charCodeAt(0): la primera mitad
      h = (h ^ unit) * 16777619u;
    }
    x = h;
  }
  double next() {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (double)(x % 1000000u) / 1000000.0;
  }
};

// Barajar (shuffle de la app): de atrás adelante, cada uno con uno de los anteriores.
template <typename T>
static void shuffle(T* a, int n, Rng& rnd) {
  for (int i = n - 1; i > 0; i--) {
    int j = (int)(rnd.next() * (i + 1));
    T tmp = a[i];
    a[i] = a[j];
    a[j] = tmp;
  }
}

// ---------------------------------------------------------------- orden de nombres (localeCompare 'es' de la app)
// Primero las letras sin tildes ni mayúsculas (la ñ, letra propia tras la n); luego las tildes; luego minúscula antes
// que mayúscula. Espacios y signos antes que las cifras, y las cifras antes que las letras, como ICU.
static void weightOf(uint32_t cp, uint16_t& p, uint16_t& s, uint8_t& t) {
  static const char* const PUNCT = " _-,;:!?.'\"()[]{}@*/\\&#%`^+<=>|~$";
  static const char LATIN[] = "AAAAAAACEEEEIIIIDNOOOOO*OUUUUY*S";
  s = 0;
  t = 0;
  if (cp >= 0xC0 && cp <= 0xFF) {
    char base = LATIN[(cp & 0xDF) - 0xC0];
    if (cp == 0xFF) base = 'Y';
    if (base != '*') {
      t = cp < 0xE0 ? 1 : 0;
      if (base == 'N') {  // ñ
        p = 300 + ('n' - 'a') * 2 + 1;
        return;
      }
      p = 300 + (uint16_t)(base - 'A') * 2;
      s = (uint16_t)(cp & 0x1F) + 1;
      return;
    }
  }
  if (cp >= 'a' && cp <= 'z') { p = 300 + (uint16_t)(cp - 'a') * 2; return; }
  if (cp >= 'A' && cp <= 'Z') { p = 300 + (uint16_t)(cp - 'A') * 2; t = 1; return; }
  if (cp >= '0' && cp <= '9') { p = 200 + (uint16_t)(cp - '0'); return; }
  if (cp < 0x80) {
    const char* q = strchr(PUNCT, (int)cp);
    p = q && cp ? (uint16_t)(1 + (q - PUNCT)) : (uint16_t)(100 + cp);
    return;
  }
  p = (uint16_t)(cp > 0xF000 ? 0xF000 : 1000 + cp);
}

static int collate(const char* a, const char* b) {
  for (int level = 0; level < 3; level++) {
    const char* x = a;
    const char* y = b;
    while (true) {
      if (!*x || !*y) {
        if (*x || *y) return *x ? 1 : -1;
        break;
      }
      uint16_t px, sx, py, sy;
      uint8_t tx, ty;
      weightOf(nextCp(x), px, sx, tx);
      weightOf(nextCp(y), py, sy, ty);
      int d = level == 0 ? (int)px - (int)py : level == 1 ? (int)sx - (int)sy : (int)tx - (int)ty;
      if (d) return d;
    }
  }
  return strcmp(a, b);
}

// ---------------------------------------------------------------- textos
static void trimInto(char* out, size_t n, const char* s) {
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
  size_t len = strlen(s);
  while (len && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\n' || s[len - 1] == '\r')) len--;
  if (len >= n) len = n - 1;
  memcpy(out, s, len);
  out[len] = 0;
}
static bool blank(const char* s) {
  char tmp[2];
  trimInto(tmp, sizeof(tmp), s ? s : "");
  return !tmp[0];
}
// Mayúsculas (letras de ASCII y las de Latin-1 con tilde): «Jornada 3» → «JORNADA 3».
static void upper(char* s) {
  for (uint8_t* p = (uint8_t*)s; *p; p++) {
    if (*p >= 'a' && *p <= 'z') *p -= 32;
    else if (*p == 0xC3 && p[1] >= 0xA0 && p[1] <= 0xBE && p[1] != 0xB7) p[1] -= 0x20, p++;
  }
}
static void lower(char* s) {
  for (uint8_t* p = (uint8_t*)s; *p; p++) {
    if (*p >= 'A' && *p <= 'Z') *p += 32;
    else if (*p == 0xC3 && p[1] >= 0x80 && p[1] <= 0x9E && p[1] != 0x97) p[1] += 0x20, p++;
  }
}
static void append(char* out, size_t n, const char* s) {
  size_t l = strlen(out);
  if (l + 1 < n) snprintf(out + l, n - l, "%s", s);
}

// ---------------------------------------------------------------- equipos y cruces
static bool sameIds(const char (*a)[40], uint8_t na, const char (*b)[40], uint8_t nb) {  // como pairKey: sin orden
  if (na != nb) return false;
  for (uint8_t i = 0; i < na; i++) {
    bool in = false;
    for (uint8_t j = 0; j < nb; j++) in = in || !strcmp(a[i], b[j]);
    if (!in) return false;
  }
  return true;
}
static bool teamHas(const Team& tm, const char* id) {
  for (uint8_t i = 0; i < tm.n; i++)
    if (!strcmp(tm.player[i], id)) return true;
  return false;
}
// ¿Están todos los de `a` en `b`? (within de finalRanking)
static bool within(const Team& a, const Team& b) {
  for (uint8_t i = 0; i < a.n; i++)
    if (!teamHas(b, a.player[i])) return false;
  return true;
}
static uint16_t maxRound(const Tournament& t) {
  uint16_t m = 0;
  for (uint16_t i = 0; i < t.nFix; i++)
    if (t.fix[i].round > m) m = t.fix[i].round;
  return m;
}
static Fixture* addFixture(Tournament& t) {
  if (!t.fix || t.nFix >= MAX_FIXTURES) return nullptr;
  Fixture* f = &t.fix[t.nFix++];
  *f = Fixture();
  return f;
}
static int16_t addTeam(Tournament& t) {
  if (!t.teams || t.nTeams >= MAX_TEAMS_T) return -1;
  t.teams[t.nTeams] = Team();
  return (int16_t)t.nTeams++;
}
static const Result* resultOf(const Tournament& t, const char* matchId, const char* exclude = nullptr) {
  if (!matchId[0] || (exclude && !strcmp(matchId, exclude))) return nullptr;
  for (uint16_t i = 0; i < t.nPlayed; i++)
    if (!strcmp(t.played[i].r.matchId, matchId)) return &t.played[i].r;
  return nullptr;
}
static void advanceWinner(Tournament& t, Fixture& f, int16_t winner, bool bye = false) {
  f.winner = winner;
  if (bye) f.bye = true;
  if (f.next >= 0 && f.next < (int16_t)t.nFix) {
    if (f.nextSlot == 0) t.fix[f.next].white = winner;
    else t.fix[f.next].blue = winner;
  }
}
static void finish(Tournament& t, int16_t winner, const Result& prize) {
  t.status = Status::Finished;
  t.winner = winner;
  t.finishedAt = prize.finishedAt;
  TORNEO_CPY(t.finalMatchId, prize.matchId);
}
// Inserta en `played` en su sitio por hora de fin (tras los de la misma hora, como el orden estable de la app).
static void insertPlayed(Tournament& t, const char* fixtureId, const Result& r) {
  if (!t.played || t.nPlayed >= MAX_PLAYED) return;
  uint16_t at = t.nPlayed;
  while (at > 0 && t.played[at - 1].r.finishedAt > r.finishedAt) at--;
  memmove(&t.played[at + 1], &t.played[at], (t.nPlayed - at) * sizeof(Played));
  memset(&t.played[at], 0, sizeof(Played));
  TORNEO_CPY(t.played[at].fixture, fixtureId);
  t.played[at].r = r;
  t.nPlayed++;
}

// ---------------------------------------------------------------- Pool en escalera (ladder.ts)
typedef char Pair[2][40];
static void setPair(Pair& d, const char (*s)[40]) {
  TORNEO_CPY(d[0], s[0]);
  TORNEO_CPY(d[1], s[1]);
}
// ensurePairTeam: el equipo de esa pareja (lo crea con los nombres de cada uno si aún no existe).
static int16_t ensurePairTeam(Tournament& t, const char (*ids)[40]) {
  for (uint16_t i = 0; i < t.nTeams; i++)
    if (sameIds(t.teams[i].player, t.teams[i].n, ids, 2)) return (int16_t)i;
  int16_t k = addTeam(t);
  if (k < 0) return -1;
  Team& tm = t.teams[k];
  newId("tt", tm.id, sizeof(tm.id));
  tm.n = 2;
  tm.name[0] = 0;
  for (int j = 0; j < 2; j++) {
    TORNEO_CPY(tm.player[j], ids[j]);
    const char* name = "?";
    for (uint16_t i = 0; i < t.nTeams; i++)
      if (t.teams[i].n == 1 && !strcmp(t.teams[i].player[0], ids[j])) { name = t.teams[i].name; break; }
    if (j) append(tm.name, sizeof(tm.name), " + ");
    append(tm.name, sizeof(tm.name), name);
  }
  return k;
}
// ladderRoundFixtures: los partidos de una ronda, de la última pista a la 1 (con `final`, la 1 es la final).
static void ladderRoundFixtures(Tournament& t, uint16_t round, const Lineup& lu, bool final) {
  if (lu.bench) ensurePairTeam(t, lu.benchIds);
  for (int c = lu.nCourts; c >= 1; c--) {
    int16_t w = ensurePairTeam(t, lu.court[c - 1][0]);
    int16_t b = ensurePairTeam(t, lu.court[c - 1][1]);
    Fixture* f = addFixture(t);
    if (!f) return;
    snprintf(f->id, sizeof(f->id), "%s-r%u-p%d", t.id, (unsigned)round, c);
    f->round = round;
    f->court = (uint8_t)c;
    f->white = w;
    f->blue = b;
    if (lu.bench) {
      f->nResting = 2;
      setPair(f->resting, lu.benchIds);
    }
    if (final && c == 1) {
      f->isFinal = true;
      if (t.ladder.finalBestOf > 1) f->bestOf = t.ladder.finalBestOf;
    }
  }
}
static void startLineup(const Tournament& t, Lineup& lu) {
  memset(&lu, 0, sizeof(lu));
  const Ladder& s = t.ladder;
  lu.nCourts = s.nStart / 2;
  for (uint8_t i = 0; i < lu.nCourts; i++) {
    setPair(lu.court[i][0], s.start[i * 2]);
    setPair(lu.court[i][1], s.start[i * 2 + 1]);
  }
  lu.bench = s.nStart % 2 == 1;
  if (lu.bench) setPair(lu.benchIds, s.start[s.nStart - 1]);
}
static void firstLadderRound(Tournament& t) {
  Lineup lu;
  startLineup(t, lu);
  ladderRoundFixtures(t, 1, lu, t.ladder.byRounds && t.ladder.rounds <= 1);
}
// Cruces de una ronda ordenados por pista (pista 1 primero; orden estable).
static uint8_t roundByCourt(const Tournament& t, uint16_t round, uint16_t* out, uint8_t max) {
  uint8_t n = 0;
  for (uint16_t i = 0; i < t.nFix && n < max; i++)
    if (t.fix[i].round == round) {
      uint8_t k = n++;
      while (k > 0 && t.fix[out[k - 1]].court > t.fix[i].court) out[k] = out[k - 1], k--;
      out[k] = i;
    }
  return n;
}
static void idsOf(const Tournament& t, int16_t team, Pair& out) {
  memset(out, 0, sizeof(Pair));
  if (team < 0 || team >= (int16_t)t.nTeams) return;
  for (uint8_t i = 0; i < t.teams[team].n && i < 2; i++) TORNEO_CPY(out[i], t.teams[team].player[i]);
}
static int16_t loserOf(const Fixture& f) { return f.winner == f.white ? f.blue : f.white; }
// Las dos parejas que llegan a una pista se deshacen: cada uno va con uno de la otra, por sorteo.
static void mix(char (*court)[2][40], Rng& rnd) {
  Pair top, bottom;
  memcpy(top, court[0], sizeof(Pair));
  memcpy(bottom, court[1], sizeof(Pair));
  int x = rnd.next() < 0.5 ? 0 : 1;
  TORNEO_CPY(court[0][0], top[0]);
  TORNEO_CPY(court[0][1], bottom[x]);
  TORNEO_CPY(court[1][0], top[1]);
  TORNEO_CPY(court[1][1], bottom[1 - x]);
}
static void nextLineup(const Tournament& t, uint16_t round, Lineup& lu) {
  arrivalLineup(t, round, lu);
  if (!t.ladder.swap) return;
  char seed[80];
  snprintf(seed, sizeof(seed), "%s-r%u", t.id, (unsigned)(round + 1));
  Rng rnd(seed);
  for (uint8_t c = 0; c < lu.nCourts; c++) mix(lu.court[c], rnd);
}
// Pool por tiempo: lo que queda (al acabar el último) es menos que dos partidos de media. En enteros: queda·n < 2·jugado.
static bool finalTime(const Tournament& t) {
  if (!t.nPlayed) return false;
  int64_t start = (int64_t)t.played[0].r.startedAt;
  int64_t last = (int64_t)t.played[t.nPlayed - 1].r.finishedAt;
  int64_t total = (int64_t)t.ladder.minutes * 60000;
  int64_t remaining = total - (last - start);
  return remaining * (int64_t)t.nPlayed < 2 * (last - start);
}
// La siguiente pista 1 pasa a ser la final; lo que quede de esa ronda en las otras pistas ya no se juega.
static void callFinal(Tournament& t) {
  uint16_t round = 0xFFFF;
  for (uint16_t i = 0; i < t.nFix; i++)
    if (!fixtureDone(t.fix[i]) && !t.fix[i].skipped && t.fix[i].round < round) round = t.fix[i].round;
  if (round == 0xFFFF) return;
  for (uint16_t i = 0; i < t.nFix; i++) {
    Fixture& f = t.fix[i];
    if (f.round != round || fixtureDone(f) || f.skipped) continue;
    if (f.court == 1) {
      f.isFinal = true;
      if (t.ladder.finalBestOf > 1) f.bestOf = t.ladder.finalBestOf;
    } else f.skipped = true;
  }
}
static void ladderAfterResult(Tournament& t, uint16_t round) {
  bool all = true;
  for (uint16_t i = 0; i < t.nFix; i++)
    if (t.fix[i].round == round && !t.fix[i].skipped && !fixtureDone(t.fix[i])) all = false;
  if (all) {
    bool lastRound = t.ladder.byRounds && round + 1 >= t.ladder.rounds;
    Lineup lu;
    nextLineup(t, round, lu);
    ladderRoundFixtures(t, round + 1, lu, lastRound);
  }
  bool hasFinal = false;
  for (uint16_t i = 0; i < t.nFix; i++) hasFinal = hasFinal || t.fix[i].isFinal;
  if (!t.ladder.byRounds && !hasFinal && finalTime(t)) callFinal(t);
}

// ---------------------------------------------------------------- liga (círculo) y copa
static void roundRobin(Tournament& t, const int16_t* ids, uint8_t n, uint8_t legs) {
  int16_t slot[MAX_TEAMS + 1];
  uint8_t m = n;
  for (uint8_t i = 0; i < n; i++) slot[i] = ids[i];
  if (m % 2 == 1) slot[m++] = -1;  // descanso
  uint16_t first = t.nFix;
  for (uint8_t r = 0; r + 1 < m; r++) {
    for (uint8_t i = 0; i < m / 2; i++) {
      int16_t a = slot[i], b = slot[m - 1 - i];
      if (a < 0 || b < 0) continue;
      Fixture* f = addFixture(t);
      if (!f) return;
      newId("fx", f->id, sizeof(f->id));
      f->round = r + 1;
      bool keep = (r + i) % 2 == 0;  // lados alternos para repartir Blanco y Azul
      f->white = keep ? a : b;
      f->blue = keep ? b : a;
    }
    int16_t last = slot[m - 1];  // ids.splice(1, 0, ids.pop())
    for (uint8_t k = m - 1; k > 1; k--) slot[k] = slot[k - 1];
    slot[1] = last;
  }
  if (legs != 2) return;
  uint16_t idaEnd = t.nFix, rounds = 0;
  for (uint16_t i = first; i < idaEnd; i++)
    if (t.fix[i].round > rounds) rounds = t.fix[i].round;
  for (uint16_t i = first; i < idaEnd; i++) {
    Fixture* f = addFixture(t);
    if (!f) return;
    newId("fx", f->id, sizeof(f->id));
    f->round = t.fix[i].round + rounds;
    f->white = t.fix[i].blue;
    f->blue = t.fix[i].white;
  }
}
static void buildBracket(Tournament& t, const int16_t* seeded, uint8_t n) {
  uint8_t size = n <= 4 ? 4 : 8;
  uint8_t order[8] = {1}, len = 1;
  while (len < size) {  // 1-8, 4-5, 2-7, 3-6
    uint8_t next = len * 2 + 1, tmp[8];
    for (uint8_t i = 0; i < len; i++) tmp[i * 2] = order[i], tmp[i * 2 + 1] = next - order[i];
    len *= 2;
    memcpy(order, tmp, len);
  }
  uint8_t rounds = size == 4 ? 2 : 3;
  uint16_t start[4] = {0};
  for (uint8_t r = 1; r <= rounds; r++) {
    start[r - 1] = t.nFix;
    for (uint8_t k = 0; k < (size >> r); k++) {
      Fixture* f = addFixture(t);
      if (!f) return;
      newId("fx", f->id, sizeof(f->id));
      f->round = r;
    }
  }
  for (uint8_t r = 0; r + 1 < rounds; r++)
    for (uint8_t i = 0; i < (size >> (r + 1)); i++) {
      Fixture& f = t.fix[start[r] + i];
      f.next = (int16_t)(start[r + 1] + i / 2);
      f.nextSlot = i % 2;
    }
  for (uint8_t i = 0; i < size / 2; i++) {
    Fixture& f = t.fix[start[0] + i];
    f.white = order[i * 2] <= n ? seeded[order[i * 2] - 1] : -1;
    f.blue = order[i * 2 + 1] <= n ? seeded[order[i * 2 + 1] - 1] : -1;
  }
  for (uint8_t i = 0; i < size / 2; i++) {  // pases directos de la primera ronda
    Fixture& f = t.fix[start[0] + i];
    if (f.white >= 0 && f.blue < 0) advanceWinner(t, f, f.white, true);
    else if (f.white < 0 && f.blue >= 0) advanceWinner(t, f, f.blue, true);
  }
}

// ---------------------------------------------------------------- clasificación (leagueStandings)
struct H2H { int16_t a, b; int16_t pts; };
struct Table {
  StandingRow* rows = nullptr;  // por equipo
  int16_t* order = nullptr;     // equipos de mejor a peor
  H2H* h2h = nullptr;
  uint16_t nH2H = 0;
  bool ok = false;
  explicit Table(uint16_t teams, uint16_t fixtures) {
    rows = (StandingRow*)calloc(teams ? teams : 1, sizeof(StandingRow));
    order = (int16_t*)calloc(teams ? teams : 1, sizeof(int16_t));
    h2h = (H2H*)calloc(fixtures ? fixtures * 2 : 1, sizeof(H2H));
    ok = rows && order && h2h;
  }
  ~Table() { free(rows); free(order); free(h2h); }
  int16_t direct(int16_t a, int16_t b) const {  // puntos de a contra b
    for (uint16_t i = 0; i < nH2H; i++)
      if (h2h[i].a == a && h2h[i].b == b) return h2h[i].pts;
    return 0;
  }
};
static void addH2H(Table& tb, int16_t a, int16_t b, int16_t pts) {
  for (uint16_t i = 0; i < tb.nH2H; i++)
    if (tb.h2h[i].a == a && tb.h2h[i].b == b) { tb.h2h[i].pts += pts; return; }
  tb.h2h[tb.nH2H++] = {a, b, pts};
}
// La clasificación sin el partido `exclude` (para las flechas: cómo estaba antes del último).
static bool table(const Tournament& t, Table& tb, const char* exclude) {
  if (!tb.ok) return false;
  Points pts = pointsOf(t);
  for (uint16_t i = 0; i < t.nTeams; i++) {
    tb.rows[i] = StandingRow();
    tb.rows[i].team = (int16_t)i;
  }
  for (uint16_t i = 0; i < t.nFix; i++) {
    const Fixture& f = t.fix[i];
    const Result* m = resultOf(t, f.matchId, exclude);
    if (f.isFinal || !m || f.white < 0 || f.blue < 0 || !fixtureDone(f)) continue;
    for (int side = 0; side < 2; side++) {
      int16_t tid = side ? f.blue : f.white, other = side ? f.white : f.blue;
      int outcome = f.draw ? 1 : f.winner == tid ? 0 : 2;  // 0 gana, 1 empata, 2 pierde
      StandingRow& r = tb.rows[tid];
      r.played++;
      r.gf += side ? m->b : m->w;
      r.ga += side ? m->w : m->b;
      r.diff = r.gf - r.ga;
      int16_t p = outcome == 0 ? pts.win : outcome == 1 ? pts.draw : pts.loss;
      r.points += p;
      if (outcome == 0) r.wins++;
      else if (outcome == 1) r.draws++;
      else r.losses++;
      addH2H(tb, tid, other, p);
    }
  }
  // Puntos → diferencia → goles a favor → enfrentamiento directo → nombre (orden estable, como sort de JS).
  for (uint16_t i = 0; i < t.nTeams; i++) {
    uint16_t k = i;
    while (k > 0) {
      const StandingRow& a = tb.rows[tb.order[k - 1]];
      const StandingRow& b = tb.rows[i];
      int c = b.points - a.points;
      if (!c) c = b.diff - a.diff;
      if (!c) c = b.gf - a.gf;
      if (!c) c = tb.direct(b.team, a.team) - tb.direct(a.team, b.team);
      if (!c) c = collate(t.teams[a.team].name, t.teams[b.team].name);
      if (c <= 0) break;  // a (el de antes) va delante
      tb.order[k] = tb.order[k - 1];
      k--;
    }
    tb.order[k] = (int16_t)i;
  }
  return true;
}

}  // namespace rg

// ================================================================ memoria
bool alloc(Tournament& t) {
  if (!t.teams) t.teams = (Team*)calloc(MAX_TEAMS_T, sizeof(Team));
  if (!t.fix) t.fix = (Fixture*)calloc(MAX_FIXTURES, sizeof(Fixture));
  if (!t.played) t.played = (Played*)calloc(MAX_PLAYED, sizeof(Played));
  return t.teams && t.fix && t.played;
}
void release(Tournament& t) {
  free(t.teams);
  free(t.fix);
  free(t.played);
  t.teams = nullptr;
  t.fix = nullptr;
  t.played = nullptr;
  clear(t);
}
void clear(Tournament& t) {
  Team* teams = t.teams;
  Fixture* fix = t.fix;
  Played* played = t.played;
  t = Tournament();  // todo a cero y con los valores de por defecto
  t.teams = teams;
  t.fix = fix;
  t.played = played;
}

// ================================================================ pequeñas
bool fixtureDone(const Fixture& f) { return f.winner >= 0 || f.draw; }

int16_t fixtureIndex(const Tournament& t, const char* id) {
  for (uint16_t i = 0; i < t.nFix; i++)
    if (!strcmp(t.fix[i].id, id)) return (int16_t)i;
  return -1;
}

void addPlayed(Tournament& t, const char* fixtureId, const Result& r) {
  if (rg::resultOf(t, r.matchId)) return;
  rg::insertPlayed(t, fixtureId, r);
}

Points pointsOf(const Tournament& t) { return t.hasPoints ? t.points : Points(); }

void rulesShort(const Rules& r, char* out, size_t n) {
  if (r.end == End::Time) snprintf(out, n, "%u min", r.minutes);
  else if (r.end == End::Both) snprintf(out, n, "a %u goles o %u min", r.goals, r.minutes);
  else snprintf(out, n, "a %u goles", r.goals);
}
static bool sameRules(const Rules& a, const Rules& b) {
  return a.end == b.end && (a.end == End::Time || a.goals == b.goals) && (a.end == End::Goals || a.minutes == b.minutes);
}

bool isFinalFixture(const Tournament& t, const Fixture& f) {
  if (f.isFinal) return true;
  return t.format == Format::Bracket && f.round == rg::maxRound(t);
}

Rules fixtureRules(const Tournament& t, const Fixture& f) {
  if (!isFinalFixture(t, f)) return t.rules;
  Rules r = t.hasFinalRules ? t.finalRules : t.rules;
  r.allowDraw = false;  // la final nunca acaba en empate
  return r;
}

int16_t nextFixture(const Tournament& t) {
  if (t.status != Status::Active) return -1;
  int16_t best = -1;
  for (uint16_t i = 0; i < t.nFix; i++) {
    const Fixture& f = t.fix[i];
    if (f.white < 0 || f.blue < 0 || fixtureDone(f) || f.skipped) continue;
    if (best < 0 || f.round < t.fix[best].round) best = (int16_t)i;
  }
  return best;
}

void fixtureProgress(const Tournament& t, uint16_t& done, uint16_t& total) {
  done = total = 0;
  for (uint16_t i = 0; i < t.nFix; i++) {
    if (t.fix[i].bye || t.fix[i].skipped) continue;
    total++;
    if (fixtureDone(t.fix[i])) done++;
  }
}

bool finalSeriesEditable(const Tournament& t) {
  if (t.status != Status::Active) return false;
  for (uint16_t i = 0; i < t.nFix; i++)
    if (isFinalFixture(t, t.fix[i]) && (t.fix[i].winner >= 0 || t.fix[i].nMatches)) return false;
  return true;
}

void markAbandoned(Tournament& t, int16_t fixture, uint64_t nowMs) {
  if (fixture < 0 || fixture >= (int16_t)t.nFix || fixtureDone(t.fix[fixture])) return;
  t.fix[fixture].abandonedAt = nowMs;
}

// ================================================================ tipos de torneo (templates.ts)
static bool templateAllowsDraw(const Template& t) {
  return t.format == Format::League && t.rules.end != End::Goals && t.allowDraw;
}
static bool hasTemplateFinal(const Template& t) {
  return t.format == Format::Bracket || t.format == Format::Pool || (t.format == Format::League && t.finalKind == FinalKind::Top2);
}
static Rules finalRulesOf(const Template& t) { return t.ownFinalRules ? t.finalRules : t.rules; }

void describeTemplate(const Template& t, char* out, size_t n) {
  static const char* const FORMAT[3] = {"Liga", "Copa", "Pool"};
  char part[96];
  snprintf(out, n, "%s", FORMAT[(int)t.format]);
  auto add = [&](const char* s) {
    rg::append(out, n, " · ");
    rg::append(out, n, s);
  };
  if (t.format == Format::Pool) {
    add(t.poolSwap ? "con cambio de pareja" : "sin cambio de pareja");
    if (t.poolByRounds) snprintf(part, sizeof(part), "%u rondas", t.poolRounds);
    else snprintf(part, sizeof(part), "%u min", t.poolMinutes);
    add(part);
  } else if (t.teamSize == 1) add("1 vs 1");
  else {
    snprintf(part, sizeof(part), "parejas %s",
             t.pairing == Pairing::Elo ? "equilibradas" : t.pairing == Pairing::Manual ? "elegidas" : "al azar");
    add(part);
  }
  if (t.format == Format::League) {
    if (t.legs == 2) add("ida y vuelta");
    if (templateAllowsDraw(t)) snprintf(part, sizeof(part), "%u-%u-%u pts", t.points.win, t.points.draw, t.points.loss);
    else snprintf(part, sizeof(part), "%u-%u pts", t.points.win, t.points.loss);
    add(part);
  }
  const char* bo = t.finalBestOf > 1 ? " al mejor de 3" : "";
  if (t.format == Format::Bracket) {
    if (*bo) {
      snprintf(part, sizeof(part), "final%s", bo);
      add(part);
    }
  } else if (t.format == Format::Pool) {
    snprintf(part, sizeof(part), "final en la pista 1%s", bo);
    add(part);
  } else {
    bool top2 = t.finalKind == FinalKind::Top2;
    snprintf(part, sizeof(part), "%s%s", top2 ? "final 1º vs 2º" : "sin final", top2 ? bo : "");
    add(part);
  }
  char rules[40], fr[40];
  rulesShort(t.rules, rules, sizeof(rules));
  Rules f = finalRulesOf(t);
  if (hasTemplateFinal(t) && !sameRules(t.rules, f)) {
    rulesShort(f, fr, sizeof(fr));
    snprintf(part, sizeof(part), "%s (final %s)", rules, fr);
    add(part);
  } else add(rules);
  if (t.ranked) add("cuenta para ELO");
}

Fit templateFit(const Template& t, uint8_t n) {
  Fit f;
  memset(&f, 0, sizeof(f));
  char extra[40] = "";
  if (t.format != Format::Bracket && t.finalKind != FinalKind::None)
    snprintf(extra, sizeof(extra), " + final%s", t.finalBestOf > 1 ? " al mejor de 3" : "");
  const char* bo = t.finalBestOf > 1 ? " al mejor de 3" : "";
  if (t.format == Format::Pool) {
    if (n % 2 == 1) { TORNEO_CPY(f.reason, "Necesita número par de jugadores"); return f; }
    if (n < POOL_MIN_PAIRS * 2) { snprintf(f.reason, sizeof(f.reason), "Mínimo %u jugadores", POOL_MIN_PAIRS * 2); return f; }
    if (n > POOL_MAX_PAIRS * 2) { snprintf(f.reason, sizeof(f.reason), "Máximo %u jugadores", POOL_MAX_PAIRS * 2); return f; }
    uint8_t courts = n / 2 / 2;
    bool bench = (n / 2) % 2 == 1;
    char limit[16];
    if (t.poolByRounds) snprintf(limit, sizeof(limit), "%u rondas", t.poolRounds);
    else snprintf(limit, sizeof(limit), "%u min", t.poolMinutes);
    f.ok = true;
    snprintf(f.summary, sizeof(f.summary), "%u parejas · %u pista%s%s · %s · final en la pista 1%s", n / 2, courts,
             courts > 1 ? "s" : "", bench ? " + banquillo" : "", limit, bo);
    return f;
  }
  uint8_t size = t.teamSize;
  const char* unit = size == 1 ? "jugadores" : "parejas";
  if (size == 2 && n % 2 == 1) { TORNEO_CPY(f.reason, "Necesita número par de jugadores"); return f; }
  uint8_t teams = n / size;
  if (teams < MIN_TEAMS) {
    if (size == 2) snprintf(f.reason, sizeof(f.reason), "Mínimo %u jugadores (%u parejas)", MIN_TEAMS * size, MIN_TEAMS);
    else snprintf(f.reason, sizeof(f.reason), "Mínimo %u jugadores", MIN_TEAMS * size);
    return f;
  }
  if (teams > MAX_TEAMS) { snprintf(f.reason, sizeof(f.reason), "Máximo %u %s", MAX_TEAMS, unit); return f; }
  f.ok = true;
  if (t.format == Format::League) {
    uint8_t legs = t.legs == 2 ? 2 : 1;
    snprintf(f.summary, sizeof(f.summary), "%u partidos%s · %u %s, todos contra todos%s", teams * (teams - 1) / 2 * legs, extra,
             teams, unit, legs == 2 ? " (ida y vuelta)" : "");
    return f;
  }
  uint8_t byes = (teams <= 4 ? 4 : 8) - teams;
  char byeText[32] = "";
  if (byes) snprintf(byeText, sizeof(byeText), " · %u pase%s directo%s", byes, byes > 1 ? "s" : "", byes > 1 ? "s" : "");
  snprintf(f.summary, sizeof(f.summary), "%u %s · %s%s%s", teams, unit, teams <= 4 ? "semifinales y final" : "cuartos, semis y final",
           bo, byeText);
  return f;
}

// formTeams: parejas equilibradas (el mejor ELO con el peor; con número impar, fuera el del medio) o al azar.
static uint8_t formTeams(const char (*sel)[40], uint8_t n, uint8_t size, bool byElo, EloFn elo, rg::Rng& rnd, Draft& d,
                         char (*leftover)[40]) {
  if (size == 1) {
    for (uint8_t i = 0; i < n && d.nTeams < MAX_ENTRANTS; i++) {
      Draft::DTeam& tm = d.teams[d.nTeams++];
      memset(&tm, 0, sizeof(tm));
      tm.n = 1;
      TORNEO_CPY(tm.player[0], sel[i]);
    }
    return 0;
  }
  uint8_t idx[MAX_ENTRANTS];
  uint8_t m = 0;
  for (uint8_t i = 0; i < n && m < MAX_ENTRANTS; i++) idx[m++] = i;
  uint8_t left = 0;
  auto pair = [&](uint8_t a, uint8_t b) {
    if (d.nTeams >= MAX_ENTRANTS) return;
    Draft::DTeam& tm = d.teams[d.nTeams++];
    memset(&tm, 0, sizeof(tm));
    tm.n = 2;
    TORNEO_CPY(tm.player[0], sel[a]);
    TORNEO_CPY(tm.player[1], sel[b]);
  };
  if (byElo) {
    int score[MAX_ENTRANTS];
    for (uint8_t i = 0; i < m; i++) score[i] = elo ? elo(sel[idx[i]]) : 0;
    for (uint8_t i = 1; i < m; i++) {  // de más a menos ELO, estable
      uint8_t v = idx[i];
      int s = score[i];
      int k = i;
      while (k > 0 && score[k - 1] < s) idx[k] = idx[k - 1], score[k] = score[k - 1], k--;
      idx[k] = v;
      score[k] = s;
    }
    if (m % 2) {
      uint8_t mid = m / 2;
      if (leftover) TORNEO_CPY(leftover[left], sel[idx[mid]]);
      left++;
      for (uint8_t k = mid; k + 1 < m; k++) idx[k] = idx[k + 1];
      m--;
    }
    for (uint8_t a = 0, b = m; a + 1 < b; a++, b--) pair(idx[a], idx[b - 1]);
    return left;
  }
  rg::shuffle(idx, m, rnd);
  uint8_t i = 0;
  for (; i + 1 < m; i += 2) pair(idx[i], idx[i + 1]);
  for (; i < m; i++) {
    if (leftover) TORNEO_CPY(leftover[left], sel[idx[i]]);
    left++;
  }
  return left;
}

uint8_t draftFromTemplate(const Template& t, const char* name, const char (*selected)[40], uint8_t n, EloFn elo,
                          const char* seed, const char (*fixedPairs)[2][40], uint8_t nFixed, Draft& out,
                          char (*leftover)[40]) {
  memset(&out, 0, sizeof(out));
  TORNEO_CPY(out.name, name);
  out.tpl = &t;
  char rs[48];
  if (!seed || !*seed) rg::newId("s", rs, sizeof(rs));
  rg::Rng rnd(seed && *seed ? seed : rs);
  uint8_t size = t.format == Format::Pool ? 2 : t.teamSize;
  Pairing pairing = t.format == Format::Pool && t.pairing == Pairing::Elo ? Pairing::Random : t.pairing;
  // Los equipos guardados (parejas con sus dos jugadores apuntados, sin repetir) juegan juntos.
  bool used[MAX_ENTRANTS] = {false};
  auto selIdx = [&](const char* id) -> int {
    for (uint8_t i = 0; i < n; i++)
      if (!strcmp(selected[i], id)) return i;
    return -1;
  };
  if (size == 2)
    for (uint8_t p = 0; p < nFixed && out.nTeams < MAX_ENTRANTS; p++) {
      int a = selIdx(fixedPairs[p][0]), b = selIdx(fixedPairs[p][1]);
      if (a < 0 || b < 0 || a == b || used[a] || used[b]) continue;
      used[a] = used[b] = true;
      Draft::DTeam& tm = out.teams[out.nTeams++];
      memset(&tm, 0, sizeof(tm));
      tm.n = 2;
      TORNEO_CPY(tm.player[0], fixedPairs[p][0]);
      TORNEO_CPY(tm.player[1], fixedPairs[p][1]);
    }
  char rest[MAX_ENTRANTS][40];
  uint8_t nRest = 0;
  for (uint8_t i = 0; i < n && nRest < MAX_ENTRANTS; i++)
    if (!used[i]) TORNEO_CPY(rest[nRest++], selected[i]);
  if (size == 2 && pairing == Pairing::Manual) {  // a mano: nadie se empareja solo
    for (uint8_t i = 0; i < nRest; i++)
      if (leftover) TORNEO_CPY(leftover[i], rest[i]);
    return nRest;
  }
  return formTeams(rest, nRest, size, pairing == Pairing::Elo, elo, rnd, out, leftover);
}

bool validateDraft(const Draft& d, char* err, size_t errN) {
  auto fail = [&](const char* msg) {
    if (err && errN) snprintf(err, errN, "%s", msg);
    return false;
  };
  if (!d.tpl) return fail("Falta el tipo de torneo.");
  const Template& tp = *d.tpl;
  char msg[120] = "";
  if (rg::blank(d.name)) snprintf(msg, sizeof(msg), "Ponle un nombre al torneo.");
  bool dup = false;
  for (uint8_t i = 0; i < d.nTeams; i++)
    for (uint8_t a = 0; a < d.teams[i].n; a++)
      for (uint8_t j = i; j < d.nTeams; j++)
        for (uint8_t b = (j == i ? a + 1 : 0); b < d.teams[j].n; b++) dup = dup || !strcmp(d.teams[i].player[a], d.teams[j].player[b]);
  if (tp.format == Format::Pool) {
    bool odd = false;
    for (uint8_t i = 0; i < d.nTeams; i++) odd = odd || d.teams[i].n != 2;
    if (!msg[0]) {
      if (d.nTeams < POOL_MIN_PAIRS || d.nTeams > POOL_MAX_PAIRS)
        snprintf(msg, sizeof(msg), "El Pool necesita entre %u y %u parejas (%u a %u jugadores).", POOL_MIN_PAIRS, POOL_MAX_PAIRS,
                 POOL_MIN_PAIRS * 2, POOL_MAX_PAIRS * 2);
      else if (odd) snprintf(msg, sizeof(msg), "En el Pool se juega por parejas: hace falta un número par de jugadores.");
      else if (dup) snprintf(msg, sizeof(msg), "Un jugador no puede estar en dos parejas.");
      else if (tp.poolByRounds ? !(tp.poolRounds >= 1) : !(tp.poolMinutes >= 5)) snprintf(msg, sizeof(msg), "Pon cuánto dura el Pool.");
    }
    return msg[0] ? fail(msg) : true;
  }
  uint8_t size = tp.teamSize;
  bool wrong = false;
  for (uint8_t i = 0; i < d.nTeams; i++) wrong = wrong || d.teams[i].n != size;
  if (!msg[0]) {
    if (d.nTeams < MIN_TEAMS || d.nTeams > MAX_TEAMS) snprintf(msg, sizeof(msg), "Entre %u y %u equipos.", MIN_TEAMS, MAX_TEAMS);
    else if (dup) snprintf(msg, sizeof(msg), "Un jugador no puede estar en dos equipos.");
    else if (wrong)
      snprintf(msg, sizeof(msg), "%s", size == 2 ? "Con parejas fijas hace falta un número par de jugadores." : "Cada equipo necesita 1 jugador.");
  }
  return msg[0] ? fail(msg) : true;
}

// teamName: «Ana + Bea» (o «?» si no se sabe).
static void teamName(const Draft::DTeam& d, NameFn name, char* out, size_t n) {
  out[0] = 0;
  for (uint8_t i = 0; i < d.n; i++) {
    const char* s = name ? name(d.player[i]) : nullptr;
    if (i) rg::append(out, n, " + ");
    rg::append(out, n, s ? s : "?");
  }
}
static void fillTeam(Team& tm, const Draft::DTeam& d, NameFn name) {
  rg::newId("tt", tm.id, sizeof(tm.id));
  rg::trimInto(tm.name, sizeof(tm.name), d.name);
  if (!tm.name[0]) teamName(d, name, tm.name, sizeof(tm.name));
  tm.n = d.n;
  for (uint8_t i = 0; i < d.n && i < 2; i++) TORNEO_CPY(tm.player[i], d.player[i]);
  TORNEO_CPY(tm.logo, d.logo);
  TORNEO_CPY(tm.clubId, d.clubId);
}

bool createTournament(const Draft& d, EloFn elo, NameFn name, uint64_t nowMs, const char* seed, Tournament& t, char* err,
                      size_t errN) {
  if (!validateDraft(d, err, errN)) return false;
  if (!alloc(t)) {
    if (err && errN) snprintf(err, errN, "No hay memoria para el torneo.");
    return false;
  }
  clear(t);
  const Template& tp = *d.tpl;
  char rs[48];
  if (!seed || !*seed) rg::newId("s", rs, sizeof(rs));
  rg::Rng rnd(seed && *seed ? seed : rs);
  bool hasFinal = hasTemplateFinal(tp);
  uint8_t finalBestOf = hasFinal && tp.finalBestOf > 1 ? tp.finalBestOf : 1;
  // Lo común (templateMatchConfig + lo que copia createTournament del borrador).
  rg::trimInto(t.name, sizeof(t.name), d.name);
  t.format = tp.format;
  t.teamSize = tp.format == Format::Pool ? 2 : tp.teamSize;
  t.ranked = tp.ranked;
  t.rules = tp.rules;
  t.rules.allowDraw = templateAllowsDraw(tp);
  Rules fr = finalRulesOf(tp);
  if (hasFinal && !sameRules(tp.rules, fr)) {  // la final nunca empata
    t.hasFinalRules = true;
    t.finalRules = fr;
    t.finalRules.allowDraw = false;
  }
  t.status = Status::Active;
  t.createdAt = nowMs;
  rg::trimInto(t.lastTitle, sizeof(t.lastTitle), tp.lastTitle);
  TORNEO_CPY(t.templateName, tp.name);
  TORNEO_CPY(t.templateId, tp.id);
  t.edition = d.edition;
  rg::trimInto(t.competition, sizeof(t.competition), d.competition);
  TORNEO_CPY(t.logo, d.logo);
  TORNEO_CPY(t.banner, d.banner);
  TORNEO_CPY(t.cup, d.cup);

  if (tp.format == Format::Pool) {
    // Un «equipo» de 1 por jugador y las parejas de salida (al azar en las pistas); empieza la ronda 1.
    rg::newId("t", t.id, sizeof(t.id));
    for (uint8_t i = 0; i < d.nTeams; i++)
      for (uint8_t k = 0; k < d.teams[i].n && t.nEntrants < MAX_ENTRANTS; k++) TORNEO_CPY(t.entrants[t.nEntrants++], d.teams[i].player[k]);
    uint8_t order[MAX_ENTRANTS];
    for (uint8_t i = 0; i < d.nTeams; i++) order[i] = i;
    rg::shuffle(order, d.nTeams, rnd);
    for (uint8_t i = 0; i < t.nEntrants; i++) {
      Team& tm = t.teams[rg::addTeam(t)];
      Draft::DTeam one;
      memset(&one, 0, sizeof(one));
      one.n = 1;
      TORNEO_CPY(one.player[0], t.entrants[i]);
      fillTeam(tm, one, name);
    }
    for (uint8_t i = 0; i < d.nTeams; i++) fillTeam(t.teams[rg::addTeam(t)], d.teams[order[i]], name);
    t.hasLadder = true;
    t.ladder.swap = tp.poolSwap;
    t.ladder.byRounds = tp.poolByRounds;
    t.ladder.minutes = tp.poolMinutes;
    t.ladder.rounds = tp.poolRounds;
    t.ladder.finalBestOf = finalBestOf;
    t.ladder.nStart = d.nTeams;
    for (uint8_t i = 0; i < d.nTeams; i++) rg::setPair(t.ladder.start[i], d.teams[order[i]].player);
    rg::firstLadderRound(t);
    return true;
  }

  for (uint8_t i = 0; i < d.nTeams; i++) fillTeam(t.teams[rg::addTeam(t)], d.teams[i], name);
  int16_t ids[MAX_TEAMS];
  uint8_t n = (uint8_t)t.nTeams;
  for (uint8_t i = 0; i < n; i++) ids[i] = i;
  if (tp.seeding == Seeding::Elo) {  // por la media de ELO, de más a menos (estable)
    double avg[MAX_TEAMS];
    for (uint8_t i = 0; i < n; i++) {
      double s = 0;
      for (uint8_t k = 0; k < t.teams[i].n; k++) s += elo ? elo(t.teams[i].player[k]) : 0;
      avg[i] = s / t.teams[i].n;
    }
    for (uint8_t i = 1; i < n; i++) {
      int16_t v = ids[i];
      int k = i;
      while (k > 0 && avg[ids[k - 1]] < avg[v]) ids[k] = ids[k - 1], k--;
      ids[k] = v;
    }
  } else rg::shuffle(ids, n, rnd);
  // Los equipos van en el orden de la siembra (como la app, que reordena `teams`).
  Team sorted[MAX_TEAMS];
  for (uint8_t i = 0; i < n; i++) sorted[i] = t.teams[ids[i]];
  for (uint8_t i = 0; i < n; i++) t.teams[i] = sorted[i], ids[i] = i;
  if (tp.format == Format::League) {
    rg::roundRobin(t, ids, n, tp.legs == 2 ? 2 : 1);
    t.hasPoints = true;
    t.points = tp.points;
    t.finalKind = tp.finalKind;
  } else {
    rg::buildBracket(t, ids, n);
    if (finalBestOf > 1) {
      uint16_t last = rg::maxRound(t);
      for (uint16_t i = 0; i < t.nFix; i++)
        if (t.fix[i].round == last) t.fix[i].bestOf = finalBestOf;
    }
  }
  if (t.finalKind != FinalKind::None) {
    uint16_t round = rg::maxRound(t) + 1;
    Fixture* f = rg::addFixture(t);
    if (f) {
      rg::newId("fx", f->id, sizeof(f->id));
      f->round = round;
      f->isFinal = true;
      f->bestOf = finalBestOf;
    }
  }
  rg::newId("t", t.id, sizeof(t.id));
  return true;
}

// ================================================================ resultados
bool recordFixtureResult(Tournament& t, int16_t fi, const Result& r) {
  if (fi < 0 || fi >= (int16_t)t.nFix) return false;
  Fixture& f = t.fix[fi];
  if (fixtureDone(f) || f.skipped || f.white < 0 || f.blue < 0) return false;
  if (!strcmp(f.matchId, r.matchId)) return false;
  for (uint8_t i = 0; i < f.nMatches; i++)
    if (!strcmp(f.matchIds[i], r.matchId)) return false;
  bool draw = r.winner != 'w' && r.winner != 'b';
  // Un empate solo vale en la fase regular de una liga; en una eliminatoria o una final no cuenta.
  if (draw && (t.format != Format::League || isFinalFixture(t, f) || f.bestOf > 1)) return false;
  TORNEO_CPY(f.matchId, r.matchId);
  f.abandonedAt = 0;
  rg::insertPlayed(t, f.id, r);
  if (draw) f.draw = true;
  else if (f.bestOf > 1) {
    if (f.nMatches < MAX_SERIES) TORNEO_CPY(f.matchIds[f.nMatches++], r.matchId);
    if (r.winner == 'w') f.serW++;
    else f.serB++;
    f.hasSeries = true;
    // La serie sigue hasta que uno llegue a las victorias necesarias.
    if ((r.winner == 'w' ? f.serW : f.serB) < (f.bestOf + 1) / 2) return true;
  }
  if (!draw) rg::advanceWinner(t, f, r.winner == 'w' ? f.white : f.blue);
  if (isFinalFixture(t, f)) {
    rg::finish(t, f.winner, r);
    return true;
  }
  if (t.hasLadder) {
    rg::ladderAfterResult(t, f.round);
    return true;
  }
  bool regularDone = true;
  for (uint16_t i = 0; i < t.nFix; i++) {
    const Fixture& x = t.fix[i];
    if (!(x.isFinal || fixtureDone(x) || (x.white < 0 && x.blue < 0))) regularDone = false;
  }
  if (t.format == Format::Bracket || !regularDone) return true;
  rg::Table tb(t.nTeams, t.nFix);
  if (!rg::table(t, tb, nullptr)) return true;
  int16_t finalFx = -1;
  for (uint16_t i = 0; i < t.nFix && finalFx < 0; i++)
    if (t.fix[i].isFinal) finalFx = (int16_t)i;
  if (finalFx < 0 || t.finalKind == FinalKind::None) {
    // Sin final: gana el líder. El premio va en su último partido del torneo.
    const Team& leader = t.teams[tb.order[0]];
    const Result* prize = nullptr;
    for (uint16_t i = 0; i < t.nFix; i++) {
      const Fixture& x = t.fix[i];
      if (!x.matchId[0]) continue;
      bool theirs = false;
      for (int s = 0; s < 2; s++) {
        int16_t id = s ? x.blue : x.white;
        if (id < 0) continue;
        for (uint8_t k = 0; k < t.teams[id].n; k++) theirs = theirs || rg::teamHas(leader, t.teams[id].player[k]);
      }
      const Result* m = theirs ? rg::resultOf(t, x.matchId) : nullptr;
      if (m && (!prize || m->finishedAt > prize->finishedAt)) prize = m;
    }
    Result keep = prize ? *prize : r;
    rg::finish(t, tb.order[0], keep);
    t.finishedAt = r.finishedAt;
    return true;
  }
  t.fix[finalFx].white = tb.order[0];
  t.fix[finalFx].blue = tb.order[1];
  return true;
}

bool undoLast(Tournament& t, char* removed, size_t n) {
  if (!t.nPlayed || !t.played) return false;
  uint16_t count = t.nPlayed;
  Played* old = (Played*)calloc(count, sizeof(Played));
  if (!old) return false;
  memcpy(old, t.played, count * sizeof(Played));
  if (removed && n) snprintf(removed, n, "%s", old[count - 1].r.matchId);
  t.status = Status::Active;
  t.winner = -1;
  t.finishedAt = 0;
  t.finalMatchId[0] = 0;
  t.nPlayed = 0;
  if (t.hasLadder) {
    // Se vuelve a la ronda 1 y se apuntan los demás: las rondas siguientes salen iguales (sorteos con semilla).
    t.nFix = 0;
    rg::firstLadderRound(t);
  } else {
    // Vuelta al calendario o cuadro inicial (sin resultados); las rondas siguientes y la final, sin equipos.
    for (uint16_t i = 0; i < t.nFix; i++) {
      Fixture& f = t.fix[i];
      if (f.bye) continue;
      f.winner = -1;
      f.draw = false;
      f.matchId[0] = 0;
      f.nMatches = 0;
      memset(f.matchIds, 0, sizeof(f.matchIds));
      f.serW = f.serB = 0;
      f.hasSeries = false;
      if (f.isFinal || (t.format == Format::Bracket && f.round > 1)) f.white = f.blue = -1;
    }
    for (uint16_t i = 0; i < t.nFix; i++)
      if (t.fix[i].bye) rg::advanceWinner(t, t.fix[i], t.fix[i].winner, true);
  }
  for (uint16_t i = 0; i + 1 < count; i++) recordFixtureResult(t, fixtureIndex(t, old[i].fixture), old[i].r);
  free(old);
  return true;
}

void updateRules(Tournament& t, const Rules& r, const Rules* finalR, uint8_t finalBestOf) {
  bool editable = finalSeriesEditable(t);
  Rules config = t.rules;  // {...config, ...rules}: allowDraw sigue como estaba
  config.end = r.end;
  config.goals = r.goals;
  config.minutes = r.minutes;
  for (uint16_t i = 0; i < t.nFix; i++) {
    Fixture& f = t.fix[i];
    if (!finalBestOf || !isFinalFixture(t, f) || f.winner >= 0 || f.nMatches) continue;
    f.bestOf = finalBestOf > 1 ? finalBestOf : 1;
  }
  t.hasFinalRules = finalR && !sameRules(r, *finalR);
  if (t.hasFinalRules) {
    t.finalRules = config;
    t.finalRules.end = finalR->end;
    t.finalRules.goals = finalR->goals;
    t.finalRules.minutes = finalR->minutes;
  } else t.finalRules = Rules();
  t.rules = config;
  // En la escalera la final aún no existe: se apunta para cuando toque.
  if (t.hasLadder && finalBestOf && editable) t.ladder.finalBestOf = finalBestOf;
}

// ================================================================ clasificaciones
uint8_t standings(const Tournament& t, StandingRow* out, uint8_t max) {
  rg::Table now(t.nTeams, t.nFix);
  if (!rg::table(t, now, nullptr)) return 0;
  // Flechas (standingsMovement): cómo estaba cada uno antes del último partido de la fase regular.
  const Result* last = nullptr;
  for (uint16_t i = 0; i < t.nFix; i++) {
    if (t.fix[i].isFinal) continue;
    const Result* m = rg::resultOf(t, t.fix[i].matchId);
    if (m && (!last || m->finishedAt >= last->finishedAt)) last = m;
  }
  rg::Table before(last ? t.nTeams : 0, last ? t.nFix : 0);
  bool moves = last && rg::table(t, before, last->matchId);
  uint8_t n = 0;
  for (uint16_t i = 0; i < t.nTeams && n < max; i++, n++) {
    out[n] = now.rows[now.order[i]];
    out[n].move = 0;
    if (!moves) continue;
    for (uint16_t k = 0; k < t.nTeams; k++)
      if (before.order[k] == now.order[i]) out[n].move = k > i ? 1 : k < i ? -1 : 0;
  }
  return n;
}

uint8_t finalRanking(const Tournament& t, Place* out, uint8_t max) {
  uint8_t n = 0;
  if (t.hasLadder) {  // ladderPlaces: la escalera de la última ronda
    uint16_t round = rg::maxRound(t);
    uint16_t fx[POOL_MAX_PAIRS];
    uint8_t k = rg::roundByCourt(t, round, fx, POOL_MAX_PAIRS);
    for (uint8_t i = 0; i < k; i++) {
      const Fixture& f = t.fix[fx[i]];
      if (f.white < 0 || f.blue < 0) continue;
      uint8_t base = i * 2 + 1;
      int16_t a = f.white, b = f.blue;
      bool decided = f.winner >= 0;
      if (decided && f.winner != f.white) a = f.blue, b = f.white;
      if (n < max) out[n++] = {a, base, a == t.winner};
      if (n < max) out[n++] = {b, (uint8_t)(decided ? base + 1 : base), b == t.winner};
    }
    if (k && t.fix[fx[0]].nResting)
      for (uint16_t i = 0; i < t.nTeams; i++)
        if (rg::sameIds(t.teams[i].player, t.teams[i].n, t.fix[fx[0]].resting, t.fix[fx[0]].nResting)) {
          if (n < max) out[n++] = {(int16_t)i, (uint8_t)(k * 2 + 1), (int16_t)i == t.winner};
          break;
        }
    return n;
  }
  rg::Table tb(t.nTeams, t.nFix);
  if (!rg::table(t, tb, nullptr)) return 0;
  int16_t* order = tb.order;
  int32_t key[MAX_TEAMS_T];
  const Team* winner = t.winner >= 0 ? &t.teams[t.winner] : nullptr;
  bool sort = false;
  if (t.format == Format::Bracket) {
    // Ronda más alta a la que llegó cada uno (el campeón, el primero); el orden estable deja el desempate.
    for (uint16_t i = 0; i < t.nTeams; i++) {
      int16_t id = order[i];
      int32_t reached = 0;
      if (id == t.winner) reached = 0x7FFFFFFF;
      else
        for (uint16_t k = 0; k < t.nFix; k++)
          if (!t.fix[k].bye && (t.fix[k].white == id || t.fix[k].blue == id) && t.fix[k].round > reached) reached = t.fix[k].round;
      key[i] = -reached;
    }
    sort = true;
  } else {
    int16_t fin = -1;
    for (uint16_t k = 0; k < t.nFix && fin < 0; k++)
      if (t.fix[k].isFinal && t.fix[k].winner >= 0) fin = (int16_t)k;
    if (fin >= 0 && winner) {
      const Fixture& f = t.fix[fin];
      int16_t loser = f.winner == f.white ? f.blue : f.white;
      for (uint16_t i = 0; i < t.nTeams; i++) {
        const Team& tm = t.teams[order[i]];
        key[i] = rg::within(tm, *winner) ? 0 : (loser >= 0 && rg::within(tm, t.teams[loser])) ? 1 : 2;
      }
      sort = true;
    }
  }
  if (sort)
    for (uint16_t i = 1; i < t.nTeams; i++) {
      int16_t v = order[i];
      int32_t kv = key[i];
      int k = i;
      while (k > 0 && key[k - 1] > kv) order[k] = order[k - 1], key[k] = key[k - 1], k--;
      order[k] = v;
      key[k] = kv;
    }
  for (uint16_t i = 0; i < t.nTeams && n < max; i++) {
    const Team& tm = t.teams[order[i]];
    out[n++] = {order[i], (uint8_t)(i + 1), winner && rg::within(tm, *winner)};
  }
  return n;
}

void roundLabel(const Tournament& t, uint16_t round, char* out, size_t n) {
  if (t.hasLadder) { snprintf(out, n, "Ronda %u", round); return; }
  for (uint16_t i = 0; i < t.nFix; i++)
    if (t.fix[i].round == round && t.fix[i].isFinal) { snprintf(out, n, "Final"); return; }
  if (t.format == Format::League) { snprintf(out, n, "Jornada %u", round); return; }
  int fromEnd = (int)rg::maxRound(t) - (int)round;
  if (fromEnd == 0) snprintf(out, n, "Final");
  else if (fromEnd == 1) snprintf(out, n, "Semifinales");
  else if (fromEnd == 2) snprintf(out, n, "Cuartos");
  else snprintf(out, n, "Ronda %u", round);
}

Phase phaseInfo(const Tournament& t, const Fixture& f) {
  Phase p;
  memset(&p, 0, sizeof(p));
  p.tier = 4;
  char label[24];
  roundLabel(t, f.round, label, sizeof(label));
  rg::upper(label);
  snprintf(p.title, sizeof(p.title), "%s", label);
  if (f.isFinal) {
    p.tier = 0;
    snprintf(p.title, sizeof(p.title), "GRAN FINAL");
  } else if (t.hasLadder && f.court) {
    snprintf(p.title, sizeof(p.title), "PISTA %u · %s", f.court, label);
  } else if (t.format == Format::Bracket) {
    static const char* const NAMES[4] = {"GRAN FINAL", "SEMIFINAL", "CUARTOS DE FINAL", "OCTAVOS DE FINAL"};
    int fromEnd = (int)rg::maxRound(t) - (int)f.round;
    if (fromEnd >= 0 && fromEnd < 4) {
      p.tier = (uint8_t)fromEnd;
      snprintf(p.title, sizeof(p.title), "%s", NAMES[fromEnd]);
    }
  }
  if (f.bestOf > 1) {
    uint8_t need = (f.bestOf + 1) / 2;
    if (f.serW == need - 1 && f.serB == need - 1) snprintf(p.sub, sizeof(p.sub), "¡PARTIDO DECISIVO!");
    else snprintf(p.sub, sizeof(p.sub), "PARTIDO %u · AL MEJOR DE %u", f.serW + f.serB + 1, f.bestOf);
  }
  return p;
}

// ================================================================ Pool: pistas, reloj y flechas
void lineupOfRound(const Tournament& t, uint16_t round, Lineup& lu) {
  memset(&lu, 0, sizeof(lu));
  uint16_t fx[POOL_MAX_PAIRS];
  uint8_t k = rg::roundByCourt(t, round, fx, POOL_MAX_PAIRS);
  for (uint8_t i = 0; i < k && i < POOL_MAX_PAIRS / 2; i++) {
    rg::idsOf(t, t.fix[fx[i]].white, lu.court[i][0]);
    rg::idsOf(t, t.fix[fx[i]].blue, lu.court[i][1]);
    lu.nCourts++;
  }
  if (k && t.fix[fx[0]].nResting) {
    lu.bench = true;
    rg::setPair(lu.benchIds, t.fix[fx[0]].resting);
  }
}

void arrivalLineup(const Tournament& t, uint16_t round, Lineup& lu) {
  memset(&lu, 0, sizeof(lu));
  uint16_t fx[POOL_MAX_PAIRS];
  uint8_t k = rg::roundByCourt(t, round, fx, POOL_MAX_PAIRS);
  if (k > POOL_MAX_PAIRS / 2) k = POOL_MAX_PAIRS / 2;
  bool bench = k && t.fix[fx[0]].nResting;
  for (uint8_t i = 0; i < k; i++) {
    // Arriba la que baja (o la que se queda en la 1) y abajo la que sube (o la que se queda abajo, o la del banquillo).
    const Fixture& f = t.fix[fx[i]];
    if (i == 0) rg::idsOf(t, f.winner, lu.court[i][0]);
    else rg::idsOf(t, rg::loserOf(t.fix[fx[i - 1]]), lu.court[i][0]);
    if (i == k - 1) {
      if (bench) rg::setPair(lu.court[i][1], f.resting);
      else rg::idsOf(t, rg::loserOf(f), lu.court[i][1]);
    } else rg::idsOf(t, t.fix[fx[i + 1]].winner, lu.court[i][1]);
  }
  lu.nCourts = k;
  lu.bench = bench;
  if (bench) rg::idsOf(t, rg::loserOf(t.fix[fx[k - 1]]), lu.benchIds);
}

Clock ladderClock(const Tournament& t, uint64_t nowMs) {
  Clock c;
  int64_t total = (int64_t)t.ladder.minutes * 60000;
  c.started = t.nPlayed > 0;
  c.avgMs = 0;
  if (!c.started) {
    c.remainingMs = (int32_t)total;
    return c;
  }
  int64_t start = (int64_t)t.played[0].r.startedAt;
  int64_t last = (int64_t)t.played[t.nPlayed - 1].r.finishedAt;
  int64_t rem = total - ((int64_t)nowMs - start);
  if (rem < INT32_MIN) rem = INT32_MIN;
  if (rem > INT32_MAX) rem = INT32_MAX;
  c.remainingMs = (int32_t)rem;
  c.avgMs = (int32_t)((last - start) / t.nPlayed);
  return c;
}

static uint8_t courtOf(const Tournament& t, uint16_t round, const char* id) {
  Lineup lu;
  lineupOfRound(t, round, lu);
  for (uint8_t c = 0; c < lu.nCourts; c++)
    for (int s = 0; s < 2; s++)
      for (int k = 0; k < 2; k++)
        if (!strcmp(lu.court[c][s][k], id)) return c + 1;
  if (lu.bench && (!strcmp(lu.benchIds[0], id) || !strcmp(lu.benchIds[1], id))) return lu.nCourts + 1;
  return 0;
}

void ladderMoves(const Tournament& t, uint16_t round, const char* playerId, int8_t& move) {
  move = 0;
  uint8_t now = courtOf(t, round, playerId);
  uint8_t was = round > 1 ? courtOf(t, round - 1, playerId) : 0;
  if (now && was && was != now) move = was > now ? 1 : -1;
}

bool timeForFinal(const Tournament& t) { return rg::finalTime(t); }

}  // namespace torneos

#endif
