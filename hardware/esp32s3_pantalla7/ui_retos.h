// MARCADOR FUTBOLÍN V3 · RETOS (diarios y semanales), como la app.
// -------------------------------------------------------------------------------------------------
// Copia de ChallengesScreen.tsx:
//   - Arriba, volver, RETOS · DIARIOS Y SEMANALES y la «?» con la explicación.
//   - Cuatro tarjetas: el reto de hoy (dorado, con las horas que le quedan) y los tres de la semana (azules), con la
//     experiencia que dan.
//   - Debajo, cada jugador (por nombre) con su foto y una barra por reto: «2/6» o ✓ verde si ya lo ha cumplido.
//     Tocar una fila abre la ficha del jugador (ui_ficha.h), como en la app.
// Los retos y el progreso salen de s3_progresion.h (activeChallenges y challengeProgress, los de la app).
// Solo letras de fuentes.txt (una actualización por Wi-Fi no lleva letras nuevas); el ✓ se dibuja con rayas.

#ifndef MFV3_UI_RETOS_H
#define MFV3_UI_RETOS_H

#include "ui_base.h"
#include "s3_jugadores.h"
#include "s3_partidos.h"
#include "s3_progresion.h"
#include "s3_torneos_reglas.h"

namespace retos {

struct Hooks {
  void (*back)() = nullptr;
  uint32_t (*nowUtc)() = nullptr;  // hora actual (segundos UTC; 0 sin hora): los retos dependen del día
  void (*openPlayer)(uint32_t code) = nullptr;  // tocar una fila: la ficha del jugador
};
static Hooks hooks;

static jug::Store* store = nullptr;
static lv_obj_t* scr = nullptr;
static lv_obj_t* help = nullptr;  // ventana de la «?»

static void onBack(lv_event_t*) {
  if (hooks.back) hooks.back();
}

// ---------------------------------------------------------------- piezas
static void backButton() {
  lv_obj_t* back = ui::box(scr, 12, 5, 46, 46);
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
  lv_obj_add_event_cb(back, onBack, LV_EVENT_CLICKED, nullptr);
}

static void avatar(lv_obj_t* parent, const jug::Player& p, int size) {
  char rel[40];
  const lv_image_dsc_t* pic = store->photoRel(p.code, rel, sizeof(rel)) ? assets::photo(rel, size, size / 2) : nullptr;
  if (pic) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, pic);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return;
  }
  lv_obj_t* b = ui::box(parent, 0, 0, size, size);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, ui::hex(0x1d3a63), 0);
  lv_obj_set_style_bg_grad_color(b, ui::hex(0x0f2140), 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, ui::hex(ui::color::LINE), 0);
  char ini[8];
  jug::initials(p.name, ini, sizeof(ini));
  lv_obj_t* l = ui::text(b, ini, 800, 12, ui::color::TEXT);
  lv_obj_center(l);
}

// ✓ verde (la letra no lo trae).
static void tick(lv_obj_t* parent, int x, int y) {
  static lv_point_precise_t pts[3] = {{1, 6}, {4, 9}, {10, 1}};
  lv_obj_t* l = lv_line_create(parent);
  lv_line_set_points(l, pts, 3);
  lv_obj_set_style_line_width(l, 2, 0);
  lv_obj_set_style_line_rounded(l, true, 0);
  lv_obj_set_style_line_color(l, ui::hex(ui::color::OK), 0);
  lv_obj_set_pos(l, x, y);
}

// ---------------------------------------------------------------- ayuda
static void onHelpClose(lv_event_t*) {
  if (help) lv_obj_delete(help);
  help = nullptr;
}
static void onHelp(lv_event_t*) {
  if (help) return;
  help = ui::box(scr, 0, 0, 800, 480);
  lv_obj_add_flag(help, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(help, ui::hex(0x02050a), 0);
  lv_obj_set_style_bg_opa(help, 190, 0);
  lv_obj_add_event_cb(help, onHelpClose, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* card = ui::box(help, 90, 90, 620, 300);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);  // tocar dentro no cierra
  lv_obj_set_style_radius(card, ui::RADIUS, 0);
  lv_obj_set_style_bg_color(card, ui::hex(ui::color::SURFACE), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 2, 0);
  lv_obj_set_style_border_color(card, ui::hex(ui::color::ACCENT), 0);
  lv_obj_t* t = ui::text(card, "Retos", 900, 24, ui::color::ACCENT);
  lv_obj_set_pos(t, 24, 22);
  lv_obj_t* p = ui::text(card, "1 reto diario (+40 XP) y 3 semanales (+100 XP). Solo cuentan los partidos guardados.", 400, 22,
                         ui::color::TEXT);
  lv_label_set_long_mode(p, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_width(p, 572);
  lv_obj_set_pos(p, 24, 62);
  lv_obj_t* ok = ui::button(card, 180, 216, 260, 64, "ENTENDIDO", 20, ui::BtnKind::Primary, onHelpClose);
  (void)ok;
}
static void helpButton() {
  lv_obj_t* b = ui::box(scr, 744, 6, 44, 44);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, ui::hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(b, 15, 0);
  lv_obj_set_style_bg_opa(b, 50, LV_STATE_PRESSED);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, ui::hex(0x8e98a6), 0);  // blanco al 35 % sobre el fondo
  lv_obj_t* q = ui::text(b, "?", 900, 20, ui::color::TEXT);
  lv_obj_center(q);
  lv_obj_add_event_cb(b, onHelp, LV_EVENT_CLICKED, nullptr);
}

// ---------------------------------------------------------------- pantalla
static void buildCards(const progresion::Challenge* ch, int nch, uint32_t now) {
  reloj::Clock lc = partidos::localOf(now);
  long left = 86400L - ((long)lc.hour * 3600 + lc.minute * 60 + lc.second);
  int hoursLeft = (int)((left + 1800) / 3600);  // Math.round de la app
  for (int c = 0; c < nch; c++) {
    bool daily = ch[c].daily;
    uint32_t col = daily ? ui::color::RANKED : ui::color::ACCENT;
    lv_obj_t* card = ui::box(scr, 16 + c * 194, 68, 186, 110);
    lv_obj_set_style_radius(card, ui::RADIUS, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, ui::hex(col), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    // Degradado de la app: el color del reto al 20 % (16 % el azul) que se funde con la tarjeta.
    lv_obj_set_style_bg_color(card, ui::hex(daily ? 0x3e433e : 0x1e3f5a), 0);
    lv_obj_set_style_bg_grad_color(card, ui::hex(ui::color::CARD), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    char scope[32];
    if (daily) snprintf(scope, sizeof(scope), "HOY · quedan %d h", hoursLeft);
    else snprintf(scope, sizeof(scope), "ESTA SEMANA");
    lv_obj_t* s = ui::text(card, scope, 900, 13, ui::color::TEXT2, 1.56f);
    lv_obj_set_pos(s, 11, 10);
    char txt[64];
    snprintf(txt, sizeof(txt), progresion::TPL_TEXT[ch[c].tpl], (unsigned)ch[c].target);
    lv_obj_t* t = ui::text(card, txt, 800, 13, ui::color::TEXT);
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(t, 164);
    lv_obj_set_pos(t, 11, 30);
    char xp[16];
    snprintf(xp, sizeof(xp), "+%d XP", ch[c].xp);
    lv_obj_t* x = ui::text(card, xp, 900, 13, col);
    lv_obj_set_pos(x, 11, 82);
  }
}

static void onRow(lv_event_t* e) {
  if (hooks.openPlayer) hooks.openPlayer((uint32_t)(uintptr_t)lv_event_get_user_data(e));
}
static void buildRow(lv_obj_t* list, int y, const jug::Player& p, const progresion::Challenge* ch, int nch, uint32_t now) {
  lv_obj_t* row = ui::box(list, 0, y, 768, 52);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(row, ui::hex(ui::color::CARD2), LV_STATE_PRESSED);
  lv_obj_add_event_cb(row, onRow, LV_EVENT_CLICKED, (void*)(uintptr_t)p.code);
  lv_obj_set_style_radius(row, ui::RADIUS_SM, 0);
  lv_obj_set_style_bg_color(row, ui::hex(ui::color::CARD), 0);
  lv_obj_set_style_bg_opa(row, 191, 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_color(row, ui::hex(ui::color::LINE), 0);
  lv_obj_set_style_border_opa(row, 179, 0);
  lv_obj_t* a = ui::box(row, 10, 9, 32, 32);
  avatar(a, p, 32);
  lv_obj_t* n = ui::text(row, p.name, 800, 17, ui::color::TEXT);
  lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(n, 110);
  lv_obj_set_pos(n, 52, 14);
  int value[4] = {0, 0, 0, 0};
  bool done[4] = {false, false, false, false};
  progresion::challengeProgress(p.code, ch, nch, now, value, done);
  for (int c = 0; c < nch; c++) {
    int x0 = 172 + (int)(c * 148.5f);
    const int W = 138;
    int textW = 11;
    lv_obj_t* label = nullptr;
    if (!done[c]) {
      char v[16];
      snprintf(v, sizeof(v), "%d/%u", value[c], (unsigned)ch[c].target);
      label = ui::text(row, v, 400, 13, ui::color::TEXT);
      lv_obj_update_layout(label);
      textW = lv_obj_get_width(label);
    }
    int trackW = W - 6 - textW;
    lv_obj_t* track = ui::box(row, x0, 21, trackW, 8);
    lv_obj_set_style_radius(track, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(track, ui::hex(ui::color::LINE), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(track, true, 0);
    int fillW = ch[c].target ? trackW * value[c] / ch[c].target : 0;
    if (fillW > 0) {
      lv_obj_t* fill = ui::box(track, 0, 0, fillW, 8);
      lv_obj_set_style_bg_color(fill, ui::hex(done[c] ? ui::color::OK : ui::color::ACCENT), 0);
      lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    }
    if (label) lv_obj_set_pos(label, x0 + trackW + 6, 17);
    else tick(row, x0 + trackW + 6, 20);
  }
}

static void build() {
  ui::gradient(scr, 0x0d1d36, 0x04080f);
  lv_obj_t* head = ui::box(scr, 0, 0, 800, 56);
  lv_obj_set_style_bg_color(head, ui::hex(ui::color::BG_DEEP), 0);
  lv_obj_set_style_bg_opa(head, 140, 0);
  backButton();
  lv_obj_t* t = ui::text(scr, "RETOS", 900, 20, ui::color::TEXT, 1.14f);
  lv_obj_set_pos(t, 70, 15);
  lv_obj_update_layout(t);
  lv_obj_t* sub = ui::text(scr, "DIARIOS Y SEMANALES", 600, 13, ui::color::TEXT2, 0.52f);
  lv_obj_set_pos(sub, 70 + lv_obj_get_width(t) + 8, 21);
  helpButton();

  uint32_t now = hooks.nowUtc ? hooks.nowUtc() : 0;
  auto center = [](const char* title, const char* text) {
    lv_obj_t* a = ui::text(scr, title, 800, 18, ui::color::TEXT);
    lv_obj_set_width(a, 768);
    lv_obj_set_style_text_align(a, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(a, 16, 304);
    lv_obj_t* b = ui::text(scr, text, 400, 16, ui::color::TEXT2);
    lv_obj_set_width(b, 768);
    lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(b, 16, 331);
  };
  if (!now) {  // sin hora no se sabe qué día es (la app siempre la tiene)
    center("Hora sin ajustar", "Pon la placa en hora para ver los retos de hoy.");
    return;
  }
  progresion::Challenge ch[4];
  int nch = progresion::activeChallenges(now, ch);
  buildCards(ch, nch, now);
  int y = 188;
  if (!progresion::rules.challenges) {
    lv_obj_t* w = ui::text(scr, "Los retos están desactivados en las reglas del juego: no dan experiencia.", 700, 14,
                           ui::color::RANKED);
    lv_obj_set_pos(w, 16, y);
    y += 26;
  }
  if (!store || store->count == 0) {
    center("Sin jugadores", "Crea jugadores para seguir su progreso.");
    return;
  }
  // Por nombre, como sortPlayers de la app.
  int order[jug::MAX];
  int n = store->count;
  for (int i = 0; i < n; i++) order[i] = i;
  qsort(order, (size_t)n, sizeof(int), [](const void* a, const void* b) {
    return torneos::rg::collate(store->list[*(const int*)a].name, store->list[*(const int*)b].name);
  });
  lv_obj_t* list = ui::box(scr, 16, y, 768, 480 - 12 - y);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
  for (int k = 0; k < n; k++) buildRow(list, k * 58, store->list[order[k]], ch, nch, now);
}

static void onDelete(lv_event_t*) {
  scr = nullptr;
  help = nullptr;
}

static bool active() { return scr && lv_screen_active() == scr; }

static void show(jug::Store* s) {
  store = s;
  lv_obj_t* old = scr;
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  help = nullptr;
  build();
  if (old) lv_obj_remove_event_cb(old, onDelete);
  ui::show(scr);
}

}  // namespace retos

#endif  // MFV3_UI_RETOS_H
