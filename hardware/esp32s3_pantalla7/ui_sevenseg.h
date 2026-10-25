// MARCADOR FUTBOLÍN V3 · reloj de 7 segmentos, igual que SevenSegment.tsx de la app.
// -------------------------------------------------------------------------------------
// Cada cifra es una caja de 60 × 100 unidades con 7 segmentos hexagonales (los apagados, muy tenues: «fantasma»),
// separadas 66 unidades; los dos puntos ocupan 20. Todo inclinado 6° hacia delante (skewX(-6deg)) y con un halo
// del mismo color (en la app, drop-shadow; aquí, el segmento repetido algo más grueso y transparente).

#ifndef MFV3_UI_SEVENSEG_H
#define MFV3_UI_SEVENSEG_H

#include <lvgl.h>
#include "misc/cache/instance/lv_image_cache.h"
#include <math.h>
#include <string.h>

namespace sevenseg {

struct Data {
  char text[12];
  uint32_t color;
  bool ghost;
  bool colonDim;  // dos puntos tenues (segundos impares)
  uint8_t glow;   // 0 = sin halo
  bool soft;      // halo difuminado de verdad (reloj de reposo, 250 px): sin bandas ni costuras entre triángulos
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

// ---------------------------------------------------------------- cifras ya pintadas
// Pintar los 7 segmentos con su halo (4 triángulos por segmento y tres pasadas) costaba ~0,1 s cada vez, y el reloj
// del partido se repinta cada segundo. Cada cifra se pinta una vez por tamaño, color, halo y «fantasma» en una
// imagen con transparencia, y luego solo se copia. Margen de 12 unidades alrededor para el halo y la inclinación.
const float CELL_MARGIN = 12;
struct Glyph {
  char ch;
  int16_t h;
  uint32_t color;
  uint8_t glow;
  bool ghost, soft;
  lv_draw_buf_t* buf;
  uint32_t used;
};
static Glyph glyphs[28];  // reloj del partido (3 colores), cuenta atrás y configuración
static uint32_t glyphUse = 0;

// Halo difuminado (`soft`): como el drop-shadow de la app. Primero se tapan las rayas que dejan los bordes suavizados
// de los triángulos dentro de cada segmento (un punto no macizo entre dos que sí lo son, a uno o dos de distancia).
// Luego se difumina la transparencia de lo encendido (tres pasadas de media móvil de radio `r`, en horizontal y en
// vertical) y se suma por debajo con la fuerza `k` (0–255).
static void blurGlow(lv_draw_buf_t* buf, uint32_t color, int r, uint8_t k) {
  int w = buf->header.w, h = buf->header.h, stride = buf->header.stride;
  uint8_t* px = buf->data;
  uint8_t* a = (uint8_t*)malloc((size_t)w * h);
  uint8_t* t = (uint8_t*)malloc((size_t)(w > h ? w : h));
  if (!a || !t) {
    free(a);
    free(t);
    return;
  }
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) a[y * w + x] = px[y * stride + x * 4 + 3];
  auto solid = [&](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h && a[y * w + x] == 255; };
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      uint8_t& q = px[y * stride + x * 4 + 3];
      if (q && q < 255)
        for (int d = 1; d <= 2; d++)
          if ((solid(x - d, y) && solid(x + d, y)) || (solid(x, y - d) && solid(x, y + d))) q = 255;
    }
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) a[y * w + x] = px[y * stride + x * 4 + 3];
  auto pass = [&](int n, int step, int count, int lineStep) {  // media móvil de una fila o columna, en su sitio
    for (int line = 0; line < count; line++) {
      uint8_t* p = a + line * lineStep;
      for (int i = 0; i < n; i++) t[i] = p[i * step];
      int sum = 0;
      for (int i = -r; i <= r; i++) sum += (i >= 0 && i < n) ? t[i] : 0;
      for (int i = 0; i < n; i++) {
        p[i * step] = (uint8_t)(sum / (2 * r + 1));
        int out = i - r, in = i + r + 1;
        sum += (in < n ? t[in] : 0) - (out >= 0 ? t[out] : 0);
      }
    }
  };
  for (int it = 0; it < 3; it++) {
    pass(w, 1, h, w);
    pass(h, w, w, 1);
  }
  const uint8_t cb = color & 0xff, cg = (color >> 8) & 0xff, cr = (color >> 16) & 0xff;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      uint8_t* q = px + y * stride + x * 4;
      int glow = a[y * w + x] * k / 255, core = q[3];
      int out = core + (255 - core) * glow / 255;  // lo encendido por encima del halo
      if (!out) continue;
      q[0] = cb;
      q[1] = cg;
      q[2] = cr;
      q[3] = (uint8_t)out;
    }
  free(a);
  free(t);
}

static void drawDigit(const Ctx& c, char ch, lv_color_t col, uint8_t glow, bool ghost) {
  const float grows[3] = {5.0f, 2.5f, 0.0f};
  const lv_opa_t haloOpa[3] = {(lv_opa_t)(glow * 0.22f), (lv_opa_t)(glow * 0.45f), 255};
  const char* on = segsOf(ch);
  for (int pass = glow ? 0 : 2; pass < 3; pass++)
    for (const char* sg = "abcdefg"; *sg; sg++) {
      bool lit = strchr(on, *sg) != nullptr;
      if (!lit && (pass < 2 || !ghost)) continue;
      float xs[6], ys[6];
      segPoly(*sg, xs, ys);
      hexa(c, xs, ys, col, lit ? haloOpa[pass] : (lv_opa_t)18, grows[pass]);
    }
}

static lv_draw_buf_t* glyph(char ch, int h, uint32_t color, uint8_t glow, bool ghost, bool soft = false) {
  int lru = -1;  // hueco libre o, si no hay, la menos usada
  for (int i = 0; i < (int)(sizeof(glyphs) / sizeof(glyphs[0])); i++) {
    Glyph& g = glyphs[i];
    if (g.buf && g.ch == ch && g.h == h && g.color == color && g.glow == glow && g.ghost == ghost && g.soft == soft) {
      g.used = ++glyphUse;
      return g.buf;
    }
    if (lru < 0 || (glyphs[lru].buf && (!g.buf || g.used < glyphs[lru].used))) lru = i;
  }
  Glyph& g = glyphs[lru];
  if (g.buf) {
    lv_image_cache_drop(g.buf);
    lv_draw_buf_destroy(g.buf);
    g.buf = nullptr;
  }
  float scale = h / 100.0f;
  int w = (int)ceilf((60 + 2 * CELL_MARGIN) * scale), hh = (int)ceilf((100 + 2 * CELL_MARGIN) * scale);
  lv_draw_buf_t* buf = lv_draw_buf_create((uint32_t)w, (uint32_t)hh, LV_COLOR_FORMAT_ARGB8888, 0);
  if (!buf) return nullptr;
  lv_draw_buf_clear(buf, nullptr);
  static lv_obj_t* canvas = nullptr;
  if (!canvas) {
    canvas = lv_canvas_create(lv_layer_sys());
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
  }
  lv_canvas_set_draw_buf(canvas, buf);
  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);
  Ctx c = {&layer, scale, CELL_MARGIN * scale, CELL_MARGIN * scale, tanf(6.0f * 3.14159265f / 180.0f)};
  if (!soft) {
    drawDigit(c, ch, lv_color_hex(color), glow, ghost);
    lv_canvas_finish_layer(canvas, &layer);
  } else {
    // Lo encendido, macizo; luego el halo difuminado debajo y, al final, los segmentos apagados («fantasma»). Los dos
    // puntos (ch = ':') son dos círculos sin «fantasma».
    const char* on = ch == ':' ? "abcdefg" : segsOf(ch);
    if (ch == ':') {
      dot(c, 10, 32, lv_color_hex(color), 255, 5);
      dot(c, 10, 68, lv_color_hex(color), 255, 5);
    } else
      for (const char* sg = "abcdefg"; *sg; sg++) {
        if (!strchr(on, *sg)) continue;
        float xs[6], ys[6];
        segPoly(*sg, xs, ys);
        hexa(c, xs, ys, lv_color_hex(color), 255, 0);
      }
    lv_canvas_finish_layer(canvas, &layer);
    if (glow) blurGlow(buf, color, (int)lroundf(3.2f * scale), glow);
    if (ghost) {
      lv_canvas_init_layer(canvas, &layer);
      for (const char* sg = "abcdefg"; *sg; sg++) {
        if (strchr(on, *sg)) continue;
        float xs[6], ys[6];
        segPoly(*sg, xs, ys);
        hexa(c, xs, ys, lv_color_hex(color), 18, 0);
      }
      lv_canvas_finish_layer(canvas, &layer);
    }
  }
  g = {ch, (int16_t)h, color, glow, ghost, soft, buf, ++glyphUse};
  return buf;
}

// Suelta las cifras ya pintadas de alto `h` salvo las de `keep` (las del reloj de reposo ocupan ~260 KB cada una:
// solo se guardan las que se ven, y fuera del reposo ninguna).
static void forget(int h, const char* keep = "") {
  for (Glyph& g : glyphs)
    if (g.buf && g.h == h && !strchr(keep, g.ch)) {
      lv_image_cache_drop(g.buf);
      lv_draw_buf_destroy(g.buf);
      g.buf = nullptr;
    }
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
  // Las cifras, ya pintadas (glyph); los dos puntos se dibujan aquí (son dos círculos).
  const float grows[3] = {5.0f, 2.5f, 0.0f};
  const lv_opa_t haloOpa[3] = {(lv_opa_t)(d->glow * 0.22f), (lv_opa_t)(d->glow * 0.45f), 255};
  float x = 0;
  int h = lv_area_get_height(&co);
  for (const char* ch = d->text; *ch; ch++) {
    Ctx cc = c;
    cc.ox += x * scale;
    if ((*ch == ':' && !d->soft) || *ch == '.') {  // con `soft`, los dos puntos van ya pintados, como las cifras
      for (int pass = d->glow ? 0 : 2; pass < 3; pass++) {
        lv_opa_t o = pass == 2 ? (d->colonDim ? (lv_opa_t)38 : 255) : (lv_opa_t)(haloOpa[pass] * (d->colonDim ? 0.15f : 1.0f));
        if (*ch == ':') dot(cc, 10, 32, col, o, 5 + grows[pass]);
        dot(cc, 10, *ch == ':' ? 68 : 92, col, o, 5 + grows[pass]);
      }
      x += 20;
      continue;
    }
    lv_draw_buf_t* g = glyph(*ch, h, d->color, d->glow, d->ghost, d->soft);
    if (g) {
      lv_draw_image_dsc_t id;
      lv_draw_image_dsc_init(&id);
      id.src = g;
      if (*ch == ':' && d->colonDim) id.opa = 38;  // dos puntos tenues, con su halo
      int32_t gx = (int32_t)lroundf(cc.ox - CELL_MARGIN * scale), gy = (int32_t)lroundf(cc.oy - CELL_MARGIN * scale);
      lv_area_t a = {gx, gy, gx + (int32_t)g->header.w - 1, gy + (int32_t)g->header.h - 1};
      lv_draw_image(c.layer, &id, &a);
    }
    x += *ch == ':' ? 20 : 66;
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
