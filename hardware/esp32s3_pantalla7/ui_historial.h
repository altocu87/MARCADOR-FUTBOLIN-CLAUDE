// MARCADOR FUTBOLÍN V3 · RANKING → HISTORIAL y detalle de un partido, como la app.
// -------------------------------------------------------------------------------------------------
// Copia de RankingScreen.tsx (pestaña Historial) y MatchDetailScreen / MatchReport (pestaña Resumen):
//   - Arriba, volver y RANKING; las pestañas de la app (de momento solo funciona HISTORIAL: las demás llegan con
//     los cálculos de ELO y niveles).
//   - Filtros TODOS / RÁPIDO / LOCO / CLASIFICATORIO, «Jugador: todos» (elegir de quién) y páginas ‹ 1/8 ›.
//   - Cinco partidos por página (la app pone 20 con desplazamiento; aquí caben cinco sin desplazar y cambiar de
//     página solo repinta las filas).
//   - Detalle: cómo acabó, duración y fecha; el marcador grande; los dos equipos con sus fotos y, en medio, los
//     goles en orden (los del Blanco a la izquierda, los del Azul a la derecha; los anulados, tachados). ELIMINAR
//     lo borra (con confirmación).
// Los partidos salen de s3_partidos.h (tarjeta SD).

#ifndef MFV3_UI_HISTORIAL_H
#define MFV3_UI_HISTORIAL_H

#include "ui_base.h"
#include "s3_jugadores.h"
#include "s3_partidos.h"

namespace historial {

struct Hooks {
  void (*back)() = nullptr;
};
static Hooks hooks;

static jug::Store* store = nullptr;
static lv_obj_t* scr = nullptr;

const int PAGE = 5;
static char modeFilter = 0;      // 0 todos, 'q', 'c', 'r'
static uint32_t whoFilter = 0;   // código del jugador (0 = todos)
static int page = 0;
static int* shown = nullptr;     // índices de partidos::rows que pasan el filtro (el más nuevo primero)
static int shownCount = 0, shownCap = 0;
static char openId[37] = "";     // partido abierto en el detalle

static void go(bool detail);

// ---------------------------------------------------------------- piezas comunes
static lv_obj_t* pill(lv_obj_t* parent, int x, int y, int w, int h, const char* txt, uint16_t weight, uint16_t size, bool on,
                      lv_event_cb_t cb, void* user, int radius = 999) {
  lv_obj_t* b = ui::box(parent, x, y, w, h);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(b, radius, 0);
  lv_obj_set_style_border_width(b, on ? 2 : 1, 0);
  lv_obj_set_style_border_color(b, ui::hex(on ? ui::color::ACCENT : ui::color::LINE), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, ui::hex(on ? 0x123a55 : 0x0c1a2e), 0);
  lv_obj_set_style_bg_color(b, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
  lv_obj_t* l = ui::text(b, txt, weight, size, on ? ui::color::TEXT : ui::color::TEXT2, size * 0.05f);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

// Foto redonda pequeña (o iniciales) de un jugador por su código; si ya no está en la lista, iniciales del nombre.
static void avatar(lv_obj_t* parent, uint32_t code, const char* name, int size) {
  char rel[40];
  const lv_image_dsc_t* pic = code && store->photoRel(code, rel, sizeof(rel)) ? assets::photo(rel, size, size / 2) : nullptr;
  if (pic) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, pic);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return;
  }
  lv_obj_t* b = ui::box(parent, 0, 0, size, size);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, ui::hex(0x1d3a63), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, ui::hex(ui::color::LINE), 0);
  char ini[8];
  jug::initials(name, ini, sizeof(ini));
  lv_obj_t* l = ui::text(b, ini, 800, size >= 40 ? 18 : 12, ui::color::TEXT2);
  lv_obj_center(l);
}

// Flecha ‹ o › (la letra no las trae).
static void chevron(lv_obj_t* b, bool right) {
  static lv_point_precise_t L[3] = {{23, 13}, {16, 21}, {23, 29}}, R[3] = {{17, 13}, {24, 21}, {17, 29}};
  lv_obj_t* a = lv_line_create(b);
  lv_line_set_points(a, right ? R : L, 3);
  lv_obj_set_style_line_width(a, 3, 0);
  lv_obj_set_style_line_rounded(a, true, 0);
  lv_obj_set_style_line_color(a, ui::hex(ui::color::TEXT), 0);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
}

static const char* modeName(char m) { return m == 'c' ? "LOCO" : m == 'r' ? "CLASIFICATORIO" : "RÁPIDO"; }
static uint32_t modeColor(char m) { return m == 'c' ? ui::color::CHAOS : m == 'r' ? ui::color::RANKED : ui::color::ACCENT; }

static void backButton(lv_event_cb_t cb) {
  lv_obj_t* back = ui::box(scr, 12, 12, 46, 46);
  lv_obj_set_style_radius(back, ui::RADIUS_SM, 0);
  lv_obj_set_style_border_width(back, 1, 0);
  lv_obj_set_style_border_color(back, ui::hex(ui::color::LINE), 0);
  lv_obj_set_style_bg_color(back, ui::hex(ui::color::CARD), 0);
  lv_obj_set_style_bg_color(back, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
  static lv_point_precise_t chevron[3] = {{26, 14}, {18, 23}, {26, 32}};
  lv_obj_t* arrow = lv_line_create(back);
  lv_line_set_points(arrow, chevron, 3);
  lv_obj_set_style_line_width(arrow, 3, 0);
  lv_obj_set_style_line_rounded(arrow, true, 0);
  lv_obj_set_style_line_color(arrow, ui::hex(ui::color::TEXT), 0);
  lv_obj_remove_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(back, cb, LV_EVENT_CLICKED, nullptr);
}

// ---------------------------------------------------------------- filtro
static bool playedBy(const partidos::Row& r, uint32_t code) {
  for (int t = 0; t < 2; t++)
    for (int i = 0; i < r.n[t]; i++)
      if (r.code[t][i] == code) return true;
  return false;
}

static void filter() {
  if (shownCap < partidos::count) {
    int nc = partidos::count + 64;
    int* n = (int*)realloc(shown, (size_t)nc * sizeof(int));
    if (!n) return;
    shown = n;
    shownCap = nc;
  }
  shownCount = 0;
  for (int i = partidos::count - 1; i >= 0; i--) {
    const partidos::Row& r = partidos::rows[i];
    if (modeFilter && r.mode != modeFilter) continue;
    if (whoFilter && !playedBy(r, whoFilter)) continue;
    shown[shownCount++] = i;
  }
  int pages = shownCount ? (shownCount + PAGE - 1) / PAGE : 1;
  if (page >= pages) page = pages - 1;
  if (page < 0) page = 0;
}

// ---------------------------------------------------------------- LISTA
static lv_obj_t *rowObj[PAGE], *pagerText = nullptr, *prevBtn = nullptr, *nextBtn = nullptr, *listBox = nullptr;
static lv_obj_t* whoPanel = nullptr;

// Nombres de un lado («Diego + Elena») con sus fotos, como SideTag de la app.
static void side(lv_obj_t* parent, const partidos::Row& r, int t, bool win, int x, int w) {
  lv_obj_t* c = ui::box(parent, x, 0, w, 48);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c, t ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(c, 4, 0);
  char names[96] = "";
  for (int i = 0; i < r.n[t]; i++) {
    if (i) strncat(names, " + ", sizeof(names) - strlen(names) - 1);
    strncat(names, r.name[t][i], sizeof(names) - strlen(names) - 1);
  }
  if (!r.n[t]) snprintf(names, sizeof(names), "%s", t ? "Azul" : "Blanco");
  if (t) {
    lv_obj_t* l = ui::text(c, names, win ? 800 : 400, 14, ui::color::TEXT);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_max_width(l, w - 28 * r.n[t] - 4, 0);
    for (int i = 0; i < r.n[t]; i++) avatar(c, r.code[t][i], r.name[t][i], 24);
  } else {
    for (int i = 0; i < r.n[t]; i++) avatar(c, r.code[t][i], r.name[t][i], 24);
    lv_obj_t* l = ui::text(c, names, win ? 800 : 400, 14, ui::color::TEXT);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_max_width(l, w - 28 * r.n[t] - 4, 0);
  }
}

static void onRow(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < 0 || i >= partidos::count) return;
  snprintf(openId, sizeof(openId), "%s", partidos::rows[i].id);
  go(true);
}

static void fillRows() {
  for (int k = 0; k < PAGE; k++) {
    if (rowObj[k]) lv_obj_delete(rowObj[k]);
    rowObj[k] = nullptr;
  }
  int pages = shownCount ? (shownCount + PAGE - 1) / PAGE : 1;
  char pg[16];
  snprintf(pg, sizeof(pg), "%d/%d", page + 1, pages);
  lv_label_set_text(pagerText, pg);
  // Apagada si no hay más páginas (color de la flecha: la transparencia obligaba a pintar en una capa aparte).
  lv_obj_t* arrows[2] = {lv_obj_get_child(prevBtn, -1), lv_obj_get_child(nextBtn, -1)};
  bool can[2] = {page > 0, page < pages - 1};
  for (int i = 0; i < 2; i++) {
    lv_color_t c = ui::hex(can[i] ? ui::color::TEXT : ui::color::LINE);
    if (arrows[i] && !lv_color_eq(lv_obj_get_style_line_color(arrows[i], LV_PART_MAIN), c)) lv_obj_set_style_line_color(arrows[i], c, 0);
  }
  for (int k = 0; k < PAGE; k++) {
    int si = page * PAGE + k;
    if (si >= shownCount) break;
    int ri = shown[si];
    const partidos::Row& r = partidos::rows[ri];
    lv_obj_t* row = ui::box(listBox, 0, k * 56, 760, 48);
    rowObj[k] = row;
    lv_obj_set_style_radius(row, ui::RADIUS_SM, 0);
    lv_obj_set_style_bg_color(row, ui::hex(0x0e1d33), 0);
    lv_obj_set_style_bg_color(row, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, ui::hex(ui::color::LINE), 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, onRow, LV_EVENT_CLICKED, (void*)(intptr_t)ri);
    char d[24];
    partidos::dateText(r.fin, d, sizeof(d));
    lv_obj_t* dl = ui::text(row, d, 400, 13, ui::color::TEXT3);
    lv_obj_align(dl, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_t* ml = ui::text(row, modeName(r.mode), 800, 12, modeColor(r.mode), 0.5f);
    lv_obj_align(ml, LV_ALIGN_LEFT_MID, 132, 0);
    side(row, r, 0, r.winner == 'w', 264, 152);
    char sc[12];
    snprintf(sc, sizeof(sc), "%u–%u", r.w, r.b);
    lv_obj_t* sl = ui::text(row, sc, 900, 20, ui::color::TEXT);
    lv_obj_align(sl, LV_ALIGN_LEFT_MID, 424 + 32 - 0, 0);
    lv_obj_update_layout(sl);
    lv_obj_set_x(sl, 456 - lv_obj_get_width(sl) / 2);
    side(row, r, 1, r.winner == 'b', 496, 152);
    char tag[20] = "";
    if (r.pw >= 0) snprintf(tag, sizeof(tag), "P %d–%d", r.pw, r.pb);
    else if (r.reason == 'g') snprintf(tag, sizeof(tag), "PRÓRROGA");
    if (tag[0]) {
      lv_obj_t* tb = ui::box(row, 0, 0, 10, 24);
      lv_obj_set_width(tb, LV_SIZE_CONTENT);
      lv_obj_set_style_pad_hor(tb, 9, 0);
      lv_obj_set_style_radius(tb, 999, 0);
      lv_obj_set_style_border_width(tb, 1, 0);
      lv_obj_set_style_border_color(tb, ui::hex(ui::color::LINE), 0);
      lv_obj_set_style_bg_color(tb, ui::hex(0x152844), 0);
      lv_obj_set_style_bg_opa(tb, LV_OPA_COVER, 0);
      lv_obj_t* tl = ui::text(tb, tag, 800, 12, ui::color::TEXT2, 0.5f);
      lv_obj_center(tl);
      lv_obj_align(tb, LV_ALIGN_RIGHT_MID, -10, 0);
    }
  }
  if (!shownCount) {
    lv_obj_t* e = ui::box(listBox, 0, 0, 760, 270);
    rowObj[0] = e;
    lv_obj_t* a = ui::text(e, "Historial vacío", 800, 17, ui::color::TEXT);
    lv_obj_align(a, LV_ALIGN_CENTER, 0, -12);
    lv_obj_t* b = ui::text(e, partidos::count ? "Ningún partido con este filtro." : "Los partidos jugados aparecerán aquí.", 400, 14,
                           ui::color::TEXT2);
    lv_obj_align(b, LV_ALIGN_CENTER, 0, 14);
  }
}

static void onBackList(lv_event_t*) {
  if (hooks.back) hooks.back();
}
static void onTab(lv_event_t*) { ui::toast("Llega con los cálculos de ELO y niveles de la app"); }
static void onMode(lv_event_t* e) {
  modeFilter = (char)(intptr_t)lv_event_get_user_data(e);
  page = 0;
  go(false);
}
static void onPage(lv_event_t* e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  int pages = shownCount ? (shownCount + PAGE - 1) / PAGE : 1;
  if (page + d < 0 || page + d >= pages) return;
  page += d;
  fillRows();
}

// «¿De quién?»: las fichas de los jugadores (como el Modal de la app).
static void onWhoPick(lv_event_t* e) {
  whoFilter = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  page = 0;
  go(false);
}
static void onWhoClose(lv_event_t*) {
  if (whoPanel) lv_obj_add_flag(whoPanel, LV_OBJ_FLAG_HIDDEN);
}
static void onWho(lv_event_t*) {
  if (whoPanel) {
    lv_obj_remove_flag(whoPanel, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  whoPanel = ui::box(scr, 0, 0, 800, 480);
  lv_obj_set_style_bg_color(whoPanel, ui::hex(0x03070d), 0);
  lv_obj_set_style_bg_opa(whoPanel, LV_OPA_COVER, 0);
  lv_obj_add_flag(whoPanel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(whoPanel, onWhoClose, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* m = ui::box(whoPanel, 70, 30, 660, 420);
  lv_obj_add_flag(m, LV_OBJ_FLAG_CLICKABLE);  // tocar dentro no cierra
  lv_obj_set_style_radius(m, ui::RADIUS, 0);
  lv_obj_set_style_bg_color(m, ui::hex(ui::color::SURFACE), 0);
  lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(m, 1, 0);
  lv_obj_set_style_border_color(m, ui::hex(ui::color::LINE), 0);
  lv_obj_t* t = ui::text(m, "¿De quién?", 900, 20, ui::color::TEXT);
  lv_obj_set_pos(t, 24, 18);
  lv_obj_t* g = ui::box(m, 12, 60, 636, 280);
  lv_obj_add_flag(g, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(g, LV_DIR_VER);
  for (int i = 0; i < store->count; i++) {
    const jug::Player& p = store->list[i];
    lv_obj_t* c = ui::box(g, 12 + (i % 6) * 102, (i / 6) * 104, 96, 96);
    lv_obj_set_style_radius(c, ui::RADIUS_SM, 0);
    bool on = p.code == whoFilter;
    lv_obj_set_style_border_width(c, on ? 2 : 1, 0);
    lv_obj_set_style_border_color(c, ui::hex(on ? ui::color::ACCENT : ui::color::LINE), 0);
    lv_obj_set_style_bg_color(c, ui::hex(ui::color::CARD), 0);
    lv_obj_set_style_bg_color(c, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c, onWhoPick, LV_EVENT_CLICKED, (void*)(uintptr_t)p.code);
    lv_obj_t* a = ui::box(c, 20, 8, 56, 56);
    avatar(a, p.code, p.name, 56);
    lv_obj_t* n = ui::text(c, p.name, 700, 12, ui::color::TEXT);
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(n, 88);
    lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(n, 4, 70);
  }
  pill(m, 200, 352, 260, 52, "Todos los jugadores", 800, 14, true, onWhoPick, (void*)(uintptr_t)0, ui::RADIUS_SM);
}

static void buildList() {
  backButton(onBackList);
  lv_obj_t* t = ui::text(scr, "RANKING", 900, 20, ui::color::TEXT, 0.6f);
  lv_obj_set_pos(t, 70, 22);
  // Pestañas de la app.
  static const char* TABS[6] = {"CLASIFICACIÓN", "JUGADORES", "HISTORIAL", "SALÓN DE LA FAMA", "RESUMEN", "PAREJAS"};
  static const int TW[6] = {124, 96, 92, 146, 84, 80};
  int x = 16;
  for (int i = 0; i < 6; i++) {
    pill(scr, x, 80, TW[i], 42, TABS[i], 800, 12, i == 2, i == 2 ? nullptr : onTab, nullptr);
    x += TW[i] + 6;
  }
  // Modalidad, jugador y páginas.
  static const char MODES[4] = {0, 'q', 'c', 'r'};
  static const char* MODE_TXT[4] = {"TODOS", "RÁPIDO", "LOCO", "CLASIFICATORIO"};
  static const int MW[4] = {78, 82, 66, 150};
  x = 16;
  for (int i = 0; i < 4; i++) {
    pill(scr, x, 132, MW[i], 46, MODE_TXT[i], 900, 15, modeFilter == MODES[i], onMode, (void*)(intptr_t)MODES[i], ui::RADIUS_SM);
    x += MW[i] + 6;
  }
  const jug::Player* who = whoFilter ? store->find(whoFilter) : nullptr;
  char wt[48];
  snprintf(wt, sizeof(wt), "JUGADOR: %s", who ? who->name : "TODOS");
  lv_obj_t* wb = pill(scr, x + 2, 132, 200, 46, wt, 900, 15, who != nullptr, onWho, nullptr, ui::RADIUS_SM);
  (void)wb;
  prevBtn = pill(scr, 658, 134, 40, 42, "", 900, 20, false, onPage, (void*)(intptr_t)-1, ui::RADIUS_SM);
  chevron(prevBtn, false);
  pagerText = ui::text(scr, "1/1", 400, 13, ui::color::TEXT2);
  lv_obj_set_pos(pagerText, 703, 148);
  nextBtn = pill(scr, 744, 134, 40, 42, "", 900, 20, false, onPage, (void*)(intptr_t)1, ui::RADIUS_SM);
  chevron(nextBtn, true);
  listBox = ui::box(scr, 16, 190, 760, 280);
  memset(rowObj, 0, sizeof(rowObj));
  filter();
  fillRows();
}

// ---------------------------------------------------------------- DETALLE
static partidos::Detail* detail = nullptr;
static lv_obj_t* confirmBox = nullptr;

static void onBackDetail(lv_event_t*) { go(false); }
static void onDeleteAsk(lv_event_t*) {
  if (confirmBox) lv_obj_remove_flag(confirmBox, LV_OBJ_FLAG_HIDDEN);
}
static void onDeleteNo(lv_event_t*) {
  if (confirmBox) lv_obj_add_flag(confirmBox, LV_OBJ_FLAG_HIDDEN);
}
static void onDeleteYes(lv_event_t*) {
  bool ok = partidos::remove(openId);
  ui::toast(ok ? "Partido eliminado" : "No se pudo eliminar");
  go(false);
}

static const partidos::Row* openRow() {
  for (int i = 0; i < partidos::count; i++)
    if (!strcmp(partidos::rows[i].id, openId)) return &partidos::rows[i];
  return nullptr;
}

static void mmss(uint32_t ms, char* out, size_t n) {
  uint32_t s = ms / 1000;
  snprintf(out, n, "%02lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

// Marco de un equipo con sus jugadores (TeamFrame de la app, sin ELO todavía).
static void teamFrame(const partidos::Row& r, int t, int x, bool win) {
  uint32_t col = t ? 0x4fb0ff : ui::color::WHITE_TEAM;
  lv_obj_t* f = ui::box(scr, x, 212, 240, 256);
  lv_obj_set_style_radius(f, ui::RADIUS, 0);
  lv_obj_set_style_border_width(f, 2, 0);
  lv_obj_set_style_border_color(f, ui::hex(win ? ui::color::RANKED : col), 0);
  lv_obj_set_style_bg_color(f, ui::hex(0x0a1626), 0);
  lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
  // Etiqueta con el nombre del equipo, sobre el borde.
  lv_obj_t* tag = ui::box(scr, x + 40, 196, 160, 34);
  lv_obj_set_style_radius(tag, ui::RADIUS_SM, 0);
  lv_obj_set_style_bg_color(tag, ui::hex(0x08111e), 0);
  lv_obj_set_style_bg_opa(tag, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(tag, 2, 0);
  lv_obj_set_style_border_color(tag, ui::hex(col), 0);
  lv_obj_t* tl = ui::text(tag, t ? "AZUL" : "BLANCO", 900, 20, col, 5);
  lv_obj_center(tl);
  int n = r.n[t];
  if (!n) {
    lv_obj_t* l = ui::text(f, "Sin jugadores elegidos", 400, 13, ui::color::TEXT3);
    lv_obj_center(l);
    return;
  }
  int ch = n <= 2 ? 104 : (230 - 8 * (n - 1)) / n;
  for (int i = 0; i < n; i++) {
    lv_obj_t* c = ui::box(f, 10, 22 + i * (ch + 8), 216, ch);
    lv_obj_set_style_radius(c, ui::RADIUS_SM, 0);
    lv_obj_set_style_bg_color(c, ui::hex(0x13243c), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_border_color(c, ui::hex(ui::color::LINE), 0);
    int ps = ch - 16 > 72 ? 72 : ch - 16;
    lv_obj_t* a = ui::box(c, 8, (ch - ps) / 2, ps, ps);
    char rel[40];
    const lv_image_dsc_t* pic = store->photoRel(r.code[t][i], rel, sizeof(rel)) ? assets::photo(rel, ps, 10) : nullptr;
    if (pic) {
      lv_obj_t* img = lv_image_create(a);
      lv_image_set_src(img, pic);
    } else {
      lv_obj_set_style_radius(a, 10, 0);
      lv_obj_set_style_bg_color(a, ui::hex(0x1d3a63), 0);
      lv_obj_set_style_bg_opa(a, LV_OPA_COVER, 0);
      char ini[8];
      jug::initials(r.name[t][i], ini, sizeof(ini));
      lv_obj_t* il = ui::text(a, ini, 900, ps >= 60 ? 24 : 14, ui::color::TEXT2);
      lv_obj_center(il);
    }
    lv_obj_t* nl = ui::text(c, r.name[t][i], 900, ps >= 60 ? 20 : 15, ui::color::TEXT);
    lv_label_set_long_mode(nl, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(nl, 216 - ps - 26);
    lv_obj_align(nl, LV_ALIGN_LEFT_MID, ps + 16, 0);
  }
}

static void buildDetail() {
  const partidos::Row* r = openRow();
  if (!r) return;
  if (!detail) detail = (partidos::Detail*)malloc(sizeof(partidos::Detail));
  bool have = detail && partidos::loadDetail(r->id, *detail);
  backButton(onBackDetail);
  // Título: la modalidad en su recuadro de color.
  uint32_t mc = modeColor(r->mode);
  lv_obj_t* tb = ui::box(scr, 0, 12, 10, 46);
  lv_obj_set_width(tb, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(tb, 18, 0);
  lv_obj_set_style_radius(tb, ui::RADIUS, 0);
  lv_obj_set_style_border_width(tb, 2, 0);
  lv_obj_set_style_border_color(tb, ui::hex(mc), 0);
  lv_obj_set_style_bg_color(tb, ui::hex(0x0a1220), 0);
  lv_obj_set_style_bg_opa(tb, LV_OPA_COVER, 0);
  const char* full = r->mode == 'c' ? "PARTIDO LOCO" : r->mode == 'r' ? "CLASIFICATORIO" : "PARTIDO RÁPIDO";
  lv_obj_t* tl = ui::text(tb, full, 900, 24, mc, 1.2f);
  lv_obj_center(tl);
  lv_obj_align(tb, LV_ALIGN_TOP_MID, 0, 12);
  // ELIMINAR
  lv_obj_t* del = ui::box(scr, 664, 14, 120, 42);
  lv_obj_set_style_radius(del, ui::RADIUS_SM, 0);
  lv_obj_set_style_border_width(del, 1, 0);
  lv_obj_set_style_border_color(del, ui::hex(ui::color::DANGER), 0);
  lv_obj_set_style_bg_color(del, ui::hex(0x3a1a26), 0);
  lv_obj_set_style_bg_color(del, ui::hex(0x57202f), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(del, LV_OPA_COVER, 0);
  lv_obj_add_flag(del, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(del, onDeleteAsk, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* dl = ui::text(del, "ELIMINAR", 800, 14, 0xffd2d8, 0.6f);
  lv_obj_center(dl);
  // Cómo acabó, duración y fecha, y el marcador grande.
  static const char* REASON[3] = {"Tiempo reglamentario", "Gol de oro en la prórroga", "Tanda de penaltis"};
  char sub[120], dur[12], date[24];
  mmss(have ? detail->totalMs : 0, dur, sizeof(dur));
  partidos::dateText(r->fin, date, sizeof(date));
  int ri = r->reason == 'g' ? 1 : r->reason == 'p' ? 2 : 0;
  if (r->pw >= 0) snprintf(sub, sizeof(sub), "Penaltis %d–%d · %s · %s · %s", r->pw, r->pb, REASON[ri], dur, date);
  else snprintf(sub, sizeof(sub), "%s · %s · %s", REASON[ri], dur, date);
  lv_obj_t* sl = ui::text(scr, sub, 700, 12, ui::color::TEXT2);
  lv_obj_align(sl, LV_ALIGN_TOP_MID, 0, 70);
  char nw[4], nb[4];
  snprintf(nw, sizeof(nw), "%u", r->w);
  snprintf(nb, sizeof(nb), "%u", r->b);
  lv_obj_t* lw = ui::text(scr, nw, 900, 60, r->winner == 'w' ? ui::color::RANKED : ui::color::TEXT);
  lv_obj_t* ls = ui::text(scr, "-", 900, 60, ui::color::TEXT3);
  lv_obj_t* lb = ui::text(scr, nb, 900, 60, r->winner == 'b' ? ui::color::RANKED : ui::color::TEXT);
  lv_obj_update_layout(ls);
  lv_obj_align(ls, LV_ALIGN_TOP_MID, 0, 88);
  lv_obj_align_to(lw, ls, LV_ALIGN_OUT_LEFT_MID, -14, 0);
  lv_obj_align_to(lb, ls, LV_ALIGN_OUT_RIGHT_MID, 14, 0);
  // Equipos y, en medio, los goles.
  teamFrame(*r, 0, 16, r->winner == 'w');
  teamFrame(*r, 1, 544, r->winner == 'b');
  lv_obj_t* mid = ui::box(scr, 268, 196, 264, 272);
  lv_obj_set_style_radius(mid, ui::RADIUS, 0);
  lv_obj_set_style_bg_color(mid, ui::hex(0x08101c), 0);
  lv_obj_set_style_bg_opa(mid, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(mid, 1, 0);
  lv_obj_set_style_border_color(mid, ui::hex(ui::color::LINE), 0);
  lv_obj_add_flag(mid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(mid, LV_DIR_VER);
  lv_obj_set_style_pad_ver(mid, 12, 0);
  if (!have || !detail->goals) {
    lv_obj_t* e = ui::text(mid, "Sin goles", 400, 14, ui::color::TEXT3);
    lv_obj_center(e);
  } else {
    lv_obj_t* lineV = ui::box(mid, 131, 0, 2, 30 + detail->goals * 34);
    lv_obj_set_style_bg_color(lineV, ui::hex(ui::color::RANKED), 0);
    lv_obj_set_style_bg_opa(lineV, 160, 0);
    static const char* PER[3] = {"1ª parte", "2ª parte", "Prórroga"};
    for (uint8_t i = 0; i < detail->goals; i++) {
      const partidos::GoalLine& g = detail->goal[i];
      char tm[8], txt[40];
      mmss(g.ms, tm, sizeof(tm));
      const char* pn = g.period == 0 && detail->singlePeriod ? "Partido" : PER[g.period % 3];
      if (g.value > 1) snprintf(txt, sizeof(txt), "%s · %s  x%u", pn, tm, g.value);
      else snprintf(txt, sizeof(txt), "%s · %s", pn, tm);
      lv_obj_t* b = ui::box(mid, 0, 6 + i * 34, 10, 26);
      lv_obj_set_width(b, LV_SIZE_CONTENT);
      lv_obj_set_style_pad_hor(b, 8, 0);
      lv_obj_set_style_radius(b, 999, 0);
      lv_obj_set_style_border_width(b, 1, 0);
      lv_obj_set_style_border_color(b, ui::hex(g.off ? ui::color::DANGER : g.team ? 0x4fb0ff : ui::color::WHITE_TEAM), 0);
      lv_obj_set_style_bg_color(b, ui::hex(0x0e1d33), 0);
      lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
      lv_obj_t* l = ui::text(b, g.off ? "ANULADO" : txt, 700, 12, g.off ? ui::color::DANGER : ui::color::TEXT);
      lv_obj_center(l);
      lv_obj_update_layout(b);
      int w = lv_obj_get_width(b);
      lv_obj_set_x(b, g.team ? 140 : 124 - w);
      lv_obj_t* dot = ui::box(mid, 128, 14 + i * 34, 8, 8);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(dot, ui::hex(ui::color::RANKED), 0);
      lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    }
  }
  // Confirmación de borrar (como el Modal de la app).
  confirmBox = ui::box(scr, 0, 0, 800, 480);
  lv_obj_set_style_bg_color(confirmBox, ui::hex(0x03070d), 0);
  lv_obj_set_style_bg_opa(confirmBox, LV_OPA_COVER, 0);
  lv_obj_add_flag(confirmBox, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN));
  lv_obj_t* m = ui::box(confirmBox, 150, 110, 500, 260);
  lv_obj_set_style_radius(m, ui::RADIUS, 0);
  lv_obj_set_style_bg_color(m, ui::hex(ui::color::SURFACE), 0);
  lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(m, 1, 0);
  lv_obj_set_style_border_color(m, ui::hex(ui::color::LINE), 0);
  lv_obj_t* q = ui::text(m, "¿Eliminar este partido?", 900, 20, ui::color::TEXT);
  lv_obj_set_pos(q, 24, 22);
  lv_obj_t* qs = ui::text(m, "Se borra del historial y de la tarjeta.\nNo se puede deshacer.", 400, 16, ui::color::TEXT2);
  lv_obj_set_pos(qs, 24, 70);
  pill(m, 24, 170, 210, 64, "Cancelar", 800, 18, false, onDeleteNo, nullptr, ui::RADIUS_SM);
  lv_obj_t* yes = pill(m, 266, 170, 210, 64, "Eliminar", 800, 18, false, onDeleteYes, nullptr, ui::RADIUS_SM);
  lv_obj_set_style_border_color(yes, ui::hex(ui::color::DANGER), 0);
  lv_obj_set_style_bg_color(yes, ui::hex(0x3a1a26), 0);
}

// ---------------------------------------------------------------- pantallas
static void onDelete(lv_event_t*) {
  scr = nullptr;
  whoPanel = confirmBox = nullptr;
  pagerText = prevBtn = nextBtn = listBox = nullptr;
  memset(rowObj, 0, sizeof(rowObj));
}

static bool inDetail = false;
static void go(bool d) {
  inDetail = d && openRow();
  lv_obj_t* old = scr;
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  whoPanel = confirmBox = nullptr;
  memset(rowObj, 0, sizeof(rowObj));
  // Degradado de fondo: azul noche en la lista; en el detalle, teñido del color de la modalidad (la app pone
  // una imagen).
  const partidos::Row* r = inDetail ? openRow() : nullptr;
  if (!r) ui::gradient(scr, 0x0d1d36, 0x04080f);
  else ui::gradient(scr, r->mode == 'c' ? 0x2a1240 : r->mode == 'r' ? 0x2e2410 : 0x0c2a3e, 0x04070d);
  if (inDetail) buildDetail();
  else buildList();
  if (old) lv_obj_remove_event_cb(old, onDelete);
  ui::show(scr);
}

static bool active() { return scr && lv_screen_active() == scr; }

// Al entrar desde Inicio (RANKING): la lista sin filtros. `fresh` = falso al volver del reposo (se queda donde estaba).
static void show(jug::Store* s, bool fresh) {
  store = s;
  if (fresh) {
    modeFilter = 0;
    whoFilter = 0;
    page = 0;
    inDetail = false;
  }
  go(inDetail);
}

}  // namespace historial

#endif  // MFV3_UI_HISTORIAL_H
