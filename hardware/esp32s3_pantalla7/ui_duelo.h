// MARCADOR FUTBOLÍN V3 · CARA A CARA de dos jugadores (DuelScreen.tsx de la app).
// -------------------------------------------------------------------------------------------------
// Se abre desde la ficha (RIVALES: rival favorito, némesis y enfrentamientos directos) tocando un rival.
//   - Arriba, volver (a la ficha como estaba) y CARA A CARA.
//   - Los dos con su foto y su nombre (tocar al rival abre su ficha; tocar al de la ficha, vuelve a ella) y en medio
//     cuántas veces ha ganado cada uno, los enfrentamientos y los empates.
//   - Sus números uno al lado del otro: ELO, nivel, partidos, victorias, % victorias, mejor racha, diferencia de goles
//     de equipo y logros; el mejor de cada fila, en verde.
// Las cuentas son las de la ficha (perfil::stats, igual que la app; el cara a cara es la fila del rival en
// «Enfrentamientos directos», que cuenta lo mismo que duel() de la app) y la progresión (s3_progresion.h).
// La tabla se dibuja como una sola pieza (cada objeto de LVGL gasta memoria interna). Solo letras de fuentes.txt.
// Se incluye después de ui_ficha.h.

#ifndef MFV3_UI_DUELO_H
#define MFV3_UI_DUELO_H

#include "ui_ficha.h"

namespace duelo {

struct Hooks {
  void (*back)() = nullptr;               // volver a la ficha
  void (*openPlayer)(uint32_t c) = nullptr;  // tocar a uno de los dos: su ficha
};
static Hooks hooks;

static const int ROWS = 8;
struct Data {
  uint32_t code[2];
  int played, wins[2], draws;
  char val[ROWS][2][12];
  int8_t better[ROWS];  // 0 el de la izquierda, 1 el de la derecha, -1 ninguno
};
static Data* d = nullptr;  // en la PSRAM; se queda para volver a pintarla (al volver del reposo)
static uint32_t dataRevision = 0;
static lv_obj_t* scr = nullptr;

static const char* const LABEL[ROWS] = {"ELO", "NIVEL", "PARTIDOS", "VICTORIAS", "% VICTORIAS", "MEJOR RACHA", "DIF. GOLES EQUIPO",
                                        "LOGROS"};

// cmp() de la app: el mayor gana; sin dato o iguales, ninguno.
static int8_t cmp(bool hx, double x, bool hy, double y) { return !hx || !hy || x == y ? -1 : x > y ? 0 : 1; }

static bool compute(uint32_t a, uint32_t b) {
  if (!d) d = (Data*)s3mem::allocateUi(sizeof(Data));
  if (!d) return false;
  memset(d, 0, sizeof(Data));
  d->code[0] = a, d->code[1] = b;
  perfil::Stats s[2];
  if (!perfil::stats(a, s[0])) return false;
  if (!perfil::stats(b, s[1])) {
    s[0].release();
    return false;
  }
  for (int i = 0; i < s[0].nr; i++)
    if (s[0].rivals[i].code == b) {
      const perfil::Rec& r = s[0].rivals[i].r;
      d->played = r.played, d->wins[0] = r.wins, d->wins[1] = r.losses, d->draws = r.draws;
    }
  const progresion::Player* p[2] = {progresion::of(a), progresion::of(b)};
  double v[ROWS][2];
  bool has[ROWS][2];
  for (int k = 0; k < 2; k++) {
    const perfil::Block& g = s[k].general;
    v[0][k] = p[k] ? p[k]->elo : 0, has[0][k] = p[k];
    v[1][k] = p[k] ? p[k]->level : 0, has[1][k] = p[k];
    v[2][k] = g.played, v[3][k] = g.wins;
    v[4][k] = g.played ? (double)g.wins / g.played * 100 : 0, has[4][k] = g.played > 0;
    v[5][k] = s[k].best, v[6][k] = g.gf - g.ga;
    v[7][k] = p[k] ? p[k]->achCount : 0;
    has[2][k] = has[3][k] = has[5][k] = has[6][k] = has[7][k] = true;
    for (int r = 0; r < ROWS; r++) {
      char* t = d->val[r][k];
      if (!has[r][k]) snprintf(t, 12, "—");
      else if (r == 4) snprintf(t, 12, "%.0f %%", v[r][k]);
      else snprintf(t, 12, "%d", (int)v[r][k]);
    }
  }
  // Logros sin progresión: 0 (como la app), pero sin dato para comparar.
  has[7][0] = p[0], has[7][1] = p[1];
  for (int r = 0; r < ROWS; r++) d->better[r] = cmp(has[r][0], v[r][0], has[r][1], v[r][1]);
  s[0].release();
  s[1].release();
  dataRevision = partidos::revision;
  return true;
}

// La tabla (.duel-table): filas de 28 px con 4 de separación.
static void drawTable(lv_event_t* e) {
  lv_obj_t* o = lv_event_get_current_target_obj(e);
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  for (int r = 0; r < ROWS; r++) {
    int y = a.y1 + r * 32;
    ficha::drawBox(layer, a.x1, y, a.x1 + 767, y + 27, 8, ui::color::CARD, 153, ui::color::LINE, 0);
    for (int k = 0; k < 2; k++) {
      int x = a.x1 + (k ? 474 : 0);
      ficha::drawText(layer, d->val[r][k], 800, 17, d->better[r] == k ? ui::color::OK : ui::color::TEXT, x, y + 3, x + 293,
                      LV_TEXT_ALIGN_CENTER);
    }
    ficha::drawText(layer, LABEL[r], 700, 12, ui::color::TEXT2, a.x1 + 294, y + 7, a.x1 + 473, LV_TEXT_ALIGN_CENTER, LV_OPA_COVER, 1);
  }
}

static void onBack(lv_event_t*) {
  if (hooks.back) hooks.back();
}
static void onFace(lv_event_t* e) {
  uint32_t c = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  if (hooks.openPlayer) hooks.openPlayer(c);
}

// Foto grande y nombre (.duel-face): se toca para ir a su ficha.
static void face(int k) {
  uint32_t c = d->code[k];
  const jug::Player* p = ficha::store->find(c);
  lv_obj_t* b = ui::box(scr, k ? 490 : 16, 68, 294, 104);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(b, ui::RADIUS, 0);
  lv_obj_set_style_bg_color(b, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_add_event_cb(b, onFace, LV_EVENT_CLICKED, (void*)(uintptr_t)c);
  ficha::avatar(b, p ? c : 0, p ? p->name : "?", 111, 1, 72);
  lv_obj_t* n = ficha::textAt(b, p ? p->name : "?", 800, 20, ui::color::TEXT, 4, 79, 286, LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
}

static void build() {
  ficha::frame(scr, "CARA A CARA", onBack);
  if (!d) {
    ficha::textAt(scr, "Sin memoria para calcular el cara a cara.", 400, 16, ui::color::TEXT2, 16, 80);
    return;
  }
  face(0);
  face(1);
  char t[48];
  snprintf(t, sizeof(t), "%d – %d", d->wins[0], d->wins[1]);
  ficha::textAt(scr, t, 900, 30, ui::color::RANKED, 320, 94, 160, LV_TEXT_ALIGN_CENTER);
  int n = snprintf(t, sizeof(t), "%d enfrentamiento%s", d->played, d->played == 1 ? "" : "s");
  if (d->draws) snprintf(t + n, sizeof(t) - n, " · %d empate%s", d->draws, d->draws == 1 ? "" : "s");
  lv_obj_t* sub = ficha::textAt(scr, t, 900, 13, ui::color::TEXT3, 320, 130, 160, LV_TEXT_ALIGN_CENTER);
  lv_label_set_long_mode(sub, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_t* table = ui::box(scr, 16, 180, 768, ROWS * 32 - 4);
  lv_obj_add_event_cb(table, drawTable, LV_EVENT_DRAW_MAIN, nullptr);
}

static void onDelete(lv_event_t*) { scr = nullptr; }

static bool active() { return scr && lv_screen_active() == scr; }

// Abre el cara a cara de `a` (el de la ficha) contra `b`; con `fresh` = falso y los mismos, lo vuelve a pintar
// como estaba (al volver del reposo).
static void show(jug::Store* s, uint32_t a, uint32_t b, bool fresh) {
  ficha::store = s;
  if (fresh || !d || d->code[0] != a || d->code[1] != b || dataRevision != partidos::revision) {
    uint32_t codes[jug::MAX];
    for (int k = 0; k < s->count; k++) codes[k] = s->list[k].code;
    progresion::refresh(codes, s->count);
    if (!compute(a, b)) {
      free(d);
      d = nullptr;
    }
  }
  lv_obj_t* old = scr;
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  build();
  if (old) lv_obj_remove_event_cb(old, onDelete);
  ui::show(scr);
}

}  // namespace duelo

#endif  // MFV3_UI_DUELO_H
