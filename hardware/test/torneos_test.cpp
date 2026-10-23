// Los torneos de la placa (s3_torneos_reglas.h y s3_torneos_ficha.h) tienen que dar lo mismo que los de la app con
// los torneos fijos de tests/fixtures/torneos (los mantiene tests/torneosPlaca.test.ts): mismos borradores, cruces,
// resultados apuntados, deshacer, cambios de reglas, clasificación con flechas, ranking, carteles, reloj del Pool,
// ficha con MVP y palmarés. Cada paso se compara línea a línea con lo que guardó la app.
// Uso (desde hardware/): g++ -std=c++17 -Itest/mock_s3 -Iesp32s3_pantalla7 test/torneos_test.cpp && ./a.out
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "s3_torneos_ficha.h"

// Lo que pone el guardado (agente 2): aquí, los resúmenes de los torneos jugados en la prueba.
namespace torneos {
Summary* summaries = nullptr;
uint16_t nSummaries = 0;
}  // namespace torneos

using namespace torneos;

// ---------------------------------------------------------------- JSON mínimo (solo para leer las pruebas)
struct J {
  enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<J> a;
  std::vector<std::pair<std::string, J>> o;
  const J& operator[](const char* k) const {
    static J none;
    for (auto& p : o)
      if (p.first == k) return p.second;
    return none;
  }
  const J& operator[](size_t i) const { return a[i]; }
  size_t size() const { return t == Arr ? a.size() : o.size(); }
  bool has(const char* k) const {
    for (auto& p : o)
      if (p.first == k) return true;
    return false;
  }
  const char* c() const { return s.c_str(); }
  uint64_t u() const { return (uint64_t)n; }
};

static void ws(const char*& p) {
  while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
}
static void utf8(std::string& s, uint32_t cp) {
  if (cp < 0x80) s += (char)cp;
  else if (cp < 0x800) s += (char)(0xC0 | cp >> 6), s += (char)(0x80 | (cp & 0x3F));
  else if (cp < 0x10000) s += (char)(0xE0 | cp >> 12), s += (char)(0x80 | ((cp >> 6) & 0x3F)), s += (char)(0x80 | (cp & 0x3F));
  else
    s += (char)(0xF0 | cp >> 18), s += (char)(0x80 | ((cp >> 12) & 0x3F)), s += (char)(0x80 | ((cp >> 6) & 0x3F)),
        s += (char)(0x80 | (cp & 0x3F));
}
static J parse(const char*& p);
static std::string parseStr(const char*& p) {
  std::string s;
  p++;
  while (*p && *p != '"') {
    if (*p == '\\') {
      p++;
      char e = *p++;
      if (e == 'n') s += '\n';
      else if (e == 't') s += '\t';
      else if (e == 'r') s += '\r';
      else if (e == 'b') s += '\b';
      else if (e == 'f') s += '\f';
      else if (e == 'u') {
        uint32_t cp = (uint32_t)strtoul(std::string(p, 4).c_str(), nullptr, 16);
        p += 4;
        if (cp >= 0xD800 && cp < 0xDC00 && p[0] == '\\' && p[1] == 'u') {
          uint32_t lo = (uint32_t)strtoul(std::string(p + 2, 4).c_str(), nullptr, 16);
          p += 6;
          cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        }
        utf8(s, cp);
      } else s += e;
    } else s += *p++;
  }
  p++;
  return s;
}
static J parse(const char*& p) {
  ws(p);
  J j;
  if (*p == '{') {
    j.t = J::Obj;
    p++;
    ws(p);
    if (*p == '}') return p++, j;
    while (true) {
      ws(p);
      std::string k = parseStr(p);
      ws(p);
      p++;  // :
      j.o.push_back({k, parse(p)});
      ws(p);
      if (*p++ == '}') return j;
    }
  }
  if (*p == '[') {
    j.t = J::Arr;
    p++;
    ws(p);
    if (*p == ']') return p++, j;
    while (true) {
      j.a.push_back(parse(p));
      ws(p);
      if (*p++ == ']') return j;
    }
  }
  if (*p == '"') {
    j.t = J::Str;
    j.s = parseStr(p);
    return j;
  }
  if (!strncmp(p, "true", 4)) return p += 4, j.t = J::Bool, j.b = true, j;
  if (!strncmp(p, "false", 5)) return p += 5, j.t = J::Bool, j;
  if (!strncmp(p, "null", 4)) return p += 4, j;
  j.t = J::Num;
  char* end;
  j.n = strtod(p, &end);
  p = end;
  return j;
}
static J load(const std::string& path) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) {
    printf("Falta %s\n", path.c_str());
    exit(1);
  }
  std::string s;
  char buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
  fclose(f);
  const char* p = s.c_str();
  return parse(p);
}

// ---------------------------------------------------------------- jugadores, ids y partidos de la prueba
static std::map<std::string, std::string> names;
static std::map<std::string, int> elos;
static int eloFn(const char* id) { return elos.count(id) ? elos[id] : 1000; }
static const char* nameFn(const char* id) { return names.count(id) ? names[id].c_str() : nullptr; }
static std::map<std::string, int> counters;
static void testId(const char* prefix, char* out, size_t n) { snprintf(out, n, "%s_%d", prefix, ++counters[prefix]); }
static std::vector<MatchLine>* current = nullptr;
static uint16_t source(const char*, MatchLine* out, uint16_t max) {
  uint16_t n = 0;
  for (auto& m : *current)
    if (n < max) out[n++] = m;
  return n;
}

static int failures = 0;
static void compare(const char* where, const std::vector<std::string>& got, const J& want) {
  if (failures) return;  // tras la primera diferencia, lo demás ya no se puede comparar
  size_t n = std::max(got.size(), want.size());
  for (size_t i = 0; i < n; i++) {
    std::string w = i < want.size() ? want[i].s : "(nada)";
    std::string g = i < got.size() ? got[i] : "(nada)";
    if (w != g) {
      printf("DIFERENTE en %s, línea %zu:\n  app:   %s\n  placa: %s\n", where, i + 1, w.c_str(), g.c_str());
      failures++;
      return;
    }
  }
}
static std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
static std::string fmt(const char* f, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof(buf), f, ap);
  va_end(ap);
  return buf;
}
static const char* dash(const char* s) { return s && *s ? s : "-"; }

static End endOf(const std::string& s) { return s == "time" ? End::Time : s == "both" ? End::Both : End::Goals; }
static const char* endText(End e) { return e == End::Time ? "time" : e == End::Both ? "both" : "goals"; }
static std::string rulesText(const Rules& r) { return fmt("%s %u %u %d", endText(r.end), r.goals, r.minutes, r.allowDraw ? 1 : 0); }

static void templateOf(const J& j, Template& t) {
  t = Template();
  TORNEO_CPY(t.id, j["id"].c());
  TORNEO_CPY(t.name, j["name"].c());
  const std::string f = j["format"].s;
  t.format = f == "pool" ? Format::Pool : f == "bracket" ? Format::Bracket : Format::League;
  t.teamSize = (uint8_t)j["teamSize"].n;
  const std::string pr = j["pairing"].s;
  t.pairing = pr == "random" ? Pairing::Random : pr == "manual" ? Pairing::Manual : Pairing::Elo;
  t.seeding = j["seeding"].s == "random" ? Seeding::Random : Seeding::Elo;
  if (j.has("poolSwap")) t.poolSwap = j["poolSwap"].b;
  if (j.has("poolLimit")) t.poolByRounds = j["poolLimit"].s == "rounds";
  if (j.has("poolMinutes")) t.poolMinutes = (uint8_t)j["poolMinutes"].n;
  if (j.has("poolRounds")) t.poolRounds = (uint8_t)j["poolRounds"].n;
  t.finalKind = j["final"].s == "top2" ? FinalKind::Top2 : FinalKind::None;
  t.finalBestOf = (uint8_t)j["finalBestOf"].n;
  t.ranked = j["ranked"].b;
  t.rules.end = endOf(j["endCondition"].s);
  t.rules.goals = (uint8_t)j["goalsPerPeriod"].n;
  t.rules.minutes = (uint8_t)j["minutesPerPeriod"].n;
  bool fg = j["finalGoals"].t == J::Num, fe = j.has("finalEndCondition"), fm = j.has("finalMinutes");
  t.ownFinalRules = fg || fe || fm;
  t.finalRules = t.rules;
  if (fe) t.finalRules.end = endOf(j["finalEndCondition"].s);
  if (fg) t.finalRules.goals = (uint8_t)j["finalGoals"].n;
  if (fm) t.finalRules.minutes = (uint8_t)j["finalMinutes"].n;
  if (j.has("allowDraw")) t.allowDraw = j["allowDraw"].b;
  if (j.has("points")) {
    t.points.win = (uint8_t)j["points"]["win"].n;
    t.points.draw = (uint8_t)j["points"]["draw"].n;
    t.points.loss = (uint8_t)j["points"]["loss"].n;
  }
  if (j.has("legs")) t.legs = (uint8_t)j["legs"].n;
  if (j.has("lastTitle")) TORNEO_CPY(t.lastTitle, j["lastTitle"].c());
}

// ---------------------------------------------------------------- lo que se compara (como snapshot() de la prueba de la app)
static std::string teamName(const Tournament& t, int16_t i) { return i >= 0 && i < (int16_t)t.nTeams ? t.teams[i].name : "-"; }
template <size_t L>
static std::string ids(const char (*p)[L], uint8_t n, const char* sep) {
  std::string s;
  for (uint8_t i = 0; i < n; i++) s += (i ? sep : "") + std::string(p[i]);
  return s;
}
static std::string lineupText(const Lineup& lu) {
  std::string s;
  for (uint8_t c = 0; c < lu.nCourts; c++)
    s += (c ? ";" : "") + ids(lu.court[c][0], 2, "+") + " v " + ids(lu.court[c][1], 2, "+");
  if (lu.bench) s += " | " + ids(lu.benchIds, 2, "+");
  return s;
}

static std::vector<std::string> snapshot(const Tournament& t, uint64_t clockAt) {
  static const char* const STATUS[3] = {"active", "finished", "cancelled"};
  std::vector<std::string> out;
  out.push_back(fmt("T|%s|%s|%s|%s|%s|%llu|%llu|%u|%s|%s|%u|%s", t.id, t.name, STATUS[(int)t.status], teamName(t, t.winner).c_str(),
                    dash(t.finalMatchId), (unsigned long long)t.finishedAt, (unsigned long long)t.createdAt, t.teamSize, dash(t.lastTitle),
                    t.finalKind == FinalKind::Top2 ? "top2" : "none", t.edition, dash(t.competition)));
  out.push_back(fmt("C|%s|%s|%s", rulesText(t.rules).c_str(), t.hasFinalRules ? rulesText(t.finalRules).c_str() : "-",
                    t.hasPoints ? fmt("%u-%u-%u", t.points.win, t.points.draw, t.points.loss).c_str() : "-"));
  if (t.hasLadder) {
    std::string start;
    for (uint8_t i = 0; i < t.ladder.nStart; i++) start += (i ? ";" : "") + ids(t.ladder.start[i], 2, "+");
    out.push_back(fmt("G|%d|%s|%u|%u|%u|%s|%s", t.ladder.swap ? 1 : 0, t.ladder.byRounds ? "rounds" : "time", t.ladder.minutes, t.ladder.rounds,
                      t.ladder.finalBestOf, start.c_str(), ids(t.entrants, t.nEntrants, ",").c_str()));
  }
  for (uint16_t i = 0; i < t.nTeams; i++) {
    const Team& m = t.teams[i];
    out.push_back(fmt("E|%s|%s|%s|%s|%s", m.id, m.name, ids(m.player, m.n, ",").c_str(), dash(m.logo), dash(m.clubId)));
  }
  for (uint16_t i = 0; i < t.nFix; i++) {
    const Fixture& f = t.fix[i];
    std::string next = f.next >= 0 ? fmt("%s/%s", t.fix[f.next].id, f.nextSlot ? "blue" : "white") : "-";
    std::string series = f.hasSeries ? fmt("%u-%u", f.serW, f.serB) : "-";
    std::string ms = f.nMatches ? ids(f.matchIds, f.nMatches, ",") : "-";
    std::string rest = f.nResting ? ids(f.resting, f.nResting, ",") : "-";
    out.push_back(fmt("F|%s|%u|%u|%s|%s|%s|%d|%d|%d|%d|%u|%s|%s|%s|%llu|%s|%s", f.id, f.round, f.court, teamName(t, f.white).c_str(),
                      teamName(t, f.blue).c_str(), teamName(t, f.winner).c_str(), f.draw, f.bye, f.isFinal, f.skipped, f.bestOf, series.c_str(),
                      dash(f.matchId), ms.c_str(), (unsigned long long)f.abandonedAt, rest.c_str(), next.c_str()));
    Phase ph = phaseInfo(t, f);
    out.push_back(fmt("H|%s|%u|%s|%s|%s", f.id, ph.tier, ph.title, dash(ph.sub), rulesText(fixtureRules(t, f)).c_str()));
  }
  int16_t nx = nextFixture(t);
  out.push_back(fmt("N|%s", nx >= 0 ? t.fix[nx].id : "-"));
  uint16_t done, total;
  fixtureProgress(t, done, total);
  out.push_back(fmt("P|%u|%u|%d", done, total, finalSeriesEditable(t) ? 1 : 0));
  std::vector<uint16_t> rounds;
  for (uint16_t i = 0; i < t.nFix; i++)
    if (std::find(rounds.begin(), rounds.end(), t.fix[i].round) == rounds.end()) rounds.push_back(t.fix[i].round);
  std::sort(rounds.begin(), rounds.end());
  for (uint16_t r : rounds) {
    char label[32];
    roundLabel(t, r, label, sizeof(label));
    out.push_back(fmt("L|%u|%s", r, label));
  }
  std::vector<StandingRow> st(t.nTeams + 1);
  uint8_t ns = standings(t, st.data(), (uint8_t)std::min<int>(255, t.nTeams));
  for (uint8_t i = 0; i < ns; i++) {
    const StandingRow& r = st[i];
    out.push_back(fmt("S|%s|%u|%u|%u|%u|%d|%d|%d|%d|%d", t.teams[r.team].name, r.played, r.wins, r.draws, r.losses, r.points, r.gf, r.ga, r.diff,
                      r.move));
  }
  std::vector<Place> pl(t.nTeams + 1);
  uint8_t np = finalRanking(t, pl.data(), (uint8_t)std::min<int>(255, t.nTeams));
  for (uint8_t i = 0; i < np; i++) out.push_back(fmt("R|%u|%s|%d", pl[i].place, t.teams[pl[i].team].name, pl[i].champion ? 1 : 0));
  if (t.hasLadder) {
    Clock c = ladderClock(t, clockAt);
    out.push_back(fmt("K|%d|%d|%d|%d", c.started ? 1 : 0, c.remainingMs, c.avgMs, timeForFinal(t) ? 1 : 0));
    for (uint16_t r : rounds) {
      Lineup lu;
      lineupOfRound(t, r, lu);
      out.push_back(fmt("U|%u|%s", r, lineupText(lu).c_str()));
      bool all = true;
      for (uint16_t i = 0; i < t.nFix; i++)
        if (t.fix[i].round == r && t.fix[i].winner < 0) all = false;
      if (all) {
        arrivalLineup(t, r, lu);
        out.push_back(fmt("A|%u|%s", r, lineupText(lu).c_str()));
      }
    }
    uint16_t round = rounds.back();
    for (uint8_t i = 0; i < t.nEntrants; i++) {
      int8_t mv;
      ladderMoves(t, round, t.entrants[i], mv);
      out.push_back(fmt("M|%u|%s|%d", round, t.entrants[i], mv));
    }
  }
  return out;
}

static std::vector<std::string> reportLines(const Tournament& t) {
  Report r;
  report(t, r);
  std::vector<std::string> out;
  out.push_back(fmt("r|%u|%u|%u|%llu|%llu|%s|%s|%d|%s|%s|%s|%s|%u", r.matches, r.totalGoals, r.playTimeMs, (unsigned long long)r.startedAt,
                    (unsigned long long)r.finishedAt, r.mvp >= 0 ? r.players[r.mvp].id : "-", r.topScorer >= 0 ? r.players[r.topScorer].id : "-",
                    r.scorersReliable ? 1 : 0, dash(r.biggestWin), dash(r.highestScoring), dash(r.finalScore), dash(r.longest), r.extraTime));
  for (uint8_t i = 0; i < r.nPlayers; i++) {
    const PlayerLine& l = r.players[i];
    out.push_back(fmt("l|%s|%u|%u|%u|%u|%d|%d|%d|%d|%.1f", l.id, l.played, l.wins, l.draws, l.losses, l.gf, l.ga, l.scored, l.champion ? 1 : 0,
                      (double)l.mvp));
  }
  return out;
}

static MatchLine matchLine(const J& m) {
  MatchLine l;
  memset(&l, 0, sizeof(l));
  TORNEO_CPY(l.id, m["id"].c());
  l.winner = m["winner"].s[0];
  l.w = (uint8_t)m["w"].n;
  l.b = (uint8_t)m["b"].n;
  l.reason = m["reason"].s[0];
  l.totalMs = (uint32_t)m["totalMs"].n;
  l.startedAt = m["startedAt"].u();
  l.finishedAt = m["finishedAt"].u();
  const char* side[2] = {"white", "blue"};
  for (int s = 0; s < 2; s++) {
    const J& p = m[side[s]];
    l.n[s] = (uint8_t)p.size();
    for (size_t k = 0; k < p.size() && k < 2; k++) {
      l.code[s][k] = playerCode(p[k].c());
      TORNEO_CPY(l.name[s][k], m["names"][side[s]][k].c());
      l.scored[s][k] = (uint8_t)m["scored"][side[s]][k].n;
    }
  }
  l.goalsValid = (uint8_t)m["goalsValid"].n;
  l.goalsAssigned = (uint8_t)m["goalsAssigned"].n;
  return l;
}

// Al abrir un torneo guardado, el guardado (agente 2) lee cruces y equipos del JSON y apunta en `played` los partidos
// de sus cruces con addPlayed, en cualquier orden. Tiene que quedar igual que el torneo jugado y deshacer igual.
static void reload(const std::string& where, const Tournament& t, uint64_t clockAt) {
  Tournament a, b;
  if (!alloc(a) || !alloc(b)) abort();
  auto copy = [&](Tournament& x) {
    Team* tm = x.teams;
    Fixture* fx = x.fix;
    Played* pl = x.played;
    x = t;
    x.teams = tm;
    x.fix = fx;
    x.played = pl;
    memcpy(x.teams, t.teams, t.nTeams * sizeof(Team));
    memcpy(x.fix, t.fix, t.nFix * sizeof(Fixture));
    x.nPlayed = 0;
    for (int i = (int)t.nPlayed - 1; i >= 0; i--) addPlayed(x, t.played[i].fixture, t.played[i].r);
    addPlayed(x, t.played[0].fixture, t.played[0].r);  // repetido: no se apunta dos veces
  };
  copy(a);
  std::vector<std::string> want = snapshot(t, clockAt);
  std::vector<std::string> got = snapshot(a, clockAt);
  if (got != want) {
    printf("DIFERENTE en %s · recargado\n", where.c_str());
    failures++;
  }
  // Deshacer el último en el recargado = deshacer en el original (copia).
  copy(b);
  Tournament c;
  if (!alloc(c)) abort();
  Team* tm = c.teams;
  Fixture* fx = c.fix;
  Played* pl = c.played;
  c = t;
  c.teams = tm, c.fix = fx, c.played = pl;
  memcpy(c.teams, t.teams, t.nTeams * sizeof(Team));
  memcpy(c.fix, t.fix, t.nFix * sizeof(Fixture));
  memcpy(c.played, t.played, t.nPlayed * sizeof(Played));
  char r1[48], r2[48];
  bool u1 = undoLast(b, r1, sizeof(r1)), u2 = undoLast(c, r2, sizeof(r2));
  if (u1 != u2 || strcmp(r1, r2) || snapshot(b, clockAt) != snapshot(c, clockAt)) {
    printf("DIFERENTE en %s · deshacer tras recargar\n", where.c_str());
    failures++;
  }
  release(a);
  release(b);
  release(c);
}

// ---------------------------------------------------------------- un torneo de principio a fin
static const std::string DIR = "../tests/fixtures/torneos/";

static void scenario(const std::string& name, Tournament& t, std::vector<MatchLine>& matches) {
  J j = load(DIR + name + ".json");
  std::string where = name;
  names.clear();
  elos.clear();
  for (size_t i = 0; i < j["players"].size(); i++) {
    names[j["players"][i]["id"].s] = j["players"][i]["name"].s;
    elos[j["players"][i]["id"].s] = (int)j["players"][i]["elo"].n;
  }
  static Template tp;
  templateOf(j["template"], tp);

  char desc[200];
  describeTemplate(tp, desc, sizeof(desc));
  compare((where + " · describeTemplate").c_str(), {desc}, J{J::Arr, false, 0, "", {j["describe"]}, {}});
  std::vector<std::string> fit;
  for (uint8_t n = 2; n <= 17; n++) {
    Fit f = templateFit(tp, n);
    fit.push_back(fmt("fit|%u|%d|%s|%s", n, f.ok ? 1 : 0, dash(f.reason), dash(f.summary)));
  }
  compare((where + " · templateFit").c_str(), fit, j["fit"]);

  // Borrador (draftFromTemplate) con la misma semilla.
  char sel[MAX_ENTRANTS][40];
  uint8_t ns = 0;
  for (size_t i = 0; i < j["selected"].size(); i++) TORNEO_CPY(sel[ns++], j["selected"][i].c());
  char fixed[8][2][40];
  uint8_t nf = 0;
  for (size_t i = 0; i < j["fixedPairs"].size(); i++, nf++)
    for (int k = 0; k < 2; k++) TORNEO_CPY(fixed[nf][k], j["fixedPairs"][i][k].c());
  static Draft d;
  char left[MAX_ENTRANTS][40];
  uint8_t nl = draftFromTemplate(tp, j["tournamentName"].c(), sel, ns, eloFn, j["draftSeed"].c(), fixed, nf, d, left);
  std::string teams;
  for (uint8_t i = 0; i < d.nTeams; i++) teams += (i ? ";" : "") + ids(d.teams[i].player, d.teams[i].n, "+");
  compare((where + " · borrador").c_str(), {fmt("D|%s|%s", nl ? ids(left, nl, ",").c_str() : "-", teams.c_str())}, j["draft"]);
  d.edition = (uint16_t)j["edition"].n;
  TORNEO_CPY(d.competition, j["competition"].c());
  TORNEO_CPY(d.logo, j["logo"].c());
  TORNEO_CPY(d.cup, j["cup"].c());
  TORNEO_CPY(d.banner, j["banner"].c());

  std::vector<std::string> val;
  char err[120];
  val.push_back(fmt("V|%s", validateDraft(d, err, sizeof(err)) ? "-" : err));
  static Draft x;
  x = d;
  TORNEO_CPY(x.name, "   ");
  val.push_back(fmt("V|%s", validateDraft(x, err, sizeof(err)) ? "-" : err));
  x = d;
  x.nTeams = 1;
  val.push_back(fmt("V|%s", validateDraft(x, err, sizeof(err)) ? "-" : err));
  compare((where + " · validateDraft").c_str(), val, j["validate"]);

  counters.clear();
  bool ok = createTournament(d, eloFn, nameFn, j["now"].u(), j["createSeed"].c(), t, err, sizeof(err));
  assert(ok);
  matches.clear();
  current = &matches;
  compare((where + " · creado").c_str(), snapshot(t, j["now"].u() + 60000), j["created"]);

  const J& steps = j["steps"];
  for (size_t i = 0; i < steps.size() && !failures; i++) {
    const J& s = steps[i];
    std::string w = where + fmt(" · paso %zu (%s)", i + 1, s["op"].c());
    const std::string op = s["op"].s;
    if (op == "play") {
      int16_t nx = nextFixture(t);
      if (nx < 0 || s["fixture"].s != t.fix[nx].id) {
        printf("DIFERENTE en %s: siguiente partido %s (app %s)\n", w.c_str(), nx >= 0 ? t.fix[nx].id : "-", s["fixture"].c());
        failures++;
        break;
      }
      MatchLine m = matchLine(s["match"]);
      Result r;
      memset(&r, 0, sizeof(r));
      TORNEO_CPY(r.matchId, m.id);
      r.winner = m.winner;
      r.w = m.w;
      r.b = m.b;
      r.startedAt = m.startedAt;
      r.finishedAt = m.finishedAt;
      bool acc = recordFixtureResult(t, nx, r);
      if (acc != s["accepted"].b) {
        printf("DIFERENTE en %s: apuntado %d (app %d)\n", w.c_str(), acc, s["accepted"].b);
        failures++;
      }
      if (acc) matches.push_back(m);
    } else if (op == "undo") {
      char removed[48];
      bool u = undoLast(t, removed, sizeof(removed));
      if (!u || s["removed"].s != removed) {
        printf("DIFERENTE en %s: deshace %s (app %s)\n", w.c_str(), u ? removed : "-", s["removed"].c());
        failures++;
      }
      for (size_t k = 0; k < matches.size(); k++)
        if (!strcmp(matches[k].id, removed)) matches.erase(matches.begin() + k);
    } else if (op == "abandon") {
      markAbandoned(t, fixtureIndex(t, s["fixture"].c()), s["at"].u());
    } else if (op == "rules") {
      Rules r, fr;
      r.end = endOf(s["rules"]["endCondition"].s);
      r.goals = (uint8_t)s["rules"]["goalsPerPeriod"].n;
      r.minutes = (uint8_t)s["rules"]["minutesPerPeriod"].n;
      bool hasFinal = s["final"].t == J::Obj;
      if (hasFinal) {
        fr.end = endOf(s["final"]["endCondition"].s);
        fr.goals = (uint8_t)s["final"]["goalsPerPeriod"].n;
        fr.minutes = (uint8_t)s["final"]["minutesPerPeriod"].n;
      }
      updateRules(t, r, hasFinal ? &fr : nullptr, (uint8_t)s["bestOf"].n);
    }
    compare(w.c_str(), snapshot(t, s["clockAt"].u()), s["lines"]);
  }
  compare((where + " · ficha").c_str(), reportLines(t), j["report"]);
  reload(where, t, j["steps"][j["steps"].size() - 1]["clockAt"].u());
  if (j["cancel"].b) t.status = Status::Cancelled;
}

int main() {
  makeId = testId;
  playerName = nameFn;
  matchSource = source;
  J pal = load(DIR + "palmares.json");
  size_t n = pal["order"].size();
  std::vector<Tournament> all(n);
  std::vector<std::vector<MatchLine>> matches(n);
  summaries = (Summary*)calloc(MAX_SUMMARIES, sizeof(Summary));
  for (size_t i = 0; i < n && !failures; i++) {
    const std::string name = pal["order"][i].s;
    scenario(name, all[i], matches[i]);
    // Para el palmarés, cada torneo con su id propio (como hace la prueba de la app).
    snprintf(all[i].id, sizeof(all[i].id), "t_%s", name.c_str());
    current = &matches[i];
    summarize(all[i], summaries[nSummaries++]);
    printf("torneos: %s igual que la app\n", name.c_str());
  }
  if (!failures) {
    std::vector<std::string> out;
    Competition* cs = (Competition*)calloc(16, sizeof(Competition));
    uint8_t nc = competitions(cs, 16);
    for (uint8_t i = 0; i < nc; i++) {
      const Competition& c = cs[i];
      std::string eds;
      for (uint8_t k = 0; k < c.nEditions; k++) {
        const Summary& s = summaries[c.editions[k]];
        eds += fmt("%s%s:%u:%s:%s", k ? "," : "", s.id, s.edition ? s.edition : (unsigned)(c.nEditions - k), s.champion[0] ? s.champion : "-",
                   dash(s.mvpId));
      }
      out.push_back(fmt("Q|%s|%s|%s|%s|%s|%s|%s|%s", c.key, c.name, pal["labels"][c.format == Format::Pool ? "pool" : c.format == Format::Bracket ? "bracket" : "league"].c(),
                        dash(c.logo), dash(c.cup), dash(c.banner), eds.c_str(), c.lastChampion >= 0 ? summaries[c.lastChampion].id : "-"));
      for (uint8_t h = 0; h < c.nHonours; h++)
        out.push_back(fmt("h|%s|%s|%u|%u|%u", c.key, c.honours[h].id, c.honours[h].titles, c.honours[h].mvps, c.honours[h].editions));
    }
    std::vector<std::string> keys;
    for (uint16_t i = 0; i < nSummaries; i++)
      if (std::find(keys.begin(), keys.end(), summaries[i].competitionKey) == keys.end()) keys.push_back(summaries[i].competitionKey);
    for (auto& k : keys) {
      int16_t a = activeEdition(k.c_str());
      out.push_back(fmt("X|%s|%u|%s", k.c_str(), nextEdition(k.c_str()), a >= 0 ? summaries[a].id : "-"));
    }
    compare("palmarés", out, pal["lines"]);
    free(cs);
  }
  // Semillas: los mismos números que seededRandom de la app (también con ñ, tildes y letras de más de 16 bits).
  for (size_t i = 0; i < pal["seeds"].size() && !failures; i++) {
    const J& sd = pal["seeds"][i];
    torneos::rg::Rng r(sd["seed"].c());
    for (size_t k = 0; k < sd["values"].size(); k++) {
      double v = r.next();
      if (v != sd["values"][k].n) {
        printf("DIFERENTE seededRandom(\"%s\") n.º %zu: app %.6f, placa %.6f\n", sd["seed"].c(), k + 1, sd["values"][k].n, v);
        failures++;
        break;
      }
    }
  }
  for (auto& t : all) release(t);
  free(summaries);
  if (failures) {
    printf("torneos: %d diferencias con la app\n", failures);
    return 1;
  }
  puts("torneos: reglas, ficha y palmarés de la placa = los de la app");
  return 0;
}
