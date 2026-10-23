// Guardado sin placa: fallo del índice, partes realmente jugadas y reloj de penaltis.
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include "Arduino.h"
#include "Wire.h"
#include "s3_partidos.h"

namespace fs = std::filesystem;
using namespace mfv3;

struct Store {
  fs::path root, previousDir;
  Store() {
    previousDir = fs::current_path();
    root = previousDir /
           ("mfv3hs-" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    assert(fs::create_directory(root));
    fs::current_path(root);
    partidos::setDir(".");
    partidos::begin(false);
  }
  ~Store() { fs::current_path(previousDir); fs::remove_all(root); }
  fs::path json(int i) const { return root / (std::string(partidos::rows[i].id) + ".json"); }
};

static std::string contents(const fs::path& path) {
  std::ifstream f(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

static Engine winner(Config c) {
  Engine e;
  assert(e.start(c, 0));
  assert(e.skipCountdown(0) == Result::Accepted);
  assert(e.goal(Team::White, 1000) == Result::Accepted);
  assert(e.phase() == Phase::Finished);
  return e;
}

static void index_failure_is_reported_and_json_preserved() {
  Store store;
  Config cfg; cfg.goalsPerPeriod = 1;
  Engine e = winner(cfg);
  partidos::Match m; m.cfg = cfg;
  assert(partidos::save(e, m));
  const fs::path originalPath = store.json(0);
  const std::string original = contents(originalPath);
  const fs::path index = store.root / "indice.txt", backup = store.root / "indice.saved";
  fs::rename(index, backup);
  assert(fs::create_directory(index));  // apertura de índice rechazada, JSON todavía escribible
  assert(!partidos::save(e, m));
  assert(partidos::count == 1);
  assert(contents(originalPath) == original);
  std::string orphan;
  for (const auto& f : fs::directory_iterator(store.root))
    if (f.path().extension() == ".json" && f.path() != originalPath) orphan = f.path().stem().string();
  assert(!orphan.empty());
  partidos::Row r;
  assert(partidos::rowFromJson((store.root / (orphan + ".json")).generic_string().c_str(), r));
  assert(r.w == 1 && r.b == 0);
  assert(fs::remove(index));
  fs::rename(backup, index);
  partidos::begin(false);
  assert(partidos::count == 1 && contents(originalPath) == original);
  // El archivo que no pudo indexarse sigue recuperable sin repetir el partido.
  assert(partidos::addFromFile(orphan.c_str()));
  partidos::begin(false);
  assert(partidos::count == 2);
}

static void both_goal_win_has_no_invented_second_period() {
  Store store;
  Config cfg; cfg.endCondition = EndCondition::Both; cfg.goalsPerPeriod = 1;
  Engine e = winner(cfg);
  partidos::Match m; m.cfg = cfg;
  assert(partidos::save(e, m));
  std::string json = contents(store.json(0));
  const char* periods = partidos::keyIn(json.c_str(), "periods");
  const char* first = partidos::nextIn(periods, nullptr);
  assert(first && !partidos::nextIn(periods, first));
  assert(json.find("\"period\":\"second\"") == std::string::npos);
}

static void shootout_timestamps_follow_overtime() {
  Store store;
  Config cfg; cfg.endCondition = EndCondition::Time; cfg.minutesPerPeriod = 1;
  Engine e; assert(e.start(cfg, 0));
  for (uint32_t t = 0; t < 180000; t += 60000) {
    assert(e.skipCountdown(t) == Result::Accepted);
    e.tick(t + 60000);
    assert(e.next(t + 60000) == Result::Accepted);
  }
  assert(e.phase() == Phase::Penalties);
  for (int i = 0; i < 3; ++i) {
    assert(e.penalty(Team::White, true, 180000) == Result::Accepted);
    assert(e.penalty(Team::Blue, false, 180000) == Result::Accepted);
  }
  assert(e.phase() == Phase::Finished && e.totalTimeMs() == 180000);
  partidos::Match m; m.cfg = cfg;
  assert(partidos::save(e, m));
  std::string json = contents(store.json(0));
  const char* events = partidos::keyIn(json.c_str(), "events");
  uint32_t previous = 0;
  int shootoutEvents = 0;
  for (const char* event = partidos::nextIn(events, nullptr); event; event = partidos::nextIn(events, event)) {
    const uint32_t total = (uint32_t)partidos::numOf(partidos::keyIn(event, "totalTimeMs"));
    assert(total >= previous);
    previous = total;
    char period[20]; partidos::strOf(partidos::keyIn(event, "period"), period, sizeof(period));
    if (!strcmp(period, "shootout")) { assert(total == e.totalTimeMs()); ++shootoutEvents; }
  }
  assert(shootoutEvents == 8 && previous == e.totalTimeMs());
}

int main(int argc, char** argv) {
  try {
  const std::string mode = argc > 1 ? argv[1] : "all";
  if (mode == "all" || mode == "index") index_failure_is_reported_and_json_preserved();
  if (mode == "all" || mode == "periods") both_goal_win_has_no_invented_second_period();
  if (mode == "all" || mode == "penalties") shootout_timestamps_follow_overtime();
  puts("Historial: errores de indice detectados, JSON conservado y fases/tiempos correctos");
  } catch (const std::exception& e) {
    printf("Error en la prueba de historial: %s\n", e.what());
    return 1;
  }
}
