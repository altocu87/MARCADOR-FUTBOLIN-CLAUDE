// MARCADOR FUTBOLÍN V3 · AJUSTES → JUGADORES: crear, cambiar y borrar jugadores en la propia placa.
// -------------------------------------------------------------------------------------------------
// Pasos de un jugador nuevo: NOMBRE (teclado en pantalla, con Ñ y tildes) → FOTO (las .jpg de la carpeta FOTOS de
// la tarjeta SD, o sin foto) → ¿ESTA FOTO? (se puede girar) → GUARDAR. Tocar un jugador de la lista deja cambiarle
// el nombre o la foto, o borrarlo (con confirmación).
// Cada paso es una pantalla nueva (se pinta una vez al entrar); el teclado solo repinta la tecla y el nombre.
// Las fotos se abren de una en una en segundo plano (s3_fotos.h) y se guardan mientras se está en la galería.

#ifndef MFV3_UI_FICHAS_H
#define MFV3_UI_FICHAS_H

#include "ui_base.h"
#include "s3_fotos.h"
#include "s3_jugadores.h"

namespace fichas {

struct Hooks {
  void (*back)() = nullptr;  // volver a AJUSTES
  void (*changed)(uint32_t code) = nullptr;   // jugador creado o cambiado (para subirlo a la cuenta)
  void (*removed)(const char* id) = nullptr;  // jugador borrado
};
static Hooks hooks;

enum class View : uint8_t { List, Detail, Name, Photos, Preview, Confirm };
static View view = View::List;
static jug::Store* store = nullptr;
static lv_obj_t* scr = nullptr;

// Lo que se está haciendo
static uint32_t editCode = 0;  // 0 = jugador nuevo
static char name[28] = "";
static uint16_t* chosen = nullptr;  // foto elegida (120 × 120)
static lv_image_dsc_t chosenDsc;
static lv_obj_t *nameLabel = nullptr, *chosenImg = nullptr;

// Galería de la tarjeta
enum : uint8_t { T_NONE, T_LOADING, T_OK, T_BAD };
static uint16_t* thumb[fotos::MAX_FILES];
static uint8_t thumbState[fotos::MAX_FILES];
static lv_image_dsc_t thumbDsc[fotos::MAX_FILES];
static lv_obj_t *thumbImg[fotos::MAX_FILES], *thumbNote[fotos::MAX_FILES];
static bool scanned = false, sdFolder = false;
static int loading = -1;

static void go(View v);

// ---------------------------------------------------------------- piezas
static void setDsc(lv_image_dsc_t& d, const uint16_t* px) {
  memset(&d, 0, sizeof(d));
  d.header.magic = LV_IMAGE_HEADER_MAGIC;
  d.header.cf = LV_COLOR_FORMAT_RGB565;
  d.header.w = fotos::SIZE;
  d.header.h = fotos::SIZE;
  d.header.stride = fotos::SIZE * 2;
  d.data = (const uint8_t*)px;
  d.data_size = fotos::PIXELS * 2;
}

// Botón sin animaciones (encoger con el dedo obligaba a pintarlo dos veces): pulsado, solo cambia de color.
enum class Kind : uint8_t { Normal, Primary, Danger };
static lv_obj_t* btn(lv_obj_t* parent, int x, int y, int w, int h, const char* txt, uint16_t size, Kind k, lv_event_cb_t cb,
                     void* user = nullptr, lv_event_code_t when = LV_EVENT_CLICKED) {
  lv_obj_t* b = ui::box(parent, x, y, w, h);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(b, ui::RADIUS_SM, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  uint32_t bg = ui::color::CARD, bgDown = ui::color::CARD2, line = ui::color::LINE, ink = ui::color::TEXT;
  if (k == Kind::Primary) bg = 0x1aa3dc, bgDown = 0x13789f, line = ui::color::ACCENT, ink = 0x03121e;
  if (k == Kind::Danger) bg = 0x3a1a26, bgDown = 0x57202f, line = ui::color::DANGER, ink = 0xffd2d8;
  lv_obj_set_style_bg_color(b, ui::hex(bg), 0);
  lv_obj_set_style_bg_color(b, ui::hex(bgDown), LV_STATE_PRESSED);
  lv_obj_set_style_border_color(b, ui::hex(line), 0);
  lv_obj_set_style_border_color(b, ui::hex(ui::color::ACCENT), LV_STATE_PRESSED);
  lv_obj_t* l = ui::text(b, txt, 800, size, ink, size * 0.04f);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, when, user);
  return b;
}

// Arriba: volver, el título y, a la derecha, el botón para seguir (si hay).
static void header(const char* title, lv_event_cb_t back, const char* next = nullptr, lv_event_cb_t nextCb = nullptr) {
  lv_obj_t* band = ui::box(scr, 0, 0, 800, 64);
  lv_obj_set_style_bg_color(band, ui::hex(0x0b1220), 0);
  lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(band, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(band, 1, 0);
  lv_obj_set_style_border_color(band, ui::hex(ui::color::LINE), 0);
  btn(scr, 12, 8, 132, 48, "VOLVER", 14, Kind::Normal, back);
  lv_obj_t* t = ui::text(scr, title, 900, 24, ui::color::TEXT, 0.5f);
  lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(t, next ? 420 : 600);
  lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(t, next ? 190 : 100, 18);
  if (next) btn(scr, 628, 8, 160, 48, next, 14, Kind::Primary, nextCb);
}

static const jug::Player* editing() { return editCode ? store->find(editCode) : nullptr; }

// Foto de un jugador (ya reducida con las esquinas hechas) o sus iniciales.
static void face(lv_obj_t* parent, const jug::Player& p, int x, int y, int size, int radius, uint16_t iniSize) {
  char rel[40];
  const lv_image_dsc_t* pic = store->photoRel(p.code, rel, sizeof(rel)) ? assets::photo(rel, size, radius) : nullptr;
  if (pic) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, pic);
    lv_obj_set_pos(img, x, y);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return;
  }
  lv_obj_t* b = ui::box(parent, x, y, size, size);
  lv_obj_set_style_radius(b, radius, 0);
  lv_obj_set_style_bg_color(b, ui::hex(0x1d3a63), 0);
  lv_obj_set_style_bg_grad_color(b, ui::hex(0x0f2140), 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  char ini[8];
  jug::initials(p.name, ini, sizeof(ini));
  lv_obj_t* l = ui::text(b, ini, 900, iniSize, ui::color::TEXT2);
  lv_obj_center(l);
}

static lv_obj_t* scroller(int y) {
  lv_obj_t* g = ui::box(scr, 0, y, 800, 480 - y);
  lv_obj_add_flag(g, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(g, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(g, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_width(g, 4, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(g, ui::hex(ui::color::LINE), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(g, LV_OPA_COVER, LV_PART_SCROLLBAR);
  lv_obj_set_style_pad_bottom(g, 16, 0);
  return g;
}

// ---------------------------------------------------------------- LISTA
static void onBackHome(lv_event_t*) {
  if (hooks.back) hooks.back();
}
static void onNew(lv_event_t*) {
  editCode = 0;
  name[0] = 0;
  go(View::Name);
}
static void onPlayer(lv_event_t* e) {
  editCode = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  if (editing()) go(View::Detail);
}

static void buildList() {
  char t[40];
  snprintf(t, sizeof(t), "JUGADORES (%u)", store->count);
  header(t, onBackHome);
  lv_obj_t* g = scroller(64);
  const int CW = 180, CH = 172, GAP = 16;
  for (int i = 0; i <= store->count; i++) {
    int x = 16 + (i % 4) * (CW + GAP), y = 16 + (i / 4) * (CH + 14);
    lv_obj_t* c = ui::box(g, x, y, CW, CH);
    lv_obj_set_style_radius(c, ui::RADIUS, 0);
    lv_obj_set_style_border_width(c, i ? 1 : 2, 0);
    lv_obj_set_style_border_color(c, ui::hex(i ? ui::color::LINE : ui::color::ACCENT), 0);
    lv_obj_set_style_border_color(c, ui::hex(ui::color::ACCENT), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(c, ui::hex(i ? ui::color::CARD : 0x0d2a40), 0);
    lv_obj_set_style_bg_color(c, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_add_flag(c, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS));
    if (!i) {
      lv_obj_t* plus = ui::text(c, "+", 900, 60, ui::color::ACCENT);
      lv_obj_align(plus, LV_ALIGN_CENTER, 0, -22);
      lv_obj_t* l = ui::text(c, "NUEVO JUGADOR", 800, 14, ui::color::ACCENT, 0.6f);
      lv_obj_align(l, LV_ALIGN_CENTER, 0, 40);
      lv_obj_add_event_cb(c, onNew, LV_EVENT_CLICKED, nullptr);
      continue;
    }
    const jug::Player& p = store->list[i - 1];
    face(c, p, (CW - 120) / 2, 12, 120, 14, 40);
    lv_obj_t* l = ui::text(c, p.name, 800, 18, ui::color::TEXT);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(l, CW - 16);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, 8, 140);
    lv_obj_add_event_cb(c, onPlayer, LV_EVENT_CLICKED, (void*)(uintptr_t)p.code);
  }
}

// ---------------------------------------------------------------- UN JUGADOR
static void onToList(lv_event_t*) { go(View::List); }
static void onRename(lv_event_t*) {
  if (const jug::Player* p = editing()) snprintf(name, sizeof(name), "%s", p->name);
  go(View::Name);
}
static void onChangePhoto(lv_event_t*) { go(View::Photos); }
static void onAskDelete(lv_event_t*) { go(View::Confirm); }

static void buildDetail() {
  const jug::Player* p = editing();
  header("JUGADOR", onToList);
  face(scr, *p, 56, 112, 240, 24, 80);
  lv_obj_t* n = ui::text(scr, p->name, 900, 38, ui::color::TEXT);
  lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(n, 420);
  lv_obj_set_pos(n, 340, 110);
  btn(scr, 340, 186, 420, 66, "CAMBIAR NOMBRE", 18, Kind::Normal, onRename);
  btn(scr, 340, 266, 420, 66, "CAMBIAR FOTO", 18, Kind::Normal, onChangePhoto);
  btn(scr, 340, 346, 420, 66, "BORRAR JUGADOR", 18, Kind::Danger, onAskDelete);
}

// ---------------------------------------------------------------- BORRAR
static void onDeleteNo(lv_event_t*) { go(View::Detail); }
static void onDeleteYes(lv_event_t*) {
  char who[28];
  const jug::Player* p = editing();
  snprintf(who, sizeof(who), "%s", p ? p->name : "");
  char gone[48] = "";
  if (editCode) store->idOf(editCode, gone, sizeof(gone));
  if (editCode && store->remove(editCode)) {
    if (hooks.removed) hooks.removed(gone);
    char b[64];
    snprintf(b, sizeof(b), "%s ya no está en la lista", who);
    ui::toast(b);
  } else {
    ui::toast("No se pudo borrar");
  }
  editCode = 0;
  go(View::List);
}

static void buildConfirm() {
  const jug::Player* p = editing();
  header("BORRAR JUGADOR", onDeleteNo);
  face(scr, *p, 340, 92, 120, 14, 40);
  char q[64];
  snprintf(q, sizeof(q), "¿Borrar a %s?", p->name);
  lv_obj_t* l = ui::text(scr, q, 900, 34, ui::color::TEXT);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 236);
  lv_obj_t* s = ui::text(scr, "Se quita de la lista con su foto. Los partidos ya jugados no cambian.", 400, 16, ui::color::TEXT2);
  lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 290);
  btn(scr, 110, 350, 280, 80, "NO", 20, Kind::Normal, onDeleteNo);
  btn(scr, 410, 350, 280, 80, "SÍ, BORRAR", 20, Kind::Danger, onDeleteYes);
}

// ---------------------------------------------------------------- NOMBRE (teclado)
// Las teclas van en mayúsculas; al escribir, la primera letra de cada palabra sale en mayúscula y las demás en
// minúscula («Ana María»), como se escriben los nombres.
static const char* const ROW0[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
static const char* const ROW1[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", "Ñ"};
static const char* const ROW2[] = {"Z", "X", "C", "V", "B", "N", "M"};
static const char* const ROW3[] = {"Á", "É", "Í", "Ó", "Ú", "Ü"};
const size_t NAME_MAX_CHARS = 16;

static size_t chars(const char* s) {
  size_t n = 0;
  for (; *s; s++)
    if (((uint8_t)*s & 0xC0) != 0x80) n++;
  return n;
}

static void showName() {
  if (!nameLabel) return;
  char b[40];
  if (name[0]) snprintf(b, sizeof(b), "%s|", name);
  else snprintf(b, sizeof(b), "|");
  lv_label_set_text(nameLabel, b);
}

static void onKey(lv_event_t* e) {
  const char* k = (const char*)lv_event_get_user_data(e);
  size_t n = strlen(name);
  if (!strcmp(k, "BORRAR")) {
    while (n && ((uint8_t)name[n - 1] & 0xC0) == 0x80) n--;  // el carácter entero (Ñ, tildes: 2 bytes)
    if (n) n--;
    name[n] = 0;
  } else if (!strcmp(k, "ESPACIO")) {
    if (n && name[n - 1] != ' ' && n + 1 < sizeof(name)) {
      name[n] = ' ';
      name[n + 1] = 0;
    }
  } else {
    if (chars(name) >= NAME_MAX_CHARS || n + strlen(k) >= sizeof(name)) return;
    bool upper = n == 0 || name[n - 1] == ' ';
    char c[4];
    snprintf(c, sizeof(c), "%s", k);
    if (!upper) {
      if (c[0] >= 'A' && c[0] <= 'Z') c[0] = (char)(c[0] + 32);
      else if ((uint8_t)c[0] == 0xC3) c[1] = (char)(c[1] + 0x20);  // Á→á, Ñ→ñ, Ü→ü
    }
    strcat(name, c);
  }
  showName();
}

static void trimName() {
  size_t n = strlen(name);
  while (n && name[n - 1] == ' ') name[--n] = 0;
}

static void onNameBack(lv_event_t*) { go(editCode ? View::Detail : View::List); }
static void onNameNext(lv_event_t*) {
  trimName();
  showName();
  if (!name[0]) return ui::toast("Escribe el nombre");
  if (store->nameTaken(name, editCode)) return ui::toast("Ya hay un jugador con ese nombre");
  if (editCode) {
    if (store->rename(editCode, name)) {
      ui::toast("Nombre cambiado");
      if (hooks.changed) hooks.changed(editCode);
    }
    go(View::Detail);
  } else {
    go(View::Photos);
  }
}

static void keyRow(const char* const* keys, int n, int x, int y, int w, int h) {
  for (int i = 0; i < n; i++)
    btn(scr, x + i * (w + 6), y, w, h, keys[i], 23, Kind::Normal, onKey, (void*)keys[i], LV_EVENT_PRESSED);
}

static void buildName() {
  header(editCode ? "CAMBIAR NOMBRE" : "NUEVO JUGADOR", onNameBack, editCode ? "GUARDAR" : "SIGUIENTE", onNameNext);
  lv_obj_t* f = ui::box(scr, 16, 76, 768, 64);
  lv_obj_set_style_radius(f, ui::RADIUS_SM, 0);
  lv_obj_set_style_bg_color(f, ui::hex(ui::color::SURFACE), 0);
  lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(f, 2, 0);
  lv_obj_set_style_border_color(f, ui::hex(ui::color::ACCENT), 0);
  nameLabel = ui::text(f, "", 900, 30, ui::color::TEXT);
  lv_obj_align(nameLabel, LV_ALIGN_LEFT_MID, 20, 0);
  showName();
  // Cuatro filas de teclas de 72 × 70 (las anchas, BORRAR y ESPACIO).
  const int KW = 72, KH = 70, X0 = 13, Y0 = 152, DY = 78;
  keyRow(ROW0, 10, X0, Y0, KW, KH);
  keyRow(ROW1, 10, X0, Y0 + DY, KW, KH);
  keyRow(ROW2, 7, X0, Y0 + 2 * DY, KW, KH);
  btn(scr, X0 + 7 * (KW + 6), Y0 + 2 * DY, 3 * KW + 2 * 6, KH, "BORRAR", 18, Kind::Normal, onKey, (void*)"BORRAR",
      LV_EVENT_PRESSED);
  keyRow(ROW3, 6, X0, Y0 + 3 * DY, KW, KH);
  btn(scr, X0 + 6 * (KW + 6), Y0 + 3 * DY, 4 * KW + 3 * 6, KH, "ESPACIO", 18, Kind::Normal, onKey, (void*)"ESPACIO",
      LV_EVENT_PRESSED);
}

// ---------------------------------------------------------------- FOTO (galería de la tarjeta)
static void freeThumbs() {
  // Si hay una a medias, se espera (escribe en su sitio).
  while (loading >= 0 && fotos::poll() == fotos::BUSY) {
#ifdef ARDUINO
    delay(10);
#endif
  }
  loading = -1;
  for (int i = 0; i < fotos::MAX_FILES; i++) {
    if (thumb[i]) {
      lv_image_cache_drop(&thumbDsc[i]);
      free(thumb[i]);
    }
    thumb[i] = nullptr;
    thumbState[i] = T_NONE;
  }
  scanned = false;
}

static bool ensureChosen() {
  if (!chosen) chosen = (uint16_t*)malloc(fotos::PIXELS * 2);
  return chosen != nullptr;
}

static void onPhotosBack(lv_event_t*) { go(editCode ? View::Detail : View::Name); }

static void saveWith(const uint16_t* px) {
  assets::forget("jugadores/");  // la foto vieja de este jugador ya no vale
  if (editCode) {
    bool ok = store->setPhoto(editCode, px);
    if (ok && hooks.changed) hooks.changed(editCode);
    ui::toast(ok ? (px ? "Foto cambiada" : "Foto quitada") : "No se pudo guardar");
    go(View::Detail);
  } else {
    uint32_t code = store->add(name, px);
    if (code && hooks.changed) hooks.changed(code);
    char b[64];
    if (code) snprintf(b, sizeof(b), "%s ya está en la lista", name);
    else snprintf(b, sizeof(b), "No se pudo guardar el jugador");
    ui::toast(b);
    editCode = 0;
    go(View::List);
  }
}

static void onNoPhoto(lv_event_t*) { saveWith(nullptr); }

static void onThumb(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (thumbState[i] == T_BAD) return ui::toast("Esta foto no se puede abrir");
  if (thumbState[i] != T_OK) return ui::toast("Un momento: abriendo la foto…");
  if (!ensureChosen()) return ui::toast("No hay memoria para la foto");
  memcpy(chosen, thumb[i], fotos::PIXELS * 2);
  go(View::Preview);
}

static void buildPhotos() {
  char t[48];
  snprintf(t, sizeof(t), "FOTO DE %s", editCode && editing() ? editing()->name : name);
  header(t, onPhotosBack);
  if (!scanned) {
    sdFolder = fotos::scan();
    scanned = true;
  }
  lv_obj_t* g = scroller(64);
  const int TW = 140, STEP_X = 157, STEP_Y = 154;
  for (int i = 0; i <= fotos::count; i++) {
    int x = 16 + (i % 5) * STEP_X, y = 16 + (i / 5) * STEP_Y;
    lv_obj_t* c = ui::box(g, x, y, TW, TW);
    lv_obj_set_style_radius(c, ui::RADIUS_SM, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_border_color(c, ui::hex(ui::color::LINE), 0);
    lv_obj_set_style_border_color(c, ui::hex(ui::color::ACCENT), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(c, ui::hex(ui::color::CARD), 0);
    lv_obj_set_style_bg_color(c, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    if (!i) {
      bool had = editCode && store->photoOk(editCode);
      lv_obj_t* l = ui::text(c, editCode ? (had ? "QUITAR\nLA FOTO" : "SIN FOTO") : "SIN FOTO", 800, 17, ui::color::TEXT2, 0.5f);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_center(l);
      lv_obj_add_event_cb(c, onNoPhoto, LV_EVENT_CLICKED, nullptr);
      continue;
    }
    int k = i - 1;
    thumbImg[k] = lv_image_create(c);
    lv_obj_set_pos(thumbImg[k], 10, 10);
    lv_obj_set_size(thumbImg[k], fotos::SIZE, fotos::SIZE);
    lv_obj_remove_flag(thumbImg[k], LV_OBJ_FLAG_CLICKABLE);
    thumbNote[k] = ui::text(c, "…", 900, 24, ui::color::TEXT3);
    lv_obj_center(thumbNote[k]);
    if (thumbState[k] == T_OK) {
      lv_image_set_src(thumbImg[k], &thumbDsc[k]);
      lv_obj_add_flag(thumbNote[k], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(thumbImg[k], LV_OBJ_FLAG_HIDDEN);
      if (thumbState[k] == T_BAD) lv_label_set_text(thumbNote[k], "NO SE\nPUEDE ABRIR");
      lv_obj_set_style_text_font(thumbNote[k], assets::font(thumbState[k] == T_BAD ? 800 : 900, thumbState[k] == T_BAD ? 12 : 24), 0);
      lv_obj_set_style_text_align(thumbNote[k], LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_center(thumbNote[k]);
    }
    lv_obj_add_event_cb(c, onThumb, LV_EVENT_CLICKED, (void*)(intptr_t)k);
  }
  if (!fotos::count) {
    lv_obj_t* a = ui::text(g, sdFolder ? "No hay fotos en la carpeta FOTOS de la tarjeta" : "No se ve la carpeta FOTOS en la tarjeta",
                           800, 20, ui::color::TEXT);
    lv_obj_set_pos(a, 190, 40);
    lv_obj_t* b = ui::text(g, "Copia las fotos (.jpg) en una carpeta llamada\nFOTOS de la tarjeta SD, métela en la placa\ny vuelve a entrar aquí.",
                           400, 16, ui::color::TEXT2);
    lv_obj_set_pos(b, 190, 80);
  }
}

// Una foto de la galería cada vez (en segundo plano): cuando está, se enseña en su sitio.
static void tickPhotos() {
  if (loading >= 0) {
    uint8_t s = fotos::poll();
    if (s == fotos::BUSY) return;
    int k = loading;
    loading = -1;
    if (s == fotos::DONE) {
      thumbState[k] = T_OK;
      setDsc(thumbDsc[k], thumb[k]);
    } else {
      thumbState[k] = T_BAD;
    }
    if (view == View::Photos && scr && lv_screen_active() == scr && thumbImg[k]) {
      if (thumbState[k] == T_OK) {
        lv_image_set_src(thumbImg[k], &thumbDsc[k]);
        lv_obj_remove_flag(thumbImg[k], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(thumbNote[k], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_label_set_text(thumbNote[k], "NO SE\nPUEDE ABRIR");
        lv_obj_set_style_text_font(thumbNote[k], assets::font(800, 12), 0);
        lv_obj_set_style_text_align(thumbNote[k], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(thumbNote[k]);
      }
    }
  }
  if (view != View::Photos || loading >= 0) return;
  for (int k = 0; k < fotos::count; k++) {
    if (thumbState[k] != T_NONE) continue;
    if (!thumb[k]) thumb[k] = (uint16_t*)malloc(fotos::PIXELS * 2);
    if (!thumb[k]) {
      thumbState[k] = T_BAD;
      continue;
    }
    if (fotos::request(k, thumb[k])) {
      thumbState[k] = T_LOADING;
      loading = k;
    }
    return;
  }
}

// ---------------------------------------------------------------- ¿ESTA FOTO?
static void onTurn(lv_event_t*) {
  uint16_t* tmp = (uint16_t*)malloc(fotos::PIXELS * 2);
  if (!tmp) return;
  fotos::rotate(chosen, tmp);
  free(tmp);
  lv_image_cache_drop(&chosenDsc);
  if (chosenImg) lv_obj_invalidate(chosenImg);
}
static void onOther(lv_event_t*) { go(View::Photos); }
static void onSave(lv_event_t*) { saveWith(chosen); }

static void buildPreview() {
  header("¿ESTA FOTO?", onOther, "GUARDAR", onSave);
  setDsc(chosenDsc, chosen);
  // La foto al doble (240 × 240): así se ve si la cara queda bien.
  chosenImg = lv_image_create(scr);
  lv_image_set_src(chosenImg, &chosenDsc);
  lv_obj_set_pos(chosenImg, 56, 112);
  lv_obj_set_size(chosenImg, 240, 240);
  lv_image_set_inner_align(chosenImg, LV_IMAGE_ALIGN_STRETCH);
  lv_obj_remove_flag(chosenImg, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* n = ui::text(scr, editCode && editing() ? editing()->name : name, 900, 38, ui::color::TEXT);
  lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(n, 420);
  lv_obj_set_pos(n, 340, 110);
  btn(scr, 340, 186, 420, 66, "GIRAR", 18, Kind::Normal, onTurn);
  btn(scr, 340, 266, 420, 66, "OTRA FOTO", 18, Kind::Normal, onOther);
  btn(scr, 340, 346, 420, 66, "GUARDAR", 18, Kind::Primary, onSave);
}

// ---------------------------------------------------------------- pantallas
static void onDelete(lv_event_t*) {
  scr = nullptr;
  nameLabel = chosenImg = nullptr;
  memset(thumbImg, 0, sizeof(thumbImg));
  memset(thumbNote, 0, sizeof(thumbNote));
}

static void go(View v) {
  if ((v == View::Detail || v == View::Confirm) && !editing()) v = View::List;  // ya no está
  if (v == View::Preview && !chosen) v = View::Photos;
  view = v;
  lv_obj_t* old = scr;
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  ui::gradient(scr, 0x0f2140, 0x05090f);
  nameLabel = chosenImg = nullptr;
  memset(thumbImg, 0, sizeof(thumbImg));
  memset(thumbNote, 0, sizeof(thumbNote));
  switch (v) {
    case View::List: buildList(); break;
    case View::Detail: buildDetail(); break;
    case View::Name: buildName(); break;
    case View::Photos: buildPhotos(); break;
    case View::Preview: buildPreview(); break;
    case View::Confirm: buildConfirm(); break;
  }
  if (scr != lv_screen_active()) {
    if (old) lv_obj_remove_event_cb(old, onDelete);  // la vieja se borra al cargar esta: que no limpie lo nuevo
    ui::show(scr);
  }
}

static bool active() { return scr && lv_screen_active() == scr; }

// Al entrar desde AJUSTES (o al volver del reloj de reposo, en el mismo paso).
static void show(jug::Store* s, bool fresh) {
  store = s;
  if (fresh) {
    view = View::List;
    editCode = 0;
    freeThumbs();  // la tarjeta puede traer fotos nuevas
  }
  go(view);
}

static void tick() { tickPhotos(); }

// Al salir a AJUSTES: fuera las fotos de la galería (hasta 1,7 MB).
static void leave() { freeThumbs(); }

}  // namespace fichas

#endif  // MFV3_UI_FICHAS_H
