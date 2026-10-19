// Regresiones del equipo B: fallos de reserva, publicación entre tareas y cola persistente.
// Todo el transporte y los datos son locales; no consulta Supabase ni toca una placa.
#include <cassert>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
static int failAfter = -1;
static void* testRealloc(void* p, size_t n) {
  if (failAfter == 0) return nullptr;
  if (failAfter > 0) --failAfter;
  return realloc(p, n);
}
#define MFV3_REALLOC testRealloc
#include "Arduino.h"
#include "mfv3_sound.h"
#include "s3_nube.h"

static int requests = 0, appliedPlayers = 0;
static bool appendDuringUpload = false;
static std::string pulled = "[]";
static std::atomic<bool> holdGet{false}, insideGet{false}, releaseGet{false};
static nube::Resp reply(int code, const std::string& body) {
  nube::Resp r;
  r.code = code;
  r.len = body.size();
  r.body = (char*)malloc(r.len + 1);
  assert(r.body);
  memcpy(r.body, body.c_str(), r.len + 1);
  return r;
}
static nube::Resp http(const char* method, const char* url, const char*, const char*, const char*) {
  ++requests;
  if (strstr(url, "/auth/")) return reply(200, R"({"access_token":"test","refresh_token":"renewed","user":{"id":"u","email":"test@example.test"}})");
  if (!strcmp(method, "GET")) {
    if (holdGet) {
      insideGet = true;
      while (!releaseGet) std::this_thread::yield();
    }
    return reply(200, strstr(url, "offset=0") ? pulled : "[]");
  }
  if (appendDuringUpload) { nube::playerChanged(1); appendDuringUpload = false; }
  return reply(201, "");
}
static const char* pending = "../build/equipo-b-cloud-test/subir.txt";
static std::string readPending() {
  FILE* f = fopen(pending, "rb");
  if (!f) return "";
  std::string out; char b[256];
  while (fgets(b, sizeof(b), f)) out += b;
  fclose(f); return out;
}
static void prepare(const char* lines) {
  assert(nube::phase != nube::BUSY);
  failAfter = -1;
  FILE* f = fopen(pending, "wb"); assert(f);
  fputs(lines, f); fclose(f);
  strcpy(nube::saved.refresh, "old");
  strcpy(nube::saved.lastPull, "2026-10-18T00:00:00Z");
  nube::phase = nube::IDLE;
  requests = appliedPlayers = 0;
  pulled = "[]";
}
static void drain() { while (nube::busy()) nube::apply(); }
int main() {
  std::filesystem::create_directories("../build/equipo-b-cloud-test");
  partidos::setDir("../build/equipo-b-cloud-test");
  fs::FS disk; jug::Store players; players.begin(&disk);
  players.count = 1; players.list[0].code = 1;
  strcpy(players.list[0].name, "Alex");
  nube::begin(&players, "../build/equipo-b-cloud-test");
  nube::http = http;
  nube::hooks.player = [](const nube::Change&) { ++appliedPlayers; };

  int* data = nullptr; int capacity = 0;
  assert(s3mem::reserve(data, capacity, 1)); data[0] = 42;
  int* before = data; int oldCapacity = capacity;
  failAfter = 0;
  assert(!s3mem::reserve(data, capacity, oldCapacity + 1));
  assert(data == before && data[0] == 42 && capacity == oldCapacity);
  free(data);

  // Falla la cola o la copia de jugadores: no hay HTTP ni se pierde lo pendiente.
  for (int fail : {0, 1}) {
    prepare("j 00000001\n"); failAfter = fail;
    nube::sync();
    assert(nube::phase == nube::FAILED && requests == 0);
    assert(!nube::ups && !nube::upPlayers && readPending() == "j 00000001\n");
  }
  // Un segundo cambio del mismo jugador durante la subida debe sobrevivir.
  prepare("j 00000001\n"); appendDuringUpload = true;
  nube::sync();
  assert(nube::phase == nube::BUSY && !strcmp(nube::saved.refresh, "old"));
  drain();
  assert(nube::phase == nube::OK && !strcmp(nube::saved.refresh, "renewed"));
  assert(readPending() == "j 00000001\n");
  assert(!nube::ups && !nube::upPlayers && !nube::changes);

  // Fallo al crear la cola nueva: conservar original; y recuperar una sustitución interrumpida.
  prepare("j 00000001\n"); nube::sync();
  std::filesystem::create_directory(std::string(pending) + ".tmp");
  drain(); assert(readPending() == "j 00000001\n");
  std::filesystem::remove(std::string(pending) + ".tmp");
  std::filesystem::rename(pending, std::string(pending) + ".bak");
  nube::countPending();
  assert(readPending() == "j 00000001\n" && nube::pendingCount == 1);

  // Falla la ampliación de lo recibido: conservar cursor para poder repetir la descarga.
  prepare("");
  pulled = R"([{"tipo":"player","id":"p1","datos":{"name":"Uno","active":true},"actualizado":"2026-10-19T00:00:00Z"}])";
  failAfter = 0; nube::sync(); drain();
  assert(nube::phase == nube::FAILED && !strcmp(nube::saved.lastPull, "2026-10-18T00:00:00Z"));
  assert(appliedPlayers == 0 && !nube::changes);

  // Dos tareas reales del PC: apply no toca la lista mientras el productor puede hacer realloc.
  prepare("");
  pulled = "[";
  for (int i = 0; i < 70; ++i) {
    if (i) pulled += ',';
    pulled += "{\"tipo\":\"player\",\"id\":\"p" + std::to_string(i) +
              "\",\"datos\":{\"name\":\"Uno\",\"active\":true},\"actualizado\":\"2026-10-19T00:00:00Z\"}";
  }
  pulled += ']';
  holdGet = true; insideGet = false; releaseGet = false;
  nube::jobSaved = nube::saved; nube::job = nube::SYNC;
  nube::jobDone = false; nube::phase = nube::BUSY; nube::pendingTrimmed = false;
  std::thread worker([] { nube::runJob(); });
  while (!insideGet) std::this_thread::yield();
  for (int i = 0; i < 100; ++i) nube::apply();
  assert(appliedPlayers == 0 && !strcmp(nube::saved.refresh, "old"));
  releaseGet = true;
  worker.join();
  nube::apply(); assert(appliedPlayers == 4 && nube::busy());
  drain(); assert(appliedPlayers == 70 && nube::phase == nube::OK);
  assert(!strcmp(nube::saved.lastPull, "2026-10-19T00:00:00Z") && !nube::changes);
  holdGet = false;

  // Foto inválida y archivo incompleto: no confirmar una descarga que se ha perdido.
  assert(!nube::takeRow(R"({"tipo":"player","id":"bad","datos":{"photo":"data:image/jpeg;base64,AAAA"}})"));
  assert(!nube::takeRow(R"({"tipo":"match","id":"bad","datos":{)"));
  assert(!std::filesystem::exists("../build/equipo-b-cloud-test/bad.json"));
  printf("Equipo B: reservas fallidas, cola persistente, cursor, fotos y publicación entre tareas OK\n");
}
