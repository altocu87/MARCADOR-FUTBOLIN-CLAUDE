// MARCADOR FUTBOLÍN V3 · lo que se lleva cada jugador al acabar un partido, debajo del marcador de la victoria
// (las filas de jugador de VictoryScreen.tsx de la app, en sencillo y sin animaciones).
// -------------------------------------------------------------------------------------------------
// Una columna por equipo (borde blanco o azul). Cada jugador: foto, nombre, la experiencia ganada (+190 XP), el ELO
// que gana o pierde (solo en clasificatorios), su nivel y la barra del nivel. Si sube de nivel, en dorado
// («¡NIVEL 27!») y la barra también. Partidos de torneo: nada (la XP se da al acabar el torneo; ya lo dice la
// pantalla). Las cuentas son las de la progresión (s3_progresion.h, iguales que la app), que se rehace después de
// guardar el partido. Todo es una sola pieza dibujada encima de la pantalla de victoria. Se incluye después de
// ui_ficha.h.

#ifndef MFV3_UI_PREMIOS_H
#define MFV3_UI_PREMIOS_H

#include "ui_ficha.h"
#include "ui_victoria.h"

namespace premios {

struct Row {
  uint32_t code;
  uint8_t team;
  uint16_t xp;
  uint8_t levelBefore, levelAfter, fill;  // fill: barra del nivel al acabar, 0–100
  bool hasElo;
  int16_t eloDelta, eloAfter;
};
struct Data {
  int n;
  Row row[8];
};
static Data* d = nullptr;  // en la PSRAM; nullptr o n = 0: no se enseña nada
static lv_obj_t* panel = nullptr;
static lv_obj_t* panelScr = nullptr;  // la pantalla de victoria en la que está

const int Y0 = 294, Y1 = 474, COL_W = 376;

static void clear() {
  if (d) d->n = 0;
}

// Lo de cada jugador en la fila `ri` de partidos::rows (el partido recién guardado). false si no hay nada que enseñar.
static bool compute(jug::Store* s, int ri) {
  clear();
  if (ri < 0 || ri >= partidos::count) return false;
  uint32_t all[jug::MAX];
  for (int k = 0; k < s->count; k++) all[k] = s->list[k].code;
  progresion::refresh(all, s->count);
  const progresion::Snapshot& sn = progresion::all;
  if (!sn.entries || ri >= sn.rowsCap) return false;
  if (!d) d = (Data*)s3mem::allocateUi(sizeof(Data));
  if (!d) return false;
  memset(d, 0, sizeof(Data));
  const partidos::Row& m = partidos::rows[ri];
  for (int t = 0; t < 2; t++)
    for (int i = 0; i < m.n[t] && i < 4; i++) {
      const progresion::Entry& e = sn.entries[(size_t)ri * 8 + t * 4 + i];
      if (e.deferred) continue;  // torneo en juego
      Row& r = d->row[d->n];
      r.code = m.code[t][i];
      r.team = (uint8_t)t;
      r.xp = e.xpGained;
      r.levelBefore = e.levelBefore;
      r.levelAfter = e.levelAfter;
      r.hasElo = e.eloDelta != 0x7FFF;
      r.eloDelta = r.hasElo ? e.eloDelta : 0;
      r.eloAfter = e.eloAfter;
      const progresion::Player* p = progresion::of(r.code);
      r.fill = p ? (uint8_t)lround(progresion::levelProgress(p->xp) * 100) : 0;
      if (r.code) d->n++;
    }
  return d->n > 0;
}

static void drawRow(lv_layer_t* layer, const Row& r, int x0, int y, int h) {
  const jug::Player* p = ficha::store ? ficha::store->find(r.code) : nullptr;
  int face = h >= 44 ? 36 : 30;
  ficha::drawFace(layer, r.code, p ? p->name : "?", x0 + 10, y + (h - face) / 2, face);
  int x = x0 + 18 + face;
  char t[48];
  snprintf(t, sizeof(t), "%s", p ? p->name : "?");
  ficha::fitText(t, 800, 17, 150);
  int line1 = y + h / 2 - 20, line2 = y + h / 2 + 2;
  ficha::drawText(layer, t, 800, 17, ui::color::TEXT, x, line1, x + 150);
  // Experiencia ganada y ELO (solo si lo movió: clasificatorios).
  snprintf(t, sizeof(t), "+%u XP", r.xp);
  ficha::drawText(layer, t, 800, 15, ui::color::OK, x0 + 196, line1 + 1, x0 + 284, LV_TEXT_ALIGN_RIGHT);
  if (r.hasElo) {
    snprintf(t, sizeof(t), "%+d ELO", (int)r.eloDelta);
    ficha::drawText(layer, t, 800, 15, r.eloDelta >= 0 ? ui::color::OK : ui::color::DANGER, x0 + 284, line1 + 1, x0 + COL_W - 10,
                    LV_TEXT_ALIGN_RIGHT);
  }
  // Nivel y su barra (dorados si ha subido).
  bool up = r.levelAfter > r.levelBefore;
  if (up) snprintf(t, sizeof(t), "¡NIVEL %u!", r.levelAfter);
  else snprintf(t, sizeof(t), "NV %u", r.levelAfter);
  ficha::drawText(layer, t, 800, 13, up ? ui::color::RANKED : ui::color::TEXT2, x, line2 + 2, x + 90);
  int bx = x + 92, bw = x0 + COL_W - 10 - bx;
  ficha::drawBox(layer, bx, line2 + 6, bx + bw - 1, line2 + 15, 5, ui::color::LINE, LV_OPA_COVER, 0, 0);
  int fw = bw * (r.fill > 100 ? 100 : r.fill) / 100;
  if (fw > 2) ficha::drawBox(layer, bx, line2 + 6, bx + fw - 1, line2 + 15, 5, up ? ui::color::RANKED : ui::color::ACCENT, LV_OPA_COVER, 0, 0);
}

static void drawPanel(lv_event_t* e) {
  if (!d || !d->n) return;
  lv_obj_t* o = lv_event_get_current_target_obj(e);
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  for (int t = 0; t < 2; t++) {
    int n = 0;
    for (int i = 0; i < d->n; i++) n += d->row[i].team == t;
    if (!n) continue;
    int x0 = t ? a.x1 + 800 - 16 - COL_W : a.x1 + 16;
    int h = (Y1 - Y0 - 8) / n;
    if (h > 52) h = 52;
    int top = a.y1, bottom = a.y1 + n * h + 8;
    ficha::drawBox(layer, x0, top, x0 + COL_W - 1, bottom, ui::RADIUS_SM, 0x04080f, 200, t ? ui::color::BLUE_TEAM : 0xc9d3e0, 2);
    int k = 0;
    for (int i = 0; i < d->n; i++)
      if (d->row[i].team == t) drawRow(layer, d->row[i], x0, top + 4 + (k++) * h, h);
  }
}

static void onPanelDelete(lv_event_t*) { panel = panelScr = nullptr; }

// Pone la tabla en la pantalla de victoria que se ve (si aún no la tiene).
static void attach(jug::Store* s) {
  if (!d || !d->n || !victoria::active()) return;
  if (panel && panelScr == victoria::scr) return;
  ficha::store = s;
  panel = ui::box(victoria::scr, 0, Y0, 800, Y1 - Y0);
  panelScr = victoria::scr;
  lv_obj_add_event_cb(panel, onPanelDelete, LV_EVENT_DELETE, nullptr);
  lv_obj_add_event_cb(panel, drawPanel, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_move_to_index(panel, 1);  // encima del fondo y debajo del confeti y los botones
}

}  // namespace premios

#endif  // MFV3_UI_PREMIOS_H
