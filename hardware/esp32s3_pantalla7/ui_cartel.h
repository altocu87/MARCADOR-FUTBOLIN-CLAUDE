// MARCADOR FUTBOLÍN V3 · carteles en franja (Alex, 18/10/2026).
// ---------------------------------------------------------------------------------------------------
// Todos los carteles que en la app ocupan la pantalla entera (¡GOL!, rachas, gol anulado, partes, prórroga,
// penaltis…) salen en la placa como una franja horizontal de lado a lado (150 px de alto) con un título grande en
// cursiva y una línea debajo (inclinada se probó y las letras se veían peor). Los dibuja la placa directamente
// (un rectángulo con degradado y el texto), sin imágenes ni memoria aparte: salen al momento, y al irse solo se
// repinta lo que tapaban (un tercio de la pantalla), no la pantalla entera.

#ifndef MFV3_UI_CARTEL_H
#define MFV3_UI_CARTEL_H

#include "ui_base.h"

namespace cartel {

struct Style {
  const char* title;
  const char* sub;
  uint32_t c1, c2;    // degradado de la franja (izquierda → derecha)
  uint32_t ink;       // título
  uint32_t subInk;    // línea de debajo
  uint32_t edge;      // filos de neón arriba y abajo
};

// Colores de cada equipo y de cada tipo de cartel.
const Style WHITE_GOAL = {"", "", 0xf4f8ff, 0xc9d9ee, 0x0b1a33, 0x2b4a77, 0xffffff};
const Style BLUE_GOAL = {"", "", 0x2f8ef0, 0x0f4fa0, 0xffffff, 0xcfe6ff, 0x7cc4ff};
const Style GOLD = {"", "", 0xffd75e, 0xe6a417, 0x2a1700, 0x5a3800, 0xfff0b3};
const Style FIRE = {"", "", 0xff8a2b, 0xd8321e, 0xffffff, 0xffe2c4, 0xffd27a};
const Style ANNUL = {"", "", 0xff4d5e, 0x9e1b2c, 0xffffff, 0xffd2d8, 0xffb3bc};
const Style START = {"", "", 0x22d3ee, 0x1d4ed8, 0xffffff, 0xd9f6ff, 0x9df1ff};
const Style CHAOS = {"", "", 0xe040fb, 0x7c3aed, 0xffffff, 0xf6dcff, 0xf0a8ff};
const Style EXTRA = {"", "", 0x2ee6b6, 0x0e7a6a, 0x04241f, 0x0b3d35, 0xb8fff0};
const Style PENALTY = {"", "", 0x8b5cf6, 0x4c1d95, 0xffffff, 0xe6dcff, 0xc4b5fd};
const Style SUDDEN = {"", "", 0xff4d5e, 0x1a0508, 0xffffff, 0xffc2c9, 0xff8a96};

// La franja: 150 px de alto, horizontal, de lado a lado.
const int BAND_H = 150, FH = BAND_H;

static lv_obj_t* shown = nullptr;
static Style cur;
static char curTitle[32], curSub[40];
static int32_t titleSpace = 2;

static void gone(lv_anim_t*) {
  if (shown) lv_obj_delete(shown);
  shown = nullptr;
}

// Se dibuja directamente cada vez que hace falta (sin imagen intermedia): solo lo que cae en la zona repintada.
// Se dibuja directamente cada vez que hace falta (sin imagen intermedia): un rectángulo con su degradado y dos filos
// de neón (lo más ligero que hay para la placa) y los dos textos.
static void drawCb(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t co;
  lv_obj_get_coords((lv_obj_t*)lv_event_get_target(e), &co);
  lv_draw_rect_dsc_t r;
  lv_draw_rect_dsc_init(&r);
  r.bg_opa = LV_OPA_COVER;
  r.bg_color = lv_color_hex(cur.c1);
  r.bg_grad.dir = LV_GRAD_DIR_HOR;
  r.bg_grad.stops_count = 2;
  r.bg_grad.stops[0].color = lv_color_hex(cur.c1);
  r.bg_grad.stops[0].opa = LV_OPA_COVER;
  r.bg_grad.stops[0].frac = 0;
  r.bg_grad.stops[1].color = lv_color_hex(cur.c2);
  r.bg_grad.stops[1].opa = LV_OPA_COVER;
  r.bg_grad.stops[1].frac = 255;
  r.border_color = lv_color_hex(cur.edge);
  r.border_width = 4;
  r.border_side = (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM);
  lv_draw_rect(layer, &r, &co);
  float mid = co.y1 + BAND_H / 2.0f;
  lv_draw_label_dsc_t t;
  lv_draw_label_dsc_init(&t);
  t.font = assets::font(900, 96, true);
  t.color = lv_color_hex(cur.ink);
  t.letter_space = titleSpace;
  t.align = LV_TEXT_ALIGN_CENTER;
  t.text = curTitle;
  lv_area_t ta = {co.x1, (int32_t)(mid - 66), co.x2, (int32_t)(mid + 40)};
  lv_draw_label(layer, &t, &ta);
  lv_draw_label_dsc_t sd;
  lv_draw_label_dsc_init(&sd);
  sd.font = assets::font(900, 24);
  sd.color = lv_color_hex(cur.subInk);
  sd.letter_space = 8;
  sd.align = LV_TEXT_ALIGN_CENTER;
  sd.text = curSub;
  lv_area_t sa = {co.x1, (int32_t)(mid + 36), co.x2, (int32_t)(mid + 66)};
  lv_draw_label(layer, &sd, &sa);
}

// Enseña la franja `ms` milisegundos, centrada en `cy`, encima de todo.
static void show(const Style& base, const char* title, const char* sub, uint32_t ms, int cy = 240) {
  if (shown) lv_obj_delete(shown);
  cur = base;
  snprintf(curTitle, sizeof(curTitle), "%s", title);
  snprintf(curSub, sizeof(curSub), "%s", sub);
  // Títulos largos («MUERTE SÚBITA»): se juntan las letras para que quepan (unos 740 px).
  const lv_font_t* f = assets::font(900, 96, true);
  lv_point_t sz;
  lv_text_get_size(&sz, curTitle, f, 2, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  int n = 0;  // letras (no bytes: la Ú son dos)
  for (const char* q = curTitle; *q; q++) n += ((uint8_t)*q & 0xC0) != 0x80;
  titleSpace = (sz.x > 740 && n > 1) ? (int32_t)(2 - (sz.x - 740 + n - 2) / (n - 1)) : 2;
  // Se crea escondida y se enseña ya colocada: si no, al crearla se marca para repintar también la franja de
  // arriba de la pantalla (donde nace).
  shown = lv_obj_create(lv_layer_top());
  lv_obj_add_flag(shown, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_style_all(shown);
  lv_obj_remove_flag(shown, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(shown, 0, cy - FH / 2);
  lv_obj_set_size(shown, 800, FH);
  lv_obj_update_layout(shown);
  lv_obj_remove_flag(shown, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(shown, drawCb, LV_EVENT_DRAW_MAIN, nullptr);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, shown);
  lv_anim_set_values(&a, 0, 1);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_completed_cb(&a, gone);
  lv_anim_start(&a);
}

static void hide() {
  if (shown) lv_obj_delete(shown);
  shown = nullptr;
}

}  // namespace cartel

#endif  // MFV3_UI_CARTEL_H
