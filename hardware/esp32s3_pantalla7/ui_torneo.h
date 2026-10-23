// MARCADOR FUTBOLÍN V3 · TORNEOS en la pantalla de 7": la pantalla del torneo y el paso por el partido.
// -------------------------------------------------------------------------------------------------
// Como TournamentDetailScreen de la app:
//   - Arriba: volver, la insignia y el nombre de la competición con su edición, las pestañas CLASIFICACIÓN (liga),
//     CUADRO (copa) o PISTAS (pool) y FICHA, y la papelera (descartar, con confirmación).
//   - Liga: la tabla (#, ▲▼, equipo con sus fotos, PJ G E P Dif Pts; E solo si se puede empatar) y la tarjeta de la
//     final. Copa: el cuadro por columnas (por decidir, pase directo, ganador en verde, la final en dorado y el
//     SIGUIENTE señalado). Pool: las pistas (la 1 con corona, el banquillo, ▲▼ con marco verde o rojo) y el reloj.
//   - A la derecha: la tarjeta del siguiente partido (BLANCO y AZUL con fotos y marco de rango, nombres, equipo fijo,
//     pastillas, pronóstico, banquillo, «se dejó a medias») con ▶ SIGUIENTE PARTIDO; o «Ronda N terminada» con
//     ▶ SIGUIENTE RONDA en el pool (las pistas nuevas salen de golpe, sin la animación de la app); las herramientas
//     (⚙ REGLAS, ↶ REPETIR ÚLTIMO PARTIDO) y la lista de jornadas o rondas (se desliza: es una lista).
//   - Torneo terminado: el banner del campeón con RESUMEN (la ceremonia).
// Antes de cada partido, el cartel de fase en franja (GRAN FINAL, SEMIFINAL, JORNADA 3…) 3,5 s o hasta tocar.
// Cambiar de pestaña solo rehace lo de debajo de las pestañas; el reloj del pool se repasa cada 500 ms y solo
// cambia su texto si cambia.

#ifndef MFV3_UI_TORNEO_H
#define MFV3_UI_TORNEO_H

#include "ui_torneo_base.h"

namespace tor {

// Reglas del editor de REGLAS (un torneo ya empezado: solo cambia lo que queda por jugar).
static torneos::Rules edRules, edFinal;
static uint8_t edBestOf = 1;

#include "ui_torneo_nuevo.h"
#include "ui_torneo_creador.h"

// ---------------------------------------------------------------- estado de la pantalla del torneo
static uint8_t tab = 0;          // 0 juego (clasificación, cuadro o pistas), 1 ficha
static bool fromList = false;    // se abrió desde el palmarés (volver allí)
static lv_obj_t* host = nullptr; // lo de debajo de las pestañas
static lv_obj_t* tabPill[2];
static int16_t playing = -1;     // cruce que se está jugando (para el .ino)
static char seenId[48] = "";     // pool: última ronda ya presentada (como el localStorage de la app)
static uint16_t seenRound = 0;
static uint32_t builtRevision = 0;
static uint32_t tickAt = 0;

static void buildDetailBody();
static void buildReport(lv_obj_t* parent);  // ui_torneo_ficha.h
static void startCeremony();                 // ui_torneo_ceremonia.h

static void openTournament(const char* id) {
  if (!torneos::alloc(cur)) return ui::toast("No hay memoria para el torneo");
  if (!curOk || strcmp(cur.id, id)) {
    if (!torneos::load(id, cur)) {
      curOk = false;
      return ui::toast("No se pudo abrir el torneo");
    }
  }
  curOk = true;
  tab = cur.status == torneos::Status::Finished ? 1 : 0;
  go(View::Detail);
}

static int16_t nextIdx() { return curOk ? torneos::nextFixture(cur) : -1; }
static bool active_() { return curOk && cur.status == torneos::Status::Active; }

// El último partido apuntado (el que deshace REPETIR ÚLTIMO PARTIDO), con su fila del historial si está.
static const partidos::Row* lastRecorded() {
  if (!curOk || !cur.nPlayed) return nullptr;
  return rowOf(cur.played[cur.nPlayed - 1].r.matchId);
}
static uint16_t newRound() {
  int16_t n = nextIdx();
  if (!cur.hasLadder || !active_() || n < 0) return 0;
  uint16_t r = cur.fix[n].round;
  if (r < 2) return 0;
  for (uint16_t i = 0; i < cur.nFix; i++)
    if (cur.fix[i].round == r && played_(cur.fix[i])) return 0;
  if (!strcmp(seenId, cur.id) && seenRound >= r) return 0;
  return r;
}

// ---------------------------------------------------------------- cabecera y pestañas
static void onDetailBack(lv_event_t*) {
  if (fromList && cur.status != torneos::Status::Active) return go(View::Honours);
  // En juego: a «¿Quién juega?» con los mismos ya marcados.
  const char* ids[torneos::MAX_ENTRANTS];
  uint8_t n = entrantsOf(cur, ids);
  pickIds(ids, n);
  go(View::Players);
}
static void paintTabs() {
  for (int i = 0; i < 2; i++) {
    bool on = tab == i;
    lv_obj_set_style_border_color(tabPill[i], ui::hex(on ? ui::color::ACCENT : ui::color::LINE), 0);
    lv_obj_set_style_border_width(tabPill[i], on ? 2 : 1, 0);
    lv_obj_set_style_bg_color(tabPill[i], ui::hex(on ? 0x123a55 : 0x0c1a2e), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(tabPill[i], 0), ui::hex(on ? ui::color::TEXT : ui::color::TEXT2), 0);
  }
}
static void onTab(lv_event_t* e) {
  uint8_t t = (uint8_t)argOf(e);
  if (t == tab) return;
  tab = t;
  paintTabs();
  buildDetailBody();
}
static void discardCurYes() {
  char id[48];
  snprintf(id, sizeof(id), "%s", cur.id);
  const char* ids[torneos::MAX_ENTRANTS];
  uint8_t n = entrantsOf(cur, ids);
  pickIds(ids, n);
  curOk = false;
  if (torneos::discard(id)) ui::toast("Torneo descartado");
  else ui::toast("No se pudo descartar");
  go(View::Players);  // de vuelta a elegir jugadores, con los mismos ya marcados
}
static void onDiscardCur(lv_event_t*) {
  uint16_t done, total;
  torneos::fixtureProgress(cur, done, total);
  askDiscard(cur.name, done, discardCurYes);
}
static const char* compName() { return cur.competition[0] ? cur.competition : cur.templateName[0] ? cur.templateName : cur.name; }

static void buildDetail() {
  char sub[48];
  if (cur.templateName[0] && cur.edition) snprintf(sub, sizeof(sub), "%uª EDICIÓN%s", cur.edition, cur.ranked ? " · ELO" : "");
  else snprintf(sub, sizeof(sub), "%s%s", kindTitle(cur.format), cur.ranked ? " · ELO" : "");
  lv_obj_t* band = ui::box(scr, 0, 0, 800, 64);
  lv_obj_set_style_bg_color(band, ui::hex(col::BAND), 0);
  lv_obj_set_style_bg_opa(band, 235, 0);
  lv_obj_set_style_border_side(band, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(band, 1, 0);
  lv_obj_set_style_border_color(band, ui::hex(ui::color::LINE), 0);
  // Banner: franja de su color abajo de la cabecera (la app pone su imagen).
  if (bannerIndex(cur.banner) >= 0) {
    lv_obj_t* bn = ui::box(scr, 0, 60, 800, 4);
    lv_obj_set_style_bg_color(bn, ui::hex(LOGO_COLORS[bannerIndex(cur.banner)]), 0);
    lv_obj_set_style_bg_opa(bn, LV_OPA_COVER, 0);
  }
  backBtn(scr, 12, 8, onDetailBack);
  crest(scr, cur.logo, compName(), 70, 12, 40);
  line(scr, compName(), 900, 24, col::GOLD, 120, 9, 352);
  line(scr, sub, 800, 13, ui::color::TEXT2, 120, 38, 352);
  const char* t0 = cur.format == Format::Bracket ? "CUADRO" : cur.hasLadder ? "PISTAS" : "CLASIFICACIÓN";
  int w0 = textW(t0, 800, 14) + 36, w1 = textW("FICHA", 800, 14) + 36;
  int right = active_() ? 732 : 788;
  for (int i = 0; i < 2; i++) {
    int w = i ? w1 : w0, x = i ? right - w1 : right - w1 - 6 - w0;
    tabPill[i] = ui::box(scr, x, 10, w, 44);
    lv_obj_set_style_radius(tabPill[i], 999, 0);
    lv_obj_set_style_bg_opa(tabPill[i], LV_OPA_COVER, 0);
    lv_obj_add_flag(tabPill[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tabPill[i], onTab, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* l = ui::text(tabPill[i], i ? "FICHA" : t0, 800, 14, ui::color::TEXT, 0.7f);
    lv_obj_center(l);
  }
  paintTabs();
  if (active_()) {
    lv_obj_t* tr = fichas::btn(scr, 740, 10, 48, 44, "", 14, Kind::Danger, onDiscardCur);
    icon(tr, Icon::Trash, 12, 10, 24, 0xffd2d8);
  }
  host = ui::box(scr, 0, 64, 800, 416);
  buildDetailBody();
}

// ---------------------------------------------------------------- piezas del juego
// Fila de un cruce (jornadas): punto del lado, fotos pequeñas, nombre · marcador · nombre, fotos y punto.
static void dot(lv_obj_t* p, int x, int y, bool blue) {
  lv_obj_t* d = ui::box(p, x, y, 10, 10);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(d, ui::hex(blue ? ui::color::BLUE_TEAM : ui::color::WHITE_TEAM), 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
}
static void fixtureRow(lv_obj_t* parent, const torneos::Fixture& f, int x, int y, int w, bool isNext) {
  lv_obj_t* r = ui::box(parent, x, y, w, 34);
  lv_obj_set_style_radius(r, 8, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(r, ui::hex(isNext ? 0x10304a : played_(f) ? 0x0e1b2e : 0x0a1424), 0);
  if (isNext) {
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, ui::hex(ui::color::ACCENT), 0);
  }
  int mid = w / 2, sw = 70;
  dot(r, 6, 12, false);
  int pw = teamFaces(r, cur, f.white, 20, 6, 22);
  bool ww = f.winner >= 0 && f.winner == f.white, bw = f.winner >= 0 && f.winner == f.blue;
  line(r, teamName(cur, f.white), ww ? 900 : 800, 13, ww ? col::OK : ui::color::TEXT, 24 + pw, 9, mid - sw / 2 - 28 - pw);
  char s[24];
  uint32_t sc = ui::color::TEXT;
  if (isNext && !f.hasSeries) {
    snprintf(s, sizeof(s), "SIGUIENTE");
    sc = ui::color::ACCENT;
  } else if (f.bye) snprintf(s, sizeof(s), "pase");
  else if (f.skipped) {
    snprintf(s, sizeof(s), "no se jugó");
    sc = ui::color::TEXT3;
  } else if (!fixtureScore(f, s, sizeof(s))) {
    snprintf(s, sizeof(s), "vs");
    sc = ui::color::TEXT3;
  }
  line(r, s, 900, s[0] == 'S' || s[0] == 'n' ? 11 : 14, sc, mid - sw / 2, 9, sw, LV_TEXT_ALIGN_CENTER);
  dot(r, w - 16, 12, true);
  int bp = teamFaces(r, cur, f.blue, w - 20, 6, 22, true);
  line(r, teamName(cur, f.blue), bw ? 900 : 800, 13, bw ? col::OK : ui::color::TEXT, mid + sw / 2 + 4, 9, mid - sw / 2 - 28 - bp,
       LV_TEXT_ALIGN_RIGHT);
}

// Una pista del pool: su número (la 1 con corona; la final con la copa), las dos parejas y el marcador.
static void ladderPlayer(lv_obj_t* p, const char* id, int x, int y, int size, uint16_t round) {
  int8_t mv = 0;
  if (round) torneos::ladderMoves(cur, round, id, mv);
  if (mv) {
    lv_obj_t* fr = ui::box(p, x - 3, y - 3, size + 6, size + 6);
    lv_obj_set_style_radius(fr, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(fr, 3, 0);
    lv_obj_set_style_border_color(fr, ui::hex(mv > 0 ? col::OK : col::BAD), 0);
  }
  face(p, id, x, y, size);
  if (mv) icon(p, mv > 0 ? Icon::Up : Icon::Down, x + size - 8, y + size - 10, 14, mv > 0 ? col::OK : col::BAD);
  line(p, nameOf(id), 800, 12, ui::color::TEXT, x - 12, y + size + 2, size + 24, LV_TEXT_ALIGN_CENTER);
}
static int ladderCourt(lv_obj_t* parent, const torneos::Fixture& f, int y, int w, uint16_t moveRound, bool isNext) {
  bool top = f.court == 1, fin = f.isFinal;
  int h = top ? 74 : 64, ps = top ? 38 : 32;
  lv_obj_t* r = ui::box(parent, 0, y, w, h);
  lv_obj_set_style_radius(r, 10, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(r, ui::hex(top ? 0x1c1a10 : 0x0e1b2e), 0);
  lv_obj_set_style_border_width(r, isNext || top ? 1 : 0, 0);
  lv_obj_set_style_border_color(r, ui::hex(isNext ? ui::color::ACCENT : col::GOLD), 0);
  // Número de la pista
  lv_obj_t* b = ui::box(r, 6, 6, 70, h - 12);
  lv_obj_set_style_radius(b, 8, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, ui::hex(top ? 0x3a3214 : 0x16243a), 0);
  if (fin) icon(b, Icon::Trophy, 23, 6, 24, col::GOLD);
  else if (top) icon(b, Icon::Crown, 23, 4, 24, col::GOLD);
  char c[16];
  if (fin) snprintf(c, sizeof(c), "FINAL");
  else snprintf(c, sizeof(c), "PISTA %u", f.court);
  line(b, c, 900, 12, top || fin ? col::GOLD : ui::color::TEXT, 0, top || fin ? h - 34 : (h - 12) / 2 - 8, 70, LV_TEXT_ALIGN_CENTER);
  int pairW = 2 * (ps + 22), sx = 84;
  for (int s = 0; s < 2; s++) {
    int16_t tm = s ? f.blue : f.white;
    int px = s ? w - 8 - pairW : sx;
    if (tm < 0) continue;
    bool won = f.winner >= 0 && f.winner == tm;
    if (won) {
      lv_obj_t* g = ui::box(r, px - 4, 3, pairW + 8, h - 6);
      lv_obj_set_style_radius(g, 8, 0);
      lv_obj_set_style_bg_color(g, ui::hex(0x0f3a26), 0);
      lv_obj_set_style_bg_opa(g, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(g, 1, 0);
      lv_obj_set_style_border_color(g, ui::hex(col::OK), 0);
    }
    const torneos::Team& t = cur.teams[tm];
    for (uint8_t k = 0; k < t.n; k++) ladderPlayer(r, t.player[k], px + 11 + k * (ps + 22), 6, ps, moveRound);
  }
  char s[24];
  uint32_t sc = ui::color::TEXT;
  if (isNext && !f.hasSeries) {
    snprintf(s, sizeof(s), "SIGUIENTE");
    sc = ui::color::ACCENT;
  } else if (f.skipped) {
    snprintf(s, sizeof(s), "no se jugó");
    sc = ui::color::TEXT3;
  } else if (!fixtureScore(f, s, sizeof(s))) {
    snprintf(s, sizeof(s), "vs");
    sc = ui::color::TEXT3;
  }
  int l = sx + pairW + 4, rr = w - 8 - pairW - 4;
  // El siguiente: un ▶ (la pista ya va con su marco; «SIGUIENTE» no cabe entre las dos parejas).
  if (s[0] == 'S') icon(r, Icon::Play, (l + rr) / 2 - 9, h / 2 - 9, 18, ui::color::ACCENT);
  else line(r, s, 900, s[0] == 'n' ? 11 : 15, sc, l, h / 2 - 9, rr - l, LV_TEXT_ALIGN_CENTER);
  return h;
}

// ---------------------------------------------------------------- panel izquierdo: clasificación, cuadro o pistas
static void fillClock(char* b, size_t n, int) {
  bool finalOn = false;
  for (uint16_t i = 0; i < cur.nFix; i++)
    if (cur.fix[i].isFinal) finalOn = true;
  if (!active_()) b[0] = 0;
  else if (finalOn) snprintf(b, n, "Toca la final");
  else if (cur.ladder.byRounds) {
    int16_t nx = nextIdx();
    snprintf(b, n, "Ronda %u de %u", nx >= 0 ? cur.fix[nx].round : cur.ladder.rounds, cur.ladder.rounds);
  } else {
    torneos::Clock c = torneos::ladderClock(cur, nowMs());
    if (!c.started) snprintf(b, n, "%u min", cur.ladder.minutes);
    else if (c.remainingMs > 0) snprintf(b, n, "Quedan %ld min", (long)((c.remainingMs + 59999) / 60000));
    else snprintf(b, n, "Tiempo cumplido");
  }
}
static void leftLadder(lv_obj_t* p, int w, int h) {
  uint16_t nr = newRound();
  int16_t nx = nextIdx();
  uint16_t maxR = 0;
  for (uint16_t i = 0; i < cur.nFix; i++) maxR = cur.fix[i].round > maxR ? cur.fix[i].round : maxR;
  // Con una ronda aún sin presentar, se siguen viendo las pistas de la que acaba de terminar.
  uint16_t round = nr ? nr - 1 : nx >= 0 ? cur.fix[nx].round : maxR;
  char lab[32], cap[48];
  torneos::roundLabel(cur, round, lab, sizeof(lab));
  // Por rondas, el reloj ya dice «Ronda r de R»: aquí no se repite (salvo con la ronda anterior aún a la vista).
  if (cur.ladder.byRounds && active_() && !nr) snprintf(cap, sizeof(cap), "PISTAS");
  else snprintf(cap, sizeof(cap), "PISTAS · %s", lab);
  for (char* c = cap; *c; c++)
    if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
  caption(p, cap, 12, 12);
  lv_obj_t* clk = panel(p, w - 170, 6, 158, 30, col::POOL, 0x1d1030);
  lv_obj_set_style_radius(clk, 999, 0);
  val(clk, 6, 6, 146, 800, 14, fillClock, 0, 0xf0c4ff, LV_TEXT_ALIGN_CENTER);
  if (!active_()) ui::setVisible(clk, false);
  lv_obj_t* list = scroller(p, 8, 42, w - 16, h - 50);
  int y = 0;
  const torneos::Fixture* bench = nullptr;
  for (uint8_t c = 1; c <= torneos::POOL_MAX_PAIRS; c++)
    for (uint16_t i = 0; i < cur.nFix; i++) {
      const torneos::Fixture& f = cur.fix[i];
      if (f.round != round || f.court != c) continue;
      y += ladderCourt(list, f, y, w - 20, round, (int16_t)i == nx && !nr) + 6;
      if (f.nResting && !bench) bench = &f;
    }
  if (bench) {
    lv_obj_t* r = ui::box(list, 0, y, w - 20, 64);
    lv_obj_set_style_radius(r, 10, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r, ui::hex(0x0a1220), 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, ui::hex(ui::color::LINE), 0);
    line(r, "BANQUILLO", 900, 12, ui::color::TEXT2, 6, 24, 76, LV_TEXT_ALIGN_CENTER);
    for (uint8_t k = 0; k < bench->nResting; k++) ladderPlayer(r, bench->resting[k], 95 + k * 54, 6, 32, round);
    y += 70;
  }
  char note[200];
  snprintf(note, sizeof(note), "%sel que gana sube y el que pierde baja.%s", cur.ladder.swap ? "Con cambio de pareja: " : "Sin cambio de pareja: ",
           !cur.ladder.byRounds ? " Cuando ya solo dé para un partido, la final en la pista 1." : "");
  para(list, note, 400, 13, ui::color::TEXT2, 2, y + 2, w - 24);
}

static void leftLeague(lv_obj_t* p, int w, int h) {
  lv_obj_t* list = scroller(p, 0, 0, w, h);
  // (Sin rótulo «CLASIFICACIÓN»: ya lo dice la pestaña.)
  bool draws = cur.rules.allowDraw;
  // Columnas desde la derecha: Pts, Dif, P, (E), G, PJ.
  const char* head[6] = {"PJ", "G", "E", "P", "Dif", "Pts"};
  int colX[6], x = w - 14;
  const int CW = 32;
  for (int c = 5; c >= 0; c--) {
    if (c == 2 && !draws) {
      colX[c] = -1;
      continue;
    }
    x -= c >= 4 ? CW + 4 : CW;
    colX[c] = x;
  }
  int nameEnd = colX[0] - 6;
  for (int c = 0; c < 6; c++)
    if (colX[c] >= 0) line(list, head[c], 800, 12, ui::color::TEXT3, colX[c], 12, c >= 4 ? CW + 4 : CW, LV_TEXT_ALIGN_CENTER);
  line(list, "#", 800, 12, ui::color::TEXT3, 8, 12, 20, LV_TEXT_ALIGN_CENTER);
  line(list, "EQUIPO", 800, 12, ui::color::TEXT3, 48, 12, 100);
  static torneos::StandingRow rows[torneos::MAX_TEAMS];
  uint8_t n = torneos::standings(cur, rows, torneos::MAX_TEAMS);
  int y = 30;
  for (uint8_t i = 0; i < n; i++, y += 36) {
    const torneos::StandingRow& s = rows[i];
    bool lead = i == 0 && s.played > 0;
    lv_obj_t* r = ui::box(list, 4, y, w - 12, 34);
    lv_obj_set_style_radius(r, 8, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r, ui::hex(lead ? 0x2a2410 : i % 2 ? 0x0c1828 : 0x0e1b2e), 0);
    char num[8];
    snprintf(num, sizeof(num), "%u", i + 1);
    line(r, num, 900, 14, lead ? col::GOLD : ui::color::TEXT2, 4, 8, 20, LV_TEXT_ALIGN_CENTER);
    if (s.move) icon(r, s.move > 0 ? Icon::Up : Icon::Down, 28, 11, 12, s.move > 0 ? col::OK : col::BAD);
    int pw = teamFaces(r, cur, s.team, 44, 5, 24);
    line(r, teamName(cur, s.team), 800, 14, lead ? col::GOLD : ui::color::TEXT, 48 + pw, 8, nameEnd - 4 - 48 - pw - 4);
    int16_t v[6] = {s.played, s.wins, s.draws, s.losses, s.diff, s.points};
    for (int c = 0; c < 6; c++) {
      if (colX[c] < 0) continue;
      char t[8];
      if (c == 4 && v[c] > 0) snprintf(t, sizeof(t), "+%d", v[c]);
      else snprintf(t, sizeof(t), "%d", v[c]);
      line(r, t, c == 5 ? 900 : 800, 14, c == 5 ? (lead ? col::GOLD : ui::color::TEXT) : ui::color::TEXT2, colX[c] - 4, 8,
           c >= 4 ? CW + 4 : CW, LV_TEXT_ALIGN_CENTER);
    }
  }
  // La final (1º contra 2º).
  int16_t fin = -1;
  for (uint16_t i = 0; i < cur.nFix; i++)
    if (cur.fix[i].isFinal) fin = (int16_t)i;
  if (fin >= 0) {
    const torneos::Fixture& f = cur.fix[fin];
    y += 8;
    lv_obj_t* card = panel(list, 4, y, w - 12, 84, col::GOLD, 0x1c1a10);
    char rs[24], cap[96];
    rulesChip(torneos::fixtureRules(cur, f), rs, sizeof(rs));
    snprintf(cap, sizeof(cap), "FINAL · 1º VS 2º · %s · %s", f.bestOf > 1 ? "AL MEJOR DE 3" : "PARTIDO ÚNICO", rs);
    for (char* c = cap; *c; c++)
      if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
    line(card, cap, 800, 12, col::GOLD, 10, 8, w - 32);
    if (f.white >= 0) fixtureRow(card, f, 6, 36, w - 26, fin == nextIdx());
    else line(card, "Se decide al acabar la fase regular.", 400, 14, ui::color::TEXT2, 10, 44, w - 32);
  }
}

static void leftBracket(lv_obj_t* p, int w, int h) {
  uint16_t maxR = 0;
  for (uint16_t i = 0; i < cur.nFix; i++) maxR = cur.fix[i].round > maxR ? cur.fix[i].round : maxR;
  int16_t nx = nextIdx();
  int gap = 8, cw = (w - 16 - (maxR - 1) * gap) / (maxR ? maxR : 1);
  for (uint16_t r = 1; r <= maxR; r++) {
    bool fin = r == maxR;
    int cx = 8 + (r - 1) * (cw + gap);
    char lab[32];
    torneos::roundLabel(cur, r, lab, sizeof(lab));
    for (char* c = lab; *c; c++)
      if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
    if (fin) icon(p, Icon::Trophy, cx, 9, 16, col::GOLD);
    line(p, lab, 800, 12, fin ? col::GOLD : ui::color::TEXT2, cx + (fin ? 20 : 0), 10, cw - 20);
    uint16_t cnt = 0;
    for (uint16_t i = 0; i < cur.nFix; i++)
      if (cur.fix[i].round == r) cnt++;
    int top = 34, avail = h - top - 8, bh = 62, k = 0;
    for (uint16_t i = 0; i < cur.nFix; i++) {
      const torneos::Fixture& f = cur.fix[i];
      if (f.round != r) continue;
      int slot = avail / (cnt ? cnt : 1), by = top + k * slot + (slot - bh) / 2;
      k++;
      bool isNext = (int16_t)i == nx;
      lv_obj_t* b = ui::box(p, cx, by, cw, bh);
      lv_obj_set_style_radius(b, 8, 0);
      lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(b, ui::hex(fin ? 0x1c1a10 : 0x0e1b2e), 0);
      lv_obj_set_style_border_width(b, isNext || fin ? 2 : 1, 0);
      lv_obj_set_style_border_color(b, ui::hex(isNext ? ui::color::ACCENT : fin ? col::GOLD : ui::color::LINE), 0);
      const torneos::Result* row = playedOf(cur, f.matchId);
      for (int s = 0; s < 2; s++) {
        int16_t tm = s ? f.blue : f.white;
        int ry = 4 + s * 28;
        if (tm < 0) {
          line(b, f.bye ? "pase directo" : "por decidir", 400, 12, ui::color::TEXT3, 8, ry + 6, cw - 16);
          continue;
        }
        bool win = f.winner >= 0 && f.winner == tm, out = f.winner >= 0 && f.winner != tm;
        int pw = teamFaces(b, cur, tm, 5, ry + 2, 20);
        int scoreW = 22;
        line(b, teamName(cur, tm), win ? 900 : 800, 12, win ? col::OK : out ? ui::color::TEXT3 : ui::color::TEXT, 8 + pw, ry + 5,
             cw - 16 - pw - scoreW);
        char sc[8] = "";
        if (f.hasSeries && f.bestOf > 1) snprintf(sc, sizeof(sc), "%u", s ? f.serB : f.serW);
        else if (row && !f.bye) snprintf(sc, sizeof(sc), "%u", s ? row->b : row->w);
        if (sc[0]) line(b, sc, 900, 14, win ? col::OK : ui::color::TEXT2, cw - 4 - scoreW, ry + 4, scoreW - 2, LV_TEXT_ALIGN_RIGHT);
      }
      if (isNext) {
        int cwid = chip(p, "SIGUIENTE", cx + cw - 90, by - 10, ui::color::ACCENT, ui::color::ACCENT, 11);
        (void)cwid;
      }
    }
  }
}

// ---------------------------------------------------------------- panel derecho: siguiente partido y jornadas
static void onPlayNext(lv_event_t*) {
  int16_t n = nextIdx();
  if (n < 0 || !hooks.play) return;
  playing = n;
  hooks.play(n);
}
static void onResumePending(lv_event_t*) {
  if (hooks.resume) hooks.resume();
}
static void onNextRound(lv_event_t*) {
  snprintf(seenId, sizeof(seenId), "%s", cur.id);
  seenRound = newRound();
  buildDetailBody();  // las pistas nuevas, de golpe (sin la animación de la app)
}
static void onRulesEdit(lv_event_t*) { go(View::Rules); }
static void undoYes() {
  char removed[48] = "";
  if (!torneos::undoLast(cur, removed, sizeof(removed))) return ui::toast("No se pudo deshacer");
  if (removed[0] && hooks.removeMatch) hooks.removeMatch(removed);
  torneos::save(cur);
  ui::toast("Resultado borrado: el partido vuelve a estar pendiente");
  go(View::Detail);
}
// Un partido del historial en una línea: fotos, nombres y marcador.
static void matchLine(lv_obj_t* p, const partidos::Row& r, int x, int y, int w) {
  for (int t = 0; t < 2; t++) {
    int n = r.n[t];
    char names[64] = "";
    for (int i = 0; i < n; i++) {
      if (i) strncat(names, " + ", sizeof(names) - strlen(names) - 1);
      strncat(names, r.name[t][i], sizeof(names) - strlen(names) - 1);
    }
    int half = w / 2 - 40;
    if (!t) {
      for (int i = 0; i < n; i++) {
        char id[40];
        store->idOf(r.code[t][i], id, sizeof(id));
        face(p, id, x + i * 39, y, 36);
      }
      line(p, n ? names : "BLANCO", 800, 17, r.winner == 'w' ? col::OK : ui::color::TEXT, x + n * 39 + 4, y + 8, half - n * 39 - 4);
    } else {
      for (int i = 0; i < n; i++) {
        char id[40];
        store->idOf(r.code[t][i], id, sizeof(id));
        face(p, id, x + w - 36 - i * 39, y, 36);
      }
      line(p, n ? names : "AZUL", 800, 17, r.winner == 'b' ? col::OK : ui::color::TEXT, x + w / 2 + 40, y + 8, half - n * 39 - 4,
           LV_TEXT_ALIGN_RIGHT);
    }
  }
  char s[16];
  snprintf(s, sizeof(s), "%u–%u", r.w, r.b);
  line(p, s, 900, 24, ui::color::TEXT, x + w / 2 - 40, y + 4, 80, LV_TEXT_ALIGN_CENTER);
}
static void onUndo(lv_event_t*) {
  const partidos::Row* r = lastRecorded();
  if (!r) return;
  lv_obj_t* card = ask("¿Repetir el último partido?",
                       "Se borra este resultado (también de las estadísticas) y el partido vuelve a quedar pendiente para jugarlo otra vez.",
                       "NO", "BORRAR Y REPETIR", undoYes, true, 136);
  matchLine(card, *r, 24, 70, 592);
}

// La tarjeta del siguiente partido. Devuelve su alto.
static int nextCard(lv_obj_t* p, int x, int y, int w, int16_t ni) {
  const torneos::Fixture& f = cur.fix[ni];
  lv_obj_t* c = panel(p, x, y, w, 100, 0x3b6a9a, 0x1a2a42);
  lv_obj_set_style_bg_grad_color(c, ui::hex(0x15426a), 0);
  lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_HOR, 0);
  caption(c, "SIGUIENTE PARTIDO", 12, 10, ui::color::TEXT);
  // Pastillas: pista, jornada, partido n de N, serie y reglas (las que no caben, a la línea de abajo).
  char ch[5][40];
  int nc = 0;
  uint16_t done, total;
  torneos::fixtureProgress(cur, done, total);
  if (cur.hasLadder && f.court) {
    if (f.isFinal) snprintf(ch[nc++], 40, "Final · pista 1");
    else snprintf(ch[nc++], 40, "Pista %u", f.court);
  }
  torneos::roundLabel(cur, f.round, ch[nc++], 40);
  if (cur.hasLadder) snprintf(ch[nc++], 40, "Partido %u", done + 1);
  else snprintf(ch[nc++], 40, "Partido %u de %u", done + 1, total);
  if (f.bestOf > 1) snprintf(ch[nc++], 40, "Serie %u–%u · partido %u", f.serW, f.serB, f.nMatches + 1);
  rulesChip(torneos::fixtureRules(cur, f), ch[nc++], 40);
  int cx = 12, cy = 32;
  for (int i = 0; i < nc; i++) {
    int cw = textW(ch[i], 800, 12) + 18;
    if (cx + cw > w - 10) {
      cx = 12;
      cy += 28;
    }
    chip(c, ch[i], cx, cy, i == 0 && cur.hasLadder ? 0xf0c4ff : ui::color::TEXT, i == 0 && cur.hasLadder ? col::POOL : ui::color::LINE, 12);
    cx += cw + 6;
  }
  int yy = cy + 32;
  bool pend = hooks.pending && hooks.pending();
  if (f.abandonedAt && !pend) {
    lv_obj_t* a = panel(c, 12, yy, w - 24, 30, col::GOLD, 0x2a2410);
    icon(a, Icon::Pause, 10, 7, 16, col::GOLD);
    line(a, "Se dejó a medias · se empieza de cero", 800, 13, 0xffe9a8, 34, 7, w - 70);
    yy += 38;
  }
  // BLANCO · VS · AZUL
  int side = (w - 24 - 44) / 2;
  int most = 1;
  for (int s = 0; s < 2; s++) {
    int16_t tm = s ? f.blue : f.white;
    if (tm >= 0 && cur.teams[tm].n > most) most = cur.teams[tm].n;
  }
  int ps = most > 1 ? 54 : 72;
  for (int s = 0; s < 2; s++) {
    int16_t tm = s ? f.blue : f.white;
    int sx = s ? w - 12 - side : 12;
    lv_obj_t* lab = ui::box(c, sx + side / 2 - 38, yy, 76, 22);
    lv_obj_set_style_radius(lab, 6, 0);
    lv_obj_set_style_bg_opa(lab, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(lab, ui::hex(s ? 0x0c2a48 : 0x101826), 0);
    lv_obj_set_style_border_width(lab, 1, 0);
    lv_obj_set_style_border_color(lab, ui::hex(s ? 0x4fb0ff : ui::color::WHITE_TEAM), 0);
    lv_obj_t* lt = ui::text(lab, s ? "AZUL" : "BLANCO", 900, 12, s ? 0x6fb6f5 : ui::color::WHITE_TEAM, 1.5f);
    lv_obj_center(lt);
    if (tm < 0) continue;
    const torneos::Team& t = cur.teams[tm];
    int rowW = t.n * (ps + 10) - 10, px = sx + (side - rowW) / 2;
    for (uint8_t k = 0; k < t.n; k++) faceRank(c, t.player[k], px + k * (ps + 10), yy + 30, ps, false);
    char names[64] = "";
    for (uint8_t k = 0; k < t.n; k++) {
      if (k) strncat(names, " + ", sizeof(names) - strlen(names) - 1);
      strncat(names, nameOf(t.player[k]), sizeof(names) - strlen(names) - 1);
    }
    for (char* q = names; *q; q++)
      if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
    line(c, names, 900, 14, ui::color::TEXT, sx, yy + 36 + ps, side, LV_TEXT_ALIGN_CENTER);
    // Equipo fijo (con nombre propio): su insignia y su nombre.
    if (t.logo[0] || t.clubId[0]) {
      crest(c, t.logo, t.name, sx + 4, yy + 58 + ps, 18);
      line(c, t.name, 800, 12, ui::color::TEXT2, sx + 26, yy + 60 + ps, side - 28);
    }
  }
  lv_obj_t* vs = ui::text(c, "VS", 900, 26, col::GOLD, 1);
  lv_obj_set_pos(vs, 12 + side + 4, yy + 30 + ps / 2 - 16);
  bool club = false;
  for (int s = 0; s < 2; s++) {
    int16_t tm = s ? f.blue : f.white;
    if (tm >= 0 && (cur.teams[tm].logo[0] || cur.teams[tm].clubId[0])) club = true;
  }
  yy += 58 + ps + (club ? 22 : 0);
  // Pronóstico (si hay datos de ELO y enfrentamientos).
  uint8_t wp = 0, bp = 0;
  if (f.white >= 0 && f.blue >= 0 &&
      torneos::predict(cur.teams[f.white].player, cur.teams[f.white].n, cur.teams[f.blue].player, cur.teams[f.blue].n, wp, bp)) {
    char a[8], b[8];
    snprintf(a, sizeof(a), "%u %%", wp);
    snprintf(b, sizeof(b), "%u %%", bp);
    line(c, a, 900, 15, ui::color::TEXT, 12, yy, 52);
    line(c, b, 900, 15, 0x6fb6f5, w - 64, yy, 52, LV_TEXT_ALIGN_RIGHT);
    lv_obj_t* bar = ui::box(c, 68, yy + 2, w - 136, 16);
    lv_obj_set_style_radius(bar, 8, 0);
    lv_obj_set_style_bg_color(bar, ui::hex(0x4fb0ff), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_t* fill = ui::box(bar, 0, 0, (w - 136) * wp / 100, 16);
    lv_obj_set_style_radius(fill, 8, 0);
    lv_obj_set_style_bg_color(fill, ui::hex(ui::color::WHITE_TEAM), 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    lv_obj_t* pl = ui::text(bar, "PRONÓSTICO", 900, 11, ui::color::WHITE_INK, 1);
    lv_obj_center(pl);
    yy += 26;
  }
  // Banquillo / descansa.
  if (f.nResting) {
    line(c, cur.hasLadder ? "Banquillo:" : f.nResting > 1 ? "Descansan:" : "Descansa:", 800, 13, ui::color::TEXT2, 12, yy + 4, 90);
    char names[64] = "";
    for (uint8_t k = 0; k < f.nResting; k++) {
      face(c, f.resting[k], 100 + k * 28, yy, 24);
      if (k) strncat(names, ", ", sizeof(names) - strlen(names) - 1);
      strncat(names, nameOf(f.resting[k]), sizeof(names) - strlen(names) - 1);
    }
    line(c, names, 400, 13, ui::color::TEXT2, 104 + f.nResting * 28, yy + 4, w - 116 - f.nResting * 28);
    yy += 30;
  } else if (cur.format == Format::League) {
    // Liga con número impar: quien no juega esta jornada descansa.
    int16_t restT = -1;
    for (uint16_t t = 0; t < cur.nTeams && restT < 0; t++) {
      bool plays = false;
      for (uint16_t i = 0; i < cur.nFix; i++)
        if (cur.fix[i].round == f.round && !cur.fix[i].isFinal && (cur.fix[i].white == (int16_t)t || cur.fix[i].blue == (int16_t)t)) plays = true;
      if (!plays && !f.isFinal) restT = (int16_t)t;
    }
    if (restT >= 0) {
      line(c, cur.teams[restT].n > 1 ? "Descansan:" : "Descansa:", 800, 13, ui::color::TEXT2, 12, yy + 4, 90);
      int pw = teamFaces(c, cur, restT, 100, yy, 24);
      line(c, teamName(cur, restT), 400, 13, ui::color::TEXT2, 104 + pw, yy + 4, w - 116 - pw);
      yy += 30;
    }
  }
  neonGo(c, 12, yy + 4, w - 24, 62, "SIGUIENTE PARTIDO", 20, onPlayNext);
  yy += 78;
  lv_obj_set_height(c, yy);
  return yy;
}

static void rightPanel(lv_obj_t* p, int w, int h) {
  lv_obj_t* list = scroller(p, 0, 0, w, h);
  lv_obj_set_style_pad_bottom(list, 10, 0);
  int y = 8, x = 8, iw = w - 16;
  int16_t ni = nextIdx();
  if (hooks.pending && hooks.pending()) {
    lv_obj_t* b = fichas::btn(list, x, y, iw, 56, "", 14, Kind::Normal, onResumePending);
    lv_obj_set_style_border_color(b, ui::hex(col::GOLD), 0);
    icon(b, Icon::Pause, 12, 18, 20, col::GOLD);
    line(b, "Hay un partido a medias", 800, 14, 0xffe9a8, 40, 18, iw - 200);
    neonGo(b, iw - 150, 6, 142, 44, "CONTINUAR", 14, onResumePending);
    y += 64;
  }
  uint16_t nr = newRound();
  if (nr) {
    lv_obj_t* c = panel(list, x, y, iw, 196, col::POOL, 0x1d1030);
    char t[40];
    snprintf(t, sizeof(t), "RONDA %u TERMINADA", nr - 1);
    caption(c, t, 14, 14, 0xf0c4ff);
    char s[160];
    snprintf(s, sizeof(s), "Los que han ganado suben de pista y los que han perdido bajan%s.", cur.ladder.swap ? "; después, cambio de pareja" : "");
    para(c, s, 400, 16, ui::color::TEXT, 14, 44, iw - 28);
    neonGo(c, 12, 118, iw - 24, 64, "SIGUIENTE RONDA", 20, onNextRound);
    y += 204;
  } else if (ni >= 0 && active_()) {
    y += nextCard(list, x, y, iw, ni) + 8;
  }
  const partidos::Row* last = lastRecorded();
  if (last || active_()) {
    int tx = x;
    if (active_()) {
      iconBtn(list, tx, y, 112, 48, Icon::Gear, "REGLAS", 14, Kind::Normal, onRulesEdit);
      tx += 120;
    }
    if (last) iconBtn(list, tx, y, x + iw - tx, 48, Icon::Undo, "REPETIR ÚLTIMO PARTIDO", 13, Kind::Normal, onUndo);
    y += 58;
  }
  // En la copa los partidos ya están en el cuadro: aquí solo se juega.
  if (cur.format == Format::Bracket) return;
  caption(list, cur.format == Format::Pool ? "RONDAS" : "JORNADAS", x + 4, y + 4);
  y += 28;
  uint16_t maxR = 0;
  for (uint16_t i = 0; i < cur.nFix; i++) maxR = cur.fix[i].round > maxR ? cur.fix[i].round : maxR;
  for (uint16_t r = 1; r <= maxR; r++) {
    bool hasFinalRound = false, any = false, allDone = true, current = false;
    for (uint16_t i = 0; i < cur.nFix; i++) {
      const torneos::Fixture& f = cur.fix[i];
      if (f.round != r) continue;
      if (f.isFinal && !cur.hasLadder) hasFinalRound = true;
      if (f.white < 0 && f.blue < 0) continue;
      any = true;
      if (!played_(f)) allDone = false;
      if ((int16_t)i == ni) current = true;
    }
    if (hasFinalRound || !any) continue;
    char lab[32];
    torneos::roundLabel(cur, r, lab, sizeof(lab));
    line(list, lab, 900, 15, ui::color::TEXT, x + 4, y + 4, 140);
    const char* st = allDone ? "JUGADA" : current ? "EN JUEGO" : "PENDIENTE";
    line(list, st, 800, 12, current ? ui::color::ACCENT : ui::color::TEXT3, x + 120, y + 7, 90);
    y += 28;
    for (uint8_t c = 0; c <= torneos::POOL_MAX_PAIRS; c++)
      for (uint16_t i = 0; i < cur.nFix; i++) {
        const torneos::Fixture& f = cur.fix[i];
        if (f.round != r || (f.white < 0 && f.blue < 0)) continue;
        if (cur.hasLadder ? f.court != c : c != 0) continue;
        fixtureRow(list, f, x, y, iw, (int16_t)i == ni);
        y += 38;
      }
    y += 6;
  }
}

// Banner del campeón (torneo terminado) con RESUMEN.
static void onCeremony(lv_event_t*) { startCeremony(); }
static void championBanner(lv_obj_t* p) {
  if (cur.winner < 0) return;
  const torneos::Team& w = cur.teams[cur.winner];
  lv_obj_t* b = panel(p, 16, 8, 768, 76, col::GOLD, 0x241f0c);
  lv_obj_set_style_bg_grad_color(b, ui::hex(0x0b1424), 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_HOR, 0);
  cup(b, cur.cup, 12, 10, 54);
  caption(b, "CAMPEÓN", 80, 12, col::GOLD);
  int x = 80;
  if (w.logo[0] || w.clubId[0]) {
    crest(b, w.logo, w.name, x, 36, 26);
    x += 32;
  }
  line(b, w.name, 900, 24, ui::color::TEXT, x, 34, 360);
  for (uint8_t k = 0; k < w.n; k++) face(b, w.player[k], 470 + k * 54, 13, 48);
  iconBtn(b, 600, 12, 152, 50, Icon::Play, "RESUMEN", 17, Kind::Normal, onCeremony, nullptr, col::GOLD);
}

static void buildDetailBody() {
  lv_obj_clean(host);
  optCount = valCount = visCount = 0;
  if (tab == 1) {
    buildReport(host);
    refresh();
    return;
  }
  int y = 8;
  if (cur.status == torneos::Status::Finished) {
    championBanner(host);
    y = 92;
  }
  int h = 416 - y - 8;
  bool bracketDone = cur.format == Format::Bracket && !active_();
  if (bracketDone) {
    lv_obj_t* l = panel(host, 16, y, 768, h);
    leftBracket(l, 768, h);
    if (lastRecorded()) iconBtn(l, 560, 4, 200, 40, Icon::Undo, "REPETIR ÚLTIMO PARTIDO", 12, Kind::Normal, onUndo);
    refresh();
    return;
  }
  lv_obj_t* l = panel(host, 16, y, 404, h);
  if (cur.format == Format::Bracket) leftBracket(l, 404, h);
  else if (cur.hasLadder) leftLadder(l, 404, h);
  else leftLeague(l, 404, h);
  lv_obj_t* r = panel(host, 428, y, 356, h);
  rightPanel(r, 356, h);
  refresh();
}

// ---------------------------------------------------------------- REGLAS de un torneo empezado
static bool seriesEditable() { return torneos::finalSeriesEditable(cur); }
static bool curHasFinal() {
  for (uint16_t i = 0; i < cur.nFix; i++)
    if (cur.fix[i].isFinal) return true;
  return cur.format != Format::League || cur.finalKind == torneos::FinalKind::Top2;
}
static bool isEdBestOf(int a) { return edBestOf == a; }
static void onEdBestOf(lv_event_t* e) {
  edBestOf = (uint8_t)argOf(e);
  refresh();
}
static void onRulesCancel(lv_event_t*) { go(View::Detail); }
static void onRulesSave(lv_event_t*) {
  bool fin = curHasFinal();
  torneos::updateRules(cur, edRules, fin ? &edFinal : nullptr, seriesEditable() ? edBestOf : 0);
  torneos::save(cur);
  ui::toast("Reglas cambiadas para los partidos que quedan");
  go(View::Detail);
}
static void buildRules() {
  header("REGLAS DEL TORNEO", nullptr, onRulesCancel);
  caption(scr, "PARTIDOS", 16, 80);
  rulesPicker(scr, 2, 16, 104, 372, false);
  if (curHasFinal()) {
    lv_obj_t* p = panel(scr, 412, 76, 372, 316, col::GOLD, 0x1c1a10);
    icon(p, Icon::Trophy, 12, 10, 20, col::GOLD);
    caption(p, "FINAL", 40, 14, col::GOLD);
    int y = 44;
    if (seriesEditable()) {
      opt(p, 12, y, 170, 56, "1 PARTIDO", 17, isEdBestOf, 1, onEdBestOf);
      opt(p, 190, y, 170, 56, "AL MEJOR DE 3", 17, isEdBestOf, 3, onEdBestOf);
      y += 66;
    }
    rulesPicker(p, 3, 12, y, 346, true);
  }
  footer();
  para(scr, "Solo cambia lo que queda por jugar; los partidos jugados se quedan como están.", 400, 14, ui::color::TEXT2, 24, 420, 380);
  tbtn(scr, 420, 408, 160, 64, "CANCELAR", 18, Kind::Normal, onRulesCancel);
  tbtn(scr, 590, 408, 194, 64, "GUARDAR", 20, Kind::Primary, onRulesSave);
}
static void openRules() {
  edRules = cur.rules;
  edFinal = cur.hasFinalRules ? cur.finalRules : cur.rules;
  edBestOf = 1;
  for (uint16_t i = 0; i < cur.nFix; i++)
    if (cur.fix[i].isFinal) edBestOf = cur.fix[i].bestOf > 1 ? 3 : 1;
}

// ---------------------------------------------------------------- cartel de fase antes del partido
static lv_timer_t* posterTimer = nullptr;
static bool posterDone = false;
static void posterGo() {
  if (posterDone) return;
  posterDone = true;
  if (posterTimer) lv_timer_delete(posterTimer);
  posterTimer = nullptr;
  cartel::hide();
  if (hooks.start) hooks.start();
}
static void onPosterTimer(lv_timer_t*) {
  posterTimer = nullptr;
  posterGo();
}
static void onPosterTap(lv_event_t*) { posterGo(); }
static void buildPoster() {
  if (playing < 0 || playing >= (int16_t)cur.nFix) return;
  const torneos::Fixture& f = cur.fix[playing];
  torneos::Phase ph = torneos::phaseInfo(cur, f);
  static const uint32_t TOP[5] = {0x3a2c08, 0x3a1608, 0x2a0d3a, 0x3a0812, 0x08243a};
  ui::gradient(scr, TOP[ph.tier % 5], 0x05090f);
  int tw = textW(cur.name, 800, 20) + 44;
  if (tw > 760) tw = 760;
  crest(scr, cur.logo, compName(), 400 - tw / 2, 26, 34);
  line(scr, cur.name, 800, 20, ui::color::TEXT, 400 - tw / 2 + 44, 32, tw - 44);
  const cartel::Style* st[5] = {&cartel::GOLD, &cartel::FIRE, &cartel::CHAOS, &cartel::SUDDEN, &cartel::START};
  cartel::show(*st[ph.tier % 5], ph.title, ph.sub, 3500, 170);
  // Los dos equipos con sus fotos.
  for (int s = 0; s < 2; s++) {
    int16_t tm = s ? f.blue : f.white;
    if (tm < 0) continue;
    const torneos::Team& t = cur.teams[tm];
    int cx = s ? 600 : 200, ps = 78, rowW = t.n * (ps + 40) - 40;
    for (uint8_t k = 0; k < t.n; k++) {
      int px = cx - rowW / 2 + k * (ps + 40);
      faceRank(scr, t.player[k], px, 280, ps);
      line(scr, nameOf(t.player[k]), 800, 17, ui::color::TEXT, px - 30, 366, ps + 60, LV_TEXT_ALIGN_CENTER);
    }
    if (t.logo[0] || t.clubId[0]) line(scr, t.name, 900, 20, s ? 0x6fb6f5 : ui::color::WHITE_TEAM, cx - 180, 252, 360, LV_TEXT_ALIGN_CENTER);
  }
  lv_obj_t* vs = ui::text(scr, "VS", 900, 38, col::GOLD, 2);
  lv_obj_align(vs, LV_ALIGN_TOP_MID, 0, 296);
  line(scr, "Toca para empezar", 800, 14, ui::color::TEXT2, 0, 444, 800, LV_TEXT_ALIGN_CENTER);
  ui::hotspot(scr, 0, 0, 800, 480, onPosterTap);
  posterDone = false;
  if (posterTimer) lv_timer_delete(posterTimer);
  posterTimer = lv_timer_create(onPosterTimer, 3500, nullptr);
  lv_timer_set_repeat_count(posterTimer, 1);
}

#include "ui_torneo_ficha.h"
#include "ui_torneo_ceremonia.h"

// ---------------------------------------------------------------- pantallas
static void forget() {
  scr = nullptr;
  pop = host = nullptr;
  kbLabel = nextBtn = chooseNext = stepBar = edList = edMore = nullptr;
  optCount = valCount = visCount = 0;
  if (kindSelTimer) lv_timer_delete(kindSelTimer);
  if (kindPressTimer) lv_timer_delete(kindPressTimer);
  kindSelTimer = kindPressTimer = nullptr;
  kindSel = kindPressed = -1;
  ceremonyLeave();
}
static void onDelete(lv_event_t* e) {
  if (lv_event_get_target_obj(e) == scr) forget();
}

static void go(View v) {
  if ((v == View::Detail || v == View::Rules || v == View::Ceremony || v == View::Poster) && !curOk) v = View::Players;
  if (v == View::Rules && view != View::Rules) openRules();
  if (v != View::Poster && posterTimer) {
    lv_timer_delete(posterTimer);
    posterTimer = nullptr;
    cartel::hide();
  }
  view = v;
  lv_obj_t* old = scr;
  if (old) lv_obj_remove_event_cb(old, onDelete);  // la vieja se borra al cargar esta: que no limpie lo nuevo
  forget();
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  ui::gradient(scr, 0x0f2140, 0x05090f);
  pop = host = nullptr;
  kbLabel = nextBtn = chooseNext = stepBar = nullptr;
  optCount = valCount = visCount = 0;
  switch (v) {
    case View::Players: buildPlayers(); break;
    case View::Kind: buildKind(); break;
    case View::Empty: buildEmpty(); break;
    case View::Choose: buildChoose(); break;
    case View::Pairs: buildPairs(); break;
    case View::Keyboard: buildKeyboard(); break;
    case View::Creator: buildCreator(); break;
    case View::Detail: buildDetail(); break;
    case View::Rules: buildRules(); break;
    case View::Honours: buildHonours(); break;
    case View::Competition: buildCompetition(); break;
    case View::Ceremony: buildCeremony(); break;
    case View::Poster: buildPoster(); break;
  }
  refresh();
  builtRevision = torneos::revision;
  tickAt = lv_tick_get();
  if (scr != lv_screen_active()) ui::show(scr);
}

static bool active() { return scr && lv_screen_active() == scr; }

// Desde Inicio (TORNEO): «¿Quién juega?» sin nadie marcado, como la app.
static void open(jug::Store* s) {
  store = s;
  torneos::loadIndex();
  nPicked = 0;
  fromList = false;
  go(View::Players);
}
// Al volver a la pantalla (del reposo, de AJUSTES → JUGADORES o del partido): la de antes, rehecha.
static void show(jug::Store* s) {
  store = s;
  if (!active()) go(view);
}
// Del partido al torneo: el resultado ya está apuntado (o el cruce marcado «a medias»). `champion`: ver la
// ceremonia (el partido cerró el torneo).
static void backFromMatch(jug::Store* s, bool champion) {
  store = s;
  playing = -1;
  tab = 0;
  go(champion && curOk && cur.status == torneos::Status::Finished ? View::Ceremony : View::Detail);
}

// Cada medio segundo: el reloj del pool y lo que cambie solo; si los torneos cambian (la nube), se rehace.
static void tick() {
  if (!active()) return;
  ceremonyTick();
  if (lv_tick_elaps(tickAt) < 500) return;
  tickAt = lv_tick_get();
  if (torneos::revision != builtRevision && !pop &&
      (view == View::Players || view == View::Honours || view == View::Competition)) {
    go(view);
    return;
  }
  refresh();
}

}  // namespace tor

#endif  // MFV3_UI_TORNEO_H
