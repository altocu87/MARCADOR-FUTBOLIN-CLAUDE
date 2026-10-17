// MARCADOR FUTBOLÍN V3 · piezas comunes de las pantallas LVGL (capas de la app, zonas táctiles, letras, colores).
// Los colores son las variables CSS de la app (src/styles/base.css).

#ifndef MFV3_UI_BASE_H
#define MFV3_UI_BASE_H

#include <lvgl.h>
#include "ui_assets.h"

namespace ui {

// ---------------------------------------------------------------- tema (src/styles/base.css)
namespace color {
const uint32_t BG_DEEP = 0x070b12, SURFACE = 0x091324, CARD = 0x11223b, CARD2 = 0x162a47, LINE = 0x22385a;
const uint32_t TEXT = 0xf3f7ff, TEXT2 = 0xa6b7cf, TEXT3 = 0x6f84a3, ACCENT = 0x62d6ff;
const uint32_t WHITE_TEAM = 0xeef6ff, WHITE_INK = 0x0b1626, BLUE_TEAM = 0x176dbc, BLUE_INK = 0xf3f7ff;
const uint32_t CHAOS = 0xd65cff, RANKED = 0xf2c94c, DANGER = 0xff5a6e, OK = 0x3ddc97;
}  // namespace color
const int RADIUS = 14, RADIUS_SM = 10;

static inline lv_color_t hex(uint32_t c) { return lv_color_hex(c); }

// Objeto vacío, sin estilos del tema (todo lo que se ve lo pone la pantalla).
static lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_remove_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}

// Pantalla nueva de 800×480 (se carga con show()).
static lv_obj_t* screen() {
  lv_obj_t* s = lv_obj_create(nullptr);
  lv_obj_remove_style_all(s);
  lv_obj_set_size(s, 800, 480);
  lv_obj_set_style_bg_color(s, hex(color::BG_DEEP), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

// Capa de la app (fondo o recorte) en su sitio. Sin tocar: los toques pasan a lo que haya debajo.
static lv_obj_t* layer(lv_obj_t* parent, const capas::Capa& c, bool visible = true) {
  lv_obj_t* img = lv_image_create(parent);
  lv_image_set_src(img, assets::src(c));
  lv_obj_set_pos(img, c.x, c.y);
  lv_obj_set_size(img, c.w, c.h);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
  if (!visible) lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
  return img;
}

// Zona táctil invisible con su acción.
static lv_obj_t* hotspot(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t cb, void* user = nullptr) {
  lv_obj_t* o = box(parent, x, y, w, h);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, user);
  return o;
}

// Texto con la letra Inter de la app.
static lv_obj_t* label(lv_obj_t* parent, const char* txt, uint16_t weight, uint16_t size, uint32_t col,
                       bool italic = false) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, assets::font(weight, size, italic), 0);
  lv_obj_set_style_text_color(l, hex(col), 0);
  lv_label_set_text(l, txt);
  return l;
}

// Aparece/desaparece fundiéndose (como las transiciones de opacidad de la app).
static void fadeCb(void* o, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }
static void fadeIn(lv_obj_t* o, uint32_t ms) {
  lv_anim_delete(o, fadeCb);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_exec_cb(&a, fadeCb);
  lv_obj_set_style_opa(o, LV_OPA_TRANSP, 0);
  lv_anim_set_values(&a, 0, 255);
  lv_anim_set_duration(&a, ms);
  lv_anim_start(&a);
}
static void hide(lv_obj_t* o) {
  lv_anim_delete(o, fadeCb);
  lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_opa(o, LV_OPA_COVER, 0);
}

// Cambia de pantalla (la anterior se borra).
static void show(lv_obj_t* scr) { lv_screen_load_anim(scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true); }

}  // namespace ui

#endif  // MFV3_UI_BASE_H
