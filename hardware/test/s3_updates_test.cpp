// Ajustes/OTA reales con LVGL. Red, flash, reinicios y mando simulados; no toca la placa.
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static void shot(const char* name) {
  if (!getenv("MFV3_ASSETS_VERIFY")) return;
  lv_obj_update_layout(gfx.scr);
  lv_draw_buf_t* buf = lv_snapshot_take(gfx.scr, LV_COLOR_FORMAT_RGB888); assert(buf);
  std::string path = std::string("../build/") + name + ".ppm";
  FILE* f = fopen(path.c_str(), "wb"); assert(f);
  fprintf(f, "P6\n%u %u\n255\n", buf->header.w, buf->header.h);
  for (uint32_t y = 0; y < buf->header.h; ++y) for (uint32_t x = 0; x < buf->header.w; ++x) {
    const uint8_t* p = buf->data + y * buf->header.stride + x * 3;
    uint8_t rgb[] = {p[2], p[1], p[0]}; fwrite(rgb, 1, 3, f);
  }
  fclose(f); lv_draw_buf_destroy(buf);
}
static void tapRect(const ui::Rect& r) { ajustesTap(r.x + r.w / 2, r.y + r.h / 2); render(); }
int main() {
  assets::setRoot(getenv("MFV3_ASSETS_VERIFY") ? getenv("MFV3_ASSETS_VERIFY") : "../build/empty-assets");
  sim::now = 1000; setup(); test.active = false; nube::phase = nube::OFF; goHome();
  assert(!strcmp(FW_VERSION, nativeRelease::VERSION));
  updateInfo::Release meta;
  updateInfo::parse("{\"version\":\"0.2\",\"notes\":\"Mas claro\\nSin esperas\"}", meta);
  assert(!strcmp(meta.version, "0.2") && !strcmp(meta.notes, "Mas claro\nSin esperas"));
  updateInfo::parse("{\"build\":1}", meta); assert(!meta.version[0] && !meta.notes[0]);
  updateInfo::parse("{\"version\":\"0.2\",\"notes\":\"truncado", meta); assert(!meta.notes[0]);
  std::string huge = "{\"version\":\"0.2\",\"notes\":\"" + std::string(2000, 'x') + "\"}";
  updateInfo::parse(huge.c_str(), meta); assert(!meta.notes[0]);

  // El adaptador no retiene la dirección de una pantalla destruida por LVGL.
  openSettings(); render(); assert(gfx.active() && gfx.scr);
  ui::show(ui::screen()); assert(gfx.scr == nullptr && !gfx.active());
  openSettings(); render(); shot("actualizaciones-ajustes");
  tapRect(ui::SETTINGS[2].r); assert(screen == Screen::Update && !canInstallUpdate());
  tapRect(ui::UPDATE[2].r); assert(screen == Screen::Update && updateTab == UpdateTab::Overview);
  tapRect(UPDATE_TABS[1]); assert(updateTab == UpdateTab::History && updateHistoryCount() == nativeRelease::COUNT);
  shot("actualizaciones-historial");
  if (getenv("MFV3_ASSETS_VERIFY")) {
    lv_obj_t* scroll = nullptr;
    for (uint32_t i = 0; i < lv_obj_get_child_count(gfx.scr); ++i) {
      auto* child = lv_obj_get_child(gfx.scr, i);
      if (lv_obj_get_width(child) == 740 && lv_obj_get_height(child) == 174) scroll = child;
    }
    assert(scroll && lv_obj_has_flag(scroll, LV_OBJ_FLAG_CLICKABLE));
    sim::touchX = 360; sim::touchY = 320; sim::touchDown = true;
    for (int i = 0; i < 35; ++i) loop();
    for (int y = 320; y >= 220; y -= 5) { sim::touchY = y; for (int i = 0; i < 10; ++i) loop(); }
    sim::touchDown = false; for (int i = 0; i < 35; ++i) loop();
    assert(updateTab == UpdateTab::History && lv_obj_get_scroll_y(scroll) > 0);
  }
  for (int i = 0; i < 30; ++i) tapRect(HISTORY_NEXT);
  assert(updateHistoryPage < updateHistoryCount() && screen == Screen::Update);
  tapRect(UPDATE_TABS[2]); shot("actualizaciones-ayuda");

  strcpy(wifiSsid, "Red de prueba"); checkState = CheckState::Ok;
  pubScreen.build = 400; pubScreen.size = 100;
  strcpy(pubScreen.md5, "0123456789abcdef0123456789abcdef");
  strcpy(publishedRelease.version, "0.2"); strcpy(publishedRelease.notes, "Una version ficticia para comprobar el aviso.");
  updateTab = UpdateTab::Overview; render(); assert(canInstallUpdate()); shot("actualizaciones-disponible");
  inMatch = true; startInstall(); assert(screen == Screen::Update); inMatch = false;
  tapRect(ui::UPDATE[2].r); assert(screen == Screen::Update && updateTab == UpdateTab::Confirm);
  shot("actualizaciones-confirmar");
  tapRect(ui::UPDATE[1].r); assert(updateTab == UpdateTab::Overview && screen == Screen::Update);
  tapRect(ui::UPDATE[2].r); tapRect(ui::UPDATE[2].r); assert(screen == Screen::Install);

  inst = Inst::ScreenDownload; drawInstall();
  auto* bar = updateBar;
  const uint32_t count = lv_obj_get_child_count(gfx.scr);
  for (int i = 0; i <= 100; ++i) instProgress(i, 100);
  for (int i = 0; i < 1000; ++i) instProgress(100, 100);
  assert(updateBar == bar && lv_obj_get_child_count(gfx.scr) == count && lv_bar_get_value(bar) == 100);
  instProgress(500, 100); assert(lv_bar_get_value(bar) == 100);
  instProgress(0, 0); assert(lv_bar_get_value(bar) == 0);
  instProgress(47, 100); shot("actualizaciones-progreso");
  sim::webDown = true;
  installPoll(millis()); assert(inst == Inst::Error && sim::restarts == 0 && sim::selfUpdatedFrom.empty());
  sim::webDown = false;

  // Una descarga larga no consume el tiempo para armar el mando ni oculta «Listo».
  std::string firmware(2048, 'x');
  const std::string digest = sim::fakeMd5(std::vector<uint8_t>(firmware.begin(), firmware.end()));
  pubMando.size = pubScreen.size = (uint32_t)firmware.size();
  strcpy(pubMando.md5, digest.c_str()); strcpy(pubScreen.md5, digest.c_str());
  sim::web[s3net::url(MANDO_ID, (String(MANDO_ID) + "-ota.bin").c_str()).c_str()] = firmware;
  sim::web[s3net::url(S3_BOARD_ID, (String(S3_BOARD_ID) + "-ota.bin").c_str()).c_str()] = firmware;
  sim::webLatencyMs = mfv3::ota::Sender::OFFER_WAIT_MS + 1000;
  inst = Inst::MandoDownload; drawInstall(); installPoll(millis());
  assert(inst == Inst::MandoRadio); installPoll(millis());
  assert(inst == Inst::MandoRadio && sender.state() == mfv3::ota::Sender::State::Offering);
  sim::webLatencyMs = 5000;
  inst = Inst::ScreenDownload; drawInstall(); installPoll(millis());
  assert(inst == Inst::Restart); drawInstall(); installPoll(millis());
  assert(sim::restarts == 0);
  sim::now += 1499; installPoll(millis()); assert(sim::restarts == 0);
  sim::now += 2; installPoll(millis()); assert(sim::restarts == 1);
  sim::webLatencyMs = 0; sim::restarts = 0;

  // Sin saludo nuevo del mando nunca se da por terminada la instalación.
  instMando = true; instScreen = false; inst = Inst::MandoConfirm;
  instAt = millis(); pubMando.build = 50; mandoBuild = 49; mandoInfoAt = instAt - 1;
  installPoll(instAt + 16000); assert(inst == Inst::Error); drawInstall(); shot("actualizaciones-error");
  inst = Inst::MandoConfirm; mandoBuild = 50; mandoInfoAt = instAt + 1;
  installPoll(instAt + 2); assert(inst == Inst::Done); drawInstall(); assert(lv_bar_get_value(updateBar) == 100);
  instMando = instScreen = false; instGoScreen(); assert(inst == Inst::Cancelled);
  drawInstall(); assert(!updateBar);
  printf("Actualizaciones: version, notas, confirmacion, limites, barra estable y confirmacion del mando OK\n");
}
