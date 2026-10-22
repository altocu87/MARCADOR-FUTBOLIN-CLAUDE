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
#include "s3_progresion.h"

namespace historial {

struct Hooks {
  void (*back)() = nullptr;
  uint32_t (*nowUtc)() = nullptr;  // hora actual (segundos UTC; 0 sin hora) para saber la temporada
};
static Hooks hooks;

static jug::Store* store = nullptr;
static lv_obj_t* scr = nullptr;
static lv_obj_t* host = nullptr;          // lo que hay debajo de las pestañas (se rehace al cambiar de pestaña)
static lv_obj_t* tabPill[6];

const int PAGE = 5;
enum Tab : uint8_t { STANDINGS, PLAYERS, HISTORY, FAME, DIGEST, PAIRS };
static uint8_t tab = STANDINGS;
static bool seasonScope = true;  // CLASIFICACIÓN: la temporada (true) o el histórico
static char modeFilter = 0;      // 0 todos, 'q', 'c', 'r'
static uint32_t whoFilter = 0;   // código del jugador (0 = todos)
static int page = 0;
static int* shown = nullptr;     // índices de partidos::rows que pasan el filtro (el más nuevo primero)
static int shownCount = 0, shownCap = 0;
static char openId[48] = "";     // partido abierto en el detalle

static void go(bool detail);
static void buildContent();

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

// Fotos de cada jugador en los tamaños de estas pantallas, preparadas al encender (si no, la primera visita a una
// pestaña esperaba a reducir cada foto).
static void prewarm(jug::Store* s, void (*tick)() = nullptr) {
  static const int SIZES[4] = {24, 32, 44, 64};
  char rel[40];
  for (int k = 0; k < s->count; k++) {
    if (!s->photoRel(s->list[k].code, rel, sizeof(rel))) continue;
    for (int z : SIZES) assets::photo(rel, z, z / 2);
    assets::photo(rel, 72, 10);
    if (tick) tick();
  }
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
static void restyle(lv_obj_t* b, bool on) {
  lv_color_t bc = ui::hex(on ? ui::color::ACCENT : ui::color::LINE), bg = ui::hex(on ? 0x123a55 : 0x0c1a2e);
  if (lv_obj_get_style_border_width(b, LV_PART_MAIN) != (on ? 2 : 1)) lv_obj_set_style_border_width(b, on ? 2 : 1, 0);
  if (!lv_color_eq(lv_obj_get_style_border_color(b, LV_PART_MAIN), bc)) lv_obj_set_style_border_color(b, bc, 0);
  if (!lv_color_eq(lv_obj_get_style_bg_color(b, LV_PART_MAIN), bg)) lv_obj_set_style_bg_color(b, bg, 0);
  lv_obj_t* l = lv_obj_get_child(b, 0);
  lv_color_t tc = ui::hex(on ? ui::color::TEXT : ui::color::TEXT2);
  if (l && !lv_color_eq(lv_obj_get_style_text_color(l, LV_PART_MAIN), tc)) lv_obj_set_style_text_color(l, tc, 0);
}
static void rebuildContent() {
  if (!host) return go(false);
  while (lv_obj_get_child_count(host)) lv_obj_delete(lv_obj_get_child(host, 0));
  buildContent();
}
static void onTab(lv_event_t* e) {
  int t = (int)(intptr_t)lv_event_get_user_data(e);
  if (t == tab) return;
  tab = (uint8_t)t;
  for (int i = 0; i < 6; i++)
    if (tabPill[i]) restyle(tabPill[i], i == tab);
  rebuildContent();
}

// ---------------------------------------------------------------- rango (CategoryBadge de la app, dibujado)
static void badge(lv_obj_t* parent, uint8_t cat, int size) {
  lv_obj_t* b = ui::box(parent, 0, 0, size, size);
  lv_obj_set_style_radius(b, size / 4, 0);
  lv_obj_set_style_bg_color(b, ui::hex(progresion::CATEGORY_COLOR[cat % 7]), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, ui::hex(0x2a1d10), 0);
  char l[4];
  snprintf(l, sizeof(l), "%.*s", progresion::CATEGORY[cat % 7][0] == '\0' ? 0 : 1, progresion::CATEGORY[cat % 7]);
  lv_obj_t* t = ui::text(b, l, 900, size >= 26 ? 14 : 12, 0x1a1208);
  lv_obj_center(t);
}

// ---------------------------------------------------------------- CLASIFICACIÓN
static void onScope(lv_event_t* e) {
  seasonScope = lv_event_get_user_data(e) != nullptr;
  rebuildContent();
}

static int cmpName(const char* a, const char* b) { return strcasecmp(a, b); }

static void buildStandings() {
  char lab[40], up[40];
  progresion::seasonLabel(progresion::seasonNow[0] ? progresion::seasonNow : "2026-01", lab, sizeof(lab));
  size_t i = 0;
  for (const char* p = lab; *p && i < sizeof(up) - 1; p++) {
    char c = *p;
    up[i++] = c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
  }
  up[i] = 0;
  pill(host, 16, 132, 170, 46, up, 900, 15, seasonScope, onScope, (void*)1, ui::RADIUS_SM);
  pill(host, 192, 132, 170, 46, "HISTÓRICO", 900, 15, !seasonScope, onScope, nullptr, ui::RADIUS_SM);
  const progresion::Snapshot& src = seasonScope ? progresion::season : progresion::all;
  // Jugadores de la lista con algún clasificatorio, por ELO, partidos y nombre.
  int idx[jug::MAX], n = 0;
  for (int k = 0; k < store->count; k++) {
    int pi = src.find(store->list[k].code);
    if (pi >= 0 && src.p[pi].rankedPlayed > 0) idx[n++] = k;
  }
  for (int a = 1; a < n; a++)
    for (int b = a; b > 0; b--) {
      const progresion::Player &x = src.p[src.find(store->list[idx[b]].code)], &y = src.p[src.find(store->list[idx[b - 1]].code)];
      bool before = x.elo != y.elo ? x.elo > y.elo
                    : x.rankedPlayed != y.rankedPlayed ? x.rankedPlayed > y.rankedPlayed
                    : cmpName(store->list[idx[b]].name, store->list[idx[b - 1]].name) < 0;
      if (!before) break;
      int t = idx[b];
      idx[b] = idx[b - 1];
      idx[b - 1] = t;
    }
  if (!n) {
    lv_obj_t* a = ui::text(host, "Sin partidos clasificatorios", 800, 17, ui::color::TEXT);
    lv_obj_align(a, LV_ALIGN_TOP_MID, 0, 290);
    lv_obj_t* b = ui::text(host, seasonScope ? "Nadie ha jugado un Clasificatorio esta temporada." : "Juega un Clasificatorio para aparecer.", 400, 14,
                           ui::color::TEXT2);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 318);
    return;
  }
  static const char* HEAD[6] = {"#", "JUGADOR", "ELO", "CATEGORÍA", "PJ", "FORMA"};
  static const int HX[6] = {28, 76, 370, 448, 556, 614};
  for (int k = 0; k < 6; k++) {
    lv_obj_t* h = ui::text(host, HEAD[k], 700, 12, ui::color::TEXT2, 1.2f);
    lv_obj_set_pos(h, HX[k], 188);
  }
  lv_obj_t* list = ui::box(host, 16, 208, 768, 266);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_pad_bottom(list, 8, 0);
  static const uint32_t POS[3] = {ui::color::RANKED, 0xc9d3e0, 0xc98a54};
  for (int r = 0; r < n; r++) {
    const jug::Player& pl = store->list[idx[r]];
    const progresion::Player& pr = src.p[src.find(pl.code)];
    lv_obj_t* row = ui::box(list, 0, r * 56, 760, 50);
    lv_obj_set_style_radius(row, ui::RADIUS_SM, 0);
    lv_obj_set_style_bg_color(row, ui::hex(r == 0 ? 0x2a2a20 : 0x0e1d33), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, ui::hex(r == 0 ? 0x8f7a32 : ui::color::LINE), 0);
    char t[16];
    snprintf(t, sizeof(t), "%d", r + 1);
    lv_obj_t* pos = ui::text(row, t, 900, 20, r < 3 ? POS[r] : ui::color::TEXT);
    lv_obj_align(pos, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_t* av = ui::box(row, 58, 9, 32, 32);
    avatar(av, pl.code, pl.name, 32);
    lv_obj_t* nm = ui::text(row, pl.name, 400, 16, ui::color::TEXT);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(nm, 250);
    lv_obj_align(nm, LV_ALIGN_LEFT_MID, 98, 0);
    snprintf(t, sizeof(t), "%d", pr.elo);
    lv_obj_t* el = ui::text(row, t, 800, 17, ui::color::TEXT);
    lv_obj_align(el, LV_ALIGN_LEFT_MID, 354, 0);
    lv_obj_t* bg = ui::box(row, 430, 11, 28, 28);
    badge(bg, pr.category, 28);
    lv_obj_t* cn = ui::text(row, progresion::CATEGORY[pr.category], 700, 15, progresion::CATEGORY_COLOR[pr.category]);
    lv_obj_align(cn, LV_ALIGN_LEFT_MID, 464, 0);
    snprintf(t, sizeof(t), "%d", pr.rankedPlayed);
    lv_obj_t* pj = ui::text(row, t, 400, 16, ui::color::TEXT2);
    lv_obj_align(pj, LV_ALIGN_LEFT_MID, 542, 0);
    for (int f = 0; pr.rform[f]; f++) {
      char c = pr.rform[f];
      lv_obj_t* chip = ui::box(row, 598 + f * 28, 13, 24, 24);
      lv_obj_set_style_radius(chip, 6, 0);
      lv_obj_set_style_bg_color(chip, ui::hex(c == 'G' ? 0x1d4a36 : c == 'P' ? 0x4a1d2a : 0x2c3a52), 0);
      lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
      char cs[2] = {c, 0};
      lv_obj_t* cl = ui::text(chip, cs, 800, 12, c == 'G' ? ui::color::OK : c == 'P' ? ui::color::DANGER : ui::color::TEXT2);
      lv_obj_center(cl);
    }
  }
}

// ---------------------------------------------------------------- JUGADORES
static void buildPlayers() {
  if (!store->count) {
    lv_obj_t* a = ui::text(host, "Sin jugadores", 800, 17, ui::color::TEXT);
    lv_obj_align(a, LV_ALIGN_TOP_MID, 0, 260);
    lv_obj_t* b = ui::text(host, "Créalos en Ajustes → Jugadores.", 400, 14, ui::color::TEXT2);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 288);
    return;
  }
  int idx[jug::MAX];
  for (int k = 0; k < store->count; k++) idx[k] = k;
  for (int a = 1; a < store->count; a++)
    for (int b = a; b > 0 && cmpName(store->list[idx[b]].name, store->list[idx[b - 1]].name) < 0; b--) {
      int t = idx[b];
      idx[b] = idx[b - 1];
      idx[b - 1] = t;
    }
  lv_obj_t* g = ui::box(host, 16, 132, 768, 342);
  lv_obj_add_flag(g, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(g, LV_DIR_VER);
  lv_obj_set_style_pad_bottom(g, 8, 0);
  for (int k = 0; k < store->count; k++) {
    const jug::Player& pl = store->list[idx[k]];
    const progresion::Player* pr = progresion::of(pl.code);
    lv_obj_t* c = ui::box(g, (k % 3) * 258, (k / 3) * 104, 250, 96);
    lv_obj_set_style_radius(c, ui::RADIUS, 0);
    lv_obj_set_style_bg_color(c, ui::hex(0x0e1d33), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_border_color(c, ui::hex(pr ? progresion::CATEGORY_COLOR[pr->category] : ui::color::LINE), 0);
    lv_obj_t* av = ui::box(c, 14, 16, 64, 64);
    avatar(av, pl.code, pl.name, 64);
    lv_obj_t* nm = ui::text(c, pl.name, 800, 17, ui::color::TEXT);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(nm, 150);
    lv_obj_set_pos(nm, 90, 14);
    char t[40];
    if (pr && pr->played) snprintf(t, sizeof(t), "Nivel %d", pr->level);
    else snprintf(t, sizeof(t), "Sin partidos");
    lv_obj_t* lv = ui::text(c, t, 400, 14, ui::color::TEXT2);
    lv_obj_set_pos(lv, 90, 40);
    if (pr && pr->rankedPlayed > 0) {
      lv_obj_t* bg = ui::box(c, 90, 62, 22, 22);
      badge(bg, pr->category, 22);
      snprintf(t, sizeof(t), "%s · %d", progresion::CATEGORY[pr->category], pr->elo);
      lv_obj_t* rk = ui::text(c, t, 700, 14, progresion::CATEGORY_COLOR[pr->category]);
      lv_obj_set_pos(rk, 118, 64);
    }
  }
}
static void onMode(lv_event_t* e) {
  modeFilter = (char)(intptr_t)lv_event_get_user_data(e);
  page = 0;
  rebuildContent();
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
  if (whoPanel) {
    lv_obj_delete(whoPanel);
    whoPanel = nullptr;
  }
  page = 0;
  rebuildContent();
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
    lv_obj_t* a = ui::box(c, 16, 6, 64, 64);
    avatar(a, p.code, p.name, 64);
    lv_obj_t* n = ui::text(c, p.name, 700, 12, ui::color::TEXT);
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(n, 88);
    lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(n, 4, 70);
  }
  pill(m, 200, 352, 260, 52, "Todos los jugadores", 800, 14, true, onWhoPick, (void*)(uintptr_t)0, ui::RADIUS_SM);
}

static void buildFame();
static void buildDigest();
static void buildPairs();
static void buildContent();
static void buildHistory();
static void buildList() {
  backButton(onBackList);
  lv_obj_t* t = ui::text(scr, "RANKING", 900, 20, ui::color::TEXT, 0.6f);
  lv_obj_set_pos(t, 70, 22);
  // Pestañas de la app.
  static const char* TABS[6] = {"CLASIFICACIÓN", "JUGADORES", "HISTORIAL", "SALÓN DE LA FAMA", "RESUMEN", "PAREJAS"};
  static const int TW[6] = {124, 96, 92, 146, 84, 80};
  int x = 16;
  for (int i = 0; i < 6; i++) {
    tabPill[i] = pill(scr, x, 80, TW[i], 42, TABS[i], 800, 12, i == tab, onTab, (void*)(intptr_t)i);
    x += TW[i] + 6;
  }
  host = ui::box(scr, 0, 0, 800, 480);
  buildContent();
}

// Lo de debajo de las pestañas, según la pestaña.
static void buildContent() {
  memset(rowObj, 0, sizeof(rowObj));
  if (tab == STANDINGS) return buildStandings();
  if (tab == PLAYERS) return buildPlayers();
  if (tab == FAME) return buildFame();
  if (tab == DIGEST) return buildDigest();
  if (tab == PAIRS) return buildPairs();
  buildHistory();
}

static void buildHistory() {
  // Modalidad, jugador y páginas.
  static const char MODES[4] = {0, 'q', 'c', 'r'};
  static const char* MODE_TXT[4] = {"TODOS", "RÁPIDO", "LOCO", "CLASIFICATORIO"};
  static const int MW[4] = {78, 82, 66, 150};
  int x = 16;
  for (int i = 0; i < 4; i++) {
    pill(host, x, 132, MW[i], 46, MODE_TXT[i], 900, 15, modeFilter == MODES[i], onMode, (void*)(intptr_t)MODES[i], ui::RADIUS_SM);
    x += MW[i] + 6;
  }
  const jug::Player* who = whoFilter ? store->find(whoFilter) : nullptr;
  char wt[48];
  snprintf(wt, sizeof(wt), "JUGADOR: %s", who ? who->name : "TODOS");
  lv_obj_t* wb = pill(host, x + 2, 132, 200, 46, wt, 900, 15, who != nullptr, onWho, nullptr, ui::RADIUS_SM);
  (void)wb;
  prevBtn = pill(host, 658, 134, 40, 42, "", 900, 20, false, onPage, (void*)(intptr_t)-1, ui::RADIUS_SM);
  chevron(prevBtn, false);
  pagerText = ui::text(host, "1/1", 400, 13, ui::color::TEXT2);
  lv_obj_set_pos(pagerText, 703, 148);
  nextBtn = pill(host, 744, 134, 40, 42, "", 900, 20, false, onPage, (void*)(intptr_t)1, ui::RADIUS_SM);
  chevron(nextBtn, true);
  listBox = ui::box(host, 16, 190, 760, 280);
  memset(rowObj, 0, sizeof(rowObj));
  filter();
  fillRows();
}


// ---------------------------------------------------------------- piezas de las tres últimas pestañas
static lv_obj_t* scrollPage(int y) {
  lv_obj_t* g = ui::box(host, 16, y, 768, 474 - y);
  lv_obj_add_flag(g, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(g, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(g, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_pad_bottom(g, 8, 0);
  return g;
}
static lv_obj_t* card(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* c = ui::box(parent, x, y, w, h);
  lv_obj_set_style_radius(c, ui::RADIUS, 0);
  lv_obj_set_style_bg_color(c, ui::hex(0x11203a), 0);  // liso: el degradado de la app costaba pintarlo en cada tarjeta
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_border_color(c, ui::hex(ui::color::LINE), 0);
  return c;
}
static void cardTitle(lv_obj_t* c, const char* t) {
  lv_obj_t* l = ui::text(c, t, 800, 12, ui::color::TEXT2, 1.4f);
  lv_obj_set_pos(l, 12, 10);
}
static const char* nameOf(uint32_t code, const char* fallback) {
  const jug::Player* p = store->find(code);
  return p ? p->name : fallback;
}
static void empty(const char* a, const char* b, int y = 270) {
  lv_obj_t* x = ui::text(host, a, 800, 17, ui::color::TEXT);
  lv_obj_align(x, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_t* z = ui::text(host, b, 400, 14, ui::color::TEXT2);
  lv_obj_align(z, LV_ALIGN_TOP_MID, 0, y + 28);
}
// «Vicky 5–2 Óscar» con sus fotos (MatchLine de la app).
static void matchLine(lv_obj_t* parent, const partidos::Row& r, int x, int y, int w) {
  lv_obj_t* c = ui::box(parent, x, y, w, 28);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(c, 5, 0);
  for (int t = 0; t < 2; t++) {
    char names[80] = "";
    for (int i = 0; i < r.n[t]; i++) {
      if (i) strncat(names, " + ", sizeof(names) - strlen(names) - 1);
      strncat(names, r.name[t][i], sizeof(names) - strlen(names) - 1);
    }
    if (!r.n[t]) snprintf(names, sizeof(names), "%s", t ? "Azul" : "Blanco");
    if (!t)
      for (int i = 0; i < r.n[t]; i++) avatar(c, r.code[t][i], r.name[t][i], 24);
    ui::text(c, names, 400, 14, ui::color::TEXT2);
    if (!t) {
      char sc[12];
      snprintf(sc, sizeof(sc), "%u–%u", r.w, r.b);
      ui::text(c, sc, 800, 14, ui::color::TEXT);
    } else {
      for (int i = 0; i < r.n[t]; i++) avatar(c, r.code[t][i], r.name[t][i], 24);
    }
  }
}

// ---------------------------------------------------------------- SALÓN DE LA FAMA
struct Leader {
  uint32_t code;
  double v;
};
// Los tres mejores de la lista de jugadores (como top() de records.ts: solo valores > 0; empates en el orden de la lista).
static int top3(double (*score)(const jug::Player&, bool&), Leader out[3]) {
  int n = 0;
  for (int k = 0; k < store->count; k++) {
    bool ok = true;
    double v = score(store->list[k], ok);
    if (!ok || v <= 0) continue;
    int pos = n < 3 ? n : 3;
    while (pos > 0 && out[pos - 1].v < v) pos--;
    if (pos >= 3) continue;
    for (int j = (n < 3 ? n : 2); j > pos; j--) out[j] = out[j - 1];
    out[pos] = Leader{store->list[k].code, v};
    if (n < 3) n++;
  }
  return n;
}
static uint16_t* pgFull = nullptr;  // goles apuntados por jugador (de la lista) en partidos con todos los goles apuntados
static double scElo(const jug::Player& p, bool& ok) {
  const progresion::Player* x = progresion::of(p.code);
  ok = x && x->rankedPlayed > 0;
  return ok ? x->elo : 0;
}
static double scXp(const jug::Player& p, bool& ok) {
  const progresion::Player* x = progresion::of(p.code);
  ok = x != nullptr;
  return x ? (double)x->xp : 0;
}
static double scWins(const jug::Player& p, bool& ok) {
  const progresion::Player* x = progresion::of(p.code);
  return x ? x->wins : 0;
}
static double scStreak(const jug::Player& p, bool& ok) {
  const progresion::Player* x = progresion::of(p.code);
  return x ? x->bestStreak : 0;
}
static double scPct(const jug::Player& p, bool& ok) {
  const progresion::Player* x = progresion::of(p.code);
  ok = x && x->played >= 10;
  return ok ? x->wins * 100.0 / x->played : 0;
}
static double scPlayed(const jug::Player& p, bool& ok) {
  const progresion::Player* x = progresion::of(p.code);
  return x ? x->played : 0;
}
static double scGoals(const jug::Player& p, bool& ok) {
  for (int k = 0; k < store->count; k++)
    if (store->list[k].code == p.code) return pgFull ? pgFull[k] : 0;
  return 0;
}

static void podium(lv_obj_t* c, const Leader* l, int n, bool pct, bool xp) {
  // 2.º a la izquierda, 1.º en medio, 3.º a la derecha, con su escalón.
  static const int ORDER[3] = {1, 0, 2}, X[3] = {14, 88, 162}, STEP[3] = {40, 28, 18};
  static const uint32_t COL[3] = {0xf2d36b, 0xe8edf5, 0xd9a273}, INK[3] = {0x3b2a00, 0x1d2633, 0x3a2210};
  for (int k = 0; k < 3; k++) {
    int i = ORDER[k];
    lv_obj_t* st = ui::box(c, X[k], 168 - 12 - STEP[i], 72, STEP[i]);
    lv_obj_set_style_radius(st, 6, 0);
    lv_obj_set_style_bg_color(st, ui::hex(COL[i]), 0);
    lv_obj_set_style_bg_opa(st, i < n ? LV_OPA_COVER : 60, 0);
    char num[4];
    snprintf(num, sizeof(num), "%d", i + 1);
    lv_obj_t* nl = ui::text(st, num, 900, 20, INK[i]);
    lv_obj_center(nl);
    if (i >= n) continue;
    int as = i == 0 ? 44 : 32;
    int top = 168 - 12 - STEP[i] - 4 - 36 - as;
    lv_obj_t* av = ui::box(c, X[k] + 36 - as / 2, top, as, as);
    avatar(av, l[i].code, nameOf(l[i].code, "?"), as);
    if (i == 0) {
      lv_obj_set_style_border_width(av, 2, 0);
      lv_obj_set_style_border_color(av, ui::hex(ui::color::RANKED), 0);
      lv_obj_set_style_radius(av, LV_RADIUS_CIRCLE, 0);
    }
    lv_obj_t* nm = ui::text(c, nameOf(l[i].code, "?"), 800, 14, ui::color::TEXT);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(nm, 76);
    lv_obj_set_style_text_align(nm, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(nm, X[k] - 2, top + as + 2);
    char v[24];
    if (pct) snprintf(v, sizeof(v), "%.0f %%", l[i].v);
    else if (xp) snprintf(v, sizeof(v), "%.0f XP", l[i].v);
    else snprintf(v, sizeof(v), "%.0f", l[i].v);
    lv_obj_t* vl = ui::text(c, v, 400, 13, ui::color::TEXT2);
    lv_obj_set_width(vl, 76);
    lv_obj_set_style_text_align(vl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(vl, X[k] - 2, top + as + 19);
  }
}

static void buildFame() {
  free(pgFull);
  pgFull = (uint16_t*)calloc(jug::MAX, sizeof(uint16_t));
  for (int i = 0; i < partidos::count; i++) {
    const partidos::Row& r = partidos::rows[i];
    if (r.goalsValid != r.goalsAssigned) continue;
    for (int t = 0; t < 2; t++)
      for (int k = 0; k < r.n[t]; k++)
        for (int j = 0; j < store->count; j++)
          if (store->list[j].code == r.code[t][k]) pgFull[j] += r.pg[t][k];
  }
  struct Def {
    const char* title;
    double (*score)(const jug::Player&, bool&);
    bool pct, xp;
  };
  static const Def DEFS[] = {{"MAYOR ELO", scElo, false, false},          {"MÁS EXPERIENCIA", scXp, false, true},
                             {"MÁS VICTORIAS", scWins, false, false},     {"MEJOR RACHA", scStreak, false, false},
                             {"% VICTORIAS (MÍN. 10)", scPct, true, false}, {"MÁS PARTIDOS", scPlayed, false, false},
                             {"PICHICHI", scGoals, false, false}};
  lv_obj_t* g = scrollPage(132);
  int shown = 0;
  for (const Def& d : DEFS) {
    Leader l[3];
    int n = top3(d.score, l);
    if (!n) continue;
    lv_obj_t* c = card(g, (shown % 3) * 258, (shown / 3) * 178, 248, 168);
    cardTitle(c, d.title);
    podium(c, l, n, d.pct, d.xp);
    shown++;
  }
  // Campeones de temporada (las ya terminadas, de la más nueva a la más vieja).
  {
    char keys[24][10];
    int nk = 0;
    for (int i = 0; i < partidos::count && nk < 24; i++) {
      if (partidos::rows[i].mode != 'r' || !partidos::rows[i].fin) continue;
      char k[10];
      progresion::seasonKey(partidos::rows[i].fin, k);
      if (!strcmp(k, progresion::seasonNow)) continue;
      bool dup = false;
      for (int j = 0; j < nk; j++) dup |= !strcmp(keys[j], k);
      if (!dup) snprintf(keys[nk++], 10, "%s", k);
    }
    qsort(keys, nk, 10, [](const void* a, const void* b) { return -strcmp((const char*)a, (const char*)b); });
    if (nk) {
      uint32_t codes[jug::MAX];
      for (int k = 0; k < store->count; k++) codes[k] = store->list[k].code;
      progresion::Snapshot tmp;
      lv_obj_t* c = card(g, (shown % 3) * 258, (shown / 3) * 178, 248, 168);
      cardTitle(c, "CAMPEONES DE TEMPORADA");
      int y = 34;
      for (int j = 0; j < nk && y < 150; j++) {
        progresion::compute(tmp, codes, store->count, progresion::keepRankedSeason, keys[j], false);
        int best = -1;
        for (int i = 0; i < tmp.n; i++) {
          const progresion::Player& x = tmp.p[i];
          if (x.rankedPlayed <= 0) continue;
          if (best < 0 || x.elo > tmp.p[best].elo || (x.elo == tmp.p[best].elo && x.rankedPlayed > tmp.p[best].rankedPlayed)) best = i;
        }
        if (best < 0) continue;
        lv_obj_t* av = ui::box(c, 12, y - 3, 32, 32);
        avatar(av, tmp.p[best].code, nameOf(tmp.p[best].code, "?"), 32);
        lv_obj_t* nm = ui::text(c, nameOf(tmp.p[best].code, "?"), 800, 14, ui::color::TEXT);
        lv_obj_set_pos(nm, 44, y + 4);
        char lab[32];
        progresion::seasonLabel(keys[j], lab, sizeof(lab));
        lv_obj_t* sl = ui::text(c, lab, 400, 13, ui::color::TEXT3);
        lv_obj_align(sl, LV_ALIGN_TOP_RIGHT, -12, y + 5);
        y += 32;
      }
      tmp.clear();
      shown++;
    }
  }
  int y = ((shown + 2) / 3) * 178;
  if (!shown) {
    empty("Salón de la fama vacío", "Los líderes aparecerán al guardar partidos.");
    return;
  }
  lv_obj_t* lab = ui::text(g, "RÉCORDS DE PARTIDO", 800, 12, ui::color::TEXT2, 1.4f);
  lv_obj_set_pos(lab, 0, y + 4);
  y += 26;
  // Récords de partido (computeRecords): el primero que consigue la marca la conserva.
  struct Rec {
    const char* title;
    int best;
    double v;
  } recs[5] = {{"Gol más rápido", -1, 0}, {"Mayor goleada", -1, 0}, {"Más goles en un encuentro", -1, 0}, {"Mayor remontada", -1, 0},
               {"Encuentro más largo", -1, 0}};
  int* ord = (int*)malloc((size_t)(partidos::count ? partidos::count : 1) * sizeof(int));
  if (!ord) { ui::text(host, "Memoria insuficiente para los detalles", 700, 16, ui::color::TEXT2); return; }
  for (int i = 0; i < partidos::count; i++) ord[i] = i;
  qsort(ord, partidos::count, sizeof(int), progresion::byFinish);
  for (int oi = 0; oi < partidos::count; oi++) {
    const partidos::Row& r = partidos::rows[ord[oi]];
    long e0 = r.earliest[0], e1 = r.earliest[1];
    double vals[5] = {(double)(e0 < 0 ? e1 : e1 < 0 ? e0 : (e0 < e1 ? e0 : e1)), (double)abs((int)r.w - (int)r.b), (double)(r.w + r.b),
                      (double)r.comeback, (double)r.totalMs};
    bool has[5] = {e0 >= 0 || e1 >= 0, true, true, true, true};
    for (int k = 0; k < 5; k++) {
      if (!has[k]) continue;
      if (recs[k].best < 0 || (k == 0 ? vals[k] < recs[k].v : vals[k] > recs[k].v)) recs[k].best = ord[oi], recs[k].v = vals[k];
    }
  }
  free(ord);
  for (int k = 0; k < 5; k++) {
    if (recs[k].best < 0 || (k >= 1 && k <= 3 && recs[k].v <= 0)) continue;
    const partidos::Row& r = partidos::rows[recs[k].best];
    lv_obj_t* row = card(g, 0, y, 758, 56);
    lv_obj_set_style_radius(row, ui::RADIUS_SM, 0);
    lv_obj_t* t = ui::text(row, recs[k].title, 800, 17, ui::color::TEXT);
    lv_obj_set_pos(t, 12, 4);
    matchLine(row, r, 10, 26, 560);
    char d[24], v[24];
    partidos::dateText(r.fin, d, sizeof(d));
    lv_obj_t* dl = ui::text(row, d, 400, 13, ui::color::TEXT3);
    lv_obj_align(dl, LV_ALIGN_TOP_RIGHT, -110, 32);
    if (k == 0) snprintf(v, sizeof(v), "%.1f s", recs[k].v / 1000.0);
    else if (k == 1) snprintf(v, sizeof(v), "+%.0f", recs[k].v);
    else if (k == 3) snprintf(v, sizeof(v), "−%.0f", recs[k].v);
    else if (k == 4) snprintf(v, sizeof(v), "%02lu:%02lu", (unsigned long)(recs[k].v / 60000), (unsigned long)((long)(recs[k].v / 1000) % 60));
    else snprintf(v, sizeof(v), "%.0f", recs[k].v);
    lv_obj_t* vl = ui::text(row, v, 900, 20, ui::color::RANKED);
    lv_obj_align(vl, LV_ALIGN_RIGHT_MID, -12, 0);
    y += 62;
  }
}

// ---------------------------------------------------------------- RESUMEN
enum DigestSpan : uint8_t { DIGEST_WEEK, DIGEST_MONTH, DIGEST_ALL };
static DigestSpan digestSpan = DIGEST_WEEK;
static bool digestSpanChosen = false;
static void onSpan(lv_event_t* e) {
  digestSpan = (DigestSpan)(intptr_t)lv_event_get_user_data(e);
  digestSpanChosen = true;
  rebuildContent();
}
static void buildDigest() {
  uint32_t now = hooks.nowUtc ? hooks.nowUtc() : 0;
  int undated = 0;
  for (int i = 0; i < partidos::count; i++) if (!partidos::rows[i].fin) undated++;
  // Sin una selección explícita, no esconder los partidos que carecen de fecha.
  if (!digestSpanChosen) digestSpan = !now || undated ? DIGEST_ALL : DIGEST_WEEK;
  pill(host, 16, 132, 190, 46, "ÚLTIMOS 7 DÍAS", 900, 15, digestSpan == DIGEST_WEEK, onSpan, nullptr, ui::RADIUS_SM);
  pill(host, 212, 132, 190, 46, "ÚLTIMOS 30 DÍAS", 900, 15, digestSpan == DIGEST_MONTH, onSpan, (void*)1, ui::RADIUS_SM);
  pill(host, 408, 132, 150, 46, "TODOS", 900, 15, digestSpan == DIGEST_ALL, onSpan, (void*)2, ui::RADIUS_SM);
  if (undated) {
    char b[60];
    snprintf(b, sizeof(b), digestSpan == DIGEST_ALL ? "Incluye %d sin fecha" : "%d sin fecha: ver TODOS", undated);
    lv_obj_t* hint = ui::text(host, b, 400, 13, ui::color::TEXT2);
    lv_obj_set_pos(hint, 572, 145);
    lv_obj_set_width(hint, 212);
  }
  uint32_t days = digestSpan == DIGEST_MONTH ? 30u : 7u;
  uint32_t from = now > days * 86400u ? now - days * 86400u : 0;
  int* ord = (int*)malloc((size_t)(partidos::count ? partidos::count : 1) * sizeof(int));
  if (!ord) { ui::text(host, "Memoria insuficiente para los detalles", 700, 16, ui::color::TEXT2); return; }
  int nl = 0;
  for (int i = 0; i < partidos::count; i++)
    if (digestSpan == DIGEST_ALL || (now && partidos::rows[i].fin && partidos::rows[i].fin >= from && partidos::rows[i].fin <= now))
      ord[nl++] = i;
  qsort(ord, nl, sizeof(int), progresion::byFinish);
  if (!nl) {
    free(ord);
    if (!partidos::count) empty("Todavía no hay partidos", "El resumen se rellena al terminar un partido.");
    else if (!now && digestSpan != DIGEST_ALL) empty("Falta la fecha y hora", "Consulta TODOS y conecta Wi-Fi para poner la hora.");
    else if (undated) empty("Hay partidos sin fecha", "Puedes verlos en TODOS; no se asignan a un periodo.");
    else empty("Sin partidos en este periodo", "Prueba con TODOS para ver el historial completo.");
    return;
  }
  // Por jugador (en orden de aparición): victorias y partidos; goles apuntados en partidos completos.
  struct W {
    uint32_t code;
    char name[20];
    int wins, played, goals;
  };
  static W w[96];
  int nw = 0, goals = 0;
  int closest = -1, biggest = -1;
  for (int oi = 0; oi < nl; oi++) {
    const partidos::Row& r = partidos::rows[ord[oi]];
    goals += r.w + r.b;
    for (int t = 0; t < 2; t++)
      for (int i = 0; i < r.n[t]; i++) {
        int j = 0;
        while (j < nw && w[j].code != r.code[t][i]) j++;
        if (j == nw) {
          if (nw >= 96) continue;
          w[nw] = W{r.code[t][i], "", 0, 0, 0};
          snprintf(w[nw].name, sizeof(w[0].name), "%s", nameOf(r.code[t][i], "?"));
          nw++;
        }
        w[j].played++;
        if ((t == 0 && r.winner == 'w') || (t == 1 && r.winner == 'b')) w[j].wins++;
        if (r.goalsValid == r.goalsAssigned) w[j].goals += r.pg[t][i];
      }
    int diff = abs((int)r.w - (int)r.b), tot = r.w + r.b;
    if (closest < 0) closest = ord[oi];
    else {
      const partidos::Row& c = partidos::rows[closest];
      int cd = abs((int)c.w - (int)c.b), ct = c.w + c.b;
      if (diff < cd || (diff == cd && tot > ct)) closest = ord[oi];
    }
    if (biggest < 0 || diff > abs((int)partidos::rows[biggest].w - (int)partidos::rows[biggest].b)) biggest = ord[oi];
  }
  free(ord);
  int pop = -1, act = -1, sc = -1;
  for (int j = 0; j < nw; j++) {
    if (pop < 0 || w[j].wins > w[pop].wins || (w[j].wins == w[pop].wins && w[j].played < w[pop].played)) pop = j;
    if (act < 0 || w[j].played > w[act].played) act = j;
    if (w[j].goals > 0 && (sc < 0 || w[j].goals > w[sc].goals)) sc = j;
  }
  lv_obj_t* g = scrollPage(186);
  auto tile = [&](int k, const char* title) {
    lv_obj_t* c = card(g, (k % 3) * 258, (k / 3) * 100, 250, 90);
    cardTitle(c, title);
    return c;
  };
  auto who = [&](lv_obj_t* c, const W& x, const char* sub) {
    lv_obj_t* av = ui::box(c, 12, 34, 44, 44);
    avatar(av, x.code, x.name, 44);
    lv_obj_t* n = ui::text(c, x.name, 800, 17, ui::color::TEXT);
    lv_obj_set_pos(n, 64, 34);
    lv_obj_t* s2 = ui::text(c, sub, 400, 14, ui::color::TEXT2);
    lv_obj_set_pos(s2, 64, 56);
  };
  char b[48];
  lv_obj_t* c = tile(0, digestSpan == DIGEST_ALL ? "JUGADOR DESTACADO" : digestSpan == DIGEST_MONTH ? "JUGADOR DEL MES" : "JUGADOR DE LA SEMANA");
  if (pop >= 0 && w[pop].wins > 0) {
    snprintf(b, sizeof(b), "%d victorias en %d", w[pop].wins, w[pop].played);
    who(c, w[pop], b);
  } else {
    lv_obj_t* text = ui::text(c, nw ? "Sin victorias registradas" : "Sin jugadores elegidos", 400, 14, ui::color::TEXT2);
    lv_obj_set_pos(text, 12, 40);
  }
  c = tile(1, "MÁS ACTIVO");
  if (act >= 0) {
    snprintf(b, sizeof(b), "%d partidos", w[act].played);
    who(c, w[act], b);
  } else {
    lv_obj_t* text = ui::text(c, "Sin jugadores elegidos", 400, 14, ui::color::TEXT2);
    lv_obj_set_pos(text, 12, 40);
  }
  c = tile(2, "PARTIDOS · GOLES");
  snprintf(b, sizeof(b), "%d · %d", nl, goals);
  lv_obj_t* t = ui::text(c, b, 900, 20, ui::color::TEXT);
  lv_obj_set_pos(t, 12, 36);
  c = tile(3, "PARTIDO MÁS IGUALADO");
  if (closest >= 0) matchLine(c, partidos::rows[closest], 10, 38, 230);
  c = tile(4, "MAYOR PALIZA");
  if (biggest >= 0 && partidos::rows[biggest].w != partidos::rows[biggest].b) matchLine(c, partidos::rows[biggest], 10, 38, 230);
  c = tile(5, "PICHICHI");
  if (sc >= 0) {
    snprintf(b, sizeof(b), "%d goles", w[sc].goals);
    who(c, w[sc], b);
  } else {
    lv_obj_t* f = ui::text(c, nw ? "Faltan goles por apuntar" : "Sin jugadores elegidos", 400, 14, ui::color::TEXT2);
    lv_obj_set_pos(f, 12, 40);
  }
}

// ---------------------------------------------------------------- PAREJAS
static void buildPairs() {
  struct P {
    uint32_t a, b;
    char na[20], nb[20];
    int played, wins, draws, losses;
  };
  static P ps[128];
  int n = 0;
  int* ord = (int*)malloc((size_t)(partidos::count ? partidos::count : 1) * sizeof(int));
  if (!ord) { ui::text(host, "Memoria insuficiente para los detalles", 700, 16, ui::color::TEXT2); return; }
  for (int i = 0; i < partidos::count; i++) ord[i] = i;
  qsort(ord, partidos::count, sizeof(int), progresion::byFinish);
  for (int oi = 0; oi < partidos::count; oi++) {
    const partidos::Row& r = partidos::rows[ord[oi]];
    for (int t = 0; t < 2; t++) {
      if (r.n[t] != 2) continue;
      int x = r.code[t][0] < r.code[t][1] ? 0 : 1;
      uint32_t a = r.code[t][x], b2 = r.code[t][1 - x];
      int j = 0;
      while (j < n && !(ps[j].a == a && ps[j].b == b2)) j++;
      if (j == n) {
        if (n >= 128) continue;
        ps[n] = P{a, b2, "", "", 0, 0, 0, 0};
        n++;
      }
      snprintf(ps[j].na, sizeof(ps[0].na), "%s", r.name[t][x]);
      snprintf(ps[j].nb, sizeof(ps[0].nb), "%s", r.name[t][1 - x]);
      ps[j].played++;
      if (r.winner == '-') ps[j].draws++;
      else if ((t == 0) == (r.winner == 'w')) ps[j].wins++;
      else ps[j].losses++;
    }
  }
  free(ord);
  if (!n) {
    empty("Sin parejas todavía", "Juega partidos de 2 vs 2 para ver qué parejas funcionan mejor.", 250);
    return;
  }
  // Por % de victorias y, si empatan, por partidos (orden estable, como la app).
  for (int a = 1; a < n; a++)
    for (int b = a; b > 0; b--) {
      double pa = ps[b].wins * 100.0 / ps[b].played, pb = ps[b - 1].wins * 100.0 / ps[b - 1].played;
      if (!(pa > pb || (pa == pb && ps[b].played > ps[b - 1].played))) break;
      P t = ps[b];
      ps[b] = ps[b - 1];
      ps[b - 1] = t;
    }
  lv_obj_t* g = scrollPage(132);
  static const uint32_t POS[3] = {ui::color::RANKED, 0xc9d3e0, 0xc98a54};
  for (int i = 0; i < n; i++) {
    lv_obj_t* row = card(g, 0, i * 56, 758, 50);
    lv_obj_set_style_radius(row, ui::RADIUS_SM, 0);
    char t[32];
    snprintf(t, sizeof(t), "%d", i + 1);
    lv_obj_t* pos = ui::text(row, t, 900, 20, i < 3 ? POS[i] : ui::color::TEXT);
    lv_obj_align(pos, LV_ALIGN_LEFT_MID, 18, 0);
    lv_obj_t* who = ui::box(row, 48, 0, 420, 50);
    lv_obj_set_flex_flow(who, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(who, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(who, 8, 0);
    avatar(who, ps[i].a, ps[i].na, 32);
    ui::text(who, ps[i].na, 800, 17, ui::color::TEXT);
    ui::text(who, "+", 800, 14, ui::color::TEXT3);
    avatar(who, ps[i].b, ps[i].nb, 32);
    ui::text(who, ps[i].nb, 800, 17, ui::color::TEXT);
    snprintf(t, sizeof(t), "%d PJ", ps[i].played);
    lv_obj_t* pj = ui::text(row, t, 400, 16, ui::color::TEXT2);
    lv_obj_align(pj, LV_ALIGN_RIGHT_MID, -196, 0);
    if (ps[i].draws) snprintf(t, sizeof(t), "%dG · %dE · %dP", ps[i].wins, ps[i].draws, ps[i].losses);
    else snprintf(t, sizeof(t), "%dG · %dP", ps[i].wins, ps[i].losses);
    lv_obj_t* rec = ui::text(row, t, 400, 16, ui::color::TEXT);
    lv_obj_align(rec, LV_ALIGN_RIGHT_MID, -86, 0);
    snprintf(t, sizeof(t), "%.0f %%", ps[i].wins * 100.0 / ps[i].played);
    lv_obj_t* pc = ui::text(row, t, 900, 17, ui::color::RANKED);
    lv_obj_align(pc, LV_ALIGN_RIGHT_MID, -12, 0);
  }
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
    // Rango, ELO y nivel de ahora (TeamFrame de la app).
    const progresion::Player* pr = progresion::of(r.code[t][i]);
    if (pr && ch >= 60) {
      lv_obj_align(nl, LV_ALIGN_TOP_LEFT, ps + 16, ch / 2 - 30);
      lv_obj_t* cat = ui::text(c, progresion::CATEGORY[pr->category], 800, 12, progresion::CATEGORY_COLOR[pr->category], 1.2f);
      lv_obj_align(cat, LV_ALIGN_TOP_LEFT, ps + 16, ch / 2 - 4);
      char el[32];
      snprintf(el, sizeof(el), "ELO %d · Nv %d", pr->elo, pr->level);
      lv_obj_t* l2 = ui::text(c, el, 700, 12, ui::color::TEXT2);
      lv_obj_align(l2, LV_ALIGN_TOP_LEFT, ps + 16, ch / 2 + 14);
    } else {
      lv_obj_align(nl, LV_ALIGN_LEFT_MID, ps + 16, 0);
    }
  }
}

// Cronología de goles (GoalTimeline de la app): línea dorada en medio, los del Blanco a la izquierda y los del
// Azul a la derecha, cada uno en su burbuja («1ª parte · 00:47»); los anulados, en rojo.
static void drawTimeline(lv_event_t* e) {
  lv_obj_t* o = lv_event_get_current_target_obj(e);
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  const lv_font_t* f = assets::font(700, 12);
  lv_draw_rect_dsc_t line;
  lv_draw_rect_dsc_init(&line);
  line.bg_color = ui::hex(ui::color::RANKED);
  line.bg_opa = 160;
  lv_area_t la = {a.x1 + 131, a.y1, a.x1 + 132, a.y2};
  lv_draw_rect(layer, &line, &la);
  static const char* PER[3] = {"1ª parte", "2ª parte", "Prórroga"};
  for (uint8_t i = 0; i < detail->goals; i++) {
    int y = a.y1 + 6 + i * 34;
    if (y > layer->_clip_area.y2 || y + 26 < layer->_clip_area.y1) continue;  // fuera de lo que se pinta
    const partidos::GoalLine& g = detail->goal[i];
    char tm[8], txt[40];
    mmss(g.ms, tm, sizeof(tm));
    const char* pn = g.period == 0 && detail->singlePeriod ? "Partido" : PER[g.period % 3];
    if (g.off) snprintf(txt, sizeof(txt), "ANULADO");
    else if (g.value > 1) snprintf(txt, sizeof(txt), "%s · %s  x%u", pn, tm, g.value);
    else snprintf(txt, sizeof(txt), "%s · %s", pn, tm);
    lv_point_t sz;
    lv_text_get_size(&sz, txt, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int w = sz.x + 16;
    int x = g.team ? a.x1 + 140 : a.x1 + 124 - w;
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.radius = 13;
    r.bg_color = ui::hex(0x0e1d33);
    r.bg_opa = LV_OPA_COVER;
    r.border_width = 1;
    r.border_color = ui::hex(g.off ? ui::color::DANGER : g.team ? 0x4fb0ff : ui::color::WHITE_TEAM);
    lv_area_t ra = {x, y, x + w - 1, y + 25};
    lv_draw_rect(layer, &r, &ra);
    lv_draw_label_dsc_t l;
    lv_draw_label_dsc_init(&l);
    l.font = f;
    l.color = ui::hex(g.off ? ui::color::DANGER : ui::color::TEXT);
    l.text = txt;
    l.text_local = 1;
    lv_area_t ta = {x + 8, y + (26 - sz.y) / 2, x + w - 8, y + 25};
    lv_draw_label(layer, &l, &ta);
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE;
    d.bg_color = ui::hex(ui::color::RANKED);
    d.bg_opa = LV_OPA_COVER;
    lv_area_t da = {a.x1 + 128, y + 9, a.x1 + 135, y + 16};
    lv_draw_rect(layer, &d, &da);
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
    // Una sola pieza que se dibuja entera (con tres objetos por gol, un partido con 70 goles gastaba memoria
    // interna y tardaba casi un segundo en abrirse).
    lv_obj_t* tl = ui::box(mid, 0, 0, 262, 18 + detail->goals * 34);
    lv_obj_add_event_cb(tl, drawTimeline, LV_EVENT_DRAW_MAIN, nullptr);
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
  host = nullptr;
  memset(tabPill, 0, sizeof(tabPill));
  memset(rowObj, 0, sizeof(rowObj));
}

static bool inDetail = false;
static void go(bool d) {
  inDetail = d && openRow();
  lv_obj_t* old = scr;
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  whoPanel = confirmBox = nullptr;
  host = nullptr;
  memset(tabPill, 0, sizeof(tabPill));
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

// Al entrar desde Inicio se actualizan los datos y se conserva la pestaña, filtros y página en RAM.
// `fresh` = falso al volver del reposo: también se conserva el detalle abierto.
static void show(jug::Store* s, bool fresh) {
  store = s;
  if (fresh) {
    // Cálculos de la app con todo el historial y con la temporada de ahora.
    uint32_t codes[jug::MAX];
    for (int k = 0; k < s->count; k++) codes[k] = s->list[k].code;
    progresion::refresh(codes, s->count);
    progresion::refreshSeason(codes, s->count, hooks.nowUtc ? hooks.nowUtc() : 0);
    if (whoFilter && !store->find(whoFilter)) { whoFilter = 0; page = 0; }
    inDetail = false;  // regresar a la lista; no reabrir un partido antiguo por sorpresa
  }
  go(inDetail);
}

}  // namespace historial

#endif  // MFV3_UI_HISTORIAL_H
