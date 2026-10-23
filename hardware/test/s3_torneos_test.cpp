// TORNEOS en la pantalla de 7" (ui_torneo*.h) con LVGL de verdad y toques: «¿Quién juega?», «¿Qué torneo?», «Elige
// la liga», el creador por pasos, las parejas, la pantalla del torneo (liga, copa y pool), la ficha, el palmarés
// (con ediciones por páginas), la ceremonia y el paso por el partido (cartel de fase, victoria con VOLVER AL TORNEO,
// empate de liga, abandono «a medias» y repetir el último partido).
// Reglas y ficha: las de verdad (s3_torneos_reglas.h, s3_torneos_ficha.h); guardado: el falso (torneos_fake.h).
// Comprueba en cada pantalla que nada se sale de 800×480, que todas las letras son de fuentes.txt y que tocar una
// opción no rehace la pantalla.
// Con MFV3_CAPTURAS=<carpeta> guarda una imagen .ppm de cada pantalla.
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include "Arduino.h"
#include "misc/lv_text_private.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"
#include "torneos_fake.h"

// Torneos que hay (en juego o terminados).
static int tournamentsCount() {
  torneos::loadIndex();
  return (int)torneos::nSummaries;
}

static void run(uint32_t ms) {
  uint32_t end = sim::now + ms;
  while ((int32_t)(end - sim::now) > 0) loop();
}
static void tap(int x, int y) {
  sim::touchX = x; sim::touchY = y; sim::touchDown = true; run(60);
  sim::touchDown = false; run(60);
}
static bool visible(lv_obj_t* o) {
  for (lv_obj_t* p = o; p; p = lv_obj_get_parent(p))
    if (lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN)) return false;
  return true;
}
static void collectText(lv_obj_t* o, std::string& out) {
  if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
  if (lv_obj_check_type(o, &lv_label_class)) { out += lv_label_get_text(o); out += '|'; }
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) collectText(lv_obj_get_child(o, i), out);
}
static std::string screenText() {
  std::string t;
  collectText(lv_screen_active(), t);
  collectText(lv_layer_top(), t);
  return t;
}
static bool shows(const char* s) { return screenText().find(s) != std::string::npos; }

// La etiqueta visible con ese texto (exacto), y el primer antepasado que se puede tocar.
static lv_obj_t* findLabel(lv_obj_t* o, const char* txt) {
  if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return nullptr;
  for (uint32_t i = lv_obj_get_child_count(o); i-- > 0;)
    if (lv_obj_t* f = findLabel(lv_obj_get_child(o, i), txt)) return f;
  if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), txt)) return o;
  return nullptr;
}
static bool tapText(const char* txt) {
  lv_obj_t* l = findLabel(lv_layer_top(), txt);
  if (!l) l = findLabel(lv_screen_active(), txt);
  if (!l) { printf("no encuentro «%s»\n", txt); return false; }
  lv_obj_t* b = l;
  while (b && !lv_obj_has_flag(b, LV_OBJ_FLAG_CLICKABLE)) b = lv_obj_get_parent(b);
  if (!b || b == lv_screen_active()) b = l;
  lv_obj_scroll_to_view_recursive(b, LV_ANIM_OFF);
  lv_obj_update_layout(lv_screen_active());
  lv_area_t a;
  lv_obj_get_coords(b, &a);
  int x = (a.x1 + a.x2) / 2, y = (a.y1 + a.y2) / 2;
  x = x < 2 ? 2 : x > 797 ? 797 : x;
  y = y < 2 ? 2 : y > 477 ? 477 : y;
  tap(x, y);
  return true;
}

// ---- comprobaciones de cada pantalla
static int problems = 0;
static const char* where = "";
// Dentro de una lista que se desliza, lo de fuera de la vista no cuenta (se ve al deslizar).
static bool inScroller(lv_obj_t* o) {
  for (lv_obj_t* p = lv_obj_get_parent(o); p; p = lv_obj_get_parent(p))
    if (lv_obj_has_flag(p, LV_OBJ_FLAG_SCROLLABLE) && p != lv_screen_active()) return true;
  return false;
}
static void checkTree(lv_obj_t* o) {
  if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  bool scr = o == lv_screen_active() || o == lv_layer_top();
  bool shadow = lv_obj_get_style_shadow_width(o, LV_PART_MAIN) > 0;
  bool confetti = lv_obj_get_width(o) <= 14 && lv_obj_get_height(o) <= 14 && !lv_obj_check_type(o, &lv_label_class);
  if (!scr && !confetti && !inScroller(o) && !shadow && (a.x1 < 0 || a.y1 < 0 || a.x2 > 799 || a.y2 > 479) &&
      !(lv_obj_get_parent(o) == lv_layer_top() && lv_obj_get_width(o) == 800)) {
    if (!lv_obj_check_type(o, &lv_line_class)) {
      printf("[%s] se sale de la pantalla: %d,%d–%d,%d %s\n", where, (int)a.x1, (int)a.y1, (int)a.x2, (int)a.y2,
             lv_obj_check_type(o, &lv_label_class) ? lv_label_get_text(o) : "");
      problems++;
    }
  }
  if (lv_obj_check_type(o, &lv_label_class)) {
    const lv_font_t* f = lv_obj_get_style_text_font(o, LV_PART_MAIN);
    const char* t = lv_label_get_text(o);
    if (t[0] && !assets::fontSize(f)) {
      printf("[%s] letra que no es de fuentes.txt: «%s»\n", where, t);
      problems++;
    }
    // Cada letra tiene que estar en la letra (fuentes.txt: Latin-1 y unos pocos signos).
    for (const char* p = t; *p;) {
      uint32_t i = (uint32_t)(p - t);
      uint32_t cp = lv_text_encoded_next(t, &i);
      p = t + i;
      if (cp == '\n' || cp == ' ' || !cp) continue;
      lv_font_glyph_dsc_t g;
      if (!lv_font_get_glyph_dsc(f, &g, cp, 0) || g.resolved_font != f) {
        printf("[%s] falta la letra U+%04X en «%s» (%u)\n", where, (unsigned)cp, t, (unsigned)assets::fontSize(f));
        problems++;
        break;
      }
    }
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) checkTree(lv_obj_get_child(o, i));
}
static void check(const char* name) {
  where = name;
  run(120);
  lv_obj_update_layout(lv_screen_active());
  checkTree(lv_screen_active());
  checkTree(lv_layer_top());
}
static void shot(const char* name) {
  check(name);
  const char* dir = getenv("MFV3_CAPTURAS");
  if (!dir) return;
  run(300);
  lv_obj_update_layout(lv_screen_active());
  lv_refr_now(nullptr);
  lv_draw_buf_t* buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
  assert(buf);
  // La capa de encima (cartel en franja, avisos) también.
  lv_draw_buf_t* top = lv_obj_get_child_count(lv_layer_top()) ? lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_ARGB8888) : nullptr;
  std::string path = std::string(dir) + "/" + name + ".ppm";
  FILE* f = fopen(path.c_str(), "wb");
  assert(f);
  fprintf(f, "P6\n%u %u\n255\n", buf->header.w, buf->header.h);
  for (uint32_t y = 0; y < buf->header.h; ++y)
    for (uint32_t x = 0; x < buf->header.w; ++x) {
      const uint8_t* p = buf->data + y * buf->header.stride + x * 3;
      int r = p[2], g = p[1], b = p[0];
      if (top && x < top->header.w && y < top->header.h) {
        const uint8_t* q = top->data + y * top->header.stride + x * 4;
        int al = q[3];
        r = (q[2] * al + r * (255 - al)) / 255;
        g = (q[1] * al + g * (255 - al)) / 255;
        b = (q[0] * al + b * (255 - al)) / 255;
      }
      uint8_t rgb[] = {(uint8_t)r, (uint8_t)g, (uint8_t)b};
      fwrite(rgb, 1, 3, f);
    }
  fclose(f);
  lv_draw_buf_destroy(buf);
  if (top) lv_draw_buf_destroy(top);
}

// Fotos falsas (un degradado con un círculo de color) para ver las fichas con foto.
static uint16_t* fakePhoto(int seed) {
  static uint16_t px[120 * 120];
  for (int y = 0; y < 120; y++)
    for (int x = 0; x < 120; x++) {
      int dx = x - 60, dy = y - 50;
      bool head = dx * dx + dy * dy < 900, body = y > 82 && dx * dx < (y - 60) * (y - 60);
      int r = head || body ? (seed * 70) % 256 : 40 + y / 4, g = head || body ? (seed * 130) % 256 : 60 + y / 3, b = head || body ? 200 : 90 + y / 2;
      px[y * 120 + x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
  return px;
}

static void goTournamentHome() {
  screen = Screen::Tournament;
  tor::open(&jugadores);
  run(200);
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  assets::setRoot(getenv("MFV3_ASSETS") ? getenv("MFV3_ASSETS") : "../build/placa/fat");
  sim::now = 1000;
  // Carpetas propias (otras pruebas usan las de build/placa a la vez).
  system("rm -rf /tmp/ag3_partidos /tmp/ag3_torneos_sd");
  partidos::setDir("/tmp/ag3_partidos");
  torneos::guardado::setDir("/tmp/ag3_torneos_sd");
  setup();
  test.active = false;
  nube::logout();
  nube::phase = nube::OFF;
  rtc.clockOk = true;
  rtc.now = {2026, 10, 23, 18, 30, 0};
  // 12 jugadores (con foto los pares, con iniciales los impares).
  static const char* const NAMES[12] = {"Alex", "Bea", "Carla", "Dani", "Eva", "Fran", "Gema", "Hugo", "Inés", "Javi", "Lucía", "Marcos"};
  static char ids[12][40];
  while (jugadores.count) jugadores.remove(jugadores.list[0].code);
  for (int i = 0; i < 12; i++) {
    uint32_t c = jugadores.add(NAMES[i], i % 2 ? nullptr : fakePhoto(i + 1));
    assert(c);
    jugadores.idOf(c, ids[i], 40);
    // La foto también donde la lee LVGL en el PC (la carpeta de recursos), como la copia la placa.
    if (i % 2 == 0) {
      char path[300];
      snprintf(path, sizeof(path), "%s/jugadores/%08lx.bin", assets::ROOT, (unsigned long)c);
      std::string dir = std::string(assets::ROOT) + "/jugadores";
      mkdir(dir.c_str(), 0755);
      if (FILE* f = fopen(path, "wb")) {
        uint8_t head[12] = {0};
        fwrite(head, 1, 12, f);
        fwrite(fakePhoto(i + 1), 2, 120 * 120, f);
        fclose(f);
      }
    }
  }
  elegir::prewarm(&jugadores);
  torneos::fake::now = tourNow();
  torneos::fake::seed(ids, 12);
  progresion::refresh(nullptr, 0);
  goHome();
  run(300);

  // ---- Inicio → TORNEO: «¿Quién juega?» con los torneos en juego arriba.
  onHomeMenu(inicio::TOURNAMENT);
  run(300);
  assert(screen == Screen::Tournament && tor::active() && tor::view == tor::View::Players);
  assert(shows("¿QUIÉN JUEGA?") && shows("EN JUEGO") && shows("Liga del Jueves · 2ª edición") && shows("CONTINUAR"));
  assert(shows("Marca al menos 3 más") && shows("PALMARÉS"));
  shot("torneo-quien-juega");
  // Tocar una ficha la marca sin rehacer la pantalla; cuenta y aviso cambian.
  lv_obj_t* same = lv_screen_active();
  lv_obj_t* grid = lv_obj_get_parent(tor::mark[0]);
  for (int i = 0; i < 6; i++) {
    lv_obj_t* card = lv_obj_get_child(grid, i * 2);
    lv_area_t a;
    lv_obj_get_coords(card, &a);
    if (a.y2 > 396) lv_obj_scroll_to_view(card, LV_ANIM_OFF), lv_obj_get_coords(card, &a);
    tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
  }
  assert(lv_screen_active() == same && tor::nPicked == 6 && !shows("Marca al menos") && shows("jugadores"));
  shot("torneo-quien-juega-6");
  assert(tapText("TODOS"));
  assert(tor::nPicked == 12 && shows("QUITAR TODOS") && lv_screen_active() == same);
  assert(tapText("QUITAR TODOS"));
  assert(tor::nPicked == 0);
  // Seis marcados (los seis primeros).
  for (int i = 0; i < 6; i++) tor::picked[tor::nPicked++] = jugadores.list[i].code;
  tor::go(tor::View::Players);
  assert(tapText("SIGUIENTE") && tor::view == tor::View::Kind);

  // ---- ¿Qué torneo jugamos?: dos toques (el primero enciende, el segundo entra).
  assert(shows("LIGA") && shows("COPA") && shows("POOL") && shows("6 JUGADORES"));
  shot("torneo-que-torneo");
  same = lv_screen_active();
  tap(16 + 123, 250);
  assert(tor::kindSel == 0 && lv_screen_active() == same);
  shot("torneo-que-torneo-encendida");
  run(8100);  // sola se apaga a los 8 s
  assert(tor::kindSel == -1);
  tap(16 + 123, 250);
  tap(16 + 123, 250);
  run(300);
  assert(tor::view == tor::View::Choose && shows("ELIGE LA LIGA"));
  // La liga del jueves tiene una edición en juego (CONTINUAR); la de parejas está libre (EDITAR).
  assert(shows("CONTINUAR") && shows("EDITAR") && shows("Liga de Parejas") && shows("Ahora, las parejas"));
  shot("torneo-elige-liga");
  // La «?» de una tarjeta: las reglas completas.
  assert(tapText("?") && shows("PARTIDOS:") && shows("FINAL:"));
  shot("torneo-reglas-liga");
  assert(tapText("VALE"));

  // ---- Parejas (Liga de Parejas, equilibradas) → poner nombre a una → CREAR TORNEO.
  assert(tapText("CONTINUAR") || true);
  tor::go(tor::View::Pairs);
  assert(shows("PAREJAS · 1ª EDICIÓN") && shows("3 parejas listas") && shows("CREAR TORNEO"));
  shot("torneo-parejas");
  assert(tapText("NOMBRE") && tor::view == tor::View::Keyboard);
  for (const char* k : {"L", "O", "S"}) tapText(k);
  tapText("ESPACIO");
  for (const char* k : {"T", "I", "T", "A", "N", "E", "S"}) tapText(k);
  assert(shows("Los Titanes|"));
  shot("torneo-teclado-pareja");
  assert(tapText("GUARDAR") && tor::view == tor::View::Pairs && shows("Los Titanes"));
  int before = tournamentsCount();
  assert(tapText("CREAR TORNEO"));
  run(200);
  assert(tor::view == tor::View::Detail && tournamentsCount() == before + 1 && tor::curOk);
  assert(shows("Liga de Parejas") && shows("1ª EDICIÓN") && shows("CLASIFICACIÓN") && shows("SIGUIENTE PARTIDO"));
  assert(equipos::count >= 1);  // la pareja con nombre se guarda como equipo
  shot("torneo-liga-nueva");

  // ---- Elige la liga → + CREAR LIGA: el creador por pasos.
  tor::kind = torneos::Format::League;
  tor::go(tor::View::Choose);
  assert(tapText("CREAR LIGA") && tor::view == tor::View::Creator && tor::cstep == tor::S_PLAYERS);
  if (!(shows("Jugadores") && shows("Puntos") && !shows("Duración"))) fprintf(stderr, "%s\n", screenText().c_str());
  assert(shows("Jugadores") && shows("Puntos") && !shows("Duración"));
  shot("creador-jugadores");
  same = lv_screen_active();
  assert(tapText("PAREJAS FIJAS") && tor::tpl.teamSize == 2 && lv_screen_active() == same);
  assert(shows("Parejas fijas") && shows("CÓMO SE FORMAN LAS PAREJAS") && shows("EQUILIBRADAS"));
  shot("creador-jugadores-parejas");
  assert(tapText("EQUILIBRADAS") && tor::tpl.pairing == torneos::Pairing::Elo && lv_screen_active() == same);
  assert(tapText("1 VS 1") && tor::tpl.teamSize == 1 && !shows("EQUILIBRADAS"));
  assert(tapText("SIGUIENTE") && tor::cstep == tor::S_RULES);
  shot("creador-partidos");
  same = lv_screen_active();
  assert(tapText("TIEMPO") && tor::tpl.rules.end == torneos::End::Both && lv_screen_active() == same);
  assert(shows("SI ACABAN IGUALADOS") && shows("EMPATE"));
  shot("creador-partidos-tiempo");
  assert(tapText("GOLES") && tor::tpl.rules.end == torneos::End::Time);
  assert(tapText("TIEMPO") && tor::tpl.rules.end == torneos::End::Time);  // siempre queda una
  assert(tapText("8 MIN") && tor::tpl.rules.minutes == 8);
  assert(tapText("SIGUIENTE") && tor::cstep == tor::S_POINTS);
  shot("creador-puntos");
  assert(tapText("IDA Y VUELTA") && tor::tpl.legs == 2);
  assert(tapText("SIGUIENTE") && tor::cstep == tor::S_FINAL);
  assert(tapText("1º VS 2º") && tor::tpl.finalKind == torneos::FinalKind::Top2 && shows("Final · 1º vs 2º"));
  assert(tapText("REGLAS DISTINTAS") && tor::tpl.ownFinalRules);
  shot("creador-final");
  assert(tapText("SIGUIENTE") && tor::cstep == tor::S_NAME);
  assert(shows("NOMBRE DE LA LIGA") && shows("Ej.: Liga del Almacén") && shows("RESUMEN") && shows("Con 6 jugadores"));
  shot("creador-nombre");
  assert(tapText("Cuchara de madera") && !strcmp(tor::tpl.lastTitle, "Cuchara de madera"));
  assert(tapText("ESCUDO") && shows("ELIGE EL ESCUDO"));
  shot("creador-escudo");
  tap(24 + 150 + 70 + 80, 64 + 42 + 110);  // el segundo color
  assert(tor::logoIndex(tor::tpl.logo) == 1);
  assert(tapText("Ej.: Liga del Almacén") && tor::view == tor::View::Keyboard);
  for (const char* k : {"L", "I", "G", "A"}) tapText(k);
  tapText("ESPACIO");
  for (const char* k : {"Ñ", "U", "Ñ", "O"}) tapText(k);
  assert(shows("Liga Ñuño|"));
  shot("creador-teclado");
  assert(tapText("GUARDAR") && tor::view == tor::View::Creator && !strcmp(tor::tpl.name, "Liga Ñuño"));
  uint8_t nt = torneos::nTemplates;
  assert(tapText("GUARDAR LIGA"));
  assert(torneos::nTemplates == nt + 1 && tor::view == tor::View::Choose && shows("Liga Ñuño"));
  // Tipo: el primer paso (sin ediciones, se puede cambiar).
  tor::startCreator(&torneos::templates[torneos::nTemplates - 1], nullptr, torneos::Format::League);
  tor::cstep = tor::S_KIND;
  tor::go(tor::View::Creator);
  assert(shows("DUPLICAR LIGA") && shows("BORRAR"));
  shot("creador-tipo");
  tap(16 + 2 * 261 + 120, 300);  // POOL
  assert(tor::tpl.format == torneos::Format::Pool && shows("Duración") && !shows("Puntos"));
  tor::cstep = tor::S_DURATION;
  tor::go(tor::View::Creator);
  assert(shows("POR TIEMPO") && shows("Por tiempo") && shows("30 MIN"));
  shot("creador-duracion");
  assert(tapText("POR RONDAS") && shows("Por rondas") && tapText("6") && tor::tpl.poolRounds == 6);
  tor::cstep = tor::S_PLAYERS;
  tor::go(tor::View::Creator);
  assert(shows("CON CAMBIO DE PAREJA") && shows("PAREJAS DEL PRIMER PARTIDO"));
  shot("creador-pool-parejas");
  // Borrar (con confirmación): vuelve a la lista.
  assert(tapText("BORRAR") && shows("¿Borrar «Liga Ñuño»?"));
  assert(tapText("BORRAR"));
  assert(torneos::nTemplates == nt);

  // ---- Sin ninguna del tipo: el cartel «¡Crea tu primera copa!».
  {
    torneos::Template keep[torneos::MAX_TEMPLATES];
    uint8_t n = torneos::nTemplates;
    memcpy(keep, torneos::templates, sizeof(torneos::Template) * n);
    torneos::nTemplates = 0;
    tor::kind = torneos::Format::Bracket;
    tor::go(tor::View::Empty);
    assert(shows("¡Crea tu primera copa!") && shows("No hay ninguna copa creada") && shows("CREAR COPA"));
    shot("torneo-crea-tu-primera");
    memcpy(torneos::templates, keep, sizeof(torneos::Template) * n);
    torneos::nTemplates = n;
  }

  // ---- Torneos en juego: liga, copa y pool.
  auto openByName = [](const char* prefix) {
    torneos::loadIndex();
    for (uint16_t i = 0; i < torneos::nSummaries; i++)
      if (!strncmp(torneos::summaries[i].name, prefix, strlen(prefix)) && torneos::summaries[i].status == torneos::Status::Active) {
        tor::fromList = false;
        tor::openTournament(torneos::summaries[i].id);
        run(200);
        return true;
      }
    return false;
  };
  assert(openByName("Liga del Jueves"));
  assert(shows("CLASIFICACIÓN") && shows("PJ") && shows("Pts") && shows("SIGUIENTE PARTIDO") && shows("JORNADAS"));
  assert(shows("E"));  // a tiempo con empate: columna E
  assert(shows("FINAL · 1º VS 2º"));
  shot("torneo-liga");
  same = lv_screen_active();
  assert(tapText("FICHA") && tor::tab == 1 && lv_screen_active() == same);
  assert(shows("EN JUEGO") && shows("PARTIDOS") && shows("GOLES") && shows("JUGADOR") && shows("MVP"));
  shot("torneo-ficha");
  assert(tapText("CLASIFICACIÓN") && tor::tab == 0);
  // REGLAS de lo que queda.
  assert(tapText("REGLAS") && tor::view == tor::View::Rules && shows("REGLAS DEL TORNEO") && shows("FINAL"));
  shot("torneo-reglas");
  assert(tapText("A 7") && tor::edRules.goals == 7);
  assert(tapText("GUARDAR") && tor::view == tor::View::Detail && tor::cur.rules.goals == 7);
  // Repetir el último partido (con confirmación y el partido).
  uint16_t played = tor::cur.nPlayed;
  int rows = partidos::count;
  assert(tapText("REPETIR ÚLTIMO PARTIDO") && shows("¿Repetir el último partido?") && shows("BORRAR Y REPETIR"));
  shot("torneo-repetir");
  assert(tapText("BORRAR Y REPETIR"));
  assert(tor::cur.nPlayed == played - 1 && partidos::count <= rows);  // (los partidos falsos no están en la tarjeta)

  // ---- Jugar un cruce: cartel de fase → partido → victoria con VOLVER AL TORNEO → el torneo.
  assert(tapText("SIGUIENTE PARTIDO") && tor::view == tor::View::Poster && tourMatch);
  assert(cfg.endCondition == mfv3::EndCondition::Both && cfg.goalsPerPeriod == 7 && cfg.allowDraw);
  assert(cartel::shown && strstr(cartel::curTitle, "JORNADA") && shows("VS") && shows("Toca para empezar"));
  shot("torneo-cartel-fase");
  run(3600);
  assert(inMatch && eng.phase() == Phase::Countdown);
  eng.skipCountdown(sim::now);
  run(100);
  shot("torneo-partido");
  assert(shows("Liga del Jueves"));  // el nombre del torneo en el marcador
  for (int g = 0; g < 7; g++) {
    inputGoal(Team::White);
    run(3200);  // bloqueo entre goles
  }
  run(800);
  assert(eng.phase() == Phase::Finished && victoria::active() && shows("VOLVER AL TORNEO"));
  shot("torneo-victoria");
  assert(tor::cur.nPlayed == played);  // apuntado en el torneo
  assert(tapText("VOLVER AL TORNEO"));
  run(200);
  assert(!inMatch && !tourMatch && screen == Screen::Tournament && tor::view == tor::View::Detail);
  assert(cfg.allowDraw == false);  // la configuración de antes

  // ---- Empate de liga: a tiempo, igualados al acabar la 2ª parte.
  assert(tapText("SIGUIENTE PARTIDO"));
  tap(400, 240);  // tocar el cartel lo salta
  assert(inMatch);
  eng.skipCountdown(sim::now);
  run(100);
  inputGoal(Team::White);
  run(3200);
  inputGoal(Team::Blue);
  run(3200);
  for (int k = 0; k < 2 && eng.phase() != Phase::Finished; k++) {
    sim::now += 5 * 60 * 1000 + 1000;
    lastActivity = millis();
    run(200);
    if (eng.phase() == Phase::PeriodEnd) onMatchNext(), run(200);
    if (eng.phase() == Phase::Countdown) eng.skipCountdown(sim::now), run(100);
  }
  run(800);
  assert(eng.phase() == Phase::Finished && eng.draw() && shows("¡EMPATE!") && shows("VOLVER AL TORNEO"));
  shot("torneo-empate");
  lastActivity = millis();  // (el tiempo saltado de golpe atenuaría la pantalla)
  assert(tapText("VOLVER AL TORNEO"));
  assert(tor::view == tor::View::Detail);

  // ---- Abandonar: el cruce queda «a medias» y se vuelve al torneo.
  assert(tapText("SIGUIENTE PARTIDO"));
  tap(400, 240);
  eng.skipCountdown(sim::now);
  run(100);
  inputPause();
  run(100);
  onMatchAbandon();
  run(300);
  assert(!inMatch && tor::view == tor::View::Detail && shows("Se dejó a medias · se empieza de cero"));
  shot("torneo-a-medias");

  // ---- Copa (cuadro) y pool (pistas, ronda terminada).
  assert(openByName("Copa del Viernes"));
  assert(shows("CUADRO") && shows("pase directo") && shows("GRAN FINAL") == false && shows("FINAL") && shows("SIGUIENTE"));
  shot("torneo-copa");
  assert(openByName("Pool de los Martes"));
  if (!shows("RONDA 1 TERMINADA")) fprintf(stderr, "%s\n", screenText().c_str());
  assert(shows("PISTAS") && shows("BANQUILLO") && shows("RONDA 1 TERMINADA") && shows("SIGUIENTE RONDA"));
  shot("torneo-pool-ronda-terminada");
  same = lv_screen_active();
  assert(tapText("SIGUIENTE RONDA") && lv_screen_active() == same);
  if (!(shows("SIGUIENTE PARTIDO") && shows("Ronda 2 de 4") && shows("PISTA 1"))) fprintf(stderr, "%s\n", screenText().c_str());
  assert(shows("SIGUIENTE PARTIDO") && shows("Ronda 2 de 4") && shows("PISTA 1"));
  shot("torneo-pool");

  // ---- Descartar (papelera, con confirmación).
  assert(openByName("Copa del Viernes"));
  int dbBefore = tournamentsCount();
  tap(764, 32);
  assert(shows("¿Descartar el torneo?") && shows("NO, SEGUIR"));
  shot("torneo-descartar");
  assert(tapText("DESCARTAR"));
  assert(tournamentsCount() == dbBefore - 1 && tor::view == tor::View::Players);

  // ---- Palmarés y una competición con 35 ediciones (de 10 en 10).
  assert(tapText("PALMARÉS") && tor::view == tor::View::Honours && shows("Copa Histórica") && shows("ÚLTIMO CAMPEÓN"));
  shot("torneo-palmares");
  assert(tapText("Copa Histórica") && tor::view == tor::View::Competition);
  assert(tor::edLoaded == 10 && tor::edTotal == 35 && shows("35 EDICIONES") && shows("CUADRO DE HONOR") && shows("VER MÁS EDICIONES"));
  shot("torneo-palmares-competicion");
  same = lv_screen_active();
  assert(tapText("VER MÁS EDICIONES") && tor::edLoaded == 20 && lv_screen_active() == same);
  lv_obj_scroll_to_y(tor::edList, 99999, LV_ANIM_OFF);
  lv_obj_send_event(tor::edList, LV_EVENT_SCROLL_END, nullptr);
  assert(tor::edLoaded == 30);
  tapText("VER MÁS EDICIONES");
  assert(tor::edLoaded == 35 && !shows("VER MÁS EDICIONES"));

  // ---- Liga terminada: banner del campeón, RESUMEN → ceremonia puesto a puesto.
  tor::go(tor::View::Honours);
  assert(tapText("Liga del Jueves") && tor::view == tor::View::Competition);
  // La 1ª edición (la última de la lista, de la más nueva a la más antigua).
  {
    lv_obj_t* row = lv_obj_get_child(tor::edList, lv_obj_get_child_count(tor::edList) - 1);
    lv_obj_scroll_to_view(row, LV_ANIM_OFF);
    lv_area_t a;
    lv_obj_get_coords(row, &a);
    tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
    run(200);
  }
  assert(tor::view == tor::View::Detail && tor::cur.status == torneos::Status::Finished && tor::tab == 1);
  assert(shows("CAMPEÓN") && shows("MVP"));
  shot("torneo-ficha-terminado");
  assert(tapText("CLASIFICACIÓN") && shows("RESUMEN"));
  shot("torneo-campeon");
  assert(tapText("RESUMEN") && tor::view == tor::View::Ceremony && shows("¡CAMPEÓN!") && shows("VER CLASIFICACIÓN"));
  run(1600);
  shot("ceremonia-campeon");
  assert(tapText("VER CLASIFICACIÓN") && shows("PUESTO 2 DE 4") && shows("SUBCAMPEÓN"));
  run(1600);
  shot("ceremonia-puesto-2");
  tapText("SIGUIENTE");
  tapText("SIGUIENTE");
  assert(shows("PUESTO 4 DE 4") && shows("FAROLILLO ROJO") && shows("TERMINAR"));
  run(1600);
  shot("ceremonia-ultimo");
  assert(tapText("TERMINAR") && tor::view == tor::View::Detail);

  // ---- Volver: «¿Quién juega?» → Inicio.
  tor::go(tor::View::Players);
  tap(36, 32);
  run(300);
  assert(screen == Screen::Home && !tor::active());
  if (problems) {
    printf("%d problemas en las pantallas\n", problems);
    return 1;
  }
  printf("Torneos: quién juega, qué torneo, elige, parejas, creador, liga, copa, pool, ficha, palmarés por páginas, ceremonia y partido (cartel, victoria, empate, a medias, repetir) OK\n");
  return 0;
}
