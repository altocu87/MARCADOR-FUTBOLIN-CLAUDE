// Capturas de la pantalla de 7" en el PC: el mismo sketch con LVGL dibujando en memoria, con las capas y letras de
// build/placa/fat (scripts/placa_recursos.py). Guarda <carpeta>/<escena>.rgb565 (800×480, RGB565) por escena; el
// script scripts/capturas_placa_pc.py las pasa a PNG. Uso: sh test/s3_build.sh 4 test/s3_capturas.cpp
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static void run(uint32_t ms) {
  uint32_t end = sim::now + ms;
  while ((int32_t)(end - sim::now) > 0) loop();
}
static void tap(int x, int y) {
  sim::touchX = x; sim::touchY = y; sim::touchDown = true; run(60);
  sim::touchDown = false; run(60);
}
static void usb(const char* s) { for (const char* c = s; *c; c++) sim::in.push_back(*c); sim::in.push_back('\n'); loop(); }

static const char* OUT = "../build/placa/capturas-pc";
static void shot(const char* name) {
  run(400);  // que acaben los fundidos
  lv_refr_now(nullptr);
  char path[256];
  snprintf(path, sizeof(path), "%s/%s.rgb565", OUT, name);
  FILE* f = fopen(path, "wb");
  assert(f);
  fwrite(uiport::shown, 2, 800 * 480, f);
  fclose(f);
  std::printf("ok  %s\n", name);
}

int main(int argc, char** argv) {
  if (argc > 1) OUT = argv[1];
  sim::now = 1000;
  assets::setRoot("../build/placa/fat");
  setup();
  usb("HORA 2026-10-17 18:30:00");
  tap(400, 430);  // TERMINAR la prueba de la placa
  run(500);
  shot("inicio");
  tap(140, 280);
  shot("inicio-elegida");
  tap(400, 46);
  run(100);
  return 0;
}
