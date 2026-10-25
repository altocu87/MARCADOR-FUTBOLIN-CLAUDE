// MARCADOR FUTBOLÍN V3 · AJUSTES → EQUIPOS, DATOS y el QR de un jugador (dentro de namespace opciones).
// -------------------------------------------------------------------------------------------------------
// Se incluye en s3_opciones.h, después de sus piezas (header, caption, opt, val, helpBtn, scroller) y antes de go().
// EQUIPOS: lista de parejas con nombre; crear o cambiar = nombre (teclado) → elegir los dos jugadores → GUARDAR.
// DATOS: modo prueba (activar / quitar) y copias de seguridad en la SD (hacer, restaurar, borrar).
// QR: el código del perfil del jugador, grande, mientras se publica (s3_perfil.h).

// ---------------------------------------------------------------- piezas comunes
// Cara del jugador (foto o iniciales), como en JUGADORES.
static void faceOf(lv_obj_t* parent, uint32_t code, int x, int y, int size, uint16_t ini) {
  const jug::Player* p = jugadores.find(code);
  if (!p) return;
  fichas::store = &jugadores;
  fichas::face(parent, *p, x, y, size, size / 8, ini);
}

// Aviso con dos botones (encima de todo); el «sí» llama a `yes`.
static lv_obj_t* askBox = nullptr;
static void (*askYes)() = nullptr;
static void onAskNo(lv_event_t*) {
  if (askBox) lv_obj_delete(askBox);
  askBox = nullptr;
}
static void onAskYes(lv_event_t*) {
  onAskNo(nullptr);
  if (askYes) askYes();
}
static void ask(const char* text, const char* yesLabel, void (*yes)(), bool danger = true) {
  onAskNo(nullptr);
  askYes = yes;
  askBox = ui::box(scr, 0, 0, 800, 480);
  lv_obj_set_style_bg_color(askBox, ui::hex(0x000000), 0);
  lv_obj_set_style_bg_opa(askBox, 190, 0);
  lv_obj_add_flag(askBox, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* card = ui::box(askBox, 90, 100, 620, 280);
  lv_obj_set_style_bg_color(card, ui::hex(ui::color::CARD2), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, ui::RADIUS, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, ui::hex(danger ? ui::color::DANGER : ui::color::ACCENT), 0);
  lv_obj_t* l = ui::text(card, text, 400, 22, ui::color::TEXT);
  lv_obj_set_width(l, 572);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_pos(l, 24, 22);
  fichas::btn(card, 24, 190, 270, 68, "NO", 20, fichas::Kind::Normal, onAskNo);
  fichas::btn(card, 326, 190, 270, 68, yesLabel, 20, danger ? fichas::Kind::Danger : fichas::Kind::Primary, onAskYes);
}

// Barra de progreso de una tarea larga (datos): se pinta en el momento, sin esperar a loop().
static lv_obj_t *workBox = nullptr, *workBar = nullptr, *workText = nullptr;
static void workProgress(const char* what, int done, int total) {
  if (!scr) return;
  if (!workBox) {
    workBox = ui::box(scr, 0, 0, 800, 480);
    lv_obj_set_style_bg_color(workBox, ui::hex(0x000000), 0);
    lv_obj_set_style_bg_opa(workBox, 200, 0);
    lv_obj_add_flag(workBox, LV_OBJ_FLAG_CLICKABLE);
    workText = ui::text(workBox, "", 900, 26, ui::color::TEXT);
    lv_obj_set_width(workText, 700);
    lv_obj_set_style_text_align(workText, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(workText, 50, 170);
    workBar = lv_bar_create(workBox);
    lv_obj_set_pos(workBar, 100, 240);
    lv_obj_set_size(workBar, 600, 28);
    lv_bar_set_range(workBar, 0, 100);
    lv_obj_set_style_bg_color(workBar, ui::hex(ui::color::LINE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(workBar, ui::hex(ui::color::ACCENT), LV_PART_INDICATOR);
  }
  int pct = total > 0 ? (done >= total ? 100 : done * 100 / total) : 0;
  char b[80];
  snprintf(b, sizeof(b), "%s… %d %%", what, pct);
  if (strcmp(lv_label_get_text(workText), b)) {
    lv_label_set_text(workText, b);
    lv_bar_set_value(workBar, pct, LV_ANIM_OFF);
    uiport::resyncPanel();  // se está escribiendo en la memoria del chip: la imagen se realinea con cada avance
    lv_refr_now(uiport::display);
  }
}
static void workDone() {
  if (workBox) lv_obj_delete(workBox);
  workBox = workBar = workText = nullptr;
  lastActivity = millis();
}

// ---------------------------------------------------------------- EQUIPOS
static int teamIdx = -1;                        // el que se cambia (-1: nuevo)
static char teamName[28] = "";
static char teamPick[2][48] = {"", ""};         // ids de los dos jugadores
static lv_obj_t* teamNameLbl = nullptr;
static lv_obj_t* teamKeys[40];
static bool teamCaps = true;

static void fillTeams(char* b, size_t n, int) { snprintf(b, n, "%u equipos", (unsigned)equipos::count); }

// Insignia del equipo: su color con las iniciales del nombre.
static void badge(lv_obj_t* parent, const equipos::Club& c, int x, int y, int size, uint16_t font) {
  lv_obj_t* b = ui::box(parent, x, y, size, size);
  lv_obj_set_style_radius(b, size / 4, 0);
  lv_obj_set_style_bg_color(b, ui::hex(equipos::colorOf(c)), 0);
  lv_obj_set_style_bg_opa(b, 60, 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_border_color(b, ui::hex(equipos::colorOf(c)), 0);
  char ini[8];
  jug::initials(c.name, ini, sizeof(ini));
  lv_obj_t* l = ui::text(b, ini, 900, font, equipos::colorOf(c));
  lv_obj_center(l);
}

static uint32_t codeOfId(const char* id) {
  const jug::Player* p = jugadores.findById(id);
  return p ? p->code : 0;
}
static const char* nameOfId(const char* id) {
  const jug::Player* p = jugadores.findById(id);
  return p ? p->name : "Jugador borrado";
}

static void onTeamNew(lv_event_t*) {
  if (equipos::count >= equipos::MAX) return ui::toast("Ya hay 32 equipos");
  if (jugadores.count < 2) return ui::toast("Primero, al menos dos jugadores");
  teamIdx = -1;
  teamName[0] = 0;
  teamPick[0][0] = teamPick[1][0] = 0;
  go(View::TeamName);
}
static void onTeam(lv_event_t* e) {
  teamIdx = argOf(e);
  go(View::TeamDetail);
}

static void buildTeams() {
  header("EQUIPOS", onBackToMenu);
  helpBtn(scr, 230, 14, 44,
          "Un equipo fijo es una pareja con nombre. Cuando esos dos juegan juntos en el mismo lado, el marcador "
          "pone el nombre del equipo. Con cuenta, se comparten con la app.");
  lv_obj_t* list = scroller(76);
  fichas::btn(list, 16, 0, 768, 72, "+ NUEVO EQUIPO", 20, fichas::Kind::Primary, onTeamNew);
  if (!equipos::count) {
    lv_obj_t* l = ui::text(list, "Sin equipos todavía", 800, 20, ui::color::TEXT2);
    lv_obj_set_pos(l, 24, 110);
  }
  for (int i = 0; i < equipos::count; i++) {
    const equipos::Club& c = equipos::list[i];
    int y = 88 + i * 92;
    lv_obj_t* row = fichas::btn(list, 16, y, 768, 84, "", 20, fichas::Kind::Normal, onTeam, (void*)(intptr_t)i);
    lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    badge(row, c, 10, 10, 64, 20);
    lv_obj_t* n = ui::text(row, c.name, 900, 24, ui::color::TEXT);
    lv_obj_set_pos(n, 90, 12);
    lv_obj_set_width(n, 380);
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
    char who[64];
    snprintf(who, sizeof(who), "%s y %s", nameOfId(c.player[0]), nameOfId(c.player[1]));
    lv_obj_t* w = ui::text(row, who, 400, 16, ui::color::TEXT2);
    lv_obj_set_pos(w, 90, 50);
    lv_obj_set_width(w, 380);
    lv_label_set_long_mode(w, LV_LABEL_LONG_MODE_DOTS);
    for (int k = 0; k < 2; k++) faceOf(row, codeOfId(c.player[k]), 590 + k * 86, 6, 72, 26);
  }
}

// Un equipo: nombre, sus dos jugadores y CAMBIAR NOMBRE / CAMBIAR JUGADORES / BORRAR.
static void onTeamRename(lv_event_t*) {
  const equipos::Club& c = equipos::list[teamIdx];
  snprintf(teamName, sizeof(teamName), "%s", c.name);
  snprintf(teamPick[0], 48, "%s", c.player[0]);
  snprintf(teamPick[1], 48, "%s", c.player[1]);
  go(View::TeamName);
}
static void onTeamPlayers(lv_event_t*) {
  const equipos::Club& c = equipos::list[teamIdx];
  snprintf(teamName, sizeof(teamName), "%s", c.name);
  snprintf(teamPick[0], 48, "%s", c.player[0]);
  snprintf(teamPick[1], 48, "%s", c.player[1]);
  go(View::TeamPlayers);
}
static void deleteTeam() {
  if (equipos::removeAt(teamIdx)) ui::toast("Equipo borrado");
  else ui::toast("No se pudo borrar");
  teamIdx = -1;
  go(View::Teams);
}
static void onTeamDelete(lv_event_t*) {
  ask("¿Borrar este equipo? Los jugadores y sus partidos no se borran: solo deja de reconocerse esta pareja.",
      "BORRAR", deleteTeam);
}
static void onBackToTeams(lv_event_t*) { go(View::Teams); }

static void buildTeamDetail() {
  if (teamIdx < 0 || teamIdx >= equipos::count) return buildTeams();
  const equipos::Club& c = equipos::list[teamIdx];
  header("EQUIPO", onBackToTeams);
  badge(scr, c, 24, 84, 96, 30);
  lv_obj_t* n = ui::text(scr, c.name, 900, 34, ui::color::TEXT);
  lv_obj_set_pos(n, 140, 108);
  lv_obj_set_width(n, 636);
  lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
  for (int k = 0; k < 2; k++) {
    faceOf(scr, codeOfId(c.player[k]), 24 + k * 200, 204, 120, 40);
    lv_obj_t* l = ui::text(scr, nameOfId(c.player[k]), 800, 18, ui::color::TEXT);
    lv_obj_set_pos(l, 24 + k * 200, 334);
    lv_obj_set_width(l, 180);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  }
  fichas::btn(scr, 440, 196, 336, 72, "CAMBIAR NOMBRE", 20, fichas::Kind::Normal, onTeamRename);
  fichas::btn(scr, 440, 280, 336, 72, "CAMBIAR JUGADORES", 20, fichas::Kind::Normal, onTeamPlayers);
  fichas::btn(scr, 440, 364, 336, 72, "BORRAR EQUIPO", 20, fichas::Kind::Danger, onTeamDelete);
}

// Nombre: teclado con Ñ y tildes en las vocales (pulsando ´ antes).
static const char* const KEYS[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKLÑ", "ZXCVBNM-.'"};
static bool teamAccent = false;
static void showTeamName() {
  if (!teamNameLbl) return;
  char b[40];
  snprintf(b, sizeof(b), "%s|", teamName);
  lv_label_set_text(teamNameLbl, b);
}
static void relabelTeamKeys() {
  int k = 0;
  for (int r = 0; r < 4; r++) {
    const char* row = KEYS[r];
    for (const char* p = row; *p; k++) {
      int len = ((uint8_t)*p & 0xE0) == 0xC0 ? 2 : 1;
      char s[8];
      snprintf(s, sizeof(s), "%.*s", len, p);
      p += len;
      if (!teamCaps && r) {  // minúsculas (Ñ → ñ)
        if (len == 1 && s[0] >= 'A' && s[0] <= 'Z') s[0] = (char)(s[0] + 32);
        else if (!strcmp(s, "Ñ")) snprintf(s, sizeof(s), "ñ");
      }
      if (teamKeys[k]) lv_label_set_text(lv_obj_get_child(teamKeys[k], 0), s);
    }
  }
}
static void onTeamKey(lv_event_t* e) {
  lv_obj_t* b = lv_event_get_target_obj(e);
  const char* k = lv_label_get_text(lv_obj_get_child(b, 0));
  size_t n = strlen(teamName);
  static const char* const PLAIN[] = {"a", "e", "i", "o", "u", "A", "E", "I", "O", "U"};
  static const char* const ACUTE[] = {"á", "é", "í", "ó", "ú", "Á", "É", "Í", "Ó", "Ú"};
  if (teamAccent)
    for (int i = 0; i < 10; i++)
      if (!strcmp(k, PLAIN[i])) k = ACUTE[i];
  teamAccent = false;
  if (n + strlen(k) < sizeof(teamName) - 1) strcat(teamName, k);
  if (teamCaps && n == 0) {  // tras la primera, minúsculas (como al escribir un nombre)
    teamCaps = false;
    relabelTeamKeys();
  }
  showTeamName();
}
static void onTeamSpecial(lv_event_t* e) {
  int what = argOf(e);
  size_t n = strlen(teamName);
  if (what == 0) {  // BORRAR
    while (n && ((uint8_t)teamName[n - 1] & 0xC0) == 0x80) n--;
    if (n) n--;
    teamName[n] = 0;
  } else if (what == 1) {
    if (n && n < sizeof(teamName) - 2 && teamName[n - 1] != ' ') strcat(teamName, " ");
  } else if (what == 2) {
    teamCaps = !teamCaps;
    relabelTeamKeys();
  } else if (what == 3) {
    teamAccent = !teamAccent;
    ui::toast(teamAccent ? "Ahora la vocal con tilde" : "Sin tilde");
  }
  showTeamName();
}
static void saveTeam() {
  uint64_t now = (uint64_t)(rtc.clockOk ? partidos::epoch(rtc.now) : 0) * 1000u;
  int i = equipos::put(teamIdx >= 0 ? equipos::list[teamIdx].id : "", teamName, teamPick[0], teamPick[1], now);
  if (i < 0) return ui::toast("No se pudo guardar el equipo");
  teamIdx = i;
  ui::toast("Equipo guardado");
  go(View::TeamDetail);
}
static void onTeamNameNext(lv_event_t*) {
  while (teamName[0] == ' ') memmove(teamName, teamName + 1, strlen(teamName));
  size_t n = strlen(teamName);
  while (n && teamName[n - 1] == ' ') teamName[--n] = 0;
  if (!teamName[0]) return ui::toast("Escribe el nombre del equipo");
  if (equipos::nameTaken(teamName, teamIdx >= 0 ? equipos::list[teamIdx].id : nullptr)) return ui::toast("Ya hay un equipo con ese nombre");
  if (teamIdx >= 0) return saveTeam();  // cambiar el nombre: ya tiene sus jugadores
  go(View::TeamPlayers);
}
static void onTeamNameBack(lv_event_t*) { go(teamIdx >= 0 ? View::TeamDetail : View::Teams); }

static void buildTeamName() {
  lv_obj_t* t = ui::text(scr, teamIdx >= 0 ? "NOMBRE DEL EQUIPO" : "NUEVO EQUIPO", 900, 26, ui::color::ACCENT);
  lv_obj_set_pos(t, 24, 20);
  fichas::btn(scr, 436, 8, 166, 56, "VOLVER", 20, fichas::Kind::Normal, onTeamNameBack);
  fichas::btn(scr, ui::BACK.x, ui::BACK.y, ui::BACK.w, ui::BACK.h, teamIdx >= 0 ? "GUARDAR" : "SIGUIENTE", 20,
              fichas::Kind::Primary, onTeamNameNext);
  lv_obj_t* field = ui::box(scr, 24, 74, 752, 64);
  lv_obj_set_style_radius(field, ui::RADIUS_SM, 0);
  lv_obj_set_style_bg_color(field, ui::hex(ui::color::SURFACE), 0);
  lv_obj_set_style_bg_opa(field, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(field, 2, 0);
  lv_obj_set_style_border_color(field, ui::hex(ui::color::ACCENT), 0);
  teamNameLbl = ui::text(field, "", 900, 30, ui::color::TEXT);
  lv_obj_set_pos(teamNameLbl, 16, 14);
  teamCaps = !teamName[0];
  teamAccent = false;
  memset(teamKeys, 0, sizeof(teamKeys));
  int k = 0;
  for (int r = 0; r < 4; r++) {
    int x = 24;
    for (const char* p = KEYS[r]; *p; k++) {
      int len = ((uint8_t)*p & 0xE0) == 0xC0 ? 2 : 1;
      char s[8];
      snprintf(s, sizeof(s), "%.*s", len, p);
      p += len;
      teamKeys[k] = fichas::btn(scr, x, 150 + r * 66, 70, 60, s, 23, fichas::Kind::Normal, onTeamKey);
      x += 76;
    }
  }
  fichas::btn(scr, 24, 414, 146, 60, "MAYÚS", 18, fichas::Kind::Normal, onTeamSpecial, (void*)2);
  fichas::btn(scr, 176, 414, 90, 60, "´", 23, fichas::Kind::Normal, onTeamSpecial, (void*)3);
  fichas::btn(scr, 272, 414, 330, 60, "ESPACIO", 18, fichas::Kind::Normal, onTeamSpecial, (void*)1);
  fichas::btn(scr, 608, 414, 168, 60, "BORRAR", 18, fichas::Kind::Normal, onTeamSpecial, (void*)0);
  relabelTeamKeys();
  showTeamName();
}

// Los dos jugadores: tocar para elegir (el tercero sustituye al primero).
static bool picked(int i) { return !strcmp(teamPick[0], jugadores.list[i].id) || !strcmp(teamPick[1], jugadores.list[i].id); }
static void onPick(lv_event_t* e) {
  int i = argOf(e);
  const char* id = jugadores.list[i].id;
  if (!strcmp(teamPick[0], id)) {
    memmove(teamPick[0], teamPick[1], 48);
    teamPick[1][0] = 0;
  } else if (!strcmp(teamPick[1], id)) {
    teamPick[1][0] = 0;
  } else if (!teamPick[0][0]) {
    snprintf(teamPick[0], 48, "%s", id);
  } else {
    if (teamPick[1][0]) memmove(teamPick[0], teamPick[1], 48);
    snprintf(teamPick[1], 48, "%s", id);
  }
  changed();
}
static void onPickSave(lv_event_t*) {
  if (!teamPick[0][0] || !teamPick[1][0]) return ui::toast("Elige los dos jugadores");
  const equipos::Club* other = equipos::find(teamPick[0], teamPick[1]);
  if (other && (teamIdx < 0 || other != &equipos::list[teamIdx])) {
    char b[64];
    snprintf(b, sizeof(b), "Esa pareja ya es «%s»", other->name);
    return ui::toast(b);
  }
  saveTeam();
}
static void onPickBack(lv_event_t*) { go(teamIdx >= 0 ? View::TeamDetail : View::TeamName); }
static void fillPicked(char* b, size_t n, int) {
  int k = !!teamPick[0][0] + !!teamPick[1][0];
  snprintf(b, n, k == 2 ? "Listo: GUARDAR" : k == 1 ? "Elige 1 más" : "Elige 2 jugadores");
}

static void buildTeamPlayers() {
  lv_obj_t* t = ui::text(scr, teamName, 900, 26, ui::color::ACCENT);
  lv_obj_set_pos(t, 24, 20);
  lv_obj_set_width(t, 230);
  lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_DOTS);
  val(scr, 260, 26, 170, 800, 18, fillPicked);
  fichas::btn(scr, 436, 8, 166, 56, "VOLVER", 20, fichas::Kind::Normal, onPickBack);
  fichas::btn(scr, ui::BACK.x, ui::BACK.y, ui::BACK.w, ui::BACK.h, "GUARDAR", 20, fichas::Kind::Primary, onPickSave);
  lv_obj_t* list = scroller(76);
  const int CW = 180, CH = 160, GAP = 16;
  for (int i = 0; i < jugadores.count; i++) {
    int x = 16 + (i % 4) * (CW + GAP), y = 4 + (i / 4) * (CH + 12);
    lv_obj_t* c = opt(list, x, y, CW, CH, "", 18, picked, i, onPick);
    lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    faceOf(c, jugadores.list[i].code, (CW - 100) / 2, 10, 100, 34);
    lv_obj_t* l = lv_obj_get_child(c, 0);  // el texto del botón: su nombre, abajo
    lv_label_set_text(l, jugadores.list[i].name);
    lv_obj_set_width(l, CW - 12);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -12);
  }
}

// ---------------------------------------------------------------- DATOS
static void fillData(char* b, size_t n, int) {
  if (datos::demo) snprintf(b, n, "!Modo prueba");
  else snprintf(b, n, "%d partidos", partidos::count);
}
static void fillDemo(char* b, size_t n, int) {
  if (datos::demo) snprintf(b, n, "!Activo: los datos de verdad están guardados aparte");
  else snprintf(b, n, "Desactivado");
}
static void fillCounts(char* b, size_t n, int) { snprintf(b, n, "%u jugadores · %d partidos", (unsigned)jugadores.count, partidos::count); }

static bool dataBusy() {
  if (inMatch) return true;
  if (nube::busy()) {
    ui::toast("Espera a que termine la sincronización");
    return true;
  }
  return false;
}
static void afterData(bool ok, const char* okText) {
  workDone();
  matchFixedDirty = true;
  rankingFresh = true;
  if (ok) ui::toast(okText);
  else ui::toast(datos::lastError[0] ? datos::lastError : "No se pudo");
  go(View::Data);
}
static void demoOn() {
  datos::progress = workProgress;
  bool ok = datos::setDemo(true, rtc.clockOk ? partidos::epoch(rtc.now) : 0);
  datos::progress = nullptr;
  afterData(ok, "Modo prueba activado");
}
static void demoOff() {
  datos::progress = workProgress;
  workProgress("Volviendo a tus datos", 0, 1);
  bool ok = datos::setDemo(false, 0);
  datos::progress = nullptr;
  afterData(ok, "Modo prueba quitado: vuelven tus datos");
}
static void onDemo(lv_event_t*) {
  if (dataBusy()) return;
  if (datos::demo)
    ask("¿Quitar el modo prueba? Se borra todo lo de prueba, también lo que hayas jugado o añadido, y vuelven tus "
        "datos tal cual estaban.", "QUITAR", demoOff);
  else
    ask("¿Activar el modo prueba? Tus jugadores y partidos se guardan aparte y se crean 12 jugadores y 40 partidos de "
        "prueba. Mientras dura no se sincroniza con la cuenta.", "ACTIVAR", demoOn, false);
}

static int copyChosen = -1;
static void makeCopy() {
  datos::progress = workProgress;
  workProgress("Haciendo la copia", 0, 1);
  bool ok = datos::makeCopy(rtc.clockOk ? &rtc.now : nullptr);
  datos::progress = nullptr;
  afterData(ok, "Copia guardada en la tarjeta");
}
static void onCopy(lv_event_t*) {
  if (dataBusy()) return;
  if (datos::demo) return ui::toast("Quita antes el modo prueba");
  makeCopy();
}
static void restoreChosen() {
  datos::progress = workProgress;
  workProgress("Restaurando la copia", 0, 1);
  bool ok = datos::restoreCopy(copyChosen);
  datos::progress = nullptr;
  if (ok) {
    loadPrefs();
#if HAS_AUDIO
    player.setVolume(sndVolume);
#endif
    if (!dimmed) lcd.backlight(backlightLevel());
  }
  afterData(ok, "Copia restaurada");
}
static void onRestore(lv_event_t* e) {
  if (dataBusy()) return;
  if (datos::demo) return ui::toast("Quita antes el modo prueba");
  copyChosen = argOf(e);
  char label[32], b[200];
  datos::copyLabel(datos::copies[copyChosen], label, sizeof(label));
  snprintf(b, sizeof(b),
           "¿Restaurar la copia del %s? Los jugadores, partidos y ajustes de ahora se sustituyen por los de la copia.",
           label);
  ask(b, "RESTAURAR", restoreChosen);
}
static void deleteChosen() {
  bool ok = datos::deleteCopy(copyChosen);
  ui::toast(ok ? "Copia borrada" : "No se pudo borrar");
  go(View::Data);
}
static void onDeleteCopy(lv_event_t* e) {
  copyChosen = argOf(e);
  ask("¿Borrar esta copia de la tarjeta? No afecta a los datos de ahora.", "BORRAR", deleteChosen);
}

static void buildData() {
  header("DATOS", onBackToMenu);
  datos::scanCopies();
  lv_obj_t* list = scroller(76);
  lv_obj_t* l = ui::text(list, "MODO PRUEBA", 800, 18, ui::color::TEXT2, 0.8f);
  lv_obj_set_pos(l, 24, 6);
  helpBtn(list, 200, 0, 40,
          "Para probar la mesa sin tocar tus datos: 12 jugadores y 40 partidos de prueba. Lo que juegues se queda en "
          "la prueba. Al quitarlo se borra y vuelven tus jugadores y partidos tal cual.");
  val(list, 24, 50, 500, 400, 22, fillDemo, 0, ui::color::TEXT2, true);
  fichas::btn(list, 536, 34, 248, 72, datos::demo ? "QUITAR" : "ACTIVAR", 20,
              datos::demo ? fichas::Kind::Danger : fichas::Kind::Normal, onDemo);
  l = ui::text(list, "COPIA DE SEGURIDAD", 800, 18, ui::color::TEXT2, 0.8f);
  lv_obj_set_pos(l, 24, 132);
  helpBtn(list, 290, 126, 40,
          "Guarda en la tarjeta SD los jugadores (con sus fotos y equipos), los partidos y los ajustes de la mesa. "
          "Restaurar vuelve a dejarlo todo como estaba en esa copia.");
  val(list, 24, 176, 500, 400, 22, fillCounts, 0, ui::color::TEXT2, true);
  fichas::btn(list, 536, 160, 248, 72, "HACER COPIA", 20, fichas::Kind::Primary, onCopy);
  char root[96];
  if (!datos::backupRoot(root, sizeof(root))) {
    lv_obj_t* w = ui::text(list, "Las copias necesitan la tarjeta SD", 400, 22, ui::color::RANKED);
    lv_obj_set_pos(w, 24, 256);
    return;
  }
  if (!datos::copyCount) {
    lv_obj_t* w = ui::text(list, "Sin copias en la tarjeta", 400, 22, ui::color::TEXT3);
    lv_obj_set_pos(w, 24, 256);
  }
  for (int i = 0; i < datos::copyCount; i++) {
    int y = 250 + i * 88;
    lv_obj_t* row = ui::box(list, 16, y, 768, 80);
    lv_obj_set_style_radius(row, ui::RADIUS_SM, 0);
    lv_obj_set_style_bg_color(row, ui::hex(ui::color::CARD), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    char label[32], info[48];
    datos::copyLabel(datos::copies[i], label, sizeof(label));
    lv_obj_t* a = ui::text(row, label, 900, 24, ui::color::TEXT);
    lv_obj_set_pos(a, 16, 8);
    snprintf(info, sizeof(info), "%u jugadores · %u partidos", datos::copies[i].players, datos::copies[i].matches);
    lv_obj_t* b = ui::text(row, info, 400, 16, ui::color::TEXT2);
    lv_obj_set_pos(b, 16, 46);
    fichas::btn(row, 400, 8, 210, 64, "RESTAURAR", 18, fichas::Kind::Normal, onRestore, (void*)(intptr_t)i);
    fichas::btn(row, 618, 8, 142, 64, "BORRAR", 18, fichas::Kind::Danger, onDeleteCopy, (void*)(intptr_t)i);
  }
}

// ---------------------------------------------------------------- QR del perfil
static uint32_t qrCode = 0;
static perfil::Creds qrCreds;
static bool qrHave = false;
static int qrState = 0;  // 0 sin publicar, 1 publicando, 2 publicado, 3 no se pudo, 4 sin Wi-Fi, 5 modo prueba
static bool qrToProfile = false;  // abierto desde OPCIONES de la ficha: volver lleva a ella

static void startPublish() {
  if (datos::demo) { qrState = 5; return; }
  if (!wifiSsid[0]) { qrState = 4; return; }
  if (inMatch || versionsRunning || s3net::scanning || checkState == CheckState::Running || nube::busy()) {
    qrState = 3;
    return;
  }
  uint32_t codes[jug::MAX];
  for (int i = 0; i < jugadores.count; i++) codes[i] = jugadores.list[i].code;
  progresion::refresh(codes, jugadores.count);
  char* body = perfil::body(qrCreds, rtc.clockOk ? partidos::epoch(rtc.now) : 0);
  qrState = body && nube::publishProfile(body) ? 1 : 3;
}
static void fillQr(char* b, size_t n, int) {
  if (qrState == 1 && nube::profileResult) qrState = nube::profileResult > 0 ? 2 : 3;
  switch (qrState) {
    case 1: snprintf(b, n, "Publicando el perfil..."); break;
    case 2: snprintf(b, n, "Listo: léelo con la cámara del móvil"); break;
    case 3: snprintf(b, n, "!No se pudo publicar: el móvil verá lo último"); break;
    case 4: snprintf(b, n, "!Sin Wi-Fi: el móvil verá lo último publicado"); break;
    case 5: snprintf(b, n, "!En modo prueba no se publica"); break;
    default: snprintf(b, n, " "); break;
  }
}
static void onQrRetry(lv_event_t*) {
  if (qrState == 1) return;
  startPublish();
  changed();
}
static void onQrBack(lv_event_t*) {
  if (qrToProfile) {
    qrToProfile = false;
    view = View::Menu;
    screen = Screen::Profile;
    profileFresh = false;
    return;
  }
  screen = Screen::Players;
  fichasFresh = false;
}

static void buildQr() {
  const jug::Player* p = jugadores.find(qrCode);
  if (!p || !qrHave) return buildMenu();
  header(p->name, onQrBack);
  lv_obj_t* white = ui::box(scr, 32, 80, 380, 380);
  lv_obj_set_style_bg_color(white, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(white, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(white, ui::RADIUS, 0);
  char u[120];
  perfil::url(qrCreds, u, sizeof(u));
  lv_obj_t* qr = lv_qrcode_create(white);
  lv_qrcode_set_size(qr, 340);
  lv_qrcode_set_dark_color(qr, ui::hex(0x000000));
  lv_qrcode_set_light_color(qr, ui::hex(0xffffff));
  lv_qrcode_update(qr, u, strlen(u));
  lv_obj_center(qr);
  faceOf(scr, qrCode, 440, 84, 120, 40);
  lv_obj_t* t = ui::text(scr, "Su perfil en el móvil", 900, 24, ui::color::TEXT);
  lv_obj_set_pos(t, 440, 220);
  helpBtn(scr, 730, 210, 44,
          "Apunta con la cámara del móvil al código: se abre su perfil (partidos, rachas, rivales, nivel y logros). "
          "Solo para verlo; desde el móvil no se cambia nada.");
  val(scr, 440, 268, 340, 400, 22, fillQr, 0, ui::color::TEXT2, true);
  lv_label_set_long_mode(vals[valCount - 1].l, LV_LABEL_LONG_MODE_WRAP);
  fichas::btn(scr, 440, 388, 336, 72, "PUBLICAR OTRA VEZ", 20, fichas::Kind::Normal, onQrRetry);
}

// Desde OPCIONES de la ficha → VER QR (lo pinta render(): no se abre otra pantalla dentro del toque).
static bool prepareQr(uint32_t code) {
  qrCode = code;
  qrHave = perfil::credsFor(code, qrCreds);
  if (!qrHave) return false;
  qrState = 0;
  qrToProfile = true;
  view = View::Qr;
  startPublish();
  return true;
}

// Desde JUGADORES → un jugador → QR DEL PERFIL.
static void openQr(uint32_t code) {
  qrToProfile = false;
  qrCode = code;
  qrHave = perfil::credsFor(code, qrCreds);
  qrState = 0;
  screen = Screen::Settings;
  view = View::Qr;
  if (qrHave) startPublish();
  go(View::Qr);
}
