// MARCADOR FUTBOLÍN V3 · piezas comunes de las pantallas LVGL (capas de la app, zonas táctiles, letras, colores).
// Los colores son las variables CSS de la app (src/styles/base.css).

#ifndef MFV3_UI_BASE_H
#define MFV3_UI_BASE_H

#include <lvgl.h>
#include <math.h>
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

// Pantalla nueva de 800×480 (se carga con show()). Desde aquí, las imágenes que pida son «suyas» (ui_assets.h).
static lv_obj_t* screen() {
  assets::newScreen();
  lv_obj_t* s = lv_obj_create(nullptr);
  lv_obj_remove_style_all(s);
  lv_obj_set_size(s, 800, 480);
  lv_obj_set_style_bg_color(s, hex(color::BG_DEEP), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

// Capa de la app (fondo o recorte) en su sitio. Sin tocar: los toques pasan a lo que haya debajo.
// Las que empiezan ocultas (botón pulsado, tarjeta elegida…) no se descomprimen hasta enseñarlas con reveal():
// así cambiar de pantalla solo cuesta lo que se ve (antes se descomprimían todas y la placa se quedaba parada).
static lv_obj_t* layer(lv_obj_t* parent, const capas::Capa& c, bool visible = true) {
  lv_obj_t* img = lv_image_create(parent);
  lv_obj_set_user_data(img, (void*)&c);
  if (visible) lv_image_set_src(img, assets::src(c));
  lv_obj_set_pos(img, c.x, c.y);
  lv_obj_set_size(img, c.w, c.h);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
  if (!visible) lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
  return img;
}

// Enseña un objeto; si es una capa aún sin imagen, la carga ahora.
static void reveal(lv_obj_t* o) {
  if (!o) return;
  if (lv_obj_check_type(o, &lv_image_class) && !lv_image_get_src(o) && lv_obj_get_user_data(o))
    lv_image_set_src(o, assets::src(*(const capas::Capa*)lv_obj_get_user_data(o)));
  lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
}
static void setVisible(lv_obj_t* o, bool v) {
  if (!o) return;
  if (v) reveal(o);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// Transparencia sin capas intermedias: en las imágenes, la de la propia imagen (más rápido y sin gastar memoria);
// en lo demás, la del objeto entero.
static void setOpa(lv_obj_t* o, lv_opa_t v) {
  if (lv_obj_check_type(o, &lv_image_class)) lv_obj_set_style_image_opa(o, v, 0);
  else lv_obj_set_style_opa(o, v, 0);
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

// Coloca un texto donde lo pone el navegador. `top` y `lineH` son los de la caja medida en la app (line-height en px;
// 0 = «normal»); `x` es el borde izquierdo, el centro o el borde derecho según `align`. La letra Inter tiene
// ascendente 0,96875 y descendente 0,2412 del tamaño: con ellas sale la línea base, que se hace coincidir.
enum class Align : uint8_t { Left, Center, Right };
static void place(lv_obj_t* l, float x, float top, float lineH, Align align = Align::Center) {
  const lv_font_t* f = lv_obj_get_style_text_font(l, LV_PART_MAIN);
  float size = assets::fontSize(f);
  if (size <= 0) size = (float)(f->line_height - f->base_line) / 0.96875f;  // letra que no es nuestra
  if (lineH <= 0) lineH = size * 1.21f;
  float baseline = top + (lineH - size * 1.2099f) / 2 + size * 0.96875f;
  lv_obj_update_layout(l);
  int32_t w = lv_obj_get_width(l);
  float lx = align == Align::Left ? x : align == Align::Center ? x - w / 2.0f : x - w;
  lv_obj_set_pos(l, (int32_t)lroundf(lx), (int32_t)lroundf(baseline - (f->line_height - f->base_line)));
}

// Texto con espaciado entre letras (letter-spacing de la app, en px).
static lv_obj_t* text(lv_obj_t* parent, const char* txt, uint16_t weight, uint16_t size, uint32_t col, float spacing = 0,
                      bool italic = false) {
  lv_obj_t* l = label(parent, txt, weight, size, col, italic);
  if (spacing) lv_obj_set_style_text_letter_space(l, (int32_t)lroundf(spacing), 0);
  return l;
}

// Botón como los de la app (.btn): primario (azul con brillo), peligro (rojo suave) o normal. Se encoge al 96 %
// mientras se pulsa, como en la app.
enum class BtnKind : uint8_t { Normal, Primary, Danger, Ghost };
static lv_obj_t* button(lv_obj_t* parent, int x, int y, int w, int h, const char* txt, uint16_t fontSize, BtnKind kind,
                        lv_event_cb_t cb, void* user = nullptr) {
  lv_obj_t* b = box(parent, x, y, w, h);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(b, kind == BtnKind::Primary && h >= 120 ? 24 : RADIUS_SM, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  uint32_t ink = color::TEXT;
  switch (kind) {
    case BtnKind::Primary:
      lv_obj_set_style_bg_color(b, hex(0x2fb8ec), 0);
      lv_obj_set_style_bg_grad_color(b, hex(0x168fc4), 0);
      lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
      lv_obj_set_style_border_color(b, hex(color::ACCENT), 0);
      lv_obj_set_style_shadow_color(b, hex(color::ACCENT), 0);
      lv_obj_set_style_shadow_width(b, 18, 0);
      lv_obj_set_style_shadow_opa(b, 46, 0);
      ink = 0x03121e;
      break;
    case BtnKind::Danger:
      lv_obj_set_style_bg_color(b, hex(color::DANGER), 0);
      lv_obj_set_style_bg_opa(b, 41, 0);  // --danger-soft: 16 %
      lv_obj_set_style_border_color(b, hex(color::DANGER), 0);
      ink = 0xffd2d8;
      break;
    case BtnKind::Ghost:
      lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_color(b, hex(color::LINE), 0);
      break;
    default:
      lv_obj_set_style_bg_color(b, hex(color::CARD), 0);
      lv_obj_set_style_border_color(b, hex(color::LINE), 0);
  }
  lv_obj_set_style_transform_pivot_x(b, w / 2, 0);
  lv_obj_set_style_transform_pivot_y(b, h / 2, 0);
  lv_obj_set_style_transform_scale(b, 246, LV_STATE_PRESSED);
  lv_obj_t* l = text(b, txt, 800, fontSize, ink, fontSize * 0.05f);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

// Aparece/desaparece fundiéndose (como las transiciones de opacidad de la app).
static void fadeCb(void* o, int32_t v) { setOpa((lv_obj_t*)o, (lv_opa_t)v); }
static void fadeIn(lv_obj_t* o, uint32_t ms) {
  lv_anim_delete(o, fadeCb);
  reveal(o);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_exec_cb(&a, fadeCb);
  setOpa(o, LV_OPA_TRANSP);
  lv_anim_set_values(&a, 0, 255);
  lv_anim_set_duration(&a, ms);
  lv_anim_start(&a);
}
static void hide(lv_obj_t* o) {
  lv_anim_delete(o, fadeCb);
  lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  setOpa(o, LV_OPA_COVER);
}

// Aviso breve abajo en el centro, como el «toast» de la app (2,6 s, encima de todo).
static lv_obj_t* toastObj = nullptr;
static lv_timer_t* toastTimer = nullptr;
static void toastGone(lv_timer_t*) {
  toastTimer = nullptr;
  if (toastObj) lv_obj_delete(toastObj);
  toastObj = nullptr;
}
static void toast(const char* msg) {
  if (toastTimer) lv_timer_delete(toastTimer);
  if (toastObj) lv_obj_delete(toastObj);
  toastObj = lv_obj_create(lv_layer_top());
  lv_obj_t* t = toastObj;
  lv_obj_remove_style_all(t);
  lv_obj_remove_flag(t, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(t, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(t, 18, 0);
  lv_obj_set_style_pad_ver(t, 10, 0);
  lv_obj_set_style_radius(t, 999, 0);
  lv_obj_set_style_bg_color(t, hex(color::CARD2), 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(t, 1, 0);
  lv_obj_set_style_border_color(t, hex(color::ACCENT), 0);
  lv_obj_set_style_shadow_width(t, 18, 0);
  lv_obj_set_style_shadow_color(t, hex(color::ACCENT), 0);
  lv_obj_set_style_shadow_opa(t, 46, 0);
  lv_obj_t* l = label(t, msg, 700, 14, color::TEXT);
  (void)l;
  lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -14);
  toastTimer = lv_timer_create(toastGone, 2600, nullptr);
  lv_timer_set_repeat_count(toastTimer, 1);
}

// Cambia de pantalla (la anterior se borra).
static void show(lv_obj_t* scr) { lv_screen_load_anim(scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true); }

}  // namespace ui

#endif  // MFV3_UI_BASE_H
