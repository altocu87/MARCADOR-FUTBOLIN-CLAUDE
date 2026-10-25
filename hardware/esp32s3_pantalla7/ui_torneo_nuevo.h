// MARCADOR FUTBOLÍN V3 · TORNEO: «¿Quién juega?», «¿Qué torneo jugamos?», «Elige la liga» y las parejas.
// -------------------------------------------------------------------------------------------------
// Como TournamentNewScreen de la app:
//   1. ¿QUIÉN JUEGA?: arriba los torneos EN JUEGO (▶ CONTINUAR y papelera con confirmación); la rejilla de jugadores
//      con su foto (la misma ficha ya pintada de «elegir jugadores», ui_jugadores.h: desplazar la lista solo copia
//      imágenes) y el ✓ de los marcados; abajo cuántos van, TODOS / QUITAR TODOS y SIGUIENTE.
//   2. ¿QUÉ TORNEO JUGAMOS?: LIGA, COPA y POOL en su neón; dos toques como en Inicio (el primero la enciende, el
//      segundo entra tras verse hundida 180 ms; sola se apaga a los 8 s).
//   3. Sin ninguna de ese tipo: el cartel «¡Crea tu primera liga!». Si hay: ELIGE LA LIGA, una tarjeta por
//      competición (insignia, nombre, edición, reglas en pastillas, «?» con las reglas completas y EDITAR, o
//      CONTINUAR si tiene una edición en juego).
//   4. PAREJAS (torneos por parejas y el Pool): la lista con sus fotos, SORTEAR OTRA VEZ (al azar), formarlas a mano
//      tocando dos jugadores, el nombre de cada pareja con el teclado y CREAR TORNEO.
// Se incluye en ui_torneo.h (dentro de namespace tor).

// ---------------------------------------------------------------- lo elegido
static uint32_t picked[jug::MAX];
static uint8_t nPicked = 0;
static Format kind = Format::League;
static char chosenTpl[48] = "";
static char seed[24] = "1";
// Parejas hechas a mano y nombres puestos aquí (se guardan como equipos al crear el torneo).
static s3mem::ExternalArray<char[2][40], torneos::MAX_ENTRANTS / 2> manual;
static uint8_t nManual = 0;
static char pickA[40] = "";
struct PairEdit {
  char a[40], b[40], name[28];
};
static s3mem::ExternalArray<PairEdit, torneos::MAX_ENTRANTS / 2> edits;
static uint8_t nEdits = 0;
static int8_t renaming = -1;  // pareja del borrador a la que se pone nombre

static bool isPicked(uint32_t code) {
  for (uint8_t i = 0; i < nPicked; i++)
    if (picked[i] == code) return true;
  return false;
}
// Solo cuentan los que siguen en la lista (al volver de un torneo puede traer jugadores borrados).
static void cleanPicked() {
  uint8_t k = 0;
  for (uint8_t i = 0; i < nPicked; i++)
    if (store && store->find(picked[i]) && store->isActive(picked[i])) picked[k++] = picked[i];
  nPicked = k;
}
static void pickIds(const char* const* ids, uint8_t n) {
  nPicked = 0;
  for (uint8_t i = 0; i < n && nPicked < jug::MAX; i++) {
    uint32_t c = codeOf(ids[i]);
    if (c && !isPicked(c)) picked[nPicked++] = c;
  }
}

// ---------------------------------------------------------------- 1 · ¿QUIÉN JUEGA?
static lv_obj_t* mark[jug::MAX];  // marco y ✓ de cada ficha marcada
static lv_obj_t* nextBtn = nullptr;
static int8_t nextShown = -1;

static void onBackHome(lv_event_t*) {
  if (hooks.home) hooks.home();
}
static void onPalmares(lv_event_t*) { go(View::Honours); }
static void onNewPlayer(lv_event_t*) {
  if (hooks.players) hooks.players();
}

static void paintPlayersFoot() {
  bool ok = nPicked >= 3;
  if (nextBtn && nextShown != (int8_t)ok) {
    nextShown = (int8_t)ok;
    ui::setOpa(nextBtn, ok ? LV_OPA_COVER : 110);
  }
  refresh();
}
static void onPickCard(lv_event_t* e) {
  int i = argOf(e);
  if (!store || i >= store->count) return;
  uint32_t code = store->list[i].code;
  if (isPicked(code)) {
    uint8_t k = 0;
    for (uint8_t j = 0; j < nPicked; j++)
      if (picked[j] != code) picked[k++] = picked[j];
    nPicked = k;
  } else if (nPicked < jug::MAX) {
    picked[nPicked++] = code;
  }
  ui::setVisible(mark[i], isPicked(code));  // solo cambia esa ficha y el pie
  paintPlayersFoot();
}
static void onAll(lv_event_t*) {
  bool all = store && nPicked == store->activeCount() && nPicked > 0;
  nPicked = 0;
  for (uint8_t i = 0; store && i < store->count; i++) {
    if (!mark[i]) continue;  // de baja: no sale
    if (!all) picked[nPicked++] = store->list[i].code;
    ui::setVisible(mark[i], !all);
  }
  paintPlayersFoot();
}
static void onPlayersNext(lv_event_t*) {
  if (nPicked < 3) return ui::toast("Marca al menos 3 jugadores");
  go(View::Kind);
}
static void fillCount(char* b, size_t n, int) { snprintf(b, n, "%u", nPicked); }
static void fillCountWord(char* b, size_t n, int) { snprintf(b, n, "%s", nPicked == 1 ? "jugador" : "jugadores"); }
static void fillMissing(char* b, size_t n, int) { snprintf(b, n, "Marca al menos %u más", nPicked < 3 ? 3 - nPicked : 0); }
static bool missing(int) { return nPicked < 3; }
static void fillAll(char* b, size_t n, int) { snprintf(b, n, "%s", store && nPicked == store->activeCount() && nPicked ? "QUITAR TODOS" : "TODOS"); }

// Torneos en juego: CONTINUAR o descartar (con confirmación).
static int16_t discarding = -1;  // índice en torneos::summaries
static void openTournament(const char* id);
static void onResume(lv_event_t* e) { openTournament(torneos::summaries[argOf(e)].id); }
static void discardYes() {
  if (discarding < 0 || discarding >= (int16_t)torneos::nSummaries) return;
  char id[48];
  snprintf(id, sizeof(id), "%s", torneos::summaries[discarding].id);
  discarding = -1;
  if (torneos::discard(id)) ui::toast("Torneo descartado");
  else ui::toast("No se pudo descartar");
  if (curOk && !strcmp(cur.id, id)) curOk = false;
  go(view);
}
static void askDiscard(const char* name, uint16_t played, void (*yes)()) {
  char t[96], s[200];
  snprintf(t, sizeof(t), "¿Descartar el torneo?");
  if (played)
    snprintf(s, sizeof(s), "%s se borra junto con sus %u partido%s jugado%s. Esos partidos desaparecen del historial y no cuentan para estadísticas, ELO, experiencia ni logros. No se puede deshacer.",
             name, played, played == 1 ? "" : "s", played == 1 ? "" : "s");
  else snprintf(s, sizeof(s), "%s se borra. No se puede deshacer.", name);
  ask(t, s, "NO, SEGUIR", "DESCARTAR", yes);
}
static void onDiscardRunning(lv_event_t* e) {
  discarding = (int16_t)argOf(e);
  const torneos::Summary& s = torneos::summaries[discarding];
  askDiscard(s.name, s.done, discardYes);
}

static uint8_t runningList(int16_t* out, uint8_t max) {
  uint8_t n = 0;
  for (uint16_t i = 0; i < torneos::nSummaries; i++)
    if (torneos::summaries[i].status == torneos::Status::Active && n < max) out[n++] = (int16_t)i;
  // El más nuevo primero.
  for (uint8_t i = 0; i < n; i++)
    for (uint8_t k = i + 1; k < n; k++)
      if (torneos::summaries[out[k]].createdAt > torneos::summaries[out[i]].createdAt) {
        int16_t x = out[i];
        out[i] = out[k];
        out[k] = x;
      }
  return n;
}

static void buildPlayers() {
  cleanPicked();
  header("¿QUIÉN JUEGA?", "TORNEO", onBackHome);
  iconBtn(scr, 470, 8, 158, 48, Icon::Trophy, "PALMARÉS", 14, Kind::Normal, onPalmares, nullptr, col::GOLD);
  iconBtn(scr, 636, 8, 152, 48, Icon::Plus, "JUGADOR", 14, Kind::Normal, onNewPlayer);
  // En juego (si hay muchos, la lista se desliza: dos a la vista).
  int16_t run[24];
  uint8_t nRun = runningList(run, 24);
  int y = 72;
  if (nRun) {
    int h = (nRun > 2 ? 2 : nRun) * 60 - 4;
    lv_obj_t* list = nRun > 2 ? scroller(scr, 16, y, 768, h) : ui::box(scr, 16, y, 768, h);
    for (uint8_t i = 0; i < nRun; i++) {
      const torneos::Summary& s = torneos::summaries[run[i]];
      lv_obj_t* row = panel(list, 0, i * 60, 768, 56, 0x6b5a1f, 0x14170f);
      chip(row, "EN JUEGO", 10, 14, col::GOLD, col::GOLD, 12);
      line(row, s.name, 800, 17, ui::color::TEXT, 110, 6, 380);
      char p[48];
      if (s.format == Format::Pool) snprintf(p, sizeof(p), "%u partidos jugados", s.done);
      else snprintf(p, sizeof(p), "%u de %u partidos", s.done, s.total);
      line(row, p, 400, 13, ui::color::TEXT2, 110, 30, 380);
      neonGo(row, 520, 5, 188, 44, "CONTINUAR", 14, onResume, (void*)(intptr_t)run[i]);
      lv_obj_t* tr = fichas::btn(row, 716, 5, 46, 44, "", 14, Kind::Danger, onDiscardRunning, (void*)(intptr_t)run[i]);
      icon(tr, Icon::Trash, 11, 10, 24, 0xffd2d8);
    }
    y += h + 8;
  }
  // Rejilla de jugadores: la ficha ya pintada de «elegir jugadores» (6 por fila).
  lv_obj_t* grid = scroller(scr, 16, y, 772, 396 - y);
  lv_obj_set_style_pad_bottom(grid, 6, 0);
  memset(mark, 0, sizeof(mark));
  elegir::store = store;
  for (uint8_t i = 0, k = 0; store && i < store->count; i++) {
    if (!store->isActive(store->list[i].code)) continue;  // de baja: no se elige (como la app)
    lv_draw_buf_t* shot = elegir::cardImage(i, grid);
    int x = (k % 6) * 129, cy = (k / 6) * 187;
    if (!shot) continue;
    k++;
    lv_obj_t* img = lv_image_create(grid);
    lv_image_set_src(img, shot);
    lv_obj_set_pos(img, x, cy);
    lv_obj_add_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);  // que la lista se siga deslizando
    lv_obj_add_event_cb(img, onPickCard, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)i);
    // Marcado: marco de neón y ✓ arriba a la derecha (un solo objeto que se enseña o no).
    lv_obj_t* m = ui::box(grid, x, cy, 123, 181);
    lv_obj_set_style_radius(m, ui::RADIUS_SM, 0);
    lv_obj_set_style_border_width(m, 3, 0);
    lv_obj_set_style_border_color(m, ui::hex(ui::color::ACCENT), 0);
    lv_obj_t* ck = ui::box(m, 123 - 38, 6, 32, 32);
    lv_obj_set_style_radius(ck, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ck, ui::hex(ui::color::ACCENT), 0);
    lv_obj_set_style_bg_opa(ck, LV_OPA_COVER, 0);
    icon(ck, Icon::Check, 6, 6, 20, col::ON_INK);
    lv_obj_add_flag(m, LV_OBJ_FLAG_EVENT_BUBBLE);
    if (!isPicked(store->list[i].code)) lv_obj_add_flag(m, LV_OBJ_FLAG_HIDDEN);
    mark[i] = m;
  }
  if (!store || !store->activeCount()) {
    line(grid, "Sin jugadores", 800, 20, ui::color::TEXT, 0, 30, 768, LV_TEXT_ALIGN_CENTER);
    line(grid, "Créalos con + JUGADOR o pásalos desde la app por USB.", 400, 16, ui::color::TEXT2, 0, 64, 768, LV_TEXT_ALIGN_CENTER);
  }
  footer();
  val(scr, 16, 412, 80, 900, 38, fillCount, 0, ui::color::TEXT, LV_TEXT_ALIGN_RIGHT);
  val(scr, 104, 430, 110, 800, 17, fillCountWord, 0, ui::color::TEXT2);
  lv_obj_t* miss = panel(scr, 214, 420, 200, 40, col::GOLD, 0x1c1a10);
  val(miss, 8, 10, 184, 800, 14, fillMissing, 0, 0xffe9a8, LV_TEXT_ALIGN_CENTER);
  visWhen(miss, missing);
  lv_obj_t* all = tbtn(scr, 424, 412, 164, 60, "QUITAR TODOS", 17, Kind::Normal, onAll);
  vals[valCount] = {lv_obj_get_child(all, 0), fillAll, 0, ui::color::TEXT, ui::color::TEXT};
  valCount++;
  nextBtn = iconBtn(scr, 598, 408, 186, 64, Icon::Play, "SIGUIENTE", 20, Kind::Primary, onPlayersNext);
  nextShown = -1;
  paintPlayersFoot();
}

// ---------------------------------------------------------------- 2 · ¿QUÉ TORNEO JUGAMOS?
static const uint32_t SELECT_TIMEOUT_MS = 8000, PRESS_MS = 180;
static lv_obj_t* kindCard[3];
static int8_t kindSel = -1, kindPressed = -1;
static lv_timer_t *kindSelTimer = nullptr, *kindPressTimer = nullptr;
static const HelpText KIND_HELP[3] = {
    {"Liga", "Todos contra todos. Gana quien más puntos sume; puede acabar con final."},
    {"Copa", "Eliminatoria: quien pierde, fuera, hasta llegar a la final."},
    {"Pool", "Escalera de pistas: el que gana sube y el que pierde baja. La final, en la pista 1."},
};
static const Format FORMATS[3] = {Format::League, Format::Bracket, Format::Pool};

static void paintKindCard(int i) {
  lv_obj_t* c = kindCard[i];
  if (!c) return;
  uint32_t k = kindColor(FORMATS[i]);
  bool on = kindSel == i, down = kindPressed == i;
  lv_obj_set_style_border_width(c, on ? 3 : 2, 0);
  lv_obj_set_style_border_opa(c, on ? LV_OPA_COVER : 120, 0);
  lv_obj_set_style_shadow_width(c, on ? 26 : 0, 0);
  lv_obj_set_style_bg_grad_color(c, ui::hex(down ? 0x05080e : on ? 0x101a2c : 0x070b12), 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(c, ui::hex(down ? 0x0a0f18 : on ? (k & 0xfefefe) >> 1 : (k & 0xfcfcfc) >> 2), 0);
}
static void onKindSelTimeout(lv_timer_t*) {
  kindSelTimer = nullptr;
  if (kindPressed >= 0) return;
  int8_t was = kindSel;
  kindSel = -1;
  if (was >= 0) paintKindCard(was);
}
static void onKindPressDone(lv_timer_t*) {
  kindPressTimer = nullptr;
  int8_t i = kindPressed;
  kindPressed = kindSel = -1;
  if (i < 0) return;
  kind = FORMATS[i];
  chosenTpl[0] = 0;
  bool any = false;
  for (uint8_t t = 0; t < torneos::nTemplates; t++)
    if (torneos::templates[t].format == kind) any = true;
  go(any ? View::Choose : View::Empty);
}
static void onKindCard(lv_event_t* e) {
  if (kindPressed >= 0) return;
  int i = argOf(e);
  if (kindSelTimer) lv_timer_delete(kindSelTimer);
  kindSelTimer = nullptr;
  if (kindSel != i) {
    int8_t was = kindSel;
    kindSel = (int8_t)i;
    if (was >= 0) paintKindCard(was);
    paintKindCard(i);
    kindSelTimer = lv_timer_create(onKindSelTimeout, SELECT_TIMEOUT_MS, nullptr);
    lv_timer_set_repeat_count(kindSelTimer, 1);
    return;
  }
  kindPressed = (int8_t)i;
  paintKindCard(i);
  kindPressTimer = lv_timer_create(onKindPressDone, PRESS_MS, nullptr);
  lv_timer_set_repeat_count(kindPressTimer, 1);
}
static void onBackToPlayers(lv_event_t*) { go(View::Players); }

// Ids de los marcados (para el contrato).
static s3mem::ExternalArray<char[40], jug::MAX> selIds;
static s3mem::ExternalArray<torneos::Fit, torneos::MAX_TEMPLATES> kindFits;
static uint8_t selCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < nPicked && store; i++) {
    const jug::Player* p = store->find(picked[i]);
    if (p) store->idOf(p->code, selIds[n++], 40);
  }
  return n;
}

static void buildKind() {
  char sub[24];
  snprintf(sub, sizeof(sub), "%u JUGADORES", nPicked);
  header("¿QUÉ TORNEO JUGAMOS?", sub, onBackToPlayers);
  uint8_t n = selCount();
  static const Icon ICONS[3] = {Icon::Table, Icon::Trophy, Icon::Ladder};
  for (int i = 0; i < 3; i++) {
    Format f = FORMATS[i];
    uint32_t k = kindColor(f);
    lv_obj_t* c = ui::box(scr, 16 + i * 261, 78, 246, 390);
    kindCard[i] = c;
    lv_obj_set_style_radius(c, 18, 0);
    lv_obj_set_style_border_color(c, ui::hex(k), 0);
    lv_obj_set_style_shadow_color(c, ui::hex(k), 0);
    lv_obj_set_style_shadow_opa(c, 110, 0);
    lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c, onKindCard, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* t = ui::text(c, kindTitle(f), 900, 72, k, 2, true);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 58);
    icon(c, ICONS[i], 68, 168, 110, k);
    // Cuántas hay para este número de jugadores, por qué no encaja ninguna, o crear la primera.
    uint8_t total = 0, ok = 0;
    const char* why = nullptr;
    auto* fits = kindFits.data();
    for (uint8_t t2 = 0; t2 < torneos::nTemplates; t2++) {
      if (torneos::templates[t2].format != f) continue;
      total++;
      fits[t2] = torneos::templateFit(torneos::templates[t2], n);
      if (fits[t2].ok) ok++;
      else if (!why) why = fits[t2].reason;
    }
    char s[96];
    uint32_t ink = k;
    if (!total) snprintf(s, sizeof(s), "+ CREAR");
    else if (ok) snprintf(s, sizeof(s), "%u disponible%s para %u jugadores", ok, ok == 1 ? "" : "s", n);
    else {
      snprintf(s, sizeof(s), "× %s", why ? why : "Ninguna encaja");
      ink = ui::color::DANGER;
    }
    para(c, s, 800, 17, ink, 16, 306, 214, LV_TEXT_ALIGN_CENTER);
    if (total && !ok) lv_obj_set_style_opa(t, 150, 0);
    helpBtn(c, 246 - 56, 12, 44, &KIND_HELP[i]);
    paintKindCard(i);
  }
}

// ---------------------------------------------------------------- 3a · ninguna creada: el cartel de neón
static void startCreator(const torneos::Template* edit, const torneos::Template* copyOf, Format f);
static void onCreateFirst(lv_event_t*) { startCreator(nullptr, nullptr, kind); }
static void onBackToKind(lv_event_t*) { go(View::Kind); }
static void buildEmpty() {
  char sub[24];
  snprintf(sub, sizeof(sub), "%u JUGADORES", nPicked);
  uint32_t k = kindColor(kind);
  ui::gradient(scr, (k & 0xfcfcfc) >> 2, 0x05090f);
  header(kindTitle(kind), sub, onBackToKind, k);
  lv_obj_t* sign = panel(scr, 100, 100, 600, 190, k, 0x070b12);
  lv_obj_set_style_border_width(sign, 3, 0);
  lv_obj_set_style_shadow_width(sign, 30, 0);
  lv_obj_set_style_shadow_color(sign, ui::hex(k), 0);
  lv_obj_set_style_shadow_opa(sign, 120, 0);
  char a[64], b[64];
  bool fem = kind != Format::Pool;
  snprintf(a, sizeof(a), "No hay %s %s %s", fem ? "ninguna" : "ningún", kindNoun(kind), fem ? "creada" : "creado");
  snprintf(b, sizeof(b), "¡Crea tu %s %s!", fem ? "primera" : "primer", kindNoun(kind));
  line(sign, a, 800, 20, ui::color::TEXT2, 0, 36, 600, LV_TEXT_ALIGN_CENTER);
  line(sign, b, 900, 38, k, 0, 86, 600, LV_TEXT_ALIGN_CENTER);
  char c[32];
  snprintf(c, sizeof(c), "CREAR %s", kindTitle(kind));
  lv_obj_t* go_ = iconBtn(scr, 200, 318, 400, 80, Icon::Plus, c, 23, Kind::Normal, onCreateFirst, nullptr, k);
  lv_obj_set_style_border_width(go_, 2, 0);
  lv_obj_set_style_border_color(go_, ui::hex(k), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(go_, 0), ui::hex(k), 0);
  line(scr, KIND_HELP[kind == Format::League ? 0 : kind == Format::Bracket ? 1 : 2].text, 400, 16, ui::color::TEXT2, 0, 424, 800,
       LV_TEXT_ALIGN_CENTER);
}

// ---------------------------------------------------------------- 3b · ELIGE LA LIGA
// El borrador de esta edición (lo que se va a crear): se rehace con cada cambio (es barato).
static torneos::Draft draft;
static s3mem::ExternalArray<char[40], torneos::MAX_ENTRANTS> leftover;
static s3mem::ExternalArray<char[2][40], (equipos::MAX > torneos::MAX_ENTRANTS ? equipos::MAX : torneos::MAX_ENTRANTS)> draftFixed;
static uint8_t nLeft = 0;
static const torneos::Template* draftTpl = nullptr;
static uint16_t draftEdition = 1;
static char draftError[128] = "";

static const torneos::Template* tplById(const char* id) {
  for (uint8_t i = 0; i < torneos::nTemplates; i++)
    if (!strcmp(torneos::templates[i].id, id)) return &torneos::templates[i];
  return nullptr;
}
static bool tplFree(const torneos::Template& t, uint8_t n) {
  return t.format == kind && torneos::templateFit(t, n).ok && torneos::activeEdition(t.id) < 0;
}
static bool pairsOf(const torneos::Template& t) { return t.format == Format::Pool || t.teamSize == 2; }
static bool swapPool(const torneos::Template& t) { return t.format == Format::Pool && t.poolSwap; }
static bool manualOf(const torneos::Template& t) { return pairsOf(t) && t.pairing == torneos::Pairing::Manual; }
static const PairEdit* editOf(const char* a, const char* b) {
  for (uint8_t i = 0; i < nEdits; i++)
    if ((!strcmp(edits[i].a, a) && !strcmp(edits[i].b, b)) || (!strcmp(edits[i].a, b) && !strcmp(edits[i].b, a))) return &edits[i];
  return nullptr;
}
static int eloFn(const char* id) { return eloOf(id); }
static const char* nameFn(const char* id) { return nameOf(id); }

// Como la app: el elegido (o el primero libre), su edición, las parejas fijas que valen y lo que falta.
static void buildDraft() {
  uint8_t n = selCount();
  draftTpl = nullptr;
  nLeft = 0;
  draftError[0] = 0;
  const torneos::Template* t = tplById(chosenTpl);
  if (!t || !tplFree(*t, n)) {
    t = nullptr;
    for (uint8_t i = 0; i < torneos::nTemplates && !t; i++)
      if (tplFree(torneos::templates[i], n)) t = &torneos::templates[i];
  }
  if (!t) {
    uint8_t fitting = 0;
    for (uint8_t i = 0; i < torneos::nTemplates; i++)
      if (torneos::templates[i].format == kind && torneos::templateFit(torneos::templates[i], n).ok) fitting++;
    if (fitting)
      snprintf(draftError, sizeof(draftError), "%s una edición en juego: termínala o descártala antes de empezar otra.",
               fitting == 1 ? (kind == Format::Pool ? "El pool ya tiene" : kind == Format::League ? "La liga ya tiene" : "La copa ya tiene")
                            : (kind == Format::Pool ? "Todos tienen" : "Todas tienen"));
    else snprintf(draftError, sizeof(draftError), "Ninguna %s encaja con %u jugadores: crea una o elige otro tipo.", kindNoun(kind), n);
    return;
  }
  snprintf(chosenTpl, sizeof(chosenTpl), "%s", t->id);
  draftTpl = t;
  draftEdition = torneos::nextEdition(t->id);
  char name[64];
  snprintf(name, sizeof(name), "%s · %uª edición", t->name, draftEdition);
  // Parejas: las hechas aquí (a mano), ninguna con cambio de pareja, o los equipos guardados.
  auto* fixed = draftFixed.data();
  uint8_t nFixed = 0;
  if (manualOf(*t)) {
    for (uint8_t i = 0; i < nManual; i++) {
      memcpy(fixed[nFixed][0], manual[i][0], 40);
      memcpy(fixed[nFixed++][1], manual[i][1], 40);
    }
  } else if (!swapPool(*t)) {
    for (uint8_t i = 0; i < equipos::count; i++) {
      memcpy(fixed[nFixed][0], equipos::list[i].player[0], 40);
      memcpy(fixed[nFixed++][1], equipos::list[i].player[1], 40);
    }
  }
  nLeft = torneos::draftFromTemplate(*t, name, selIds, n, eloFn, seed, fixed, nFixed, draft, leftover);
  draft.edition = draftEdition;
  snprintf(draft.logo, sizeof(draft.logo), "%s", t->logo);
  snprintf(draft.cup, sizeof(draft.cup), "%s", t->cup);
  snprintf(draft.banner, sizeof(draft.banner), "%s", t->banner);
  // Nombre (y escudo) de las parejas fijas: lo escrito aquí o su equipo guardado.
  for (uint8_t i = 0; i < draft.nTeams; i++) {
    torneos::Draft::DTeam& d = draft.teams[i];
    if (d.n != 2 || swapPool(*t)) continue;
    const PairEdit* ed = editOf(d.player[0], d.player[1]);
    const equipos::Club* club = equipos::find(d.player[0], d.player[1]);
    if (ed) snprintf(d.name, sizeof(d.name), "%s", ed->name);
    else if (club) {
      snprintf(d.name, sizeof(d.name), "%s", club->name);
      snprintf(d.logo, sizeof(d.logo), "%s", club->logo);
      snprintf(d.clubId, sizeof(d.clubId), "%s", club->id);
    }
  }
  if (nLeft) {
    if (manualOf(*t)) snprintf(draftError, sizeof(draftError), "Faltan %u jugadores por emparejar.", nLeft);
    else snprintf(draftError, sizeof(draftError), "Sobra %s.", nameOf(leftover[0]));
  }
}
// En la lista, con parejas a mano aún sin formar no es un error: se forman en la ventana de parejas.
static bool mainBlocked() {
  if (!draftTpl) return true;
  if (manualOf(*draftTpl) && selCount() % 2 == 0) return false;
  return draftError[0] != 0;
}

static lv_obj_t* tplCard[torneos::MAX_TEMPLATES];
static lv_obj_t* chooseNext = nullptr;
static void paintChoose() {
  for (uint8_t i = 0; i < torneos::nTemplates; i++) {
    lv_obj_t* c = tplCard[i];
    if (!c) continue;
    bool on = draftTpl == &torneos::templates[i];
    lv_color_t bc = ui::hex(on ? ui::color::ACCENT : ui::color::LINE);
    if (!lv_color_eq(lv_obj_get_style_border_color(c, LV_PART_MAIN), bc)) {
      lv_obj_set_style_border_color(c, bc, 0);
      lv_obj_set_style_border_width(c, on ? 2 : 1, 0);
      lv_obj_set_style_bg_color(c, ui::hex(on ? 0x10304a : ui::color::CARD), 0);
    }
  }
  if (chooseNext) ui::setOpa(chooseNext, mainBlocked() ? 110 : LV_OPA_COVER);
  refresh();
}
static void onTplCard(lv_event_t* e) {
  const torneos::Template& t = torneos::templates[argOf(e)];
  if (torneos::activeEdition(t.id) >= 0) return;
  snprintf(chosenTpl, sizeof(chosenTpl), "%s", t.id);
  buildDraft();
  paintChoose();
}
static void onTplEdit(lv_event_t* e) { startCreator(&torneos::templates[argOf(e)], nullptr, kind); }
static void onTplContinue(lv_event_t* e) {
  int16_t s = torneos::activeEdition(torneos::templates[argOf(e)].id);
  if (s >= 0) openTournament(torneos::summaries[s].id);
}
static void onCreateTpl(lv_event_t*) { startCreator(nullptr, nullptr, kind); }

// Reglas completas de una competición (la «?» de su tarjeta): se escriben al abrirla.
static char rulesHelpText[640];
static char rulesHelpTitle[32];
static void rulesSummary(const torneos::Template& t, char* out, size_t n) {
  char r[96];
  torneos::Rules rr = t.rules;
  rulesText(rr, r, sizeof(r));
  int k = snprintf(out, n, "PARTIDOS: %s", r);
  auto add = [&](const char* fmt, auto... a) {
    if (k < (int)n - 1) k += snprintf(out + k, n - k, fmt, a...);
  };
  if (t.format == Format::Pool) {
    add("\nPAREJAS: %s", t.poolSwap ? "con cambio de pareja (en cada pista, por sorteo)." : "las mismas todo el pool.");
    if (t.poolByRounds) add("\nDURACIÓN: %u rondas; la pista 1 de la última es la final.", t.poolRounds);
    else add("\nDURACIÓN: %u minutos; cuando solo dé para un partido, final en la pista 1.", t.poolMinutes);
  }
  bool draws = t.format == Format::League && t.rules.end != torneos::End::Goals && t.allowDraw;
  if (t.format == Format::League) {
    add("\nEMPATE: %s", draws ? "se puede empatar, sin prórroga ni penaltis." : "siempre hay ganador.");
    if (draws) add("\nPUNTOS: victoria %u · empate %u · derrota %u", t.points.win, t.points.draw, t.points.loss);
    else add("\nPUNTOS: victoria %u · derrota %u", t.points.win, t.points.loss);
    add("\nVUELTAS: %s", t.legs == 2 ? "ida y vuelta." : "solo ida.");
  }
  add("\nÚLTIMO: se lleva el título «%s».", t.lastTitle[0] ? t.lastTitle : "Patético");
  if (t.format != Format::League || t.finalKind == torneos::FinalKind::Top2)
    add("\nFINAL: %s%s.", t.format == Format::League ? "1º contra 2º, " : t.format == Format::Pool ? "en la pista 1, " : "",
        t.finalBestOf > 1 ? "al mejor de 3" : "a un partido");
  else add("\nFINAL: sin final; gana el primero de la clasificación.");
  add("\nELO: %s", t.ranked ? "cuenta para el ELO." : "no cuenta para el ELO.");
}
static void onTplHelp(lv_event_t* e) {
  const torneos::Template& t = torneos::templates[argOf(e)];
  rulesSummary(t, rulesHelpText, sizeof(rulesHelpText));
  snprintf(rulesHelpTitle, sizeof(rulesHelpTitle), "%s", t.name);
  lv_obj_t* card = popCard(720, 440, ui::color::ACCENT);
  lv_obj_add_event_cb(pop, onPopClose, LV_EVENT_CLICKED, nullptr);
  line(card, rulesHelpTitle, 900, 24, ui::color::ACCENT, 24, 18, 672);
  para(card, rulesHelpText, 400, 16, ui::color::TEXT, 24, 58, 672);
  tbtn(card, 230, 362, 260, 60, "VALE", 20, Kind::Primary, onPopClose);
}

// Reglas en pastillas (ruleChips de la app).
static void ruleChips(lv_obj_t* parent, const torneos::Template& t, int x, int y, int maxW) {
  char c[10][24];
  int n = 0;
  if (t.format == Format::Pool) {
    snprintf(c[n++], 24, "%s", t.poolSwap ? "Cambio de pareja" : "Parejas fijas");
    if (t.poolByRounds) snprintf(c[n++], 24, "%u rondas", t.poolRounds);
    else snprintf(c[n++], 24, "%u min", t.poolMinutes);
  } else snprintf(c[n++], 24, "%s", t.teamSize == 2 ? "Parejas" : "1 vs 1");
  if (t.rules.end != torneos::End::Time) snprintf(c[n++], 24, "A %u", t.rules.goals);
  if (t.rules.end != torneos::End::Goals) snprintf(c[n++], 24, "%u'", t.rules.minutes);
  bool draws = t.format == Format::League && t.rules.end != torneos::End::Goals && t.allowDraw;
  if (t.format == Format::League) {
    snprintf(c[n++], 24, "%s", t.legs == 2 ? "Ida y vuelta" : "Solo ida");
    if (draws) snprintf(c[n++], 24, "%u-%u-%u", t.points.win, t.points.draw, t.points.loss);
    else snprintf(c[n++], 24, "%u-%u", t.points.win, t.points.loss);
    if (draws) snprintf(c[n++], 24, "Empate");
  }
  bool fin = t.format != Format::League || t.finalKind == torneos::FinalKind::Top2;
  snprintf(c[n++], 24, "%s", fin ? (t.finalBestOf > 1 ? "Final al mejor de 3" : "Final") : "Sin final");
  if (t.ranked) snprintf(c[n++], 24, "ELO");
  int cx = x;
  for (int i = 0; i < n; i++) {
    int w = textW(c[i], 800, 12) + 18;
    if (cx + w > x + maxW) break;  // las que no caben, en la «?»
    chip(parent, c[i], cx, y, ui::color::TEXT2, ui::color::LINE, 12);
    cx += w + 6;
  }
}

static void fillChooseNote(char* b, size_t n, int) {
  if (mainBlocked() && draftError[0]) snprintf(b, n, "!%s", draftError);
  else if (draftTpl && pairsOf(*draftTpl)) snprintf(b, n, "Ahora, las parejas");
  else b[0] = 0;
}
static void createNow();
static void onChooseNext(lv_event_t*) {
  buildDraft();
  if (mainBlocked()) return ui::toast(draftError[0] ? draftError : "Elige una");
  if (pairsOf(*draftTpl)) go(View::Pairs);
  else createNow();
}

static void buildChoose() {
  char t[32], sub[24], cr[32];
  snprintf(t, sizeof(t), "ELIGE %s", kind == Format::Pool ? "EL POOL" : kind == Format::League ? "LA LIGA" : "LA COPA");
  snprintf(sub, sizeof(sub), "%u JUGADORES", nPicked);
  header(t, sub, onBackToKind);
  snprintf(cr, sizeof(cr), "CREAR %s", kindTitle(kind));
  iconBtn(scr, 598, 8, 190, 48, Icon::Plus, cr, 14, Kind::Normal, onCreateTpl);
  uint8_t n = selCount();
  buildDraft();
  memset(tplCard, 0, sizeof(tplCard));
  lv_obj_t* list = scroller(scr, 16, 72, 772, 324);
  int k = 0;
  for (uint8_t i = 0; i < torneos::nTemplates; i++) {
    const torneos::Template& tp = torneos::templates[i];
    if (tp.format != kind) continue;
    torneos::Fit fitT = torneos::templateFit(tp, n);
    if (!fitT.ok) continue;  // los que no encajan con este número de jugadores no se enseñan (como la app)
    int16_t live = torneos::activeEdition(tp.id);
    int y = k++ * 110;
    lv_obj_t* c = panel(list, 0, y, 768, 102, ui::color::LINE, ui::color::CARD);
    tplCard[i] = c;
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(c, onTplCard, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)i);
    lv_obj_add_flag(c, LV_OBJ_FLAG_EVENT_BUBBLE);
    crest(c, tp.logo, tp.name, 14, 18, 64);
    line(c, tp.name, 900, 24, ui::color::TEXT, 92, 8, 400);
    cup(c, tp.cup, 500, 8, 32);
    char s[96];
    if (live >= 0) {
      snprintf(s, sizeof(s), "%uª edición en juego · termínala o descártala para empezar otra", torneos::summaries[live].edition);
      lv_obj_t* l = para(c, s, 800, 14, col::GOLD, 92, 44, 470);
      (void)l;
      lv_obj_set_style_opa(c, LV_OPA_COVER, 0);
    } else {
      char first[48];
      snprintf(first, sizeof(first), "%s", fitT.summary);
      if (char* dot = strstr(first, " · ")) *dot = 0;
      snprintf(s, sizeof(s), "%uª edición · %s", torneos::nextEdition(tp.id), first);
      line(c, s, 400, 16, ui::color::TEXT2, 92, 40, 470);
      ruleChips(c, tp, 92, 66, 474);
    }
    // La «?» de cada tarjeta escribe sus reglas al abrirse.
    lv_obj_t* hb = fichas::btn(c, 584, 25, 52, 52, "?", 20, Kind::Normal, onTplHelp, (void*)(intptr_t)i);
    lv_obj_set_style_radius(hb, 26, 0);
    if (live >= 0) tbtn(c, 646, 23, 110, 56, "CONTINUAR", 14, Kind::Primary, onTplContinue, (void*)(intptr_t)i);
    else tbtn(c, 646, 23, 110, 56, "EDITAR", 17, Kind::Normal, onTplEdit, (void*)(intptr_t)i);
  }
  if (!k) {
    char e[80];
    snprintf(e, sizeof(e), "Aún no hay %s %s para %u jugadores.", kind == Format::Pool ? "ningún" : "ninguna", kindNoun(kind), n);
    line(list, e, 800, 20, ui::color::TEXT, 0, 60, 768, LV_TEXT_ALIGN_CENTER);
    iconBtn(list, 234, 110, 300, 72, Icon::Plus, cr, 20, Kind::Primary, onCreateTpl);
  }
  footer();
  val(scr, 24, 424, 500, 800, 14, fillChooseNote, 0, ui::color::TEXT2, LV_TEXT_ALIGN_LEFT, true);
  chooseNext = iconBtn(scr, 560, 408, 224, 64, Icon::Play, "CONTINUAR", 20, Kind::Primary, onChooseNext);
  paintChoose();
}

// ---------------------------------------------------------------- 4 · PAREJAS
static void onPairsBack(lv_event_t*) {
  pickA[0] = 0;
  go(View::Choose);
}
static lv_obj_t* leftBtn[torneos::MAX_ENTRANTS];
static void onLeftover(lv_event_t* e) {
  int i = argOf(e);
  if (i >= nLeft) return;
  const char* id = leftover[i];
  if (!pickA[0]) {
    snprintf(pickA, sizeof(pickA), "%s", id);
  } else if (!strcmp(pickA, id)) {
    pickA[0] = 0;
  } else if (nManual < torneos::MAX_ENTRANTS / 2) {
    snprintf(manual[nManual][0], 40, "%s", pickA);
    snprintf(manual[nManual][1], 40, "%s", id);
    nManual++;
    pickA[0] = 0;
    go(View::Pairs);  // la lista de parejas cambia: se rehace la ventana
    return;
  }
  for (uint8_t k = 0; k < nLeft; k++) {  // solo cambia el borde de los tocados
    bool on = !strcmp(pickA, leftover[k]);
    lv_obj_set_style_border_color(leftBtn[k], ui::hex(on ? ui::color::ACCENT : ui::color::LINE), 0);
    lv_obj_set_style_border_width(leftBtn[k], on ? 3 : 1, 0);
  }
}
static void onUnpair(lv_event_t* e) {
  const torneos::Draft::DTeam& d = draft.teams[argOf(e)];
  uint8_t k = 0;
  for (uint8_t i = 0; i < nManual; i++) {
    bool same = (!strcmp(manual[i][0], d.player[0]) && !strcmp(manual[i][1], d.player[1])) ||
                (!strcmp(manual[i][0], d.player[1]) && !strcmp(manual[i][1], d.player[0]));
    if (!same) memcpy(manual[k++], manual[i], sizeof(manual[0]));
  }
  nManual = k;
  go(View::Pairs);
}
static void onReshuffle(lv_event_t*) {
  snprintf(seed, sizeof(seed), "%lu", (unsigned long)(lv_tick_get() ^ (uint32_t)nowMs() ^ (uint32_t)hashOf(seed)));
  go(View::Pairs);
}
static void pairNameDone(const char* text) {
  if (renaming >= 0 && renaming < draft.nTeams) {
    const torneos::Draft::DTeam& d = draft.teams[renaming];
    PairEdit* ed = (PairEdit*)editOf(d.player[0], d.player[1]);
    if (!ed && nEdits < torneos::MAX_ENTRANTS / 2) ed = &edits[nEdits++];
    if (ed) {
      snprintf(ed->a, 40, "%s", d.player[0]);
      snprintf(ed->b, 40, "%s", d.player[1]);
      snprintf(ed->name, sizeof(ed->name), "%s", text);
    }
  }
  renaming = -1;
  go(View::Pairs);
}
static void onRename(lv_event_t* e) {
  renaming = (int8_t)argOf(e);
  openKeyboard("NOMBRE DE LA PAREJA", draft.teams[renaming].name, 24, pairNameDone, View::Pairs);
}
static void fillPairsNote(char* b, size_t n, int) {
  if (draftError[0]) snprintf(b, n, "!%s", draftError);
  else snprintf(b, n, "%u parejas listas", draft.nTeams);
}
static void onCreate(lv_event_t*) { createNow(); }

static void buildPairs() {
  buildDraft();
  if (!draftTpl) return go(View::Choose);
  char t[48];
  if (swapPool(*draftTpl)) snprintf(t, sizeof(t), "PAREJAS DEL PRIMER PARTIDO");
  else snprintf(t, sizeof(t), "PAREJAS · %uª EDICIÓN", draftEdition);
  header(t, nullptr, onPairsBack);
  int y = 72;
  memset(leftBtn, 0, sizeof(leftBtn));
  bool man = manualOf(*draftTpl);
  if (man && nLeft) {
    caption(scr, "TOCA DOS JUGADORES PARA JUNTARLOS EN UNA PAREJA", 16, y);
    y += 22;
    for (uint8_t i = 0; i < nLeft; i++) {
      int x = 16 + (i % 4) * 194, yy = y + (i / 4) * 62;
      lv_obj_t* b = fichas::btn(scr, x, yy, 186, 56, "", 14, Kind::Normal, onLeftover, (void*)(intptr_t)i);
      bool on = !strcmp(pickA, leftover[i]);
      lv_obj_set_style_border_color(b, ui::hex(on ? ui::color::ACCENT : ui::color::LINE), 0);
      lv_obj_set_style_border_width(b, on ? 3 : 1, 0);
      face(b, leftover[i], 6, 6, 44);
      line(b, nameOf(leftover[i]), 800, 17, ui::color::TEXT, 58, 18, 120);
      leftBtn[i] = b;
    }
    y += ((nLeft + 3) / 4) * 62 + 4;
  }
  lv_obj_t* list = scroller(scr, 16, y, 772, 396 - y);
  bool swap = swapPool(*draftTpl);
  for (uint8_t i = 0; i < draft.nTeams; i++) {
    const torneos::Draft::DTeam& d = draft.teams[i];
    lv_obj_t* row = panel(list, 0, i * 64, 768, 58, ui::color::LINE, ui::color::CARD);
    int x = 12;
    if (!swap) {
      crest(row, d.logo, d.name[0] ? d.name : nameOf(d.player[0]), x, 9, 40);
      x += 52;
      line(row, d.name[0] ? d.name : "Sin nombre", 800, 18, d.name[0] ? ui::color::TEXT : ui::color::TEXT3, x, 17, 200);
      x += 210;
    }
    for (uint8_t k = 0; k < d.n; k++) {
      face(row, d.player[k], x, 9, 40);
      line(row, nameOf(d.player[k]), 800, 17, ui::color::TEXT, x + 48, 18, swap ? 240 : 120);
      x += swap ? 300 : 178;
    }
    int bx = 768 - 12;
    if (man) {
      bx -= 48;
      fichas::btn(row, bx, 7, 48, 44, "×", 23, Kind::Normal, onUnpair, (void*)(intptr_t)i);
      bx -= 8;
    }
    if (!swap) {
      bx -= 110;
      tbtn(row, bx, 7, 110, 44, "NOMBRE", 14, Kind::Normal, onRename, (void*)(intptr_t)i);
    }
  }
  footer();
  val(scr, 24, 430, 330, 800, 14, fillPairsNote, 0, ui::color::TEXT2, LV_TEXT_ALIGN_LEFT, true);
  if (draftTpl->pairing == torneos::Pairing::Random)
    iconBtn(scr, 360, 412, 196, 60, Icon::Again, "SORTEAR OTRA VEZ", 14, Kind::Normal, onReshuffle);
  lv_obj_t* c = iconBtn(scr, 564, 408, 220, 64, Icon::Play, "CREAR TORNEO", 18, Kind::Primary, onCreate);
  if (draftError[0]) ui::setOpa(c, 110);
  refresh();
}

// Crear: las parejas con nombre se guardan como equipos (o se actualizan) y el torneo las enlaza, como la app.
static void createNow() {
  buildDraft();
  if (!draftTpl || draftError[0]) return ui::toast(draftError[0] ? draftError : "No se pudo crear");
  uint64_t now = nowMs();
  for (uint8_t i = 0; i < draft.nTeams; i++) {
    torneos::Draft::DTeam& d = draft.teams[i];
    if (d.n != 2 || swapPool(*draftTpl)) continue;
    const PairEdit* ed = editOf(d.player[0], d.player[1]);
    if (!ed) continue;
    const equipos::Club* old = equipos::find(d.player[0], d.player[1]);
    int k = equipos::put(old ? old->id : "", ed->name, d.player[0], d.player[1], now);
    if (k >= 0) snprintf(d.clubId, sizeof(d.clubId), "%s", equipos::list[k].id);
  }
  char err[96] = "", calSeed[32];
  snprintf(calSeed, sizeof(calSeed), "%s-cal", seed);
  if (!torneos::alloc(cur)) return ui::toast("No hay memoria para el torneo");
  if (!torneos::createTournament(draft, eloFn, nameFn, now, calSeed, cur, err, sizeof(err))) {
    curOk = false;
    return ui::toast(err[0] ? err : "No se pudo crear");
  }
  if (!torneos::save(cur)) {
    curOk = false;
    return ui::toast("No se pudo guardar el torneo");
  }
  curOk = true;
  nManual = nEdits = 0;
  pickA[0] = 0;
  openTournament(cur.id);
}
