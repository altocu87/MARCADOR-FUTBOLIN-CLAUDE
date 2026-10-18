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
#include "s3_jugadores.h"

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
  // Fichas de jugadores bajo cada marcador (0 = sin jugadores: la tarjeta ocupa todo el alto)
  uint8_t nPlayers[2];
  struct Card {
    uint32_t code;
    char name[28], meta[40], title[32], photo[72];  // photo: ruta «A:…» de la foto o vacía (iniciales)
  } cards[2][4];
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
static lv_obj_t *annulOff[2], *pulAnnul[2], *pulPause, *mp[3], *clockFrame[3];
static lv_obj_t *panel[2], *panelDim[2], *teamLabel[2], *cardsBox[2], *cardsVeil[2];
static lv_obj_t *score[2], *clock, *info, *cond, *lockMsg, *lockTrack[2], *lockFill[2];
static float panelH[2] = {356, 356};
static uint32_t lineupKey[2] = {0xFFFFFFFF, 0xFFFFFFFF};
static lv_obj_t *overlay, *cdLayer, *cdSub, *cdRing, *cdNum, *cdHint;
static lv_obj_t *pauseLayer, *pausePulCont, *pausePulAband, *confirmLayer;
static lv_obj_t *endLayer, *endTitle, *endWhite, *endSep, *endBlue, *endPartial, *endNote, *endCta;
static View shown;
static bool fresh = true;
static lv_anim_t mpAnim;

// Medidas de la app (docs/capturas-app/partido-*.json).
const float PANEL_X[2] = {12, 555.2f}, PANEL_W = 232.8f, PANEL_Y = 8;
const uint32_t INK[2] = {0x0b1626, 0xf3f7ff};

static void vis(lv_obj_t* o, bool v) { ui::setVisible(o, v); }

// Número del marcador: una cifra a 250 px y dos a 134 px (las fórmulas de match.css para este tamaño de tarjeta),
// centrado como en el navegador (su letter-spacing de −0,04 em desplaza media letra).
static void setScore(int t, uint8_t v) {
  char b[4];
  snprintf(b, sizeof(b), "%u", v);
  bool two = v >= 10;
  // Fórmulas de match.css: una cifra = min((ancho − 40)/0,7, (alto − 80)/0,8) → 250 px, o 242 con dos jugadores
  // por equipo (la tarjeta queda más baja); dos cifras = 134 px.
  float inner = panelH[t] - 8;  // sin bordes (3 + 3) ni relleno (1 + 1)
  uint16_t size = two ? 134 : (uint16_t)((inner - 80) / 0.8f < 249 ? 242 : 250);
  lv_obj_set_style_text_font(score[t], assets::font(900, size), 0);
  lv_obj_set_style_text_letter_space(score[t], (int32_t)lroundf(-0.04f * size), 0);
  lv_label_set_text(score[t], b);
  float lh = 0.82f * size;
  // Coordenadas dentro del borde de 3 px (así cuenta LVGL las de los hijos).
  float top = 1 + (inner - (18 + lh)) / 2 + 18;  // centrado en la tarjeta con su margen de 18 px arriba
  ui::place(score[t], PANEL_W / 2 - 3 + 0.02f * size, top, lh);
}

static void onPanel(lv_event_t* e) {
  int t = (int)(intptr_t)lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  bool playing = shown.phase == P_PLAYING;
  if (c == LV_EVENT_CLICKED && playing && hooks.goal) hooks.goal((uint8_t)t);  // se encoge sola al pulsarla
}

// ---------------------------------------------------------------- tarjetas del marcador (.score-btn)
// Dibujadas con LVGL para que cambien de alto con las fichas de debajo: degradado de 3 tonos, esquinas de 22 px,
// borde de 3 px (claro en la azul) y halo; con bola de partido, aro rojo que late.
static void stylePanel(int t) {
  lv_obj_t* p = panel[t];
  static lv_grad_dsc_t g[2];
  const uint32_t stops[2][3] = {{0xffffff, 0xeef6ff, 0xcfe0f4}, {0x2a86dc, 0x176dbc, 0x0f4f8c}};
  const uint8_t frac[3] = {0, 140, 255};
  memset(&g[t], 0, sizeof(g[t]));
  for (int i = 0; i < 3; i++) {
    g[t].stops[i].color = lv_color_hex(stops[t][i]);
    g[t].stops[i].opa = LV_OPA_COVER;
    g[t].stops[i].frac = frac[i];
  }
  g[t].stops_count = 3;
  g[t].dir = LV_GRAD_DIR_VER;
  lv_obj_set_style_bg_grad(p, &g[t], 0);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(p, 22, 0);
  lv_obj_set_style_border_width(p, 3, 0);
  lv_obj_set_style_border_color(p, ui::hex(t ? ui::color::ACCENT : 0xffffff), 0);
  lv_obj_set_style_border_opa(p, t ? 102 : 0, 0);
  lv_obj_set_style_shadow_width(p, 28, 0);
  lv_obj_set_style_shadow_color(p, ui::hex(t ? ui::color::BLUE_TEAM : ui::color::WHITE_TEAM), 0);
  lv_obj_set_style_shadow_opa(p, t ? 115 : 56, 0);
  lv_obj_set_style_outline_color(p, ui::hex(ui::color::DANGER), 0);
  lv_obj_set_style_outline_pad(p, 0, 0);
  lv_obj_set_style_transform_pivot_x(p, (int32_t)(PANEL_W / 2), 0);
  lv_obj_set_style_transform_scale(p, 252, LV_STATE_PRESSED);  // :active → scale(0.985)
}

static void layoutSide(int t, uint8_t n) {
  // Alto de las fichas: 1 jugador = 100; 2 = 84 + 6 + 84; 3 o 4 = dos filas de 66 (+ 6); sin jugadores, nada.
  float cardsH = n == 0 ? 0 : n == 1 ? 100 : n == 2 ? 174 : 138;
  panelH[t] = n == 0 ? 462 : 462 - 6 - cardsH;
  lv_obj_set_height(panel[t], (int32_t)lroundf(panelH[t]));
  lv_obj_set_style_transform_pivot_y(panel[t], (int32_t)(panelH[t] / 2), 0);
  lv_obj_set_y(cardsBox[t], (int32_t)lroundf(PANEL_Y + panelH[t] + 6));
  lv_obj_set_height(cardsBox[t], cardsH > 0 ? (int32_t)lroundf(cardsH) : 1);
  lv_obj_set_y(lockTrack[t], (int32_t)lroundf(panelH[t] - 6 - 12 - 5));
  lv_obj_set_size(panelDim[t], (int32_t)lroundf(PANEL_W), (int32_t)lroundf(panelH[t]));
}

// Ficha de un jugador como las de la app (.match-card): foto o iniciales, nombre en mayúsculas, detalle y título.
// Tocarla también es gol de su equipo.
static void playerCard(lv_obj_t* parent, int t, const View::Card& c, int x, int y, int w, int h, uint8_t n) {
  bool one = n == 1, many = n >= 3;
  int pad = one ? 7 : many ? 4 : 5, ph = one ? 84 : many ? 34 : 70, pr = one ? 14 : many ? 8 : 10;
  int gap = one ? 12 : many ? 4 : 10;
  uint16_t nameSz = one ? 23 : many ? 13 : 20, metaSz = one ? 14 : many ? 11 : 13, titleSz = many ? 11 : 13;
  uint32_t col = t ? 0x4fb0ff : ui::color::WHITE_TEAM;
  lv_obj_t* card = ui::box(parent, x, y, w, h);
  lv_obj_set_style_radius(card, ui::RADIUS_SM, 0);
  lv_obj_set_style_border_width(card, 2, 0);
  lv_obj_set_style_border_color(card, ui::hex(col), 0);
  lv_obj_set_style_shadow_width(card, 10, 0);
  lv_obj_set_style_shadow_color(card, ui::hex(col), 0);
  lv_obj_set_style_shadow_opa(card, t ? 115 : 89, 0);
  lv_obj_set_style_bg_color(card, ui::hex(0x0a1628), 0);
  lv_obj_set_style_bg_opa(card, 235, 0);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, onPanel, LV_EVENT_ALL, (void*)(intptr_t)t);
  int phx = pad, phy = (h - 4 - ph) / 2;
  // Foto ya reducida y con sus esquinas (assets::photo); sin foto, el degradado con las iniciales.
  const lv_image_dsc_t* pic = c.photo[0] ? assets::photo(c.photo, ph, pr) : nullptr;
  if (pic) {
    lv_obj_t* img = lv_image_create(card);
    lv_image_set_src(img, pic);
    lv_obj_set_pos(img, phx, phy);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_obj_t* box = ui::box(card, phx, phy, ph, ph);
    lv_obj_set_style_radius(box, pr, 0);
    lv_obj_set_style_bg_color(box, ui::hex(0x1d3a63), 0);
    lv_obj_set_style_bg_grad_color(box, ui::hex(0x0f2140), 0);
    lv_obj_set_style_bg_grad_dir(box, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    char ini[8];
    jug::initials(c.name, ini, sizeof(ini));
    lv_obj_t* l = ui::text(box, ini, 900, one ? 38 : many ? 14 : 26, ui::color::TEXT2);
    lv_obj_center(l);
  }
  int tx = phx + ph + gap, tw = w - 4 - tx - (many ? 2 : 8);
  // Nombre en mayúsculas (también las vocales con tilde y la ñ), salvo con 3 o 4 por equipo, como la app.
  char upper[40];
  size_t k = 0;
  for (const char* q = c.name; *q && k < sizeof(upper) - 2; q++) {
    char ch = *q;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    else if ((uint8_t)ch == 0xC3 && (uint8_t)q[1] >= 0xA0 && (uint8_t)q[1] <= 0xBE) {
      upper[k++] = ch;
      q++;
      ch = (char)((uint8_t)*q - 0x20);
    }
    upper[k++] = ch;
  }
  upper[k] = 0;
  auto oneLine = [&](const char* txt, uint16_t wt, uint16_t sz, uint32_t color, float ls) {
    lv_obj_t* l = ui::text(card, txt, wt, sz, color, ls);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_size(l, tw, lv_font_get_line_height(assets::font(wt, sz)));
    return l;
  };
  lv_obj_t* nm = oneLine(many ? c.name : upper, 800, nameSz, ui::color::TEXT, many ? 0 : nameSz * 0.04f);
  lv_obj_t* mt = c.meta[0] ? oneLine(c.meta, 400, metaSz, ui::color::TEXT2, 0) : nullptr;
  lv_obj_t* ti = c.title[0] ? oneLine(c.title, 900, titleSz, ui::color::RANKED, 0) : nullptr;
  int lhN = lv_font_get_line_height(assets::font(800, nameSz)), lhM = lv_font_get_line_height(assets::font(400, metaSz)),
      lhT = lv_font_get_line_height(assets::font(900, titleSz));
  int total = lhN + (mt ? 1 + lhM : 0) + (ti ? 1 + lhT : 0);
  int yy = (h - 4 - total) / 2;
  lv_obj_set_pos(nm, tx, yy);
  yy += lhN + 1;
  if (mt) {
    lv_obj_set_pos(mt, tx, yy);
    yy += lhM + 1;
  }
  if (ti) lv_obj_set_pos(ti, tx, yy);
}

static void lineup(int t, const View& v) {
  uint8_t n = v.nPlayers[t] > 4 ? 4 : v.nPlayers[t];
  uint32_t key = n;
  for (uint8_t i = 0; i < n; i++) key = key * 31u + v.cards[t][i].code;
  if (key == lineupKey[t]) return;
  lineupKey[t] = key;
  layoutSide(t, n);
  lv_obj_clean(cardsBox[t]);
  cardsVeil[t] = nullptr;
  int W = (int)lroundf(PANEL_W);
  for (uint8_t i = 0; i < n; i++) {
    if (n <= 2) playerCard(cardsBox[t], t, v.cards[t][i], 0, i * 90, W, n == 1 ? 100 : 84, n);
    else {
      int half = (W - 6) / 2;
      bool full = n == 3 && i == 2;  // con 3, el tercero ocupa el ancho entero
      playerCard(cardsBox[t], t, v.cards[t][i], full ? 0 : (i % 2) * (half + 6), (i / 2) * 72, full ? W : half, 66, n);
    }
  }
  // Velo del bloqueo (.match-card.locked, al 70 %): un velo oscuro encima, más ligero que la transparencia.
  if (n) {
    lv_obj_update_layout(cardsBox[t]);
    cardsVeil[t] = ui::box(cardsBox[t], -6, -6, lv_obj_get_width(cardsBox[t]) + 12, lv_obj_get_height(cardsBox[t]) + 12);
    lv_obj_set_style_bg_color(cardsVeil[t], ui::hex(0x04080f), 0);
    lv_obj_set_style_bg_opa(cardsVeil[t], 77, 0);
    lv_obj_add_flag(cardsVeil[t], LV_OBJ_FLAG_HIDDEN);
  }
  setScore(t, t ? v.blue : v.white);
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

static void preloadNext(lv_timer_t*) {
  assets::preload(*GOL[0][nextVariant]);
  assets::preload(*GOL[1][nextVariant]);
}

static void celebrate(uint8_t team) {
  if (!scr || lv_screen_active() != scr) return;
  if (goalShow) lv_obj_delete(goalShow);
  goalShow = ui::layer(scr, *GOL[team & 1][nextVariant % 6]);
  // Aparece y se va de golpe (la app la funde): fundir una imagen de pantalla entera obliga a repintarla mezclada
  // en cada paso y la placa daba trompicones. Se queda los mismos 3,2 s.
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, goalShow);
  lv_anim_set_values(&a, 0, 1);
  lv_anim_set_duration(&a, 3200);
  lv_anim_set_completed_cb(&a, goalGone);
  lv_anim_start(&a);
  nextVariant = (uint8_t)((nextVariant + 1) % 6);
  // La siguiente se prepara mientras se ve esta (la imagen está quieta entre 0,2 y 2,8 s): al marcar no hay espera.
  lv_timer_t* t = lv_timer_create(preloadNext, 700, nullptr);
  lv_timer_set_repeat_count(t, 1);
}

// «🔥 xN» arriba en la tarjeta de quien lleva la racha; aparece creciendo (pop-in de la app, 0,3 s).
static void popCb(void* o, int32_t v) {
  lv_image_set_scale((lv_obj_t*)o, (uint32_t)v);
  lv_obj_set_style_image_opa((lv_obj_t*)o, (lv_opa_t)(v >= 256 ? 255 : v), 0);
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
  ui::reveal(streakBadge);
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
static void bannerGone(lv_anim_t* a) {
  lv_obj_t* o = (lv_obj_t*)a->var;
  if (o == bannerObj) bannerObj = nullptr;
  lv_obj_delete_async(o);
}
static void banner(uint8_t which) {
  if (bannerObj) lv_obj_delete(bannerObj);
  const capas::Capa& c = *CARTEL[which % 7];
  bannerObj = ui::layer(lv_layer_top(), c);
  // En su sitio desde el principio y fuera a los 1,5 s (en la app entra y sale deslizándose): moverla por la
  // pantalla repintaba en cada paso una franja de 800 × 220 con transparencias.
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, bannerObj);
  lv_anim_set_values(&a, 0, 1);
  lv_anim_set_duration(&a, 1500);
  lv_anim_set_completed_cb(&a, bannerGone);
  lv_anim_start(&a);
}

// Bola de partido (mp-pulse): aro de 3 px y halo rojo de 32 px que baja a 12 px y vuelve cada 1,1 s; el cartel late.
static uint8_t mpMask = 0;
// En la placa solo late el cartel «BOLA DE PARTIDO», a saltos (fuerte / suave); el aro rojo y el halo de la tarjeta
// se quedan fijos: cambiar el halo obliga a repintar la tarjeta entera con su sombra y frenaba todo el partido.
static int8_t mpLevel = -1;
static void mpPulse(void*, int32_t v) {
  int8_t level = v >= 128 ? 1 : 0;
  if (level == mpLevel) return;
  mpLevel = level;
  for (int i = 0; i < 3; i++)
    if (mp[i] && !lv_obj_has_flag(mp[i], LV_OBJ_FLAG_HIDDEN)) lv_obj_set_style_image_opa(mp[i], level ? 255 : 110, 0);
}
static void setMatchPoint(uint8_t mask) {
  if (mask == mpMask) return;
  mpMask = mask;
  mpLevel = -1;
  for (int t = 0; t < 2; t++) {
    bool on = mask >> t & 1;
    lv_obj_set_style_outline_width(panel[t], on ? 3 : 0, 0);
    lv_obj_set_style_shadow_color(panel[t], ui::hex(on ? ui::color::DANGER : t ? ui::color::BLUE_TEAM : ui::color::WHITE_TEAM), 0);
    lv_obj_set_style_shadow_width(panel[t], on ? 32 : 28, 0);
    lv_obj_set_style_shadow_opa(panel[t], on ? 140 : t ? 115 : 56, 0);
  }
}

static void onDelete(lv_event_t*) {
  lv_anim_delete(nullptr, mpPulse);
  scr = nullptr;
  panel[0] = panel[1] = nullptr;  // ya no existen: que nadie las toque
  cardsVeil[0] = cardsVeil[1] = nullptr;
  mpMask = 0;
}

static void build() {
  panel[0] = panel[1] = nullptr;
  cardsVeil[0] = cardsVeil[1] = nullptr;
  mpMask = 0;
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
  pulAnnul[0] = ui::layer(scr, capas::PARTIDO_PUL_ANULAR_BLANCO, false);
  pulAnnul[1] = ui::layer(scr, capas::PARTIDO_PUL_ANULAR_AZUL, false);
  pulPause = ui::layer(scr, capas::PARTIDO_PUL_PAUSA, false);
  // El aro rojo de la bola de partido late (1,1 s), como mp-pulse.
  lv_anim_init(&mpAnim);
  lv_anim_set_exec_cb(&mpAnim, mpPulse);
  lv_anim_set_values(&mpAnim, 255, 0);
  lv_anim_set_duration(&mpAnim, 550);
  lv_anim_set_reverse_duration(&mpAnim, 550);
  lv_anim_set_repeat_count(&mpAnim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&mpAnim, lv_anim_path_ease_in_out);
  lv_anim_start(&mpAnim);
  for (int t = 0; t < 2; t++) {
    panel[t] = ui::box(scr, (int)lroundf(PANEL_X[t]), (int)PANEL_Y, (int)lroundf(PANEL_W), 356);
    stylePanel(t);
    lv_obj_add_flag(panel[t], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(panel[t], onPanel, LV_EVENT_ALL, (void*)(intptr_t)t);
    // Bloqueo (filter: saturate(0.75) brightness(0.92) en la app): un velo oscuro del 8 %.
    panelDim[t] = ui::box(panel[t], -3, -3, 10, 10);
    lv_obj_set_style_radius(panelDim[t], 22, 0);
    lv_obj_set_style_bg_color(panelDim[t], lv_color_black(), 0);
    lv_obj_set_style_bg_opa(panelDim[t], 20, 0);
    lv_obj_add_flag(panelDim[t], LV_OBJ_FLAG_HIDDEN);
    teamLabel[t] = ui::text(panel[t], t ? "AZUL" : "BLANCO", 900, 15, INK[t], 3.6f);
    lv_obj_set_style_text_opa(teamLabel[t], 191, 0);
    ui::place(teamLabel[t], PANEL_W / 2 - 3 + 1.8f, 10, 0);
    cardsBox[t] = ui::box(scr, (int)lroundf(PANEL_X[t]), 370, (int)lroundf(PANEL_W), 100);
    score[t] = ui::text(panel[t], "0", 900, 250, INK[t]);
    // Barra del bloqueo: abajo en la tarjeta (left/right 18, bottom 12, 5 px).
    int x = 18, w = (int)lroundf(PANEL_W - 6 - 36);
    lockTrack[t] = ui::box(panel[t], x, 336, w, 5);
    lv_obj_set_style_radius(lockTrack[t], 3, 0);
    lv_obj_set_style_bg_color(lockTrack[t], lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lockTrack[t], 46, 0);
    lockFill[t] = ui::box(lockTrack[t], 0, 0, w, 5);
    lv_obj_set_style_radius(lockFill[t], 3, 0);
    lv_obj_set_style_bg_color(lockFill[t], ui::hex(INK[t]), 0);
    lv_obj_set_style_bg_opa(lockFill[t], 153, 0);
    vis(lockTrack[t], false);
  }
  // Racha «🔥 xN» encima de las tarjetas.
  streakBadge = lv_image_create(scr);
  lv_obj_remove_flag(streakBadge, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(streakBadge, LV_OBJ_FLAG_HIDDEN);
  streakShown = 0;
  // Reloj: el SVG de la app (viewBox de 286 × 100) encajado en 278 × 94 → 268 × 94 centrado.
  clock = sevenseg::create(scr, 266, 124, 94, "00:00", 0x3dff7a, 160);
  ((sevenseg::Data*)lv_obj_get_user_data(clock))->ghost = false;
  info = ui::text(scr, "", 800, 17, ui::color::TEXT);
  cond = ui::text(scr, "", 700, 15, ui::color::TEXT);
  lv_obj_set_style_text_opa(cond, 230, 0);
  lockMsg = ui::text(scr, "", 800, 12, ui::color::RANKED, 1);
  for (int t = 0; t < 2; t++) zone(scr, t ? 474.8f : 254.8f, 237, 70.4f, 154.3f, onAnnul, t);
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
  mpMask = 0;
  lineupKey[0] = lineupKey[1] = 0xFFFFFFFF;
  lv_timer_t* t = lv_timer_create(preloadNext, 1500, nullptr);
  lv_timer_set_repeat_count(t, 1);
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
  lineup(0, v);
  lineup(1, v);
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
  setMatchPoint(v.matchPoint);
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
    vis(panelDim[t], v.lockMs > 0);
    vis(cardsVeil[t], v.lockMs > 0);
    vis(lockTrack[t], v.lockMs > 0);
    if (v.lockMs) {
      // A saltos de una décima, como el aviso: cada cambio repinta la franja de las dos tarjetas.
      uint32_t tenths = (v.lockMs + 99) / 100 * 100;
      int32_t w = (int32_t)(lv_obj_get_width(lockTrack[t]) * (tenths > LOCK_MS ? LOCK_MS : tenths) / LOCK_MS);
      if (lv_obj_get_width(lockFill[t]) != w) lv_obj_set_width(lockFill[t], w);
    }
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
