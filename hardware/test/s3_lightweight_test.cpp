// Cinco mejoras ligeras del equipo B; motor/LVGL reales, dispositivos y datos simulados.
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static void checkStreak() {
  uint8_t count = 0, team = 0;
  for (int i = eng.goalCount() - 1; i >= 0; --i) {
    const auto& g = eng.goalAt(i);
    if (g.annulled) continue;
    if (!count) team = (uint8_t)g.team;
    else if (team != (uint8_t)g.team) break;
    ++count;
  }
  partido::View view;
  for (int repeat = 0; repeat < 50; ++repeat) {
    matchView(view, millis());
    assert(view.streak == count && view.streakTeam == team);
  }
}
static void score(Team team) {
  sim::now += 3100;
  inputGoal(team);
  checkStreak();
}
static void captureHome(const char* path) {
  lv_obj_update_layout(inicio::scr);
  lv_draw_buf_t* shot = lv_snapshot_take(inicio::scr, LV_COLOR_FORMAT_RGB888);
  assert(shot);
  FILE* f = fopen(path, "wb"); assert(f);
  fprintf(f, "P6\n%u %u\n255\n", shot->header.w, shot->header.h);
  for (uint32_t y = 0; y < shot->header.h; ++y) for (uint32_t x = 0; x < shot->header.w; ++x) {
    const uint8_t* bgr = shot->data + y * shot->header.stride + x * 3;
    uint8_t rgb[] = {bgr[2], bgr[1], bgr[0]}; fwrite(rgb, 1, 3, f);
  }
  fclose(f); lv_draw_buf_destroy(shot);
}
int main() {
  if (const char* root = getenv("MFV3_ASSETS_VERIFY")) assets::setRoot(root);
  else assets::setRoot("../build/equipo-b-empty-assets");
  sim::now = 1000;
  setup(); test.active = false;
  nube::phase = nube::OFF;
  goHome();
  jugadores.count = 2;
  jugadores.list[0] = {}; jugadores.list[1] = {};
  jugadores.list[0].code = 101; jugadores.list[1].code = 102;
  strcpy(jugadores.list[0].name, "Alex"); strcpy(jugadores.list[1].name, "Bea");
  chosenTeams = {}; chosenTeams.n[0] = chosenTeams.n[1] = 1;
  chosenTeams.code[0][0] = 101; chosenTeams.code[1][0] = 102; haveTeams = true;
  cfg.endCondition = mfv3::EndCondition::Goals; cfg.goalsPerPeriod = 20;
  chosenMode = inicio::QUICK; startMatch(); assert(inMatch);
  onMatchSkip();
  partido::View before, later;
  matchView(before, millis());
  assert(!strcmp(before.cards[0][0].name, "Alex") && !strcmp(before.cond, "Gana quien llegue a 20 goles"));
  matchView(later, millis() + 2000);
  assert(strcmp(before.clock, later.clock) && !memcmp(before.cards, later.cards, sizeof(before.cards)));
  assert(!strcmp(before.info, later.info) && !matchFixedDirty);
  score(Team::White); score(Team::White); score(Team::Blue);
  assert(lastStreak() == 1 && streakTeam == (uint8_t)Team::Blue);
  inputAnnul(Team::Blue); checkStreak(); assert(lastStreak() == 2);
  assert(eng.undo(millis()) == mfv3::Result::Accepted);
  afterChange(false, Team::White); checkStreak(); assert(lastStreak() == 1);
  assert(eng.undo(millis()) == mfv3::Result::Accepted);
  afterChange(false, Team::White); checkStreak(); assert(lastStreak() == 2);
  inputPause(); checkStreak(); inputPause(); checkStreak();
  // Actualización de ficha recibida por el cauce normal de loop: no dejar la copia antigua.
  strcpy(jugadores.list[0].name, "Alex nuevo"); jugadores.changed = true;
  loop(); matchView(later, millis());
  assert(!strcmp(later.cards[0][0].name, "Alex nuevo"));
  goHome(); cfg.goalsPerPeriod = 1; cfg.endCondition = mfv3::EndCondition::Time; cfg.minutesPerPeriod = 1;
  startMatch(); checkStreak(); assert(lastStreak() == 0);
  matchView(later, millis()); assert(strstr(later.info, "1ª PARTE"));
  onMatchSkip(); sim::now += 60001;
  assert(eng.tick(millis())); afterChange(false, Team::White);
  assert(eng.phase() == Phase::PeriodEnd);
  onMatchNext(); matchView(later, millis()); assert(strstr(later.info, "2ª PARTE"));
  goHome();

  // Conservar pestaña, temporada/histórico, modo, jugador y página al regresar desde Inicio.
  partidos::count = 0; ++partidos::revision;
  for (int i = 0; i < 12; ++i) {
    partidos::Row r{}; snprintf(r.id, sizeof(r.id), "b-light-%d", i);
    r.mode = 'q'; r.winner = 'w'; r.w = 5; r.b = 2;
    r.n[0] = r.n[1] = 1; r.code[0][0] = 101; r.code[1][0] = 102;
    strcpy(r.name[0][0], "Alex"); strcpy(r.name[1][0], "Bea");
    assert(partidos::push(r));
  }
  historial::show(&jugadores, true);
  historial::tab = historial::HISTORY; historial::seasonScope = false;
  historial::modeFilter = 'q'; historial::whoFilter = 101; historial::page = 2;
  historial::go(false);
  inicio::show(); historial::show(&jugadores, true);
  assert(historial::tab == historial::HISTORY && !historial::seasonScope);
  assert(historial::modeFilter == 'q' && historial::whoFilter == 101 && historial::page == 2);
  // Si el jugador desaparece se limpia ese filtro; si quedan menos partidos se ajusta la página.
  jugadores.count = 1; jugadores.list[0] = jugadores.list[1];
  partidos::count = 1; ++partidos::revision;
  inicio::show(); historial::show(&jugadores, true);
  assert(historial::whoFilter == 0 && historial::page == 0 && historial::shownCount == 1);
  assert(historial::tab == historial::HISTORY && historial::modeFilter == 'q');

  inicio::show(); inicio::setStatus(false, false, nullptr, 0);
  assert(lv_obj_has_flag(inicio::pendingText, LV_OBJ_FLAG_HIDDEN));
  inicio::setStatus(false, false, nullptr, 1);
  assert(!strcmp(lv_label_get_text(inicio::pendingText), "1 cambio por subir"));
  inicio::setStatus(true, true, "BATERÍA BAJA 15 %", 12);
  for (int i = 0; i < 100; ++i) inicio::setStatus(true, true, "BATERÍA BAJA 15 %", 12);
  assert(!strcmp(lv_label_get_text(inicio::pendingText), "12 cambios por subir"));
  lv_obj_update_layout(inicio::scr);
  lv_area_t rect; lv_obj_get_coords(inicio::pendingText, &rect);
  assert(rect.x1 >= 0 && rect.y1 >= 462 && rect.x2 < 800 && rect.y2 < 480);
  if (getenv("MFV3_ASSETS_VERIFY")) captureHome("../build/inicio-pendientes-equipo-b.ppm");
  inicio::setStatus(false, false, nullptr, 0);
  assert(lv_obj_has_flag(inicio::pendingText, LV_OBJ_FLAG_HIDDEN));
  // El Inicio usa la cola existente y oculta el indicador al salir de la cuenta.
  goHome(); lastActivity = millis();
  strcpy(nube::saved.refresh, "local-test"); nube::pendingCount = 7;
  render();
  assert(!strcmp(lv_label_get_text(inicio::pendingText), "7 cambios por subir"));
  nube::saved.refresh[0] = 0; render();
  assert(lv_obj_has_flag(inicio::pendingText, LV_OBJ_FLAG_HIDDEN));
  printf("Equipo B: fichas, reloj, rachas/undo, cambio de parte, filtros y pendientes OK (cache fija %zu B)\n", sizeof(matchFixed));
}
