// Reloj de reposo: cada segundo se repinta poco (los dos puntos solo con sus círculos) y lo pintado es igual que si
// se repintara la pantalla entera.
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static long invPx = 0;
// Termina las cifras pedidas; devuelve cuántas vueltas del programa ha costado (cada una, un trozo).
static int drain() {
  int calls = 0;
  while (sevenseg::softBusy()) {
    sevenseg::work();
    calls++;
    assert(calls < 100000);
  }
  return calls;
}
static std::vector<uint8_t> bytes(const lv_draw_buf_t* b) {
  return std::vector<uint8_t>(b->data, b->data + (size_t)b->header.stride * b->header.h);
}
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
  // Al entrar en reposo nada se pinta de golpe: las cifras se piden y salen a trozos.
  reposo::update(18, 30, 10, 6, 17, 10, L);
  lv_refr_now(nullptr);
  assert(sevenseg::softBusy());
  assert(!sevenseg::findGlyph('8', reposo::CLOCK_H, reposo::CYAN, 255, true, true));
  int calls = drain();
  assert(calls > 5 * 30);  // 5 cifras (1, 8, :, 3, 0), cada una en decenas de trozos
  for (char c : {'1', '8', ':', '3', '0'}) assert(sevenseg::findGlyph(c, reposo::CLOCK_H, reposo::CYAN, 255, true, true));
  // Igual punto a punto que pintarla de una vez (glyph): se compara cada cifra con la hecha a la antigua.
  for (char c : {'8', ':', '1'}) {
    std::vector<uint8_t> parts = bytes(sevenseg::want(c, reposo::CLOCK_H, reposo::CYAN, 255, true));
    sevenseg::forget(reposo::CLOCK_H, c == '8' ? ":1" : c == ':' ? "18" : "8:");
    auto t0 = std::chrono::steady_clock::now();
    const lv_draw_buf_t* whole = sevenseg::glyph(c, reposo::CLOCK_H, reposo::CYAN, 255, true, true);
    auto t1 = std::chrono::steady_clock::now();
    assert(whole && bytes(whole) == parts);
    if (c == '8') printf("Cifra de reposo entera: %.1f ms en PC\n", std::chrono::duration<double, std::milli>(t1 - t0).count());
  }
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
  // El minuto siguiente se pinta antes: a las 18:59 ya están el 9 y el 0 de las 19:00, y al cambiar no hay espera.
  reposo::update(18, 59, 20, 6, 17, 10, L);
  drain();
  assert(sevenseg::findGlyph('9', reposo::CLOCK_H, reposo::CYAN, 255, true, true));
  reposo::update(19, 0, 0, 6, 17, 10, L);
  assert(!sevenseg::softBusy() || sevenseg::job.spec.ch != '9');
  for (char c : {'1', '9', ':', '0'}) assert(sevenseg::findGlyph(c, reposo::CLOCK_H, reposo::CYAN, 255, true, true));
  // Lo que se ve, completo: igual que pintado entero.
  drain();
  lv_refr_now(nullptr);
  std::vector<uint16_t> partial(uiport::pcFb, uiport::pcFb + 800 * 480);
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(nullptr);
  for (int i = 0; i < 800 * 480; i++) assert(partial[i] == uiport::pcFb[i]);
  // Al salir del reposo se suelta todo, también lo pedido.
  reposo::update(19, 1, 0, 6, 17, 10, L);
  goHome();
  wakeScreen();
  render();
  sim::now += 10;
  uiport::step();  // la pantalla vieja se borra al cargar la nueva
  assert(!reposo::scr && !sevenseg::softBusy());
  for (char c : {'0', '1', '2', '9'}) assert(!sevenseg::findGlyph(c, reposo::CLOCK_H, reposo::CYAN, 255, true, true));
  puts("Reposo: cifras a trozos e iguales, minuto siguiente adelantado; cada segundo solo los dos puntos, las marcas y los segundos OK");
}
