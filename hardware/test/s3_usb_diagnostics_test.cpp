// Regresión: USB sin lector, diagnóstico apagado y protocolo/capturas íntegros.
// LVGL y sketch reales; el transporte saturado se simula, sin tocar la placa.
#include <cassert>
#include <cstring>
#include <string>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"
int main() {
  sim::now = 1000;
  setup(); test.active = false; nube::phase = nube::OFF;
  sim::out.clear();
  sim::serialSaturated = true;
  uint32_t start = millis();
  // Control: una escritura normal sobre el transporte lleno sí simula la espera del núcleo.
  Serial.print("control");
  assert(millis() - start == 2000 && sim::serialBlockingWrites == 1);
  sim::serialBlockingWrites = 0; start = millis();
  for (int i = 0; i < 1000; ++i) uiport::logCb(LV_LOG_LEVEL_WARN, "Aviso prescindible\n");
  assert(millis() == start && sim::serialBlockingWrites == 0 && sim::out.empty());
  assert(sim::serialTxTimeout == 100);
  uiport::perfEnable(2);
  lv_area_t area = {0, 0, 799, 479};
  lv_display_send_event(uiport::display, LV_EVENT_INVALIDATE_AREA, &area);
  lv_display_send_event(uiport::display, LV_EVENT_RENDER_START, nullptr);
  sim::now += 120;
  lv_display_send_event(uiport::display, LV_EVENT_RENDER_READY, nullptr);
  assert(uiport::perfFrames == 1 && uiport::perfDrawMax == 120000);
  assert(sim::serialBlockingWrites == 0);
  uiport::perfEnable(0);
  sim::serialSaturated = false;
  for (int i = 0; i < 10; ++i) {
    lv_display_send_event(uiport::display, LV_EVENT_INVALIDATE_AREA, &area);
    lv_display_send_event(uiport::display, LV_EVENT_RENDER_START, nullptr);
    sim::now += 120;
    lv_display_send_event(uiport::display, LV_EVENT_RENDER_READY, nullptr);
  }
  assert(sim::out.empty() && uiport::perfFrames == 0 && uiport::perfDrawMax == 0);
  uiport::perfEnable(1);
  lv_display_send_event(uiport::display, LV_EVENT_RENDER_START, nullptr);
  sim::now += 120;
  lv_display_send_event(uiport::display, LV_EVENT_RENDER_READY, nullptr);
  assert(uiport::perfFrames == 1 && sim::out.find("RENDIMIENTO lenta 120 ms") != std::string::npos);
  uiport::perfEnable(0);
  // Volver a Inicio/Configuración/Ajustes sigue atendiendo loop con el USB lleno.
  sim::serialSaturated = true;
  cfg.goalsPerPeriod = 5; cfg.minutesPerPeriod = 5; cfg.endCondition = mfv3::EndCondition::Both;
  auto tap = [](int x, int y) { uiport::injectTouch(x, y); for (int i = 0; i < 110; ++i) loop(); };
  for (int i = 0; i < 20; ++i) {
    goHome(); render(); onModeChosen(inicio::QUICK); render();
    for (int frame = 0; frame < 160; ++frame) loop(); // completar la carga y liberar el táctil de la pantalla anterior
    tap(200, 100); assert(cfg.goalsPerPeriod == 6);
    tap(200, 370); assert(cfg.goalsPerPeriod == 5);
    tap(590, 100); assert(cfg.minutesPerPeriod == 6);
    tap(590, 370); assert(cfg.minutesPerPeriod == 5);
    tap(30, 24); assert(homeCards); render();
    openSettings(); render(); uiport::step();
  }
  assert(sim::serialBlockingWrites == 0 && sim::restarts == 0 && !inMatch);
  // La opción de cuenta queda visible y el cierre no finge éxito mientras hay trabajo.
  strcpy(nube::saved.refresh, "prueba"); strcpy(nube::saved.email, "prueba@example.invalid");
  nube::phase = nube::IDLE; cuenta::show(true);
  lv_obj_update_layout(cuenta::scr);
  assert(cuenta::rememberButton && lv_obj_get_height(cuenta::rememberButton) >= 44);
  nube::phase = nube::BUSY; cuenta::onOutYes(nullptr); assert(nube::loggedIn());
  nube::phase = nube::OFF; memset(&nube::saved, 0, sizeof(nube::saved));
  sim::serialSaturated = false; sim::out.clear();
  handleBoardLine("HELLO");
  assert(sim::out.find("HELLO") != std::string::npos);
  sim::out.clear();
  uiport::sendCapture();
  size_t eol = sim::out.find('\n'); assert(eol != std::string::npos);
  unsigned w, h, bytes, crc;
  assert(sscanf(sim::out.c_str(), "CAPTURA %u %u %u %x", &w, &h, &bytes, &crc) == 4);
  assert(w == 800 && h == 480 && bytes == 768000 && sim::out.size() == eol + 1 + bytes);
  assert(crc == uiport::crc32Update(0, (const uint8_t*)sim::out.data() + eol + 1, bytes));
  assert(sim::serialTxTimeout == 100);
  puts("USB: saturacion sin espera, RENDIMIENTO 0 completo, recorrido sin bloqueo y CAPTURA CRC OK");
}
