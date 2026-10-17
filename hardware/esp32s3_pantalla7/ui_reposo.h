// MARCADOR FUTBOLÍN V3 · reposo, igual que SleepOverlay de la app (src/ui/components/SleepOverlay.tsx).
// ---------------------------------------------------------------------------------------------------
// Fondo casi negro, reloj de segmentos que ocupa casi todo el ancho (los dos puntos laten con los segundos),
// barra de 60 marcas que se encienden una por segundo (la actual, naranja, da un destello) con los segundos al
// lado, la fecha, el último partido y «Toca para despertar». El primer toque solo despierta.

#ifndef MFV3_UI_REPOSO_H
#define MFV3_UI_REPOSO_H

#include "ui_base.h"
#include "ui_sevenseg.h"

namespace reposo {

static lv_obj_t* scr = nullptr;
static lv_obj_t *clock, *ticks[60], *sec, *date, *last, *hint, *battery;
static int lastSec = -1, lastMinute = -1;
static char lastText[64] = "";

static const char* const DIAS[7] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};
static const char* const MESES[12] = {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio",
                                      "agosto", "septiembre", "octubre", "noviembre", "diciembre"};

// Marcas: 60 en 662 px (de 40 a 702), 6 × 12 px; cada cinco, 18 px de alto. Centradas en y = 322.
static void styleTick(int i, int s) {
  lv_obj_t* t = ticks[i];
  if (i < s) {
    lv_obj_set_style_bg_color(t, ui::hex(0x4fd8ff), 0);
    lv_obj_set_style_bg_opa(t, 140, 0);
    lv_obj_set_style_shadow_width(t, 6, 0);
    lv_obj_set_style_shadow_color(t, ui::hex(0x4fd8ff), 0);
    lv_obj_set_style_shadow_opa(t, 128, 0);
  } else if (i == s) {
    lv_obj_set_style_bg_color(t, ui::hex(0xff9a3c), 0);
    lv_obj_set_style_bg_opa(t, 255, 0);
    lv_obj_set_style_shadow_width(t, 18, 0);
    lv_obj_set_style_shadow_color(t, ui::hex(0xff9a3c), 0);
    lv_obj_set_style_shadow_opa(t, 204, 0);
  } else {
    lv_obj_set_style_bg_color(t, ui::hex(0x4fd8ff), 0);
    lv_obj_set_style_bg_opa(t, 26, 0);
    lv_obj_set_style_shadow_width(t, 0, 0);
  }
}

// Destello de la marca actual (tick-pop: crece en alto y vuelve en 0,6 s).
static void popCb(void* o, int32_t v) { lv_obj_set_style_transform_scale_y((lv_obj_t*)o, v, 0); }

static void onDelete(lv_event_t*) { scr = nullptr; }

static void build() {
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  lv_obj_set_style_bg_color(scr, ui::hex(0x020407), 0);
  clock = sevenseg::create(scr, 42, 42, 250, "00:00", 0x4fd8ff, 220);
  for (int i = 0; i < 60; i++) {
    int h = (i % 5 == 4) ? 18 : 12;
    int x = 40 + (int)lroundf(i * (662.0f - 6) / 59);
    ticks[i] = ui::box(scr, x, 322 - h / 2, 6, h);
    lv_obj_set_style_radius(ticks[i], 3, 0);
    lv_obj_set_style_transform_pivot_y(ticks[i], h / 2, 0);
  }
  sec = ui::text(scr, "00", 900, 26, 0xff9a3c);
  date = ui::text(scr, "", 400, 22, 0x5d7f9c, 1.32f);
  last = ui::text(scr, "", 700, 15, 0x34506a, 0.9f);
  lv_label_set_recolor(last, true);
  hint = ui::text(scr, "TOCA PARA DESPERTAR", 400, 12, 0x23344a, 1.68f);
  ui::place(hint, 400 + 0.84f, 447, 0);
  // Solo en la placa (la app no tiene batería): aviso en rojo sobre «Toca para despertar».
  battery = ui::text(scr, "BATERÍA BAJA: CONECTA EL CARGADOR", 800, 13, ui::color::DANGER, 1.3f);
  ui::place(battery, 400, 424, 0);
  lv_obj_add_flag(battery, LV_OBJ_FLAG_HIDDEN);
  lastSec = lastMinute = -1;
  lastText[0] = 0;
}

static bool active() { return scr && lv_screen_active() == scr; }

// `lastMatch`: «Último: BLANCO 5 – 3 AZUL» o vacío. Fecha: día de la semana (0 = domingo), día y mes.
static void update(uint8_t hour, uint8_t minute, uint8_t second, uint8_t weekday, uint8_t day, uint8_t month,
                   const char* lastMatch, bool lowBattery = false) {
  if (!active()) {
    build();
    ui::show(scr);
  }
  char b[64];
  if (minute != lastMinute || lastSec < 0 || (second & 1) != (lastSec & 1)) {
    snprintf(b, sizeof(b), "%02u:%02u", hour, minute);
    sevenseg::setText(clock, b, second & 1);
  }
  if (minute != lastMinute) {
    snprintf(b, sizeof(b), "%s, %u de %s", DIAS[weekday % 7], day, MESES[(month + 11) % 12]);
    lv_label_set_text(date, b);
    ui::place(date, 400 + 0.66f, 355.4f, 26);
    lastMinute = minute;
  }
  if (second != lastSec) {
    for (int i = 0; i < 60; i++) styleTick(i, second);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ticks[second % 60]);
    lv_anim_set_exec_cb(&a, popCb);
    lv_anim_set_values(&a, 563, 256);  // scaleY(2,2) → 1
    lv_anim_set_duration(&a, 600);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    snprintf(b, sizeof(b), "%02u", second);
    lv_label_set_text(sec, b);
    ui::place(sec, 764.2f, 306.2f, 0, ui::Align::Right);
    lastSec = second;
  }
  if (lowBattery) lv_obj_remove_flag(battery, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(battery, LV_OBJ_FLAG_HIDDEN);
  if (strcmp(lastMatch ? lastMatch : "", lastText)) {
    snprintf(lastText, sizeof(lastText), "%s", lastMatch ? lastMatch : "");
    lv_label_set_text(last, lastText);
    ui::place(last, 400, 402, 19);
  }
}

}  // namespace reposo

#endif  // MFV3_UI_REPOSO_H
