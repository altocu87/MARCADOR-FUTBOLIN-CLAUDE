// MARCADOR FUTBOLÍN V3 · TORNEO: creador de ligas, copas y pools (TournamentTemplateScreen de la app).
// -------------------------------------------------------------------------------------------------
// Asistente por pasos con la barra de pasos arriba: Tipo, Jugadores, Duración (solo Pool), Partidos, Puntos (solo
// Liga), Final y Nombre. Las opciones son botones grandes con su título (la elegida, iluminada) y una «?» donde no
// se entienden solas; las que dependen de otra van en un panel hijo («↳ Parejas fijas») unido a su madre por una
// pestaña. Abajo: DUPLICAR y BORRAR (si ya existe), ATRÁS y SIGUIENTE / GUARDAR.
// Cambiar de paso es otra pantalla; tocar una opción solo repinta lo que cambia (refresh()).
// Escudo, copa y banner: un selector de colores de insignia (la placa no lleva las imágenes de la app).
// Se incluye en ui_torneo.h (dentro de namespace tor).

static torneos::Template tpl;
static bool tplExisting = false;
static uint16_t tplEditions = 0;
enum Step : uint8_t { S_KIND, S_PLAYERS, S_DURATION, S_RULES, S_POINTS, S_FINAL, S_NAME };
static Step cstep = S_KIND;
static const char* const STEP_LABEL[7] = {"Tipo", "Jugadores", "Duración", "Partidos", "Puntos", "Final", "Nombre"};
static const char* const LAST_IDEAS[5] = {"Farolillo rojo", "Cuchara de madera", "Colista", "El paquete", "Patético"};

static uint8_t stepsOf(Format f, Step* out) {
  uint8_t n = 0;
  for (uint8_t s = 0; s < 7; s++) {
    if (s == S_POINTS && f != Format::League) continue;
    if (s == S_DURATION && f != Format::Pool) continue;
    out[n++] = (Step)s;
  }
  return n;
}
static int stepIndex() {
  Step st[7];
  uint8_t n = stepsOf(tpl.format, st);
  for (uint8_t i = 0; i < n; i++)
    if (st[i] == cstep) return i;
  return 0;
}
static bool hasFinal() { return tpl.format != Format::League || tpl.finalKind == torneos::FinalKind::Top2; }
static bool canDraw() { return tpl.format == Format::League && tpl.rules.end != torneos::End::Goals && tpl.allowDraw; }

// Copia sin partir una letra de dos bytes (Ñ, tildes).
static void copyUtf8(char* dst, size_t n, const char* src) {
  size_t k = 0;
  while (src[k] && k + 1 < n) k++;
  while (k && ((uint8_t)src[k] & 0xC0) == 0x80) k--;  // no cortar a media letra
  memcpy(dst, src, k);
  dst[k] = 0;
}

static void startCreator(const torneos::Template* edit, const torneos::Template* copyOf, Format f) {
  if (edit) {
    tpl = *edit;
    tplExisting = true;
  } else {
    torneos::newTemplate(tpl, f, copyOf);
    tplExisting = false;
  }
  tplEditions = 0;
  if (tplExisting)
    for (uint16_t i = 0; i < torneos::nSummaries; i++)
      if (!strcmp(torneos::summaries[i].competitionKey, tpl.id) && torneos::summaries[i].status != torneos::Status::Cancelled)
        tplEditions++;
  // Si ya se sabe el tipo (se viene de «CREAR LIGA») o se edita uno, se empieza por los jugadores.
  cstep = S_PLAYERS;
  go(View::Creator);
}

// ---------------------------------------------------------------- barra de pasos
static lv_obj_t* stepBar = nullptr;
static void onStep(lv_event_t* e) {
  cstep = (Step)argOf(e);
  go(View::Creator);
}
static void buildStepBar() {
  lv_obj_clean(stepBar);
  Step st[7];
  uint8_t n = stepsOf(tpl.format, st);
  int idx = stepIndex(), gap = 4, w = (638 - (n - 1) * gap) / n;
  for (uint8_t i = 0; i < n; i++) {
    bool on = (int)i == idx, done = (int)i < idx;
    lv_obj_t* p = ui::box(stepBar, i * (w + gap), 0, w, 44);
    lv_obj_set_style_radius(p, 999, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(p, ui::hex(on ? col::ON_BG : 0x0c1a2e), 0);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_border_color(p, ui::hex(on ? ui::color::ACCENT : done ? col::OK : ui::color::LINE), 0);
    lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(p, onStep, LV_EVENT_CLICKED, (void*)(intptr_t)st[i]);
    lv_obj_t* dot = ui::box(p, 6, 11, 22, 22);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, ui::hex(on ? col::ON_INK : done ? col::OK : ui::color::LINE), 0);
    if (done) icon(dot, Icon::Check, 4, 4, 14, col::ON_INK);
    else {
      char num[4];
      snprintf(num, sizeof(num), "%u", i + 1);
      lv_obj_t* l = ui::text(dot, num, 900, 13, on ? col::ON_BG : ui::color::TEXT);
      lv_obj_center(l);
    }
    // El primero dice el tipo («Liga») cuando no es el paso de ahora.
    const char* lab = st[i] == S_KIND && !on ? kindName(tpl.format) : STEP_LABEL[st[i]];
    line(p, lab, 700, 12, on ? col::ON_INK : ui::color::TEXT, 31, 14, w - 33);
  }
}

// ---------------------------------------------------------------- piezas: panel hijo y pestaña
// Panel de las opciones que dependen de otra: «↳ Madre» arriba.
static lv_obj_t* subPanel(int x, int y, int w, int h, const char* parent) {
  lv_obj_t* p = panel(scr, x, y, w, h, ui::color::ACCENT, 0x0b1a2c);
  lv_obj_set_style_border_width(p, 2, 0);
  icon(p, Icon::Sub, 12, 10, 20, ui::color::ACCENT);
  line(p, parent, 800, 14, ui::color::ACCENT, 38, 12, w - 50);
  return p;
}
// Pestaña que une la opción madre (su botón) con su panel hijo, a la altura del botón.
static lv_obj_t* tabTo(lv_obj_t* parentBtn, int panelX) {
  lv_obj_update_layout(parentBtn);
  lv_area_t a;
  lv_obj_get_coords(parentBtn, &a);
  int x = a.x2 - 2, h = lv_area_get_height(&a) - 20;
  lv_obj_t* t = ui::box(scr, x, a.y1 + 10, panelX - x + 3, h);
  lv_obj_set_style_bg_color(t, ui::hex(col::ON_BG), 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(t, (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM), 0);
  lv_obj_set_style_border_width(t, 2, 0);
  lv_obj_set_style_border_color(t, ui::hex(ui::color::ACCENT), 0);
  return t;
}
// «?» dentro de un botón de opción, a la derecha (la pestaña sale del borde, no la tapa).
static void optHelp(lv_obj_t* b, const HelpText* h) {
  lv_obj_update_layout(b);
  int w = lv_obj_get_width(b), hh = lv_obj_get_height(b), s = hh >= 64 ? 44 : 36;
  helpBtn(b, w - s - 8, (hh - s) / 2, s, h);
}

static void changed() { refresh(); }

// ---------------------------------------------------------------- Tipo
static lv_obj_t* tipoCard[3];
static void paintTipo() {
  for (int i = 0; i < 3; i++) {
    lv_obj_t* c = tipoCard[i];
    if (!c) continue;
    bool on = tpl.format == FORMATS[i];
    uint32_t k = kindColor(FORMATS[i]);
    lv_obj_set_style_border_width(c, on ? 3 : 2, 0);
    lv_obj_set_style_border_opa(c, on ? LV_OPA_COVER : 110, 0);
    lv_obj_set_style_shadow_width(c, on ? 24 : 0, 0);
    lv_obj_set_style_bg_color(c, ui::hex(on ? (k & 0xfefefe) >> 1 : (k & 0xfcfcfc) >> 2), 0);
    if (tplEditions && !on) lv_obj_set_style_opa(c, 110, 0);
  }
}
static void onTipo(lv_event_t* e) {
  Format f = FORMATS[argOf(e)];
  if (tplEditions && f != tpl.format) return ui::toast("Ya tiene ediciones jugadas: para otro tipo, crea una nueva");
  if (f == tpl.format) return;
  Format was = tpl.format;
  tpl.format = f;
  tpl.teamSize = f == Format::Pool ? 2 : was == Format::Pool ? 1 : tpl.teamSize;
  if (f == Format::Pool && tpl.pairing == torneos::Pairing::Elo) tpl.pairing = torneos::Pairing::Random;
  if (f != Format::League) tpl.finalKind = torneos::FinalKind::None;
  paintTipo();
  buildStepBar();  // los pasos cambian con el tipo (Duración, Puntos)
}
static void bodyKind() {
  int y = 76;
  if (tplEditions) {
    char s[128];
    snprintf(s, sizeof(s), "Ya tiene %u edici%s como %s: para otro tipo, crea una competición nueva.", tplEditions,
             tplEditions == 1 ? "ón jugada" : "ones jugadas", kindName(tpl.format));
    lv_obj_t* n = panel(scr, 16, y, 768, 40, col::GOLD, 0x1c1a10);
    line(n, s, 800, 14, 0xffe9a8, 12, 11, 744);
    y += 48;
  }
  static const Icon ICONS[3] = {Icon::Table, Icon::Trophy, Icon::Ladder};
  for (int i = 0; i < 3; i++) {
    uint32_t k = kindColor(FORMATS[i]);
    lv_obj_t* c = ui::box(scr, 16 + i * 261, y, 246, 392 - y);
    tipoCard[i] = c;
    lv_obj_set_style_radius(c, 18, 0);
    lv_obj_set_style_border_color(c, ui::hex(k), 0);
    lv_obj_set_style_shadow_color(c, ui::hex(k), 0);
    lv_obj_set_style_shadow_opa(c, 110, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(c, ui::hex(0x070b12), 0);
    lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c, onTipo, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* t = ui::text(c, kindTitle(FORMATS[i]), 900, 72, k, 2, true);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 26);
    icon(c, ICONS[i], 73, 150, 100, k);
    helpBtn(c, 246 - 56, 12, 44, &KIND_HELP[i]);
  }
  paintTipo();
}

// ---------------------------------------------------------------- Jugadores
static const HelpText H_PAIRS = {"Parejas fijas", "Las mismas parejas durante todo el torneo. Hace falta un número par de jugadores."};
static const HelpText H_ELO = {"Por ELO", "Los mejores no se cruzan hasta el final. Si faltan rivales, los mejores pasan directos a la siguiente ronda."};
static const HelpText H_BAL = {"Equilibradas", "Se junta a un jugador fuerte con uno flojo según su ELO, para que todas las parejas estén igualadas."};
static const HelpText H_SWAP = {"Con cambio de pareja", "En cada pista, las dos parejas se deshacen y cada uno juega con uno de la otra pareja, por sorteo."};
static const HelpText H_NOSWAP = {"Sin cambio de pareja", "Las mismas parejas durante todo el pool: suben y bajan juntas."};
static bool isSize(int a) { return tpl.teamSize == a; }
static void onSize(lv_event_t* e) {
  tpl.teamSize = (uint8_t)argOf(e);
  changed();
}
static bool isSeed(int a) { return (int)tpl.seeding == a; }
static void onSeed(lv_event_t* e) {
  tpl.seeding = (torneos::Seeding)argOf(e);
  changed();
}
static bool isPairing(int a) { return (int)tpl.pairing == a; }
static bool isPairingPool(int a) { return (a == (int)torneos::Pairing::Manual) == (tpl.pairing == torneos::Pairing::Manual); }
static void onPairing(lv_event_t* e) {
  tpl.pairing = (torneos::Pairing)argOf(e);
  changed();
}
static bool isSwap(int a) { return tpl.poolSwap == (a != 0); }
static void onSwap(lv_event_t* e) {
  tpl.poolSwap = argOf(e) != 0;
  changed();
}
static bool pairsOn(int) { return tpl.teamSize == 2; }
static void fillPoolPairsTitle(char* b, size_t n, int) { snprintf(b, n, "%s", tpl.poolSwap ? "PAREJAS DEL PRIMER PARTIDO" : "CÓMO SE FORMAN LAS PAREJAS"); }
static void bodyPlayers() {
  const int LX = 16, LW = 372, RX = 412, RW = 372;
  if (tpl.format == Format::Pool) {
    caption(scr, "PAREJAS", LX, 80);
    lv_obj_t* a = opt(scr, LX, 104, LW, 84, "CON CAMBIO DE PAREJA", 20, isSwap, 1, onSwap);
    optHelp(a, &H_SWAP);
    lv_obj_t* b = opt(scr, LX, 200, LW, 84, "SIN CAMBIO DE PAREJA", 20, isSwap, 0, onSwap);
    optHelp(b, &H_NOSWAP);
    lv_obj_t* l = val(scr, RX, 80, RW, 800, 13, fillPoolPairsTitle);
    lv_obj_set_style_text_letter_space(l, 1, 0);
    opt(scr, RX, 104, RW, 84, "AL AZAR", 20, isPairingPool, (int)torneos::Pairing::Random, onPairing);
    opt(scr, RX, 200, RW, 84, "LAS ELEGIMOS NOSOTROS", 20, isPairingPool, (int)torneos::Pairing::Manual, onPairing);
    para(scr, "Hace falta un número par de jugadores.", 400, 16, ui::color::TEXT2, RX, 300, RW);
    return;
  }
  caption(scr, "QUIÉN JUEGA", LX, 80);
  opt(scr, LX, 104, LW, 72, "1 VS 1", 20, isSize, 1, onSize);
  lv_obj_t* fixed = opt(scr, LX, 186, LW, 72, "PAREJAS FIJAS", 20, isSize, 2, onSize);
  optHelp(fixed, &H_PAIRS);
  if (tpl.format == Format::Bracket) {
    caption(scr, "CRUCES", LX, 274);
    lv_obj_t* e = opt(scr, LX, 298, 182, 72, "POR ELO", 20, isSeed, (int)torneos::Seeding::Elo, onSeed);
    optHelp(e, &H_ELO);
    opt(scr, LX + 190, 298, 182, 72, "SORTEO", 20, isSeed, (int)torneos::Seeding::Random, onSeed);
  }
  lv_obj_t* sp = subPanel(RX, 76, RW, 316, "Parejas fijas");
  caption(sp, "CÓMO SE FORMAN LAS PAREJAS", 14, 44);
  opt(sp, 12, 68, RW - 26, 64, "AL AZAR", 20, isPairing, (int)torneos::Pairing::Random, onPairing);
  opt(sp, 12, 140, RW - 26, 64, "LAS ELEGIMOS NOSOTROS", 20, isPairing, (int)torneos::Pairing::Manual, onPairing);
  lv_obj_t* bal = opt(sp, 12, 212, RW - 26, 64, "EQUILIBRADAS", 20, isPairing, (int)torneos::Pairing::Elo, onPairing);
  optHelp(bal, &H_BAL);
  para(sp, "Los equipos ya guardados siempre juegan juntos.", 400, 14, ui::color::TEXT2, 14, 284, RW - 28);
  visWhen(sp, pairsOn);
  visWhen(tabTo(fixed, RX), pairsOn);
}

// ---------------------------------------------------------------- Duración (Pool)
static const HelpText H_TIME = {"Por tiempo", "Cuando ya solo da tiempo a un partido, se juega la final en la pista 1."};
static const HelpText H_ROUNDS = {"Por rondas", "En cada ronda se juega un partido en cada pista. En la última ronda, la pista 1 es la final."};
static const HelpText H_MIN = {"Minutos", "El reloj arranca con el primer partido."};
static bool isLimit(int a) { return tpl.poolByRounds == (a != 0); }
static void onLimit(lv_event_t* e) {
  tpl.poolByRounds = argOf(e) != 0;
  changed();
}
static bool byTime(int) { return !tpl.poolByRounds; }
static bool byRounds(int) { return tpl.poolByRounds; }
static bool isMinutes(int a) { return tpl.poolMinutes == a; }
static void onMinutes(lv_event_t* e) {
  tpl.poolMinutes = (uint8_t)argOf(e);
  changed();
}
static bool isRounds(int a) { return tpl.poolRounds == a; }
static void onRounds(lv_event_t* e) {
  tpl.poolRounds = (uint8_t)argOf(e);
  changed();
}
static void bodyDuration() {
  const int LX = 16, LW = 372, RX = 412, RW = 372;
  caption(scr, "CUÁNDO SE ACABA", LX, 80);
  lv_obj_t* t = opt(scr, LX, 104, LW, 84, "POR TIEMPO", 20, isLimit, 0, onLimit);
  optHelp(t, &H_TIME);
  lv_obj_t* r = opt(scr, LX, 200, LW, 84, "POR RONDAS", 20, isLimit, 1, onLimit);
  optHelp(r, &H_ROUNDS);
  lv_obj_t* pt = subPanel(RX, 76, RW, 316, "Por tiempo");
  caption(pt, "MINUTOS", 14, 48);
  helpBtn(pt, 110, 38, 36, &H_MIN);
  for (int i = 0; i < 4; i++) {
    char s[12];
    snprintf(s, sizeof(s), "%u MIN", torneos::POOL_MINUTES[i]);
    opt(pt, 12 + (i % 2) * 176, 88 + (i / 2) * 92, 168, 84, s, 23, isMinutes, torneos::POOL_MINUTES[i], onMinutes);
  }
  lv_obj_t* pr = subPanel(RX, 76, RW, 316, "Por rondas");
  caption(pr, "RONDAS", 14, 48);
  for (int i = 0; i < 6; i++) {
    char s[8];
    snprintf(s, sizeof(s), "%u", torneos::POOL_ROUNDS[i]);
    opt(pr, 12 + (i % 3) * 117, 88 + (i / 3) * 92, 109, 84, s, 23, isRounds, torneos::POOL_ROUNDS[i], onRounds);
  }
  visWhen(pt, byTime);
  visWhen(pr, byRounds);
  visWhen(tabTo(t, RX), byTime);
  visWhen(tabTo(r, RX), byRounds);
}

// ---------------------------------------------------------------- Partidos (cómo se gana)
// Goles y tiempo se encienden o apagan por separado (los dos: lo que llegue antes); siempre queda uno.
static const uint8_t GOAL_OPTS[4] = {3, 5, 7, 10}, MIN_OPTS[4] = {3, 5, 8, 10};
// 0 y 1: las del tipo de torneo (partidos y final); 2 y 3: las del editor de REGLAS de un torneo empezado.
static torneos::Rules* rulesTarget(int which) {
  return which == 0 ? &tpl.rules : which == 1 ? &tpl.finalRules : which == 2 ? &edRules : &edFinal;
}
static bool goalsOn(int which) { return rulesTarget(which)->end != torneos::End::Time; }
static bool timeOn(int which) { return rulesTarget(which)->end != torneos::End::Goals; }
static bool goalsOff(int a) { return !goalsOn(a >> 8); }
static bool timeOff(int a) { return !timeOn(a >> 8); }
static void onWay(lv_event_t* e) {
  int a = argOf(e), which = a >> 8;
  torneos::Rules* r = rulesTarget(which);
  bool g = goalsOn(which), t = timeOn(which);
  if ((a & 0xFF) == 0) g = !g;
  else t = !t;
  if (!g && !t) return ui::toast("Deja activa al menos una: goles o tiempo");
  r->end = g && t ? torneos::End::Both : g ? torneos::End::Goals : torneos::End::Time;
  changed();
}
static bool wayOn(int a) { return (a & 0xFF) == 0 ? goalsOn(a >> 8) : timeOn(a >> 8); }
static bool isGoals(int a) { return rulesTarget(a >> 8)->goals == (a & 0xFF); }
static bool isMins(int a) { return rulesTarget(a >> 8)->minutes == (a & 0xFF); }
static void onGoals(lv_event_t* e) {
  int a = argOf(e);
  if (!goalsOn(a >> 8)) return;
  rulesTarget(a >> 8)->goals = (uint8_t)(a & 0xFF);
  changed();
}
static void onMins(lv_event_t* e) {
  int a = argOf(e);
  if (!timeOn(a >> 8)) return;
  rulesTarget(a >> 8)->minutes = (uint8_t)(a & 0xFF);
  changed();
}
// Los dos modos con sus cantidades, en una columna de ancho `w` (compacto: dentro de un panel, cantidades solas).
static lv_obj_t* rulesPicker(lv_obj_t* parent, int which, int x, int y, int w, bool compact, lv_obj_t** timeBtn = nullptr) {
  int h = compact ? 50 : 58, gap = compact ? 6 : 8;
  if (compact) {
    int bw = 104, sw = (w - bw - 8 - 3 * 6) / 4;
    opt(parent, x, y, bw, h, "GOLES", 17, wayOn, which << 8, onWay);
    for (int i = 0; i < 4; i++) {
      char s[8];
      snprintf(s, sizeof(s), "%u", GOAL_OPTS[i]);
      opt(parent, x + bw + 8 + i * (sw + 6), y, sw, h, s, 20, isGoals, (which << 8) | GOAL_OPTS[i], onGoals, goalsOff);
    }
    lv_obj_t* tb = opt(parent, x, y + h + gap, bw, h, "TIEMPO", 17, wayOn, (which << 8) | 1, onWay);
    for (int i = 0; i < 4; i++) {
      char s[8];
      snprintf(s, sizeof(s), "%u'", MIN_OPTS[i]);
      opt(parent, x + bw + 8 + i * (sw + 6), y + h + gap, sw, h, s, 20, isMins, (which << 8) | MIN_OPTS[i], onMins, timeOff);
    }
    if (timeBtn) *timeBtn = tb;
    return tb;
  }
  int sw = (w - 3 * 8) / 4;
  h = 52;
  opt(parent, x, y, w, h, "GOLES", 20, wayOn, which << 8, onWay);
  for (int i = 0; i < 4; i++) {
    char s[8];
    snprintf(s, sizeof(s), "A %u", GOAL_OPTS[i]);
    opt(parent, x + i * (sw + 8), y + h + gap, sw, h, s, 20, isGoals, (which << 8) | GOAL_OPTS[i], onGoals, goalsOff);
  }
  lv_obj_t* tb = opt(parent, x, y + 2 * (h + gap) + 6, w, h, "TIEMPO", 20, wayOn, (which << 8) | 1, onWay);
  for (int i = 0; i < 4; i++) {
    char s[12];
    snprintf(s, sizeof(s), "%u MIN", MIN_OPTS[i]);
    opt(parent, x + i * (sw + 8), y + 3 * (h + gap) + 6, sw, h, s, 20, isMins, (which << 8) | MIN_OPTS[i], onMins, timeOff);
  }
  if (timeBtn) *timeBtn = tb;
  return tb;
}
static const HelpText H_DRAW = {"Empate", "Sin prórroga ni penaltis: cada uno se lleva los puntos del empate."};
static const HelpText H_WINNER = {"Siempre hay ganador", "Prórroga con gol de oro y, si sigue igualado, penaltis."};
static bool isDraw(int a) { return tpl.allowDraw == (a != 0); }
static void onDraw(lv_event_t* e) {
  tpl.allowDraw = argOf(e) != 0;
  changed();
}
static bool leagueTime(int) { return tpl.format == Format::League && tpl.rules.end != torneos::End::Goals; }
static bool leagueGoals(int) { return tpl.format == Format::League && tpl.rules.end == torneos::End::Goals; }
static void fillRulesText(char* b, size_t n, int which) { rulesText(*rulesTarget(which), b, n); }
static void bodyRules() {
  const int LX = 16, LW = 372, RX = 412, RW = 372;
  caption(scr, "CÓMO SE GANA UN PARTIDO", LX, 80);
  lv_obj_t* tb = nullptr;
  rulesPicker(scr, 0, LX, 104, LW, false, &tb);
  val(scr, LX, 352, LW, 400, 14, fillRulesText, 0, ui::color::TEXT2, LV_TEXT_ALIGN_LEFT, true);
  if (tpl.format != Format::League) return;
  lv_obj_t* sp = subPanel(RX, 76, RW, 316, "Tiempo");
  caption(sp, "SI ACABAN IGUALADOS", 14, 48);
  lv_obj_t* d = opt(sp, 12, 76, RW - 26, 84, "EMPATE", 20, isDraw, 1, onDraw);
  optHelp(d, &H_DRAW);
  lv_obj_t* w = opt(sp, 12, 172, RW - 26, 84, "SIEMPRE HAY GANADOR", 20, isDraw, 0, onDraw);
  optHelp(w, &H_WINNER);
  visWhen(sp, leagueTime);
  visWhen(tabTo(tb, RX), leagueTime);
  lv_obj_t* note = para(scr, "A goles siempre hay ganador: no hay empates. Para poder empatar, activa también el tiempo.", 400, 16,
                        ui::color::TEXT2, RX + 8, 110, RW - 16);
  visWhen(note, leagueGoals);
}

// ---------------------------------------------------------------- Puntos (Liga)
static bool isLegs(int a) { return tpl.legs == a; }
static void onLegs(lv_event_t* e) {
  tpl.legs = (uint8_t)argOf(e);
  changed();
}
static uint8_t* pointsOf(int k) { return k == 0 ? &tpl.points.win : k == 1 ? &tpl.points.draw : &tpl.points.loss; }
static void fillPoints(char* b, size_t n, int k) { snprintf(b, n, "%u", *pointsOf(k)); }
static void onPoints(lv_event_t* e) {
  int a = argOf(e), k = a / 2, v = *pointsOf(k) + ((a & 1) ? 1 : -1);
  if (v < 0 || v > 9) return;
  *pointsOf(k) = (uint8_t)v;
  changed();
}
static bool drawsOn(int) { return canDraw(); }
static bool drawsOff(int) { return !canDraw(); }
static void bodyPoints() {
  const int LX = 16, LW = 372, RX = 412, RW = 372;
  caption(scr, "VUELTAS", LX, 80);
  opt(scr, LX, 104, LW, 84, "SOLO IDA", 20, isLegs, 1, onLegs);
  opt(scr, LX, 200, LW, 84, "IDA Y VUELTA", 20, isLegs, 2, onLegs);
  caption(scr, "PUNTOS POR PARTIDO", RX, 80);
  static const char* const LAB[3] = {"Victoria", "Empate", "Derrota"};
  for (int k = 0; k < 3; k++) {
    int y = 104 + k * 86;
    lv_obj_t* row = ui::box(scr, RX, y, RW, 76);
    line(row, LAB[k], 800, 20, ui::color::TEXT, 0, 24, 120);
    for (int i = 0; i < 2; i++) {
      lv_obj_t* b = fichas::btn(row, i ? RW - 76 : 124, 0, 76, 76, i ? "+" : "-", 23, Kind::Normal, onPoints, (void*)(intptr_t)(k * 2 + i));
      lv_obj_set_style_text_font(lv_obj_get_child(b, 0), assets::font(900, 34), 0);
    }
    val(row, 200, 16, RW - 76 - 200, 900, 34, fillPoints, k, ui::color::TEXT, LV_TEXT_ALIGN_CENTER);
    if (k == 1) visWhen(row, drawsOn);
  }
  lv_obj_t* note = para(scr, "Sin empates en esta liga (a goles, o con prórroga y penaltis).", 400, 14, ui::color::TEXT2, RX, 360, RW);
  visWhen(note, drawsOff);
}

// ---------------------------------------------------------------- Final
static bool isFinal(int a) { return (int)tpl.finalKind == a; }
static void onFinal(lv_event_t* e) {
  tpl.finalKind = (torneos::FinalKind)argOf(e);
  changed();
}
static bool finalOn(int) { return hasFinal(); }
static bool isBestOf(int a) { return tpl.finalBestOf == a; }
static void onBestOf(lv_event_t* e) {
  tpl.finalBestOf = (uint8_t)argOf(e);
  changed();
}
static bool isOwn(int a) { return tpl.ownFinalRules == (a != 0); }
static void onOwn(lv_event_t* e) {
  bool own = argOf(e) != 0;
  if (own && !tpl.ownFinalRules) tpl.finalRules = tpl.rules;  // se empieza con las mismas
  tpl.ownFinalRules = own;
  changed();
}
static bool ownOn(int) { return tpl.ownFinalRules; }
static bool ownOff(int) { return !tpl.ownFinalRules; }
static void fillFinalNote(char* b, size_t n, int) {
  if (tpl.format == Format::League)
    snprintf(b, n, "%s", tpl.finalKind == torneos::FinalKind::Top2 ? "Al acabar la fase regular, la final decide el campeón." : "Gana el primero de la clasificación.");
  else b[0] = 0;
}
static void fillSame(char* b, size_t n, int) {
  char r[96];
  rulesText(tpl.rules, r, sizeof(r));
  if (r[0] >= 'A' && r[0] <= 'Z') r[0] = (char)(r[0] + 32);
  snprintf(b, n, "Como el resto: %s", r);
}
static void bodyFinal() {
  const int LX = 16, LW = 372, RX = 412, RW = 372;
  lv_obj_t* parentBtn = nullptr;
  if (tpl.format == Format::League) {
    caption(scr, "¿ACABA CON FINAL?", LX, 80);
    opt(scr, LX, 104, LW, 84, "SIN FINAL", 20, isFinal, (int)torneos::FinalKind::None, onFinal);
    parentBtn = opt(scr, LX, 200, LW, 84, "1º VS 2º", 20, isFinal, (int)torneos::FinalKind::Top2, onFinal);
    val(scr, LX, 300, LW, 400, 16, fillFinalNote, 0, ui::color::TEXT2, LV_TEXT_ALIGN_LEFT, true);
  } else {
    para(scr,
         tpl.format == Format::Pool ? "El pool siempre acaba con la final en la pista 1: la juegan los que estén arriba cuando llegue la hora."
                                    : "La copa siempre acaba en final: la juegan los dos que llegan.",
         400, 22, ui::color::TEXT2, LX, 104, LW);
  }
  lv_obj_t* sp = subPanel(RX, 76, RW, 316, tpl.format == Format::League ? "Final · 1º vs 2º" : "Final");
  int w2 = (RW - 26 - 8) / 2;
  opt(sp, 12, 44, w2, 56, "PARTIDO ÚNICO", 17, isBestOf, 1, onBestOf);
  opt(sp, 12 + w2 + 8, 44, w2, 56, "AL MEJOR DE 3", 17, isBestOf, 3, onBestOf);
  opt(sp, 12, 108, w2, 56, "REGLAS IGUALES", 17, isOwn, 0, onOwn);
  opt(sp, 12 + w2 + 8, 108, w2, 56, "REGLAS DISTINTAS", 17, isOwn, 1, onOwn);
  // Justo debajo de «Reglas distintas», que es de quien dependen.
  lv_obj_t* own = ui::box(sp, 0, 172, RW - 4, 130);
  rulesPicker(own, 1, 12, 0, RW - 26, true);
  visWhen(own, ownOn);
  lv_obj_t* same = val(sp, 14, 180, RW - 28, 400, 16, fillSame, 0, ui::color::TEXT2, LV_TEXT_ALIGN_LEFT, true);
  visWhen(same, ownOff);
  if (parentBtn) {
    visWhen(sp, finalOn);
    visWhen(tabTo(parentBtn, RX), finalOn);
  }
}

// ---------------------------------------------------------------- Nombre (y escudo, copa, banner, título del último)
static const HelpText H_NAME = {"Nombre", "Así se llamará cada edición: «1ª edición», «2ª edición»…"};
static const HelpText H_ELOT = {"Cuenta para ELO", "Los partidos se juegan como Clasificatorio y suben o bajan el ELO."};
static const HelpText H_LAST = {"Título del último", "Lo que se lleva el último en la ceremonia final."};
static void fillTplName(char* b, size_t n, int) {
  if (tpl.name[0]) snprintf(b, n, "%s", tpl.name);
  else snprintf(b, n, "Ej.: %s", tpl.format == Format::League ? "Liga del Almacén" : tpl.format == Format::Bracket ? "Copa del Viernes" : "Pool de los Jueves");
}
static void fillLast(char* b, size_t n, int) { snprintf(b, n, "%s", tpl.lastTitle[0] ? tpl.lastTitle : "Patético"); }
static void nameDone(const char* t) {
  copyUtf8(tpl.name, sizeof(tpl.name), t);
  go(View::Creator);
}
static void lastDone(const char* t) {
  copyUtf8(tpl.lastTitle, sizeof(tpl.lastTitle), t);
  go(View::Creator);
}
static void onEditName(lv_event_t*) {
  char t[40];
  snprintf(t, sizeof(t), "NOMBRE %s", tpl.format == Format::Pool ? "DEL POOL" : tpl.format == Format::League ? "DE LA LIGA" : "DE LA COPA");
  static char title[40];
  snprintf(title, sizeof(title), "%s", t);
  openKeyboard(title, tpl.name, 20, nameDone, View::Creator);
}
static void onEditLast(lv_event_t*) { openKeyboard("TÍTULO DEL ÚLTIMO", tpl.lastTitle, 18, lastDone, View::Creator, LAST_IDEAS, 5); }
static bool isRanked(int) { return tpl.ranked; }
static void onRanked(lv_event_t*) {
  tpl.ranked = !tpl.ranked;
  changed();
}
static bool isIdea(int i) { return !strcmp(tpl.lastTitle, LAST_IDEAS[i]); }
static void onIdea(lv_event_t* e) {
  snprintf(tpl.lastTitle, sizeof(tpl.lastTitle), "%s", LAST_IDEAS[argOf(e)]);
  changed();
}
static void fillDescribe(char* b, size_t n, int) { torneos::describeTemplate(tpl, b, n); }
static void fillFit(char* b, size_t n, int) {
  uint8_t players = selCount();
  if (!players) {
    b[0] = 0;
    return;
  }
  torneos::Fit f = torneos::templateFit(tpl, players);
  if (f.ok) snprintf(b, n, "Con %u jugadores: %s", players, f.summary);
  else snprintf(b, n, "!× Con %u jugadores: %s", players, f.reason);
}
// Insignias que se pueden elegir: escudo (8 colores), copa (7) y banner (8 o ninguno).
static lv_obj_t* prizeBadge[3];
static void paintPrize(int which) {
  lv_obj_t* b = prizeBadge[which];
  if (!b) return;
  uint32_t c = which == 0 ? logoColor(tpl.logo, tpl.name) : which == 1 ? cupColor(tpl.cup)
                                                         : bannerIndex(tpl.banner) >= 0 ? LOGO_COLORS[bannerIndex(tpl.banner)] : ui::color::LINE;
  if (which == 1) {
    lv_obj_set_user_data(b, (void*)(uintptr_t)(((uintptr_t)(c & 0xFFFFFF) << 8) | (uint8_t)Icon::Trophy));
    lv_obj_invalidate(b);
    return;
  }
  lv_obj_set_style_bg_color(b, ui::hex(c), 0);
  lv_obj_set_style_border_color(b, ui::hex(c), 0);
  lv_obj_t* l = lv_obj_get_child(b, 0);
  if (l) lv_obj_set_style_text_color(l, ui::hex(c), 0);
}
static int picking = 0;
static void onSwatch(lv_event_t* e) {
  int i = argOf(e);
  if (picking == 0) snprintf(tpl.logo, sizeof(tpl.logo), "emoji:%s", EMOJI_LOGOS[i]);
  else if (picking == 1) snprintf(tpl.cup, sizeof(tpl.cup), "trofeo:%s", CUP_IDS[i]);
  else if (i < 0 || i > 7) tpl.banner[0] = 0;
  else snprintf(tpl.banner, sizeof(tpl.banner), "color:%d", i);
  onPopClose(nullptr);
  paintPrize(picking);
}
static void onPrize(lv_event_t* e) {
  picking = argOf(e);
  int n = picking == 1 ? 7 : 8;
  lv_obj_t* card = popCard(640, picking == 2 ? 330 : 260, ui::color::ACCENT);
  lv_obj_add_event_cb(pop, onPopClose, LV_EVENT_CLICKED, nullptr);
  line(card, picking == 0 ? "ELIGE EL ESCUDO" : picking == 1 ? "ELIGE LA COPA" : "ELIGE EL BANNER", 900, 24, ui::color::TEXT, 24, 18, 560);
  for (int i = 0; i < n; i++) {
    int x = 24 + (i % 4) * 150, y = 64 + (i / 4) * 92;
    lv_obj_t* b = fichas::btn(card, x, y, 140, 84, "", 14, Kind::Normal, onSwatch, (void*)(intptr_t)i);
    uint32_t c = picking == 1 ? CUP_COLORS[i] : LOGO_COLORS[i];
    if (picking == 1) {
      icon(b, Icon::Trophy, 40, 10, 60, c);
      lv_obj_t* l = ui::text(b, CUP_IDS[i], 800, 13, c);
      lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -4);
    } else badge(b, c, tpl.name[0] ? tpl.name : kindName(tpl.format), 40, 12, 60);
  }
  if (picking == 2) tbtn(card, 24, 250, 592, 60, "SIN BANNER", 17, Kind::Normal, onSwatch, (void*)(intptr_t)8);
}
static lv_obj_t* field(lv_obj_t* parent, int x, int y, int w, int h, void (*fill)(char*, size_t, int), lv_event_cb_t cb) {
  lv_obj_t* f = fichas::btn(parent, x, y, w, h, "", 14, Kind::Normal, cb);
  lv_obj_set_style_bg_color(f, ui::hex(ui::color::SURFACE), 0);
  lv_obj_set_style_border_color(f, ui::hex(ui::color::ACCENT), 0);
  lv_obj_set_style_border_width(f, 2, 0);
  val(f, 14, (h - 30) / 2, w - 28, 900, 24, fill, 0, ui::color::TEXT);
  return f;
}
static void bodyName() {
  const int LX = 16, LW = 372, RX = 412, RW = 372;
  char t[40];
  snprintf(t, sizeof(t), "NOMBRE %s", tpl.format == Format::Pool ? "DEL POOL" : tpl.format == Format::League ? "DE LA LIGA" : "DE LA COPA");
  caption(scr, t, LX, 80);
  helpBtn(scr, LX + textW(t, 800, 13, 1) + 34, 70, 36, &H_NAME);
  field(scr, LX, 108, LW, 60, fillTplName, onEditName);
  lv_obj_t* r = opt(scr, LX, 180, LW, 64, "CUENTA PARA ELO", 20, isRanked, 0, onRanked);
  optHelp(r, &H_ELOT);
  lv_obj_t* sum = panel(scr, LX, 256, LW, 136);
  caption(sum, "RESUMEN", 14, 10);
  val(sum, 14, 32, LW - 28, 400, 14, fillDescribe, 0, ui::color::TEXT, LV_TEXT_ALIGN_LEFT, true);
  val(sum, 14, 92, LW - 28, 800, 13, fillFit, 0, col::OK, LV_TEXT_ALIGN_LEFT, true);
  // Escudo, copa y banner.
  static const char* const PRIZE[3] = {"ESCUDO", "COPA", "BANNER"};
  memset(prizeBadge, 0, sizeof(prizeBadge));
  for (int i = 0; i < 3; i++) {
    lv_obj_t* b = fichas::btn(scr, RX + i * 128, 76, 116, 104, "", 14, Kind::Normal, onPrize, (void*)(intptr_t)i);
    if (i == 1) prizeBadge[i] = icon(b, Icon::Trophy, 30, 8, 56, cupColor(tpl.cup));
    else prizeBadge[i] = badge(b, ui::color::LINE, tpl.name[0] ? tpl.name : kindName(tpl.format), 30, 8, 56);
    lv_obj_t* l = ui::text(b, PRIZE[i], 800, 13, ui::color::TEXT2, 1);
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -8);
    paintPrize(i);
  }
  caption(scr, "TÍTULO DEL ÚLTIMO", RX, 194);
  helpBtn(scr, RX + textW("TÍTULO DEL ÚLTIMO", 800, 13, 1) + 34, 184, 36, &H_LAST);
  field(scr, RX, 222, RW, 52, fillLast, onEditLast);
  int x = RX, y = 284;
  for (int i = 0; i < 5; i++) {
    int w = textW(LAST_IDEAS[i], 800, 14) + 26;
    if (x + w > RX + RW) {
      x = RX;
      y += 54;
    }
    opt(scr, x, y, w, 48, LAST_IDEAS[i], 14, isIdea, i, onIdea);
    x += w + 8;
  }
}

// ---------------------------------------------------------------- pie y pantalla
static void onCreatorBack(lv_event_t*) {
  kind = tpl.format;
  go(View::Choose);
}
static void onCreatorPrev(lv_event_t*) {
  Step st[7];
  stepsOf(tpl.format, st);
  int i = stepIndex();
  if (i > 0) cstep = st[i - 1];
  go(View::Creator);
}
static void saveTpl() {
  // Sin espacios de más.
  size_t n = strlen(tpl.name);
  while (n && tpl.name[n - 1] == ' ') tpl.name[--n] = 0;
  if (!tpl.name[0]) {
    ui::toast("Escribe el nombre");
    cstep = S_NAME;
    return go(View::Creator);
  }
  if (!torneos::saveTemplate(tpl)) return ui::toast("No se pudo guardar");
  char b[64];
  if (tplExisting) snprintf(b, sizeof(b), "Cambios guardados");
  else snprintf(b, sizeof(b), "«%s» guardado", tpl.name);
  ui::toast(b);
  kind = tpl.format;
  snprintf(chosenTpl, sizeof(chosenTpl), "%s", tpl.id);
  go(View::Choose);
}
static void onCreatorNext(lv_event_t*) {
  Step st[7];
  uint8_t n = stepsOf(tpl.format, st);
  int i = stepIndex();
  if (i >= n - 1) return saveTpl();
  cstep = st[i + 1];
  go(View::Creator);
}
static void onDuplicate(lv_event_t*) {
  torneos::Template base = tpl;
  startCreator(nullptr, &base, base.format);
}
static void deleteTpl() {
  Format f = tpl.format;
  if (torneos::removeTemplate(tpl.id)) ui::toast("Borrado");
  kind = f;
  bool any = false;
  for (uint8_t t = 0; t < torneos::nTemplates; t++)
    if (torneos::templates[t].format == f) any = true;
  go(any ? View::Choose : View::Empty);
}
static void onDeleteTpl(lv_event_t*) {
  char t[64];
  snprintf(t, sizeof(t), "¿Borrar «%s»?", tpl.name);
  ask(t, "Los torneos ya jugados con él no cambian.", "NO", "BORRAR", deleteTpl);
}

static void buildCreator() {
  lv_obj_t* band = ui::box(scr, 0, 0, 800, 64);
  lv_obj_set_style_bg_color(band, ui::hex(col::BAND), 0);
  lv_obj_set_style_bg_opa(band, 235, 0);
  lv_obj_set_style_border_side(band, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(band, 1, 0);
  lv_obj_set_style_border_color(band, ui::hex(ui::color::LINE), 0);
  backBtn(scr, 12, 8, onCreatorBack);
  ui::text(scr, tplExisting ? "EDITAR" : "CREAR", 900, 20, ui::color::TEXT, 0.5f);
  lv_obj_set_pos(lv_obj_get_child(scr, lv_obj_get_child_count(scr) - 1), 70, 20);
  stepBar = ui::box(scr, 150, 10, 638, 44);
  buildStepBar();
  memset(tipoCard, 0, sizeof(tipoCard));
  switch (cstep) {
    case S_KIND: bodyKind(); break;
    case S_PLAYERS: bodyPlayers(); break;
    case S_DURATION: bodyDuration(); break;
    case S_RULES: bodyRules(); break;
    case S_POINTS: bodyPoints(); break;
    case S_FINAL: bodyFinal(); break;
    case S_NAME: bodyName(); break;
  }
  footer();
  int x = 16;
  if (tplExisting) {
    char d[32];
    snprintf(d, sizeof(d), "DUPLICAR %s", kindTitle(tpl.format));
    tbtn(scr, x, 412, 178, 60, d, 14, Kind::Normal, onDuplicate);
    x += 186;
    tbtn(scr, x, 412, 120, 60, "BORRAR", 14, Kind::Danger, onDeleteTpl);
  }
  Step st[7];
  uint8_t n = stepsOf(tpl.format, st);
  int i = stepIndex();
  if (i > 0) tbtn(scr, 410, 408, 140, 64, "ATRÁS", 20, Kind::Normal, onCreatorPrev);
  if (i >= n - 1) {
    char g[32];
    snprintf(g, sizeof(g), "GUARDAR %s", kindTitle(tpl.format));
    iconBtn(scr, 558, 408, 226, 64, Icon::Check, g, 18, Kind::Primary, onCreatorNext);
  } else {
    iconBtn(scr, 558, 408, 226, 64, Icon::Play, "SIGUIENTE", 20, Kind::Primary, onCreatorNext);
  }
}
