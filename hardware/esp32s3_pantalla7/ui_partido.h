// MARCADOR FUTBOLÍN V3 · pantalla del partido, igual que la app (src/ui/screens/MatchScreen.tsx).
// ---------------------------------------------------------------------------------------------
// Marcador de cada equipo (tocarlo = gol), reloj de segmentos (verde; amarillo en el último minuto, rojo en los
// últimos 20 s, menta en la prórroga), ANULAR GOL a cada lado y PAUSA en el centro, bola de partido, bloqueo de
// 3 s tras cada gol (barra que se vacía y «BLOQUEO 2.4 S»), y encima: cuenta atrás (anillo y cifra; tocar la
// salta), pausa (CONTINUAR / ABANDONAR con confirmación) y final de parte.
// Lo fijo son capas de la app (scripts/placa-capas.mjs); números, reloj y textos que cambian los dibuja LVGL con
// la letra Inter, en las mismas posiciones (medidas en docs/capturas-app/*.json).
// Las reglas son siempre las de mfv3_engine.h: esta pantalla solo enseña el estado y avisa de los toques.

#ifndef MFV3_UI_PARTIDO_H
#define MFV3_UI_PARTIDO_H

#include "ui_base.h"
#include "ui_sevenseg.h"

namespace partido {

// Lo que hay que enseñar (lo rellena el programa a partir del motor en cada vuelta).
struct View {
  uint8_t mode;    // inicio::QUICK, CHAOS o RANKED (fondo y cartel)
  uint8_t phase;   // mfv3::Phase
  uint8_t period;  // mfv3::Period
  uint8_t white, blue;
  bool annulWhite, annulBlue;  // hay goles de esa parte que anular
  bool singlePeriod;            // por goles: sin partes
  char clock[8];                // «MM:SS»
  uint32_t clockColor;
  uint32_t lockMs;              // bloqueo que queda (0 = libre)
  uint8_t matchPoint;           // bit 0 = Blanco, bit 1 = Azul
  uint8_t streakTeam, streak;   // racha: goles seguidos del último que marcó (se ve con 3 o más)
  char info[40];                // bajo el tipo de partido: «A 5 goles», «2ª PARTE · A 5 goles»…
  char cond[64];                // «Gana quien llegue a 5 goles»…
  // Cuenta atrás
  uint8_t countdownN;
  uint16_t countdownPermille;   // lo que queda del anillo (0–1000)
  char countdownTitle[48];
  // Final de parte
  char endTitle[48], endNote[96], endCta[32], endPartial[64];
  uint8_t endWhite, endBlue;
  bool confirmAbandon;
};

struct Hooks {
  void (*goal)(uint8_t team) = nullptr;
  void (*annul)(uint8_t team) = nullptr;
  void (*pause)() = nullptr;         // pausar o seguir
  void (*skip)() = nullptr;          // saltar la cuenta atrás
  void (*next)() = nullptr;          // continuar tras el final de parte
  void (*askAbandon)(bool on) = nullptr;
  void (*abandon)() = nullptr;
};
static Hooks hooks;

const uint8_t P_COUNTDOWN = 0, P_PLAYING = 1, P_PAUSED = 2, P_PERIOD_END = 3;
const uint32_t LOCK_MS = 3000;

static lv_obj_t* scr = nullptr;
static lv_obj_t* modeBg[3];
static lv_obj_t* streakBadge = nullptr;
static uint8_t streakShown = 0, streakTeamShown = 0;
static const capas::Capa* RACHA[7] = {&capas::RACHA_3, &capas::RACHA_4, &capas::RACHA_5, &capas::RACHA_6,
                                      &capas::RACHA_7, &capas::RACHA_8, &capas::RACHA_9};
static lv_obj_t *annulOff[2], *pulAnnul[2], *pulPause, *pulPanel[2], *mp[3], *clockFrame[3];
static lv_obj_t *score[2], *clock, *info, *cond, *lockMsg, *lockTrack[2], *lockFill[2];
static lv_obj_t *overlay, *cdLayer, *cdSub, *cdRing, *cdNum, *cdHint;
static lv_obj_t *pauseLayer, *pausePulCont, *pausePulAband, *confirmLayer;
static lv_obj_t *endLayer, *endTitle, *endWhite, *endSep, *endBlue, *endPartial, *endNote, *endCta;
static View shown;
static bool fresh = true;
static lv_anim_t mpAnim;

// Medidas de la app (docs/capturas-app/partido-*.json).
const float PANEL_X[2] = {12, 555.2f}, PANEL_W = 232.8f, PANEL_Y = 8, PANEL_H = 356;
const uint32_t INK[2] = {0x0b1626, 0xf3f7ff};

static void vis(lv_obj_t* o, bool v) {
  if (!o) return;
  if (v) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// Número del marcador: una cifra a 250 px y dos a 134 px (las fórmulas de match.css para este tamaño de tarjeta),
// centrado como en el navegador (su letter-spacing de −0,04 em desplaza media letra).
static void setScore(int t, uint8_t v) {
  char b[4];
  snprintf(b, sizeof(b), "%u", v);
  bool two = v >= 10;
  float size = two ? 134 : 250;
  lv_obj_set_style_text_font(score[t], assets::font(900, two ? 134 : 250), 0);
  lv_obj_set_style_text_letter_space(score[t], (int32_t)lroundf(-0.04f * size), 0);
  lv_label_set_text(score[t], b);
  float lh = 0.82f * size;
  float top = 12 + (348 - (18 + lh)) / 2 + 18;  // centrado en la tarjeta con su margen de 18 px arriba
  ui::place(score[t], PANEL_X[t] + PANEL_W / 2 + 0.02f * size, top, lh);
}

static void onPanel(lv_event_t* e) {
  int t = (int)(intptr_t)lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  bool playing = shown.phase == P_PLAYING;
  if (c == LV_EVENT_PRESSED) vis(pulPanel[t], playing);
  else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) vis(pulPanel[t], false);
  else if (c == LV_EVENT_CLICKED && playing && hooks.goal) hooks.goal((uint8_t)t);
}

static void onAnnul(lv_event_t* e) {
  int t = (int)(intptr_t)lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  bool ok = (shown.phase == P_PLAYING || shown.phase == P_PAUSED) && (t ? shown.annulBlue : shown.annulWhite);
  if (c == LV_EVENT_PRESSED) vis(pulAnnul[t], ok);
  else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) vis(pulAnnul[t], false);
  else if (c == LV_EVENT_CLICKED && ok && hooks.annul) hooks.annul((uint8_t)t);
}

static void onPause(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  bool ok = shown.phase == P_PLAYING;
  if (c == LV_EVENT_PRESSED) vis(pulPause, ok);
  else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) vis(pulPause, false);
  else if (c == LV_EVENT_CLICKED && ok && hooks.pause) hooks.pause();
}

static void onOverlay(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED && shown.phase == P_COUNTDOWN && hooks.skip) hooks.skip();
}

static void onPauseBtn(lv_event_t* e) {
  int which = (int)(intptr_t)lv_event_get_user_data(e);  // 0 = continuar, 1 = abandonar
  lv_event_code_t c = lv_event_get_code(e);
  lv_obj_t* pul = which ? pausePulAband : pausePulCont;
  if (c == LV_EVENT_PRESSED) vis(pul, true);
  else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) vis(pul, false);
  else if (c == LV_EVENT_CLICKED) {
    if (which == 0 && hooks.pause) hooks.pause();
    if (which == 1 && hooks.askAbandon) hooks.askAbandon(true);
  }
}

static void onConfirm(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  int which = (int)(intptr_t)lv_event_get_user_data(e);  // 0 = seguir jugando, 1 = abandonar
  if (which == 1 && hooks.abandon) hooks.abandon();
  else if (hooks.askAbandon) hooks.askAbandon(false);
}

static void onNext(lv_event_t*) {
  if (hooks.next) hooks.next();
}

static lv_obj_t* zone(lv_obj_t* parent, float x, float y, float w, float h, lv_event_cb_t cb, intptr_t user) {
  lv_obj_t* o = ui::box(parent, (int)lroundf(x), (int)lroundf(y), (int)lroundf(w), (int)lroundf(h));
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, cb, LV_EVENT_ALL, (void*)user);
  return o;
}

// ---------------------------------------------------------------- ¡GOL! a pantalla completa
// Seis variantes por equipo fotografiadas de la app (GoalEffect.tsx); cada gol enseña una, como allí. Entra
// fundiéndose en 0,2 s y se va de 2,8 a 3,2 s (lo que dura en la app, justo pasado el bloqueo de 3 s). La app las
// anima con mucho más movimiento; aquí, la imagen final de cada una (un zoom de pantalla entera iría a tirones).
static const capas::Capa* GOL[2][6] = {
    {&capas::GOL_BLANCO_ZOOM, &capas::GOL_BLANCO_GOOOOL, &capas::GOL_BLANCO_NEON, &capas::GOL_BLANCO_GOLDEN,
     &capas::GOL_BLANCO_COMIC, &capas::GOL_BLANCO_CONFETTI},
    {&capas::GOL_AZUL_ZOOM, &capas::GOL_AZUL_GOOOOL, &capas::GOL_AZUL_NEON, &capas::GOL_AZUL_GOLDEN,
     &capas::GOL_AZUL_COMIC, &capas::GOL_AZUL_CONFETTI}};
static lv_obj_t* goalShow = nullptr;
static uint8_t nextVariant = 0;

static void goalGone(lv_anim_t* a) {
  lv_obj_t* o = (lv_obj_t*)a->var;
  if (o == goalShow) goalShow = nullptr;
  lv_obj_delete_async(o);
}

static void celebrate(uint8_t team) {
  if (!scr || lv_screen_active() != scr) return;
  if (goalShow) lv_obj_delete(goalShow);
  goalShow = ui::layer(scr, *GOL[team & 1][nextVariant % 6]);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, goalShow);
  lv_anim_set_exec_cb(&a, ui::fadeCb);
  lv_anim_set_values(&a, 0, 255);
  lv_anim_set_duration(&a, 200);
  lv_anim_start(&a);
  lv_anim_set_values(&a, 255, 0);
  lv_anim_set_delay(&a, 2800);
  lv_anim_set_duration(&a, 400);
  lv_anim_set_completed_cb(&a, goalGone);
  lv_anim_start(&a);
  nextVariant = (uint8_t)((nextVariant + 1) % 6);
  // La siguiente, ya descomprimida para el próximo gol (de cualquiera de los dos).
  assets::preload(*GOL[0][nextVariant]);
  assets::preload(*GOL[1][nextVariant]);
}

// «🔥 xN» arriba en la tarjeta de quien lleva la racha; aparece creciendo (pop-in de la app, 0,3 s).
static void popCb(void* o, int32_t v) {
  lv_obj_set_style_transform_scale((lv_obj_t*)o, v, 0);
  lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)(v * 255 / 256 > 255 ? 255 : v * 255 / 256), 0);
}
static void showStreak(uint8_t team, uint8_t n) {
  if (n < 3) {
    if (streakBadge) lv_obj_add_flag(streakBadge, LV_OBJ_FLAG_HIDDEN);
    streakShown = 0;
    return;
  }
  if (n == streakShown && team == streakTeamShown) return;
  const capas::Capa& c = *RACHA[(n > 9 ? 9 : n) - 3];
  lv_image_set_src(streakBadge, assets::src(c));
  lv_obj_set_pos(streakBadge, c.x + (team ? (int)lroundf(PANEL_X[1] - PANEL_X[0]) : 0), c.y);
  lv_obj_set_size(streakBadge, c.w, c.h);
  lv_image_set_pivot(streakBadge, c.w / 2, c.h / 2);
  lv_obj_remove_flag(streakBadge, LV_OBJ_FLAG_HIDDEN);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, streakBadge);
  lv_anim_set_exec_cb(&a, popCb);
  lv_anim_set_values(&a, 102, 256);
  lv_anim_set_duration(&a, 300);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
  streakShown = n;
  streakTeamShown = team;
}

// ---------------------------------------------------------------- carteles (Banner de la app)
// Franja inclinada que entra por la izquierda (0,29 s), se queda y sale por la derecha; 1,5 s en total. Va en la capa
// de arriba para verse también sobre los penaltis.
enum Banner : uint8_t { B_PARTIDO, B_PARTIDO_LOCO, B_1_PARTE, B_2_PARTE, B_PRORROGA, B_PENALTIS, B_MUERTE_SUBITA };
static const capas::Capa* CARTEL[7] = {&capas::CARTEL_PARTIDO, &capas::CARTEL_PARTIDO_LOCO, &capas::CARTEL_1_PARTE,
                                       &capas::CARTEL_2_PARTE, &capas::CARTEL_PRORROGA, &capas::CARTEL_PENALTIS,
                                       &capas::CARTEL_MUERTE_SUBITA};
static lv_obj_t* bannerObj = nullptr;
static void bannerX(void* o, int32_t v) {
  lv_obj_set_x((lv_obj_t*)o, v);
  int32_t d = v < 0 ? -v : v;  // se ve del todo en el centro y se desvanece hacia los lados
  lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)(d >= 880 ? 0 : 255 - d * 255 / 880), 0);
}
static void bannerGone(lv_anim_t* a) {
  lv_obj_t* o = (lv_obj_t*)a->var;
  if (o == bannerObj) bannerObj = nullptr;
  lv_obj_delete_async(o);
}
static void banner(uint8_t which) {
  if (bannerObj) lv_obj_delete(bannerObj);
  const capas::Capa& c = *CARTEL[which % 7];
  bannerObj = ui::layer(lv_layer_top(), c);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, bannerObj);
  lv_anim_set_exec_cb(&a, bannerX);
  lv_anim_set_values(&a, -880, 0);
  lv_anim_set_duration(&a, 290);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
  lv_anim_set_values(&a, 0, 880);
  lv_anim_set_delay(&a, 1210);
  lv_anim_set_duration(&a, 290);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
  lv_anim_set_completed_cb(&a, bannerGone);
  lv_anim_start(&a);
}

static void mpPulse(void*, int32_t v) {
  for (int i = 0; i < 3; i++)
    if (mp[i]) lv_obj_set_style_opa(mp[i], (lv_opa_t)v, 0);
}

static void onDelete(lv_event_t*) {
  lv_anim_delete(nullptr, mpPulse);
  scr = nullptr;
}

static void build() {
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  ui::layer(scr, capas::PARTIDO);
  modeBg[0] = nullptr;
  modeBg[1] = ui::layer(scr, capas::PARTIDO_MODO_LOCO, false);
  modeBg[2] = ui::layer(scr, capas::PARTIDO_MODO_CLASIFICATORIO, false);
  annulOff[0] = ui::layer(scr, capas::PARTIDO_ANULAR_OFF_BLANCO, false);
  annulOff[1] = ui::layer(scr, capas::PARTIDO_ANULAR_OFF_AZUL, false);
  clockFrame[0] = ui::layer(scr, capas::PARTIDO_RELOJ_AMARILLO, false);
  clockFrame[1] = ui::layer(scr, capas::PARTIDO_RELOJ_ROJO, false);
  clockFrame[2] = ui::layer(scr, capas::PARTIDO_PRORROGA, false);
  mp[0] = ui::layer(scr, capas::PARTIDO_MP_BLANCO, false);
  mp[1] = ui::layer(scr, capas::PARTIDO_MP_AZUL, false);
  mp[2] = ui::layer(scr, capas::PARTIDO_MP_AMBOS, false);
  pulPanel[0] = ui::layer(scr, capas::PARTIDO_PUL_BLANCO, false);
  pulPanel[1] = ui::layer(scr, capas::PARTIDO_PUL_AZUL, false);
  pulAnnul[0] = ui::layer(scr, capas::PARTIDO_PUL_ANULAR_BLANCO, false);
  pulAnnul[1] = ui::layer(scr, capas::PARTIDO_PUL_ANULAR_AZUL, false);
  pulPause = ui::layer(scr, capas::PARTIDO_PUL_PAUSA, false);
  streakBadge = lv_image_create(scr);
  lv_obj_remove_flag(streakBadge, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(streakBadge, LV_OBJ_FLAG_HIDDEN);
  streakShown = 0;
  // El aro rojo de la bola de partido late (1,1 s), como mp-pulse.
  lv_anim_init(&mpAnim);
  lv_anim_set_exec_cb(&mpAnim, mpPulse);
  lv_anim_set_values(&mpAnim, 255, 110);
  lv_anim_set_duration(&mpAnim, 550);
  lv_anim_set_reverse_duration(&mpAnim, 550);
  lv_anim_set_repeat_count(&mpAnim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&mpAnim, lv_anim_path_ease_in_out);
  lv_anim_start(&mpAnim);
  for (int t = 0; t < 2; t++) {
    score[t] = ui::text(scr, "0", 900, 250, INK[t]);
    // Barra del bloqueo: abajo en la tarjeta (left/right 18, bottom 12, 5 px).
    int x = (int)lroundf(PANEL_X[t] + 3 + 18), w = (int)lroundf(PANEL_W - 6 - 36);
    lockTrack[t] = ui::box(scr, x, 344, w, 5);
    lv_obj_set_style_radius(lockTrack[t], 3, 0);
    lv_obj_set_style_bg_color(lockTrack[t], lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lockTrack[t], 46, 0);
    lv_obj_set_style_clip_corner(lockTrack[t], true, 0);
    lockFill[t] = ui::box(lockTrack[t], 0, 0, w, 5);
    lv_obj_set_style_bg_color(lockFill[t], ui::hex(INK[t]), 0);
    lv_obj_set_style_bg_opa(lockFill[t], 153, 0);
    vis(lockTrack[t], false);
  }
  // Reloj: el SVG de la app (viewBox de 286 × 100) encajado en 278 × 94 → 268 × 94 centrado.
  clock = sevenseg::create(scr, 266, 124, 94, "00:00", 0x3dff7a, 160);
  ((sevenseg::Data*)lv_obj_get_user_data(clock))->ghost = false;
  info = ui::text(scr, "", 800, 17, ui::color::TEXT);
  cond = ui::text(scr, "", 700, 15, ui::color::TEXT);
  lv_obj_set_style_text_opa(cond, 230, 0);
  lockMsg = ui::text(scr, "", 800, 12, ui::color::RANKED, 1);
  for (int t = 0; t < 2; t++) {
    zone(scr, PANEL_X[t], PANEL_Y, PANEL_W, PANEL_H, onPanel, t);
    zone(scr, t ? 474.8f : 254.8f, 237, 70.4f, 154.3f, onAnnul, t);
  }
  zone(scr, 333.2f, 237, 133.7f, 154.3f, onPause, 0);

  // ---- Capas de encima (fondo oscuro rgba(4, 8, 15, 0.86), como .overlay)
  overlay = ui::box(scr, 0, 0, 800, 480);
  lv_obj_set_style_bg_color(overlay, ui::hex(0x04080f), 0);
  lv_obj_set_style_bg_opa(overlay, 219, 0);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(overlay, onOverlay, LV_EVENT_CLICKED, nullptr);
  // Cuenta atrás
  cdLayer = ui::box(overlay, 0, 0, 800, 480);
  cdSub = ui::text(cdLayer, "PARTIDO", 800, 18, ui::color::TEXT, 18 * 0.16f);
  cdRing = lv_arc_create(cdLayer);
  lv_obj_remove_style_all(cdRing);
  lv_obj_set_pos(cdRing, 275 + 6, 117 + 6);
  lv_obj_set_size(cdRing, 238, 238);  // r = 115 (eje del trazo) con 8 px de grosor
  lv_arc_set_bg_angles(cdRing, 0, 360);
  lv_arc_set_rotation(cdRing, 270);
  lv_arc_set_range(cdRing, 0, 1000);
  lv_obj_set_style_arc_width(cdRing, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_color(cdRing, ui::hex(0x62d6ff), LV_PART_MAIN);
  lv_obj_set_style_arc_opa(cdRing, 31, LV_PART_MAIN);
  lv_obj_set_style_arc_width(cdRing, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(cdRing, ui::hex(0x62d6ff), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(cdRing, true, LV_PART_INDICATOR);
  lv_obj_remove_flag(cdRing, LV_OBJ_FLAG_CLICKABLE);
  cdNum = sevenseg::create(cdLayer, 349, 167, 150, "3", 0x62d6ff, 230);
  cdHint = ui::text(cdLayer, "TOCA PARA SALTAR", 400, 13, ui::color::TEXT2, 13 * 0.14f);
  ui::place(cdHint, 400, 377.5f, 0);
  // Pausa
  pauseLayer = ui::layer(overlay, capas::PAUSA);
  pausePulCont = ui::layer(overlay, capas::PAUSA_PUL_CONTINUAR, false);
  pausePulAband = ui::layer(overlay, capas::PAUSA_PUL_ABANDONAR, false);
  zone(pauseLayer, 140 - capas::PAUSA.x, 190 - capas::PAUSA.y, 520, 140, onPauseBtn, 0);
  zone(pauseLayer, 307.8f - capas::PAUSA.x, 354 - capas::PAUSA.y, 184.4f, 44, onPauseBtn, 1);
  lv_obj_add_flag(pauseLayer, LV_OBJ_FLAG_CLICKABLE);
  confirmLayer = ui::layer(overlay, capas::PAUSA_ABANDONAR, false);
  lv_obj_add_flag(confirmLayer, LV_OBJ_FLAG_CLICKABLE);
  zone(confirmLayer, capas::PAUSA_ABANDONAR_SEGUIR.x, capas::PAUSA_ABANDONAR_SEGUIR.y, capas::PAUSA_ABANDONAR_SEGUIR.w,
       capas::PAUSA_ABANDONAR_SEGUIR.h, onConfirm, 0);
  zone(confirmLayer, capas::PAUSA_ABANDONAR_SI.x, capas::PAUSA_ABANDONAR_SI.y, capas::PAUSA_ABANDONAR_SI.w,
       capas::PAUSA_ABANDONAR_SI.h, onConfirm, 1);
  // Final de parte (todo con LVGL: los textos cambian según la parte)
  endLayer = ui::box(overlay, 0, 0, 800, 480);
  endTitle = ui::text(endLayer, "", 800, 18, ui::color::TEXT, 18 * 0.16f);
  lv_label_set_recolor(endTitle, true);  // «· por tiempo» va en gris (#6f84a3), como el .dim de la app
  endWhite = ui::text(endLayer, "0", 900, 110, ui::color::WHITE_TEAM);
  endSep = ui::text(endLayer, "–", 900, 60, ui::color::TEXT3);
  endBlue = ui::text(endLayer, "0", 900, 110, 0x4fa3f0);
  endPartial = ui::text(endLayer, "", 400, 16, ui::color::TEXT2);
  endNote = ui::text(endLayer, "", 400, 16, ui::color::TEXT2);
  endCta = ui::button(endLayer, 250, 296, 300, 68, "CONTINUAR", 20, ui::BtnKind::Primary, onNext);
  fresh = true;
  goalShow = nullptr;
  assets::preload(*GOL[0][nextVariant]);
  assets::preload(*GOL[1][nextVariant]);
}

static bool active() { return scr && lv_screen_active() == scr; }


// Final de parte, colocado como .overlay-period (centrado en vertical con 10 px entre piezas).
static void layoutEnd(const View& v) {
  char b[4];
  lv_label_set_text(endTitle, v.endTitle);
  snprintf(b, sizeof(b), "%u", v.endWhite);
  lv_label_set_text(endWhite, b);
  snprintf(b, sizeof(b), "%u", v.endBlue);
  lv_label_set_text(endBlue, b);
  lv_label_set_text(endPartial, v.endPartial);
  lv_label_set_text(endNote, v.endNote);
  lv_label_set_text(lv_obj_get_child(endCta, 0), v.endCta);
  bool note = v.endNote[0] != 0;
  // Alturas: título 21,8 · marcador 110 · parcial 20 · [nota 20] · botón 68; huecos de 10 px.
  float total = 21.8f + 110 + 20 + (note ? 30 : 0) + 68 + 40;
  float y = (480 - total) / 2;
  ui::place(endTitle, 400, y, 0);
  y += 21.8f + 10;
  // «1 – 2»: cifras de 110 px con 18 px de hueco y el guion de 60 px en medio (tabular-nums).
  lv_obj_update_layout(endWhite);
  float ww = (float)lv_obj_get_width(endWhite), wb = (float)lv_obj_get_width(endBlue), ws = 30;
  float x = 400 - (ww + ws + wb + 36) / 2;
  ui::place(endWhite, x, y, 110, ui::Align::Left);
  ui::place(endSep, x + ww + 18, y + 25, 60, ui::Align::Left);
  ui::place(endBlue, x + ww + 18 + ws + 18, y, 110, ui::Align::Left);
  y += 110 + 10;
  ui::place(endPartial, 400, y, 20);
  y += 20 + 10;
  vis(endNote, note);
  if (note) {
    ui::place(endNote, 400, y, 20);
    y += 20 + 10;
  }
  lv_obj_set_y(endCta, (int32_t)lroundf(y));
}

static void update(const View& v) {
  if (!active()) {
    build();
    ui::show(scr);
  }
  bool first = fresh;
  fresh = false;
  vis(modeBg[1], v.mode == 1);
  vis(modeBg[2], v.mode == 2);
  if (first || v.white != shown.white) setScore(0, v.white);
  if (first || v.blue != shown.blue) setScore(1, v.blue);
  vis(annulOff[0], !v.annulWhite);
  vis(annulOff[1], !v.annulBlue);
  sevenseg::setText(clock, v.clock);
  sevenseg::setColor(clock, v.clockColor);
  vis(clockFrame[0], v.clockColor == 0xffd43b);
  vis(clockFrame[1], v.clockColor == 0xff4d5e);
  vis(clockFrame[2], v.clockColor == 0x7dffd8);
  showStreak(v.streakTeam, v.streak);
  vis(mp[0], v.matchPoint == 1);
  vis(mp[1], v.matchPoint == 2);
  vis(mp[2], v.matchPoint == 3);
  if (first || strcmp(v.info, shown.info)) {
    lv_label_set_text(info, v.info);
    ui::place(info, 400, 58.8f, 20.4f);
  }
  if (first || strcmp(v.cond, shown.cond)) {
    lv_label_set_text(cond, v.cond);
    ui::place(cond, 400, 397.3f, 18.75f);
  }
  // Bloqueo: barra que se vacía en las dos tarjetas y el aviso con décimas.
  for (int t = 0; t < 2; t++) {
    vis(lockTrack[t], v.lockMs > 0);
    if (v.lockMs) lv_obj_set_width(lockFill[t], (int32_t)(lv_obj_get_width(lockTrack[t]) * v.lockMs / LOCK_MS));
  }
  char lm[24] = "";
  if (v.lockMs) snprintf(lm, sizeof(lm), "BLOQUEO %u.%u S", (unsigned)(v.lockMs / 1000), (unsigned)(v.lockMs % 1000 / 100));
  if (strcmp(lm, lv_label_get_text(lockMsg))) {
    lv_label_set_text(lockMsg, lm);
    ui::place(lockMsg, 400, 422.1f, 0);
  }
  // Capas de encima
  bool cd = v.phase == P_COUNTDOWN, paused = v.phase == P_PAUSED, end = v.phase == P_PERIOD_END;
  vis(overlay, cd || paused || end);
  vis(cdLayer, cd);
  vis(pauseLayer, paused && !v.confirmAbandon);
  vis(confirmLayer, paused && v.confirmAbandon);
  if (!paused) {
    vis(pausePulCont, false);
    vis(pausePulAband, false);
  }
  vis(endLayer, end);
  if (cd) {
    if (first || strcmp(v.countdownTitle, shown.countdownTitle) || shown.phase != P_COUNTDOWN) {
      lv_label_set_text(cdSub, v.countdownTitle);
      ui::place(cdSub, 400, 86.5f, 0);
    }
    lv_arc_set_value(cdRing, v.countdownPermille);
    char n[4];
    snprintf(n, sizeof(n), "%u", v.countdownN);
    sevenseg::setText(cdNum, n);
  }
  if (end && (first || shown.phase != P_PERIOD_END || strcmp(v.endTitle, shown.endTitle) || v.endWhite != shown.endWhite ||
              v.endBlue != shown.endBlue))
    layoutEnd(v);
  shown = v;
}

}  // namespace partido

#endif  // MFV3_UI_PARTIDO_H
