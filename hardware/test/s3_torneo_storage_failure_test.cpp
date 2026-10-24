// SD y PSRAM con fallos: no confirmar campeón, duplicar resultados ni borrar antes de guardar.
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
static bool failSnapshotAllocation = false;
static size_t largestUiAllocation = 0;
static void* auditUiAlloc(size_t bytes) {
  if (bytes > largestUiAllocation) largestUiAllocation = bytes;
  return failSnapshotAllocation ? nullptr : malloc(bytes);
}
#define MFV3_UI_ALLOC(bytes) auditUiAlloc(bytes)
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

namespace fsx = std::filesystem;
static int deletions = 0;
static bool (*realRemove)(const char*) = nullptr;
static bool checkedRemove(const char* id) {
  ++deletions;
  torneos::Tournament disk;
  assert(torneos::alloc(disk) && torneos::load(tor::cur.id, disk));
  assert(disk.status == torneos::Status::Active && !disk.fix[0].matchId[0]);
  torneos::release(disk);
  return realRemove(id);
}

// Avería persistente: carpeta con contenido en lugar del temporal; ::remove() no la borra ni en
// Windows ni en Linux (allí sí borraría una carpeta vacía y el fallo desaparecería tras el primer intento).
static bool blockTmp() {
  if (!fsx::create_directory("tournaments/audit-cup.tmp")) return false;
  FILE* f = fopen("tournaments/audit-cup.tmp/x", "wb");
  return f && fclose(f) == 0;
}
static bool unblockTmp() { return fsx::remove_all("tournaments/audit-cup.tmp") > 0; }

static void makeFinal() {
  assert(torneos::alloc(tor::cur));
  torneos::clear(tor::cur);
  auto& t = tor::cur;
  snprintf(t.id, sizeof(t.id), "audit-cup"); snprintf(t.name, sizeof(t.name), "Copa prueba");
  t.format = torneos::Format::Bracket; t.nTeams = 2; t.nFix = 1;
  for (int i = 0; i < 2; ++i) {
    t.teams[i] = torneos::Team{};
    snprintf(t.teams[i].id, 48, "team-%d", i); snprintf(t.teams[i].name, 48, "Equipo %d", i);
    t.teams[i].n = 1; snprintf(t.teams[i].player[0], 40, "player-%d", i);
  }
  t.fix[0] = torneos::Fixture();
  snprintf(t.fix[0].id, sizeof(t.fix[0].id), "final");
  t.fix[0].white = 0; t.fix[0].blue = 1; t.fix[0].isFinal = true;
  tor::curOk = true;
  assert(torneos::save(t));
}

static void assertDiskFinished(bool finished) {
  torneos::Tournament disk;
  assert(torneos::alloc(disk) && torneos::load(tor::cur.id, disk));
  assert((disk.status == torneos::Status::Finished) == finished);
  torneos::release(disk);
}

int main() {
  const auto previous = fsx::current_path();
  const auto root = previous / ("mfv3tf-" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
  assert(fsx::create_directory(root)); fsx::current_path(root);
  assert(fsx::create_directory("matches") && fsx::create_directory("tournaments"));
  partidos::setDir("matches"); torneos::guardado::setDir("tournaments");
  sim::now = 1000; setup(); test.active = false; bootCheckPending = false;
  nube::phase = nube::OFF; s3net::link = s3net::Link::Off;
  makeFinal();
  cfg = mfv3::Config(); cfg.goalsPerPeriod = 1; cfgBeforeTour = cfg;
  tourMatch = true; tourFixture = 0; haveTeams = false;
  startMatch(); eng.skipCountdown(millis()); eng.goal(Team::White, millis());
  resultEndMs = millis();
  assert(blockTmp());
  resultPending = true; resultQueuedAt = millis() - 300;
  loop();
  assert(!resultPending && !resultSaveFailed && tourSavePending && tourResultApplied);
  assert(partidos::count == 1 && matchesPlayed == 1 && tor::cur.nPlayed == 1);
  const std::string matchId = partidos::rows[0].id;
  const fsx::path matchFile = fsx::path("matches") / (matchId + ".json");
  assert(fsx::exists(matchFile)); assertDiskFinished(false);
  const uint32_t until = millis() + 3000;
  while ((int32_t)(until - millis()) > 0) loop();
  victoria::View v; victoryView(v);
  assert(v.tour == 1 && strstr(v.facts, "Torneo sin guardar"));
  assert(tor::cur.nPlayed == 1 && partidos::count == 1 && matchesPlayed == 1);
  backToTournament(); // reintento explícito fallido: no otro partido ni otra aplicación
  assert(inMatch && tourSavePending && tor::cur.nPlayed == 1);
  goHome(); // tampoco descarta el cambio pendiente al pulsar Inicio
  assert(inMatch && tourSavePending && tourMatch && tor::cur.nPlayed == 1);
  assert(unblockTmp());
  backToTournament();
  assert(!inMatch && !tourSavePending && !tourMatch);
  assert(tor::cur.nPlayed == 1 && partidos::count == 1 && matchesPlayed == 1);
  assertDiskFinished(true);

  realRemove = tor::hooks.removeMatch; tor::hooks.removeMatch = checkedRemove;
  const auto fixture = tor::cur.fix[0];
  const auto rules = tor::cur.rules;
  failSnapshotAllocation = true;
  tor::undoYes();
  failSnapshotAllocation = false;
  assert(deletions == 0 && tor::cur.nPlayed == 1 && tor::cur.status == torneos::Status::Finished);
  assert(!memcmp(&fixture, &tor::cur.fix[0], sizeof(fixture)) && fsx::exists(matchFile));

  assert(blockTmp());
  tor::undoYes();
  assert(deletions == 0 && tor::cur.nPlayed == 1 && tor::cur.status == torneos::Status::Finished);
  assert(!memcmp(&fixture, &tor::cur.fix[0], sizeof(fixture)) && fsx::exists(matchFile));
  assertDiskFinished(true);
  tor::edRules = rules; tor::edRules.goals = 17; tor::edFinal = tor::cur.finalRules;
  tor::onRulesSave(nullptr);
  assert(tor::cur.rules.goals == rules.goals && tor::cur.nPlayed == 1);
  assert(unblockTmp());
  tor::store = &jugadores;
  tor::undoYes();
  assert(deletions == 1 && !fsx::exists(matchFile));
  assert(tor::cur.status == torneos::Status::Active && tor::cur.nPlayed == 0);
  assertDiskFinished(false);

  // Final nueva con participantes: undo deja las finales sin equipos hasta recalcular el cuadro.
  makeFinal();
  // Si ni siquiera se guardó el partido, no se aplica al cruce ni se crea otro JSON al volver.
  const uint32_t beforeFailedMatch = matchesPlayed;
  partidos::setDir("matches/missing");
  tourMatch = true; tourFixture = 0; cfgBeforeTour = cfg;
  startMatch(); eng.skipCountdown(millis()); eng.goal(Team::White, millis());
  assert(!saveResult());
  assert(resultSaveFailed && !tourSavedOk && !tourSavePending && tor::cur.nPlayed == 0);
  assert(matchesPlayed == beforeFailedMatch && partidos::count == 0);
  victoryView(v); assert(v.tour == 1 && strstr(v.facts, "Partido sin guardar"));
  goHome(); assert(!inMatch && !tourMatch);
  assert(!fsx::exists("matches/missing"));
  partidos::setDir("matches");

  // Nueva final: reintento con éxito desde Inicio, mismo partido y una sola victoria.
  tourMatch = true; tourFixture = 0; cfgBeforeTour = cfg;
  startMatch(); eng.skipCountdown(millis()); eng.goal(Team::White, millis());
  assert(blockTmp());
  assert(!saveResult() && tourSavePending);
  const int indexedMatches = partidos::count;
  const uint32_t countedMatches = matchesPlayed;
  assert(unblockTmp());
  goHome();
  assert(!tourSavePending && !inMatch && !tourMatch && tor::cur.nPlayed == 1);
  assert(partidos::count == indexedMatches && matchesPlayed == countedMatches);
  assertDiskFinished(true);

  fsx::current_path(previous); fsx::remove_all(root);
  puts("Torneo: fallo visible, reintento sin duplicar, rollback exacto y borrado tras persistir");
  printf("Mayor reserva temporal UI en esta prueba: %zu bytes\n", largestUiAllocation);
}
