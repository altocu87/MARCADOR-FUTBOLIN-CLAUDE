// Capturas de la pantalla de 7" en el PC: el mismo sketch con LVGL dibujando en memoria, con las capas y letras de
// build/placa/fat (scripts/placa_recursos.py). Guarda <carpeta>/<escena>.rgb565 (800×480, RGB565) por escena; el
// script scripts/capturas_placa_pc.py las pasa a PNG. Uso: sh test/s3_build.sh 4 test/s3_capturas.cpp
// Las escenas siguen las mismas jugadas que scripts/capturas-app.mjs, para compararlas una a una.
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
static void shot(const char* name, uint32_t settle = 400) {
  run(settle);
  lv_refr_now(nullptr);
  char path[256];
  snprintf(path, sizeof(path), "%s/%s.rgb565", OUT, name);
  FILE* f = fopen(path, "wb");
  assert(f);
  fwrite(uiport::shown, 2, 800 * 480, f);
  fclose(f);
  std::printf("ok  %s\n", name);
}

// Inicio → RÁPIDO → Configuración (por goles a 5, o por tiempo con 5 min) → CONTINUAR.
static void toMatch(bool time) {
  if (!inicio::active()) {
    goHome();
    run(300);
  }
  tap(140, 280); tap(140, 280);
  run(300);
  cfg.goalsPerPeriod = 5;
  cfg.minutesPerPeriod = 5;
  cfg.endCondition = time ? mfv3::EndCondition::Time : mfv3::EndCondition::Goals;
  goHome();
  run(100);
  tap(140, 280); tap(140, 280);
  run(300);
  tap(400, 443);  // CONTINUAR
}
static void goal(bool white, uint32_t after = 3300) {
  tap(white ? 128 : 671, 186);
  run(after);
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
  tap(140, 280);
  run(300);
  shot("configuracion");
  tap(595, 220);  // también por tiempo
  shot("configuracion-ambas");
  tap(204, 220);  // solo por tiempo
  for (int i = 0; i < 10; i++) tap(595, 104);
  shot("configuracion-tiempo-15");

  // Partido por goles: las mismas jugadas que scripts/capturas-app.mjs.
  toMatch(false);
  shot("cuenta-atras", 1100);
  tap(400, 240);  // saltar
  shot("partido", 1080);
  goal(true, 600);
  shot("partido-gol", 0);
  run(2700);
  for (bool w : {false, true, false, true}) goal(w);
  run(65000);
  shot("partido-marcador", 0);
  goal(true);
  shot("bola-de-partido");  // 4–2: a un gol
  tap(400, 314);            // PAUSA
  shot("pausa");
  tap(400, 260);            // CONTINUAR
  goal(true, 2500);
  shot("victoria");

  // Por tiempo: descanso, prórroga y penaltis.
  toMatch(true);
  run(1500);
  tap(400, 240);
  goal(true);
  goal(false);
  goal(false);
  run(5 * 60000);
  shot("descanso", 0);
  toMatch(true);
  run(1500);
  tap(400, 240);
  goal(true);
  run(5 * 60000);
  tap(400, 326);  // CONTINUAR 2ª PARTE
  run(4000);
  goal(false);
  run(5 * 60000);
  tap(400, 340);  // IR A PRÓRROGA
  run(4000);
  run(20000);
  shot("prorroga", 0);
  run(60000);
  tap(400, 340);  // IR A PENALTIS
  run(500);
  tap(88, 422);   // GOL del Blanco
  run(1000);
  tap(712, 422);  // FALLO del Azul
  shot("penaltis");

  // Reposo: Inicio sin tocar nada durante más de 5 minutos.
  goHome();
  run(SLEEP_AFTER_MS + 2000);
  shot("reposo", 0);
  return 0;
}
