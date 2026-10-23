// MARCADOR FUTBOLÍN V3 · reposo (el reloj que sale tras un rato sin tocar nada).
// ---------------------------------------------------------------------------------------------------
// Diseño propio de la placa (Alex, 18/10/2026), más moderno y ligero que el SleepOverlay de la app: la hora grande
// en el centro de un aro que se va llenando segundo a segundo (el secundero, con un punto naranja en la punta), los
// segundos y la fecha dentro del aro, el último partido arriba y «Toca para despertar» abajo. El primer toque solo
// despierta. Mientras se sincroniza con la nube (solo en reposo), abajo pone que no se toque: tocar la corta.
// Ligero a propósito: cada segundo solo se repinta el trocito de aro nuevo, el punto y los dos números de los
// segundos (el reloj anterior repintaba el reloj grande, las 60 marcas con sombra y una animación, y la imagen
// daba saltos). La hora y la fecha, una vez por minuto.

#ifndef MFV3_UI_REPOSO_H
#define MFV3_UI_REPOSO_H

#include "ui_base.h"

namespace reposo {

static lv_obj_t* scr = nullptr;
static lv_obj_t *ring, *timeLbl, *sec, *date, *last, *hint, *battery, *syncing;
static int lastSec = -1, lastMinute = -1;
static char lastText[64] = "";

static const char* const DIAS[7] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};
static const char* const MESES[12] = {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio",
                                      "agosto", "septiembre", "octubre", "noviembre", "diciembre"};

const int CX = 400, CY = 236, R = 222;  // centro y radio del aro
const uint32_t CYAN = 0x4fd8ff, ORANGE = 0xff9a3c;
static const char* const SYNC_TEXT = "SINCRONIZANDO · NO TOQUES LA PANTALLA";

static void onDelete(lv_event_t*) { scr = nullptr; }

static void build() {
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  // Fondo liso casi negro (un degradado tan oscuro hace franjas con los colores de la pantalla).
  lv_obj_set_style_bg_color(scr, ui::hex(0x050a14), 0);

  // Aro del secundero: pista tenue entera y, encima, lo que va de minuto, con un punto naranja en la punta.
  ring = lv_arc_create(scr);
  lv_obj_set_size(ring, 2 * R, 2 * R);
  lv_obj_set_pos(ring, CX - R, CY - R);
  lv_arc_set_rotation(ring, 270);
  lv_arc_set_bg_angles(ring, 0, 360);
  lv_arc_set_range(ring, 0, 60);
  lv_arc_set_value(ring, 0);
  lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(ring, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_color(ring, ui::hex(0x15263b), LV_PART_MAIN);
  lv_obj_set_style_arc_width(ring, 6, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(ring, ui::hex(CYAN), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(ring, true, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(ring, ui::hex(ORANGE), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(ring, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_pad_all(ring, 4, LV_PART_KNOB);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_KNOB);

  // Dentro del aro: hora, segundos y fecha.
  timeLbl = ui::text(scr, "00:00", 900, 150, 0xeaf6ff);  // grande, que se lea desde lejos
  sec = ui::text(scr, "00", 900, 26, ORANGE);
  date = ui::text(scr, "", 400, 22, 0x6f90ae, 0.5f);
  // Fuera: último partido arriba y el aviso para despertar abajo.
  last = ui::text(scr, "", 700, 15, 0x4a6784, 0.9f);
  lv_label_set_recolor(last, true);
  hint = ui::text(scr, "TOCA PARA DESPERTAR", 400, 12, 0x2c4058, 1.68f);
  ui::place(hint, 400 + 0.84f, 463, 0);
  syncing = ui::text(scr, SYNC_TEXT, 900, 20, CYAN, 1.2f);
  ui::place(syncing, 400, 452, 24);
  lv_obj_add_flag(syncing, LV_OBJ_FLAG_HIDDEN);
  // Solo en la placa (la app no tiene batería): aviso en rojo arriba a la derecha.
  battery = ui::text(scr, "BATERÍA BAJA: CONECTA EL CARGADOR", 800, 13, ui::color::DANGER, 1.3f);
  ui::place(battery, 784, 12, 0, ui::Align::Right);
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
    lv_label_set_text(timeLbl, b);
    ui::place(timeLbl, CX, CY - 136, 156);
    snprintf(b, sizeof(b), "%s, %u de %s", DIAS[weekday % 7], day, MESES[(month + 11) % 12]);
    lv_label_set_text(date, b);
    ui::place(date, CX, CY + 92, 26);
    lastMinute = minute;
  }
  if (second != lastSec) {
    lv_arc_set_value(ring, second % 60);
    snprintf(b, sizeof(b), "%02u", second);
    lv_label_set_text(sec, b);
    ui::place(sec, CX, CY + 46, 32);
    lastSec = second;
  }
  ui::setVisible(battery, lowBattery);
  ui::setVisible(syncing, syncNow);
  ui::setVisible(hint, !syncNow);
  if (strcmp(lastMatch ? lastMatch : "", lastText)) {
    snprintf(lastText, sizeof(lastText), "%s", lastMatch ? lastMatch : "");
    lv_label_set_text(last, lastText);
    ui::place(last, 16, 10, 19, ui::Align::Left);
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
