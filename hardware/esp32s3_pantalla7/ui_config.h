// MARCADOR FUTBOLÍN V3 · Configuración del partido, igual que la app (src/ui/screens/SetupScreen.tsx).
// ------------------------------------------------------------------------------------------------
// Dos tarjetas: GOLES PARA GANAR y MINUTOS POR PARTE. ▲/▼ suman y restan (solo en la tarjeta encendida y dentro
// de los límites: goles 1–20, minutos 1–30); tocar el número enciende o apaga la tarjeta (siempre queda al menos
// una: por goles, por tiempo o ambas). CONTINUAR sigue al partido. Arriba: volver, el modo y la hora.
// Fondo, tarjetas, botones pulsados y cifras salen de la app (scripts/placa-capas.mjs); el reloj es ui_sevenseg.h.

#ifndef MFV3_UI_CONFIG_H
#define MFV3_UI_CONFIG_H

#include "ui_base.h"
#include "ui_sevenseg.h"

namespace config {

struct State {
  uint8_t mode;  // inicio::QUICK, CHAOS, RANKED
  uint8_t goals, minutes;
  bool useGoals, useTime;
};

struct Hooks {
  void (*back)() = nullptr;
  void (*next)() = nullptr;
  void (*changed)(const State&) = nullptr;   // al cambiar cualquier valor (se guarda en la NVS)
  void (*refused)() = nullptr;              // intento de apagar la única tarjeta encendida
};
static Hooks hooks;
static State st;

const uint8_t GOALS_MIN = 1, GOALS_MAX = 20, MIN_MIN = 1, MIN_MAX = 30;

// Posición de las cifras (medidas de la app: docs/capturas-app/configuracion.json y config-num-*).
const int DIGIT_X1 = 102, DIGIT_Y = 125;  // una cifra (recorte con 24 px de margen para el brillo)
const int DIGIT_X2 = 41;                   // primera de dos cifras
const int DIGIT_ADV = 123;                 // avance de cada cifra (tabular-nums)
const int RIGHT_DX = 391;                  // de la tarjeta de goles a la de minutos

static lv_obj_t* scr = nullptr;
static lv_obj_t *modeLayer[3], *leftOff, *rightOn, *limit[2][2], *pressed[2][2], *pressedNext, *clock;
static lv_obj_t* digits[2][2];
static uint32_t lastClockKey = 0xFFFFFFFF;
static int drawnKey[2] = {-1, -1};  // número y encendido que enseña cada tarjeta (para no repintar la otra)

static const capas::Capa* DIG_ON[10] = {&capas::CONFIG_CIFRA_ON_0, &capas::CONFIG_CIFRA_ON_1, &capas::CONFIG_CIFRA_ON_2,
                                        &capas::CONFIG_CIFRA_ON_3, &capas::CONFIG_CIFRA_ON_4, &capas::CONFIG_CIFRA_ON_5,
                                        &capas::CONFIG_CIFRA_ON_6, &capas::CONFIG_CIFRA_ON_7, &capas::CONFIG_CIFRA_ON_8,
                                        &capas::CONFIG_CIFRA_ON_9};
static const capas::Capa* DIG_OFF[10] = {&capas::CONFIG_CIFRA_OFF_0, &capas::CONFIG_CIFRA_OFF_1, &capas::CONFIG_CIFRA_OFF_2,
                                         &capas::CONFIG_CIFRA_OFF_3, &capas::CONFIG_CIFRA_OFF_4, &capas::CONFIG_CIFRA_OFF_5,
                                         &capas::CONFIG_CIFRA_OFF_6, &capas::CONFIG_CIFRA_OFF_7, &capas::CONFIG_CIFRA_OFF_8,
                                         &capas::CONFIG_CIFRA_OFF_9};

static bool on(int side) { return side == 0 ? st.useGoals : st.useTime; }
static uint8_t value(int side) { return side == 0 ? st.goals : st.minutes; }
static bool atMax(int side) { return side == 0 ? st.goals >= GOALS_MAX : st.minutes >= MIN_MAX; }
static bool atMin(int side) { return side == 0 ? st.goals <= GOALS_MIN : st.minutes <= MIN_MIN; }

static void setVisible(lv_obj_t* o, bool v) { ui::setVisible(o, v); }

// Número de una tarjeta con las cifras de la app (una o dos, centradas igual que allí).
static void drawNumber(int side) {
  uint8_t v = value(side);
  int key = v * 2 + (on(side) ? 1 : 0);
  if (key == drawnKey[side]) return;  // igual que antes: no se toca (cambiar la imagen la repinta aunque sea la misma)
  drawnKey[side] = key;
  const capas::Capa** set = on(side) ? DIG_ON : DIG_OFF;
  int dx = side ? RIGHT_DX : 0;
  if (v < 10) {
    lv_image_set_src(digits[side][0], assets::src(*set[v]));
    lv_obj_set_pos(digits[side][0], DIGIT_X1 + dx, DIGIT_Y);
    setVisible(digits[side][1], false);
  } else {
    lv_image_set_src(digits[side][0], assets::src(*set[v / 10]));
    lv_image_set_src(digits[side][1], assets::src(*set[v % 10]));
    lv_obj_set_pos(digits[side][0], DIGIT_X2 + dx, DIGIT_Y);
    lv_obj_set_pos(digits[side][1], DIGIT_X2 + DIGIT_ADV + dx, DIGIT_Y);
    setVisible(digits[side][1], true);
  }
  setVisible(digits[side][0], true);
}

static void refresh() {
  // Cada tarjeta con su imagen de encendida o apagada: al tocar una solo se repinta esa.
  setVisible(leftOff, !st.useGoals);
  setVisible(rightOn, st.useTime);
  for (int s = 0; s < 2; s++) {
    setVisible(limit[s][0], on(s) && atMax(s));
    setVisible(limit[s][1], on(s) && atMin(s));
    drawNumber(s);
  }
}

// ▲ / ▼: cambian el valor al pulsar (no al soltar, como en la app: en la placa se notaba el retraso) y, si se
// mantienen, siguen subiendo o bajando solos. Se ven pulsados mientras se mantienen y se pueden usar.
static void step(int side, int down) {
  uint8_t& v = side ? st.minutes : st.goals;
  v = (uint8_t)(v + (down ? -1 : 1));
  refresh();
  if (down ? atMin(side) : atMax(side)) setVisible(pressed[side][down], false);  // ya en el tope
  if (hooks.changed) hooks.changed(st);
}
static void onStep(lv_event_t* e) {
  int code = (int)(intptr_t)lv_event_get_user_data(e);
  int side = code >> 1, down = code & 1;
  bool usable = on(side) && !(down ? atMin(side) : atMax(side));
  lv_event_code_t ev = lv_event_get_code(e);
  if (ev == LV_EVENT_PRESSED) {
    setVisible(pressed[side][down], usable);
    if (usable) step(side, down);
  } else if (ev == LV_EVENT_LONG_PRESSED_REPEAT) {
    if (usable) step(side, down);
  } else if (ev == LV_EVENT_RELEASED || ev == LV_EVENT_PRESS_LOST) {
    setVisible(pressed[side][down], false);
  }
}

// Número: al pulsarlo enciende o apaga la tarjeta (al momento; la app además lo encoge un poco mientras se pulsa,
// pero encoger una imagen con brillo en la placa costaba repintarla dos veces).
static void onBox(lv_event_t* e) {
  int side = (int)(intptr_t)lv_event_get_user_data(e);
  if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;
  bool goals = side == 0 ? !st.useGoals : st.useGoals;
  bool time = side == 1 ? !st.useTime : st.useTime;
  if (!goals && !time) {
    if (hooks.refused) hooks.refused();
    return;
  }
  st.useGoals = goals;
  st.useTime = time;
  refresh();
  if (hooks.changed) hooks.changed(st);
}

static void onNext(lv_event_t* e) {
  lv_event_code_t ev = lv_event_get_code(e);
  if (ev == LV_EVENT_PRESSED) setVisible(pressedNext, true);
  else if (ev == LV_EVENT_RELEASED || ev == LV_EVENT_PRESS_LOST) setVisible(pressedNext, false);
  else if (ev == LV_EVENT_CLICKED && hooks.next) hooks.next();
}

static void onBack(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED && hooks.back) hooks.back();
}

static lv_obj_t* zone(int x, int y, int w, int h, lv_event_cb_t cb, intptr_t user) {
  lv_obj_t* o = ui::box(scr, x, y, w, h);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, cb, LV_EVENT_ALL, (void*)user);
  return o;
}

// Cifras preparadas de una en una al entrar (encendidas y apagadas, 20): así al tocar no hay que descomprimir
// ninguna. Una cada 50 ms para no parar la pantalla; se quedan mientras se esté en esta pantalla.
static lv_timer_t* warmTimer = nullptr;
static int warmNext = 0;
static void warm(lv_timer_t*) {
  if (warmNext < 20) {
    assets::preload(warmNext < 10 ? *DIG_ON[warmNext] : *DIG_OFF[warmNext - 10]);
    warmNext++;
  }
  if (warmNext >= 20 && warmTimer) {
    lv_timer_delete(warmTimer);
    warmTimer = nullptr;
  }
}

static void onDelete(lv_event_t*) {
  scr = nullptr;
  if (warmTimer) lv_timer_delete(warmTimer);
  warmTimer = nullptr;
}

static void build() {
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  ui::layer(scr, capas::CONFIG);
  modeLayer[1] = ui::layer(scr, capas::CONFIG_MODO_LOCO, st.mode == 1);
  modeLayer[2] = ui::layer(scr, capas::CONFIG_MODO_CLASIFICATORIO, st.mode == 2);
  rightOn = ui::layer(scr, capas::CONFIG_AMBAS, false);       // minutos encendida
  leftOff = ui::layer(scr, capas::CONFIG_IZQ_APAGADA, false);  // goles apagada
  limit[0][0] = ui::layer(scr, capas::CONFIG_IZQ_MAX, false);
  limit[0][1] = ui::layer(scr, capas::CONFIG_IZQ_MIN, false);
  limit[1][0] = ui::layer(scr, capas::CONFIG_DER_MAX, false);
  limit[1][1] = ui::layer(scr, capas::CONFIG_DER_MIN, false);
  pressed[0][0] = ui::layer(scr, capas::CONFIG_IZQ_PUL_MAS, false);
  pressed[0][1] = ui::layer(scr, capas::CONFIG_IZQ_PUL_MENOS, false);
  pressed[1][0] = ui::layer(scr, capas::CONFIG_DER_PUL_MAS, false);
  pressed[1][1] = ui::layer(scr, capas::CONFIG_DER_PUL_MENOS, false);
  pressedNext = ui::layer(scr, capas::CONFIG_PUL_CONTINUAR, false);
  drawnKey[0] = drawnKey[1] = -1;
  for (int s = 0; s < 2; s++)
    for (int i = 0; i < 2; i++) {
      digits[s][i] = lv_image_create(scr);
      lv_obj_remove_flag(digits[s][i], LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_size(digits[s][i], capas::CONFIG_CIFRA_ON_0.w, capas::CONFIG_CIFRA_ON_0.h);
      lv_image_set_pivot(digits[s][i], capas::CONFIG_CIFRA_ON_0.w / 2, capas::CONFIG_CIFRA_ON_0.h / 2);
    }
  clock = sevenseg::create(scr, 707, 14, 28, "00:00", 0x4fd8ff);
  // Zonas táctiles (medidas de la app).
  zone(12, 4, 48, 48, onBack, 0);
  for (int s = 0; s < 2; s++) {
    int x = 20 + s * RIGHT_DX;
    zone(x, 66, 369, 76, onStep, s << 1);
    zone(x, 142, 369, 192, onBox, s);
    zone(x, 334, 369, 76, onStep, (s << 1) | 1);
  }
  zone(275, 416, 250, 54, onNext, 0);
  refresh();
  lastClockKey = 0xFFFFFFFF;
  warmNext = 0;
  if (!warmTimer) warmTimer = lv_timer_create(warm, 50, nullptr);
}

static bool active() { return scr && lv_screen_active() == scr; }

static void show(const State& s) {
  if (active()) return;
  st = s;
  build();
  ui::show(scr);
}

// Hora de la esquina (los dos puntos se apagan los segundos impares, como en la app).
static void tickClock(bool known, uint8_t hour, uint8_t minute, uint8_t second) {
  if (!active()) return;
  uint32_t key = known ? (uint32_t)hour * 3600 + minute * 60 + (second & 1) : 0xFFFFFFFE;
  if (key == lastClockKey) return;
  lastClockKey = key;
  char t[8];
  if (known) snprintf(t, sizeof(t), "%02u:%02u", hour, minute);
  else snprintf(t, sizeof(t), "--:--");
  sevenseg::setText(clock, t, known && (second & 1));
}

}  // namespace config

#endif  // MFV3_UI_CONFIG_H
