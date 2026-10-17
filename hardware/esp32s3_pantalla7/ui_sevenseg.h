// MARCADOR FUTBOLÍN V3 · reloj de 7 segmentos, igual que SevenSegment.tsx de la app.
// -------------------------------------------------------------------------------------
// Cada cifra es una caja de 60 × 100 unidades con 7 segmentos hexagonales (los apagados, muy tenues: «fantasma»),
// separadas 66 unidades; los dos puntos ocupan 20. Todo inclinado 6° hacia delante (skewX(-6deg)) y con un halo
// del mismo color (en la app, drop-shadow; aquí, el segmento repetido algo más grueso y transparente).

#ifndef MFV3_UI_SEVENSEG_H
#define MFV3_UI_SEVENSEG_H

#include <lvgl.h>
#include <math.h>
#include <string.h>

namespace sevenseg {

struct Data {
  char text[12];
  uint32_t color;
  bool ghost;
  bool colonDim;  // dos puntos tenues (segundos impares)
  uint8_t glow;   // 0 = sin halo
};

// Segmentos: a, b, c, d, e, f, g → polígonos de 6 puntos en coordenadas de la app (viewBox de 100 de alto).
static void segPoly(char seg, float* xs, float* ys) {
  auto h = [&](float cy) {
    const float px[6] = {12, 16, 44, 48, 44, 16}, py[6] = {cy, cy - 4, cy - 4, cy, cy + 4, cy + 4};
    memcpy(xs, px, sizeof(px));
    memcpy(ys, py, sizeof(py));
  };
  auto v = [&](float cx, float y1, float y2) {
    const float px[6] = {cx, cx + 4, cx + 4, cx, cx - 4, cx - 4}, py[6] = {y1, y1 + 4, y2 - 4, y2, y2 - 4, y1 + 4};
    memcpy(xs, px, sizeof(px));
    memcpy(ys, py, sizeof(py));
  };
  switch (seg) {
    case 'a': h(6); break;
    case 'g': h(50); break;
    case 'd': h(94); break;
    case 'f': v(8, 10, 46); break;
    case 'b': v(52, 10, 46); break;
    case 'e': v(8, 54, 90); break;
    default: v(52, 54, 90); break;  // c
  }
}

static const char* segsOf(char ch) {
  switch (ch) {
    case '0': return "abcdef";
    case '1': return "bc";
    case '2': return "abged";
    case '3': return "abgcd";
    case '4': return "fgbc";
    case '5': return "afgcd";
    case '6': return "afgedc";
    case '7': return "abc";
    case '8': return "abcdefg";
    case '9': return "abcdfg";
    case '-': return "g";
    default: return "";
  }
}

// Ancho del texto en unidades de la app (como el viewBox del SVG: cifras 66, separadores 20, menos 6 del final).
static float unitsWidth(const char* t) {
  float x = 0;
  for (const char* c = t; *c; c++) x += (*c == ':' || *c == '.') ? 20 : 66;
  return x - 6;
}

struct Ctx {
  lv_layer_t* layer;
  float scale, ox, oy, skew;
};

static void tri(const Ctx& c, lv_color_t col, lv_opa_t opa, float x0, float y0, float x1, float y1, float x2, float y2) {
  lv_draw_triangle_dsc_t d;
  lv_draw_triangle_dsc_init(&d);
  d.color = col;
  d.opa = opa;
  auto P = [&](float x, float y, lv_point_precise_t& p) {
    // skewX(-6°) alrededor del centro vertical (y = 50 unidades)
    p.x = (lv_value_precise_t)(c.ox + (x - (y - 50) * c.skew) * c.scale);
    p.y = (lv_value_precise_t)(c.oy + y * c.scale);
  };
  P(x0, y0, d.p[0]);
  P(x1, y1, d.p[1]);
  P(x2, y2, d.p[2]);
  lv_draw_triangle(c.layer, &d);
}

// Polígono de 6 puntos (convexo) → 4 triángulos. `grow` lo engorda (unidades) para el halo.
static void hexa(const Ctx& c, const float* xs, const float* ys, lv_color_t col, lv_opa_t opa, float grow) {
  float cx = 0, cy = 0;
  for (int i = 0; i < 6; i++) {
    cx += xs[i];
    cy += ys[i];
  }
  cx /= 6;
  cy /= 6;
  float X[6], Y[6];
  for (int i = 0; i < 6; i++) {
    float dx = xs[i] - cx, dy = ys[i] - cy, len = sqrtf(dx * dx + dy * dy);
    X[i] = xs[i] + (len > 0 ? dx / len * grow : 0);
    Y[i] = ys[i] + (len > 0 ? dy / len * grow : 0);
  }
  tri(c, col, opa, X[0], Y[0], X[1], Y[1], X[2], Y[2]);
  tri(c, col, opa, X[0], Y[0], X[2], Y[2], X[3], Y[3]);
  tri(c, col, opa, X[0], Y[0], X[3], Y[3], X[4], Y[4]);
  tri(c, col, opa, X[0], Y[0], X[4], Y[4], X[5], Y[5]);
}

static void dot(const Ctx& c, float x, float y, lv_color_t col, lv_opa_t opa, float r) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = col;
  d.bg_opa = opa;
  d.radius = LV_RADIUS_CIRCLE;
  float px = c.ox + (x - (y - 50) * c.skew) * c.scale, py = c.oy + y * c.scale, pr = r * c.scale;
  lv_area_t a = {(int32_t)(px - pr), (int32_t)(py - pr), (int32_t)(px + pr), (int32_t)(py + pr)};
  lv_draw_rect(c.layer, &d, &a);
}

static void drawCb(lv_event_t* e) {
  lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
  Data* d = (Data*)lv_obj_get_user_data(obj);
  if (!d) return;
  lv_area_t co;
  lv_obj_get_coords(obj, &co);
  float w = unitsWidth(d->text) + 8;  // viewBox de -4 a ancho + 4
  float scale = (float)lv_area_get_height(&co) / 100.0f;
  Ctx c = {lv_event_get_layer(e), scale, co.x1 + 4 * scale, (float)co.y1, tanf(6.0f * 3.14159265f / 180.0f)};
  (void)w;
  lv_color_t col = lv_color_hex(d->color);
  // Tres pasadas: halo ancho, halo cercano y el segmento.
  const float grows[3] = {5.0f, 2.5f, 0.0f};
  const lv_opa_t haloOpa[3] = {(lv_opa_t)(d->glow * 0.22f), (lv_opa_t)(d->glow * 0.45f), 255};
  for (int pass = d->glow ? 0 : 2; pass < 3; pass++) {
    float x = 0;
    for (const char* ch = d->text; *ch; ch++) {
      Ctx cc = c;
      cc.ox += x * scale;
      if (*ch == ':' || *ch == '.') {
        lv_opa_t o = pass == 2 ? (d->colonDim ? (lv_opa_t)38 : 255) : (lv_opa_t)(haloOpa[pass] * (d->colonDim ? 0.15f : 1.0f));
        if (*ch == ':') dot(cc, 10, 32, col, o, 5 + grows[pass]);
        dot(cc, 10, *ch == ':' ? 68 : 92, col, o, 5 + grows[pass]);
        x += 20;
        continue;
      }
      const char* on = segsOf(*ch);
      for (const char* s = "abcdefg"; *s; s++) {
        bool lit = strchr(on, *s) != nullptr;
        if (!lit && (pass < 2 || !d->ghost)) continue;
        float xs[6], ys[6];
        segPoly(*s, xs, ys);
        hexa(cc, xs, ys, col, lit ? haloOpa[pass] : (lv_opa_t)18, grows[pass]);
      }
      x += 66;
    }
  }
}

// El halo y la inclinación salen un poco de la caja: se amplía la zona que se redibuja.
static void extCb(lv_event_t* e) {
  lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
  lv_event_set_ext_draw_size(e, (int32_t)(lv_obj_get_height(obj) * 0.12f) + 2);
}

static void freeCb(lv_event_t* e) {
  lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
  free(lv_obj_get_user_data(obj));
}

// Reloj de alto `h` (píxeles) en (x, y). Su ancho sale del texto, como en la app.
static lv_obj_t* create(lv_obj_t* parent, int x, int y, int h, const char* text, uint32_t color, uint8_t glow = 200) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  Data* d = (Data*)calloc(1, sizeof(Data));
  snprintf(d->text, sizeof(d->text), "%s", text);
  d->color = color;
  d->ghost = true;
  d->glow = glow;
  lv_obj_set_user_data(o, d);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, (int32_t)ceilf(h * (unitsWidth(text) + 8) / 100.0f), h);
  // El halo y la inclinación salen un poco de la caja.
  lv_obj_add_event_cb(o, extCb, LV_EVENT_REFR_EXT_DRAW_SIZE, nullptr);
  lv_obj_add_event_cb(o, drawCb, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_refresh_ext_draw_size(o);
  lv_obj_add_event_cb(o, freeCb, LV_EVENT_DELETE, nullptr);
  return o;
}

static void setText(lv_obj_t* o, const char* text, bool colonDim = false) {
  Data* d = (Data*)lv_obj_get_user_data(o);
  if (!strcmp(d->text, text) && d->colonDim == colonDim) return;
  snprintf(d->text, sizeof(d->text), "%s", text);
  d->colonDim = colonDim;
  lv_obj_invalidate(o);
}

static void setColor(lv_obj_t* o, uint32_t color) {
  Data* d = (Data*)lv_obj_get_user_data(o);
  if (d->color == color) return;
  d->color = color;
  lv_obj_invalidate(o);
}

}  // namespace sevenseg

#endif  // MFV3_UI_SEVENSEG_H
