// PRONÓSTICO antes de un clasificatorio (ui_prevision.h) y lo que se lleva cada jugador al acabar (victoria): con los
// datos del modo prueba. Capturas con MFV3_CAPTURAS=<carpeta> (PPM).
#include <cassert>
#include <cstdio>
#include <string>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static void run(unsigned ms) {
  for (unsigned t = 0; t < ms; t += 20) {
    sim::now += 20;
    loop();
  }
}
static void collectText(lv_obj_t* o, std::string& out) {
  if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
  if (lv_obj_check_type(o, &lv_label_class)) {
    out += lv_label_get_text(o);
    out += '|';
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) collectText(lv_obj_get_child(o, i), out);
}
static bool shows(const char* s) {
  std::string t;
  collectText(lv_screen_active(), t);
  return t.find(s) != std::string::npos;
}
static void shot(const char* name) {
  const char* dir = getenv("MFV3_CAPTURAS");
  if (!dir) return;
  run(200);
  lv_obj_update_layout(lv_screen_active());
  lv_draw_buf_t* buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
  assert(buf);
  std::string path = std::string(dir) + "/" + name + ".ppm";
  FILE* f = fopen(path.c_str(), "wb");
  assert(f);
  fprintf(f, "P6\n%u %u\n255\n", buf->header.w, buf->header.h);
  for (uint32_t y = 0; y < buf->header.h; ++y)
    for (uint32_t x = 0; x < buf->header.w; ++x) {
      const uint8_t* p = buf->data + y * buf->header.stride + x * 3;
      uint8_t rgb[] = {p[2], p[1], p[0]};
      fwrite(rgb, 1, 3, f);
    }
  fclose(f);
  lv_draw_buf_destroy(buf);
}

int main() {
  sim::now = 1000;
  sim::sdCard = true;
  setup();
  if (getenv("MFV3_ASSETS")) assets::setRoot(getenv("MFV3_ASSETS"));  // letras reales para las capturas
  test.active = false;
  nube::phase = nube::OFF;
  goHome();
  run(100);
  assert(rtc.set({2026, 10, 20, 18, 0, 0}));  // con hora: el partido nuevo va después de los del modo prueba
  assert(datos::setDemo(true, 1792000000u));
  run(300);
  assert(datos::demo && jugadores.count >= 4 && partidos::count >= 40);

  // Clasificatorio 2 contra 2: tras elegir jugadores sale el pronóstico (no el partido).
  chosenMode = inicio::RANKED;
  cfg = mfv3::Config();
  elegir::Teams t;
  memset(&t, 0, sizeof(t));
  t.n[0] = t.n[1] = 2;
  t.code[0][0] = jugadores.list[0].code, t.code[0][1] = jugadores.list[1].code;
  t.code[1][0] = jugadores.list[2].code, t.code[1][1] = jugadores.list[3].code;
  screen = Screen::Home;
  homeCards = false;  // en la configuración, como al venir de elegir jugadores
  onPlayersDone(t);
  run(200);
  assert(!inMatch && prematchOn && pronostico::active());
  assert(shows("CLASIFICATORIO") && shows("EMPEZAR PARTIDO"));
  assert(pronostico::d && pronostico::d->r.whitePct + pronostico::d->r.bluePct == 100);
  shot("pronostico-2v2");
  // Volver: a elegir jugadores, con los mismos.
  onPrematchBack();
  run(200);
  assert(!prematchOn && choosingPlayers && elegir::active());
  // Otra vez y EMPEZAR PARTIDO: empieza el partido.
  onPlayersDone(t);
  run(200);
  assert(pronostico::active());
  onPrematchStart();
  run(100);
  assert(inMatch && !prematchOn);
  goHome();
  run(100);

  // Clasificatorio hasta el final: debajo de la victoria, la XP, el ELO y el nivel de los cuatro.
  homeCards = false;
  chosenMode = inicio::RANKED;
  cfg = mfv3::Config();
  cfg.endCondition = mfv3::EndCondition::Goals;
  cfg.goalsPerPeriod = 2;
  onPlayersDone(t);
  run(100);
  onPrematchStart();
  run(3200);
  assert(inMatch && eng.phase() == Phase::Playing);
  inputGoal(Team::White);
  run(3200);
  inputGoal(Team::White);
  run(600);
  assert(eng.phase() == Phase::Finished && victoria::active());
  assert(premios::d && premios::d->n == 4 && premios::panel);
  int up = 0, down = 0;
  for (int i = 0; i < premios::d->n; i++) {
    const premios::Row& r = premios::d->row[i];
    assert(r.hasElo && r.xp > 0);
    (r.team == 0 ? up : down) += r.eloDelta > 0;
  }
  assert(up == 2 && down == 0);  // ganan los blancos: suben los dos, bajan los dos azules
  for (int i = 0; i < premios::d->n; i++) assert(premios::d->row[i].levelBefore > 1);  // con su historial
  shot("victoria-premios-2v2");
  goHome();
  run(100);
  assert(!premios::d->n || !victoria::active());

  // Un rápido no pasa por el pronóstico.
  homeCards = false;
  chosenMode = inicio::QUICK;
  onPlayersDone(t);
  run(100);
  assert(inMatch && !prematchOn);
  goHome();
  run(100);

  puts("Pronóstico: antes del clasificatorio, volver a elegir, empezar y nada en el rápido OK");
}
