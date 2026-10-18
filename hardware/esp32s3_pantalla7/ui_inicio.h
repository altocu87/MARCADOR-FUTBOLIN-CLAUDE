// MARCADOR FUTBOLÍN V3 · pantalla de Inicio, igual que la app (src/ui/screens/HomeScreen.tsx).
// ------------------------------------------------------------------------------------------
// - Tres tarjetas de modo (RÁPIDO, LOCO, CLASIFICATORIO). Dos toques: el primero la enciende (su ilustración
//   «encendida» y el resplandor aparecen en 0,25 s) y el segundo, sobre la misma, entra en el modo tras verse
//   hundida 180 ms. Sin tocar nada, la tarjeta encendida se apaga a los 8 s.
// - Cabecera: TORNEO, RANKING, RETOS (se hunden 180 ms al tocarlas) y el engranaje de Ajustes.
// - «?» de LOCO y CLASIFICATORIO: la explicación en un globo; cualquier toque la cierra.
// Todo lo que se ve son capas fotografiadas de la app (scripts/placa-capas.mjs): no hay nada que cambie solo.

#ifndef MFV3_UI_INICIO_H
#define MFV3_UI_INICIO_H

#include "ui_base.h"

namespace inicio {

enum Mode : uint8_t { QUICK = 0, CHAOS = 1, RANKED = 2 };
enum Menu : uint8_t { TOURNAMENT = 0, RANKING = 1, CHALLENGES = 2 };

struct Hooks {
  void (*mode)(uint8_t mode) = nullptr;   // entrar en un modo (configuración del partido)
  void (*menu)(uint8_t which) = nullptr;  // TORNEO / RANKING / RETOS
  void (*settings)() = nullptr;           // engranaje
};
static Hooks hooks;

static const uint32_t SELECT_TIMEOUT_MS = 8000;
static const uint32_t PRESS_FEEDBACK_MS = 180;

static lv_obj_t* scr = nullptr;
static lv_obj_t *sel[3], *pul[3], *pulMenu[3], *help[3], *helpCloser;
static lv_obj_t *dot = nullptr, *warn = nullptr, *warnText = nullptr;
static int8_t selected = -1;
static bool pressing = false;
static lv_timer_t* selTimer = nullptr;
static lv_timer_t* pressTimer = nullptr;
static int8_t pressedMode = -1, pressedMenu = -1;

static const capas::Capa* SEL[3] = {&capas::INICIO_SEL_RAPIDO, &capas::INICIO_SEL_LOCO, &capas::INICIO_SEL_CLASIFICATORIO};
static const capas::Capa* PUL[3] = {&capas::INICIO_PUL_RAPIDO, &capas::INICIO_PUL_LOCO, &capas::INICIO_PUL_CLASIFICATORIO};
static const capas::Capa* PULM[3] = {&capas::INICIO_PUL_TORNEO, &capas::INICIO_PUL_RANKING, &capas::INICIO_PUL_RETOS};

static void deselect() {
  for (int i = 0; i < 3; i++) ui::hide(sel[i]);
  selected = -1;
}

static void onSelTimeout(lv_timer_t*) {
  selTimer = nullptr;
  if (!pressing) deselect();
}

static void onPressDone(lv_timer_t*) {
  pressTimer = nullptr;
  int8_t m = pressedMode, mn = pressedMenu;
  pressedMode = pressedMenu = -1;
  pressing = false;
  for (int i = 0; i < 3; i++) {
    ui::hide(pul[i]);
    ui::hide(pulMenu[i]);
  }
  deselect();
  if (m >= 0 && hooks.mode) hooks.mode((uint8_t)m);
  if (mn >= 0 && hooks.menu) hooks.menu((uint8_t)mn);
}

static void startPress() {
  pressing = true;
  pressTimer = lv_timer_create(onPressDone, PRESS_FEEDBACK_MS, nullptr);
  lv_timer_set_repeat_count(pressTimer, 1);
}

// Primer toque: la tarjeta se enciende. Segundo, sobre la misma: se hunde y se entra en el modo.
static void onCard(lv_event_t* e) {
  if (pressing) return;
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (selTimer) {
    lv_timer_delete(selTimer);
    selTimer = nullptr;
  }
  if (selected != i) {
    for (int k = 0; k < 3; k++)
      if (k != i) ui::hide(sel[k]);
    ui::reveal(sel[i]);  // de golpe (la app la funde en 0,25 s): en la placa el fundido iba a saltos y retrasaba el toque
    selected = (int8_t)i;
    selTimer = lv_timer_create(onSelTimeout, SELECT_TIMEOUT_MS, nullptr);
    lv_timer_set_repeat_count(selTimer, 1);
    return;
  }
  ui::reveal(pul[i]);
  pressedMode = (int8_t)i;
  startPress();
}

static void onMenu(lv_event_t* e) {
  if (pressing) return;
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  ui::reveal(pulMenu[i]);
  pressedMenu = (int8_t)i;
  startPress();
}

static void onSettings(lv_event_t*) {
  if (pressing) return;
  if (hooks.settings) hooks.settings();
}

static void onHelp(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  ui::reveal(help[i]);
  lv_obj_remove_flag(helpCloser, LV_OBJ_FLAG_HIDDEN);
}

static void onHelpClose(lv_event_t*) {
  for (int i = 1; i < 3; i++) ui::hide(help[i]);
  lv_obj_add_flag(helpCloser, LV_OBJ_FLAG_HIDDEN);
}

// Al salir de Inicio la pantalla se borra: se olvidan sus objetos y temporizadores.
static void onDelete(lv_event_t*) {
  if (selTimer) lv_timer_delete(selTimer);
  if (pressTimer) lv_timer_delete(pressTimer);
  selTimer = pressTimer = nullptr;
  scr = nullptr;
}

static void build() {
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  ui::layer(scr, capas::INICIO);
  for (int i = 0; i < 3; i++) sel[i] = ui::layer(scr, *SEL[i], false);
  for (int i = 0; i < 3; i++) pul[i] = ui::layer(scr, *PUL[i], false);
  for (int i = 0; i < 3; i++) pulMenu[i] = ui::layer(scr, *PULM[i], false);
  help[0] = nullptr;
  help[1] = ui::layer(scr, capas::INICIO_AYUDA_LOCO, false);
  help[2] = ui::layer(scr, capas::INICIO_AYUDA_CLASIFICATORIO, false);
  // Zonas táctiles con las medidas de la app (docs/capturas-app/inicio.json).
  for (int i = 0; i < 3; i++) ui::hotspot(scr, 20 + 260 * i, 92, 240, 370, onCard, (void*)(intptr_t)i);
  ui::hotspot(scr, 470, 102, 40, 40, onHelp, (void*)(intptr_t)1);
  ui::hotspot(scr, 730, 102, 40, 40, onHelp, (void*)(intptr_t)2);
  const int menuX[3] = {230, 393, 555};
  for (int i = 0; i < 3; i++) ui::hotspot(scr, menuX[i], 14, 153, 64, onMenu, (void*)(intptr_t)i);
  ui::hotspot(scr, 720, 16, 60, 60, onSettings);
  // Estado de la placa (lo que en la app es el punto del engranaje y la pastilla de aviso de la cabecera):
  // punto verde = mando conectado, dorado = hay versión nueva (en Ajustes); aviso = batería baja.
  dot = ui::box(scr, 758, 28, 10, 10);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(dot, 8, 0);
  lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
  warn = ui::box(scr, 0, 446, 10, 28);
  lv_obj_set_width(warn, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(warn, 10, 0);
  lv_obj_set_style_radius(warn, 999, 0);
  lv_obj_set_style_bg_color(warn, ui::hex(0x0b1626), 0);
  lv_obj_set_style_bg_opa(warn, LV_OPA_90, 0);
  lv_obj_set_style_border_width(warn, 1, 0);
  lv_obj_set_style_border_color(warn, ui::hex(ui::color::DANGER), 0);
  warnText = ui::label(warn, "", 800, 13, ui::color::DANGER);
  lv_obj_set_style_text_letter_space(warnText, 1, 0);
  lv_obj_center(warnText);
  lv_obj_add_flag(warn, LV_OBJ_FLAG_HIDDEN);
  helpCloser = ui::hotspot(scr, 0, 0, 800, 480, onHelpClose);
  lv_obj_add_flag(helpCloser, LV_OBJ_FLAG_HIDDEN);
  selected = -1;
  pressing = false;
}

// Enseña Inicio (la crea si hace falta). Las capas se descomprimen aquí, no al tocar.
static void show() {
  if (scr && lv_screen_active() == scr) return;
  build();
  ui::show(scr);
}

static bool active() { return scr && lv_screen_active() == scr; }

// Mando conectado / versión nueva / aviso (nullptr = ninguno).
static void setStatus(bool mandoLinked, bool update, const char* warning) {
  if (!active()) return;
  // Se llama en cada vuelta: solo se toca lo que cambia (cambiar un color, aunque sea al mismo, lo repinta).
  if (mandoLinked || update) {
    lv_color_t c = ui::hex(update ? ui::color::RANKED : ui::color::OK);
    if (!lv_color_eq(lv_obj_get_style_bg_color(dot, LV_PART_MAIN), c)) {
      lv_obj_set_style_bg_color(dot, c, 0);
      lv_obj_set_style_shadow_color(dot, c, 0);
    }
    ui::setVisible(dot, true);
  } else {
    ui::setVisible(dot, false);
  }
  if (warning) {
    if (strcmp(lv_label_get_text(warnText), warning)) {
      lv_label_set_text(warnText, warning);
      lv_obj_update_layout(warn);
      lv_obj_set_x(warn, 400 - lv_obj_get_width(warn) / 2);
    }
    ui::setVisible(warn, true);
  } else {
    ui::setVisible(warn, false);
  }
}
static bool dotShown() { return dot && !lv_obj_has_flag(dot, LV_OBJ_FLAG_HIDDEN); }

}  // namespace inicio

#endif  // MFV3_UI_INICIO_H
