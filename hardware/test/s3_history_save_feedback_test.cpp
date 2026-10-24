// Fallo de SD/índice: victoria visible, aviso duradero y ningún falso guardado en NVS.
#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static void collect(lv_obj_t* o, std::string& out) {
  if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
  if (lv_obj_check_type(o, &lv_label_class)) out += lv_label_get_text(o);
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) collect(lv_obj_get_child(o, i), out);
}

int main() {
  namespace fs = std::filesystem;
  const auto previous = fs::current_path();
  const auto dir = previous / ("mfv3hf-" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
  assert(fs::create_directory(dir));
  fs::current_path(dir);
  partidos::setDir(".");
  fs::create_directory("tournaments"); torneos::guardado::setDir("tournaments");
  sim::now = 1000; setup(); test.active = false; bootCheckPending = false;
  nube::phase = nube::OFF; s3net::link = s3net::Link::Off;
  cfg = mfv3::Config(); cfg.goalsPerPeriod = 1; haveTeams = false;
  startMatch();
  assert(eng.skipCountdown(millis()) == mfv3::Result::Accepted);
  assert(eng.goal(Team::White, millis()) == mfv3::Result::Accepted);
  assert(fs::create_directory(dir / "indice.txt"));
  const auto savedNvs = sim::nvs;
  const uint32_t savedPlayed = matchesPlayed;
  const uint8_t savedCount = historyCount;
  const int savedRows = partidos::count;
  resultPending = true; resultQueuedAt = millis() - 300;
  loop();
  assert(!resultPending && !resultSaving && resultSaveFailed);
  assert(eng.phase() == Phase::Finished && eng.score().white == 1);
  assert(matchesPlayed == savedPlayed && historyCount == savedCount && partidos::count == savedRows);
  assert(sim::nvs == savedNvs);
  const auto fileCount = std::distance(fs::directory_iterator(dir), fs::directory_iterator());
  const uint32_t until = millis() + 3000;
  while ((int32_t)(until - millis()) > 0) loop();
  assert(std::distance(fs::directory_iterator(dir), fs::directory_iterator()) == fileCount); // no reintentos
  render();
  std::string finished; collect(lv_screen_active(), finished);
  assert(finished.find("Partido sin guardar") != std::string::npos);
  goHome(); render();
  std::string home; collect(lv_screen_active(), home); // excluye el aviso temporal de la capa superior
  assert(home.find("Partido sin guardar") != std::string::npos);
  startMatch();
  assert(inMatch && eng.phase() == Phase::Countdown && !resultSaveFailed);
  fs::current_path(previous);
  fs::remove_all(dir);
  puts("Guardado: error visible, victoria conservada, sin NVS falso ni reintentos");
}
