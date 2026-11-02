// Reloj de reposo: cada segundo se repinta poco (los dos puntos solo con sus círculos) y lo pintado es igual que si
// se repintara la pantalla entera.
#include <cassert>
#include <cstdio>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static long invPx = 0;
static void onInv(lv_event_t* e) {
  const lv_area_t* a = (const lv_area_t*)lv_event_get_param(e);
  invPx += (long)lv_area_get_width(a) * lv_area_get_height(a);
}

int main() {
  sim::now = 1000;
  setup();
  test.active = false;
  nube::phase = nube::OFF;
  goHome();
  const char* L = "Último: BLANCO 1 – 0 AZUL";
  reposo::update(18, 30, 10, 6, 17, 10, L);
  lv_refr_now(nullptr);
  lv_display_add_event_cb(lv_display_get_default(), onInv, LV_EVENT_INVALIDATE_AREA, nullptr);
  for (int s = 11; s <= 14; s++) {
    invPx = 0;
    reposo::update(18, 30, (uint8_t)s, 6, 17, 10, L);
    assert(invPx < 25000);  // antes ~42 000 (la columna entera de los dos puntos)
    lv_refr_now(nullptr);
    std::vector<uint16_t> partial(uiport::pcFb, uiport::pcFb + 800 * 480);
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(nullptr);
    for (int i = 0; i < 800 * 480; i++) assert(partial[i] == uiport::pcFb[i]);
  }
  lv_display_remove_event_cb_with_user_data(lv_display_get_default(), onInv, nullptr);
  puts("Reposo: cada segundo solo los dos puntos, las marcas y los segundos, igual que pintado entero OK");
}
