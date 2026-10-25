// MARCADOR FUTBOLÍN V3 · reposo (el reloj que sale tras un rato sin tocar nada).
// ---------------------------------------------------------------------------------------------------
// Como el SleepOverlay de la app (Alex, 25/10/2026: «el círculo por una línea para el segundero como el de la app, y
// la hora más grande y estilo arcade»): la hora en siete segmentos de 250 px (ui_sevenseg.h, los dos puntos laten
// con cada segundo), debajo la línea de 60 marcas que se van encendiendo (la del segundo actual, naranja) con los
// segundos a la derecha, la fecha y el último partido. El primer toque solo despierta. Mientras se sincroniza con la
// nube (solo en reposo), abajo pone que no se toque: tocar la corta.
// Ligero a propósito (se ve muchas horas): nada se crea ni se borra mientras está a la vista. Cada segundo solo se
// repintan los dos puntos, las dos marcas que cambian y el número de los segundos; las cifras de la hora, ya
// pintadas una vez (sevenseg::glyph), una vez por minuto.

#ifndef MFV3_UI_REPOSO_H
#define MFV3_UI_REPOSO_H

#include "ui_base.h"
#include "ui_sevenseg.h"

namespace reposo {

static lv_obj_t* scr = nullptr;
static lv_obj_t *hours, *colon, *minutes, *ticks, *sec, *date, *last, *hint, *battery, *syncing;
static int lastSec = -1, lastMinute = -1;
static char lastText[64] = "";

static const char* const DIAS[7] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};
static const char* const MESES[12] = {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio",
                                      "agosto", "septiembre", "octubre", "noviembre", "diciembre"};

const uint32_t CYAN = 0x4fd8ff, ORANGE = 0xff9a3c;
static const char* const SYNC_TEXT = "SINCRONIZANDO · NO TOQUES LA PANTALLA";
// Reloj como en la app: 250 px de alto, «HH:MM» empieza en x = 52,5 (cifras de 66 unidades, dos puntos de 20, a
// 2,5 px la unidad). Cada parte es su propio objeto para que el latido de los dos puntos no repinte las cifras.
const int CLOCK_H = 250, CLOCK_Y = 40;
// Línea de segundos (.sleep-ticks): 60 marcas de 6 px repartidas en 662 px; cada cinco, más alta.
const int TICKS_X = 40, TICKS_Y = 311, TICKS_W = 662, TICKS_H = 22, TICK_W = 6;

static void onDelete(lv_event_t*) {
  scr = nullptr;
  sevenseg::forget(CLOCK_H);
}

static int tickX(int i) { return (int)lroundf(i * (TICKS_W - TICK_W) / 59.0f); }  // relativo a la línea

static void ticksDraw(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t co;
  lv_obj_get_coords(ticks, &co);
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = 3;
  for (int i = 0; i < 60; i++) {
    int th = i % 5 == 4 ? 18 : 12, x = co.x1 + tickX(i), y = co.y1 + (TICKS_H - th) / 2;
    lv_area_t a = {x, y, x + TICK_W - 1, y + th - 1};
    bool on = i < lastSec, now = i == lastSec;
    uint32_t col = now ? ORANGE : CYAN;
    if (on || now) {  // halo (en la app, box-shadow)
      d.bg_color = ui::hex(col);
      d.bg_opa = now ? 70 : 40;
      int g = now ? 4 : 2;
      lv_area_t h = {a.x1 - g, a.y1 - g, a.x2 + g, a.y2 + g};
      d.radius = 3 + g;
      lv_draw_rect(layer, &d, &h);
      d.radius = 3;
    }
    d.bg_color = ui::hex(col);
    d.bg_opa = now ? LV_OPA_COVER : on ? 140 : 26;  // 0,55 y 0,1 de la app
    lv_draw_rect(layer, &d, &a);
  }
}

static void ticksExt(lv_event_t* e) { lv_event_set_ext_draw_size(e, 6); }

// Repinta solo la marca `i` (con su halo).
static void invalidateTick(int i) {
  if (i < 0 || i > 59) return;
  lv_area_t co;
  lv_obj_get_coords(ticks, &co);
  lv_area_t a = {co.x1 + tickX(i) - 5, co.y1 - 5, co.x1 + tickX(i) + TICK_W + 4, co.y2 + 5};
  lv_obj_invalidate_area(ticks, &a);
}

static void build() {
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  // Fondo liso casi negro, el #020407 de la app llevado al color más cercano de la pantalla que no tira a verde
  // (un degradado tan oscuro hace franjas con los colores de la pantalla).
  lv_obj_set_style_bg_color(scr, ui::hex(0x030409), 0);

  // Hora: «HH», «:» y «MM» (la caja de cada parte empieza 4 unidades antes de su primera cifra).
  hours = sevenseg::create(scr, 42, CLOCK_Y, CLOCK_H, "00", CYAN, 255);
  colon = sevenseg::create(scr, 372, CLOCK_Y, CLOCK_H, ":", CYAN, 255);
  minutes = sevenseg::create(scr, 422, CLOCK_Y, CLOCK_H, "00", CYAN, 255);
  for (lv_obj_t* o : {hours, colon, minutes}) ((sevenseg::Data*)lv_obj_get_user_data(o))->soft = true;

  ticks = lv_obj_create(scr);
  lv_obj_remove_style_all(ticks);
  lv_obj_remove_flag(ticks, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(ticks, TICKS_X, TICKS_Y);
  lv_obj_set_size(ticks, TICKS_W, TICKS_H);
  lv_obj_add_event_cb(ticks, ticksExt, LV_EVENT_REFR_EXT_DRAW_SIZE, nullptr);
  lv_obj_add_event_cb(ticks, ticksDraw, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_refresh_ext_draw_size(ticks);

  sec = ui::text(scr, "00", 900, 26, ORANGE);
  date = ui::text(scr, "", 400, 22, 0x5d7f9c, 1.3f);
  // Debajo: el último partido y el aviso para despertar.
  last = ui::text(scr, "", 700, 15, 0x34506a, 0.9f);
  lv_label_set_recolor(last, true);
  hint = ui::text(scr, "TOCA PARA DESPERTAR", 400, 12, 0x23344a, 1.68f);
  ui::place(hint, 400 + 0.84f, 448, 0);
  syncing = ui::text(scr, SYNC_TEXT, 900, 20, CYAN, 1.2f);
  ui::place(syncing, 400, 444, 24);
  lv_obj_add_flag(syncing, LV_OBJ_FLAG_HIDDEN);
  // Solo en la placa (la app no tiene batería): aviso en rojo arriba a la derecha, encima del reloj.
  battery = ui::text(scr, "BATERÍA BAJA: CONECTA EL CARGADOR", 800, 13, ui::color::DANGER, 1.3f);
  ui::place(battery, 784, 10, 0, ui::Align::Right);
  lv_obj_add_flag(battery, LV_OBJ_FLAG_HIDDEN);
  lastSec = lastMinute = -1;
  lastText[0] = 0;
}

static bool active() { return scr && lv_screen_active() == scr; }

// `lastMatch`: «Último: BLANCO 5 – 3 AZUL» o vacío. Fecha: día de la semana (0 = domingo), día y mes.
static void update(uint8_t hour, uint8_t minute, uint8_t second, uint8_t weekday, uint8_t day, uint8_t month,
                   const char* lastMatch, bool lowBattery = false, bool syncNow = false) {
  if (!active()) {
    build();
    ui::show(scr);
  }
  char b[64];
  if (minute != lastMinute) {
    snprintf(b, sizeof(b), "%02u:%02u", hour, minute);
    sevenseg::forget(CLOCK_H, b);  // las cifras que ya no se ven
    b[2] = 0;
    sevenseg::setText(hours, b);
    sevenseg::setText(minutes, b + 3);
    snprintf(b, sizeof(b), "%s, %u de %s", DIAS[weekday % 7], day, MESES[(month + 11) % 12]);
    lv_label_set_text(date, b);
    ui::place(date, 400, 355, 26);
    lastMinute = minute;
  }
  if (second != lastSec) {
    int s = second % 60;
    if (lastSec < 0 || s < lastSec || s > lastSec + 1) lv_obj_invalidate(ticks);  // minuto nuevo o salto
    else {
      invalidateTick(lastSec);
      invalidateTick(s);
    }
    lastSec = s;
    sevenseg::setText(colon, ":", s % 2);  // pares encendidos, impares tenues
    snprintf(b, sizeof(b), "%02u", s);
    lv_label_set_text(sec, b);
    ui::place(sec, 760, 306, 31, ui::Align::Right);
  }
  ui::setVisible(battery, lowBattery);
  ui::setVisible(syncing, syncNow);
  ui::setVisible(hint, !syncNow);
  if (strcmp(lastMatch ? lastMatch : "", lastText)) {
    snprintf(lastText, sizeof(lastText), "%s", lastMatch ? lastMatch : "");
    lv_label_set_text(last, lastText);
    ui::place(last, 400, 393, 36);
  }
}

// Fuera del reloj (placa sin hora, o que no se atenúa nunca): el mismo aviso en una franja abajo, encima de todo.
static lv_obj_t* banner = nullptr;
static void syncBanner(bool on) {
  if (on == (banner != nullptr)) return;
  if (!on) {
    lv_obj_delete(banner);
    banner = nullptr;
    return;
  }
  banner = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(banner);
  lv_obj_set_size(banner, 800, 48);
  lv_obj_set_pos(banner, 0, 432);
  lv_obj_set_style_bg_color(banner, ui::hex(0x050a14), 0);
  lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
  lv_obj_t* l = ui::text(banner, SYNC_TEXT, 900, 20, CYAN, 1.2f);
  ui::place(l, 400, 12, 24);
}

}  // namespace reposo

#endif  // MFV3_UI_REPOSO_H
