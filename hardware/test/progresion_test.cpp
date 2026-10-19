// La progresión de la placa (s3_progresion.h) tiene que dar lo mismo que la de la app con los partidos fijos de
// tests/fixtures/progresion: lo que calcula la app está en esperado.txt (lo mantiene tests/progresionPlaca.test.ts).
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

int main() {
  std::string all = readFile("../tests/fixtures/progresion/partidos.jsonl");
  std::istringstream in(all);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    partidos::Row r;
    bool ok = partidos::rowFromText(line.c_str(), r);
    assert(ok);
    partidos::push(r);
  }
  // Todos los jugadores que salen en los partidos.
  std::vector<uint32_t> codes;
  for (int i = 0; i < partidos::count; i++)
    for (int t = 0; t < 2; t++)
      for (int k = 0; k < partidos::rows[i].n[t]; k++) codes.push_back(partidos::rows[i].code[t][k]);
  std::sort(codes.begin(), codes.end());
  codes.erase(std::unique(codes.begin(), codes.end()), codes.end());
  progresion::Snapshot s;
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
  // Por partido, en el orden del archivo (el de la app).
  for (int i = 0; i < partidos::count; i++) {
    const partidos::Row& r = partidos::rows[i];
    for (int t = 0; t < 2; t++)
      for (int k = 0; k < r.n[t]; k++) {
        const progresion::Entry& e = s.entries[(size_t)i * 8 + t * 4 + k];
        char b[160], eb[16], ea[16];
        if (e.eloDelta == 0x7FFF) snprintf(eb, sizeof(eb), "-"), snprintf(ea, sizeof(ea), "-");
        else snprintf(eb, sizeof(eb), "%d", e.eloBefore), snprintf(ea, sizeof(ea), "%d", e.eloAfter);
        snprintf(b, sizeof(b), "P %s %08lx %s %s %u %u", r.id, (unsigned long)r.code[t][k], eb, ea, e.xpGained, e.levelAfter);
        out.push_back(b);
      }
  }
  std::string want = readFile("../tests/fixtures/progresion/esperado.txt");
  std::istringstream w(want);
  int bad = 0, i = 0;
  while (std::getline(w, line)) {
    if (i >= (int)out.size() || out[i] != line) {
      if (bad < 8) std::printf("distinto (línea %d)\n  app:   %s\n  placa: %s\n", i + 1, line.c_str(), i < (int)out.size() ? out[i].c_str() : "(nada)");
      bad++;
    }
    i++;
  }
  if (i != (int)out.size()) bad++, std::printf("líneas: app %d, placa %d\n", i, (int)out.size());
  if (bad) {
    std::printf("progresión de la placa: %d diferencias con la app\n", bad);
    return 1;
  }
  std::printf("progresión de la placa igual que la app (%d partidos, %d jugadores)\n", partidos::count, (int)codes.size());
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
