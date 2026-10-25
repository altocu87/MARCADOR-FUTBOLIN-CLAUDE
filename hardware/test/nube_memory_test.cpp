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
static std::string posted;  // lo último subido
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
static nube::Resp http(const char* method, const char* url, const char*, const char*, const char* body) {
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
  if (body) posted = body;
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
  nube::apply(); assert(appliedPlayers == 1 && nube::busy());  // un jugador por vuelta (su foto va a la flash)
  drain(); assert(appliedPlayers == 70 && nube::phase == nube::OK);
  assert(!strcmp(nube::saved.lastPull, "2026-10-19T00:00:00Z") && !nube::changes);

  // Sincronización de fondo cortada al volver a usar la placa: acaba tras la petición en curso, sin error, aplica lo
  // que ya llegó y conserva el cursor para repetir la bajada en el siguiente reposo.
  prepare("");
  pulled = "[";
  for (int i = 0; i < 25; ++i) {
    if (i) pulled += ',';
    pulled += "{\"tipo\":\"player\",\"id\":\"q" + std::to_string(i) +
              "\",\"datos\":{\"name\":\"Dos\",\"active\":true},\"actualizado\":\"2026-10-20T00:00:00Z\"}";
  }
  pulled += ']';
  insideGet = false; releaseGet = false;
  nube::jobSaved = nube::saved; nube::job = nube::SYNC;
  nube::background = true; nube::abortJob = false;
  nube::jobDone = false; nube::phase = nube::BUSY; nube::pendingTrimmed = false;
  std::thread bg([] { nube::runJob(); });
  while (!insideGet) std::this_thread::yield();
  assert(nube::networking());
  nube::abortBackground();
  releaseGet = true;
  bg.join();
  assert(!nube::networking() && nube::busy());
  drain();
  assert(nube::lastAborted && nube::phase == nube::IDLE && !nube::message[0]);
  assert(appliedPlayers == 25 && !strcmp(nube::saved.lastPull, "2026-10-18T00:00:00Z"));
  // Lo pedido a mano no se corta.
  nube::background = false; nube::phase = nube::BUSY; nube::jobDone = false; nube::abortJob = false;
  nube::abortBackground(); assert(!nube::abortJob.load());
  nube::jobDone = true; nube::phase = nube::IDLE;
  holdGet = false;
  printf("Nube en reposo: se corta al usar la placa sin perder nada OK\n");

  // Conversión por filas: mismos bytes BMP que antes (cabecera, colores y orientación).
  assert(nube::photoDataUrl(nullptr) == nullptr);
  std::vector<uint16_t> photo(120 * 120);
  for (int pattern = 0; pattern < 3; ++pattern) {
    for (size_t i = 0; i < photo.size(); ++i)
      photo[i] = pattern == 0 ? 0 : pattern == 1 ? 0xffff : (uint16_t)(i * 157 + i / 120);
    char* url = nube::photoDataUrl(photo.data()); assert(url);
    const char* b64 = strchr(url, ',') + 1;
    std::vector<uint8_t> bmp(54 + 120 * 120 * 3 + 3);
    int n = mfv3snd::base64Decode(b64, strlen(b64), bmp.data(), bmp.size());
    assert(n == 43254 && bmp[0] == 'B' && bmp[1] == 'M');
    const uint8_t header[54] = {66,77,246,168,0,0,0,0,0,0,54,0,0,0,40,0,0,0,
      120,0,0,0,120,0,0,0,1,0,24,0,0,0,0,0,192,168,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    assert(!memcmp(bmp.data(), header, 54));
    for (int y = 0; y < 120; ++y) for (int x = 0; x < 120; ++x) {
      uint16_t c = photo[(119 - y) * 120 + x];
      const uint8_t* pixel = bmp.data() + 54 + (y * 120 + x) * 3;
      assert(pixel[0] == (c & 31) * 8 && pixel[1] == ((c >> 5) & 63) * 4 && pixel[2] == (c >> 11) * 8);
    }
    std::vector<uint16_t> roundTrip(photo.size());
    assert(nube::photoFromDataUrl(url, strlen(url), roundTrip.data()) && roundTrip == photo);
    free(url);
  }
  printf("Equipo B: foto BMP/base64 identica, orientacion y vuelta a RGB565 OK\n");

  // Foto inválida y archivo incompleto: no confirmar una descarga que se ha perdido.
  assert(!nube::takeRow(R"({"tipo":"player","id":"bad","datos":{"photo":"data:image/jpeg;base64,AAAA"}})"));
  assert(!nube::takeRow(R"({"tipo":"match","id":"bad","datos":{)"));
  assert(!std::filesystem::exists("../build/equipo-b-cloud-test/bad.json"));

  // 0.22: la primera vez se bajan todos los jugadores; un jugador cambiado aquí (sin tocar su título) se sube con el
  // título, la melodía y el apodo de la cuenta, y lo bajado antes de subir no deshace lo subido.
  static nube::Change got;
  nube::hooks.player = [](const nube::Change& c) { ++appliedPlayers; got = c; };
  prepare("j 00000001\n");
  nube::saved.players = false;
  pulled = R"([{"tipo":"player","id":"placa-00000001","datos":{"name":"Alex","active":true,"titleId":"phoenix","anthem":"arcade","alias":"El Rayo"},"actualizado":"2026-10-19T00:00:00Z"}])";
  nube::sync();
  drain();
  assert(nube::phase == nube::OK && nube::saved.players);
  assert(posted.find("\"titleId\":\"phoenix\"") != std::string::npos && posted.find("\"anthem\":\"arcade\"") != std::string::npos);
  assert(posted.find("\"alias\":\"El Rayo\"") != std::string::npos && posted.find("\"active\":true") != std::string::npos);
  assert(appliedPlayers >= 1 && !strcmp(got.titleId, "phoenix") && got.active);
  // Dado de baja aquí (y entretanto, otra melodía en la app): la baja es de aquí, la melodía la de la app, y lo
  // bajado no lo vuelve a dar de alta.
  assert(players.setExtras(1, true, "phoenix", "arcade", "El Rayo") && players.setActive(1, false));
  prepare("j 00000001\n");
  pulled = R"([{"tipo":"player","id":"placa-00000001","datos":{"name":"Alex","active":true,"titleId":"phoenix","anthem":"funky"},"actualizado":"2026-10-19T00:00:00Z"}])";
  nube::sync();
  drain();
  assert(posted.find("\"active\":false") != std::string::npos && posted.find("\"anthem\":\"funky\"") != std::string::npos);
  assert(!got.active && !strcmp(got.anthem, "funky"));
  printf("Equipo B: 0.22 jugadores: todos una vez, sin borrar título/melodía/apodo de la cuenta OK\n");
  printf("Equipo B: reservas fallidas, cola persistente, cursor, fotos y publicación entre tareas OK\n");
}
