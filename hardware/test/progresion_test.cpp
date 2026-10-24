// La progresión de la placa (s3_progresion.h) tiene que dar lo mismo que la de la app con los partidos fijos de
// tests/fixtures/progresion: lo que calcula la app está en esperado.txt (lo mantiene tests/progresionPlaca.test.ts).
// Y con torneos: partidos-torneos.jsonl + torneos.txt (en juego / terminado, partido final y campeones) contra
// esperado-torneos.txt (XP en espera marcada con « d», «Ganar torneo» y los logros de campeón).
// Uso (desde hardware/): g++ -std=c++17 -Itest/mock_s3 -Iesp32s3_pantalla7 test/progresion_test.cpp && ./a.out
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "Arduino.h"
#include "s3_partidos.h"
static int calcAllocations = 0;
static bool failCalcAllocation = false;
static void* countedCalloc(size_t n, size_t size) {
  ++calcAllocations;
  return failCalcAllocation ? nullptr : calloc(n, size);
}
#define calloc countedCalloc
#include "s3_progresion.h"
#undef calloc

static std::string readFile(const char* p) {
  FILE* f = fopen(p, "rb");
  if (!f) {
    std::printf("Falta %s\n", p);
    std::exit(1);
  }
  std::string s;
  char buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
  fclose(f);
  return s;
}

// Partidos de un archivo (uno por línea) en lugar de los que hubiera.
static void loadMatches(const char* path) {
  partidos::count = 0;
  ++partidos::revision;
  std::string all = readFile(path);
  std::istringstream in(all);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    partidos::Row r;
    bool ok = partidos::rowFromText(line.c_str(), r);
    assert(ok);
    partidos::push(r);
  }
}

// Lo que calcula la placa, en el formato de lo esperado.
static std::vector<std::string> board(std::vector<uint32_t>& codes, progresion::Snapshot& s) {
  codes.clear();
  for (int i = 0; i < partidos::count; i++)
    for (int t = 0; t < 2; t++)
      for (int k = 0; k < partidos::rows[i].n[t]; k++) codes.push_back(partidos::rows[i].code[t][k]);
  std::sort(codes.begin(), codes.end());
  codes.erase(std::unique(codes.begin(), codes.end()), codes.end());
  bool ok = progresion::compute(s, codes.data(), (int)codes.size(), nullptr, nullptr, true);
  assert(ok);
  std::vector<std::string> out;
  for (uint32_t c : codes) {
    const progresion::Player& p = s.p[s.find(c)];
    char b[1024];
    int n = snprintf(b, sizeof(b), "%08lx %d %d %d %ld %d %d ", (unsigned long)c, p.elo, p.maxElo, p.rankedPlayed, p.xp, p.level,
                     progresion::categoryFor(p.elo));
    bool any = false;
    for (int a = 0; a < progresion::ACH_COUNT; a++)
      if (p.ach[a >> 3] & (1 << (a & 7))) n += snprintf(b + n, sizeof(b) - n, "%s%s", any ? "," : "", progresion::ACH[a].id), any = true;
    if (!any) snprintf(b + n, sizeof(b) - n, "-");
    out.push_back(b);
  }
  std::sort(out.begin(), out.end());
  for (int i = 0; i < partidos::count; i++) {
    const partidos::Row& r = partidos::rows[i];
    for (int t = 0; t < 2; t++)
      for (int k = 0; k < r.n[t]; k++) {
        const progresion::Entry& e = s.entries[(size_t)i * 8 + t * 4 + k];
        char b[200], eb[16], ea[16];
        if (e.eloDelta == 0x7FFF) snprintf(eb, sizeof(eb), "-"), snprintf(ea, sizeof(ea), "-");
        else snprintf(eb, sizeof(eb), "%d", e.eloBefore), snprintf(ea, sizeof(ea), "%d", e.eloAfter);
        snprintf(b, sizeof(b), "P %s %08lx %s %s %u %u%s", r.id, (unsigned long)r.code[t][k], eb, ea, e.xpGained, e.levelAfter,
                 e.deferred ? " d" : "");
        out.push_back(b);
      }
  }
  return out;
}

static int compare(const std::vector<std::string>& out, const char* expectedPath) {
  std::string want = readFile(expectedPath);
  std::istringstream w(want);
  std::string line;
  int bad = 0, i = 0;
  while (std::getline(w, line)) {
    if (i >= (int)out.size() || out[i] != line) {
      if (bad < 8) std::printf("distinto (línea %d)\n  app:   %s\n  placa: %s\n", i + 1, line.c_str(), i < (int)out.size() ? out[i].c_str() : "(nada)");
      bad++;
    }
    i++;
  }
  if (i != (int)out.size()) bad++, std::printf("líneas: app %d, placa %d\n", i, (int)out.size());
  return bad;
}

// Torneos: «id estado partidoFinal campeones» (como los resúmenes de s3_torneos_datos.h).
static void loadTournaments(const char* path) {
  std::string all = readFile(path);
  std::istringstream in(all);
  std::string line;
  std::vector<progresion::TournamentInfo> list;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    progresion::TournamentInfo t;
    memset(&t, 0, sizeof(t));
    char id[64], st[4], fin[64], champ[64];
    assert(sscanf(line.c_str(), "%63s %3s %63s %63s", id, st, fin, champ) == 4);
    t.id = progresion::idHash(id);
    t.active = st[0] == 'a';
    if (strcmp(fin, "-")) t.finalMatch = progresion::idHash(fin);
    for (char* c = strtok(champ, ","); c && strcmp(c, "-") && t.nChampion < 2; c = strtok(nullptr, ","))
      t.champion[t.nChampion++] = (uint32_t)strtoul(c, nullptr, 16);
    list.push_back(t);
  }
  assert(progresion::setTournaments(list.data(), (int)list.size()));
}

static void withTournaments() {
  loadMatches("../tests/fixtures/progresion/partidos-torneos.jsonl");
  loadTournaments("../tests/fixtures/progresion/torneos.txt");
  int tagged = 0;
  for (int i = 0; i < partidos::count; i++) tagged += partidos::rows[i].tournament[0] != 0;
  assert(tagged > 0);
  std::vector<uint32_t> codes;
  progresion::Snapshot s;
  int bad = compare(board(codes, s), "../tests/fixtures/progresion/esperado-torneos.txt");
  if (bad) {
    std::printf("progresión de la placa con torneos: %d diferencias con la app\n", bad);
    std::exit(1);
  }
  int won = 0;
  for (int i = 0; i < s.n; i++) won += s.p[i].tournamentsWon;
  assert(won > 0);
  // Un cambio en los torneos (el que estaba en juego termina) obliga a recalcular.
  progresion::refresh(codes.data(), (int)codes.size());
  long before = 0;
  for (int i = 0; i < progresion::all.n; i++) before += progresion::all.p[i].xp;
  for (int i = 0; i < progresion::nTournaments; i++) progresion::tournaments[i].active = false;
  progresion::setTournaments(progresion::tournaments, progresion::nTournaments);
  progresion::refresh(codes.data(), (int)codes.size());
  long after = 0;
  for (int i = 0; i < progresion::all.n; i++) after += progresion::all.p[i].xp;
  assert(after > before);  // la XP que esperaba ya se cuenta
  std::printf("progresión de la placa con torneos igual que la app (%d partidos, %d de torneo, %d títulos)\n", partidos::count, tagged, won);
  s.clear();
  progresion::all.clear();
  progresion::allKey.valid = false;
  progresion::setTournaments(nullptr, 0);
}

// Pantalla RETOS (ui_retos.h): retos activos y progreso de cada jugador en los momentos de esperado-retos.txt.
static int compareChallenges() {
  std::string want = readFile("../tests/fixtures/progresion/esperado-retos.txt");
  std::istringstream w(want);
  std::string line;
  int bad = 0, lines = 0;
  while (std::getline(w, line)) {
    char b[512];
    lines++;
    if (line[0] == 'R') {  // R momento clave meta xp texto
      unsigned long t = 0;
      sscanf(line.c_str(), "R %lu", &t);
      progresion::Challenge ch[4];
      int n = progresion::activeChallenges((uint32_t)t, ch);
      bool found = false;
      for (int c = 0; c < n; c++) {
        char txt[64];
        snprintf(txt, sizeof(txt), progresion::TPL_TEXT[ch[c].tpl], (unsigned)ch[c].target);
        snprintf(b, sizeof(b), "R %lu %s %u %d %s", t, ch[c].key, (unsigned)ch[c].target, ch[c].xp, txt);
        found |= line == b;
      }
      if (!found && bad++ < 8) std::printf("reto distinto\n  app: %s\n", line.c_str());
      continue;
    }
    unsigned long t = 0, code = 0;
    sscanf(line.c_str(), "%lu %lx", &t, &code);
    progresion::Challenge ch[4];
    int n = progresion::activeChallenges((uint32_t)t, ch);
    int value[4];
    bool done[4];
    progresion::challengeProgress((uint32_t)code, ch, n, (uint32_t)t, value, done);
    int k = snprintf(b, sizeof(b), "%lu %08lx", t, code);
    for (int c = 0; c < n; c++)
      k += done[c] ? snprintf(b + k, sizeof(b) - k, " ok") : snprintf(b + k, sizeof(b) - k, " %d/%u", value[c], (unsigned)ch[c].target);
    if (line != b && bad++ < 8) std::printf("progreso distinto\n  app:   %s\n  placa: %s\n", line.c_str(), b);
  }
  if (!bad) std::printf("retos de la placa iguales que la app (%d líneas)\n", lines);
  return bad;
}

int main() {
  withTournaments();
  loadMatches("../tests/fixtures/progresion/partidos.jsonl");
  std::string line;
  // Todos los jugadores que salen en los partidos.
  std::vector<uint32_t> codes;
  progresion::Snapshot s;
  int bad = compare(board(codes, s), "../tests/fixtures/progresion/esperado.txt");
  if (bad) {
    std::printf("progresión de la placa: %d diferencias con la app\n", bad);
    return 1;
  }
  std::printf("progresión de la placa igual que la app (%d partidos, %d jugadores)\n", partidos::count, (int)codes.size());
  if (compareChallenges()) {
    std::printf("retos de la placa: distintos de la app\n");
    return 1;
  }
  // Abrir el ranking repetidamente conserva el cálculo y los resultados de la app.
  progresion::refresh(codes.data(), (int)codes.size());
  auto* cached = progresion::all.p;
  int beforeCalc = calcAllocations;
  for (int j = 0; j < 20; ++j) progresion::refresh(codes.data(), (int)codes.size());
  assert(progresion::all.p == cached && calcAllocations == beforeCalc);
  for (uint32_t c : codes) {
    const auto& a = s.p[s.find(c)];
    const auto& b = progresion::all.p[progresion::all.find(c)];
    assert(a.elo == b.elo && a.xp == b.xp && a.level == b.level);
  }
  // Nuevo jugador sin partidos, nueva regla y nueva temporada invalidan cada cálculo pertinente.
  codes.push_back(0xfedcba98);
  progresion::refresh(codes.data(), (int)codes.size());
  assert(progresion::all.find(0xfedcba98) >= 0);
  int oldInitial = progresion::rules.eloInitial;
  progresion::rules.eloInitial += 100;
  progresion::refresh(codes.data(), (int)codes.size());
  assert(progresion::of(0xfedcba98)->elo == oldInitial + 100);
  progresion::rules.eloInitial = oldInitial;
  progresion::refresh(codes.data(), (int)codes.size());
  assert(progresion::of(0xfedcba98)->elo == oldInitial);
  progresion::refreshSeason(codes.data(), (int)codes.size(), 1767225600);  // enero 2026
  auto* seasonCached = progresion::season.p;
  beforeCalc = calcAllocations;
  progresion::refreshSeason(codes.data(), (int)codes.size(), 1767225601);
  assert(progresion::season.p == seasonCached && calcAllocations == beforeCalc);
  progresion::refreshSeason(codes.data(), (int)codes.size(), 1769904000);  // febrero 2026
  assert(!strcmp(progresion::seasonNow, "2026-02"));
  auto extra = partidos::rows[0];
  strcpy(extra.id, "equipo-b-extra");
  uint32_t oldRevision = partidos::revision;
  assert(partidos::push(extra) && partidos::revision != oldRevision);
  progresion::refresh(codes.data(), (int)codes.size());
  assert(progresion::all.rowsCap == partidos::count);
  // Borrado: no reutilizar entradas que todavía contengan el partido retirado.
  std::filesystem::create_directories("../build/equipo-b-ranking-test");
  partidos::setDir("../build/equipo-b-ranking-test");
  partidos::rewriteIndex();
  oldRevision = partidos::revision;
  assert(partidos::remove("equipo-b-extra") && partidos::revision != oldRevision);
  progresion::refresh(codes.data(), (int)codes.size());
  assert(progresion::all.rowsCap == partidos::count);
  // Un cálculo que falla no se considera actualizado; vuelve a intentarse con memoria disponible.
  failCalcAllocation = true;
  progresion::rules.eloInitial += 1;
  progresion::refresh(codes.data(), (int)codes.size());
  assert(!progresion::all.p && !progresion::allKey.valid);
  failCalcAllocation = false;
  progresion::refresh(codes.data(), (int)codes.size());
  assert(progresion::of(0xfedcba98)->elo == oldInitial + 1);
  printf("Equipo B: ranking sin nuevas reservas, invalidacion y recuperacion tras falta de memoria OK\n");
  s.clear();
  progresion::all.clear();
  progresion::season.clear();
  return 0;
}
