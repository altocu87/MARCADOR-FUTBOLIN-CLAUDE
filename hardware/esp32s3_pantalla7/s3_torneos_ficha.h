// MARCADOR FUTBOLÍN V3 · ficha, ediciones y palmarés de los torneos en la pantalla de 7" (como report.ts de la app).
// -------------------------------------------------------------------------------------------------
// - Ficha de un torneo: partidos, goles, tiempo de juego, cada jugador con su balance y su MVP, el pichichi (solo si
//   casi todos los goles tienen autor apuntado: SCORER_COVERAGE_MIN), la mayor goleada, el partido con más goles y el
//   marcador de la final con el campeón primero.
// - MVP = 3 por victoria + 1 por empate + la mitad de su diferencia de goles + 2 si es campeón (a una décima). Los
//   goles apuntados a cada uno no cuentan: en parejas muchas veces no se apunta quién marcó.
// - Competición: los torneos con el mismo nombre propio (o del mismo tipo de torneo) son sus ediciones; las
//   descartadas no cuentan. Palmarés: campeón y MVP de cada edición y quién suma más títulos.
// Los partidos salen de `matchSource` (las filas de partidos::rows de ese torneo); el palmarés, de `summaries`.
// Lo prueba hardware/test/torneos_test.cpp contra lo que calcula la app (tests/torneosPlaca.test.ts).

#ifndef MFV3_S3_TORNEOS_FICHA_H
#define MFV3_S3_TORNEOS_FICHA_H

#include "s3_torneos_reglas.h"

namespace torneos {

static const uint8_t SCORER_COVERAGE_MIN_PCT = 80;  // SCORER_COVERAGE_MIN = 0.8 de la app

uint32_t playerCode(const char* id) {  // = partidos::codeOf (FNV-1a; los jugadores «placa-xxxxxxxx», su número)
  if (!strncmp(id, "placa-", 6) && strlen(id) == 14) return (uint32_t)strtoul(id + 6, nullptr, 16);
  uint32_t h = 2166136261u;
  for (const char* s = id; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
  return h;
}

const char* competitionName(const Tournament& t) {
  return t.competition[0] ? t.competition : t.templateName[0] ? t.templateName : t.name;
}

// competitionNameKey: «name:» + el nombre sin espacios de más y en minúsculas.
static void nameKey(const char* name, char* out, size_t n) {
  char tmp[72];
  size_t k = 0;
  bool space = false;
  for (const char* p = name; *p && k + 1 < sizeof(tmp); p++) {
    bool ws = *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r';
    if (ws) {
      space = k > 0;
      continue;
    }
    if (space && k + 2 < sizeof(tmp)) tmp[k++] = ' ';
    space = false;
    tmp[k++] = *p;
  }
  tmp[k] = 0;
  rg::lower(tmp);
  snprintf(out, n, "name:%s", tmp);
}

void competitionKey(const Tournament& t, char* out, size_t n) {
  if (t.competition[0]) nameKey(t.competition, out, n);
  else if (t.templateId[0]) snprintf(out, n, "%s", t.templateId);
  else nameKey(t.templateName[0] ? t.templateName : t.name, out, n);
}

namespace fc {

// Partidos del torneo: los de `matchSource` o, sin él, los apuntados en los cruces (sin goleadores).
static uint16_t matchesOf(const Tournament& t, MatchLine* ms, uint16_t max) {
  uint16_t n = 0;
  if (matchSource) n = matchSource(t.id, ms, max);
  else
    for (uint16_t i = 0; i < t.nPlayed && n < max; i++) {
      const Played& p = t.played[i];
      int16_t fi = fixtureIndex(t, p.fixture);
      if (fi < 0) continue;
      MatchLine& m = ms[n++];
      memset(&m, 0, sizeof(m));
      TORNEO_CPY(m.id, p.r.matchId);
      m.winner = p.r.winner;
      m.w = p.r.w;
      m.b = p.r.b;
      m.reason = 'r';
      m.startedAt = p.r.startedAt;
      m.finishedAt = p.r.finishedAt;
      m.totalMs = (uint32_t)(p.r.finishedAt - p.r.startedAt);
      m.goalsValid = (uint8_t)(m.w + m.b);
      for (int s = 0; s < 2; s++) {
        int16_t tm = s ? t.fix[fi].blue : t.fix[fi].white;
        if (tm < 0) continue;
        for (uint8_t k = 0; k < t.teams[tm].n && k < 2; k++) {
          m.code[s][m.n[s]] = playerCode(t.teams[tm].player[k]);
          TORNEO_CPY(m.name[s][m.n[s]], t.teams[tm].player[k]);
          m.n[s]++;
        }
      }
    }
  for (uint16_t i = 1; i < n; i++) {  // por hora de fin, estable
    MatchLine v = ms[i];
    uint16_t k = i;
    while (k > 0 && ms[k - 1].finishedAt > v.finishedAt) ms[k] = ms[k - 1], k--;
    ms[k] = v;
  }
  return n;
}

// Id del jugador con ese código (los del torneo); si no está, «placa-<código>».
static void idOf(const Tournament& t, uint32_t code, char* out, size_t n) {
  for (uint8_t i = 0; i < t.nEntrants; i++)
    if (playerCode(t.entrants[i]) == code) { snprintf(out, n, "%s", t.entrants[i]); return; }
  for (uint16_t i = 0; i < t.nTeams; i++)
    for (uint8_t k = 0; k < t.teams[i].n; k++)
      if (playerCode(t.teams[i].player[k]) == code) { snprintf(out, n, "%s", t.teams[i].player[k]); return; }
  snprintf(out, n, "placa-%08x", (unsigned)code);
}

struct Line {
  PlayerLine l;
  uint32_t code;
  char name[48];
  int32_t mvp10;  // MVP × 10 (entero: las cuentas salen exactas)
};

}  // namespace fc

void report(const Tournament& t, Report& out) {
  memset(&out, 0, sizeof(out));
  out.mvp = out.topScorer = -1;
  MatchLine* ms = (MatchLine*)calloc(MAX_PLAYED, sizeof(MatchLine));
  if (!ms) return;
  uint16_t n = fc::matchesOf(t, ms, MAX_PLAYED);
  const Team* champ = t.winner >= 0 && t.winner < (int16_t)t.nTeams ? &t.teams[t.winner] : nullptr;
  fc::Line lines[MAX_ENTRANTS];
  uint8_t nl = 0;
  auto lineOf = [&](uint32_t code, const char* snapshot) -> fc::Line* {
    for (uint8_t i = 0; i < nl; i++)
      if (lines[i].code == code) return &lines[i];
    if (nl >= MAX_ENTRANTS) return nullptr;
    fc::Line& L = lines[nl++];
    memset(&L, 0, sizeof(L));
    L.code = code;
    fc::idOf(t, code, L.l.id, sizeof(L.l.id));
    const char* nm = playerName ? playerName(L.l.id) : nullptr;
    TORNEO_CPY(L.name, nm ? nm : snapshot);
    L.l.champion = champ && rg::teamHas(*champ, L.l.id);
    return &L;
  };
  uint32_t total = 0, assigned = 0;
  for (uint16_t i = 0; i < n; i++) {
    const MatchLine& m = ms[i];
    out.totalGoals += m.w + m.b;
    out.playTimeMs += m.totalMs;
    if (m.reason == 'g' || m.reason == 'p') out.extraTime++;
    total += m.goalsValid;
    assigned += m.goalsAssigned;
    for (int s = 0; s < 2; s++)
      for (uint8_t k = 0; k < m.n[s] && k < 2; k++) {
        fc::Line* L = lineOf(m.code[s][k], m.name[s][k]);
        if (!L) continue;
        L->l.played++;
        bool draw = m.winner != 'w' && m.winner != 'b';
        if (draw) L->l.draws++;
        else if ((m.winner == 'b') == (s == 1)) L->l.wins++;
        else L->l.losses++;
        L->l.gf += s ? m.b : m.w;
        L->l.ga += s ? m.w : m.b;
        L->l.scored += m.scored[s][k];
      }
  }
  // Inscritos que aún no han jugado también salen en la ficha.
  if (t.nEntrants)
    for (uint8_t i = 0; i < t.nEntrants; i++) lineOf(playerCode(t.entrants[i]), "?");
  else
    for (uint16_t i = 0; i < t.nTeams; i++)
      for (uint8_t k = 0; k < t.teams[i].n; k++) lineOf(playerCode(t.teams[i].player[k]), "?");
  for (uint8_t i = 0; i < nl; i++) {
    PlayerLine& l = lines[i].l;
    lines[i].mvp10 = (int32_t)(l.wins * 3 + l.draws + (l.champion ? 2 : 0)) * 10 + (int32_t)(l.gf - l.ga) * 5;
    l.mvp = lines[i].mvp10 / 10.0f;
  }
  auto pct = [](const PlayerLine& l) { return l.played ? (double)l.wins / l.played * 100 : 0.0; };
  for (uint8_t i = 1; i < nl; i++) {  // MVP → % de victorias → nombre (estable)
    fc::Line v = lines[i];
    int k = i;
    while (k > 0) {
      const fc::Line& a = lines[k - 1];
      double c = v.mvp10 - a.mvp10;
      if (!c) c = pct(v.l) - pct(a.l);
      if (!c) c = -rg::collate(v.name, a.name);
      if (c <= 0) break;
      lines[k] = lines[k - 1];
      k--;
    }
    lines[k] = v;
  }
  out.nPlayers = nl;
  for (uint8_t i = 0; i < nl; i++) out.players[i] = lines[i].l;
  out.matches = n;
  out.scorersReliable = total > 0 && (double)assigned / total >= SCORER_COVERAGE_MIN_PCT / 100.0;
  if (n) {
    out.startedAt = ms[0].startedAt;
    out.mvp = nl && out.players[0].played ? 0 : -1;
  }
  out.finishedAt = t.finishedAt ? t.finishedAt : n ? ms[n - 1].finishedAt : 0;
  int best = -1, high = -1, longest = -1;
  for (uint16_t i = 0; i < n; i++) {
    int d = ms[i].w > ms[i].b ? ms[i].w - ms[i].b : ms[i].b - ms[i].w;
    int bd = best < 0 ? -1 : (ms[best].w > ms[best].b ? ms[best].w - ms[best].b : ms[best].b - ms[best].w);
    if (best < 0 || d > bd) best = i;
    if (high < 0 || ms[i].w + ms[i].b > ms[high].w + ms[high].b) high = i;
    if (longest < 0 || ms[i].totalMs > ms[longest].totalMs) longest = i;
  }
  if (longest >= 0) TORNEO_CPY(out.longest, ms[longest].id);
  if (best >= 0) TORNEO_CPY(out.biggestWin, ms[best].id);
  if (high >= 0) TORNEO_CPY(out.highestScoring, ms[high].id);
  if (out.scorersReliable) {  // pichichi: el que más goles tiene apuntados (el primero si empatan)
    int top = -1;
    for (uint8_t i = 0; i < nl; i++)
      if (out.players[i].scored > 0 && (top < 0 || out.players[i].scored > out.players[top].scored)) top = i;
    out.topScorer = (int8_t)top;
  }
  // Marcador de la final (o de la serie), siempre con el campeón primero.
  int16_t fin = -1;
  for (uint16_t i = 0; i < t.nFix && fin < 0; i++)
    if (t.fix[i].isFinal) fin = (int16_t)i;
  if (fin < 0 && t.format == Format::Bracket && t.nFix) {
    fin = 0;
    for (uint16_t i = 1; i < t.nFix; i++)
      if (t.fix[i].round > t.fix[fin].round) fin = (int16_t)i;
  }
  if (fin >= 0) {
    const Fixture& f = t.fix[fin];
    bool champFirst = f.winner >= 0 && f.winner == f.blue;
    if (f.hasSeries)
      snprintf(out.finalScore, sizeof(out.finalScore), "%u–%u en la serie", champFirst ? f.serB : f.serW, champFirst ? f.serW : f.serB);
    else if (f.matchId[0])
      for (uint16_t i = 0; i < n; i++)
        if (!strcmp(ms[i].id, f.matchId)) {
          snprintf(out.finalScore, sizeof(out.finalScore), "%u–%u", champFirst ? ms[i].b : ms[i].w, champFirst ? ms[i].w : ms[i].b);
          break;
        }
  }
  free(ms);
}

void summarize(const Tournament& t, Summary& s) {
  memset(&s, 0, sizeof(s));
  TORNEO_CPY(s.id, t.id);
  TORNEO_CPY(s.name, t.name);
  competitionKey(t, s.competitionKey, sizeof(s.competitionKey));
  TORNEO_CPY(s.competition, competitionName(t));
  s.format = t.format;
  s.status = t.status;
  s.edition = t.edition;
  s.createdAt = t.createdAt;
  s.finishedAt = t.finishedAt;
  fixtureProgress(t, s.done, s.total);
  if (t.winner >= 0 && t.winner < (int16_t)t.nTeams) {
    const Team& c = t.teams[t.winner];
    TORNEO_CPY(s.champion, c.name);
    s.nChampion = c.n;
    for (uint8_t i = 0; i < c.n && i < 2; i++) TORNEO_CPY(s.championIds[i], c.player[i]);
  }
  TORNEO_CPY(s.logo, t.logo);
  TORNEO_CPY(s.cup, t.cup);
  TORNEO_CPY(s.banner, t.banner);
  s.ranked = t.ranked;
  Report* r = (Report*)calloc(1, sizeof(Report));
  if (!r) return;
  report(t, *r);
  if (t.status == Status::Finished && r->mvp >= 0) TORNEO_CPY(s.mvpId, r->players[r->mvp].id);
  for (uint8_t i = 0; i < r->nPlayers; i++)
    if (r->players[i].played > 0 && s.nPlayers < MAX_ENTRANTS) TORNEO_CPY(s.players[s.nPlayers++], r->players[i].id);
  free(r);
}

// ---------------------------------------------------------------- ediciones y palmarés
namespace fc {
// Las ediciones (no descartadas) de una competición, de la más antigua a la más nueva (por creación, estable).
static uint16_t editionsOf(const char* key, uint16_t* out, uint16_t max) {
  uint16_t n = 0;
  for (uint16_t i = 0; i < nSummaries && n < max; i++) {
    const Summary& s = summaries[i];
    if (s.status == Status::Cancelled || strcmp(s.competitionKey, key)) continue;
    uint16_t k = n++;
    while (k > 0 && summaries[out[k - 1]].createdAt > s.createdAt) out[k] = out[k - 1], k--;
    out[k] = i;
  }
  return n;
}
static const char* honourName(const char* id) {
  const char* n = playerName ? playerName(id) : nullptr;
  return n ? n : "?";
}
}  // namespace fc

// Todos los resúmenes: los de la SD (eachSummary, del guardado) o, sin él, los de memoria.
namespace fc {
static void forAll(SummaryFn fn, void* arg) {
  if (eachSummary) {
    eachSummary(fn, arg);
    return;
  }
  for (uint16_t i = 0; i < nSummaries; i++)
    if (!fn(summaries[i], arg)) return;
}
}  // namespace fc

// Agente 2: recorre todas las ediciones (SD), no solo las de memoria.
uint16_t nextEdition(const char* key) {
  struct E { uint64_t at; uint16_t edition; uint32_t order; };
  struct Acc { const char* key; E* list; int n, cap; bool ok; } acc = {key, nullptr, 0, 0, true};
  fc::forAll(
      [](const Summary& s, void* a) {
        Acc& x = *(Acc*)a;
        if (s.status == Status::Cancelled || strcmp(s.competitionKey, x.key)) return true;
        if (x.n == x.cap) {
          int nc = x.cap ? x.cap * 2 : 32;
          E* q = (E*)realloc(x.list, (size_t)nc * sizeof(E));
          if (!q) return x.ok = false;
          x.list = q, x.cap = nc;
        }
        x.list[x.n] = E{s.createdAt, s.edition, (uint32_t)x.n};
        x.n++;
        return true;
      },
      &acc);
  // Por creación (estable); los torneos antiguos sin número guardado cuentan por su orden.
  qsort(acc.list, (size_t)acc.n, sizeof(E), [](const void* a, const void* b) {
    const E &x = *(const E*)a, &y = *(const E*)b;
    if (x.at != y.at) return x.at < y.at ? -1 : 1;
    return x.order < y.order ? -1 : x.order > y.order ? 1 : 0;
  });
  uint16_t best = 0;
  for (int i = 0; i < acc.n; i++) {
    uint16_t e = acc.list[i].edition ? acc.list[i].edition : (uint16_t)(i + 1);
    if (e > best) best = e;
  }
  free(acc.list);
  return best + 1;
}

int16_t activeEdition(const char* key) {  // las que están en juego siempre están en memoria
  for (uint16_t i = 0; i < nSummaries; i++)
    if (summaries[i].status == Status::Active && !strcmp(summaries[i].competitionKey, key)) return (int16_t)i;
  return -1;
}

// Agente 2: el cuadro de honor de cada competición, de todas sus ediciones (recorriendo el índice de la SD), con la
// lista de jugadores en PSRAM y sin tope. Las ediciones de `editions` son las recientes que hay en memoria.
namespace fc {
static HonourRow** owned = nullptr;  // los cuadros de la última llamada (se liberan en la siguiente)
static uint8_t nOwned = 0;
}  // namespace fc

uint8_t competitions(Competition* out, uint8_t max) {
  for (uint8_t i = 0; i < fc::nOwned; i++) free(fc::owned[i]);
  free(fc::owned);
  fc::owned = nullptr;
  fc::nOwned = 0;
  if (!summaries || !out || !max) return 0;
  uint16_t* list = (uint16_t*)calloc(nSummaries ? nSummaries : 1, sizeof(uint16_t));
  if (!list) return 0;
  uint8_t nc = 0;
  // Las competiciones: las de memoria (cada una tiene allí sus ediciones más recientes).
  for (uint16_t i = 0; i < nSummaries && nc < max; i++) {
    const Summary& first = summaries[i];
    if (first.status == Status::Cancelled) continue;
    bool seen = false;  // cada competición, una vez (en el orden en que aparece)
    for (uint8_t c = 0; c < nc && !seen; c++) seen = !strcmp(out[c].key, first.competitionKey);
    if (seen) continue;
    Competition& c = out[nc++];
    memset(&c, 0, sizeof(c));
    TORNEO_CPY(c.key, first.competitionKey);
    uint16_t n = fc::editionsOf(c.key, list, nSummaries);
    const Summary& last = summaries[list[n - 1]];
    TORNEO_CPY(c.name, last.competition);
    c.format = last.format;
    for (int16_t k = (int16_t)n - 1; k >= 0 && c.nEditions < RECENT_EDITIONS; k--) c.editions[c.nEditions++] = list[k];  // la más nueva primero
    c.lastChampion = -1;
    for (uint16_t k = 0; k < n; k++)
      if (summaries[list[k]].status == Status::Finished) c.lastChampion = (int16_t)list[k];
  }
  free(list);
  // Todas las ediciones (SD): cuántas hay, escudo/copa/banner de la más nueva que los tenga y el cuadro de honor.
  struct Build { int cap; uint64_t logoAt, cupAt, bannerAt; bool haveLogo, haveCup, haveBanner; };
  Build* b = (Build*)calloc(nc ? nc : 1, sizeof(Build));
  fc::owned = (HonourRow**)calloc(nc ? nc : 1, sizeof(HonourRow*));
  if (!b || !fc::owned) {
    free(b);
    return nc;
  }
  fc::nOwned = nc;
  struct Acc { Competition* out; uint8_t nc; Build* b; } acc = {out, nc, b};
  fc::forAll(
      [](const Summary& s, void* a) {
        Acc& x = *(Acc*)a;
        if (s.status == Status::Cancelled) return true;
        int ci = -1;
        for (uint8_t c = 0; c < x.nc && ci < 0; c++)
          if (!strcmp(x.out[c].key, s.competitionKey)) ci = c;
        if (ci < 0) return true;
        Competition& c = x.out[ci];
        Build& bd = x.b[ci];
        c.totalEditions++;
        auto take = [&](char* dst, const char* src, uint64_t& at, bool& have) {
          if (src[0] && (!have || s.createdAt >= at)) snprintf(dst, 40, "%s", src), at = s.createdAt, have = true;
        };
        take(c.logo, s.logo, bd.logoAt, bd.haveLogo);
        take(c.cup, s.cup, bd.cupAt, bd.haveCup);
        take(c.banner, s.banner, bd.bannerAt, bd.haveBanner);
        auto row = [&](const char* id) -> HonourRow* {
          for (uint16_t h = 0; h < c.nHonours; h++)
            if (!strcmp(c.honours[h].id, id)) return &c.honours[h];
          if (c.nHonours == bd.cap) {
            int ncap = bd.cap ? bd.cap * 2 : 16;
            HonourRow* q = (HonourRow*)realloc(c.honours, (size_t)ncap * sizeof(HonourRow));
            if (!q) return nullptr;
            c.honours = q;
            fc::owned[ci] = q;
            bd.cap = ncap;
          }
          HonourRow& h = c.honours[c.nHonours++];
          memset(&h, 0, sizeof(h));
          TORNEO_CPY(h.id, id);
          return &h;
        };
        for (uint8_t p = 0; p < s.nPlayers; p++)
          if (HonourRow* h = row(s.players[p])) h->editions++;
        for (uint8_t p = 0; p < s.nChampion; p++)
          if (HonourRow* h = row(s.championIds[p])) h->titles++;
        if (s.mvpId[0])
          if (HonourRow* h = row(s.mvpId)) h->mvps++;
        return true;
      },
      &acc);
  free(b);
  for (uint8_t ci = 0; ci < nc; ci++) {  // títulos → MVP → ediciones → nombre (estable)
    Competition& c = out[ci];
    for (uint16_t i2 = 1; i2 < c.nHonours; i2++) {
      HonourRow v = c.honours[i2];
      int k = i2;
      while (k > 0) {
        const HonourRow& a = c.honours[k - 1];
        int d = (int)v.titles - (int)a.titles;
        if (!d) d = (int)v.mvps - (int)a.mvps;
        if (!d) d = (int)v.editions - (int)a.editions;
        if (!d) d = -rg::collate(fc::honourName(v.id), fc::honourName(a.id));
        if (d <= 0) break;
        c.honours[k] = c.honours[k - 1];
        k--;
      }
      c.honours[k] = v;
    }
  }
  // La competición con la edición más reciente primero (por la fecha de esa edición: fin o, si no, creación).
  auto date = [&](const Competition& c) -> uint64_t {
    if (!c.nEditions) return 0;
    const Summary& s = summaries[c.editions[0]];
    return s.finishedAt ? s.finishedAt : s.createdAt;
  };
  Competition* tmp = (Competition*)malloc(sizeof(Competition));
  if (!tmp) return nc;
  for (uint8_t i = 1; i < nc; i++) {
    memcpy(tmp, &out[i], sizeof(Competition));
    uint64_t d = date(*tmp);
    int k = i;
    while (k > 0 && date(out[k - 1]) < d) memcpy(&out[k], &out[k - 1], sizeof(Competition)), k--;
    memcpy(&out[k], tmp, sizeof(Competition));
  }
  free(tmp);
  return nc;
}

}  // namespace torneos

#endif
