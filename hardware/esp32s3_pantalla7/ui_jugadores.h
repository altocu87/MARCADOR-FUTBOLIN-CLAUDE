// MARCADOR FUTBOLÍN V3 · elegir jugadores, como la app (src/ui/screens/SelectPlayersScreen.tsx).
// --------------------------------------------------------------------------------------------
// Primero el Equipo BLANCO y luego el AZUL (de 1 a 4 cada uno). A los lados, las columnas de cada equipo con sus
// 4 plazas («Plaza 1», «Plaza 2 · opcional»…); la del turno se ilumina. En medio, la lista de jugadores libres con
// su foto (o iniciales), «Nv · ELO» y título, que se desplaza si no caben. Tocar un libre lo mete en el equipo del
// turno; tocar una ficha de un equipo lo devuelve a la lista. Abajo, el aviso de cuántos van y SIGUIENTE: AZUL /
// CONTINUAR; con 4, 6 u 8 elegidos, también Equilibrar (por ELO) y Aleatorio.
// Los jugadores y sus fotos los pasa la app (s3_jugadores.h).
// Para que la placa vaya ligera (Alex, 18/10/2026): el fondo es un degradado de blancos o de azules según el turno
// (no la foto de la app, que costaba repintar en cada toque), y al tocar solo se mueve lo que cambia: las fichas de
// la lista se crean una vez y se recolocan.

#ifndef MFV3_UI_JUGADORES_H
#define MFV3_UI_JUGADORES_H

#include "ui_base.h"
#include "s3_jugadores.h"

namespace elegir {

const uint8_t MAX_PER_TEAM = 4;

struct Teams {
  uint32_t code[2][MAX_PER_TEAM];
  uint8_t n[2];
};

struct Hooks {
  void (*back)() = nullptr;                 // volver a la configuración
  void (*done)(const Teams& t) = nullptr;   // CONTINUAR con los dos equipos
};
static Hooks hooks;

static jug::Store* store = nullptr;
static const char* modeLabel = "PARTIDO RÁPIDO";
static Teams teams;
static uint8_t step = 0;  // 0 = Blanco, 1 = Azul

static lv_obj_t *scr = nullptr, *title, *small, *col[2], *grid, *notice, *noticeText, *btnNext, *btnSplit[2];
static lv_obj_t* freeCard[jug::MAX];  // una por jugador de la lista, en el mismo orden (nullptr si no hay)
static uint32_t splitSeed = 1;

const uint32_t TEAM_COLOR[2] = {ui::color::WHITE_TEAM, 0x4fb0ff};
const uint32_t LABEL_COLOR[2] = {ui::color::WHITE_TEAM, 0x6fb6f5};
const char* const SIDE[2] = {"BLANCO", "AZUL"};

static bool inTeam(uint32_t code, int* team = nullptr) {
  for (int t = 0; t < 2; t++)
    for (int i = 0; i < teams.n[t]; i++)
      if (teams.code[t][i] == code) {
        if (team) *team = t;
        return true;
      }
  return false;
}

// Foto del jugador (120 × 120 de la app) a `size` px, o sus iniciales sobre el degradado de la app. La foto va ya
// reducida y con las esquinas hechas (assets::photo): LVGL solo la copia. `bottom`: redondear también abajo.
static void photo(lv_obj_t* parent, const jug::Player& p, int x, int y, int size, int initialsSize, int radius,
                  bool bottom = true) {
  char rel[40];
  const lv_image_dsc_t* pic = store && store->photoRel(p.code, rel, sizeof(rel)) ? assets::photo(rel, size, radius, bottom) : nullptr;
  if (pic) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, pic);
    lv_obj_set_pos(img, x, y);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return;
  }
  lv_obj_t* box = ui::box(parent, x, y, size, size);
  lv_obj_set_style_radius(box, radius, 0);
  lv_obj_set_style_bg_color(box, ui::hex(0x1d3a63), 0);
  lv_obj_set_style_bg_grad_color(box, ui::hex(0x0f2140), 0);
  lv_obj_set_style_bg_grad_dir(box, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  char ini[8];
  jug::initials(p.name, ini, sizeof(ini));
  lv_obj_t* l = ui::text(box, ini, 900, (uint16_t)initialsSize, ui::color::TEXT2);
  lv_obj_center(l);
}

static lv_obj_t* line(lv_obj_t* parent, const char* txt, uint16_t w, uint16_t sz, uint32_t c, int width, bool center) {
  lv_obj_t* l = ui::text(parent, txt, w, sz, c);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);  // una sola línea, con «…» si no cabe (como la app)
  lv_obj_set_size(l, width, lv_font_get_line_height(assets::font(w, sz)));
  if (center) lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  return l;
}

static void refresh();
static void update();

static void onPick(lv_event_t* e) {
  uint32_t code = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  if (inTeam(code)) return;
  if (teams.n[step] >= MAX_PER_TEAM) {
    char b[64];
    snprintf(b, sizeof(b), "El equipo %s ya tiene %u jugadores (máximo)", SIDE[step], MAX_PER_TEAM);
    ui::toast(b);
    return;
  }
  teams.code[step][teams.n[step]++] = code;
  update();
}

static void onRemove(lv_event_t* e) {
  uint32_t code = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  int t;
  if (!inTeam(code, &t)) return;
  for (int i = 0, k = 0; i < teams.n[t]; i++)
    if (teams.code[t][i] != code) teams.code[t][k++] = teams.code[t][i];
  teams.n[t]--;
  update();
}

static void onCol(lv_event_t* e) {
  int t = (int)(intptr_t)lv_event_get_user_data(e);
  if (t != step && (t == 0 || teams.n[0] > 0)) {
    step = (uint8_t)t;
    update();
  }
}

static void onBack(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (step == 1) {
    step = 0;
    update();
  } else if (hooks.back) hooks.back();
}

static bool valid() { return teams.n[0] > 0 && teams.n[1] > 0; }

static void onNext(lv_event_t*) {
  if (step == 0) {
    if (teams.n[0] == 0) return;
    step = 1;
    update();
  } else if (valid() && hooks.done) hooks.done(teams);
}

// ELO del detalle «Nv 23 · ELO 1299» (1200 si no ha jugado clasificatorios, como la app).
static int eloOf(uint32_t code) {
  const jug::Player* p = store ? store->find(code) : nullptr;
  const char* e = p ? strstr(p->meta, "ELO ") : nullptr;
  return e ? atoi(e + 4) : 1200;
}

// Equilibrar (por ELO: el mejor a un lado, los dos siguientes al otro…, buscando la menor diferencia) o al azar.
static void onSplit(lv_event_t* e) {
  bool balanced = lv_event_get_user_data(e) == nullptr;
  uint32_t all[8];
  uint8_t n = 0;
  for (int t = 0; t < 2; t++)
    for (int i = 0; i < teams.n[t]; i++) all[n++] = teams.code[t][i];
  uint8_t half = n / 2;
  uint32_t best = 0;
  long bestDiff = -1;
  for (uint32_t mask = 0; mask < (1u << n); mask++) {
    if ((uint8_t)__builtin_popcount(mask) != half || (mask & 1u) == 0) continue;  // el primero siempre a un lado
    long a = 0, b = 0;
    for (uint8_t i = 0; i < n; i++) (mask >> i & 1 ? a : b) += eloOf(all[i]);
    long d = a > b ? a - b : b - a;
    if (balanced ? (bestDiff < 0 || d < bestDiff) : true) {
      if (!balanced) {
        splitSeed = splitSeed * 1103515245u + 12345u;
        if (bestDiff >= 0 && (splitSeed >> 16) % 3) continue;
      }
      best = mask;
      bestDiff = d;
    }
  }
  teams.n[0] = teams.n[1] = 0;
  for (uint8_t i = 0; i < n; i++) {
    int t = (best >> i & 1) ? 0 : 1;
    teams.code[t][teams.n[t]++] = all[i];
  }
  update();
}

static uint32_t colKey[2] = {0, 0};  // lo que enseña cada columna (turno y jugadores): si no cambia, no se rehace
static void teamColumn(int t) {
  bool active = step == t;
  uint32_t key = 2166136261u ^ (active ? 1u : 2u);
  for (int i = 0; i < teams.n[t]; i++) key = (key ^ teams.code[t][i]) * 16777619u;
  if (key == colKey[t]) return;
  colKey[t] = key;
  lv_obj_clean(col[t]);
  lv_obj_set_style_border_color(col[t], ui::hex(active ? TEAM_COLOR[t] : 0x000000), 0);
  lv_obj_set_style_border_opa(col[t], active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_width(col[t], active ? 16 : 0, 0);
  lv_obj_set_style_shadow_color(col[t], ui::hex(TEAM_COLOR[t]), 0);
  lv_obj_set_style_shadow_opa(col[t], t ? 128 : 102, 0);
  lv_obj_t* label = ui::text(col[t], SIDE[t], 900, 13, LABEL_COLOR[t], 1.82f);
  ui::place(label, 92 - 2 + 0.91f, 72 - 64 - 2, 16);
  for (int i = 0; i < MAX_PER_TEAM; i++) {
    int y = (int)lroundf(94 - 64 - 2 + i * 73.5f);
    if (i < teams.n[t]) {
      const jug::Player* p = store ? store->find(teams.code[t][i]) : nullptr;
      if (!p) continue;
      // Ficha horizontal (.team-card): foto de 52 px, nombre, detalle y título.
      lv_obj_t* c = ui::box(col[t], 6, y, 168, 67);
      lv_obj_set_style_radius(c, ui::RADIUS_SM, 0);
      lv_obj_set_style_border_width(c, 2, 0);
      lv_obj_set_style_border_color(c, ui::hex(TEAM_COLOR[t]), 0);
      lv_obj_set_style_shadow_width(c, 10, 0);
      lv_obj_set_style_shadow_color(c, ui::hex(TEAM_COLOR[t]), 0);
      lv_obj_set_style_shadow_opa(c, t ? 115 : 89, 0);
      lv_obj_set_style_bg_color(c, ui::hex(0x0a1628), 0);
      lv_obj_set_style_bg_opa(c, 235, 0);
      lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(c, onRemove, LV_EVENT_CLICKED, (void*)(uintptr_t)p->code);
      photo(c, *p, 3, 5, 52, 20, 10);
      int tx = 3 + 52 + 8;
      lv_obj_t* n = line(c, p->name, 800, 14, ui::color::TEXT, 168 - tx - 10, false);
      lv_obj_set_pos(n, tx, p->title[0] ? 6 : 13);
      lv_obj_t* m = line(c, p->meta, 400, 13, ui::color::TEXT2, 168 - tx - 10, false);
      lv_obj_set_pos(m, tx, p->title[0] ? 24 : 31);
      if (p->title[0]) {
        lv_obj_t* ti = line(c, p->title, 900, 12, ui::color::RANKED, 168 - tx - 10, false);
        lv_obj_set_pos(ti, tx, 42);
      }
    } else {
      lv_obj_t* s = ui::box(col[t], 6, y, 168, 67);
      lv_obj_set_style_radius(s, ui::RADIUS_SM, 0);
      lv_obj_set_style_border_width(s, 1, 0);
      lv_obj_set_style_border_color(s, ui::hex(ui::color::LINE), 0);
      lv_obj_set_style_bg_color(s, ui::hex(0x11223b), 0);
      lv_obj_set_style_bg_opa(s, 153, 0);
      char b[24];
      if (i == 0) snprintf(b, sizeof(b), "Plaza 1");
      else snprintf(b, sizeof(b), "Plaza %d · opcional", i + 1);
      lv_obj_t* l = ui::text(s, b, 400, 13, ui::color::TEXT3);
      lv_obj_center(l);
    }
  }
  // La columna que no tiene el turno, apagada (en la app, al 60 %): un velo oscuro encima, mucho más ligero de
  // pintar que la transparencia de la columna entera.
  if (!active) {
    lv_obj_t* veil = ui::box(col[t], -2, -2, 184, 326);
    lv_obj_set_style_radius(veil, ui::RADIUS, 0);
    lv_obj_set_style_bg_color(veil, ui::hex(0x07101c), 0);
    lv_obj_set_style_bg_opa(veil, 110, 0);
  }
}

// Lista de libres: 3 por fila de 123 × 181 (foto cuadrada y debajo nombre, detalle y título). Las fichas se crean
// todas una vez (makeCards) y en cada toque solo se esconden las elegidas y se recolocan las demás (layoutGrid).
static const float CARD_W = (388 - 2 - 16) / 3.0f;
static void makeCards() {
  lv_obj_clean(grid);
  memset(freeCard, 0, sizeof(freeCard));
  for (uint8_t i = 0; store && i < store->count; i++) {
    const jug::Player& p = store->list[i];
    lv_obj_t* c = ui::box(grid, 0, 0, (int)lroundf(CARD_W), 181);
    lv_obj_set_style_radius(c, ui::RADIUS_SM, 0);
    lv_obj_set_style_border_width(c, 2, 0);
    lv_obj_set_style_border_color(c, ui::hex(ui::color::LINE), 0);
    lv_obj_set_style_bg_color(c, ui::hex(0x0a1628), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(c, LV_OBJ_FLAG_EVENT_BUBBLE);  // que el desplazamiento de la lista siga funcionando
    lv_obj_add_event_cb(c, onPick, LV_EVENT_SHORT_CLICKED, (void*)(uintptr_t)p.code);
    int pw = (int)lroundf(CARD_W) - 4;
    // Arriba de la ficha, con las esquinas de la ficha (radio menos el borde) y recta abajo.
    photo(c, p, 0, 0, pw, 34, ui::RADIUS_SM - 2, false);
    lv_obj_t* n = line(c, p.name, 800, 13, ui::color::TEXT, pw - 8, true);
    lv_obj_set_pos(n, 4, pw + 2);
    lv_obj_t* m = line(c, p.meta, 400, 13, ui::color::TEXT2, pw - 8, true);
    lv_obj_set_pos(m, 4, pw + 19);
    if (p.title[0]) {
      lv_obj_t* ti = line(c, p.title, 900, 12, ui::color::RANKED, pw - 8, true);
      lv_obj_set_pos(ti, 4, pw + 36);
    }
    freeCard[i] = c;
  }
  if (store && store->count == 0) {
    lv_obj_t* l = ui::text(grid, "Sin jugadores", 800, 18, ui::color::TEXT);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_t* l2 = ui::text(grid, "Créalos en la app y conecta la placa por USB.", 400, 14, ui::color::TEXT2);
    lv_obj_align(l2, LV_ALIGN_TOP_MID, 0, 62);
  }
}

static void layoutGrid() {
  int k = 0;
  for (uint8_t i = 0; store && i < store->count; i++) {
    lv_obj_t* c = freeCard[i];
    if (!c) continue;
    if (inTeam(store->list[i].code)) {
      ui::setVisible(c, false);
      continue;
    }
    int32_t x = (int32_t)lroundf((k % 3) * (CARD_W + 8)), y = (k / 3) * (181 + 8);
    k++;
    if (lv_obj_get_x(c) != x || lv_obj_get_y(c) != y) lv_obj_set_pos(c, x, y);
    ui::setVisible(c, true);
  }
}

static void footer() {
  char b[64];
  bool ok = step == 1 && valid();
  uint8_t count = teams.n[step];
  if (ok) snprintf(b, sizeof(b), "%u contra %u listo", teams.n[0], teams.n[1]);
  else if (count == 0) snprintf(b, sizeof(b), "Equipo %s: elige de 1 a %u jugadores", SIDE[step], MAX_PER_TEAM);
  else snprintf(b, sizeof(b), "Equipo %s: %u de %u jugadores", SIDE[step], count, MAX_PER_TEAM);
  lv_label_set_text(noticeText, b);
  // .notice (y .warn hasta que se puede seguir): borde fino y letra de 13 px.
  lv_obj_set_style_border_color(notice, ui::hex(ok ? ui::color::LINE : ui::color::RANKED), 0);
  lv_obj_set_style_text_color(noticeText, ui::hex(ok ? ui::color::TEXT2 : 0xffe9a8), 0);
  if (btnNext) lv_obj_delete(btnNext);
  bool enabled = step == 0 ? teams.n[0] > 0 : valid();
  int w = step == 0 ? 228 : 177;
  btnNext = ui::button(scr, 784 - w, 408, w, 60, step == 0 ? "SIGUIENTE: AZUL" : "CONTINUAR", 18, ui::BtnKind::Primary, onNext);
  if (!enabled) {
    lv_obj_set_style_opa(btnNext, 102, 0);
    lv_obj_remove_flag(btnNext, LV_OBJ_FLAG_CLICKABLE);
  }
  uint8_t chosen = teams.n[0] + teams.n[1];
  bool canSplit = ok && chosen >= 4 && chosen % 2 == 0;
  for (int i = 0; i < 2; i++) {
    if (btnSplit[i]) lv_obj_delete(btnSplit[i]);
    btnSplit[i] = nullptr;
  }
  if (canSplit) {
    int x = 784 - w - 10;
    const char* txt[2] = {"ALEATORIO", "EQUILIBRAR"};
    const int bw[2] = {120, 128};
    for (int i = 0; i < 2; i++) {
      x -= bw[i];
      btnSplit[i] = ui::button(scr, x, 416, bw[i], 44, txt[i], 13, ui::BtnKind::Normal, onSplit, i == 0 ? (void*)1 : nullptr);
      x -= 10;
    }
  }
}

// Fondo y título del turno: degradado de blancos para el Blanco y de azules para el Azul.
static void stepLook() {
  static const uint32_t TOP[2] = {0xdde5ee, 0x2f86e8}, BOTTOM[2] = {0x7e8ea3, 0x0b2a5e};
  lv_obj_set_style_bg_color(scr, ui::hex(TOP[step]), 0);
  lv_obj_set_style_bg_grad_color(scr, ui::hex(BOTTOM[step]), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
  lv_label_set_text(title, step ? "EQUIPO AZUL" : "EQUIPO BLANCO");
  lv_obj_set_style_text_color(title, ui::hex(step ? 0x6fb6f5 : ui::color::TEXT), 0);
  ui::place(title, 70, 15, 26, ui::Align::Left);
  lv_obj_update_layout(title);
  lv_label_set_text(small, modeLabel);
  ui::place(small, 70 + lv_obj_get_width(title) + 10, 21, 16, ui::Align::Left);
}

// Lo que cambia con cada toque.
static uint8_t lookStep = 0xFF;
static void update() {
  if (lookStep != step) {
    lookStep = step;
    stepLook();
  }
  for (int t = 0; t < 2; t++) teamColumn(t);
  layoutGrid();
  footer();
}

// Todo otra vez (al abrir la pantalla o cuando la app ha pasado jugadores o fotos nuevos).
static void refresh() {
  int32_t scroll = lv_obj_get_scroll_y(grid);
  makeCards();
  lookStep = 0xFF;
  colKey[0] = colKey[1] = 0;
  update();
  lv_obj_update_layout(grid);
  lv_obj_scroll_to_y(grid, scroll, LV_ANIM_OFF);
}

static void onDelete(lv_event_t*) {
  scr = nullptr;
  memset(freeCard, 0, sizeof(freeCard));
  btnNext = btnSplit[0] = btnSplit[1] = nullptr;
}

static void build() {
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  // Franjas oscuras arriba (título) y abajo (aviso y botones) sobre el degradado.
  const int bandY[2] = {0, 398}, bandH[2] = {56, 82};
  for (int i = 0; i < 2; i++) {
    lv_obj_t* band = ui::box(scr, 0, bandY[i], 800, bandH[i]);
    lv_obj_set_style_bg_color(band, ui::hex(0x0b1220), 0);
    lv_obj_set_style_bg_opa(band, 225, 0);
    lv_obj_set_style_border_side(band, i ? LV_BORDER_SIDE_TOP : LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(band, 1, 0);
    lv_obj_set_style_border_color(band, ui::hex(ui::color::LINE), 0);
  }
  title = ui::text(scr, "", 900, 20, ui::color::TEXT, 0.5f);
  small = ui::text(scr, "", 600, 13, ui::color::TEXT2, 0.52f);
  // Botón de volver: recuadro con la flecha.
  lv_obj_t* back = ui::box(scr, 12, 6, 44, 44);
  lv_obj_set_style_radius(back, ui::RADIUS_SM, 0);
  lv_obj_set_style_border_width(back, 1, 0);
  lv_obj_set_style_border_color(back, ui::hex(ui::color::LINE), 0);
  lv_obj_set_style_bg_color(back, ui::hex(ui::color::CARD), 0);
  lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
  static lv_point_precise_t chevron[3] = {{24, 13}, {16, 21}, {24, 29}};
  lv_obj_t* arrow = lv_line_create(back);
  lv_line_set_points(arrow, chevron, 3);
  lv_obj_set_style_line_width(arrow, 3, 0);
  lv_obj_set_style_line_rounded(arrow, true, 0);
  lv_obj_set_style_line_color(arrow, ui::hex(ui::color::TEXT), 0);
  lv_obj_remove_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(back, onBack, LV_EVENT_ALL, nullptr);
  const int colX[2] = {12, 604};
  for (int t = 0; t < 2; t++) {
    col[t] = ui::box(scr, colX[t], 64, 184, 326);
    lv_obj_set_style_radius(col[t], ui::RADIUS, 0);
    lv_obj_set_style_border_width(col[t], 2, 0);
    lv_obj_set_style_bg_color(col[t], ui::hex(0x07101c), 0);
    lv_obj_set_style_bg_opa(col[t], 140, 0);
    lv_obj_add_flag(col[t], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(col[t], onCol, LV_EVENT_CLICKED, (void*)(intptr_t)t);
  }
  grid = ui::box(scr, 206, 64, 388, 326);
  lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_width(grid, 4, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(grid, ui::hex(ui::color::LINE), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, LV_PART_SCROLLBAR);
  notice = ui::box(scr, 16, 421, 10, 34);
  lv_obj_set_width(notice, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(notice, 12, 0);
  lv_obj_set_style_radius(notice, ui::RADIUS_SM, 0);
  lv_obj_set_style_border_width(notice, 1, 0);
  noticeText = ui::text(notice, "", 400, 13, ui::color::TEXT2);
  lv_obj_align(noticeText, LV_ALIGN_LEFT_MID, 0, 0);
  btnNext = btnSplit[0] = btnSplit[1] = nullptr;
}

static bool active() { return scr && lv_screen_active() == scr; }

// Abre la pantalla con los equipos vacíos (o los de la vez anterior) en el turno del Blanco.
static void show(jug::Store* s, const char* mode, const Teams* previous = nullptr) {
  store = s;
  modeLabel = mode;
  if (previous) teams = *previous;
  else memset(&teams, 0, sizeof(teams));
  // Quien ya no está en la lista (la app lo borró) sale del equipo.
  for (int t = 0; t < 2; t++) {
    uint8_t k = 0;
    for (uint8_t i = 0; i < teams.n[t]; i++)
      if (s && s->find(teams.code[t][i])) teams.code[t][k++] = teams.code[t][i];
    teams.n[t] = k;
  }
  step = 0;
  build();
  ui::show(scr);
  refresh();
}

}  // namespace elegir

#endif  // MFV3_UI_JUGADORES_H
