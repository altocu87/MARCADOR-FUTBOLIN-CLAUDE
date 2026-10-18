// MARCADOR FUTBOLÍN V3 · pantalla de victoria, igual que la app (src/ui/components/VictoryScreen.tsx).
// ------------------------------------------------------------------------------------------------
// Fondo con rayos, «¡VICTORIA BLANCO!» o «¡VICTORIA AZUL!», inicio y REVANCHA (capas de la app, una por ganador),
// y encima el marcador final («BLANCO 5 – 1 AZUL», el ganador en dorado), la duración y el confeti cayendo.
// Los rayos de la app giran despacio (24 s por vuelta); aquí están quietos (girar una imagen de pantalla entera
// a cada momento sería demasiado para la placa).

#ifndef MFV3_UI_VICTORIA_H
#define MFV3_UI_VICTORIA_H

#include "ui_base.h"

namespace victoria {

struct View {
  uint8_t winner;  // 0 Blanco, 1 Azul
  uint8_t white, blue;
  char facts[64];  // «Duración 00:17 · …»
};

struct Hooks {
  void (*home)() = nullptr;
  void (*rematch)() = nullptr;
};
static Hooks hooks;

static lv_obj_t* scr = nullptr;
static lv_obj_t *box, *teamL[2], *num[2], *sep, *facts, *pulRematch, *pulHome;
static View shown;
static uint8_t builtFor = 255;

// --num de la app para esta pantalla: 118,15 px (cifras); rótulos al 20 % y guion al 70 %.
const float NUM = 118.15f, TEAM = 23.63f, GAP = NUM * 0.25f, PAD = NUM * 0.4f;

static void onBtn(lv_event_t* e) {
  int which = (int)(intptr_t)lv_event_get_user_data(e);  // 0 = inicio, 1 = revancha
  lv_event_code_t c = lv_event_get_code(e);
  lv_obj_t* pul = which ? pulRematch : pulHome;
  if (c == LV_EVENT_PRESSED) ui::reveal(pul);
  else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) lv_obj_add_flag(pul, LV_OBJ_FLAG_HIDDEN);
  else if (c == LV_EVENT_CLICKED) {
    if (which == 0 && hooks.home) hooks.home();
    if (which == 1 && hooks.rematch) hooks.rematch();
  }
}

// ---------------------------------------------------------------- confeti
// Como la app: trocitos de colores que caen 520 px con su deriva, en dos pasadas. En la placa no giran (cada trocito
// girado se pinta aparte y luego se mezcla: con 40 a la vez la pantalla iba a 7 imágenes por segundo) y son 16.
static const uint32_t CONF_COLORS[6] = {0x62d6ff, 0xf2c94c, 0xff5a6e, 0x3ddc97, 0xd65cff, 0xeef6ff};
static void confY(void* o, int32_t v) { lv_obj_set_y((lv_obj_t*)o, v); }
static void confX(void* o, int32_t v) { lv_obj_set_x((lv_obj_t*)o, v); }

static void confetti(lv_obj_t* parent) {
  uint32_t seed = 12345;
  auto rnd = [&](int n) {
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 16) % (uint32_t)n);
  };
  for (int i = 0; i < 16; i++) {
    int w = 5 + rnd(9), h = 4 + rnd(10), x = rnd(800);
    lv_obj_t* c = ui::box(parent, x, -20, w, h);
    lv_obj_set_style_radius(c, 2, 0);
    lv_obj_set_style_bg_color(c, ui::hex(CONF_COLORS[i % 6]), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    uint32_t dur = 2200 + rnd(1800), delay = rnd(1600);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, c);
    lv_anim_set_duration(&a, dur);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_repeat_count(&a, 2);
    lv_anim_set_exec_cb(&a, confY);
    lv_anim_set_values(&a, -20, 500);
    lv_anim_start(&a);
    lv_anim_set_exec_cb(&a, confX);
    lv_anim_set_values(&a, x, x + rnd(160) - 80);
    lv_anim_start(&a);
    rnd(3600);  // (el giro de la app: se gasta el número para que la lluvia caiga igual que antes)
  }
}

static void onDelete(lv_event_t*) {
  scr = nullptr;
  builtFor = 255;
}

static void build(uint8_t winner) {
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  ui::layer(scr, winner ? capas::VICTORIA_AZUL : capas::VICTORIA_BLANCO);
  pulRematch = ui::layer(scr, capas::VICTORIA_PUL_REVANCHA, false);
  pulHome = ui::layer(scr, capas::VICTORIA_PUL_INICIO, false);
  // Recuadro del marcador (.vic-score): fondo oscuro, borde claro tenue y esquinas de 14 px.
  box = ui::box(scr, 88, 135, 624, 130);
  lv_obj_set_style_radius(box, 14, 0);
  lv_obj_set_style_bg_color(box, ui::hex(0x04080f), 0);
  lv_obj_set_style_bg_opa(box, 209, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, lv_color_white(), 0);
  lv_obj_set_style_border_opa(box, 38, 0);
  for (int t = 0; t < 2; t++) {
    bool won = t == winner;
    teamL[t] = ui::text(scr, t ? "AZUL" : "BLANCO", 900, 24, t ? 0x6fb6f5 : ui::color::WHITE_TEAM, TEAM * 0.2f);
    if (!won) lv_obj_set_style_text_opa(teamL[t], 140, 0);
    num[t] = ui::text(scr, "0", 900, 118, won ? ui::color::RANKED : ui::color::TEXT2);
  }
  sep = ui::text(scr, "–", 900, 83, ui::color::TEXT3);
  facts = ui::text(scr, "", 800, 13, ui::color::RANKED, 0.78f);
  confetti(scr);
  lv_obj_t* h = ui::box(scr, 16, 8, 56, 56);
  lv_obj_add_flag(h, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(h, onBtn, LV_EVENT_ALL, (void*)0);
  lv_obj_t* r = ui::box(scr, 656, 8, 128, 56);
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(r, onBtn, LV_EVENT_ALL, (void*)1);
  builtFor = winner;
}

static bool active() { return scr && lv_screen_active() == scr; }

// Marcador centrado en x = 400 con los huecos de la app (rótulo · cifra · guion · cifra · rótulo).
static void layout(const View& v) {
  char b[4];
  snprintf(b, sizeof(b), "%u", v.white);
  lv_label_set_text(num[0], b);
  snprintf(b, sizeof(b), "%u", v.blue);
  lv_label_set_text(num[1], b);
  lv_obj_t* items[5] = {teamL[0], num[0], sep, num[1], teamL[1]};
  float w[5], total = 0;
  for (int i = 0; i < 5; i++) {
    lv_obj_update_layout(items[i]);
    w[i] = (float)lv_obj_get_width(items[i]) + ((i == 0 || i == 4) ? TEAM * 0.2f : 0);  // el espaciado final del CSS
    total += w[i];
  }
  total += 4 * GAP;
  float boxW = total + 2 * PAD;
  lv_obj_set_x(box, (int32_t)lroundf(400 - boxW / 2));
  lv_obj_set_width(box, (int32_t)lroundf(boxW));
  float x = 400 - total / 2;
  const float tops[5] = {185.5f, 138, 150, 138, 185.5f}, lh[5] = {28.6f, 124.1f, 100, 124.1f, 28.6f};
  for (int i = 0; i < 5; i++) {
    ui::place(items[i], x, tops[i], lh[i], ui::Align::Left);
    x += w[i] + GAP;
  }
  lv_label_set_text(facts, v.facts);
  ui::place(facts, 400 + 0.39f, 267, 16);
}

static void update(const View& v) {
  if (!active() || builtFor != v.winner) {
    build(v.winner);
    ui::show(scr);
    layout(v);
  } else if (v.white != shown.white || v.blue != shown.blue || strcmp(v.facts, shown.facts)) {
    layout(v);
  }
  shown = v;
}

}  // namespace victoria

#endif  // MFV3_UI_VICTORIA_H
