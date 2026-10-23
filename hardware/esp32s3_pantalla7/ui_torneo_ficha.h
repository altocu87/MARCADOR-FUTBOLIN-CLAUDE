// MARCADOR FUTBOLÍN V3 · TORNEO: FICHA de un torneo, PALMARÉS y palmarés de una competición.
// -------------------------------------------------------------------------------------------------
// Como TournamentReportView, TournamentListScreen y TournamentHonoursScreen de la app:
//   - FICHA (pestaña del torneo): el campeón y el MVP arriba; Fecha, Partidos, Goles, Jugado (y «Cuenta para ELO»)
//     en cuadros; los destacados (pichichi, mayor goleada, partido con más goles) y la tabla de jugadores con la «?»
//     del MVP. La lista de jugadores se desliza.
//   - PALMARÉS: una tarjeta por competición con campeón (último campeón y quién suma más títulos).
//   - Una competición: sus ediciones (con campeón y MVP; tocar una abre el torneo) y el cuadro de honor.
// Los números son del contrato (torneos::report y torneos::competitions); aquí solo se enseñan.
// Se incluye en ui_torneo.h (dentro de namespace tor).

// ---------------------------------------------------------------- FICHA
static torneos::Report rep;
static const HelpText H_MVP = {"Puntos de MVP",
                               "3 por victoria, 1 por empate, la mitad de la diferencia de goles de su equipo y 2 al campeón. Los goles "
                               "de cada jugador no cuentan: no siempre se apunta quién marcó."};

static void tile(lv_obj_t* p, int x, int y, int w, const char* value, const char* label, uint32_t c = ui::color::TEXT) {
  lv_obj_t* t = panel(p, x, y, w, 58);
  line(t, value, 900, 20, c, 12, 7, w - 20);
  line(t, label, 800, 12, ui::color::TEXT2, 12, 34, w - 20);
}
// Un partido del historial en pequeño: fotos del Blanco, marcador y fotos del Azul.
static void miniMatch(lv_obj_t* p, const partidos::Row& r, int x, int y) {
  for (int t = 0; t < 2; t++)
    for (int i = 0; i < r.n[t]; i++) {
      char id[40];
      store->idOf(r.code[t][i], id, sizeof(id));
      face(p, id, t ? x + 130 + i * 30 : x + i * 30, y, 28);
    }
  char s[12];
  snprintf(s, sizeof(s), "%u–%u", r.w, r.b);
  line(p, s, 900, 20, ui::color::TEXT, x + 62, y + 2, 64, LV_TEXT_ALIGN_CENTER);
}

static void buildReport(lv_obj_t* parent) {
  torneos::report(cur, rep);
  const torneos::Team* champ = cur.winner >= 0 ? &cur.teams[cur.winner] : nullptr;
  // Campeón
  lv_obj_t* hc = panel(parent, 16, 8, 500, 92, champ ? col::GOLD : ui::color::LINE, champ ? 0x241f0c : col::PANEL);
  cup(hc, cur.cup, 12, 16, 58);
  caption(hc, champ ? "CAMPEÓN" : cur.status == torneos::Status::Cancelled ? "CANCELADO" : "EN JUEGO", 82, 12, champ ? col::GOLD : ui::color::TEXT2);
  line(hc, champ ? champ->name : "Por decidir", 900, 24, champ ? ui::color::TEXT : ui::color::TEXT2, 82, 32, 300);
  if (rep.finalScore[0]) {
    char f[48];
    snprintf(f, sizeof(f), "Final: %s", rep.finalScore);
    line(hc, f, 400, 13, ui::color::TEXT2, 82, 64, 300);
  }
  if (champ)
    for (uint8_t k = 0; k < champ->n; k++) face(hc, champ->player[k], 500 - 12 - (champ->n - k) * 56 + 4, 20, 50);
  // MVP
  if (rep.mvp >= 0) {
    const torneos::PlayerLine& m = rep.players[rep.mvp];
    lv_obj_t* hm = panel(parent, 524, 8, 260, 92, col::GOLD, 0x1c1a10);
    face(hm, m.id, 12, 19, 54);
    caption(hm, cur.status == torneos::Status::Finished ? "MVP" : "MVP PROVISIONAL", 78, 20, col::GOLD);
    line(hm, nameOf(m.id), 900, 20, ui::color::TEXT, 78, 42, 170);
  }
  // Cuadros: cuándo, cuántos partidos, cuántos goles y cuánto se jugó.
  char date[40], from[24], to[24];
  uint64_t start = rep.startedAt ? rep.startedAt : cur.createdAt;
  shortDate(start, from, sizeof(from));
  if (cur.status == torneos::Status::Finished && rep.finishedAt) {
    shortDate(rep.finishedAt, to, sizeof(to));
    reloj::Clock a = partidos::localOf((uint32_t)(start / 1000)), b = partidos::localOf((uint32_t)(rep.finishedAt / 1000));
    if (a.day == b.day && a.month == b.month && a.year == b.year) snprintf(date, sizeof(date), "%s", from);
    else snprintf(date, sizeof(date), "%u – %s", a.day, to);
  } else snprintf(date, sizeof(date), "%s", from);
  uint16_t total = 0;
  for (uint16_t i = 0; i < cur.nFix; i++)
    if (!cur.fix[i].bye && !cur.fix[i].skipped) total++;
  char pj[24], gl[12], tm[16];
  if (active_() && !cur.hasLadder) snprintf(pj, sizeof(pj), "%u de %u", rep.matches, total);
  else snprintf(pj, sizeof(pj), "%u", rep.matches);
  snprintf(gl, sizeof(gl), "%u", rep.totalGoals);
  if (rep.matches) snprintf(tm, sizeof(tm), "%lu min", (unsigned long)(rep.playTimeMs < 60000 ? 1 : (rep.playTimeMs + 30000) / 60000));
  else snprintf(tm, sizeof(tm), "—");
  int nt = cur.ranked ? 5 : 4, tw = (768 - (nt - 1) * 8) / nt;
  tile(parent, 16, 108, tw, date, active_() ? "EMPEZÓ" : "FECHA");
  tile(parent, 16 + (tw + 8), 108, tw, pj, "PARTIDOS");
  tile(parent, 16 + 2 * (tw + 8), 108, tw, gl, "GOLES");
  tile(parent, 16 + 3 * (tw + 8), 108, tw, tm, "JUGADO");
  if (cur.ranked) tile(parent, 16 + 4 * (tw + 8), 108, tw, "ELO", "CUENTA PARA ELO", col::GOLD);
  // Destacados
  int tx = 16, twid = 768;
  const partidos::Row* big = rowOf(rep.biggestWin);
  const partidos::Row* most = rowOf(rep.highestScoring);
  if (most && big && most == big) most = nullptr;
  bool anyHl = rep.topScorer >= 0 || big || most;
  if (anyHl) {
    lv_obj_t* d = panel(parent, 16, 174, 300, 234);
    caption(d, "DESTACADOS", 12, 10);
    int y = 34;
    if (rep.topScorer >= 0) {
      const torneos::PlayerLine& p = rep.players[rep.topScorer];
      line(d, "PICHICHI", 800, 12, ui::color::TEXT3, 12, y, 120);
      face(d, p.id, 12, y + 18, 32);
      char g[48];
      snprintf(g, sizeof(g), "%s · %d goles", nameOf(p.id), p.scored);
      line(d, g, 800, 14, ui::color::TEXT, 50, y + 26, 238);
      y += 62;
    }
    if (big) {
      line(d, "MAYOR GOLEADA", 800, 12, ui::color::TEXT3, 12, y, 200);
      miniMatch(d, *big, 12, y + 18);
      y += 62;
    }
    if (most && y < 200) {
      line(d, "PARTIDO CON MÁS GOLES", 800, 12, ui::color::TEXT3, 12, y, 220);
      miniMatch(d, *most, 12, y + 18);
    }
    tx = 324;
    twid = 460;
  }
  // Jugadores
  lv_obj_t* tp = panel(parent, tx, 174, twid, 234);
  bool anyDraws = false, anyScored = false;
  for (uint8_t i = 0; i < rep.nPlayers; i++) {
    if (rep.players[i].draws) anyDraws = true;
    if (rep.scorersReliable && rep.players[i].scored > 0) anyScored = true;
  }
  const char* head[6] = {"PJ", "G", "E", "P", "GOLES", "MVP"};
  bool on[6] = {true, true, anyDraws, true, anyScored, true};
  int colX[6], x = twid - 10;
  for (int c = 5; c >= 0; c--) {
    if (!on[c]) {
      colX[c] = -1;
      continue;
    }
    x -= c >= 4 ? 52 : 34;
    colX[c] = x;
  }
  line(tp, "JUGADOR", 800, 12, ui::color::TEXT3, 12, 10, 120);
  for (int c = 0; c < 6; c++)
    if (colX[c] >= 0) line(tp, head[c], 800, 12, ui::color::TEXT3, colX[c], 10, c >= 4 ? (c == 5 ? 36 : 52) : 34, LV_TEXT_ALIGN_CENTER);
  helpBtn(tp, twid - 30, 4, 26, &H_MVP);
  lv_obj_t* list = scroller(tp, 4, 30, twid - 8, 200);
  for (uint8_t i = 0; i < rep.nPlayers; i++) {
    const torneos::PlayerLine& p = rep.players[i];
    bool lead = i == 0 && rep.mvp >= 0;
    lv_obj_t* r = ui::box(list, 0, i * 36, twid - 14, 34);
    lv_obj_set_style_radius(r, 8, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r, ui::hex(lead ? 0x2a2410 : i % 2 ? 0x0c1828 : 0x0e1b2e), 0);
    face(r, p.id, 6, 3, 28);
    int nameW = colX[0] - 4 - 40 - (p.champion ? 22 : 0);
    line(r, nameOf(p.id), 800, 14, lead ? col::GOLD : ui::color::TEXT, 40, 8, nameW);
    if (p.champion) icon(r, Icon::Trophy, 40 + (textW(nameOf(p.id), 800, 14) < nameW ? textW(nameOf(p.id), 800, 14) : nameW) + 4, 8, 16, col::GOLD);
    int16_t v[5] = {p.played, p.wins, p.draws, p.losses, p.scored};
    for (int c = 0; c < 6; c++) {
      if (colX[c] < 0) continue;
      char t[12];
      if (c == 5) snprintf(t, sizeof(t), "%g", (double)p.mvp);
      else snprintf(t, sizeof(t), "%d", v[c]);
      line(r, t, c == 5 ? 900 : 800, 14, c == 5 ? ui::color::TEXT : ui::color::TEXT2, colX[c] - 4, 8, c >= 4 ? (c == 5 ? 36 : 52) : 34,
           LV_TEXT_ALIGN_CENTER);
    }
  }
}

// ---------------------------------------------------------------- PALMARÉS
static torneos::Competition* comps = nullptr;  // en la PSRAM (cada una es grande)
static uint8_t nComps = 0;
static const uint8_t MAX_COMPS = 24;
static char compKey[56] = "";
static void loadComps() {
  if (!comps) comps = (torneos::Competition*)calloc(MAX_COMPS, sizeof(torneos::Competition));
  nComps = comps ? torneos::competitions(comps, MAX_COMPS) : 0;
}
static void onHonoursBack(lv_event_t*) { go(View::Players); }
static void onComp(lv_event_t* e) {
  snprintf(compKey, sizeof(compKey), "%s", comps[argOf(e)].key);
  go(View::Competition);
}
static void championFaces(lv_obj_t* p, const torneos::Summary& s, int x, int y, int size) {
  for (uint8_t k = 0; k < s.nChampion && k < 2; k++) face(p, s.championIds[k], x + k * (size + 4), y, size);
}
static void buildHonours() {
  header("PALMARÉS", nullptr, onHonoursBack);
  torneos::loadIndex();
  loadComps();
  uint8_t shown = 0;
  lv_obj_t* list = scroller(scr, 16, 72, 772, 408);
  lv_obj_set_style_pad_bottom(list, 8, 0);
  for (uint8_t i = 0; i < nComps; i++) {
    const torneos::Competition& c = comps[i];
    if (c.lastChampion < 0) continue;  // solo las que ya tienen algún campeón
    int x = (shown % 2) * 388, y = (shown / 2) * 160;
    shown++;
    lv_obj_t* card = fichas::btn(list, x, y, 380, 152, "", 14, Kind::Normal, onComp, (void*)(intptr_t)i);
    lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);
    if (bannerIndex(c.banner) >= 0) {
      lv_obj_t* bn = ui::box(card, 0, 0, 380, 6);
      lv_obj_set_style_bg_color(bn, ui::hex(LOGO_COLORS[bannerIndex(c.banner)]), 0);
      lv_obj_set_style_bg_opa(bn, LV_OPA_COVER, 0);
    }
    crest(card, c.logo, c.name, 12, 14, 52);
    line(card, c.name, 900, 20, ui::color::TEXT, 74, 14, 230);
    uint8_t live = 0;
    for (uint8_t k = 0; k < c.nEditions; k++)
      if (torneos::summaries[c.editions[k]].status == torneos::Status::Active) live++;
    uint16_t done = torneos::editionCount(c.key) - live;  // todas, también las que solo están en la SD
    char sub[64];
    snprintf(sub, sizeof(sub), "%s · %u edici%s%s", kindName(c.format), done, done == 1 ? "ón jugada" : "ones jugadas", live ? " · 1 en juego" : "");
    line(card, sub, 400, 13, ui::color::TEXT2, 74, 42, 230);
    cup(card, c.cup, 316, 10, 52);
    const torneos::Summary& last = torneos::summaries[c.lastChampion];
    char lc[48], d[24];
    shortDate(last.finishedAt, d, sizeof(d));
    snprintf(lc, sizeof(lc), "ÚLTIMO CAMPEÓN · %s", d);
    line(card, lc, 800, 12, ui::color::TEXT3, 12, 76, 220);
    championFaces(card, last, 12, 96, 36);
    line(card, last.champion, 800, 17, ui::color::TEXT, 14 + last.nChampion * 40, 104, 200 - last.nChampion * 40);
    if (c.nHonours && c.honours[0].titles > 1) {
      line(card, "MÁS TÍTULOS", 800, 12, ui::color::TEXT3, 236, 76, 130);
      face(card, c.honours[0].id, 236, 98, 30);
      char tt[32];
      snprintf(tt, sizeof(tt), "%s · %u", nameOf(c.honours[0].id), c.honours[0].titles);
      line(card, tt, 800, 14, col::GOLD, 270, 104, 100);
    }
  }
  if (!shown) {
    icon(scr, Icon::Trophy, 355, 120, 90, col::GOLD);
    line(scr, "Palmarés vacío", 900, 26, ui::color::TEXT, 0, 228, 800, LV_TEXT_ALIGN_CENTER);
    line(scr, "Cuando termine el primer torneo, aquí saldrá su campeón.", 400, 16, ui::color::TEXT2, 0, 270, 800, LV_TEXT_ALIGN_CENTER);
  }
}

// ---------------------------------------------------------------- palmarés de una competición
static void onCompBack(lv_event_t*) { go(View::Honours); }
// Ediciones por páginas: en memoria solo están las 10 últimas; al llegar al final de la lista (o con VER MÁS) se
// leen las 10 siguientes de la SD y se añaden debajo, sin rehacer la pantalla.
static const uint16_t ED_PAGE = 10, ED_ROW = 82;
static lv_obj_t *edList = nullptr, *edMore = nullptr;
static uint16_t edLoaded = 0, edTotal = 0, edCap = 0;
static char (*edIds)[48] = nullptr;  // id del torneo de cada fila (crece con las páginas)
static torneos::Summary* edPage = nullptr;
static void onEdition(lv_event_t* e) {
  fromList = true;
  openTournament(edIds[argOf(e)]);
}
static void editionRow(const torneos::Summary& s, uint16_t k) {
  lv_obj_t* r = fichas::btn(edList, 0, k * ED_ROW, 386, ED_ROW - 6, "", 14, Kind::Normal, onEdition, (void*)(intptr_t)k);
  lv_obj_add_flag(r, LV_OBJ_FLAG_EVENT_BUBBLE);
  char num[8];
  snprintf(num, sizeof(num), "%u", s.edition);
  lv_obj_t* nl = ui::text(r, num, 900, 30, col::GOLD);
  lv_obj_set_pos(nl, 10, 16);
  lv_obj_update_layout(nl);
  lv_obj_t* sup = ui::text(r, "ª", 900, 15, col::GOLD);
  lv_obj_set_pos(sup, 10 + lv_obj_get_width(nl), 18);
  char d[48], dd[24];
  if (s.status == torneos::Status::Finished) shortDate(s.finishedAt, d, sizeof(d));
  else {
    shortDate(s.createdAt, dd, sizeof(dd));
    snprintf(d, sizeof(d), "En juego desde %s", dd);
  }
  line(r, d, 400, 13, ui::color::TEXT2, 64, 8, 230);
  if (s.nChampion) {
    icon(r, Icon::Trophy, 64, 28, 18, col::GOLD);
    line(r, s.champion, 800, 17, ui::color::TEXT, 88, 28, 200);
  } else line(r, "Sin campeón todavía", 800, 17, ui::color::TEXT3, 64, 28, 220);
  if (s.mvpId[0]) {
    line(r, "MVP:", 800, 12, ui::color::TEXT3, 64, 54, 40);
    face(r, s.mvpId, 100, 50, 20);
    line(r, nameOf(s.mvpId), 800, 12, ui::color::TEXT2, 124, 54, 160);
  }
  for (uint8_t q = 0; q < s.nChampion && q < 2; q++) face(r, s.championIds[q], 386 - 10 - (s.nChampion - q) * 40 + 4, 20, 36);
}
static void moreEditions() {
  if (!edList || edLoaded >= edTotal) return;
  if (!edPage) edPage = (torneos::Summary*)calloc(ED_PAGE, sizeof(torneos::Summary));
  if (edCap < edLoaded + ED_PAGE) {
    void* p = realloc(edIds, (size_t)(edLoaded + ED_PAGE) * 48);
    if (!p || !edPage) return ui::toast("No hay memoria para más ediciones");
    edIds = (char(*)[48])p;
    edCap = edLoaded + ED_PAGE;
  }
  uint16_t n = torneos::editionsPage(compKey, edLoaded, edPage, ED_PAGE);
  for (uint16_t i = 0; i < n; i++) {
    snprintf(edIds[edLoaded], 48, "%s", edPage[i].id);
    editionRow(edPage[i], edLoaded++);
  }
  if (!n) edTotal = edLoaded;  // ya no hay más
  if (edMore) {
    if (edLoaded < edTotal) lv_obj_set_y(edMore, edLoaded * ED_ROW);
    else ui::setVisible(edMore, false);
  }
}
static void onMoreEditions(lv_event_t*) { moreEditions(); }
static void onEditionsScroll(lv_event_t*) {
  if (edList && lv_obj_get_scroll_bottom(edList) < 40) moreEditions();
}
static void buildCompetition() {
  if (!comps || !nComps) loadComps();
  const torneos::Competition* c = nullptr;
  for (uint8_t i = 0; i < nComps; i++)
    if (!strcmp(comps[i].key, compKey)) c = &comps[i];
  if (!c) {
    header("PALMARÉS", nullptr, onCompBack);
    line(scr, "Competición no encontrada.", 800, 20, ui::color::TEXT2, 0, 200, 800, LV_TEXT_ALIGN_CENTER);
    return;
  }
  lv_obj_t* band = ui::box(scr, 0, 0, 800, 64);
  lv_obj_set_style_bg_color(band, ui::hex(col::BAND), 0);
  lv_obj_set_style_bg_opa(band, 235, 0);
  backBtn(scr, 12, 8, onCompBack);
  crest(scr, c->logo, c->name, 70, 12, 40);
  int tw = textW(c->name, 900, 24, 0);
  if (tw > 480) tw = 480;
  line(scr, c->name, 900, 24, col::GOLD, 120, 9, tw + 4);
  cup(scr, c->cup, 128 + tw, 10, 40);
  char sub[64];
  uint16_t ne = torneos::editionCount(compKey);
  snprintf(sub, sizeof(sub), "PALMARÉS · %s · %u EDICI%s", kindTitle(c->format), ne, ne == 1 ? "ÓN" : "ONES");
  line(scr, sub, 800, 13, ui::color::TEXT2, 120, 40, 600);
  // Ediciones
  lv_obj_t* l = panel(scr, 16, 72, 404, 400);
  caption(l, "EDICIONES", 12, 12);
  edList = scroller(l, 6, 36, 392, 358);
  edLoaded = 0;
  edTotal = torneos::editionCount(compKey);
  char more[32];
  snprintf(more, sizeof(more), "VER MÁS EDICIONES");
  edMore = tbtn(edList, 0, 0, 386, 64, more, 17, Kind::Normal, onMoreEditions);
  lv_obj_add_flag(edMore, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_event_cb(edList, onEditionsScroll, LV_EVENT_SCROLL_END, nullptr);
  moreEditions();
  // Cuadro de honor
  lv_obj_t* h = panel(scr, 428, 72, 356, 400);
  caption(h, "CUADRO DE HONOR", 12, 12);
  const char* head[3] = {"TÍT.", "MVP", "ED."};
  for (int i = 0; i < 3; i++) line(h, head[i], 800, 12, ui::color::TEXT3, 210 + i * 46, 38, 44, LV_TEXT_ALIGN_CENTER);
  lv_obj_t* hl = scroller(h, 6, 58, 344, 336);
  for (uint8_t i = 0; i < c->nHonours; i++) {
    const torneos::HonourRow& hr = c->honours[i];
    bool lead = i == 0 && hr.titles > 0;
    lv_obj_t* r = ui::box(hl, 0, i * 38, 338, 36);
    lv_obj_set_style_radius(r, 8, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r, ui::hex(lead ? 0x2a2410 : i % 2 ? 0x0c1828 : 0x0e1b2e), 0);
    char n[6];
    snprintf(n, sizeof(n), "%u", i + 1);
    line(r, n, 900, 14, lead ? col::GOLD : ui::color::TEXT2, 4, 9, 20, LV_TEXT_ALIGN_CENTER);
    face(r, hr.id, 28, 4, 28);
    line(r, nameOf(hr.id), 800, 14, lead ? col::GOLD : ui::color::TEXT, 62, 9, 136);
    uint8_t v[3] = {hr.titles, hr.mvps, hr.editions};
    for (int k = 0; k < 3; k++) {
      char t[6];
      snprintf(t, sizeof(t), "%u", v[k]);
      line(r, t, k ? 800 : 900, 14, k ? ui::color::TEXT2 : ui::color::TEXT, 204 + k * 46, 9, 44, LV_TEXT_ALIGN_CENTER);
    }
  }
}
