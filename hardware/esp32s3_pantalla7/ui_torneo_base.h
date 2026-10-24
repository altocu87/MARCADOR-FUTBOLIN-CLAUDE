// MARCADOR FUTBOLÍN V3 · TORNEOS en la pantalla de 7": piezas comunes de sus pantallas.
// -------------------------------------------------------------------------------------------------
// Las pantallas de torneo (ui_torneo_nuevo.h, _creador.h, ui_torneo.h, _ficha.h, _ceremonia.h) son las de la app
// (src/ui/screens/TournamentScreens.tsx) con el estilo austero de la placa:
//   - Cada pantalla es una pantalla LVGL que se crea una vez al entrar (go()). Al tocar solo cambia lo tocado:
//     el color de los botones de opción (Opt), el texto de un valor (Val) o si algo se ve (Vis), con refresh().
//   - Todo dibujado por la placa (sin imágenes nuevas): escudos, copas y banners son insignias de color con sus
//     iniciales; los iconos (▶ ▲ ▼ ✓ papelera, copa, corona…) son trazos de LVGL, porque la letra no los trae.
//   - Solo letras de fuentes.txt (peso y tamaño): una actualización por Wi-Fi no lleva letras nuevas.
//   - Fotos de los jugadores con la caché reducida y redondeada de la placa (assets::photo); si no hay, iniciales.
// Las reglas, la ficha y el guardado son del contrato (s3_torneos.h): aquí no se calcula nada de los torneos.
// Se incluye en el sketch después de s3_jugadores.h, s3_partidos.h, s3_progresion.h, s3_equipos.h y ui_fichas.h.

#ifndef MFV3_UI_TORNEO_BASE_H
#define MFV3_UI_TORNEO_BASE_H

#include "s3_memory.h"
#include "ui_base.h"
#include "ui_fichas.h"
#include "ui_cartel.h"
#include "s3_jugadores.h"
#include "s3_partidos.h"
#include "s3_progresion.h"
#include "s3_equipos.h"
#include "s3_torneos_pantallas.h"

namespace tor {

using torneos::Format;

struct Hooks {
  void (*home)() = nullptr;                    // volver a Inicio
  void (*players)() = nullptr;                 // + JUGADOR: AJUSTES → JUGADORES
  void (*play)(int16_t fixture) = nullptr;     // jugar ese cruce del torneo abierto (el .ino arma el partido)
  void (*start)() = nullptr;                   // tras el cartel de fase: empieza el partido
  bool (*pending)() = nullptr;                 // la placa guarda un partido a medias de este torneo
  void (*resume)() = nullptr;                  // seguir ese partido
  void (*removeMatch)(const char* id) = nullptr;  // borrar un partido del historial (repetir el último)
  void (*sound)(uint8_t what) = nullptr;       // 0 campeón, 1 siguiente puesto, 2 sirena (el último)
  uint64_t (*nowMs)() = nullptr;               // hora (ms UTC; 0 sin hora)
};
static Hooks hooks;

static jug::Store* store = nullptr;

enum class View : uint8_t {
  Players, Kind, Empty, Choose, Pairs, Keyboard, Creator, Detail, Rules, Honours, Competition, Ceremony, Poster
};
static View view = View::Players;
static lv_obj_t* scr = nullptr;
static lv_obj_t* pop = nullptr;  // ventana encima (ayuda, confirmación, colores): una a la vez

// El torneo abierto (en la PSRAM: equipos y cruces con calloc, torneos::alloc).
static torneos::Tournament cur;
static bool curOk = false;

static void go(View v);
static uint64_t nowMs() { return hooks.nowMs ? hooks.nowMs() : 0; }

// ---------------------------------------------------------------- colores
namespace col {
const uint32_t LIGA = 0x62d6ff, COPA = 0xf2c94c, POOL = 0xd65cff;
const uint32_t GOLD = ui::color::RANKED, OK = ui::color::OK, BAD = ui::color::DANGER;
const uint32_t PANEL = 0x0b1424, BAND = 0x0b1220, ON_BG = 0x1aa3dc, ON_INK = 0x03121e;
}  // namespace col
static uint32_t kindColor(Format f) { return f == Format::League ? col::LIGA : f == Format::Bracket ? col::COPA : col::POOL; }
static const char* kindTitle(Format f) { return f == Format::League ? "LIGA" : f == Format::Bracket ? "COPA" : "POOL"; }
static const char* kindNoun(Format f) { return f == Format::League ? "liga" : f == Format::Bracket ? "copa" : "pool"; }
static const char* kindName(Format f) { return f == Format::League ? "Liga" : f == Format::Bracket ? "Copa" : "Pool"; }

// Escudos de la app (Crest.tsx): sin imágenes, «emoji:🦅»… con el color de su posición. Aquí, la insignia de color.
static const char* const EMOJI_LOGOS[24] = {"🦅", "🐺", "🦁", "🐯", "🦈", "🐉", "🔥", "⚡", "🚀", "👑", "💀", "🌪️",
                                            "🐍", "🦂", "🐻", "🦊", "🐝", "🐙", "🎯", "💎", "🛡️", "⚔️", "🌟", "☄️"};
static const uint32_t LOGO_COLORS[8] = {0x62d6ff, 0xff5cd6, 0xffcc33, 0x3dff9a, 0xb46cff, 0xff8a2a, 0xff4d6d, 0x8fd0ff};
// Copas (CUP_TINTS de la app): «trofeo:oro»…
static const char* const CUP_IDS[7] = {"oro", "plata", "bronce", "cian", "morado", "verde", "rojo"};
static const uint32_t CUP_COLORS[7] = {0xf2c94c, 0xd8dee8, 0xc98a54, 0x62d6ff, 0xb46cff, 0x3ddc97, 0xff5a6e};
// CONTRATO: los banners de la app son imágenes; la placa guarda «color:<n>» (0–7, LOGO_COLORS) y la app no lo
// pinta (BannerArt sin imagen no enseña nada). Que el coordinador lo confirme o proponga otro id.

static uint32_t hashOf(const char* s) {
  uint32_t h = 2166136261u;
  for (; s && *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
  return h;
}
static int logoIndex(const char* id) {
  if (!id || strncmp(id, "emoji:", 6)) return -1;
  for (int i = 0; i < 24; i++)
    if (!strcmp(id + 6, EMOJI_LOGOS[i])) return i;
  return -1;
}
static uint32_t logoColor(const char* id, const char* name) {
  int i = logoIndex(id);
  if (i >= 0) return LOGO_COLORS[i % 8];
  return LOGO_COLORS[hashOf(id && id[0] ? id : name) % 8];
}
static int cupIndex(const char* id) {
  if (!id || strncmp(id, "trofeo:", 7)) return 0;
  for (int i = 0; i < 7; i++)
    if (!strcmp(id + 7, CUP_IDS[i])) return i;
  return 0;
}
static uint32_t cupColor(const char* id) { return CUP_COLORS[cupIndex(id)]; }
static int bannerIndex(const char* id) { return id && !strncmp(id, "color:", 6) ? atoi(id + 6) % 8 : -1; }

// ---------------------------------------------------------------- letras
// Tamaños que hay en la placa para cada peso (fuentes.txt): otro caería en la letra pequeña de LVGL.
static uint16_t fit(uint16_t weight, uint16_t want) {
  static const uint16_t W400[] = {11, 12, 13, 14, 16, 22}, W700[] = {12, 14, 15}, W800[] = {12, 13, 14, 17, 18, 20, 23},
                        W900[] = {11, 12, 13, 14, 15, 20, 24, 26, 30, 34, 38};
  const uint16_t* l = weight == 400 ? W400 : weight == 700 ? W700 : weight == 800 ? W800 : W900;
  int n = weight == 400 ? 6 : weight == 700 ? 3 : weight == 800 ? 7 : 11;
  uint16_t s = l[0];
  for (int i = 0; i < n; i++)
    if (l[i] <= want) s = l[i];
  return s;
}
static int32_t textW(const char* t, uint16_t weight, uint16_t size, int32_t space = 0) {
  lv_point_t p;
  lv_text_get_size(&p, t, assets::font(weight, size), space, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return p.x;
}
// Una línea con «…» si no cabe.
static lv_obj_t* line(lv_obj_t* parent, const char* txt, uint16_t weight, uint16_t size, uint32_t c, int x, int y, int w,
                      lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t* l = ui::text(parent, txt, weight, size, c);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_size(l, w, lv_font_get_line_height(assets::font(weight, size)));
  lv_obj_set_style_text_align(l, align, 0);
  lv_obj_set_pos(l, x, y);
  return l;
}
// Texto que se parte en varias líneas.
static lv_obj_t* para(lv_obj_t* parent, const char* txt, uint16_t weight, uint16_t size, uint32_t c, int x, int y, int w,
                      lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t* l = ui::text(parent, txt, weight, size, c);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, align, 0);
  lv_obj_set_pos(l, x, y);
  return l;
}
// Rótulo de sección (.label de la app): mayúsculas pequeñas y espaciadas.
static lv_obj_t* caption(lv_obj_t* parent, const char* s, int x, int y, uint32_t c = ui::color::TEXT2) {
  lv_obj_t* l = ui::text(parent, s, 800, 13, c, 1.4f);
  lv_obj_set_pos(l, x, y);
  return l;
}

// ---------------------------------------------------------------- iconos dibujados
enum class Icon : uint8_t { Play, Up, Down, Check, Trash, Gear, Undo, Again, Crown, Trophy, Sub, Pause, Table, Ladder, Plus };
static void iconTri(lv_layer_t* L, lv_color_t c, float x0, float y0, float x1, float y1, float x2, float y2) {
  lv_draw_triangle_dsc_t d;
  lv_draw_triangle_dsc_init(&d);
  d.color = c;
  d.opa = LV_OPA_COVER;
  d.p[0] = {x0, y0};
  d.p[1] = {x1, y1};
  d.p[2] = {x2, y2};
  lv_draw_triangle(L, &d);
}
static void iconLine(lv_layer_t* L, lv_color_t c, float w, float x0, float y0, float x1, float y1) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = c;
  d.width = (int32_t)(w < 1 ? 1 : w);
  d.round_start = d.round_end = 1;
  d.p1 = {x0, y0};
  d.p2 = {x1, y1};
  lv_draw_line(L, &d);
}
static void iconRect(lv_layer_t* L, lv_color_t c, float x0, float y0, float x1, float y1, int r = 0, bool fill = true, int bw = 2) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = r;
  if (fill) {
    d.bg_color = c;
    d.bg_opa = LV_OPA_COVER;
  } else {
    d.bg_opa = LV_OPA_TRANSP;
    d.border_color = c;
    d.border_width = bw;
    d.border_opa = LV_OPA_COVER;
  }
  lv_area_t a = {(int32_t)x0, (int32_t)y0, (int32_t)x1, (int32_t)y1};
  lv_draw_rect(L, &d, &a);
}
static void iconArc(lv_layer_t* L, lv_color_t c, float cx, float cy, float r, float w, float a0, float a1) {
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  d.color = c;
  d.width = (int32_t)(w < 1 ? 1 : w);
  d.center = {(int32_t)cx, (int32_t)cy};
  d.radius = (uint16_t)r;
  d.start_angle = a0;
  d.end_angle = a1;
  d.rounded = 1;
  lv_draw_arc(L, &d);
}
static void iconDraw(lv_event_t* e) {
  lv_obj_t* o = lv_event_get_current_target_obj(e);
  lv_layer_t* L = lv_event_get_layer(e);
  uintptr_t u = (uintptr_t)lv_obj_get_user_data(o);
  Icon k = (Icon)(u & 0xFF);
  lv_color_t c = lv_color_hex((uint32_t)(u >> 8) & 0xFFFFFF);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  float x = (float)a.x1, y = (float)a.y1, s = (float)lv_area_get_width(&a), w = s / 9;
  auto X = [&](float f) { return x + f * s; };
  auto Y = [&](float f) { return y + f * s; };
  switch (k) {
    case Icon::Play: iconTri(L, c, X(0.18f), Y(0.08f), X(0.18f), Y(0.92f), X(0.92f), Y(0.5f)); break;
    case Icon::Up: iconTri(L, c, X(0.5f), Y(0.1f), X(0.05f), Y(0.85f), X(0.95f), Y(0.85f)); break;
    case Icon::Down: iconTri(L, c, X(0.05f), Y(0.15f), X(0.95f), Y(0.15f), X(0.5f), Y(0.9f)); break;
    case Icon::Check:
      iconLine(L, c, w * 1.4f, X(0.12f), Y(0.52f), X(0.4f), Y(0.8f));
      iconLine(L, c, w * 1.4f, X(0.4f), Y(0.8f), X(0.9f), Y(0.2f));
      break;
    case Icon::Plus:
      iconLine(L, c, w * 1.3f, X(0.5f), Y(0.12f), X(0.5f), Y(0.88f));
      iconLine(L, c, w * 1.3f, X(0.12f), Y(0.5f), X(0.88f), Y(0.5f));
      break;
    case Icon::Trash:
      iconLine(L, c, w, X(0.12f), Y(0.26f), X(0.88f), Y(0.26f));
      iconLine(L, c, w, X(0.38f), Y(0.26f), X(0.4f), Y(0.12f));
      iconLine(L, c, w, X(0.4f), Y(0.12f), X(0.6f), Y(0.12f));
      iconLine(L, c, w, X(0.6f), Y(0.12f), X(0.62f), Y(0.26f));
      iconLine(L, c, w, X(0.22f), Y(0.26f), X(0.28f), Y(0.9f));
      iconLine(L, c, w, X(0.28f), Y(0.9f), X(0.72f), Y(0.9f));
      iconLine(L, c, w, X(0.72f), Y(0.9f), X(0.78f), Y(0.26f));
      iconLine(L, c, w * 0.8f, X(0.42f), Y(0.42f), X(0.42f), Y(0.74f));
      iconLine(L, c, w * 0.8f, X(0.58f), Y(0.42f), X(0.58f), Y(0.74f));
      break;
    case Icon::Gear:
      iconArc(L, c, X(0.5f), Y(0.5f), s * 0.3f, w * 1.3f, 0, 360);
      for (int i = 0; i < 8; i++) {
        float an = i * 3.14159f / 4;
        iconLine(L, c, w * 1.2f, X(0.5f + 0.3f * cosf(an)), Y(0.5f + 0.3f * sinf(an)), X(0.5f + 0.46f * cosf(an)),
                 Y(0.5f + 0.46f * sinf(an)));
      }
      break;
    case Icon::Undo:
    case Icon::Again: {
      bool undo = k == Icon::Undo;
      iconArc(L, c, X(0.5f), Y(0.55f), s * 0.34f, w, undo ? 200 : 300, undo ? 110 : 240);
      if (undo) iconTri(L, c, X(0.04f), Y(0.42f), X(0.36f), Y(0.36f), X(0.2f), Y(0.7f));
      else iconTri(L, c, X(0.62f), Y(0.02f), X(0.9f), Y(0.26f), X(0.56f), Y(0.36f));
      break;
    }
    case Icon::Crown:
      iconRect(L, c, X(0.14f), Y(0.62f), X(0.86f), Y(0.8f), 2);
      iconTri(L, c, X(0.14f), Y(0.64f), X(0.14f), Y(0.22f), X(0.38f), Y(0.64f));
      iconTri(L, c, X(0.32f), Y(0.64f), X(0.5f), Y(0.14f), X(0.68f), Y(0.64f));
      iconTri(L, c, X(0.62f), Y(0.64f), X(0.86f), Y(0.22f), X(0.86f), Y(0.64f));
      break;
    case Icon::Trophy:
      iconRect(L, c, X(0.24f), Y(0.08f), X(0.76f), Y(0.2f), 0);
      iconTri(L, c, X(0.24f), Y(0.18f), X(0.76f), Y(0.18f), X(0.5f), Y(0.62f));
      iconRect(L, c, X(0.24f), Y(0.14f), X(0.76f), Y(0.42f), (int)(s * 0.14f));
      iconArc(L, c, X(0.24f), Y(0.3f), s * 0.14f, w * 0.9f, 90, 270);
      iconArc(L, c, X(0.76f), Y(0.3f), s * 0.14f, w * 0.9f, 270, 90);
      iconRect(L, c, X(0.45f), Y(0.5f), X(0.55f), Y(0.74f), 0);
      iconRect(L, c, X(0.28f), Y(0.74f), X(0.72f), Y(0.9f), 2);
      break;
    case Icon::Sub:
      iconLine(L, c, w, X(0.22f), Y(0.1f), X(0.22f), Y(0.62f));
      iconLine(L, c, w, X(0.22f), Y(0.62f), X(0.78f), Y(0.62f));
      iconTri(L, c, X(0.66f), Y(0.4f), X(0.95f), Y(0.62f), X(0.66f), Y(0.84f));
      break;
    case Icon::Pause:
      iconRect(L, c, X(0.2f), Y(0.12f), X(0.4f), Y(0.88f), 2);
      iconRect(L, c, X(0.6f), Y(0.12f), X(0.8f), Y(0.88f), 2);
      break;
    case Icon::Table:
      for (int i = 0; i < 3; i++) {
        iconRect(L, c, X(0.08f), Y(0.16f + i * 0.28f), X(0.22f), Y(0.3f + i * 0.28f), 2);
        iconRect(L, c, X(0.3f), Y(0.16f + i * 0.28f), X(0.92f - i * 0.12f), Y(0.3f + i * 0.28f), 2);
      }
      break;
    case Icon::Ladder:
      for (int i = 0; i < 3; i++) iconRect(L, c, X(0.08f + i * 0.3f), Y(0.62f - i * 0.24f), X(0.32f + i * 0.3f), Y(0.9f), 2);
      break;
  }
}
static lv_obj_t* icon(lv_obj_t* parent, Icon k, int x, int y, int s, uint32_t c) {
  lv_obj_t* o = ui::box(parent, x, y, s, s);
  lv_obj_set_user_data(o, (void*)(uintptr_t)(((uintptr_t)(c & 0xFFFFFF) << 8) | (uint8_t)k));
  lv_obj_add_event_cb(o, iconDraw, LV_EVENT_DRAW_MAIN, nullptr);
  return o;
}

// ---------------------------------------------------------------- botones, opciones y valores
using fichas::Kind;
static lv_obj_t* tbtn(lv_obj_t* parent, int x, int y, int w, int h, const char* txt, uint16_t size, Kind k, lv_event_cb_t cb,
                     void* user = nullptr) {
  lv_obj_t* b = fichas::btn(parent, x, y, w, h, txt, fit(800, size), k, cb, user);
  lv_obj_t* l = lv_obj_get_child(b, 0);
  if (textW(txt, 800, fit(800, size)) > w - 12) {  // que no se salga: se parte en dos líneas
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, w - 12);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
  }
  return b;
}
// Botón con un icono delante del texto (▶ SIGUIENTE, ⚙ REGLAS…).
static lv_obj_t* iconBtn(lv_obj_t* parent, int x, int y, int w, int h, Icon ic, const char* txt, uint16_t size, Kind k,
                         lv_event_cb_t cb, void* user = nullptr, uint32_t icColor = 0) {
  lv_obj_t* b = fichas::btn(parent, x, y, w, h, txt, fit(800, size), k, cb, user);
  lv_obj_t* l = lv_obj_get_child(b, 0);
  int s = size + 2, tw = textW(txt, 800, fit(800, size));
  // Que quepa: la letra un poco más pequeña si hace falta.
  while (tw > w - 24 - s - 10 && size > 12) {
    size = fit(800, (uint16_t)(fit(800, size) - 1));
    lv_obj_set_style_text_font(l, assets::font(800, size), 0);
    tw = textW(txt, 800, size);
  }
  int gap = txt[0] ? 10 : 0, total = s + gap + tw;
  lv_obj_align(l, LV_ALIGN_LEFT_MID, (w - total) / 2 + s + gap, 0);
  uint32_t c = icColor ? icColor : k == Kind::Primary ? col::ON_INK : k == Kind::Danger ? 0xffd2d8 : ui::color::TEXT;
  icon(b, ic, (w - total) / 2, (h - s) / 2, s, c);
  return b;
}
// Botón grande de neón verde (.neon-go de la app): ▶ SIGUIENTE PARTIDO, ▶ CONTINUAR…
static lv_obj_t* neonGo(lv_obj_t* parent, int x, int y, int w, int h, const char* txt, uint16_t size, lv_event_cb_t cb,
                        void* user = nullptr) {
  lv_obj_t* b = iconBtn(parent, x, y, w, h, Icon::Play, txt, size, Kind::Normal, cb, user, 0x7dffb3);
  lv_obj_set_style_bg_color(b, ui::hex(0x0c3b2a), 0);
  lv_obj_set_style_bg_grad_color(b, ui::hex(0x062016), 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_color(b, ui::hex(0x0a2a1e), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, ui::hex(0x3dff9a), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(b, 0), ui::hex(0x7dffb3), 0);
  return b;
}

struct Opt {
  lv_obj_t* b;
  bool (*on)(int);
  bool (*dis)(int);
  int arg;
  int8_t shown;
};
struct Val {
  lv_obj_t* l;
  void (*fill)(char*, size_t, int);
  int arg;
  uint32_t color, shownColor;
};
struct Vis {
  lv_obj_t* o;
  bool (*when)(int);
  int arg;
  int8_t shown;
};
static s3mem::ExternalArray<Opt, 96> opts;
static Val vals[24];
static Vis viss[32];
static uint8_t optCount = 0, valCount = 0, visCount = 0;

static void paintOpt(Opt& o) {
  bool d = o.dis && o.dis(o.arg);
  bool v = !d && o.on(o.arg);
  int8_t st = d ? 2 : v ? 1 : 0;
  if (o.shown == st) return;
  o.shown = st;
  lv_obj_set_style_bg_color(o.b, ui::hex(v ? col::ON_BG : ui::color::CARD), 0);
  lv_obj_set_style_border_color(o.b, ui::hex(v ? ui::color::ACCENT : ui::color::LINE), 0);
  lv_obj_set_style_border_width(o.b, v ? 2 : 1, 0);
  lv_obj_t* l = lv_obj_get_child(o.b, 0);
  if (l) lv_obj_set_style_text_color(l, ui::hex(v ? col::ON_INK : d ? ui::color::TEXT3 : ui::color::TEXT), 0);
}
static void paintVal(Val& v) {
  char b[160];
  v.fill(b, sizeof(b), v.arg);
  const char* t = b[0] == '!' ? b + 1 : b;  // «!» delante: aviso (dorado)
  uint32_t c = b[0] == '!' ? col::GOLD : v.color;
  if (strcmp(lv_label_get_text(v.l), t)) lv_label_set_text(v.l, t);
  if (v.shownColor != c) {
    v.shownColor = c;
    lv_obj_set_style_text_color(v.l, ui::hex(c), 0);
  }
}
static void refresh() {
  for (uint8_t i = 0; i < optCount; i++) paintOpt(opts[i]);
  for (uint8_t i = 0; i < valCount; i++) paintVal(vals[i]);
  for (uint8_t i = 0; i < visCount; i++) {
    int8_t v = viss[i].when(viss[i].arg) ? 1 : 0;
    if (viss[i].shown == v) continue;
    viss[i].shown = v;
    ui::setVisible(viss[i].o, v);
  }
}
static int argOf(lv_event_t* e) { return (int)(intptr_t)lv_event_get_user_data(e); }

static lv_obj_t* opt(lv_obj_t* parent, int x, int y, int w, int h, const char* txt, uint16_t size, bool (*on)(int), int arg,
                     lv_event_cb_t cb, bool (*dis)(int) = nullptr) {
  lv_obj_t* b = tbtn(parent, x, y, w, h, txt, size, Kind::Normal, cb, (void*)(intptr_t)arg);
  if (optCount < opts.capacity()) opts[optCount++] = {b, on, dis, arg, -1};
  return b;
}
static lv_obj_t* val(lv_obj_t* parent, int x, int y, int w, uint16_t weight, uint16_t size, void (*fill)(char*, size_t, int),
                     int arg = 0, uint32_t color = ui::color::TEXT2, lv_text_align_t align = LV_TEXT_ALIGN_LEFT, bool wrap = false) {
  lv_obj_t* l = ui::text(parent, "", weight, size, color);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, align, 0);
  lv_label_set_long_mode(l, wrap ? LV_LABEL_LONG_MODE_WRAP : LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_pos(l, x, y);
  if (valCount < sizeof(vals) / sizeof(vals[0])) vals[valCount++] = {l, fill, arg, color, color};
  return l;
}
static void visWhen(lv_obj_t* o, bool (*when)(int), int arg = 0) {
  if (visCount < sizeof(viss) / sizeof(viss[0])) viss[visCount++] = {o, when, arg, -1};
}

// ---------------------------------------------------------------- cajas
static lv_obj_t* panel(lv_obj_t* parent, int x, int y, int w, int h, uint32_t border = ui::color::LINE, uint32_t bg = col::PANEL) {
  lv_obj_t* p = ui::box(parent, x, y, w, h);
  lv_obj_set_style_radius(p, ui::RADIUS, 0);
  lv_obj_set_style_bg_color(p, ui::hex(bg), 0);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(p, 1, 0);
  lv_obj_set_style_border_color(p, ui::hex(border), 0);
  return p;
}
// Lista que se desliza (solo en las listas: jugadores, jornadas, palmarés…).
static lv_obj_t* scroller(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* g = ui::box(parent, x, y, w, h);
  lv_obj_add_flag(g, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(g, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(g, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_width(g, 4, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(g, ui::hex(ui::color::LINE), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(g, LV_OPA_COVER, LV_PART_SCROLLBAR);
  return g;
}
// Pastilla pequeña («Pista 1», «Jornada 3»…). Devuelve su ancho.
static int chip(lv_obj_t* parent, const char* t, int x, int y, uint32_t ink = ui::color::TEXT, uint32_t border = ui::color::LINE,
                uint16_t size = 13) {
  size = fit(800, size);
  int w = textW(t, 800, size) + 18;
  lv_obj_t* c = ui::box(parent, x, y, w, size + 11);
  lv_obj_set_style_radius(c, 999, 0);
  lv_obj_set_style_bg_color(c, ui::hex(0x0c1a2e), 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_border_color(c, ui::hex(border), 0);
  lv_obj_t* l = ui::text(c, t, 800, size, ink);
  lv_obj_center(l);
  return w;
}

// Volver: recuadro con la flecha (como RANKING).
static void backBtn(lv_obj_t* parent, int x, int y, lv_event_cb_t cb) {
  lv_obj_t* back = ui::box(parent, x, y, 48, 48);
  lv_obj_set_style_radius(back, ui::RADIUS_SM, 0);
  lv_obj_set_style_border_width(back, 1, 0);
  lv_obj_set_style_border_color(back, ui::hex(ui::color::LINE), 0);
  lv_obj_set_style_bg_color(back, ui::hex(ui::color::CARD), 0);
  lv_obj_set_style_bg_color(back, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
  static lv_point_precise_t chevron[3] = {{27, 14}, {19, 24}, {27, 34}};
  lv_obj_t* arrow = lv_line_create(back);
  lv_line_set_points(arrow, chevron, 3);
  lv_obj_set_style_line_width(arrow, 3, 0);
  lv_obj_set_style_line_rounded(arrow, true, 0);
  lv_obj_set_style_line_color(arrow, ui::hex(ui::color::TEXT), 0);
  lv_obj_remove_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(back, cb, LV_EVENT_CLICKED, nullptr);
}

// Arriba: franja con volver, el título y su subtítulo. Devuelve el borde derecho del texto (para lo de al lado).
static int header(const char* title, const char* sub, lv_event_cb_t back, uint32_t titleColor = ui::color::TEXT) {
  lv_obj_t* band = ui::box(scr, 0, 0, 800, 64);
  lv_obj_set_style_bg_color(band, ui::hex(col::BAND), 0);
  lv_obj_set_style_bg_opa(band, 235, 0);
  lv_obj_set_style_border_side(band, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(band, 1, 0);
  lv_obj_set_style_border_color(band, ui::hex(ui::color::LINE), 0);
  backBtn(scr, 12, 8, back);
  lv_obj_t* t = ui::text(scr, title, 900, 24, titleColor, 0.5f);
  lv_obj_set_pos(t, 74, 17);
  lv_obj_update_layout(t);
  int x = 74 + lv_obj_get_width(t);
  if (sub && sub[0]) {
    lv_obj_t* s = ui::text(scr, sub, 800, 13, ui::color::TEXT2, 0.8f);
    lv_obj_set_pos(s, x + 12, 25);
    lv_obj_update_layout(s);
    x += 12 + lv_obj_get_width(s);
  }
  return x;
}

// Franja de abajo (aviso y botones para seguir).
static lv_obj_t* footer() {
  lv_obj_t* band = ui::box(scr, 0, 400, 800, 80);
  lv_obj_set_style_bg_color(band, ui::hex(col::BAND), 0);
  lv_obj_set_style_bg_opa(band, 235, 0);
  lv_obj_set_style_border_side(band, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(band, 1, 0);
  lv_obj_set_style_border_color(band, ui::hex(ui::color::LINE), 0);
  return band;
}

// ---------------------------------------------------------------- ventanas encima
static void onPopClose(lv_event_t*) {
  if (pop) lv_obj_delete(pop);
  pop = nullptr;
}
static lv_obj_t* popCard(int w, int h, uint32_t border) {
  onPopClose(nullptr);
  pop = ui::box(scr, 0, 0, 800, 480);
  lv_obj_set_style_bg_color(pop, ui::hex(0x000000), 0);
  lv_obj_set_style_bg_opa(pop, 180, 0);
  lv_obj_add_flag(pop, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* card = ui::box(pop, (800 - w) / 2, (480 - h) / 2, w, h);
  lv_obj_set_style_bg_color(card, ui::hex(ui::color::CARD2), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, ui::RADIUS, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, ui::hex(border), 0);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);  // tocar dentro no cierra
  return card;
}
// «?»: la explicación; ENTENDIDO o tocar fuera la cierra.
struct HelpText {
  const char* title;
  const char* text;
};
static void onHelp(lv_event_t* e) {
  const HelpText* h = (const HelpText*)lv_event_get_user_data(e);
  lv_obj_t* card = popCard(620, 300, ui::color::ACCENT);
  lv_obj_add_event_cb(pop, onPopClose, LV_EVENT_CLICKED, nullptr);
  int y = 22;
  if (h->title && h->title[0]) {
    line(card, h->title, 900, 24, ui::color::ACCENT, 24, y, 572);
    y += 40;
  }
  para(card, h->text, 400, 22, ui::color::TEXT, 24, y, 572);
  tbtn(card, 180, 216, 260, 64, "ENTENDIDO", 20, Kind::Primary, onPopClose);
}
static lv_obj_t* helpBtn(lv_obj_t* parent, int x, int y, int size, const HelpText* h) {
  lv_obj_t* b = fichas::btn(parent, x, y, size, size, "?", 20, Kind::Normal, onHelp, (void*)h);
  lv_obj_set_style_radius(b, size / 2, 0);
  return b;
}

// Confirmación con dos botones: el «sí» llama a `yes`. Devuelve la tarjeta (para añadirle algo, como el partido).
static void (*askYesFn)() = nullptr;
static void onAskYes(lv_event_t*) {
  onPopClose(nullptr);
  if (askYesFn) askYesFn();
}
static lv_obj_t* ask(const char* title, const char* text, const char* no, const char* yes, void (*yesFn)(), bool danger = true,
                     int textY = 70) {
  lv_obj_t* card = popCard(640, 320, danger ? ui::color::DANGER : ui::color::ACCENT);
  askYesFn = yesFn;
  line(card, title, 900, 26, ui::color::TEXT, 24, 20, 592);
  para(card, text, 400, 16, ui::color::TEXT2, 24, textY, 592);
  tbtn(card, 24, 236, 286, 68, no, 20, Kind::Normal, onPopClose);
  tbtn(card, 330, 236, 286, 68, yes, 20, danger ? Kind::Danger : Kind::Primary, onAskYes);
  return card;
}

// ---------------------------------------------------------------- jugadores
static const jug::Player* playerOf(const char* id) { return store && id && id[0] ? store->findById(id) : nullptr; }
static const char* nameOf(const char* id) {
  const jug::Player* p = playerOf(id);
  return p ? p->name : "?";
}
static uint32_t codeOf(const char* id) {
  const jug::Player* p = playerOf(id);
  return p ? p->code : 0;
}
static int eloOf(const char* id) {
  const progresion::Player* p = progresion::of(codeOf(id));
  return p ? p->elo : 1200;
}
// Rango del jugador (marco de su foto): categoría 0–6, o -1 sin clasificatorios.
static int rankOf(const char* id) {
  const progresion::Player* p = progresion::of(codeOf(id));
  return p && p->rankedPlayed > 0 ? p->category : -1;
}

// Foto del jugador (o sus iniciales) en un cuadrado de `size`; `round`: redonda (Avatar) o con esquinas.
static lv_obj_t* face(lv_obj_t* parent, const char* id, int x, int y, int size, bool round = true) {
  const jug::Player* p = playerOf(id);
  int radius = round ? size / 2 : size / 6;
  char rel[40];
  const lv_image_dsc_t* pic = p && store->photoRel(p->code, rel, sizeof(rel)) ? assets::photo(rel, size, radius) : nullptr;
  if (pic) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, pic);
    lv_obj_set_pos(img, x, y);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
  }
  lv_obj_t* b = ui::box(parent, x, y, size, size);
  lv_obj_set_style_radius(b, radius, 0);
  lv_obj_set_style_bg_color(b, ui::hex(0x1d3a63), 0);
  lv_obj_set_style_bg_grad_color(b, ui::hex(0x0f2140), 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, ui::hex(ui::color::LINE), 0);
  char ini[8];
  jug::initials(p ? p->name : "?", ini, sizeof(ini));
  lv_obj_t* l = ui::text(b, ini, 900, fit(900, (uint16_t)(size * 0.4f)), ui::color::TEXT2);
  lv_obj_center(l);
  return b;
}
// Con su marco de rango (si tiene): un aro del color de su categoría.
static void faceRank(lv_obj_t* parent, const char* id, int x, int y, int size, bool round = true) {
  int r = rankOf(id);
  if (r >= 0) {
    lv_obj_t* ring = ui::box(parent, x - 4, y - 4, size + 8, size + 8);
    lv_obj_set_style_radius(ring, round ? LV_RADIUS_CIRCLE : size / 6 + 4, 0);
    lv_obj_set_style_border_width(ring, 3, 0);
    lv_obj_set_style_border_color(ring, ui::hex(progresion::CATEGORY_COLOR[r % 7]), 0);
    lv_obj_set_style_shadow_width(ring, 10, 0);
    lv_obj_set_style_shadow_color(ring, ui::hex(progresion::CATEGORY_COLOR[r % 7]), 0);
    lv_obj_set_style_shadow_opa(ring, 90, 0);
  }
  face(parent, id, x, y, size, round);
}
// Fotos de un equipo seguidas (de izquierda a derecha, o de derecha a izquierda desde `x` si `right`).
static int teamFaces(lv_obj_t* parent, const torneos::Tournament& t, int16_t team, int x, int y, int size, bool right = false) {
  if (team < 0 || team >= (int16_t)t.nTeams) return 0;
  const torneos::Team& tm = t.teams[team];
  int step = size + 3, w = tm.n * step - 3;
  for (uint8_t k = 0; k < tm.n; k++) face(parent, tm.player[k], right ? x - w + k * step : x + k * step, y, size);
  return w;
}

// Insignia (escudo, copa o banner de la app): su color con las iniciales del nombre.
static lv_obj_t* badge(lv_obj_t* parent, uint32_t c, const char* name, int x, int y, int size) {
  lv_obj_t* b = ui::box(parent, x, y, size, size);
  lv_obj_set_style_radius(b, size / 4, 0);
  lv_obj_set_style_bg_color(b, ui::hex(c), 0);
  lv_obj_set_style_bg_opa(b, 60, 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, ui::hex(c), 0);
  char ini[8];
  jug::initials(name && name[0] ? name : "T", ini, sizeof(ini));
  lv_obj_t* l = ui::text(b, ini, 900, fit(900, (uint16_t)(size * 0.38f)), c);
  lv_obj_center(l);
  return b;
}
static lv_obj_t* crest(lv_obj_t* parent, const char* logoId, const char* name, int x, int y, int size) {
  return badge(parent, logoColor(logoId, name), name, x, y, size);
}
// Copa: el trofeo dibujado en su color.
static lv_obj_t* cup(lv_obj_t* parent, const char* cupId, int x, int y, int size) { return icon(parent, Icon::Trophy, x, y, size, cupColor(cupId)); }

// ---------------------------------------------------------------- torneo abierto
static const char* teamName(const torneos::Tournament& t, int16_t team) {
  return team >= 0 && team < (int16_t)t.nTeams ? t.teams[team].name : "—";
}
static const partidos::Row* rowOf(const char* id) {
  if (!id || !id[0]) return nullptr;
  for (int i = partidos::count - 1; i >= 0; i--)
    if (!strcmp(partidos::rows[i].id, id)) return &partidos::rows[i];
  return nullptr;
}
static bool played_(const torneos::Fixture& f) { return f.winner >= 0 || f.draw || f.skipped; }
// Resultado apuntado de un partido del torneo (los que guarda el propio torneo, `played`).
static const torneos::Result* playedOf(const torneos::Tournament& t, const char* matchId) {
  if (!matchId || !matchId[0] || !t.played) return nullptr;
  for (uint16_t i = t.nPlayed; i-- > 0;)
    if (!strcmp(t.played[i].r.matchId, matchId)) return &t.played[i].r;
  return nullptr;
}
// Marcador de un cruce jugado («5–3», o la serie «2–1»); false si no hay.
static bool fixtureScore(const torneos::Fixture& f, char* out, size_t n) {
  if (f.hasSeries && f.bestOf > 1 && (f.serW || f.serB)) {
    snprintf(out, n, "%u–%u", f.serW, f.serB);
    return true;
  }
  const torneos::Result* r = playedOf(cur, f.matchId);
  if (!r) return false;
  snprintf(out, n, "%u–%u", r->w, r->b);
  return true;
}
// «a 5 goles» / «5 min» / «a 5 o 5 min» (rulesShort de la app).
static void rulesChip(const torneos::Rules& r, char* out, size_t n) {
  if (r.end == torneos::End::Goals) snprintf(out, n, "A %u goles", r.goals);
  else if (r.end == torneos::End::Time) snprintf(out, n, "%u min", r.minutes);
  else snprintf(out, n, "A %u o %u min", r.goals, r.minutes);
}
// rulesText de la app: «Gana quien llegue a 5 goles.»…
static void rulesText(const torneos::Rules& r, char* out, size_t n) {
  if (r.end == torneos::End::Goals) snprintf(out, n, "Gana quien llegue a %u goles.", r.goals);
  else if (r.end == torneos::End::Time) snprintf(out, n, "2 partes de %u minutos: gana quien lleve más goles.", r.minutes);
  else snprintf(out, n, "A %u goles o 2 partes de %u minutos, lo que llegue antes.", r.goals, r.minutes);
}
// Fecha corta («5 oct 2026») de una hora en ms.
static void shortDate(uint64_t ms, char* out, size_t n) {
  if (!ms) {
    snprintf(out, n, "Sin fecha");
    return;
  }
  static const char* const M[12] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sept", "oct", "nov", "dic"};
  reloj::Clock c = partidos::localOf((uint32_t)(ms / 1000));
  snprintf(out, n, "%u %s %u", c.day, M[(c.month + 11) % 12], c.year);
}
// Ids de los jugadores del torneo (los apuntados) para «¿Quién juega?» al volver.
static uint8_t entrantsOf(const torneos::Tournament& t, const char* (&out)[torneos::MAX_ENTRANTS]) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < t.nEntrants && n < torneos::MAX_ENTRANTS; i++) out[n++] = t.entrants[i];
  return n;
}

// ---------------------------------------------------------------- teclado (nombre del torneo, del último, de una pareja)
// Las teclas en mayúsculas; al escribir, la primera de cada palabra en mayúscula y las demás en minúscula, como
// en JUGADORES (ui_fichas.h). Con ´ antes, la vocal con tilde.
struct Keyboard {
  char text[64];
  uint8_t max;          // letras como mucho
  const char* title;
  void (*done)(const char* text);  // GUARDAR
  View back;
  bool accent;
};
static Keyboard kb;
static lv_obj_t* kbLabel = nullptr;
static void kbShow() {
  if (!kbLabel) return;
  char b[72];
  snprintf(b, sizeof(b), "%s|", kb.text);
  lv_label_set_text(kbLabel, b);
}
static size_t utf8Chars(const char* s) {
  size_t n = 0;
  for (; *s; s++)
    if (((uint8_t)*s & 0xC0) != 0x80) n++;
  return n;
}
static void onKbKey(lv_event_t* e) {
  const char* k = (const char*)lv_event_get_user_data(e);
  size_t n = strlen(kb.text);
  if (!strcmp(k, "BORRAR")) {
    while (n && ((uint8_t)kb.text[n - 1] & 0xC0) == 0x80) n--;
    if (n) n--;
    kb.text[n] = 0;
  } else if (!strcmp(k, "ESPACIO")) {
    if (n && kb.text[n - 1] != ' ' && n + 1 < sizeof(kb.text)) strcat(kb.text, " ");
  } else if (!strcmp(k, "´")) {
    kb.accent = !kb.accent;
    ui::toast(kb.accent ? "Ahora, la vocal con tilde" : "Sin tilde");
  } else {
    if (utf8Chars(kb.text) >= kb.max || n + strlen(k) + 2 >= sizeof(kb.text)) return;
    char c[4];
    snprintf(c, sizeof(c), "%s", k);
    if (kb.accent) {
      static const char* const PLAIN = "AEIOU";
      static const char* const ACUTE[5] = {"Á", "É", "Í", "Ó", "Ú"};
      const char* p = strchr(PLAIN, c[0]);
      if (p && !c[1]) snprintf(c, sizeof(c), "%s", ACUTE[p - PLAIN]);
      kb.accent = false;
    }
    bool upper = n == 0 || kb.text[n - 1] == ' ';
    if (!upper) {
      if (c[0] >= 'A' && c[0] <= 'Z') c[0] = (char)(c[0] + 32);
      else if ((uint8_t)c[0] == 0xC3) c[1] = (char)(c[1] + 0x20);  // Á→á, Ñ→ñ
    }
    strcat(kb.text, c);
  }
  kbShow();
}
static void onKbBack(lv_event_t*) { go(kb.back); }
static void onKbDone(lv_event_t*) {
  size_t n = strlen(kb.text);
  while (n && kb.text[n - 1] == ' ') kb.text[--n] = 0;
  if (kb.done) kb.done(kb.text);
}
// Ideas para el campo (título del último): botones que lo rellenan.
static const char* const* kbIdeas = nullptr;
static uint8_t kbIdeaCount = 0;
static void onKbIdea(lv_event_t* e) {
  snprintf(kb.text, sizeof(kb.text), "%s", (const char*)lv_event_get_user_data(e));
  kbShow();
}
static void buildKeyboard() {
  lv_obj_t* band = ui::box(scr, 0, 0, 800, 64);
  lv_obj_set_style_bg_color(band, ui::hex(col::BAND), 0);
  lv_obj_set_style_bg_opa(band, 235, 0);
  backBtn(scr, 12, 8, onKbBack);
  line(scr, kb.title, 900, 24, ui::color::TEXT, 74, 17, 520);
  tbtn(scr, 628, 8, 160, 48, "GUARDAR", 18, Kind::Primary, onKbDone);
  lv_obj_t* f = ui::box(scr, 16, 72, 768, 56);
  lv_obj_set_style_radius(f, ui::RADIUS_SM, 0);
  lv_obj_set_style_bg_color(f, ui::hex(ui::color::SURFACE), 0);
  lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(f, 2, 0);
  lv_obj_set_style_border_color(f, ui::hex(ui::color::ACCENT), 0);
  kbLabel = ui::text(f, "", 900, 30, ui::color::TEXT);
  lv_obj_align(kbLabel, LV_ALIGN_LEFT_MID, 16, 0);
  kbShow();
  static const char* const R0[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
  static const char* const R1[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", "Ñ"};
  static const char* const R2[] = {"Z", "X", "C", "V", "B", "N", "M", "´"};
  static const char* const R3[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
  const char* const* rows[4] = {R3, R0, R1, R2};
  const int counts[4] = {10, 10, 10, 8};
  int y0 = kbIdeaCount ? 186 : 140, kh = kbIdeaCount ? 52 : 60, dy = kh + 6;
  for (int r = 0; r < 4; r++)
    for (int i = 0; i < counts[r]; i++)
      fichas::btn(scr, 13 + i * 78, y0 + r * dy, 72, kh, rows[r][i], 23, Kind::Normal, onKbKey, (void*)rows[r][i], LV_EVENT_PRESSED);
  fichas::btn(scr, 13 + 8 * 78, y0 + 3 * dy, 150, kh, "BORRAR", 18, Kind::Normal, onKbKey, (void*)"BORRAR", LV_EVENT_PRESSED);
  fichas::btn(scr, 13, y0 + 4 * dy, 774, 480 - (y0 + 4 * dy) - 8, "ESPACIO", 18, Kind::Normal, onKbKey, (void*)"ESPACIO",
              LV_EVENT_PRESSED);
  if (kbIdeaCount) {
    int x = 16;
    for (uint8_t i = 0; i < kbIdeaCount; i++) {
      int w = textW(kbIdeas[i], 800, 14) + 28;
      fichas::btn(scr, x, 136, w, 42, kbIdeas[i], 14, Kind::Normal, onKbIdea, (void*)kbIdeas[i]);
      x += w + 8;
    }
  }
}
static void openKeyboard(const char* title, const char* text, uint8_t max, void (*done)(const char*), View back,
                         const char* const* ideas = nullptr, uint8_t nIdeas = 0) {
  snprintf(kb.text, sizeof(kb.text), "%s", text ? text : "");
  kb.max = max;
  kb.title = title;
  kb.done = done;
  kb.back = back;
  kb.accent = false;
  kbIdeas = ideas;
  kbIdeaCount = nIdeas;
  go(View::Keyboard);
}

}  // namespace tor

#endif  // MFV3_UI_TORNEO_BASE_H
