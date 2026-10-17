// MARCADOR FUTBOLÍN V3 · tanda de penaltis, igual que la app (PenaltiesView de MatchScreen.tsx y match.css).
// --------------------------------------------------------------------------------------------------------
// Dos columnas (Blanco y Azul) con su marcador de penaltis, un círculo por lanzamiento (✓ verde / ✕ rojo; los de
// muerte súbita con borde discontinuo) y GOL / FALLO; la columna que lanza se ilumina con un foco que late.
// En el centro, la portería de neón (el balón entra o se va fuera tras cada lanzamiento), quién lanza, el intento
// y «Deshacer lanzamiento». Arriba: TANDA DE PENALTIS, MUERTE SÚBITA y el marcador ordinario.
// Fondo, portería y botón de deshacer son capas de la app; lo demás lo dibuja LVGL con las medidas de la app.

#ifndef MFV3_UI_PENALTIS_H
#define MFV3_UI_PENALTIS_H

#include "ui_base.h"

namespace penaltis {

const uint8_t MAX_KICKS = 40;

struct View {
  uint8_t penWhite, penBlue;      // goles de la tanda
  uint8_t white, blue;            // marcador ordinario
  uint8_t rounds;                 // lanzamientos por equipo antes de la muerte súbita
  uint8_t kicks;                  // lanzamientos hechos
  bool scored[MAX_KICKS];         // por orden
  uint8_t team[MAX_KICKS];        // 0 Blanco, 1 Azul
  bool active;                    // aún no decidida
  uint8_t turn;                   // quién lanza
  bool sudden;
};

struct Hooks {
  void (*kick)(uint8_t team, bool scored) = nullptr;
  void (*undo)() = nullptr;
};
static Hooks hooks;

static lv_obj_t* scr = nullptr;
static lv_obj_t *col[2], *spot[2], *score[2], *dots[2], *btnGoal[2], *btnMiss[2];
static lv_obj_t *sudden, *ordinary, *turnBox, *turnText, *attempt, *lanza, *undo, *ball, *decided;
static View shown;
static bool fresh = true;
static uint8_t lastKicks = 0;

const int COL_X[2] = {12, 500}, COL_Y = 54, COL_W = 288, COL_H = 416;

static void onKick(lv_event_t* e) {
  intptr_t code = (intptr_t)lv_event_get_user_data(e);
  uint8_t t = (uint8_t)(code >> 1);
  bool goal = !(code & 1);
  if (shown.active && shown.turn == t && hooks.kick) hooks.kick(t, goal);
}
static void onUndo(lv_event_t*) {
  if (shown.kicks && hooks.undo) hooks.undo();
}

static void spotCb(void* o, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }

// Botón GOL / FALLO: verde o rojo cuando le toca a esa columna; apagado (gris al 40 %) si no.
static lv_obj_t* kickButton(lv_obj_t* parent, int x, const char* txt, bool goal, intptr_t code) {
  lv_obj_t* b = ui::button(parent, x, 386 - COL_Y, 128, 72, txt, 20, ui::BtnKind::Normal, onKick, (void*)code);
  (void)goal;
  return b;
}

static void styleKickButton(lv_obj_t* b, bool enabled, bool goal) {
  if (enabled) {
    lv_obj_set_style_bg_color(b, ui::hex(goal ? ui::color::OK : ui::color::DANGER), 0);
    lv_obj_set_style_bg_opa(b, goal ? 46 : 41, 0);
    lv_obj_set_style_border_color(b, ui::hex(goal ? ui::color::OK : ui::color::DANGER), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(b, 0), ui::hex(goal ? 0xc9ffe6 : 0xffd2d8), 0);
    lv_obj_set_style_opa(b, LV_OPA_COVER, 0);
  } else {
    lv_obj_set_style_bg_color(b, ui::hex(ui::color::CARD), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, ui::hex(ui::color::LINE), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(b, 0), ui::hex(ui::color::TEXT), 0);
    lv_obj_set_style_opa(b, 102, 0);
  }
}

// Marca ✓ / ✕ dibujada con rayas (la letra Inter de la app no se usa para estos símbolos).
static void mark(lv_obj_t* dot, bool hit) {
  static lv_point_precise_t tick[3] = {{8, 14}, {12, 18}, {19, 9}};
  static lv_point_precise_t c1[2] = {{9, 9}, {18, 18}}, c2[2] = {{18, 9}, {9, 18}};
  uint32_t c = hit ? ui::color::OK : ui::color::DANGER;
  auto line = [&](lv_point_precise_t* p, uint32_t n) {
    lv_obj_t* l = lv_line_create(dot);
    lv_line_set_points(l, p, n);
    lv_obj_set_style_line_width(l, 3, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_set_style_line_color(l, ui::hex(c), 0);
    lv_obj_set_pos(l, -2, -2);
  };
  if (hit) line(tick, 3);
  else {
    line(c1, 2);
    line(c2, 2);
  }
}

static void drawDots(int t, const View& v) {
  lv_obj_clean(dots[t]);
  uint8_t mine = 0;
  bool done[MAX_KICKS] = {}, hit[MAX_KICKS] = {};
  for (uint8_t i = 0; i < v.kicks; i++)
    if (v.team[i] == t) {
      done[mine] = true;
      hit[mine] = v.scored[i];
      mine++;
    }
  uint8_t slots = v.rounds;
  if (mine + (v.active && v.turn == t ? 1 : 0) > slots) slots = mine + 1;
  // Cinco por fila (164 px de ancho: 28 + 6 de hueco), centradas, como el flex-wrap de la app.
  for (uint8_t i = 0; i < slots; i++) {
    int row = i / 5, inRow = (slots - row * 5) < 5 ? slots - row * 5 : 5;
    int x = (164 - (inRow * 28 + (inRow - 1) * 6)) / 2 + (i % 5) * 34;
    lv_obj_t* d = ui::box(dots[t], x, row * 34, 28, 28);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(d, 2, 0);
    uint32_t bc = !done[i] ? ui::color::LINE : hit[i] ? ui::color::OK : ui::color::DANGER;
    lv_obj_set_style_border_color(d, ui::hex(bc), 0);
    if (done[i]) {
      lv_obj_set_style_bg_color(d, ui::hex(bc), 0);
      lv_obj_set_style_bg_opa(d, hit[i] ? 51 : 41, 0);
      mark(d, hit[i]);
    }
    if (i >= v.rounds) lv_obj_set_style_border_opa(d, 170, 0);  // muerte súbita (en la app, borde discontinuo)
  }
}

// Balón de la portería: entra (sube a la red) o se va fuera, como ball-goal / ball-miss.
static void ballX(void* o, int32_t v) { lv_obj_set_x((lv_obj_t*)o, v); }
static void ballY(void* o, int32_t v) { lv_obj_set_y((lv_obj_t*)o, v); }
static void ballOpa(void* o, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }
static void shoot(bool goal) {
  // Portería: SVG de 200 × 120 unidades dibujado a 170 × 102 (escala 0,85) en (315, 58); balón r = 7 en (100, 104).
  const float k = 0.85f;
  int x0 = (int)(315 + (100 - 7) * k), y0 = (int)(58 + (104 - 7) * k);
  lv_anim_delete(ball, nullptr);
  lv_obj_set_pos(ball, x0, y0);
  lv_obj_set_style_opa(ball, LV_OPA_COVER, 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, ball);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_duration(&a, goal ? 700 : 800);
  lv_anim_set_exec_cb(&a, ballX);
  lv_anim_set_values(&a, x0, x0 + (int)((goal ? 18 : 120) * k));
  lv_anim_start(&a);
  lv_anim_set_exec_cb(&a, ballY);
  lv_anim_set_values(&a, y0, y0 + (int)((goal ? -60 : -40) * k));
  lv_anim_start(&a);
  if (!goal) {
    lv_anim_set_exec_cb(&a, ballOpa);
    lv_anim_set_values(&a, 255, 77);
    lv_anim_start(&a);
  }
}

static void onDelete(lv_event_t*) { scr = nullptr; }

static void build() {
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  ui::layer(scr, capas::PENALTIS);
  // Cabecera
  lv_obj_t* chip = ui::text(scr, "TANDA DE PENALTIS", 900, 15, ui::color::TEXT, 1.8f);
  ui::place(chip, 12, 17.5f, 0, ui::Align::Left);
  sudden = ui::box(scr, 205, 15, 10, 24);
  lv_obj_set_width(sudden, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(sudden, 9, 0);
  lv_obj_set_style_radius(sudden, 999, 0);
  lv_obj_set_style_border_width(sudden, 1, 0);
  lv_obj_set_style_border_color(sudden, ui::hex(ui::color::DANGER), 0);
  lv_obj_set_style_bg_color(sudden, ui::hex(ui::color::DANGER), 0);
  lv_obj_set_style_bg_opa(sudden, 41, 0);
  lv_obj_t* st = ui::text(sudden, "MUERTE SÚBITA", 800, 12, ui::color::DANGER, 0.96f);
  lv_obj_center(st);
  ordinary = ui::text(scr, "", 400, 13, ui::color::TEXT2);
  for (int t = 0; t < 2; t++) {
    col[t] = ui::box(scr, COL_X[t], COL_Y, COL_W, COL_H);
    lv_obj_set_style_radius(col[t], 20, 0);
    lv_obj_set_style_border_width(col[t], 2, 0);
    lv_obj_set_style_bg_color(col[t], ui::hex(0x11223b), 0);
    lv_obj_set_style_bg_opa(col[t], 153, 0);
    lv_obj_set_style_clip_corner(col[t], true, 0);
    // Foco de arriba (radial blanco al 18 %), que late cada 2,4 s.
    spot[t] = ui::box(col[t], -26, -83, 341, 333);
    static lv_grad_dsc_t g;
    lv_grad_radial_init(&g, LV_PCT(50), 0, LV_PCT(50), LV_PCT(70), LV_GRAD_EXTEND_PAD);
    g.stops[0].color = lv_color_white();
    g.stops[0].opa = 46;
    g.stops[0].frac = 0;
    g.stops[1].color = lv_color_white();
    g.stops[1].opa = 0;
    g.stops[1].frac = 255;
    g.stops_count = 2;
    lv_obj_set_style_bg_grad(spot[t], &g, 0);
    lv_obj_set_style_bg_opa(spot[t], LV_OPA_COVER, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, spot[t]);
    lv_anim_set_exec_cb(&a, spotCb);
    lv_anim_set_values(&a, 255, 140);
    lv_anim_set_duration(&a, 1200);
    lv_anim_set_reverse_duration(&a, 1200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
    lv_obj_t* name = ui::text(col[t], t ? "AZUL" : "BLANCO", 900, 15, ui::color::TEXT, 3.6f);
    ui::place(name, COL_W / 2.0f - 2 + 1.8f, 66 - COL_Y - 2, 0);
    score[t] = ui::text(col[t], "0", 900, 96, t ? 0x4fa3f0 : ui::color::WHITE_TEAM);
    dots[t] = ui::box(col[t], 62 - 2, 197 - COL_Y - 2, 164, 70);
    btnGoal[t] = kickButton(col[t], 10, "GOL", true, (t << 1) | 0);
    btnMiss[t] = kickButton(col[t], 146, "FALLO", false, (t << 1) | 1);
  }
  // Centro: portería, quién lanza, intento y deshacer.
  ui::layer(scr, capas::PENALTIS_PORTERIA);
  ball = ui::box(scr, 0, 0, 12, 12);
  lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ball, ui::hex(ui::color::WHITE_TEAM), 0);
  lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(ball, 12, 0);
  lv_obj_set_style_shadow_color(ball, lv_color_white(), 0);
  lanza = ui::text(scr, "LANZA", 700, 12, ui::color::TEXT2, 1.44f);
  ui::place(lanza, 400 + 0.72f, 168, 0);
  turnBox = ui::box(scr, 310, 191, 180, 48);
  lv_obj_set_style_radius(turnBox, 12, 0);
  lv_obj_set_style_bg_opa(turnBox, LV_OPA_COVER, 0);
  turnText = ui::text(turnBox, "", 900, 30, ui::color::TEXT, 3);
  attempt = ui::text(scr, "", 400, 13, ui::color::TEXT3);
  undo = ui::layer(scr, capas::PENALTIS_DESHACER);
  lv_obj_add_flag(undo, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(undo, onUndo, LV_EVENT_CLICKED, nullptr);
  decided = ui::text(scr, "DECIDIDA", 900, 30, ui::color::TEXT, 3);
  ui::place(decided, 400 + 1.5f, 191 + 6, 0);
  fresh = true;
}

static bool active() { return scr && lv_screen_active() == scr; }

static void update(const View& v) {
  if (!active()) {
    build();
    ui::show(scr);
    lastKicks = v.kicks;
  }
  bool first = fresh;
  fresh = false;
  char b[48];
  for (int t = 0; t < 2; t++) {
    bool turn = v.active && v.turn == t;
    lv_obj_set_style_opa(col[t], turn ? LV_OPA_COVER : 153, 0);
    lv_obj_set_style_border_color(col[t], ui::hex(turn ? (t ? ui::color::BLUE_TEAM : ui::color::WHITE_TEAM) : ui::color::LINE), 0);
    lv_obj_set_style_shadow_width(col[t], turn ? 24 : 0, 0);
    lv_obj_set_style_shadow_color(col[t], ui::hex(t ? ui::color::BLUE_TEAM : ui::color::WHITE_TEAM), 0);
    lv_obj_set_style_shadow_opa(col[t], t ? 128 : 64, 0);
    if (turn) lv_obj_remove_flag(spot[t], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(spot[t], LV_OBJ_FLAG_HIDDEN);
    uint8_t p = t ? v.penBlue : v.penWhite;
    if (first || p != (t ? shown.penBlue : shown.penWhite)) {
      snprintf(b, sizeof(b), "%u", p);
      lv_label_set_text(score[t], b);
      ui::place(score[t], COL_W / 2.0f - 2, 93 - COL_Y - 2, 96);
    }
    styleKickButton(btnGoal[t], turn, true);
    styleKickButton(btnMiss[t], turn, false);
    if (first || v.kicks != shown.kicks || v.active != shown.active || v.turn != shown.turn) drawDots(t, v);
  }
  if (v.sudden) lv_obj_remove_flag(sudden, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(sudden, LV_OBJ_FLAG_HIDDEN);
  snprintf(b, sizeof(b), "Marcador ordinario %u–%u", v.white, v.blue);
  if (strcmp(b, lv_label_get_text(ordinary))) {
    lv_label_set_text(ordinary, b);
    ui::place(ordinary, 788, 19, 0, ui::Align::Right);
  }
  // Quién lanza: recuadro con el color del equipo (blanco con letra oscura o azul con letra clara).
  bool act = v.active;
  if (act) lv_obj_remove_flag(turnBox, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(turnBox, LV_OBJ_FLAG_HIDDEN);
  if (act) lv_obj_remove_flag(lanza, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(lanza, LV_OBJ_FLAG_HIDDEN);
  if (act) lv_obj_remove_flag(attempt, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(attempt, LV_OBJ_FLAG_HIDDEN);
  if (act) lv_obj_remove_flag(undo, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(undo, LV_OBJ_FLAG_HIDDEN);
  if (act) lv_obj_add_flag(decided, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(decided, LV_OBJ_FLAG_HIDDEN);
  if (act) {
    lv_obj_set_style_bg_color(turnBox, ui::hex(v.turn ? ui::color::BLUE_TEAM : ui::color::WHITE_TEAM), 0);
    lv_obj_set_style_text_color(turnText, ui::hex(v.turn ? ui::color::TEXT : ui::color::WHITE_INK), 0);
    lv_label_set_text(turnText, v.turn ? "AZUL" : "BLANCO");
    lv_obj_center(turnText);
    uint8_t mine = 0;
    for (uint8_t i = 0; i < v.kicks; i++)
      if (v.team[i] == v.turn) mine++;
    if (v.sudden) snprintf(b, sizeof(b), "Intento %u · se decide por parejas", mine + 1);
    else snprintf(b, sizeof(b), "Intento %u de %u", mine + 1, v.rounds);
    lv_label_set_text(attempt, b);
    ui::place(attempt, 400, 247, 0);
    lv_obj_set_style_opa(undo, v.kicks ? LV_OPA_COVER : 102, 0);
  }
  if (!first && v.kicks > lastKicks) shoot(v.scored[v.kicks - 1]);
  if (first || v.kicks < lastKicks) {
    const float k = 0.85f;
    lv_anim_delete(ball, nullptr);
    lv_obj_set_pos(ball, (int)(315 + 93 * k), (int)(58 + 97 * k));
    lv_obj_set_style_opa(ball, LV_OPA_COVER, 0);
  }
  lastKicks = v.kicks;
  shown = v;
}

}  // namespace penaltis

#endif  // MFV3_UI_PENALTIS_H
