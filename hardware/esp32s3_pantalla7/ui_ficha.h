// MARCADOR FUTBOLÍN V3 · FICHA de un jugador (Perfil de la app).
// -------------------------------------------------------------------------------------------------
// Copia de ProfileScreen.tsx:
//   - Arriba, volver y PERFIL. La tarjeta del jugador: foto con su nivel, nombre, título, experiencia (barra hasta el
//     siguiente nivel) y su rango con el ELO y el máximo.
//   - Pestañas GENERAL, HÁBITOS, RIVALES, HISTORIAL, TORNEOS y LOGROS n/51; debajo, lo de cada una (se desliza si no
//     cabe):
//       GENERAL: TODOS / CLASIFICATORIOS con sus números, rachas, tiempo, goles apuntados (con «?»), los últimos
//                resultados y la evolución del ELO.
//       HÁBITOS: lado de la mesa, franja horaria y días de la semana.
//       RIVALES: rival favorito, némesis, enfrentamientos directos y compañeros.
//       HISTORIAL: sus últimos 50 partidos (tocar uno abre su detalle, el del RANKING).
//       TORNEOS: los torneos en que ha jugado, con filtros por tipo y «ganados».
//       LOGROS: todos, por apartados; los secretos se revelan al conseguirlos.
// Diferencias con la app: sin «Opciones» (el QR está en AJUSTES), sin cara a cara (la placa no lo tiene) y los
// escudos, copas, rangos e iconos de logro van dibujados (una actualización por Wi-Fi no lleva imágenes nuevas).
// Las cuentas son las del QR del perfil (perfil::stats, igual que la app) y la progresión (s3_progresion.h).
// Solo letras de fuentes.txt. Se incluye después de ui_historial.h, ui_torneo.h y s3_perfil.h.

#ifndef MFV3_UI_FICHA_H
#define MFV3_UI_FICHA_H

#include "ui_base.h"
#include "s3_jugadores.h"
#include "s3_partidos.h"
#include "s3_progresion.h"

namespace ficha {

struct Hooks {
  void (*back)() = nullptr;                    // volver a donde se abrió
  void (*openMatch)(const char* id) = nullptr;  // HISTORIAL: el detalle de un partido
};
static Hooks hooks;

enum Tab : uint8_t { GENERAL, HABITS, RIVALS, HISTORY, TOURNAMENTS, ACHIEVEMENTS, TAB_COUNT };

static jug::Store* store = nullptr;
static uint32_t code = 0;          // jugador abierto
static uint8_t tab = GENERAL;
static bool rankedScope = false;   // GENERAL: TODOS (false) o CLASIFICATORIOS
static uint8_t tourFilter = 0;     // TORNEOS: 0 todos, 1 ligas, 2 copas, 3 pools, 4 ganados
static lv_obj_t* scr = nullptr;
static lv_obj_t* host = nullptr;   // lo de debajo de las pestañas (se rehace al cambiar de pestaña)
static lv_obj_t* tabPill[TAB_COUNT];
static lv_obj_t* help = nullptr;
static perfil::Stats st;
static bool haveStats = false;
static uint32_t statsRevision = 0;  // partidos::revision de las cuentas (si se borra un partido, se rehacen)

static const uint32_t RARITY_COLOR[3] = {ui::color::ACCENT, 0x8ab8ff, ui::color::CHAOS};

// ---------------------------------------------------------------- piezas
// Mayúsculas (también las vocales con tilde y la ñ), como el text-transform de la app.
static void upper(const char* s, char* out, size_t n) {
  size_t k = 0;
  for (const char* q = s; *q && k + 2 < n; q++) {
    char ch = *q;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    else if ((uint8_t)ch == 0xC3 && (uint8_t)q[1] >= 0xA0 && (uint8_t)q[1] <= 0xBE) {
      out[k++] = ch;
      q++;
      ch = (char)((uint8_t)*q - 0x20);
    }
    out[k++] = ch;
  }
  out[k] = 0;
}

static lv_obj_t* panel(lv_obj_t* parent, int x, int y, int w, int h, uint32_t border = ui::color::LINE, int radius = ui::RADIUS_SM) {
  lv_obj_t* b = ui::box(parent, x, y, w, h);
  lv_obj_set_style_radius(b, radius, 0);
  lv_obj_set_style_bg_color(b, ui::hex(ui::color::CARD), 0);
  lv_obj_set_style_bg_opa(b, 191, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, ui::hex(border), 0);
  if (border == ui::color::LINE) lv_obj_set_style_border_opa(b, 179, 0);
  return b;
}
// Tarjeta (.card de la app): degradado suave y borde.
static lv_obj_t* card(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* b = ui::box(parent, x, y, w, h);
  lv_obj_set_style_radius(b, ui::RADIUS, 0);
  lv_obj_set_style_bg_color(b, ui::hex(ui::color::CARD2), 0);
  lv_obj_set_style_bg_grad_color(b, ui::hex(ui::color::CARD), 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, ui::hex(ui::color::LINE), 0);
  return b;
}
static lv_obj_t* label(lv_obj_t* parent, const char* txt, int x, int y) {  // .label: mayúsculas pequeñas
  char up[64];
  upper(txt, up, sizeof(up));
  lv_obj_t* l = ui::text(parent, up, 700, 12, ui::color::TEXT2, 1.44f);
  lv_obj_set_pos(l, x, y);
  return l;
}
static lv_obj_t* textAt(lv_obj_t* parent, const char* txt, uint16_t w, uint16_t s, uint32_t c, int x, int y, int width = 0,
                        lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t* l = ui::text(parent, txt, w, s, c);
  if (width) {
    lv_obj_set_width(l, width);
    lv_obj_set_style_text_align(l, align, 0);
  }
  lv_obj_set_pos(l, x, y);
  return l;
}

// Foto (o iniciales) redonda.
static void avatar(lv_obj_t* parent, uint32_t c, const char* name, int x, int y, int size, uint32_t ring = ui::color::LINE) {
  char rel[40];
  const lv_image_dsc_t* pic = c && store->photoRel(c, rel, sizeof(rel)) ? assets::photo(rel, size, size / 2) : nullptr;
  lv_obj_t* b = ui::box(parent, x, y, size, size);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, ui::hex(0x1d3a63), 0);
  lv_obj_set_style_bg_grad_color(b, ui::hex(0x0f2140), 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, ui::hex(ring), 0);
  if (pic) {
    lv_obj_t* img = lv_image_create(b);
    lv_image_set_src(img, pic);
    lv_obj_center(img);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return;
  }
  char ini[8];
  jug::initials(name, ini, sizeof(ini));
  lv_obj_t* l = ui::text(b, ini, 800, size >= 44 ? 18 : 12, ui::color::TEXT);
  lv_obj_center(l);
}
// Tamaño de foto que la placa tiene preparado (historial::prewarm): 24, 32, 44 o 64; 72 (el de la ficha) también.
static void avatarOf(lv_obj_t* parent, uint32_t c, const char* name, int x, int y, int size) {
  const jug::Player* p = c ? store->find(c) : nullptr;
  avatar(parent, p ? c : 0, p ? p->name : name, x, y, size);
}

// Rango (CategoryBadge de la app, dibujado como en el RANKING): círculo del color de su categoría con la inicial.
static void rankBadge(lv_obj_t* parent, uint8_t cat, int x, int y, int size) {
  uint32_t c = progresion::CATEGORY_COLOR[cat % 7];
  lv_obj_t* b = ui::box(parent, x, y, size, size);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, ui::hex(c), 0);
  lv_obj_set_style_bg_opa(b, 70, 0);
  lv_obj_set_style_border_width(b, 3, 0);
  lv_obj_set_style_border_color(b, ui::hex(c), 0);
  lv_obj_set_style_shadow_width(b, 14, 0);
  lv_obj_set_style_shadow_color(b, ui::hex(c), 0);
  lv_obj_set_style_shadow_opa(b, 90, 0);
  char l[4];
  snprintf(l, sizeof(l), "%.1s", progresion::CATEGORY[cat % 7]);
  lv_obj_t* t = ui::text(b, l, 900, 30, c);
  lv_obj_center(t);
}

// Casilla G / E / P (.form-chip).
static void chip(lv_obj_t* parent, char r, int x, int y) {
  uint32_t c = r == 'G' ? ui::color::OK : r == 'P' ? ui::color::DANGER : ui::color::TEXT2;
  lv_obj_t* b = ui::box(parent, x, y, 22, 22);
  lv_obj_set_style_radius(b, 6, 0);
  lv_obj_set_style_bg_color(b, ui::hex(c), 0);
  lv_obj_set_style_bg_opa(b, 45, 0);
  char t[2] = {r, 0};
  lv_obj_t* l = ui::text(b, t, 900, 13, c);
  lv_obj_center(l);
}

static void pctText(double v, bool has, char* out, size_t n) {
  if (has) snprintf(out, n, "%.0f %%", v);
  else snprintf(out, n, "—");
}

// ---------------------------------------------------------------- ayuda («?»)
static void onHelpClose(lv_event_t*) {
  if (help) lv_obj_delete(help);
  help = nullptr;
}
static void onHelp(lv_event_t* e) {
  if (help) return;
  const char* const* t = (const char* const*)lv_event_get_user_data(e);
  help = ui::box(scr, 0, 0, 800, 480);
  lv_obj_add_flag(help, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(help, ui::hex(0x02050a), 0);
  lv_obj_set_style_bg_opa(help, 190, 0);
  lv_obj_add_event_cb(help, onHelpClose, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* c = ui::box(help, 90, 90, 620, 300);
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);  // tocar dentro no cierra
  lv_obj_set_style_radius(c, ui::RADIUS, 0);
  lv_obj_set_style_bg_color(c, ui::hex(ui::color::SURFACE), 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c, 2, 0);
  lv_obj_set_style_border_color(c, ui::hex(ui::color::ACCENT), 0);
  textAt(c, t[0], 900, 24, ui::color::ACCENT, 24, 22);
  lv_obj_t* p = textAt(c, t[1], 400, 22, ui::color::TEXT, 24, 62, 572);
  lv_label_set_long_mode(p, LV_LABEL_LONG_MODE_WRAP);
  ui::button(c, 180, 216, 260, 64, "ENTENDIDO", 20, ui::BtnKind::Primary, onHelpClose);
}
static void helpDot(lv_obj_t* parent, int x, int y, const char* const* text) {
  lv_obj_t* b = ui::box(parent, x, y, 30, 30);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(b, 10);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, ui::hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(b, 15, 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, ui::hex(0x8e98a6), 0);
  lv_obj_t* q = ui::text(b, "?", 900, 15, ui::color::TEXT);
  lv_obj_center(q);
  lv_obj_add_event_cb(b, onHelp, LV_EVENT_CLICKED, (void*)text);
}

// ---------------------------------------------------------------- GENERAL
static void buildContent();
static void onScope(lv_event_t* e) {
  bool r = (bool)(intptr_t)lv_event_get_user_data(e);
  if (r == rankedScope) return;
  rankedScope = r;
  buildContent();
}
static lv_obj_t* seg(lv_obj_t* parent, int x, int y, int w, int h, const char* txt, bool on, lv_event_cb_t cb, void* user,
                     uint16_t size = 17, int radius = ui::RADIUS_SM) {
  lv_obj_t* b = ui::box(parent, x, y, w, h);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(b, radius, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, ui::hex(on ? 0x123a55 : ui::color::CARD), 0);
  lv_obj_set_style_bg_color(b, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, ui::hex(on ? ui::color::ACCENT : ui::color::LINE), 0);
  if (on) {
    lv_obj_set_style_shadow_width(b, 12, 0);
    lv_obj_set_style_shadow_color(b, ui::hex(ui::color::ACCENT), 0);
    lv_obj_set_style_shadow_opa(b, 70, 0);
  }
  lv_obj_t* l = ui::text(b, txt, 800, size, ui::color::TEXT, 0.8f);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}
static lv_obj_t* tile(lv_obj_t* parent, int x, int y, int h, const char* v, const char* k) {
  lv_obj_t* t = panel(parent, x, y, 186, h);
  textAt(t, v, 800, 23, ui::color::TEXT, 10, 7);
  char up[48];
  upper(k, up, sizeof(up));
  lv_obj_t* l = ui::text(t, up, 700, 12, ui::color::TEXT2, 1.2f);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_width(l, 164);
  lv_obj_set_pos(l, 10, 34);
  return t;
}

static const char* const GOALS_HELP[2] = {
    "Goles apuntados",
    "Los goles que se apuntaron a su nombre. En 1 vs 1 se apuntan solos; en 2 vs 2, solo si alguien toca su ficha al marcar."};

// Evolución del ELO (EloChart de la app): desde el ELO inicial, el de después de cada partido que lo movió.
static void eloChart(lv_obj_t* parent, int y) {
  lv_obj_t* c = card(parent, 0, y, 768, 186);
  label(c, "Evolución del ELO", 13, 12);
  // Los ELO, en orden de partidos (la entrada de la progresión de cada fila donde juega).
  const progresion::Snapshot& all = progresion::all;
  int n = 0;
  int cap = st.nmine + 1;
  int16_t* elo = (int16_t*)malloc(sizeof(int16_t) * (size_t)cap);
  if (!elo) return;
  elo[n++] = (int16_t)progresion::rules.eloInitial;
  for (int k = 0; k < st.nmine && all.entries; k++) {
    int ri = st.mine[k];
    if (ri >= all.rowsCap) continue;
    const partidos::Row& m = partidos::rows[ri];
    for (int t = 0; t < 2; t++)
      for (int i = 0; i < m.n[t]; i++)
        if (m.code[t][i] == code) {
          const progresion::Entry& e = all.entries[(size_t)ri * 8 + t * 4 + i];
          if (e.eloDelta != 0x7FFF) elo[n++] = e.eloAfter;
        }
  }
  if (n < 2) {
    textAt(c, "Sin clasificatorios todavía.", 400, 13, ui::color::TEXT3, 13, 36);
    free(elo);
    return;
  }
  int lo = elo[0], hi = elo[0];
  for (int i = 0; i < n; i++) lo = elo[i] < lo ? elo[i] : lo, hi = elo[i] > hi ? elo[i] : hi;
  lo -= 10, hi += 10;
  const int W = 740, H = 136, X0 = 14, Y0 = 36;
  // Como mucho 120 puntos (con muchos partidos, uno de cada varios; el último siempre).
  int step = (n + 119) / 120;
  int np = 0;
  static lv_point_precise_t pts[122];
  for (int i = 0; i < n; i += step) {
    pts[np].x = 8 + (lv_value_precise_t)i * (W - 16) / (n - 1);
    pts[np].y = H - 10 - (lv_value_precise_t)(elo[i] - lo) * (H - 20) / (hi - lo);
    np++;
  }
  if ((n - 1) % step) {
    pts[np].x = W - 8;
    pts[np].y = H - 10 - (lv_value_precise_t)(elo[n - 1] - lo) * (H - 20) / (hi - lo);
    np++;
  }
  lv_obj_t* area = ui::box(c, X0, Y0, W, H);
  // Línea del ELO inicial (discontinua) y su número.
  int yi = H - 10 - (elo[0] - lo) * (H - 20) / (hi - lo);
  for (int x = 8; x < W - 8; x += 8) {
    lv_obj_t* d = ui::box(area, x, yi, 4, 1);
    lv_obj_set_style_bg_color(d, ui::hex(ui::color::LINE), 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  }
  char t[12];
  snprintf(t, sizeof(t), "%d", elo[0]);
  textAt(area, t, 400, 11, ui::color::TEXT3, W - 8 - 60, yi - 16, 60, LV_TEXT_ALIGN_RIGHT);
  lv_obj_t* line = lv_line_create(area);
  lv_line_set_points_mutable(line, pts, (uint32_t)np);
  lv_obj_set_style_line_width(line, 2, 0);
  lv_obj_set_style_line_color(line, ui::hex(ui::color::ACCENT), 0);
  lv_obj_set_style_line_rounded(line, true, 0);
  int lx = (int)pts[np - 1].x, ly = (int)pts[np - 1].y;
  lv_obj_t* dot = ui::box(area, lx - 6, ly - 6, 12, 12);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, ui::hex(ui::color::ACCENT), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dot, 2, 0);
  lv_obj_set_style_border_color(dot, ui::hex(ui::color::SURFACE), 0);
  snprintf(t, sizeof(t), "%d", elo[n - 1]);
  textAt(area, t, 700, 14, ui::color::TEXT, lx - 6 - 60, ly - 26 < 0 ? ly + 6 : ly - 26, 60, LV_TEXT_ALIGN_RIGHT);
  free(elo);
}

static void buildGeneral() {
  seg(host, 0, 0, 381, 48, "TODOS", !rankedScope, onScope, (void*)(intptr_t)0);
  seg(host, 387, 0, 381, 48, "CLASIFICATORIOS", rankedScope, onScope, (void*)(intptr_t)1);
  const perfil::Block& b = rankedScope ? st.ranked : st.general;
  char v[8][24];
  snprintf(v[0], 24, "%d", b.played);
  snprintf(v[1], 24, "%d", b.wins);
  snprintf(v[2], 24, "%d", b.losses);
  pctText(b.played ? (double)b.wins / b.played * 100 : 0, b.played > 0, v[3], 24);
  snprintf(v[4], 24, "%d", b.gf);
  snprintf(v[5], 24, "%d", b.ga);
  if (b.gf - b.ga > 0) snprintf(v[6], 24, "+%d", b.gf - b.ga);
  else snprintf(v[6], 24, "%d", b.gf - b.ga);
  snprintf(v[7], 24, "%d/%d", b.pgf, b.pga);
  static const char* const K[8] = {"Partidos", "Victorias", "Derrotas", "% victorias", "Goles equipo a favor", "Goles equipo en contra",
                                   "Diferencia", "Penaltis a favor/contra"};
  for (int i = 0; i < 8; i++) tile(host, (i % 4) * 194, i < 4 ? 56 : 123, i < 4 ? 59 : 74, v[i], K[i]);
  int y = 205;
  if (!rankedScope) {
    char s[4][32];
    if (st.sType) snprintf(s[0], 32, "%d %s%s", st.sCount, st.sType == 'G' ? "victoria" : "derrota", st.sCount == 1 ? "" : "s");
    else snprintf(s[0], 32, "—");
    snprintf(s[1], 32, "%d", st.best);
    uint64_t secs = st.playMs / 1000;
    snprintf(s[2], 32, "%02llu:%02llu", (unsigned long long)(secs / 60), (unsigned long long)(secs % 60));
    snprintf(s[3], 32, "%d", st.goals);
    static const char* const K2[4] = {"Racha actual", "Mejor racha", "Tiempo jugado", "Goles apuntados"};
    for (int i = 0; i < 4; i++) {
      lv_obj_t* t = tile(host, i * 194, y, 74, s[i], K2[i]);
      if (i == 3) helpDot(t, 146, 36, GOALS_HELP);
    }
    y += 82;
  }
  // Últimos resultados (todos) o últimos clasificatorios (rform de la progresión).
  lv_obj_t* l = textAt(host, rankedScope ? "Últimos clasificatorios:" : "Últimos partidos:", 400, 13, ui::color::TEXT2, 0, y + 3);
  lv_obj_update_layout(l);
  int x = lv_obj_get_width(l) + 6;
  const progresion::Player* pr = progresion::of(code);
  const char* form = rankedScope ? (pr ? pr->rform : "") : nullptr;
  int nf = rankedScope ? (int)strlen(form) : st.nl;
  if (!nf) textAt(host, rankedScope ? "Sin clasificatorios" : "—", 400, 13, ui::color::TEXT3, x, y + 3);
  for (int i = 0; i < nf; i++) chip(host, rankedScope ? form[i] : st.letters[i], x + i * 25, y);
  y += 32;
  if (pr) eloChart(host, y);
}

// ---------------------------------------------------------------- HÁBITOS
static void bar(lv_obj_t* parent, int y, const char* name, const perfil::Rec& r) {
  textAt(parent, name, 400, 13, ui::color::TEXT, 13, y, 96);
  lv_obj_t* tr = ui::box(parent, 117, y + 4, 137, 8);
  lv_obj_set_style_radius(tr, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(tr, ui::hex(ui::color::LINE), 0);
  lv_obj_set_style_bg_opa(tr, LV_OPA_COVER, 0);
  lv_obj_set_style_clip_corner(tr, true, 0);
  int w = r.played ? (int)lround(137.0 * r.wins / r.played) : 0;
  if (w > 0) {
    lv_obj_t* f = ui::box(tr, 0, 0, w, 8);
    lv_obj_set_style_bg_color(f, ui::hex(ui::color::ACCENT), 0);
    lv_obj_set_style_bg_grad_color(f, ui::hex(ui::color::OK), 0);
    lv_obj_set_style_bg_grad_dir(f, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
  }
  char v[32];
  if (r.played) snprintf(v, sizeof(v), "%.0f %% · %d PJ", (double)r.wins / r.played * 100, r.played);
  else snprintf(v, sizeof(v), "—");
  textAt(parent, v, 400, 13, ui::color::TEXT2, 262, y, 104, LV_TEXT_ALIGN_RIGHT);
}
static void buildHabits() {
  static const char* const PART[4] = {"Mañana", "Mediodía", "Tarde", "Noche"};
  static const char* const DAY[7] = {"Dom", "Lun", "Mar", "Mié", "Jue", "Vie", "Sáb"};
  lv_obj_t* a = card(host, 0, 0, 379, 212);
  label(a, "Lado de la mesa", 13, 13);
  bar(a, 33, "Blanco (izq.)", st.sides[0]);
  bar(a, 54, "Azul (dcha.)", st.sides[1]);
  label(a, "Franja horaria", 13, 80);
  int best = -1;
  for (int j = 0; j < 4; j++) {
    bar(a, 100 + j * 21, PART[j], st.parts[j]);
    const perfil::Rec& r = st.parts[j];
    if (r.played >= 3 && (best < 0 || (double)r.wins / r.played > (double)st.parts[best].wins / st.parts[best].played)) best = j;
  }
  char t[64];
  if (best >= 0) snprintf(t, sizeof(t), "Rinde mejor por la %s.", perfil::DAYPART[best]);
  else snprintf(t, sizeof(t), "Mejor franja: se necesitan 3 partidos en alguna.");
  textAt(a, t, 400, 13, ui::color::TEXT3, 13, 183, 353);
  lv_obj_t* b = card(host, 389, 0, 379, 212);
  label(b, "Días de la semana", 13, 13);
  int fav = -1;
  for (int j = 0; j < 7; j++) {
    int d = (j + 1) % 7;  // lunes primero
    bar(b, 33 + j * 21, DAY[d], st.days[d]);
    if (fav < 0 || st.days[d].played > st.days[fav].played) fav = d;
  }
  if (fav >= 0 && st.days[fav].played) snprintf(t, sizeof(t), "Día favorito: %s.", DAY[fav]);
  else snprintf(t, sizeof(t), "Sin partidos aún.");
  textAt(b, t, 400, 13, ui::color::TEXT3, 13, 183, 353);
}

// ---------------------------------------------------------------- RIVALES
static void personCard(int x, const char* title, int idx, bool fav) {
  lv_obj_t* c = card(host, x, 0, 379, 91);
  label(c, title, 13, 13);
  char t[96];
  if (idx < 0) {
    snprintf(t, sizeof(t), "Aún sin %s (hacen falta %d partidos contra alguien)", fav ? "rival favorito" : "némesis", perfil::RIVAL_MIN);
    lv_obj_t* l = textAt(c, t, 800, 17, ui::color::TEXT, 13, 32, 353);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    return;
  }
  const perfil::Other& o = st.rivals[idx];
  avatarOf(c, o.code, o.name, 13, 29, 44);
  lv_obj_t* n = textAt(c, o.name, 800, 18, ui::color::TEXT, 69, 33, 290);
  lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
  if (fav) snprintf(t, sizeof(t), "le ganas el %.0f %%", (double)o.r.wins / o.r.played * 100);
  else snprintf(t, sizeof(t), "te gana el %ld %%", lround((double)o.r.losses / o.r.played * 100));
  textAt(c, t, 800, 14, ui::color::TEXT2, 69, 55);
}
static int otherRows(int y, const perfil::Other* list, int n) {
  for (int i = 0; i < n; i++, y += 58) {
    const perfil::Other& o = list[i];
    lv_obj_t* r = panel(host, 0, y, 768, 52);
    avatarOf(r, o.code, o.name, 11, 10, 32);
    lv_obj_t* nm = textAt(r, o.name, 700, 15, ui::color::TEXT, 51, 15, 360);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_MODE_DOTS);
    char t[40];
    snprintf(t, sizeof(t), "%d PJ", o.r.played);
    textAt(r, t, 400, 16, ui::color::TEXT2, 470, 14, 100, LV_TEXT_ALIGN_RIGHT);
    if (o.r.draws) snprintf(t, sizeof(t), "%dG · %dE · %dP", o.r.wins, o.r.draws, o.r.losses);
    else snprintf(t, sizeof(t), "%dG · %dP", o.r.wins, o.r.losses);
    textAt(r, t, 400, 16, ui::color::TEXT, 578, 14, 100, LV_TEXT_ALIGN_RIGHT);
    snprintf(t, sizeof(t), "%.0f %%", o.r.played ? (double)o.r.wins / o.r.played * 100 : 0.0);
    textAt(r, t, 700, 15, ui::color::TEXT, 686, 14, 66, LV_TEXT_ALIGN_RIGHT);
  }
  return y;
}
static void buildRivals() {
  personCard(0, "Rival favorito", st.fav, true);
  personCard(389, "Némesis", st.nem, false);
  label(host, "Enfrentamientos directos", 0, 99);
  int y = 122;
  if (!st.nr) {
    textAt(host, "Sin enfrentamientos.", 400, 16, ui::color::TEXT2, 0, y);
    y += 30;
  } else {
    y = otherRows(y, st.rivals, st.nr);
  }
  if (st.nt) {
    label(host, "Compañeros (2 vs 2)", 0, y + 2);
    otherRows(y + 25, st.mates, st.nt);
  }
}

// ---------------------------------------------------------------- HISTORIAL
static void onMatch(lv_event_t* e) {
  int ri = (int)(intptr_t)lv_event_get_user_data(e);
  if (ri >= 0 && ri < partidos::count && hooks.openMatch) hooks.openMatch(partidos::rows[ri].id);
}
// Un lado del partido: fotos y nombres («Alex + Diego»), hacia el marcador.
static void side(lv_obj_t* row, const partidos::Row& m, int t, int x, int w, bool right) {
  char names[64] = "";
  for (int i = 0; i < m.n[t]; i++) {
    size_t k = strlen(names);
    snprintf(names + k, sizeof(names) - k, "%s%s", i ? " + " : "", m.name[t][i]);
  }
  int faces = m.n[t] * 27 - 3;
  for (int i = 0; i < m.n[t]; i++) {
    int fx = right ? x + w - faces + i * 27 : x + i * 27;
    avatarOf(row, m.code[t][i], m.name[t][i], fx, 14, 24);
  }
  int tw = w - faces - 6;
  lv_obj_t* l = textAt(row, names, 400, 16, ui::color::TEXT, right ? x : x + faces + 6, 15, tw, right ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
}
static void buildHistory() {
  if (!st.nmine) {
    textAt(host, "Sin partidos guardados.", 400, 16, ui::color::TEXT2, 0, 0);
    return;
  }
  int y = 0;
  for (int k = st.nmine - 1, n = 0; k >= 0 && n < 50; k--, n++, y += 58) {
    int ri = st.mine[k];
    const partidos::Row& m = partidos::rows[ri];
    int team = 0;
    for (int t = 0; t < 2; t++)
      for (int i = 0; i < m.n[t]; i++)
        if (m.code[t][i] == code) team = t;
    char res = m.winner == '-' ? 'E' : (m.winner == 'w') == (team == 0) ? 'G' : 'P';
    lv_obj_t* r = panel(host, 0, y, 768, 52);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
    lv_obj_add_event_cb(r, onMatch, LV_EVENT_CLICKED, (void*)(intptr_t)ri);
    chip(r, res, 11, 14);
    char t[32];
    partidos::dateText(m.fin, t, sizeof(t));
    textAt(r, t, 400, 13, ui::color::TEXT3, 43, 17, 120);
    textAt(r, m.mode == 'c' ? "Partido Loco" : m.mode == 'r' ? "Clasificatorio" : "Partido rápido", 400, 13, ui::color::TEXT2, 173, 17, 110);
    // Blanco a la izquierda, Azul a la derecha, y el marcador en medio (MatchLine de la app).
    snprintf(t, sizeof(t), "%u – %u", m.w, m.b);
    textAt(r, t, 700, 15, ui::color::TEXT, 505, 15, 60, LV_TEXT_ALIGN_CENTER);
    side(r, m, 0, 293, 205, false);
    side(r, m, 1, 572, 185, true);
  }
}

// ---------------------------------------------------------------- TORNEOS
static perfil::Title* titles = nullptr;
static int nTitles = 0;
static void onTourFilter(lv_event_t* e) {
  uint8_t f = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  tourFilter = tourFilter == f && f ? 0 : f;
  buildContent();
}
static void buildTournaments() {
  if (!titles) {
    char id[48];
    store->idOf(code, id, sizeof(id));
    nTitles = perfil::titles(id, titles);
  }
  if (!nTitles) {
    textAt(host, "No ha participado en torneos.", 400, 16, ui::color::TEXT2, 0, 0);
    return;
  }
  int count[3] = {0, 0, 0}, won = 0;
  for (int i = 0; i < nTitles; i++) count[titles[i].format % 3]++, won += titles[i].champion;
  // Filtros: Todos, los tipos que tiene y Ganados.
  static const char* const PLURAL[3] = {"Ligas", "Copas", "Pools"};
  static const uint8_t FMT_OF[3] = {0, 1, 2};  // Format::League, Bracket, Pool
  int x = 0;
  auto filter = [&](const char* name, int n, uint8_t f, bool dis) {
    char t[24];
    snprintf(t, sizeof(t), "%s  %d", name, n);
    lv_obj_t* b = seg(host, x, 0, 200, 46, t, tourFilter == f, onTourFilter, (void*)(intptr_t)f, 17, 12);
    lv_obj_t* l = lv_obj_get_child(b, 0);
    lv_obj_update_layout(l);
    int w = lv_obj_get_width(l) + 34;  // el ancho de su texto, como la app
    lv_obj_set_width(b, w);
    lv_obj_center(l);
    if (dis) lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE), lv_obj_set_style_opa(b, 120, 0);
    x += w + 8;
  };
  filter("Todos", nTitles, 0, false);
  for (int f = 0; f < 3; f++)
    if (count[f]) filter(PLURAL[f], count[f], (uint8_t)(f + 1), false);
  filter("Ganados", won, 4, won == 0);
  static const char* const FMT_LABEL[3] = {"Liga", "Copa", "Pool"};
  static const char* const MONTH[12] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sept", "oct", "nov", "dic"};
  int y = 56, shown = 0;
  for (int i = 0; i < nTitles; i++) {
    const perfil::Title& e = titles[i];
    if (tourFilter >= 1 && tourFilter <= 3 && e.format != FMT_OF[tourFilter - 1]) continue;
    if (tourFilter == 4 && !e.champion) continue;
    bool live = e.status == (uint8_t)torneos::Status::Active;
    lv_obj_t* c = ui::box(host, 0, y, 768, 78);
    lv_obj_set_style_radius(c, ui::RADIUS, 0);
    lv_obj_set_style_bg_color(c, ui::hex(0x07101c), 0);
    lv_obj_set_style_bg_opa(c, 191, 0);
    lv_obj_set_style_border_width(c, e.champion ? 2 : 1, 0);
    lv_obj_set_style_border_color(c, ui::hex(e.champion ? ui::color::RANKED : ui::color::LINE), 0);
    const char* name = e.comp[0] ? e.comp : e.name;
    tor::crest(c, e.logo, name, 15, 12, 54);
    lv_obj_t* n = textAt(c, name, 800, 18, ui::color::TEXT, 83, 16, 500);
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
    char sub[80], when[24];
    if (live) snprintf(when, sizeof(when), "en juego");
    else {
      reloj::Clock k = partidos::localOf((uint32_t)(e.at / 1000));
      snprintf(when, sizeof(when), "%u %s %u", k.day, MONTH[(k.month + 11) % 12], k.year);
    }
    if (e.edition) snprintf(sub, sizeof(sub), "%uª edición · %s · %s", e.edition, FMT_LABEL[e.format % 3], when);
    else snprintf(sub, sizeof(sub), "%s · %s", FMT_LABEL[e.format % 3], when);
    textAt(c, sub, 400, 13, ui::color::TEXT2, 83, 44);
    if (live) {
      lv_obj_t* p = ui::box(c, 605, 27, 88, 24);
      lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_border_width(p, 1, 0);
      lv_obj_set_style_border_color(p, ui::hex(ui::color::ACCENT), 0);
      lv_obj_t* l = ui::text(p, "EN JUEGO", 900, 13, ui::color::ACCENT);
      lv_obj_center(l);
    } else if (e.place) {
      char pl[8];
      snprintf(pl, sizeof(pl), "%uº", e.place);
      textAt(c, pl, 900, 30, ui::color::TEXT, 630, 8, 64, LV_TEXT_ALIGN_CENTER);
      snprintf(pl, sizeof(pl), "de %u", e.of);
      textAt(c, pl, 400, 12, ui::color::TEXT2, 630, 46, 64, LV_TEXT_ALIGN_CENTER);
    }
    lv_obj_t* cp = tor::cup(c, e.cup, e.champion ? 700 : 706, e.champion ? 8 : 15, e.champion ? 58 : 46);
    if (!e.champion && cp) lv_obj_set_style_opa(cp, 153, 0);
    if (e.champion) {
      lv_obj_t* rb = ui::box(c, 12, -1, 84, 18);
      lv_obj_set_style_bg_color(rb, ui::hex(ui::color::RANKED), 0);
      lv_obj_set_style_bg_opa(rb, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(rb, 4, 0);
      lv_obj_t* l = ui::text(rb, "CAMPEÓN", 900, 11, 0x1a1300);
      lv_obj_center(l);
    }
    y += 86;
    shown++;
  }
  if (!shown) textAt(host, "Ningún torneo con este filtro.", 400, 16, ui::color::TEXT2, 0, y);
}

// ---------------------------------------------------------------- LOGROS
static void achCard(int x, int y, int a, const progresion::Player* pr, int& h) {
  const progresion::Ach& d = progresion::ACH[a];
  const progresion::AchInfo& info = progresion::ACH_INFO[a];
  bool got = pr && (pr->ach[a >> 3] & (1 << (a & 7)));
  bool hidden = info.secret && !got;
  lv_obj_t* c = ui::box(host, x, y, 381, 48);
  lv_obj_set_style_radius(c, ui::RADIUS_SM, 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_border_color(c, ui::hex(got ? RARITY_COLOR[d.rarity] : ui::color::LINE), 0);
  if (!got) lv_obj_set_style_opa(c, 115, 0);
  // Icono: insignia del color de su rareza con la inicial (la imagen de la app no viene por Wi-Fi); «?» si es secreto.
  lv_obj_t* ic = ui::box(c, 10, 0, 34, 34);
  lv_obj_set_style_radius(ic, 10, 0);
  lv_obj_set_style_bg_color(ic, ui::hex(ui::color::CARD2), 0);
  lv_obj_set_style_bg_opa(ic, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ic, 2, 0);
  lv_obj_set_style_border_color(ic, ui::hex(hidden ? ui::color::LINE : RARITY_COLOR[d.rarity]), 0);
  char ini[8];
  if (hidden) snprintf(ini, sizeof(ini), "?");
  else jug::initials(d.name, ini, sizeof(ini));
  lv_obj_t* il = ui::text(ic, ini, 900, 13, hidden ? ui::color::TEXT3 : RARITY_COLOR[d.rarity]);
  lv_obj_center(il);
  lv_obj_t* nm = textAt(c, hidden ? "Logro secreto" : d.name, 700, 14, ui::color::TEXT, 54, 7);
  if (d.title && !hidden) {
    lv_obj_update_layout(nm);
    char t[48], up[48];
    snprintf(t, sizeof(t), "«%s»", d.title);
    upper(t, up, sizeof(up));
    lv_obj_t* tl = ui::text(c, up, 900, 12, ui::color::RANKED, 1.44f);
    lv_obj_set_pos(tl, 54 + lv_obj_get_width(nm) + 6, 9);
  }
  char line[160], when[24] = "";
  snprintf(line, sizeof(line), "%s · %s", hidden ? "???" : info.description, progresion::RARITY_NAME[d.rarity]);
  if (got && pr->achAt[a]) {
    char dt[24];
    partidos::dateText(pr->achAt[a], dt, sizeof(dt));
    snprintf(when, sizeof(when), " · %s", dt);
    // La fecha no se parte (LVGL cortaría en «18:» / «30»): si no cabe entera, a la línea siguiente.
    size_t k = strlen(line);
    snprintf(line + k, sizeof(line) - k, "%s", when);
    const lv_font_t* f = assets::font(400, 13);
    lv_point_t full, part;
    lv_text_get_size(&full, line, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    char pre[160];
    snprintf(pre, sizeof(pre), "%.*s", (int)k, line);
    lv_text_get_size(&part, pre, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (full.x > 315 && part.x <= 315) snprintf(line + k, sizeof(line) - k, " ·\n%s", dt);
  }
  lv_obj_t* l = textAt(c, line, 400, 13, ui::color::TEXT2, 54, 25, 315);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_update_layout(l);
  int hh = 25 + lv_obj_get_height(l) + 7;
  if (hh < 48) hh = 48;
  lv_obj_set_height(c, hh);
  lv_obj_set_y(ic, (hh - 34) / 2);
  if (hh > h) h = hh;
}
static void buildAchievements() {
  const progresion::Player* pr = progresion::of(code);
  textAt(host, "Los secretos se revelan al conseguirlos.", 400, 13, ui::color::TEXT3, 0, 0);
  int y = 24;
  for (uint8_t g = 0; g < 6; g++) {
    label(host, progresion::ACH_GROUP[g], 0, y + 4);
    y += 27;
    int col = 0, rowH = 0;
    lv_obj_t* left = nullptr;
    for (int a = 0; a < progresion::ACH_COUNT; a++) {
      if (progresion::ACH_INFO[a].group != g) continue;
      int h = 0;
      achCard(col ? 387 : 0, y, a, pr, h);
      lv_obj_t* made = lv_obj_get_child(host, -1);
      if (h > rowH) rowH = h;
      if (col) {  // la pareja de la fila, del mismo alto
        lv_obj_set_height(made, rowH);
        if (left) lv_obj_set_height(left, rowH);
        y += rowH + 6;
        rowH = 0;
        left = nullptr;
      } else {
        left = made;
      }
      col ^= 1;
    }
    if (col) y += rowH + 6;
    y += 4;
  }
}

// ---------------------------------------------------------------- pantalla
static void buildContent() {
  if (host) {  // lo de antes se borra después (puede ser el botón que se acaba de tocar)
    lv_obj_add_flag(host, LV_OBJ_FLAG_HIDDEN);
    lv_obj_delete_async(host);
  }
  host = ui::box(scr, 16, 228, 768, 252);
  lv_obj_add_flag(host, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(host, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(host, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_pad_bottom(host, 12, 0);
  if (!haveStats) {
    textAt(host, "Sin memoria para calcular su ficha.", 400, 16, ui::color::TEXT2, 0, 0);
    return;
  }
  switch (tab) {
    case GENERAL: buildGeneral(); break;
    case HABITS: buildHabits(); break;
    case RIVALS: buildRivals(); break;
    case HISTORY: buildHistory(); break;
    case TOURNAMENTS: buildTournaments(); break;
    default: buildAchievements(); break;
  }
}

static void restyleTab(lv_obj_t* b, bool on) {
  lv_obj_set_style_bg_opa(b, on ? 41 : LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(b, ui::hex(on ? ui::color::ACCENT : ui::color::LINE), 0);
  lv_obj_t* l = lv_obj_get_child(b, 0);
  if (l) lv_obj_set_style_text_color(l, ui::hex(on ? ui::color::TEXT : ui::color::TEXT2), 0);
}
static void onTab(lv_event_t* e) {
  uint8_t t = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (t == tab) return;
  tab = t;
  for (int i = 0; i < TAB_COUNT; i++)
    if (tabPill[i]) restyleTab(tabPill[i], i == tab);
  buildContent();
}

static void onBack(lv_event_t*) {
  if (hooks.back) hooks.back();
}

static void buildHead() {
  ui::gradient(scr, 0x0d1d36, 0x04080f);
  lv_obj_t* head = ui::box(scr, 0, 0, 800, 56);
  lv_obj_set_style_bg_color(head, ui::hex(ui::color::BG_DEEP), 0);
  lv_obj_set_style_bg_opa(head, 140, 0);
  lv_obj_t* back = ui::box(scr, 12, 4, 48, 48);
  lv_obj_set_style_radius(back, ui::RADIUS_SM, 0);
  lv_obj_set_style_border_width(back, 1, 0);
  lv_obj_set_style_border_color(back, ui::hex(ui::color::LINE), 0);
  lv_obj_set_style_bg_color(back, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(back, LV_OPA_COVER, LV_STATE_PRESSED);
  static lv_point_precise_t chevron[3] = {{27, 15}, {19, 24}, {27, 33}};
  lv_obj_t* arrow = lv_line_create(back);
  lv_line_set_points(arrow, chevron, 3);
  lv_obj_set_style_line_width(arrow, 3, 0);
  lv_obj_set_style_line_rounded(arrow, true, 0);
  lv_obj_set_style_line_color(arrow, ui::hex(ui::color::TEXT), 0);
  lv_obj_remove_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(back, onBack, LV_EVENT_CLICKED, nullptr);
  textAt(scr, "PERFIL", 900, 20, ui::color::TEXT, 70, 15);

  // Tarjeta del jugador, con el color de su rango.
  const jug::Player* p = store->find(code);
  const progresion::Player* pr = progresion::of(code);
  uint8_t cat = pr ? pr->category : progresion::categoryFor(progresion::rules.eloInitial);
  uint32_t cc = progresion::CATEGORY_COLOR[cat % 7];
  lv_obj_t* c = ui::box(scr, 16, 68, 768, 96);
  lv_obj_set_style_radius(c, 16, 0);
  lv_obj_set_style_bg_color(c, ui::hex(ui::color::CARD2), 0);
  lv_obj_set_style_bg_grad_color(c, ui::hex(ui::color::CARD), 0);
  lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c, 2, 0);
  lv_obj_set_style_border_color(c, ui::hex(cc), 0);
  lv_obj_set_style_shadow_width(c, 22, 0);
  lv_obj_set_style_shadow_color(c, ui::hex(cc), 0);
  lv_obj_set_style_shadow_opa(c, 89, 0);
  avatar(c, code, p ? p->name : "?", 14, 10, 72, cc);
  if (pr) {
    char lv[8];
    snprintf(lv, sizeof(lv), "%d", pr->level);
    lv_obj_t* b = ui::box(c, 59, 58, 33, 28);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, ui::hex(ui::color::ACCENT), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, ui::hex(ui::color::SURFACE), 0);
    lv_obj_t* l = ui::text(b, lv, 900, 13, 0x03121e);
    lv_obj_center(l);
  }
  lv_obj_t* nm = textAt(c, p ? p->name : "?", 900, 24, ui::color::TEXT, 100, 9, 460);
  lv_label_set_long_mode(nm, LV_LABEL_LONG_MODE_DOTS);
  if (p && p->title[0]) {
    char up[48];
    upper(p->title, up, sizeof(up));
    lv_obj_t* t = ui::text(c, up, 900, 12, ui::color::RANKED, 1.44f);
    lv_obj_set_pos(t, 100, 37);
  }
  char line[96];
  if (pr) {
    snprintf(line, sizeof(line), "Nivel %d · %ld XP · siguiente a %ld XP", pr->level, pr->xp, progresion::xpForLevel(pr->level + 1));
    textAt(c, line, 400, 13, ui::color::TEXT2, 100, 52);
    lv_obj_t* tr = ui::box(c, 100, 72, 300, 8);
    lv_obj_set_style_radius(tr, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(tr, ui::hex(ui::color::LINE), 0);
    lv_obj_set_style_bg_opa(tr, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(tr, true, 0);
    int w = (int)lround(300 * progresion::levelProgress(pr->xp));
    if (w > 0) {
      lv_obj_t* f = ui::box(tr, 0, 0, w, 8);
      lv_obj_set_style_bg_color(f, ui::hex(ui::color::ACCENT), 0);
      lv_obj_set_style_bg_grad_color(f, ui::hex(0xa58bff), 0);
      lv_obj_set_style_bg_grad_dir(f, LV_GRAD_DIR_HOR, 0);
      lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
    }
    rankBadge(c, cat, 584, 14, 64);
    textAt(c, progresion::CATEGORY[cat % 7], 800, 13, cc, 656, 16, 95, LV_TEXT_ALIGN_RIGHT);
    snprintf(line, sizeof(line), "%d", pr->elo);
    textAt(c, line, 900, 24, ui::color::TEXT, 656, 31, 95, LV_TEXT_ALIGN_RIGHT);
    snprintf(line, sizeof(line), "ELO · máx %d", pr->maxElo);
    textAt(c, line, 400, 13, ui::color::TEXT3, 616, 61, 135, LV_TEXT_ALIGN_RIGHT);
  } else {
    textAt(c, "Progresión pendiente", 400, 13, ui::color::TEXT2, 100, 52);
  }

  // Pestañas.
  char logros[24];
  snprintf(logros, sizeof(logros), "LOGROS %d/%d", pr ? pr->achCount : 0, progresion::ACH_COUNT);
  const char* names[TAB_COUNT] = {"GENERAL", "HÁBITOS", "RIVALES", "HISTORIAL", "TORNEOS", logros};
  static const int W[TAB_COUNT] = {85, 82, 78, 93, 87, 117};
  int x = 16;
  for (int i = 0; i < TAB_COUNT; i++) {
    lv_obj_t* b = ui::box(scr, x, 174, W[i], 44);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, ui::hex(ui::color::ACCENT), 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_t* l = ui::text(b, names[i], 700, 12, ui::color::TEXT2, 0.6f);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, onTab, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    tabPill[i] = b;
    restyleTab(b, i == tab);
    x += W[i] + 6;
  }
}

static void onDelete(lv_event_t*) {
  scr = nullptr;
  host = nullptr;
  help = nullptr;
  memset(tabPill, 0, sizeof(tabPill));
}

static bool active() { return scr && lv_screen_active() == scr; }

static void freeData() {
  if (haveStats) st.release();
  haveStats = false;
  free(titles);
  titles = nullptr;
  nTitles = 0;
}

// Abre la ficha de `c` (desde el principio: GENERAL) o, con `fresh` = falso, la vuelve a pintar como estaba (al
// volver del detalle de un partido o del reposo).
static void show(jug::Store* s, uint32_t c, bool fresh) {
  store = s;
  if (fresh || c != code || !haveStats || statsRevision != partidos::revision) {
    freeData();
    if (fresh) tab = GENERAL, rankedScope = false, tourFilter = 0;
    code = c;
    uint32_t codes[jug::MAX];
    for (int k = 0; k < s->count; k++) codes[k] = s->list[k].code;
    progresion::refresh(codes, s->count);
    haveStats = perfil::stats(code, st);
    statsRevision = partidos::revision;
  }
  lv_obj_t* old = scr;
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  host = help = nullptr;
  memset(tabPill, 0, sizeof(tabPill));
  buildHead();
  buildContent();
  if (old) lv_obj_remove_event_cb(old, onDelete);
  ui::show(scr);
}

// Al salir del todo (volver a donde se abrió): devuelve la memoria de las cuentas.
static void close() { freeData(); }

}  // namespace ficha

#endif  // MFV3_UI_FICHA_H
