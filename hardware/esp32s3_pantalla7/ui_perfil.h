// «PERFIL» por el USB: cuánto cuesta pintar la pantalla que se ve. La repinta entera varias veces y luego va
// quitando efectos (sombras, degradados, transparencias, textos, imágenes) y dice cuánto ha bajado con cada uno.
// Deja la pantalla estropeada: después se vuelve al inicio.
#ifndef MFV3_UI_PERFIL_H
#define MFV3_UI_PERFIL_H

#include <functional>
#include "ui_port.h"
#include "display/lv_display_private.h"
#include "core/lv_refr_private.h"
#include "core/lv_obj_draw_private.h"
#include "core/lv_obj_event_private.h"

namespace perfil {

static uint32_t paintUs(const lv_area_t* a) {
  const int N = 4;
  uint32_t total = 0;
  lv_refr_now(nullptr);
  for (int i = 0; i < N; i++) {
    if (a) lv_obj_invalidate_area(lv_screen_active(), a);
    else lv_obj_invalidate(lv_screen_active());
    uiport::perfWaitFrame = 0;
    uint32_t t0 = micros();
    lv_refr_now(nullptr);
    total += micros() - t0 - uiport::perfWaitFrame;  // sin la espera al retorno vertical
  }
  return total / N;
}

// Por qué objeto empieza LVGL a pintar la pantalla entera (lo de debajo de él no se pinta).
static void topObj(void (*out)(const char*)) {
  char b[96];
  lv_obj_t* scr = lv_screen_active();
  lv_area_t all = {0, 0, 799, 479};
  lv_obj_t* top = lv_refr_get_top_obj(&all, scr);
  lv_area_t c = {0, 0, -1, -1};
  if (top) lv_obj_get_coords(top, &c);
  for (uint32_t i = 0; i < lv_obj_get_child_count(scr); i++) {
    lv_obj_t* o = lv_obj_get_child(scr, i);
    if (!lv_obj_check_type(o, &lv_image_class) || lv_obj_get_width(o) < 800 || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) continue;
    lv_cover_check_info_t info;
    info.res = LV_COVER_RES_COVER;
    info.area = &all;
    lv_obj_send_event(o, LV_EVENT_COVER_CHECK, &info);
    lv_area_t oc;
    lv_obj_get_coords(o, &oc);
    snprintf(b, sizeof(b), "PERFIL   imagen %lu: cubre %d capa %d opa %u coords %ld,%ld-%ld,%ld", (unsigned long)i, (int)info.res,
             (int)lv_obj_get_layer_type(o), lv_obj_get_style_opa(o, LV_PART_MAIN), (long)oc.x1, (long)oc.y1, (long)oc.x2, (long)oc.y2);
    out(b);
  }
  snprintf(b, sizeof(b), "PERFIL   LVGL empieza a pintar por: %s %ldx%ld", !top ? "nada" : top == scr ? "la pantalla (color)" :
           lv_obj_check_type(top, &lv_image_class) ? "imagen" : "caja", (long)lv_area_get_width(&c), (long)lv_area_get_height(&c));
  out(b);
}

static void walk(lv_obj_t* o, const std::function<void(lv_obj_t*)>& f) {
  f(o);
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) walk(lv_obj_get_child(o, i), f);
}

static void run(void (*out)(const char*), const lv_area_t* a = nullptr) {
  lv_obj_t* scr = lv_screen_active();
  char b[96];
  topObj(out);
  uint32_t base = paintUs(a), last = base;
  snprintf(b, sizeof(b), "PERFIL pantalla entera: %lu ms", (unsigned long)(base / 1000));
  out(b);
  auto step = [&](const char* what, const std::function<void(lv_obj_t*)>& f) {
    walk(scr, f);
    walk(lv_layer_top(), f);
    uint32_t t = paintUs(a);
    snprintf(b, sizeof(b), "PERFIL - %s: %lu ms (ahorra %ld ms)", what, (unsigned long)(t / 1000), (long)((int32_t)(last - t) / 1000));
    out(b);
    last = t;
  };
  step("sombras", [](lv_obj_t* o) { if (lv_obj_get_style_shadow_width(o, LV_PART_MAIN)) lv_obj_set_style_shadow_width(o, 0, 0); });
  step("contornos (outline)", [](lv_obj_t* o) { if (lv_obj_get_style_outline_width(o, LV_PART_MAIN)) lv_obj_set_style_outline_width(o, 0, 0); });
  step("degradados", [](lv_obj_t* o) {
    if (lv_obj_get_style_bg_grad_dir(o, LV_PART_MAIN) != LV_GRAD_DIR_NONE) lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_NONE, 0);
    if (lv_obj_get_style_bg_grad(o, LV_PART_MAIN)) lv_obj_set_style_bg_grad(o, nullptr, 0);
  });
  step("transparencias de objetos", [](lv_obj_t* o) {
    if (lv_obj_get_style_opa(o, LV_PART_MAIN) < 255) lv_obj_set_style_opa(o, 255, 0);
  });
  step("fondos semitransparentes", [](lv_obj_t* o) {
    lv_opa_t v = lv_obj_get_style_bg_opa(o, LV_PART_MAIN);
    if (v > 0 && v < 255) lv_obj_set_style_bg_opa(o, 255, 0);
  });
  step("bordes", [](lv_obj_t* o) { if (lv_obj_get_style_border_width(o, LV_PART_MAIN)) lv_obj_set_style_border_width(o, 0, 0); });
  step("esquinas redondeadas", [](lv_obj_t* o) { if (lv_obj_get_style_radius(o, LV_PART_MAIN)) lv_obj_set_style_radius(o, 0, 0); });
  step("textos", [](lv_obj_t* o) { if (lv_obj_check_type(o, &lv_label_class)) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); });
  step("reloj de 7 segmentos", [](lv_obj_t* o) {
    if (lv_obj_get_user_data(o) && !lv_obj_check_type(o, &lv_image_class) && !lv_obj_check_type(o, &lv_label_class) &&
        lv_obj_get_child_count(o) == 0 && lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) == false && lv_obj_get_width(o) > 60)
      lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  });
  step("imágenes salvo el fondo", [&](lv_obj_t* o) {
    if (lv_obj_check_type(o, &lv_image_class) && lv_obj_get_width(o) < 800) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  });
  step("todo lo demás (queda el fondo)", [&](lv_obj_t* o) {
    if (o != scr && lv_obj_get_parent(o) == scr && !(lv_obj_check_type(o, &lv_image_class) && lv_obj_get_width(o) >= 800))
      lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  });
  // Lo que queda a la vista (en la pantalla y en las capas de encima).
  lv_obj_t* roots[3] = {scr, lv_layer_top(), lv_layer_sys()};
  for (int r = 0; r < 3; r++)
  for (uint32_t i = 0; i < lv_obj_get_child_count(roots[r]); i++) {
    lv_obj_t* o = lv_obj_get_child(roots[r], i);
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) continue;
    lv_area_t c;
    lv_obj_get_coords(o, &c);
    const char* type = lv_obj_check_type(o, &lv_image_class) ? "imagen" : lv_obj_check_type(o, &lv_label_class) ? "texto" : "caja";
    int cf = -1;
    if (lv_obj_check_type(o, &lv_image_class)) {
      const void* src = lv_image_get_src(o);
      if (src && lv_image_src_get_type(src) == LV_IMAGE_SRC_VARIABLE) cf = ((const lv_image_dsc_t*)src)->header.cf;
    }
    snprintf(b, sizeof(b), "PERFIL   %s %ld,%ld %ldx%ld cf %d opa %u img_opa %u bg_opa %u recolor %u scale %ld", type, (long)c.x1, (long)c.y1,
             (long)lv_area_get_width(&c), (long)lv_area_get_height(&c), cf, lv_obj_get_style_opa(o, LV_PART_MAIN),
             lv_obj_get_style_image_opa(o, LV_PART_MAIN), lv_obj_get_style_bg_opa(o, LV_PART_MAIN),
             lv_obj_get_style_image_recolor_opa(o, LV_PART_MAIN),
             lv_obj_check_type(o, &lv_image_class) ? (long)lv_image_get_scale(o) : 0L);
    out(b);
  }
  topObj(out);
  step("sin color de fondo de la pantalla", [&](lv_obj_t* o) { if (o == scr) lv_obj_set_style_bg_opa(o, 0, 0); });
  step("el fondo también", [&](lv_obj_t* o) { if (o != scr && lv_obj_get_parent(o) == scr) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); });
  lv_display_t* d = lv_display_get_default();
  snprintf(b, sizeof(b), "PERFIL   pantallas cargadas %lu, anterior %p, capa de abajo %lu hijos", (unsigned long)d->screen_cnt,
           (void*)lv_display_get_screen_prev(d), (unsigned long)lv_obj_get_child_count(lv_layer_bottom()));
  out(b);
  snprintf(b, sizeof(b), "PERFIL   pantalla: bg_opa %u opa %u", lv_obj_get_style_bg_opa(scr, LV_PART_MAIN), lv_obj_get_style_opa(scr, LV_PART_MAIN));
  out(b);
}


}  // namespace perfil

#endif  // MFV3_UI_PERFIL_H
