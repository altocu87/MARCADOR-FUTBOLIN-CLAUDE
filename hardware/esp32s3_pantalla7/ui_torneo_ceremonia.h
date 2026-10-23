// MARCADOR FUTBOLÍN V3 · TORNEO: la ceremonia final (TournamentCeremony de la app).
// -------------------------------------------------------------------------------------------------
// Primero ¡CAMPEÓN! con la copa (insignia dibujada) y confeti ligero (el de la victoria: 16 trocitos que no giran);
// después la clasificación final puesto a puesto («Puesto n de N»), del 2º al último, con sus números en el
// torneo y lo que se lleva cada jugador ahora: XP (cuenta sola), nivel con su barra, ELO y logros. El último se
// lleva el título del último («Farolillo rojo»…) sellado en su foto y suena la sirena.
// ◀ / SIGUIENTE ▶ / TERMINAR y SALTAR. Cada puesto es una pantalla nueva (se pinta una vez); solo se mueven el
// número de la XP y la barra de nivel.
// Se incluye en ui_torneo.h (dentro de namespace tor).

static int8_t cerStep = -1;  // -1 el campeón; después, un puesto por paso desde el 2º
static torneos::Place cerRank[torneos::MAX_TEAMS_T > 16 ? 16 : torneos::MAX_TEAMS_T];
static uint8_t cerN = 0;
static torneos::Report cerRep;
static lv_timer_t* cerSound = nullptr;

static void ceremonyLeave() {
  if (cerSound) lv_timer_delete(cerSound);
  cerSound = nullptr;
}
static void ceremonyTick() {}

static uint8_t restCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < cerN; i++)
    if (!cerRank[i].champion) n++;
  return n;
}
static const torneos::Place* restAt(int k) {
  for (uint8_t i = 0; i < cerN; i++)
    if (!cerRank[i].champion && k-- == 0) return &cerRank[i];
  return nullptr;
}
static uint8_t totalPlaces() {
  uint8_t m = 0;
  for (uint8_t i = 0; i < cerN; i++) m = cerRank[i].place > m ? cerRank[i].place : m;
  return m;
}
static const torneos::PlayerLine* lineOf(const char* id) {
  for (uint8_t i = 0; i < cerRep.nPlayers; i++)
    if (!strcmp(cerRep.players[i].id, id)) return &cerRep.players[i];
  return nullptr;
}
static void upper(char* s) {
  for (unsigned char* p = (unsigned char*)s; *p; p++) {
    if (*p >= 'a' && *p <= 'z') *p = (unsigned char)(*p - 32);
    else if (*p == 0xC3 && p[1] >= 0xA0 && p[1] <= 0xBE) {
      p[1] = (unsigned char)(p[1] - 0x20);
      p++;
    }
  }
}

static void startCeremony() {
  if (!curOk) return;
  cerStep = -1;
  go(View::Ceremony);
}
static void onCerPrev(lv_event_t*) {
  if (cerStep < 0) return;
  cerStep--;
  go(View::Ceremony);
}
static void onCerNext(lv_event_t*) {
  if (cerStep >= (int)restCount() - 1) return go(View::Detail);
  cerStep++;
  go(View::Ceremony);
}
static void onCerSkip(lv_event_t*) { go(View::Detail); }
static void onCerSound(lv_timer_t*) {
  cerSound = nullptr;
  if (hooks.sound) hooks.sound(0);
}

// La XP cuenta sola y la barra de nivel se llena (como la app).
static void xpAnim(void* o, int32_t v) {
  char b[24];
  snprintf(b, sizeof(b), "+%ld XP", (long)v);
  lv_label_set_text((lv_obj_t*)o, b);
}
static void barAnim(void* o, int32_t v) { lv_obj_set_width((lv_obj_t*)o, v); }

// Lo que se lleva un jugador: foto con su marco de rango, nombre, XP, ELO, nivel, desglose y logros.
static void playerReward(lv_obj_t* p, const char* id, int x, int y, int w, int ps, bool compact, const char* shame,
                         const torneos::PlayerLine* stats) {
  faceRank(p, id, x + 4, y + 4, ps, false);
  if (shame) {
    lv_obj_t* st = ui::box(p, x, y + ps - 22, ps + 8, 30);
    lv_obj_set_style_radius(st, 6, 0);
    lv_obj_set_style_bg_color(st, ui::hex(0x3a0812), 0);
    lv_obj_set_style_bg_opa(st, 230, 0);
    lv_obj_set_style_border_width(st, 2, 0);
    lv_obj_set_style_border_color(st, ui::hex(col::BAD), 0);
    line(st, shame, 900, strlen(shame) > 12 ? 11 : 14, 0xffb3bc, 2, 6, ps + 4, LV_TEXT_ALIGN_CENTER);
  }
  int tx = x + ps + 20, tw = w - ps - 20;
  line(p, nameOf(id), 900, 24, ui::color::TEXT, tx, y, tw);
  int yy = y + 34;
  if (stats) {
    char s[96];
    if (stats->draws)
      snprintf(s, sizeof(s), "%u PJ · %u G · %u E · %u P · %d–%d", stats->played, stats->wins, stats->draws, stats->losses, stats->gf, stats->ga);
    else snprintf(s, sizeof(s), "%u PJ · %u G · %u P · %d–%d", stats->played, stats->wins, stats->losses, stats->gf, stats->ga);
    line(p, s, 800, 13, ui::color::TEXT2, tx, yy, tw);
    yy += 22;
  }
  torneos::Reward rw;
  if (!torneos::reward(cur, id, rw)) {
    line(p, "No llegó a jugar", 400, 16, ui::color::TEXT3, tx, yy, tw);
    return;
  }
  lv_obj_t* xp = ui::text(p, "+0 XP", 900, 24, ui::color::ACCENT);
  lv_obj_set_pos(xp, tx, yy);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, xp);
  lv_anim_set_exec_cb(&a, xpAnim);
  lv_anim_set_values(&a, 0, rw.xp);
  lv_anim_set_duration(&a, 1000);
  lv_anim_set_delay(&a, 400);
  lv_anim_start(&a);
  if (rw.hasElo) {
    char e[24];
    snprintf(e, sizeof(e), "ELO %s%d", rw.eloDelta >= 0 ? "+" : "", rw.eloDelta);
    chip(p, e, tx + 150, yy + 2, rw.eloDelta >= 0 ? col::OK : col::BAD, rw.eloDelta >= 0 ? col::OK : col::BAD, 13);
  }
  yy += 34;
  char lv[16];
  snprintf(lv, sizeof(lv), "NV %u", rw.levelAfter);
  bool up = rw.levelAfter > rw.levelBefore;
  lv_obj_t* ll = ui::text(p, lv, 900, 15, up ? col::GOLD : ui::color::TEXT, 1);
  lv_obj_set_pos(ll, tx, yy);
  int bx = tx + 64;
  if (up) {
    icon(p, Icon::Up, tx + 46, yy + 2, 14, col::GOLD);
    bx += 4;
  }
  int bw = tw - (bx - tx) - 8;
  if (bw > 200) bw = 200;
  lv_obj_t* track = ui::box(p, bx, yy + 4, bw, 10);
  lv_obj_set_style_radius(track, 5, 0);
  lv_obj_set_style_bg_color(track, ui::hex(0x1d2c44), 0);
  lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
  lv_obj_t* fill = ui::box(track, 0, 0, 0, 10);
  lv_obj_set_style_radius(fill, 5, 0);
  lv_obj_set_style_bg_color(fill, ui::hex(ui::color::ACCENT), 0);
  lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
  lv_anim_t b;
  lv_anim_init(&b);
  lv_anim_set_var(&b, fill);
  lv_anim_set_exec_cb(&b, barAnim);
  lv_anim_set_values(&b, 0, bw * rw.levelFill / 100);
  lv_anim_set_duration(&b, 1200);
  lv_anim_set_delay(&b, 400);
  lv_anim_start(&b);
  yy += 26;
  if (!compact)
    for (uint8_t i = 0; i < rw.nLines && i < 2; i++) {
      char s[40];
      snprintf(s, sizeof(s), "%s +%u", rw.line[i], rw.lineXp[i]);
      line(p, s, 400, 13, ui::color::TEXT2, tx + i * (tw / 2), yy, tw / 2 - 6);
    }
  if (!compact && rw.nLines) yy += 22;
  int ax = tx;
  for (uint8_t i = 0; i < rw.nAch && i < 3; i++) {
    int cw = textW(rw.ach[i], 800, 12) + 18;
    if (ax + cw > x + w) break;
    chip(p, rw.ach[i], ax, yy, col::GOLD, col::GOLD, 12);
    ax += cw + 6;
  }
}

static void buildCeremony() {
  torneos::report(cur, cerRep);
  cerN = torneos::finalRanking(cur, cerRank, sizeof(cerRank) / sizeof(cerRank[0]));
  uint8_t rest = restCount();
  if (cerStep >= (int)rest) cerStep = (int8_t)(rest - 1);
  bool last = cerStep >= 0 && cerStep == rest - 1 && cerN > 1;
  if (cerStep < 0) ui::gradient(scr, 0x4a3608, 0x0a0806);
  else if (last) ui::gradient(scr, 0x4a0812, 0x0a0406);
  else ui::gradient(scr, 0x10284a, 0x05090f);
  // Arriba: cuál va y SALTAR.
  char cnt[32];
  if (cerStep < 0) cnt[0] = 0;  // ya lo dice «¡CAMPEÓN!»
  else snprintf(cnt, sizeof(cnt), "PUESTO %u DE %u", restAt(cerStep)->place, totalPlaces());
  lv_obj_t* c = ui::text(scr, cnt, 800, 17, cerStep < 0 ? col::GOLD : ui::color::TEXT2, 2);
  lv_obj_set_pos(c, 24, 22);
  tbtn(scr, 644, 10, 140, 48, "SALTAR", 17, Kind::Normal, onCerSkip);
  if (cerStep < 0 || !rest) {
    const torneos::Team* ch = cur.winner >= 0 ? &cur.teams[cur.winner] : nullptr;
    victoria::confetti(scr);
    line(scr, cur.name, 800, 17, ui::color::TEXT2, 0, 62, 800, LV_TEXT_ALIGN_CENTER);
    lv_obj_t* t = ui::text(scr, "¡CAMPEÓN!", 900, 72, col::GOLD, 2, true);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 82);
    lv_obj_update_layout(t);
    int half = lv_obj_get_width(t) / 2;
    cup(scr, cur.cup, 400 - half - 76, 92, 60);
    cup(scr, cur.cup, 400 + half + 16, 92, 60);
    int y = 176;
    if (ch) {
      char joined[96] = "";
      for (uint8_t k = 0; k < ch->n; k++) {
        if (k) strncat(joined, " + ", sizeof(joined) - strlen(joined) - 1);
        strncat(joined, nameOf(ch->player[k]), sizeof(joined) - strlen(joined) - 1);
      }
      // El nombre del equipo solo si no es la suma de sus nombres (equipo guardado).
      if (ch->logo[0] || strcmp(joined, ch->name)) {
        line(scr, ch->name, 900, 24, ui::color::TEXT, 0, y, 800, LV_TEXT_ALIGN_CENTER);
        y += 32;
      }
      const torneos::PlayerLine* s = lineOf(ch->player[0]);
      if (s) {
        char st[128];
        int n = s->draws ? snprintf(st, sizeof(st), "%u PJ · %u G · %u E · %u P · %d–%d goles", s->played, s->wins, s->draws, s->losses, s->gf, s->ga)
                         : snprintf(st, sizeof(st), "%u PJ · %u G · %u P · %d–%d goles", s->played, s->wins, s->losses, s->gf, s->ga);
        if (cerRep.finalScore[0] && n < (int)sizeof(st) - 16) snprintf(st + n, sizeof(st) - n, " · Final %s", cerRep.finalScore);
        line(scr, st, 800, 17, ui::color::TEXT, 0, y, 800, LV_TEXT_ALIGN_CENTER);
      }
      y += 34;
      bool pair = ch->n > 1;
      for (uint8_t k = 0; k < ch->n; k++) {
        int w = pair ? 370 : 520, x = pair ? 24 + k * 388 : 140;
        playerReward(scr, ch->player[k], x, y, w, pair ? 88 : 100, pair, nullptr, nullptr);
      }
    }
    if (cerSound) lv_timer_delete(cerSound);
    cerSound = lv_timer_create(onCerSound, 500, nullptr);
    lv_timer_set_repeat_count(cerSound, 1);
  } else {
    const torneos::Place* pl = restAt(cerStep);
    const torneos::Team& tm = cur.teams[pl->team];
    char shame[32] = "";
    if (last) {
      snprintf(shame, sizeof(shame), "%s", cur.lastTitle[0] ? cur.lastTitle : "Patético");
      upper(shame);
    }
    // El puesto en grande y su sello.
    char num[8];
    snprintf(num, sizeof(num), "%u", pl->place);
    uint32_t pc = last ? col::BAD : pl->place == 2 ? 0xd8dee8 : pl->place == 3 ? 0xc98a54 : ui::color::TEXT;
    lv_obj_t* nl = ui::text(scr, num, 900, 96, pc);
    lv_obj_set_pos(nl, 40, 76);
    lv_obj_update_layout(nl);
    lv_obj_t* o = ui::text(scr, "º", 900, 38, pc);
    lv_obj_set_pos(o, 42 + lv_obj_get_width(nl), 90);
    // El título del último va sellado en su foto: aquí no se repite.
    const char* tag = !last && pl->place == 2 ? "SUBCAMPEÓN" : nullptr;
    if (tag) chip(scr, tag, 30, 200, last ? 0xffb3bc : 0xd8dee8, last ? col::BAD : 0xd8dee8, 14);
    int x0 = 210, w0 = 566, y = 72;
    bool pair = tm.n > 1;
    if (pair || tm.logo[0]) {
      if (tm.logo[0] || tm.clubId[0]) crest(scr, tm.logo, tm.name, x0, y, 30);
      line(scr, tm.name, 900, 26, ui::color::TEXT, x0 + (tm.logo[0] || tm.clubId[0] ? 40 : 0), y, w0 - 40);
      y += 40;
    }
    // Pareja fija: los mismos números; con cambio de pareja (pool), cada uno los suyos.
    const torneos::PlayerLine* s0 = lineOf(tm.player[0]);
    bool shared = true;
    for (uint8_t k = 1; k < tm.n; k++) {
      const torneos::PlayerLine* sk = lineOf(tm.player[k]);
      if (!s0 || !sk || sk->played != s0->played || sk->wins != s0->wins || sk->gf != s0->gf || sk->ga != s0->ga) shared = false;
    }
    if (s0 && shared) {
      char st[128];
      if (s0->draws)
        snprintf(st, sizeof(st), "%u PJ · %u G · %u E · %u P · %d–%d goles", s0->played, s0->wins, s0->draws, s0->losses, s0->gf, s0->ga);
      else snprintf(st, sizeof(st), "%u PJ · %u G · %u P · %d–%d goles", s0->played, s0->wins, s0->losses, s0->gf, s0->ga);
      line(scr, st, 800, 17, ui::color::TEXT, x0, y, w0);
      y += 34;
    }
    for (uint8_t k = 0; k < tm.n; k++) {
      int py = pair ? y + k * 136 : y + 10;
      playerReward(scr, tm.player[k], x0, py, w0, pair ? 88 : 112, pair, last ? shame : nullptr, shared ? nullptr : lineOf(tm.player[k]));
    }
    if (hooks.sound) hooks.sound(last ? 2 : 1);
  }
  // Abajo: ◀, los puntos de cada puesto y SIGUIENTE / TERMINAR.
  lv_obj_t* prev = tbtn(scr, 16, 412, 84, 60, "", 20, Kind::Normal, onCerPrev);
  icon(prev, Icon::Play, 30, 18, 24, ui::color::TEXT);
  lv_obj_set_style_transform_rotation(lv_obj_get_child(prev, 1), 1800, 0);
  lv_obj_set_style_transform_pivot_x(lv_obj_get_child(prev, 1), 12, 0);
  lv_obj_set_style_transform_pivot_y(lv_obj_get_child(prev, 1), 12, 0);
  if (cerStep < 0) ui::setOpa(prev, 90);
  int dw = rest * 18 - 8, dx = 302 - dw / 2;  // entre ◀ y SIGUIENTE
  for (uint8_t i = 0; i < rest; i++) {
    lv_obj_t* d = ui::box(scr, dx + i * 18, 436, 10, 10);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(d, ui::hex((int)i == cerStep ? ui::color::ACCENT : (int)i < cerStep ? ui::color::TEXT2 : ui::color::LINE), 0);
  }
  if (cerStep < (int)rest - 1) iconBtn(scr, 504, 408, 280, 64, Icon::Play, cerStep < 0 ? "VER CLASIFICACIÓN" : "SIGUIENTE", 18, Kind::Primary, onCerNext);
  else tbtn(scr, 504, 408, 280, 64, "TERMINAR", 20, Kind::Primary, onCerNext);
}
