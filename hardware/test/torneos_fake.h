// MARCADOR FUTBOLÍN V3 · guardado de torneos FALSO, solo para las pruebas de las pantallas (agente 3).
// -------------------------------------------------------------------------------------------------
// Las reglas, la ficha y el palmarés son los de verdad (s3_torneos_reglas.h y s3_torneos_ficha.h, agente 1). Aquí
// solo lo que aún escribe el agente 2: el guardado (en memoria, sin tarjeta), las plantillas, y los dos datos que
// piden las pantallas a la progresión (s3_torneos_pantallas.h: pronóstico y premio de la ceremonia), con números
// inventados. Lo sustituye s3_torneos_datos.h al unir las partes.
// fake::seed(): plantillas y torneos de ejemplo (una liga de 5 en juego, una copa de 6 con pases, un pool de 5
// parejas con banquillo y una liga terminada) jugados con partidos falsos del historial.

#ifndef MFV3_TORNEOS_FAKE_H
#define MFV3_TORNEOS_FAKE_H

#include "s3_torneos_ficha.h"
#include "s3_torneos_pantallas.h"

namespace torneos {

Summary* summaries = nullptr;
uint16_t nSummaries = 0;
uint32_t revision = 1;
Template* templates = nullptr;
uint8_t nTemplates = 0;

namespace fake {
static const uint8_t MAX_DB = 12;
static Tournament db[MAX_DB];
static uint8_t nDb = 0;
static uint32_t ids = 1;
static uint64_t now = 1792000000000ull;  // ms (oct 2026)
static uint32_t matchSeq = 1;
static int saves = 0, discards = 0;
}  // namespace fake

static void copyT(Tournament& dst, const Tournament& src) {
  alloc(dst);
  Team* tm = dst.teams;
  Fixture* fx = dst.fix;
  Played* pl = dst.played;
  dst = src;
  dst.teams = tm;
  dst.fix = fx;
  dst.played = pl;
  memcpy(tm, src.teams, sizeof(Team) * MAX_TEAMS_T);
  memcpy(fx, src.fix, sizeof(Fixture) * MAX_FIXTURES);
  memcpy(pl, src.played, sizeof(Played) * MAX_PLAYED);
}

namespace fake {
static Summary* history = nullptr;
static uint16_t nHistory = 0;
}  // namespace fake
static void rebuildSummaries() {
  nSummaries = 0;
  for (uint8_t i = 0; i < fake::nDb && nSummaries < MAX_SUMMARIES; i++) summarize(fake::db[i], summaries[nSummaries++]);
  // En memoria, solo las 10 últimas de la competición larga (las demás, «en la SD»).
  for (uint16_t i = fake::nHistory > 10 ? fake::nHistory - 10 : 0; i < fake::nHistory && nSummaries < MAX_SUMMARIES; i++)
    summaries[nSummaries++] = fake::history[i];
}

// ---------------------------------------------------------------- guardado (en memoria)
void begin(bool) {
  if (!summaries) summaries = (Summary*)calloc(MAX_SUMMARIES, sizeof(Summary));
  if (!templates) templates = (Template*)calloc(MAX_TEMPLATES, sizeof(Template));
}
bool loadIndex() {
  rebuildSummaries();
  return true;
}
bool load(const char* id, Tournament& t) {
  for (uint8_t i = 0; i < fake::nDb; i++)
    if (!strcmp(fake::db[i].id, id)) {
      copyT(t, fake::db[i]);
      return true;
    }
  return false;
}
bool save(Tournament& t) {
  fake::saves++;
  int at = -1;
  for (uint8_t i = 0; i < fake::nDb; i++)
    if (!strcmp(fake::db[i].id, t.id)) at = i;
  if (at < 0) {
    if (fake::nDb >= fake::MAX_DB) return false;
    at = fake::nDb++;
  }
  copyT(fake::db[at], t);
  rebuildSummaries();
  revision++;
  return true;
}
bool discard(const char* id) {
  for (uint8_t i = 0; i < fake::nDb; i++)
    if (!strcmp(fake::db[i].id, id)) {
      release(fake::db[i]);
      for (uint8_t k = i; k + 1 < fake::nDb; k++) fake::db[k] = fake::db[k + 1];
      fake::db[--fake::nDb] = Tournament();
      fake::discards++;
      rebuildSummaries();
      revision++;
      return true;
    }
  return false;
}
bool saveTemplate(const Template& t) {
  for (uint8_t i = 0; i < nTemplates; i++)
    if (!strcmp(templates[i].id, t.id)) {
      templates[i] = t;
      revision++;
      return true;
    }
  if (nTemplates >= MAX_TEMPLATES) return false;
  templates[nTemplates++] = t;
  revision++;
  return true;
}
bool removeTemplate(const char* id) {
  for (uint8_t i = 0; i < nTemplates; i++)
    if (!strcmp(templates[i].id, id)) {
      for (uint8_t k = i; k + 1 < nTemplates; k++) templates[k] = templates[k + 1];
      nTemplates--;
      revision++;
      return true;
    }
  return false;
}
void newTemplate(Template& out, Format f, const Template* copyOf) {
  if (copyOf) {
    out = *copyOf;
    snprintf(out.name, sizeof(out.name), "%.18s (copia)", copyOf->name);
  } else {
    out = Template();
    out.format = f;
    out.name[0] = out.lastTitle[0] = 0;
    out.logo[0] = out.banner[0] = out.cup[0] = 0;
    if (f == Format::Pool) {
      out.teamSize = 2;
      out.pairing = Pairing::Random;
    }
  }
  snprintf(out.id, sizeof(out.id), "tpl-%lu", (unsigned long)fake::ids++);
}

// ---------------------------------------------------------------- lo que piden las pantallas (números inventados)
bool predict(const char (*white)[40], uint8_t nw, const char (*blue)[40], uint8_t nb, uint8_t& wPct, uint8_t& bPct) {
  if (!nw || !nb) return false;
  uint32_t h = 0;
  for (const char* p = white[0]; *p; p++) h = h * 31 + (uint8_t)*p;
  for (const char* p = blue[0]; *p; p++) h = h * 17 + (uint8_t)*p;
  wPct = (uint8_t)(35 + h % 31);
  bPct = (uint8_t)(100 - wPct);
  return true;
}
bool reward(const Tournament& t, const char* playerId, Reward& out) {
  memset(&out, 0, sizeof(out));
  uint32_t h = 0;
  for (const char* p = playerId; *p; p++) h = h * 31 + (uint8_t)*p;
  out.xp = (uint16_t)(120 + h % 260);
  out.levelBefore = (uint8_t)(3 + h % 9);
  out.levelAfter = (uint8_t)(out.levelBefore + (h % 3 == 0 ? 1 : 0));
  out.levelFill = (uint8_t)(20 + h % 70);
  out.hasElo = t.ranked;
  out.eloDelta = (int16_t)((int)(h % 41) - 20);
  out.eloAfter = (int16_t)(1200 + out.eloDelta);
  static const char* const ACH[3] = {"Campeón de torneo", "Racha de 3", "Portería a cero"};
  out.nAch = (uint8_t)(h % 3);
  for (uint8_t i = 0; i < out.nAch; i++) out.ach[i] = ACH[i];
  out.nLines = 2;
  snprintf(out.line[0], sizeof(out.line[0]), "Partidos");
  out.lineXp[0] = (uint16_t)(out.xp * 2 / 3);
  snprintf(out.line[1], sizeof(out.line[1]), "Victorias");
  out.lineXp[1] = (uint16_t)(out.xp - out.lineXp[0]);
  return true;
}

// ---------------------------------------------------------------- palmarés por páginas
namespace fake {
}  // namespace fake
uint16_t editionCount(const char* key) {
  uint16_t n = 0;
  for (uint16_t i = 0; i < fake::nHistory; i++)
    if (!strcmp(fake::history[i].competitionKey, key)) n++;
  bool inHistory = n > 0;
  for (uint16_t i = 0; i < nSummaries && !inHistory; i++)
    if (!strcmp(summaries[i].competitionKey, key) && summaries[i].status != Status::Cancelled) n++;
  return n;
}
uint16_t editionsPage(const char* key, uint16_t from, Summary* out, uint16_t max) {
  uint16_t k = 0, seen = 0;
  bool inHistory = false;
  for (uint16_t i = 0; i < fake::nHistory; i++)
    if (!strcmp(fake::history[i].competitionKey, key)) inHistory = true;
  if (inHistory) {
    for (int i = fake::nHistory - 1; i >= 0 && k < max; i--) {  // de la más nueva a la más antigua
      if (strcmp(fake::history[i].competitionKey, key)) continue;
      if (seen++ >= from) out[k++] = fake::history[i];
    }
    return k;
  }
  uint16_t idx[MAX_SUMMARIES], n = 0;
  for (uint16_t i = 0; i < nSummaries; i++)
    if (!strcmp(summaries[i].competitionKey, key) && summaries[i].status != Status::Cancelled) idx[n++] = i;
  for (uint16_t i = 0; i < n; i++)
    for (uint16_t j = i + 1; j < n; j++)
      if (summaries[idx[j]].createdAt > summaries[idx[i]].createdAt) {
        uint16_t x = idx[i];
        idx[i] = idx[j];
        idx[j] = x;
      }
  for (uint16_t i = from; i < n && k < max; i++) out[k++] = summaries[idx[i]];
  return k;
}

// ---------------------------------------------------------------- datos de ejemplo
namespace fake {

// 35 ediciones terminadas de «Copa Histórica» (cada una con su campeón); las 10 últimas, también en `summaries`.
static void seedHistory(const char (*sel)[40], uint8_t n) {
  if (!history) history = (Summary*)calloc(40, sizeof(Summary));
  nHistory = 0;
  for (uint16_t e = 1; e <= 35; e++) {
    Summary& s = history[nHistory++];
    memset(&s, 0, sizeof(s));
    snprintf(s.id, sizeof(s.id), "hist-%u", e);
    snprintf(s.name, sizeof(s.name), "Copa Histórica · %uª edición", e);
    snprintf(s.competitionKey, sizeof(s.competitionKey), "tpl-hist");
    snprintf(s.competition, sizeof(s.competition), "Copa Histórica");
    s.format = Format::Bracket;
    s.status = Status::Finished;
    s.edition = e;
    s.createdAt = now - (uint64_t)(36 - e) * 7 * 86400000ull;
    s.finishedAt = s.createdAt + 7200000ull;
    s.done = s.total = 3;
    const char* a = sel[e % n];
    snprintf(s.champion, sizeof(s.champion), "%s", playerName && playerName(a) ? playerName(a) : a);
    s.nChampion = 1;
    snprintf(s.championIds[0], 40, "%s", a);
    snprintf(s.mvpId, sizeof(s.mvpId), "%s", sel[(e + 1) % n]);
    snprintf(s.cup, sizeof(s.cup), "trofeo:bronce");
  }
  rebuildSummaries();
}

// Apunta un partido falso (fila del historial con su marcador) en el cruce `fi`.
static void play(Tournament& t, int16_t fi, uint8_t w, uint8_t b) {
  if (fi < 0) return;
  partidos::Row r = {};
  snprintf(r.id, sizeof(r.id), "m-fake-%lu", (unsigned long)matchSeq++);
  r.fin = (uint32_t)(now / 1000) + matchSeq * 300;
  r.mode = t.ranked ? 'r' : 'q';
  r.winner = w > b ? 'w' : w < b ? 'b' : '-';
  r.reason = 'r';
  r.w = w;
  r.b = b;
  r.pw = r.pb = -1;
  r.totalMs = 240000;
  const Fixture& f = t.fix[fi];
  for (int s = 0; s < 2; s++) {
    int16_t tm = s ? f.blue : f.white;
    if (tm < 0) continue;
    for (uint8_t k = 0; k < t.teams[tm].n && k < 4; k++) {
      r.code[s][r.n[s]] = partidos::codeOf(t.teams[tm].player[k]);
      snprintf(r.name[s][r.n[s]], 20, "%s", playerName ? playerName(t.teams[tm].player[k]) : "?");
      r.n[s]++;
    }
  }
  partidos::push(r);
  Result res = {};
  snprintf(res.matchId, sizeof(res.matchId), "%s", r.id);
  res.winner = r.winner;
  res.w = w;
  res.b = b;
  res.finishedAt = (uint64_t)r.fin * 1000ull;
  res.startedAt = res.finishedAt - r.totalMs;
  recordFixtureResult(t, fi, res);
}

static void tpl(Template& t, const char* id, const char* name, Format f) {
  t = Template();
  snprintf(t.id, sizeof(t.id), "%s", id);
  snprintf(t.name, sizeof(t.name), "%s", name);
  t.format = f;
  t.lastTitle[0] = 0;
  t.logo[0] = t.cup[0] = t.banner[0] = 0;
}

static int eloSeed(const char*) { return 1200; }
static const char* nameSeed(const char* id) { return playerName ? playerName(id) : id; }

static Tournament& make(const Template& tp, const char (*sel)[40], uint8_t n, uint16_t edition, const char* seed, uint64_t at) {
  static Draft d;
  static char left[MAX_ENTRANTS][40];
  char name[64];
  snprintf(name, sizeof(name), "%s · %uª edición", tp.name, edition);
  draftFromTemplate(tp, name, sel, n, eloSeed, seed, nullptr, 0, d, left);
  d.edition = edition;
  Tournament& t = db[nDb++];
  alloc(t);
  char err[64];
  if (!createTournament(d, eloSeed, nameSeed, at, seed, t, err, sizeof(err))) printf("fake: no se creó %s: %s\n", name, err);
  return t;
}

// Plantillas y torneos de ejemplo con los jugadores `sel` (12 o más). Sin `withTournaments`, solo las plantillas.
static void seed(const char (*sel)[40], uint8_t n, bool withTournaments = true) {
  begin(true);
  for (uint8_t i = 0; i < nDb; i++) release(db[i]);
  nDb = 0;
  nTemplates = 0;
  tpl(templates[nTemplates++], "tpl-liga", "Liga del Jueves", Format::League);
  templates[0].rules.end = End::Both;
  templates[0].rules.minutes = 5;
  templates[0].finalKind = FinalKind::Top2;
  snprintf(templates[0].logo, 40, "emoji:\xf0\x9f\xa6\x85");  // 🦅
  snprintf(templates[0].lastTitle, 24, "Farolillo rojo");
  tpl(templates[nTemplates++], "tpl-copa", "Copa del Viernes", Format::Bracket);
  templates[1].finalBestOf = 3;
  templates[1].ranked = true;
  snprintf(templates[1].cup, 40, "trofeo:plata");
  tpl(templates[nTemplates++], "tpl-pool", "Pool de los Martes", Format::Pool);
  templates[2].teamSize = 2;
  templates[2].pairing = Pairing::Random;
  templates[2].poolByRounds = true;
  templates[2].poolRounds = 4;
  tpl(templates[nTemplates++], "tpl-liga2", "Liga de Parejas", Format::League);
  templates[3].teamSize = 2;
  templates[3].pairing = Pairing::Elo;
  templates[3].legs = 2;
  if (!withTournaments || n < 12) {
    rebuildSummaries();
    return;
  }
  // 1ª edición de la liga (sin final), terminada.
  Template noFinal = templates[0];
  noFinal.finalKind = FinalKind::None;
  Tournament& old = make(noFinal, sel, 4, 1, "a", now - 20 * 86400000ull);
  for (int k = 0; k < 12; k++) {
    int16_t f = nextFixture(old);
    if (f < 0) break;
    play(old, f, (uint8_t)(k % 2 ? 5 : 3), (uint8_t)(k % 2 ? 2 : 5));
  }
  // 2ª edición: liga de 5 en juego (cuatro partidos jugados, uno empatado).
  Tournament& liga = make(templates[0], sel, 5, 2, "b", now - 2 * 86400000ull);
  for (int k = 0; k < 4; k++) play(liga, nextFixture(liga), (uint8_t)(k == 1 ? 3 : k % 2 ? 2 : 5), (uint8_t)(k == 1 ? 3 : k % 2 ? 5 : 2));
  // Copa de 6 con dos pases directos (un partido jugado).
  Tournament& copa = make(templates[1], sel + 2, 6, 1, "c", now - 86400000ull);
  play(copa, nextFixture(copa), 5, 1);
  // Pool de 5 parejas (2 pistas y banquillo): la ronda 1 jugada.
  Tournament& pool = make(templates[2], sel, 10, 1, "d", now - 3600000ull);
  play(pool, nextFixture(pool), 5, 4);
  play(pool, nextFixture(pool), 2, 5);
  rebuildSummaries();
  seedHistory(sel, n);
}

}  // namespace fake
}  // namespace torneos

#endif
