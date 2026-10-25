// MARCADOR FUTBOLÍN V3 · OPCIONES de la ficha de un jugador (PlayerOptions de src/ui/components/PlayerQr.tsx).
// -------------------------------------------------------------------------------------------------
// «⋯ OPCIONES» arriba a la derecha de la ficha abre una ventana sobre ella (el fondo se oscurece), como la de la app:
//   - su nombre, su foto (y INACTIVO si está de baja) y cuatro botones: VER QR, EDITAR, TÍTULO Y MELODÍA y
//     DESACTIVAR / ACTIVAR; abajo, CERRAR.
//   - VER QR: el QR de su perfil (el de AJUSTES); EDITAR: cambiar el nombre o la foto (el de AJUSTES → JUGADORES).
//     Volver de los dos lleva a la ficha como estaba.
//   - TÍTULO Y MELODÍA: «Título bajo el nombre» (Automático, Sin título y los que tiene) y «Melodía de victoria»
//     (las de la app, s3_melodias.h; al tocarla suena). Se guarda al momento; ATRÁS vuelve al menú.
//   - DESACTIVAR / ACTIVAR: le da de baja (sigue en la placa, fuera de las listas para elegir) o de alta, y cierra.
// Lo cambiado aquí se sube a la cuenta (lo mismo que guarda la app: active, titleId, anthem).
// Diferencias con la app: sin los iconos de los botones; EDITAR no cambia el apodo ni junta dos jugadores repetidos
// (dice que ya hay uno con ese nombre). La lista de títulos y melodías es UNA pieza dibujada (pocos objetos LVGL:
// cada uno gasta RAM interna) y sus datos van en la PSRAM. Solo letras de fuentes.txt.
// Se incluye después de ui_ficha.h (la ventana se pone encima de su pantalla).

#ifndef MFV3_UI_JUGADOR_OPCIONES_H
#define MFV3_UI_JUGADOR_OPCIONES_H

#include "ui_base.h"
#include "s3_jugadores.h"
#include "s3_melodias.h"
#include "s3_progresion.h"

namespace jopc {

struct Hooks {
  void (*qr)(uint32_t code) = nullptr;       // VER QR
  void (*edit)(uint32_t code) = nullptr;     // EDITAR
  void (*changed)(uint32_t code) = nullptr;  // de alta o baja, título o melodía: a la cuenta
  void (*play)(int anthem) = nullptr;        // oír una melodía (melodias::LIST)
  void (*redo)() = nullptr;                  // rehacer la ficha (cambió algo que se ve en ella)
};
static Hooks hooks;

enum class View : uint8_t { Menu, Style };
static jug::Store* store = nullptr;
static uint32_t code = 0;
static View view = View::Menu;
static lv_obj_t* back = nullptr;  // fondo oscuro (y la ventana dentro), sobre la ficha
static bool dirty = false;        // la ficha tiene que rehacerse al cerrar

// ---------------------------------------------------------------- TÍTULO Y MELODÍA: una pieza dibujada
const int W = 522;  // ancho de lo de dentro de la ventana
struct Chip {
  int16_t x, y, w;
  int8_t kind;  // 0 título, 1 melodía
  int8_t arg;   // título: -2 automático, -1 sin título, ≥ 0 el logro; melodía: su índice
};
struct StyleData {
  int n;
  int16_t label2, h;  // dónde va «Melodía de victoria» y el alto de todo
  Chip chip[progresion::ACH_COUNT + 2 + melodias::COUNT];
};

static StyleData* styleData = nullptr;  // los de la pieza a la vista (para las pruebas del PC)
static lv_obj_t* stylePiece = nullptr;

static const char* chipText(const Chip& c) {
  if (c.kind) return melodias::LIST[c.arg].label;
  return c.arg == -2 ? "Automático" : c.arg == -1 ? "Sin título" : progresion::ACH[c.arg].title;
}
static bool chipOn(const Chip& c) {
  const jug::Extra* e = store->extraOf(code);
  if (c.kind) return melodias::indexOf(e ? e->anthem : "") == c.arg;
  const char* t = e ? e->titleId : "";
  if (c.arg == -2) return !*t;
  if (c.arg == -1) return !strcmp(t, "none");
  return !strcmp(t, progresion::ACH[c.arg].id);
}

static StyleData* layout() {
  StyleData* d = (StyleData*)calloc(1, sizeof(StyleData));  // en la PSRAM (con todos los títulos, ~0,5 KB)
  if (!d) return nullptr;
  const lv_font_t* f = assets::font(800, 13);
  int x = 0, y = 21;
  auto add = [&](int8_t kind, int8_t arg) {
    lv_point_t sz;
    Chip c = {0, 0, 0, kind, arg};
    lv_text_get_size(&sz, chipText(c), f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int w = sz.x + 30;
    if (x && x + w > W) x = 0, y += 50;
    c.x = (int16_t)x, c.y = (int16_t)y, c.w = (int16_t)w;
    d->chip[d->n++] = c;
    x += w + 6;
  };
  add(0, -2);
  add(0, -1);
  int t[progresion::ACH_COUNT];
  int n = progresion::availableTitles(progresion::of(code), t);
  for (int i = 0; i < n; i++) add(0, (int8_t)t[i]);
  d->label2 = (int16_t)(y + 44 + 14);
  x = 0, y = d->label2 + 21;
  for (int i = 0; i < melodias::COUNT; i++) add(1, (int8_t)i);
  d->h = (int16_t)(y + 44);
  return d;
}

static void drawStyle(lv_event_t* e) {
  lv_obj_t* o = lv_event_get_current_target_obj(e);
  const StyleData* d = (const StyleData*)lv_event_get_user_data(e);
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  ficha::drawText(layer, "TÍTULO BAJO EL NOMBRE", 700, 12, ui::color::TEXT2, a.x1, a.y1, a.x1 + W, LV_TEXT_ALIGN_LEFT, LV_OPA_COVER, 1);
  ficha::drawText(layer, "MELODÍA DE VICTORIA", 700, 12, ui::color::TEXT2, a.x1, a.y1 + d->label2, a.x1 + W, LV_TEXT_ALIGN_LEFT,
                  LV_OPA_COVER, 1);
  for (int i = 0; i < d->n; i++) {
    const Chip& c = d->chip[i];
    int x = a.x1 + c.x, y = a.y1 + c.y;
    if (y > layer->_clip_area.y2 || y + 44 < layer->_clip_area.y1) continue;
    bool on = chipOn(c);
    // .pick-chip / .pick-blue de la app
    ficha::drawBox(layer, x, y, x + c.w - 1, y + 43, LV_RADIUS_CIRCLE, on ? 0x176dbc : 0x070b12, on ? LV_OPA_COVER : 153,
                   on ? 0x4fa3f0 : ui::color::LINE, 1);
    ficha::drawText(layer, chipText(c), 800, 13, ui::color::TEXT, x, y + 13, x + c.w - 1, LV_TEXT_ALIGN_CENTER);
  }
}

static void saveStyle(const char* titleId, const char* anthem) {
  const char* shown = progresion::displayTitle(progresion::of(code), titleId);
  if (!store->setStyle(code, titleId, anthem, shown ? shown : "")) return ui::toast("No se pudo guardar");
  dirty = true;
  if (hooks.changed) hooks.changed(code);
}

static int32_t pressX = -1, pressY = -1;  // dónde se apretó, relativo a la pieza (al soltar puede ser otro)
static void onStyleTap(lv_event_t* e) {
  lv_obj_t* o = lv_event_get_current_target_obj(e);
  const StyleData* d = (const StyleData*)lv_event_get_user_data(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
    lv_indev_t* in = lv_event_get_indev(e);
    lv_point_t p = {0, 0};
    if (in) lv_indev_get_point(in, &p);
    pressX = in ? p.x - a.x1 : -1;
    pressY = in ? p.y - a.y1 : -1;
    return;
  }
  int32_t x = pressX, y = pressY;
  pressX = pressY = -1;
  for (int i = 0; x >= 0 && i < d->n; i++) {
    const Chip& c = d->chip[i];
    if (x < c.x || x >= c.x + c.w || y < c.y || y >= c.y + 44) continue;
    const jug::Extra* ex = store->extraOf(code);
    char titleId[24], anthem[12];
    snprintf(titleId, sizeof(titleId), "%s", ex ? ex->titleId : "");
    snprintf(anthem, sizeof(anthem), "%s", ex ? ex->anthem : "");
    if (c.kind) {
      if (hooks.play) hooks.play(c.arg);
      snprintf(anthem, sizeof(anthem), "%s", melodias::LIST[c.arg].id);
    } else {
      snprintf(titleId, sizeof(titleId), "%s", c.arg == -2 ? "" : c.arg == -1 ? "none" : progresion::ACH[c.arg].id);
    }
    saveStyle(titleId, anthem);
    lv_obj_invalidate(o);  // solo cambia qué está iluminado
    return;
  }
}

// ---------------------------------------------------------------- la ventana
static void build();
// Los botones de la ventana la rehacen (o la quitan) después del toque: no se puede borrar un botón mientras se toca.
static void later(View v) {
  view = v;
  lv_async_call([](void*) {
    if (back) build();
  }, nullptr);
}
static void close() {
  if (back) lv_obj_delete(back);
  back = nullptr;
  if (dirty && hooks.redo) hooks.redo();
  dirty = false;
}
static void closeLater() {
  lv_async_call([](void*) { close(); }, nullptr);
}

static void onBackdrop(lv_event_t* e) {
  if (lv_event_get_target_obj(e) == back) closeLater();  // tocar fuera de la ventana la cierra (como la app)
}
static void onClose(lv_event_t*) { closeLater(); }
static void onMenu(lv_event_t*) { later(View::Menu); }
static void onQr(lv_event_t*) {
  if (hooks.qr) hooks.qr(code);
}
static void onEdit(lv_event_t*) {
  if (hooks.edit) hooks.edit(code);
}
static void onStyle(lv_event_t*) { later(View::Style); }
static void onToggle(lv_event_t*) {
  const jug::Player* p = store->find(code);
  bool active = store->isActive(code);
  char b[64];
  if (!p || !store->setActive(code, !active)) return ui::toast("No se pudo guardar");
  snprintf(b, sizeof(b), "%s, %s", p->name, active ? "de baja" : "de alta");
  ui::toast(b);
  if (hooks.changed) hooks.changed(code);
  dirty = true;
  closeLater();
}

static void badge(lv_obj_t* parent, int x, int y) {
  lv_obj_t* b = ui::box(parent, x, y, 74, 22);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, ui::hex(ui::color::CARD2), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, ui::hex(ui::color::LINE), 0);
  lv_obj_t* l = ui::text(b, "INACTIVO", 800, 12, ui::color::TEXT2, 0.6f);
  lv_obj_center(l);
}

static void build() {
  const jug::Player* p = store->find(code);
  if (!p || !ficha::scr) {
    if (back) lv_obj_delete(back);
    back = nullptr;
    return;
  }
  if (back) lv_obj_delete(back);  // lo viejo, antes de crear lo nuevo
  back = ui::box(ficha::scr, 0, 0, 800, 480);
  lv_obj_set_style_bg_color(back, ui::hex(0x03060c), 0);
  lv_obj_set_style_bg_opa(back, 199, 0);
  lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(back, onBackdrop, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(back, [](lv_event_t*) { back = nullptr; }, LV_EVENT_DELETE, nullptr);  // también si se va la ficha

  StyleData* d = view == View::Style ? layout() : nullptr;
  if (view == View::Style && !d) view = View::Menu;
  int content = d ? (d->h < 314 ? d->h : 314) : 216;  // con muchos títulos, se desliza
  int h = 134 + content, y0 = (480 - h) / 2;
  lv_obj_t* m = ui::box(back, 120, y0, 560, h);
  lv_obj_add_flag(m, LV_OBJ_FLAG_CLICKABLE);  // tocar dentro no la cierra
  lv_obj_set_style_radius(m, 18, 0);
  lv_obj_set_style_bg_color(m, ui::hex(0x162a47), 0);
  lv_obj_set_style_bg_grad_color(m, ui::hex(0x091324), 0);
  lv_obj_set_style_bg_grad_dir(m, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(m, 1, 0);
  lv_obj_set_style_border_color(m, ui::hex(ui::color::ACCENT), 0);
  lv_obj_set_style_shadow_width(m, 30, 0);
  lv_obj_set_style_shadow_color(m, ui::hex(ui::color::ACCENT), 0);
  lv_obj_set_style_shadow_opa(m, 60, 0);
  lv_obj_t* t = ficha::textAt(m, p->name, 800, 20, ui::color::TEXT, 18, 18, 520);
  lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_DOTS);
  int ay = 55 + content + 12;  // los botones de abajo
  if (d) {
    lv_obj_t* sc = ui::box(m, 18, 55, W, content);
    lv_obj_t* pc = ui::box(sc, 0, 0, W, d->h);
    if (d->h > content) {
      lv_obj_add_flag(sc, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_scroll_dir(sc, LV_DIR_VER);
      lv_obj_set_scrollbar_mode(sc, LV_SCROLLBAR_MODE_AUTO);
    }
    lv_obj_add_flag(pc, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE));  // que la lista se deslice
    lv_obj_add_event_cb(pc, drawStyle, LV_EVENT_DRAW_MAIN, d);
    lv_obj_add_event_cb(pc, onStyleTap, LV_EVENT_PRESSED, d);
    lv_obj_add_event_cb(pc, onStyleTap, LV_EVENT_CLICKED, d);
    lv_obj_add_event_cb(pc, ficha::freeUserData, LV_EVENT_DELETE, d);
    lv_obj_add_event_cb(pc, [](lv_event_t*) { styleData = nullptr, stylePiece = nullptr; }, LV_EVENT_DELETE, nullptr);
    styleData = d, stylePiece = pc;
    ui::button(m, 333, ay, 93, 48, "ATRÁS", 14, ui::BtnKind::Normal, onMenu);
  } else {
    ficha::avatar(m, code, p->name, 18, 55, 64);
    if (!store->isActive(code)) badge(m, 92, 76);
    ui::button(m, 18, 133, 256, 64, "VER QR", 18, ui::BtnKind::Normal, onQr);
    ui::button(m, 284, 133, 256, 64, "EDITAR", 18, ui::BtnKind::Normal, onEdit);
    ui::button(m, 18, 207, 256, 64, "TÍTULO Y MELODÍA", 18, ui::BtnKind::Normal, onStyle);
    ui::button(m, 284, 207, 256, 64, store->isActive(code) ? "DESACTIVAR" : "ACTIVAR", 18, ui::BtnKind::Normal, onToggle);
  }
  ui::button(m, 436, ay, 104, 48, "CERRAR", 14, ui::BtnKind::Primary, onClose);
}

// «⋯ OPCIONES» de la ficha de `c`.
static void open(jug::Store* s, uint32_t c) {
  store = s;
  code = c;
  view = View::Menu;
  dirty = false;
  build();
}

}  // namespace jopc

#endif  // MFV3_UI_JUGADOR_OPCIONES_H
