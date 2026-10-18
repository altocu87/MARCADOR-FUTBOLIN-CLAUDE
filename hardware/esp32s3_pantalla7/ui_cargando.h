// MARCADOR FUTBOLÍN V3 · «Cargando…» al encender (Alex, 18/10/2026).
// ---------------------------------------------------------------------------------------------------
// Mientras la placa prepara las imágenes, las letras y las fichas de los jugadores (unos segundos), se ve el nombre,
// una barra y el porcentaje. Cada paso repinta solo la barra y el número.

#ifndef MFV3_UI_CARGANDO_H
#define MFV3_UI_CARGANDO_H

#include "ui_base.h"

namespace cargando {

static lv_obj_t *scr = nullptr, *bar = nullptr, *pct = nullptr;
static int total = 1, done = 0, shownPct = -1;

static void show(int steps) {
  total = steps > 0 ? steps : 1;
  done = 0;
  shownPct = -1;
  scr = ui::screen();
  lv_obj_t* t = ui::text(scr, "MARCADOR FUTBOLÍN", 900, 26, ui::color::TEXT, 2.6f);
  ui::place(t, 400, 170, 32);
  bar = lv_bar_create(scr);
  lv_obj_set_size(bar, 420, 12);
  lv_obj_set_pos(bar, 190, 236);
  lv_bar_set_range(bar, 0, 100);
  lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, ui::hex(ui::color::CARD), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 6, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(bar, ui::hex(ui::color::ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
  pct = ui::text(scr, "Cargando… 0 %", 400, 16, ui::color::TEXT2);
  ui::place(pct, 400, 266, 22);
  ui::show(scr);
  lv_refr_now(nullptr);
}

// Un paso más hecho: si el porcentaje cambia, se repinta (la barra y el número).
static void step() {
  if (!scr) return;
  done++;
  int p = done * 100 / total;
  if (p > 100) p = 100;
  if (p == shownPct) return;
  shownPct = p;
  lv_bar_set_value(bar, p, LV_ANIM_OFF);
  char b[32];
  snprintf(b, sizeof(b), "Cargando… %d %%", p);
  lv_label_set_text(pct, b);
  ui::place(pct, 400, 266, 22);
  lv_refr_now(nullptr);
}

// Fin: la pantalla de carga se borra al enseñarse la primera de verdad.
static void finish() {
  scr = bar = pct = nullptr;
}

}  // namespace cargando

#endif  // MFV3_UI_CARGANDO_H
