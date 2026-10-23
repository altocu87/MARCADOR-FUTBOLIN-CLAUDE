// MARCADOR FUTBOLÍN V3 · pantallas antiguas (dibujadas a mano) sobre LVGL, mientras se pasan una a una a LVGL.
// ----------------------------------------------------------------------------------------------------------
// Las pantallas que aún no se han rehecho igual que la app (prueba de la placa, ajustes, Wi-Fi, actualizar…)
// siguen llamando a fillRect, drawString, etc. Este adaptador convierte cada llamada en un objeto LVGL sobre una
// pantalla propia: fillScreen la vacía, y un rectángulo relleno tapa (borra) lo que había debajo. Los toques llegan
// a onTap(x, y) como antes, desde una zona táctil que cubre la pantalla.
// Las letras FreeSans de LovyanGFX se cambian por Inter (la de la app), de tamaño parecido.

#ifndef MFV3_UI_LEGACY_H
#define MFV3_UI_LEGACY_H

#include <lvgl.h>
#include "ui_base.h"

class LegacyGfx {
 public:
  lv_obj_t* scr = nullptr;
  void (*tapCb)(int16_t, int16_t) = nullptr;

  // Pantalla de dibujo antiguo: se crea (y se enseña) la primera vez que se dibuja en ella.
  void activate() {
    if (scr && lv_screen_active() == scr) return;
    scr = ui::screen();
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, onPress, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, this);
    ui::show(scr);
  }
  bool active() const { return scr && lv_screen_active() == scr; }

  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
  }

  void fillScreen(uint16_t c) {
    activate();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, col(c), 0);
  }
  void fillRect(int x, int y, int w, int h, uint16_t c) { rect(x, y, w, h, 0, c, true); }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) { rect(x, y, w, h, r, c, true); }
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t c) { rect(x, y, w, h, r, c, false); }
  void drawFastHLine(int x, int y, int w, uint16_t c) { rect(x, y, w, 1, 0, c, true, false); }
  void fillCircle(int x, int y, int r, uint16_t c) { rect(x - r, y - r, 2 * r + 1, 2 * r + 1, LV_RADIUS_CIRCLE, c, true, false); }
  void drawCircle(int x, int y, int r, uint16_t c) { rect(x - r, y - r, 2 * r + 1, 2 * r + 1, LV_RADIUS_CIRCLE, c, false); }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    // Solo se usan rayas horizontales y verticales (la cruz del último toque).
    if (y0 == y1) rect(x0 < x1 ? x0 : x1, y0, abs(x1 - x0) + 1, 1, 0, c, true, false);
    else rect(x0, y0 < y1 ? y0 : y1, 1, abs(y1 - y0) + 1, 0, c, true, false);
  }

  void setFont(const lgfx::IFont* f) { font_ = f; }
  void setTextSize(float) {}
  void setTextColor(uint16_t c) { textColor_ = c; }
  void setTextDatum(lgfx::textdatum_t d) { datum_ = d; }
  int32_t textWidth(const char* s) {
    lv_point_t sz;
    lv_text_get_size(&sz, s, lvFont(), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return sz.x;
  }
  void drawString(const char* s, int x, int y) {
    activate();
    const lv_font_t* f = lvFont();
    lv_obj_t* l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, col(textColor_), 0);
    lv_label_set_text(l, s);
    lv_point_t sz;
    lv_text_get_size(&sz, s, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int w = sz.x;
    int h = lv_font_get_line_height(f);
    int lx = datum_ == lgfx::textdatum_t::middle_left ? x : datum_ == lgfx::textdatum_t::middle_right ? x - w : x - w / 2;
    lv_obj_set_pos(l, lx, y - h / 2);
  }

 private:
  const lgfx::IFont* font_ = nullptr;
  uint16_t textColor_ = 0xFFFF;
  lgfx::textdatum_t datum_ = lgfx::textdatum_t::middle_center;

  static lv_color_t col(uint16_t c) {
    return lv_color_make((uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31));
  }

  const lv_font_t* lvFont() {
    using namespace lgfx::fonts;
    const lgfx::IFont* f = font_;
    // Solo letras que ya están en la memoria de la placa (fuentes.txt): las que faltaban se quedaban en la letra
    // pequeña de LVGL con un aviso en cada dibujo.
    if (f == &FreeSans9pt7b) return assets::font(400, 16);
    if (f == &FreeSansBold9pt7b) return assets::font(800, 17);
    if (f == &FreeSans12pt7b) return assets::font(400, 22);
    if (f == &FreeSansBold12pt7b) return assets::font(800, 20);
    if (f == &FreeSansBold18pt7b) return assets::font(900, 30);
    if (f == &FreeSansBold24pt7b) return assets::font(900, 38);
    return assets::font(800, 20);
  }

  // Un rectángulo relleno tapa lo que había: se borran los objetos que quedan enteros debajo.
  void rect(int x, int y, int w, int h, int r, uint16_t c, bool filled, bool covers = true) {
    activate();
    if (filled && covers) {
      lv_obj_update_layout(scr);
      for (int i = (int)lv_obj_get_child_count(scr) - 1; i >= 0; i--) {
        lv_obj_t* o = lv_obj_get_child(scr, i);
        lv_area_t a;
        lv_obj_get_coords(o, &a);
        if (a.x1 >= x && a.y1 >= y && a.x2 < x + w && a.y2 < y + h) lv_obj_delete(o);
      }
    }
    lv_obj_t* o = ui::box(scr, x, y, w, h);
    lv_obj_set_style_radius(o, r, 0);
    if (filled) {
      lv_obj_set_style_bg_color(o, col(c), 0);
      lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    } else {
      lv_obj_set_style_border_color(o, col(c), 0);
      lv_obj_set_style_border_width(o, 1, 0);
    }
  }

  static void onDelete(lv_event_t* e) {
    LegacyGfx* g = (LegacyGfx*)lv_event_get_user_data(e);
    // LVGL libera las pantallas al sustituirlas. Su dirección puede reutilizarse:
    // no confundir otra pantalla con la nuestra ni perder sus eventos táctiles.
    if (lv_event_get_target_obj(e) == g->scr) g->scr = nullptr;
  }

  static void onPress(lv_event_t* e) {
    LegacyGfx* g = (LegacyGfx*)lv_event_get_user_data(e);
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    if (g->tapCb) g->tapCb((int16_t)p.x, (int16_t)p.y);
  }
};

#endif  // MFV3_UI_LEGACY_H
