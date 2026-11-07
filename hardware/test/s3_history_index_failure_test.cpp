// Historial en la SD (MD-02 y MD-03 de la auditoría 0.26): si el índice no se puede escribir al importar o al borrar
// un partido, no se comunica éxito, no se pierde ninguna fila y lo que queda en la tarjeta cuadra tras reiniciar.
// Las averías se simulan con carpetas con contenido en lugar de los archivos (ni ::remove() ni rename() pueden con
// ellas, en Windows ni en Linux).
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
           ("mfv3hi-" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    assert(fs::create_directory(root));
    fs::current_path(root);
    partidos::setDir(".");
    partidos::begin(false);
  }
  ~Store() { fs::current_path(previousDir); fs::remove_all(root); }
};

static bool block(const char* name) {
  if (!fs::create_directory(name)) return false;
  std::ofstream(std::string(name) + "/x") << "x";
  return true;
}
static void unblock(const char* name) { assert(fs::remove_all(name) > 0); }
static std::string json(const std::string& id) { return id + ".json"; }

static Engine winner() {
  Config c; c.goalsPerPeriod = 1;
  Engine e;
  assert(e.start(c, 0));
  assert(e.skipCountdown(0) == Result::Accepted);
  assert(e.goal(Team::White, 1000) == Result::Accepted);
  assert(e.phase() == Phase::Finished);
  return e;
}
static std::string saveOne(const char* tournament = "") {
  Engine e = winner();
  partidos::Match m; m.cfg.goalsPerPeriod = 1;
  snprintf(m.tournament, sizeof(m.tournament), "%s", tournament);
  if (tournament[0]) snprintf(m.fixture, sizeof(m.fixture), "f-%d", partidos::count);
  assert(partidos::save(e, m));
  return partidos::rows[partidos::count - 1].id;
}
static bool listed(const std::string& id) {
  for (int i = 0; i < partidos::count; i++)
    if (id == partidos::rows[i].id) return true;
  return false;
}

// MD-02: el índice rechaza un partido llegado de la nube.
static void import_with_index_failure_is_reported_and_survives_restart() {
  Store store;
  std::string a = saveOne();
  // Un JSON «de la nube» (copia de otro con id nuevo), ya escrito en la tarjeta.
  std::string b = "m_cloud-0000-0000-0000-000000000001";
  {
    std::ifstream in(json(a), std::ios::binary);
    std::string t((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    t.replace(t.find(a), a.size(), b);
    std::ofstream(json(b), std::ios::binary) << t;
  }
  fs::rename("indice.txt", "indice.saved");
  assert(block("indice.txt") && block("indice.txt.tmp"));
  assert(!partidos::addFromFile(b.c_str()));  // no se comunica éxito…
  assert(partidos::count == 2 && listed(b));   // …pero sale en la lista
  unblock("indice.txt"); unblock("indice.txt.tmp");
  fs::rename("indice.saved", "indice.txt");
  partidos::begin(false);  // reinicio: no desaparece
  assert(partidos::count == 2 && listed(a) && listed(b));
  assert(!fs::exists("revisar.txt"));  // ya apuntado en el índice
  partidos::begin(false);
  assert(partidos::count == 2);
  // Repetir la llegada no lo duplica y lo confirma.
  assert(partidos::addFromFile(b.c_str()) && partidos::count == 2);
}

// MD-02: el índice se queda atrás y se pone al día con la siguiente escritura, sin reiniciar.
static void stale_index_is_rewritten_by_next_write() {
  Store store;
  std::string a = saveOne();
  std::string b = "m_cloud-0000-0000-0000-000000000002";
  fs::copy_file(json(a), json(b));
  {  // mismo contenido, otro id
    std::ifstream in(json(b), std::ios::binary);
    std::string t((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    t.replace(t.find(a), a.size(), b);
    std::ofstream(json(b), std::ios::binary | std::ios::trunc) << t;
  }
  fs::rename("indice.txt", "indice.saved");
  assert(block("indice.txt"));
  assert(!partidos::addFromFile(b.c_str()));
  unblock("indice.txt");
  fs::rename("indice.saved", "indice.txt");
  assert(partidos::addFromFile(b.c_str()));  // reintento: ahora sí queda en el índice
  std::string c = saveOne();
  partidos::begin(false);
  assert(partidos::count == 3 && listed(a) && listed(b) && listed(c));
}

// MD-03 a: no se puede abrir el temporal del índice → falla sin borrar el JSON ni la fila.
static void delete_with_unopenable_tmp_keeps_everything() {
  Store store;
  std::string a = saveOne(), b = saveOne();
  assert(block("indice.txt.tmp"));
  assert(!partidos::remove(a.c_str()));
  assert(partidos::count == 2 && listed(a) && fs::exists(json(a)) && !fs::exists(json(a) + ".del"));
  unblock("indice.txt.tmp");
  partidos::begin(false);
  assert(partidos::count == 2 && listed(a) && fs::exists(json(a)));
  // Con la tarjeta bien, el borrado sí se completa y no deja restos.
  assert(partidos::remove(a.c_str()));
  assert(partidos::count == 1 && !listed(a) && !fs::exists(json(a)) && !fs::exists(json(a) + ".del"));
  assert(!fs::exists("revisar.txt"));
  partidos::begin(false);
  assert(partidos::count == 1 && listed(b));
}

// MD-03 b: el temporal se escribe pero no puede sustituir al índice → falla y nada cambia.
static void delete_with_failed_replace_is_not_success() {
  Store store;
  std::string a = saveOne(), b = saveOne();
  assert(block("indice.txt.bak"));
  assert(!partidos::remove(a.c_str()));
  assert(partidos::count == 2 && listed(a) && fs::exists(json(a)));
  unblock("indice.txt.bak");
  partidos::begin(false);
  assert(partidos::count == 2 && listed(a) && listed(b) && fs::exists(json(a)));
}

// Corte de luz a mitad de un borrado: el arranque decide según el índice.
static void restart_mid_delete_is_consistent() {
  Store store;
  std::string a = saveOne(), b = saveOne();
  // Antes de confirmar el índice: el JSON está retirado, el índice aún lo tiene → vuelve.
  fs::rename(json(a), json(a) + ".del");
  std::ofstream("revisar.txt") << "-" << a << "\n";
  partidos::begin(false);
  assert(partidos::count == 2 && listed(a) && fs::exists(json(a)) && !fs::exists(json(a) + ".del"));
  assert(!fs::exists("revisar.txt"));
  // Con el índice ya confirmado: el resto retirado se borra.
  assert(partidos::remove(b.c_str()));
  std::ofstream(json(b) + ".del") << "{}";
  std::ofstream("revisar.txt") << "-" << b << "\n";
  partidos::begin(false);
  assert(partidos::count == 1 && listed(a) && !listed(b) && !fs::exists(json(b) + ".del"));
}

// Anular un torneo: si el índice falla no se borra ningún partido ni se avisa a la cuenta.
static int goneCalls = 0;
static void countGone(const char*) { ++goneCalls; }
static void tournament_removal_is_all_or_nothing() {
  Store store;
  std::string x = saveOne(), t1 = saveOne("t-audit"), t2 = saveOne("t-audit");
  assert(block("indice.txt.tmp"));
  goneCalls = 0;
  assert(partidos::removeTournament("t-audit", countGone) < 0);
  assert(goneCalls == 0 && partidos::count == 3 && fs::exists(json(t1)) && fs::exists(json(t2)));
  unblock("indice.txt.tmp");
  partidos::begin(false);
  assert(partidos::count == 3 && fs::exists(json(t1)) && fs::exists(json(t2)));
  assert(partidos::removeTournament("t-audit", countGone) == 2 && goneCalls == 2);
  assert(partidos::count == 1 && listed(x) && !fs::exists(json(t1)) && !fs::exists(json(t2) + ".del"));
  partidos::begin(false);
  assert(partidos::count == 1 && listed(x));
}

int main(int argc, char** argv) {
  try {
    const std::string mode = argc > 1 ? argv[1] : "all";
    if (mode == "all" || mode == "import") import_with_index_failure_is_reported_and_survives_restart();
    if (mode == "all" || mode == "stale") stale_index_is_rewritten_by_next_write();
    if (mode == "all" || mode == "tmp") delete_with_unopenable_tmp_keeps_everything();
    if (mode == "all" || mode == "replace") delete_with_failed_replace_is_not_success();
    if (mode == "all" || mode == "restart") restart_mid_delete_is_consistent();
    if (mode == "all" || mode == "tournament") tournament_removal_is_all_or_nothing();
    puts("Historial: importar y borrar no fingen exito ni pierden partidos si falla el indice");
  } catch (const std::exception& e) {
    printf("Error en la prueba de indice del historial: %s\n", e.what());
    return 1;
  }
}
