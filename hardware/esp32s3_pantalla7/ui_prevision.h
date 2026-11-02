// MARCADOR FUTBOLÍN V3 · PRONÓSTICO antes de un clasificatorio (PrematchScreen.tsx de la app, en sencillo).
// -------------------------------------------------------------------------------------------------
// Tras elegir los jugadores de un clasificatorio (fuera de torneos: allí ya sale en la tarjeta del cruce):
//   - Arriba, volver (a elegir jugadores) y CLASIFICATORIO.
//   - Los dos equipos con sus fotos y nombres, y debajo de cada uno su % de victoria con una barra.
//   - Debajo, los últimos enfrentamientos entre estas mismas alineaciones (fecha y marcador; el ganador en verde).
//   - EMPEZAR PARTIDO, grande, abajo.
// Sin clasificatorios de nadie no hay %: lo dice y se puede jugar igual. Las cuentas, en s3_prevision.h (iguales que
// la app: tests/fixtures/progresion/esperado-prevision.txt). Todo lo de dentro es una sola pieza dibujada (cada
// objeto de LVGL gasta memoria interna). Solo letras de fuentes.txt. Se incluye después de ui_ficha.h.

#ifndef MFV3_UI_PREVISION_H
#define MFV3_UI_PREVISION_H

#include "ui_ficha.h"
#include "s3_prevision.h"

namespace pronostico {

struct Hooks {
  void (*back)() = nullptr;   // volver a elegir jugadores
  void (*start)() = nullptr;  // EMPEZAR PARTIDO
};
static Hooks hooks;

struct Data {
  uint32_t code[2][4];
  uint8_t n[2];
  prevision::Result r;
  prevision::Meeting meet[3];
  int nMeet;
};
static Data* d = nullptr;  // en la PSRAM
static lv_obj_t* scr = nullptr;

const int PANEL_Y = 66, PANEL_H = 306, CARD_W = 340, ROW_H = 40;

static void drawTeam(lv_layer_t* layer, int s, int x0, int y0) {
  uint32_t edge = s ? ui::color::BLUE_TEAM : ui::color::WHITE_TEAM;
  ficha::drawBox(layer, x0, y0, x0 + CARD_W - 1, y0 + 236, ui::RADIUS, ui::color::CARD, 170, edge, 2);
  ficha::drawText(layer, s ? "AZUL" : "BLANCO", 900, 15, s ? 0x6fb6f5 : ui::color::WHITE_TEAM, x0, y0 + 10, x0 + CARD_W - 1,
                  LV_TEXT_ALIGN_CENTER, LV_OPA_COVER, 3);
  int top = y0 + 36 + (4 - d->n[s]) * ROW_H / 2;  // con menos jugadores, centrados
  for (uint8_t i = 0; i < d->n[s]; i++) {
    const jug::Player* p = ficha::store->find(d->code[s][i]);
    int y = top + i * ROW_H;
    ficha::drawFace(layer, d->code[s][i], p ? p->name : "?", x0 + 16, y + 2, 34);
    char nm[48];
    snprintf(nm, sizeof(nm), "%s", p ? p->name : "?");
    ficha::fitText(nm, 800, 18, CARD_W - 80);
    ficha::drawText(layer, nm, 800, 18, ui::color::TEXT, x0 + 60, y + 8, x0 + CARD_W - 12);
  }
  // % y barra (como .pre-team-pct).
  int y = y0 + 196;
  if (!d->r.available) {
    ficha::drawText(layer, "—", 900, 30, ui::color::TEXT3, x0 + 16, y - 6, x0 + 120);
    return;
  }
  uint8_t pct = s ? d->r.bluePct : d->r.whitePct;
  char t[8];
  snprintf(t, sizeof(t), "%u %%", pct);
  bool fav = pct > 50;
  ficha::drawText(layer, t, 900, 30, fav ? ui::color::OK : ui::color::TEXT, x0 + 16, y - 6, x0 + 120);
  int bx = x0 + 124, bw = CARD_W - 140;
  ficha::drawBox(layer, bx, y + 8, bx + bw - 1, y + 19, 6, ui::color::LINE, LV_OPA_COVER, 0, 0);
  int fw = bw * pct / 100;
  if (fw > 0) ficha::drawBox(layer, bx, y + 8, bx + fw - 1, y + 19, 6, fav ? ui::color::OK : ui::color::ACCENT, LV_OPA_COVER, 0, 0);
}

static void drawPanel(lv_event_t* e) {
  if (!d) return;
  lv_obj_t* o = lv_event_get_current_target_obj(e);
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  drawTeam(layer, 0, a.x1, a.y1);
  drawTeam(layer, 1, a.x1 + 768 - CARD_W, a.y1);
  ficha::drawText(layer, "VS", 900, 30, ui::color::TEXT3, a.x1 + CARD_W, a.y1 + 90, a.x1 + 768 - CARD_W, LV_TEXT_ALIGN_CENTER);
  ficha::drawText(layer, "PRONÓSTICO", 700, 12, ui::color::RANKED, a.x1 + CARD_W - 6, a.y1 + 198, a.x1 + 768 - CARD_W + 6,
                  LV_TEXT_ALIGN_CENTER);
  // Últimos enfrentamientos (o el aviso de que no hay pronóstico).
  int y = a.y1 + 248;
  ficha::drawBox(layer, a.x1, y, a.x1 + 767, a.y1 + PANEL_H - 1, ui::RADIUS_SM, ui::color::CARD, 120, ui::color::LINE, 1);
  if (!d->r.available) {
    ficha::drawText(layer, "Sin pronóstico: nadie ha jugado clasificatorios todavía.", 700, 15, ui::color::TEXT2, a.x1, y + 8,
                    a.x1 + 767, LV_TEXT_ALIGN_CENTER);
    ficha::drawText(layer, "Puedes jugar igualmente.", 400, 14, ui::color::TEXT3, a.x1, y + 32, a.x1 + 767, LV_TEXT_ALIGN_CENTER);
    return;
  }
  ficha::drawText(layer, "ÚLTIMOS ENFRENTAMIENTOS", 700, 12, ui::color::TEXT3, a.x1 + 16, y + 8, a.x1 + 400, LV_TEXT_ALIGN_LEFT,
                  LV_OPA_COVER, 1);
  if (!d->nMeet) {
    ficha::drawText(layer, "Primer enfrentamiento entre estos equipos.", 400, 16, ui::color::TEXT2, a.x1 + 16, y + 28, a.x1 + 760);
    return;
  }
  int colW = 752 / 3;
  for (int i = 0; i < d->nMeet; i++) {
    const prevision::Meeting& m = d->meet[i];
    int x = a.x1 + 16 + i * colW;
    char dt[16] = "Sin fecha", sc[16];
    if (m.at) {
      reloj::Clock c = partidos::localOf((uint32_t)(m.at / 1000));
      snprintf(dt, sizeof(dt), "%02u/%02u/%02u", c.day, c.month, c.year % 100);
    }
    ficha::drawText(layer, dt, 400, 14, ui::color::TEXT3, x, y + 30, x + 90);
    snprintf(sc, sizeof(sc), "%u – %u", m.white, m.blue);
    uint32_t col = m.winner == 0 ? 0xeef6ff : m.winner == 1 ? 0x6fb6f5 : ui::color::TEXT2;
    ficha::drawText(layer, sc, 800, 18, col, x + 88, y + 27, x + colW - 8);
  }
}

static void onBack(lv_event_t*) {
  if (hooks.back) hooks.back();
}
static void onStart(lv_event_t*) {
  if (hooks.start) hooks.start();
}

static void onDelete(lv_event_t*) { scr = nullptr; }
static bool active() { return scr && lv_screen_active() == scr; }

// Calcula y enseña el pronóstico de estas alineaciones (códigos de jugador).
static void show(jug::Store* s, const uint32_t (*codes)[4], const uint8_t* n) {
  ficha::store = s;
  uint32_t all[jug::MAX];
  for (int k = 0; k < s->count; k++) all[k] = s->list[k].code;
  progresion::refresh(all, s->count);
  if (!d) d = (Data*)s3mem::allocateUi(sizeof(Data));
  if (d) {
    memset(d, 0, sizeof(Data));
    for (int t = 0; t < 2; t++) {
      d->n[t] = n[t] > 4 ? 4 : n[t];
      for (int i = 0; i < d->n[t]; i++) d->code[t][i] = codes[t][i];
    }
    d->r = prevision::predict(d->code[0], d->n[0], d->code[1], d->n[1]);
    d->nMeet = prevision::lastMeetings(d->code[0], d->n[0], d->code[1], d->n[1], d->meet, 3);
  }
  lv_obj_t* old = scr;
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  ficha::frame(scr, "CLASIFICATORIO", onBack);
  lv_obj_t* panel = ui::box(scr, 16, PANEL_Y, 768, PANEL_H);
  lv_obj_add_event_cb(panel, drawPanel, LV_EVENT_DRAW_MAIN, nullptr);
  // EMPEZAR PARTIDO (el verde de la app).
  lv_obj_t* b = ui::box(scr, 16, PANEL_Y + PANEL_H + 12, 768, 480 - (PANEL_Y + PANEL_H + 12) - 12);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(b, ui::RADIUS, 0);
  lv_obj_set_style_bg_color(b, ui::hex(0x0f3a2a), 0);
  lv_obj_set_style_bg_color(b, ui::hex(0x0a281d), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 3, 0);
  lv_obj_set_style_border_color(b, ui::hex(ui::color::OK), 0);
  lv_obj_add_event_cb(b, onStart, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* l = ui::text(b, "EMPEZAR PARTIDO", 900, 30, ui::color::OK, 1.5f);
  lv_obj_center(l);
  if (old) lv_obj_remove_event_cb(old, onDelete);
  ui::show(scr);
}

}  // namespace pronostico

#endif  // MFV3_UI_PREVISION_H
