// Jugadores en la pantalla de 7" (s3_jugadores.h): la lista y las fotos que pasa la app, con un disco simulado.
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "Arduino.h"
#include "FS.h"
#include "../esp32s3_pantalla7/s3_jugadores.h"

static std::vector<std::string> out;
static void send(const char* l) { out.push_back(l); }
static bool said(const std::string& s) {
  for (auto& l : out)
    if (l == s) return true;
  return false;
}

static std::string b64(const uint8_t* d, size_t n) {
  static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string s;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
    s += T[v >> 18 & 63];
    s += T[v >> 12 & 63];
    s += i + 1 < n ? T[v >> 6 & 63] : '=';
    s += i + 2 < n ? T[v & 63] : '=';
  }
  return s;
}

int main() {
  fs::FS disk;
  jug::Store st;
  st.begin(&disk);
  assert(st.count == 0 && disk.exists("/jugadores"));

  // La huella de la lista coincide con la que calcula la app (tests/boardPlayers.test.ts).
  assert(st.handle("JUGADORES_INICIO 1", send) && said("JUGADORES_LISTO"));
  st.handle("JUGADOR 0a1b2c3d - Alex|Nv 2|", send);
  st.handle("JUGADORES_FIN 4b387986", send);
  assert(said("JUGADORES_GUARDADOS 1") && st.count == 1 && st.hash == 0x4b387986u);
  out.clear();
  st.handle("JUGADORES?", send);
  assert(said("JUGADORES 1 4b387986"));

  // Lista con tildes y foto; una huella mala no cambia nada.
  out.clear();
  st.handle("JUGADORES_INICIO 2", send);
  st.handle("JUGADOR 0a1b2c3d 11223344 Alex|Nv 23 · ELO 1299|Tricampeón", send);
  st.handle("JUGADOR deadbeef - Lucía|Nv 25|", send);
  st.handle("JUGADORES_FIN 00000000", send);
  assert(said("JUGADORES_ERROR huella") && st.count == 1);

  // Foto: se sube en trozos con su CRC y queda en /jugadores/<código>.bin.
  std::vector<uint8_t> img(jug::PHOTO_BYTES);
  for (size_t i = 0; i < img.size(); i++) img[i] = (uint8_t)(i * 7);
  uint32_t crc = mfv3snd::crc32Update(0, img.data(), img.size());
  char l[600];
  snprintf(l, sizeof(l), "FOTO_SUBIR 0a1b2c3d %u", (unsigned)img.size());
  out.clear();
  st.handle(l, send);
  assert(said("FOTO_LISTO 0a1b2c3d"));
  for (size_t i = 0; i < img.size(); i += 192) {
    size_t n = std::min<size_t>(192, img.size() - i);
    st.handle(("FOTO_DATOS " + b64(&img[i], n)).c_str(), send);
    assert(said("FOTO_OK " + std::to_string(i + n)));
  }
  snprintf(l, sizeof(l), "FOTO_FIN %08lx", (unsigned long)crc);
  st.handle(l, send);
  assert(said("FOTO_GUARDADA 0a1b2c3d") && disk.disk.files["/jugadores/0a1b2c3d.bin"] == img);
  assert(!disk.exists("/jugadores/subida.tmp"));

  // La lista dice que Alex tiene esa foto: se guarda la lista nueva y FOTOS? la anuncia.
  char want[16];
  {
    jug::Player ps[2] = {};
    ps[0].code = 0x0a1b2c3d;
    ps[0].photoCrc = crc;
    snprintf(ps[0].name, sizeof(ps[0].name), "Alex");
    snprintf(ps[0].meta, sizeof(ps[0].meta), "Nv 23 · ELO 1299");
    snprintf(ps[0].title, sizeof(ps[0].title), "Tricampeón");
    ps[1].code = 0xdeadbeef;
    snprintf(ps[1].name, sizeof(ps[1].name), "Lucía");
    snprintf(ps[1].meta, sizeof(ps[1].meta), "Nv 25");
    // Misma cuenta que la placa: FNV-1a de las líneas canónicas.
    uint32_t h = 2166136261u;
    char line[160];
    snprintf(line, sizeof(line), "0a1b2c3d %08lx Alex|Nv 23 · ELO 1299|Tricampeón\ndeadbeef - Lucía|Nv 25|\n", (unsigned long)crc);
    for (const char* c = line; *c; c++) { h ^= (uint8_t)*c; h *= 16777619u; }
    snprintf(want, sizeof(want), "%08lx", (unsigned long)h);
  }
  out.clear();
  st.handle("JUGADORES_INICIO 2", send);
  snprintf(l, sizeof(l), "JUGADOR 0a1b2c3d %08lx Alex|Nv 23 · ELO 1299|Tricampeón", (unsigned long)crc);
  st.handle(l, send);
  st.handle("JUGADOR deadbeef - Lucía|Nv 25|", send);
  st.handle((std::string("JUGADORES_FIN ") + want).c_str(), send);
  assert(said("JUGADORES_GUARDADOS 2") && st.count == 2);
  out.clear();
  st.handle("FOTOS?", send);
  snprintf(l, sizeof(l), "FOTOS 0a1b2c3d:%08lx", (unsigned long)crc);
  assert(said(l));
  char path[64];
  assert(st.photoPath(0x0a1b2c3d, path, sizeof(path), "/ffat") && std::string(path) == "A:/ffat/jugadores/0a1b2c3d.bin");
  assert(!st.photoPath(0xdeadbeef, path, sizeof(path), "/ffat"));

  // Al reiniciar se lee igual de la memoria.
  jug::Store again;
  again.begin(&disk);
  assert(again.count == 2 && again.hash == st.hash && !strcmp(again.find(0xdeadbeef)->name, "Lucía"));
  assert(!strcmp(again.find(0x0a1b2c3d)->title, "Tricampeón"));

  // Foto con datos de más o CRC malo: error y no se guarda nada a medias.
  out.clear();
  st.handle("FOTO_SUBIR deadbeef 99", send);
  assert(said("FOTO_ERROR deadbeef formato"));
  snprintf(l, sizeof(l), "FOTO_SUBIR deadbeef %u", (unsigned)img.size());
  st.handle(l, send);
  st.handle(("FOTO_DATOS " + b64(img.data(), 192)).c_str(), send);
  st.handle("FOTO_FIN 12345678", send);
  assert(said("FOTO_ERROR deadbeef crc") && !disk.exists("/jugadores/deadbeef.bin") && !disk.exists("/jugadores/subida.tmp"));

  // Iniciales como la app.
  char ini[8];
  jug::initials("Alex", ini, sizeof(ini));
  assert(!strcmp(ini, "AL"));
  jug::initials("José María", ini, sizeof(ini));
  assert(!strcmp(ini, "JM"));
  jug::initials("ángel", ini, sizeof(ini));
  assert(!strcmp(ini, "ÁN"));
  std::printf("jugadores de la placa: lista, fotos y huella como la app OK\n");
  return 0;
}
