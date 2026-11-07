// Equipo B: regresiones de LVGL y red con el sketch real y transporte simulado.
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static int styleChanges = 0;
static void onStyle(lv_event_t*) { ++styleChanges; }
static void layerFile(const char* name, uint32_t raw, bool truncated = false, bool alpha = false) {
  uint8_t h[16] = {LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565, 0, 0, 2, 0, 1, 0, 4};
  if (alpha) h[1] = LV_COLOR_FORMAT_RGB565A8;
  for (int i = 0; i < 4; ++i) h[12 + i] = (uint8_t)(raw >> (8 * i));
  std::string path = std::string(assets::ROOT) + '/' + name;
  FILE* f = fopen(path.c_str(), "wb"); assert(f);
  assert(fwrite(h, 1, 16, f) == 16);
  uint8_t data[] = {(uint8_t)(alpha ? 0x60 : 0x40), 0x00, 0xf8, 0xe0, 0x07, 0x80, 0xff};
  size_t n = truncated ? 3 : alpha ? 7 : 5;
  assert(fwrite(data, 1, n, f) == n);
  fclose(f);
}
int main() {
  std::filesystem::create_directories("../build/equipo-b-assets-test");
  assets::setRoot("../build/equipo-b-assets-test");
  sim::now = 1000;
  setup();
  test.active = false;
  // LZ4 válido, reutilización exacta, tamaño falso, truncado y protección de imágenes activas.
  layerFile("ok.lz4", 4);
  const lv_image_dsc_t* d = assets::loadLz4("ok.lz4");
  assert(d && d->data_size == 4 && ((const uint16_t*)d->data)[0] == 0xf800);
  layerFile("alpha.lz4", 6, false, true);
  const lv_image_dsc_t* a = assets::loadLz4("alpha.lz4");
  assert(a && a->header.cf == LV_COLOR_FORMAT_RGB565A8 && a->data_size == 6 && a->data[4] == 0x80);
  size_t bytes = assets::cachedBytes;
  assert(assets::loadLz4("ok.lz4") == d && assets::cachedBytes == bytes);
  layerFile("bad.lz4", 0xffffffff);
  layerFile("short.lz4", 4, true);
  assert(!assets::loadLz4("bad.lz4") && !assets::loadLz4("short.lz4"));
  assert(assets::cachedBytes == bytes && !assets::evictOne());
  assets::newScreen();
  assert(assets::evictOne());
  while (assets::evictOne()) {}
  assert(assets::cachedBytes == 0);

  // El mismo hándicap no vuelve a invalidar el estilo de la etiqueta cada vuelta.
  partido::View v{};
  v.mode = 1; v.phase = partido::P_PLAYING; v.hcButton = true;
  v.hcLeftMs = 5000; v.hcSeq = 1;
  strcpy(v.clock, "02:00");
  partido::update(v);
  lv_obj_add_event_cb(partido::hcBtnTime, onStyle, LV_EVENT_STYLE_CHANGED, nullptr);
  for (int i = 0; i < 100; ++i) partido::update(v);
  assert(styleChanges == 0);
  v.hcLeftMs = -1; partido::update(v); assert(styleChanges == 1);
  v.hcLeftMs = 4000; partido::update(v); assert(styleChanges == 2);
  v.hcButton = false; partido::update(v);
  v.hcButton = true; partido::update(v); assert(styleChanges == 3);

  // Dos segundos buscando redes: loop y los temporizadores siguen avanzando.
  sim::networks = {{"Casa", "clave", -40}, {"Casa", "clave", -50}, {"Otra", "", -70}};
  sim::scanDelayMs = 2000;
  screen = Screen::Settings;
  s3net::startScan();
  uint32_t began = millis(); int loops = 0;
  while (millis() - began < 1000) { loop(); ++loops; }
  assert(loops > 100 && s3net::scanning);
  // Inicio de partido y otras conexiones esperan al fin de la búsqueda.
  startMatch(); assert(!inMatch);
  while (s3net::scanning) loop();
  assert(netCount == 2 && !strcmp(nets[0].ssid, "Casa"));
  assert(!sim::scanActive);

  // Un resultado de versiones cancelado no sobrescribe datos ni cambia el estado del partido.
  checkState = CheckState::Running;
  assert(beginVersions());
  assert(versionsRunning && versionsDone);
  pubScreen.build = 123;
  prepareMatchNetwork();
  finishVersions();
  assert(!versionsRunning && checkState == CheckState::Never && pubScreen.build == 123);
  assert(s3net::link == s3net::Link::Off);
  // Opcional: leer recursos ya generados, sin modificarlos ni generarlos durante la prueba.
  if (const char* root = getenv("MFV3_ASSETS_VERIFY")) {
    assets::setRoot(root); int layers = 0;
    for (const auto& file : std::filesystem::recursive_directory_iterator(root)) {
      if (file.path().extension() != ".lz4") continue;
      std::string rel = std::filesystem::relative(file.path(), root).generic_string();
      assets::newScreen();
      assert(assets::loadLz4(rel.c_str()));
      ++layers;
    }
    printf("Equipo B: %d capas existentes leidas y descomprimidas correctamente\n", layers);
  }
  // Penaltis: sin cambios no se invalida nada (el repintado continuo dejaba sin alimentar el audio).
  penaltis::View pv;
  memset(&pv, 0, sizeof(pv));
  pv.active = true; pv.rounds = 5;
  penaltis::update(pv);
  penaltis::update(pv);
  lv_display_add_event_cb(lv_display_get_default(), onStyle, LV_EVENT_INVALIDATE_AREA, nullptr);
  styleChanges = 0;
  for (int i = 0; i < 100; ++i) penaltis::update(pv);
  assert(styleChanges == 0);
  pv.turn = 1; penaltis::update(pv);
  assert(styleChanges > 0);
  // El foco late a saltos: dentro del mismo escalón no repinta.
  penaltis::spotCb(penaltis::spot[1], 200);
  styleChanges = 0;
  penaltis::spotCb(penaltis::spot[1], 205); assert(styleChanges == 0);
  penaltis::spotCb(penaltis::spot[1], 230); assert(styleChanges > 0);
  // Victoria: pasar a «VER CAMPEÓN» con la pantalla ya enseñada la rehace una sola vez (antes, en cada vuelta).
  victoria::View vv;
  memset(&vv, 0, sizeof(vv));
  victoria::update(vv);
  vv.tour = 2;
  victoria::update(vv);
  lv_obj_t* champion = victoria::scr;
  assert(champion && lv_screen_active() == champion);
  victoria::update(vv);
  assert(victoria::scr == champion && lv_screen_active() == champion);
  printf("Equipo B: LZ4, cache activa, repintados, busqueda asincrona y cancelacion de versiones OK\n");
}
