// MARCADOR FUTBOLÍN V3 · pantalla de 7": AJUSTES (menú) y sus apartados PARTIDO, SONIDO, PANTALLA y MANDO.
// -------------------------------------------------------------------------------------------------------
// Lo mismo que Ajustes de la app (src/ui/screens/SettingsScreen.tsx y SoundsTab.tsx), con el estilo austero de la
// placa: botones grandes con su título, la opción elegida iluminada y una «?» donde hace falta explicarla.
// Wi-Fi, Actualizaciones, Jugadores, Cuenta y la prueba de la placa siguen en sus pantallas (s3_ajustes.h,
// ui_fichas.h, ui_cuenta.h).
//
// Rapidez: cada apartado es una pantalla LVGL que se crea una vez al entrar. Al tocar una opción solo cambia el
// color de sus botones y el texto de su valor (refresh()); nada de rehacer la pantalla entera.
// Se incluye A MITAD del sketch, después de s3_ajustes.h: usa cfg, prefs, el reproductor, el brillo y el mando.

#ifndef MFV3_S3_OPCIONES_H
#define MFV3_S3_OPCIONES_H

namespace opciones {

enum class View : uint8_t { Menu, Match, Sound, Display, Mando, Teams, TeamDetail, TeamName, TeamPlayers, Data, Qr };
static View view = View::Menu;
static lv_obj_t* scr = nullptr;
static lv_obj_t* help = nullptr;  // explicación de una «?» (encima de todo)

// ---------------------------------------------------------------- opciones y valores que se repasan
// Un botón de opción sabe si está elegido; un texto sabe escribir su valor. refresh() solo toca lo que cambia.
struct Opt {
  lv_obj_t* b;
  bool (*on)(int);
  int arg;
  int8_t shown;
};
struct Val {
  lv_obj_t* l;
  void (*fill)(char*, size_t, int);
  int arg;
  uint32_t color, shownColor;
};
static Opt opts[72];  // hasta 64 jugadores al elegir pareja
static Val vals[16];
static uint8_t optCount = 0, valCount = 0;
static uint32_t refreshAt = 0;

static const uint32_t ON_BG = 0x1aa3dc, ON_INK = 0x03121e, GOLD = ui::color::RANKED;

static void paintOpt(Opt& o) {
  bool v = o.on(o.arg);
  if (o.shown == (int8_t)v) return;
  o.shown = (int8_t)v;
  lv_obj_set_style_bg_color(o.b, ui::hex(v ? ON_BG : ui::color::CARD), 0);
  lv_obj_set_style_border_color(o.b, ui::hex(v ? ui::color::ACCENT : ui::color::LINE), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(o.b, 0), ui::hex(v ? ON_INK : ui::color::TEXT), 0);
}

static void paintVal(Val& v) {
  char b[64];
  v.fill(b, sizeof(b), v.arg);
  // «!» delante: el valor pide atención (en dorado). El «!» no se enseña.
  const char* t = b[0] == '!' ? b + 1 : b;
  uint32_t c = b[0] == '!' ? GOLD : v.color;
  if (strcmp(lv_label_get_text(v.l), t)) lv_label_set_text(v.l, t);
  if (v.shownColor != c) {
    v.shownColor = c;
    lv_obj_set_style_text_color(v.l, ui::hex(c), 0);
  }
}

static void refresh() {
  for (uint8_t i = 0; i < optCount; i++) paintOpt(opts[i]);
  for (uint8_t i = 0; i < valCount; i++) paintVal(vals[i]);
}

// ---------------------------------------------------------------- piezas
static void go(View v);
static void onBackToMenu(lv_event_t*) { go(View::Menu); }
static void onBackHome(lv_event_t*) { goHome(); }

// Arriba: el título y VOLVER (a la derecha, como en Wi-Fi y Actualizaciones).
static void header(const char* title, lv_event_cb_t back) {
  lv_obj_t* t = ui::text(scr, title, 900, 30, ui::color::ACCENT, 0.5f);
  lv_obj_set_pos(t, 24, 18);
  fichas::btn(scr, ui::BACK.x, ui::BACK.y, ui::BACK.w, ui::BACK.h, "VOLVER", 20, fichas::Kind::Normal, back);
}

static lv_obj_t* caption(const char* s, int x, int y) {
  lv_obj_t* l = ui::text(scr, s, 800, 18, ui::color::TEXT2, 0.8f);
  lv_obj_set_pos(l, x, y);
  return l;
}

static lv_obj_t* opt(lv_obj_t* parent, int x, int y, int w, int h, const char* txt, uint16_t size, bool (*on)(int), int arg,
                     lv_event_cb_t cb) {
  lv_obj_t* b = fichas::btn(parent, x, y, w, h, txt, size, fichas::Kind::Normal, cb, (void*)(intptr_t)arg);
  if (optCount < sizeof(opts) / sizeof(opts[0])) opts[optCount++] = {b, on, arg, -1};
  return b;
}

static lv_obj_t* val(lv_obj_t* parent, int x, int y, int w, uint16_t weight, uint16_t size, void (*fill)(char*, size_t, int),
                     int arg = 0, uint32_t color = ui::color::TEXT2, bool left = false) {
  lv_obj_t* l = ui::text(parent, "", weight, size, color);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, left ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_pos(l, x, y);
  if (valCount < sizeof(vals) / sizeof(vals[0])) vals[valCount++] = {l, fill, arg, color, color};
  return l;
}

// Lista que se desliza (solo en Ajustes, como en la app).
static lv_obj_t* scroller(int y) {
  lv_obj_t* g = ui::box(scr, 0, y, 800, 480 - y);
  lv_obj_add_flag(g, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(g, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(g, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_width(g, 4, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(g, ui::hex(ui::color::ACCENT), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(g, LV_OPA_COVER, LV_PART_SCROLLBAR);
  return g;
}

// «?»: abre la explicación; ENTENDIDO (o tocar fuera) la cierra.
static void onHelpClose(lv_event_t*) {
  if (help) lv_obj_delete(help);
  help = nullptr;
}
static void onHelp(lv_event_t* e) {
  const char* txt = (const char*)lv_event_get_user_data(e);
  onHelpClose(nullptr);
  help = ui::box(scr, 0, 0, 800, 480);
  lv_obj_set_style_bg_color(help, ui::hex(0x000000), 0);
  lv_obj_set_style_bg_opa(help, 170, 0);
  lv_obj_add_flag(help, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(help, onHelpClose, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* card = ui::box(help, 90, 90, 620, 300);
  lv_obj_set_style_bg_color(card, ui::hex(ui::color::CARD2), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, ui::RADIUS, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, ui::hex(ui::color::ACCENT), 0);
  lv_obj_t* l = ui::text(card, txt, 400, 22, ui::color::TEXT);
  lv_obj_set_width(l, 572);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_pos(l, 24, 22);
  fichas::btn(card, 180, 214, 260, 64, "ENTENDIDO", 20, fichas::Kind::Primary, onHelpClose);
}
static lv_obj_t* helpBtn(lv_obj_t* parent, int x, int y, int size, const char* txt) {
  lv_obj_t* b = fichas::btn(parent, x, y, size, size, "?", 20, fichas::Kind::Normal, onHelp, (void*)txt);
  lv_obj_set_style_radius(b, size / 2, 0);
  return b;
}

// [-] valor [+] en una franja de ancho w.
static void stepper(int x, int y, int w, int h, void (*fill)(char*, size_t, int), lv_event_cb_t cb, int arg) {
  // Los signos, con la letra grande de las cifras (los botones solo llegan a 23).
  for (int i = 0; i < 2; i++) {
    lv_obj_t* b = fichas::btn(scr, i ? x + w - 96 : x, y, 96, h, i ? "+" : "-", 23, fichas::Kind::Normal, cb,
                              (void*)(intptr_t)(arg * 2 + i));
    lv_obj_set_style_text_font(lv_obj_get_child(b, 0), assets::font(900, 34), 0);
  }
  val(scr, x + 96, y + h / 2 - 22, w - 192, 900, 34, fill, arg, ui::color::TEXT);
}

static int argOf(lv_event_t* e) { return (int)(intptr_t)lv_event_get_user_data(e); }
static void changed() {
  refresh();
  lastActivity = millis();
}

static void buildMenu();
#include "s3_opciones_datos.h"  // EQUIPOS, DATOS y el QR de un jugador

// ---------------------------------------------------------------- MENÚ
// Mismos sitios que ui::SETTINGS (las pruebas tocan ahí).
static void fillMatch(char* b, size_t n, int) {
  if (cfg.endCondition == mfv3::EndCondition::Goals) snprintf(b, n, "A %u goles", cfg.goalsPerPeriod);
  else if (cfg.endCondition == mfv3::EndCondition::Time) snprintf(b, n, "%u min por parte", cfg.minutesPerPeriod);
  else snprintf(b, n, "%u goles o %u min", cfg.goalsPerPeriod, cfg.minutesPerPeriod);
}
static void fillSound(char* b, size_t n, int) {
#if HAS_AUDIO
  if (sndVolume) snprintf(b, n, "Volumen %u %%", (unsigned)sndVolume);
  else snprintf(b, n, "!Sin sonido");
#else
  snprintf(b, n, "Sin altavoz");
#endif
}
static void fillDisplay(char* b, size_t n, int) {
  if (sleepMinutes) snprintf(b, n, "Brillo %u %% · reposo %u min", (unsigned)userBrightness, (unsigned)sleepMinutes);
  else snprintf(b, n, "Brillo %u %% · sin reposo", (unsigned)userBrightness);
}
static const char* modelName(uint8_t m) { return m == mfv3::ota::MODEL_C3_ZERO ? "C3-Zero" : m == mfv3::ota::MODEL_SUPER_MINI ? "Super Mini" : "?"; }
static void fillMando(char* b, size_t n, int) {
  MandoUpd u = mandoUpd();
  if (u == MandoUpd::Outdated) snprintf(b, n, "!Hay versión nueva");
  else if (u == MandoUpd::NeedsModel) snprintf(b, n, "!Elige su modelo");
  else if (remoteLinked()) snprintf(b, n, "Conectado%s%s", mandoModel() ? " · " : "", mandoModel() ? modelName(mandoModel()) : "");
  else snprintf(b, n, "Sin señal");
}
static void fillPlayers(char* b, size_t n, int) { snprintf(b, n, "%u jugadores", (unsigned)jugadores.count); }
static void fillAccount(char* b, size_t n, int) {
  if (nube::loggedIn()) snprintf(b, n, "%s", nube::saved.email);
  else snprintf(b, n, "!Sin cuenta");
}
static void fillWifi(char* b, size_t n, int) {
  if (wifiSsid[0]) snprintf(b, n, "%s", wifiSsid);
  else snprintf(b, n, "!Sin configurar");
}
static void fillUpdate(char* b, size_t n, int) {
  if (updateAvailable()) snprintf(b, n, "!Hay versión nueva");
  else if (checkState == CheckState::Running) snprintf(b, n, "Buscando...");
  else snprintf(b, n, "Versión %s", FW_VERSION);
}

static void onMenu(lv_event_t* e) {
  switch ((ui::Btn)argOf(e)) {
    case ui::Btn::SetMatch: go(View::Match); break;
#if HAS_AUDIO
    case ui::Btn::SetSound: go(View::Sound); break;
#endif
    case ui::Btn::SetScreen: go(View::Display); break;
    case ui::Btn::SetMando: go(View::Mando); break;
    case ui::Btn::SetTeams: go(View::Teams); break;
    case ui::Btn::SetData: go(View::Data); break;
    case ui::Btn::SetPlayers: openPlayers(); break;
    case ui::Btn::SetAccount: openAccount(); break;
    case ui::Btn::SetWifi: openWifi(); break;
    case ui::Btn::SetUpdate: openUpdate(); break;
    case ui::Btn::SetTest:
      screen = Screen::Home;
      enterTest();
      break;
    default: break;
  }
}

static void buildMenu() {
  header("AJUSTES", onBackHome);
  struct Item { ui::Btn id; const char* title; void (*fill)(char*, size_t, int); };
  static const Item ITEMS[] = {
      {ui::Btn::SetMatch, "PARTIDO", fillMatch},     {ui::Btn::SetSound, "SONIDO", fillSound},
      {ui::Btn::SetScreen, "PANTALLA", fillDisplay}, {ui::Btn::SetMando, "MANDO", fillMando},
      {ui::Btn::SetPlayers, "JUGADORES", fillPlayers}, {ui::Btn::SetAccount, "CUENTA", fillAccount},
      {ui::Btn::SetWifi, "WI-FI", fillWifi},         {ui::Btn::SetUpdate, "ACTUALIZACIONES", fillUpdate},
      {ui::Btn::SetTest, "PRUEBA DE LA PLACA", nullptr},
      {ui::Btn::SetTeams, "EQUIPOS", fillTeams},     {ui::Btn::SetData, "DATOS", fillData},
  };
  for (const Item& it : ITEMS) {
    const ui::Rect* r = nullptr;
    for (const ui::Button& b : ui::SETTINGS)
      if (b.id == it.id) r = &b.r;
    if (!r) continue;
    // Los títulos largos, un poco más pequeños: que no se partan a media palabra.
    uint16_t size = strlen(it.title) > 12 && !strchr(it.title, ' ') && r->w < 300 ? 20 : 23;
    lv_obj_t* b = fichas::btn(scr, r->x, r->y, r->w, r->h, "", size, fichas::Kind::Normal, onMenu, (void*)(intptr_t)it.id);
    lv_obj_t* t = lv_obj_get_child(b, 0);
    lv_label_set_text(t, it.title);
    lv_obj_set_width(t, r->w - 20);
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, it.fill ? -13 : 0);
    if (it.fill) {
      lv_obj_t* v = val(b, 10, r->h - 32, r->w - 20, 400, 16, it.fill);
      lv_obj_remove_flag(v, LV_OBJ_FLAG_CLICKABLE);
    }
  }
}

// ---------------------------------------------------------------- PARTIDO
static bool isCond(int c) { return (int)cfg.endCondition == c; }
static bool isFirst(int t) { return (int)cfg.penaltyFirst == t; }
static void onCond(lv_event_t* e) {
  cfg.endCondition = (mfv3::EndCondition)argOf(e);
  saveConfig();
  changed();
}
static void onFirst(lv_event_t* e) {
  cfg.penaltyFirst = (Team)argOf(e);
  saveConfig();
  changed();
}
static void fillGoals(char* b, size_t n, int) { snprintf(b, n, "%u", cfg.goalsPerPeriod); }
static void fillMinutes(char* b, size_t n, int) { snprintf(b, n, "%u min", cfg.minutesPerPeriod); }
static void onMatchStep(lv_event_t* e) {
  int a = argOf(e), d = (a & 1) ? 1 : -1;
  if (a / 2 == 0) {
    int g = cfg.goalsPerPeriod + d;
    if (g >= 1 && g <= 20) cfg.goalsPerPeriod = (uint8_t)g;
  } else {
    int m = cfg.minutesPerPeriod + d;
    if (m >= 1 && m <= 30) cfg.minutesPerPeriod = (uint8_t)m;
  }
  saveConfig();
  changed();
}

static void buildMatch() {
  header("PARTIDO", onBackToMenu);
  caption("CÓMO SE GANA", 24, 80);
  helpBtn(scr, 190, 70, 40,
          "GOLES: gana el primero que llega a esos goles. TIEMPO: dos partes de esos minutos; gana quien va "
          "por delante (si hay empate, prórroga y penaltis). AMBAS: con tiempo, pero el primero que llega a "
          "los goles gana en ese momento.");
  static const char* const COND[] = {"GOLES", "TIEMPO", "AMBAS"};
  for (int i = 0; i < 3; i++) opt(scr, 24 + i * 256, 108, 240, 72, COND[i], 23, isCond, i, onCond);
  caption("GOLES PARA GANAR", 24, 196);
  stepper(24, 224, 360, 76, fillGoals, onMatchStep, 0);
  caption("MINUTOS POR PARTE", 416, 196);
  stepper(416, 224, 360, 76, fillMinutes, onMatchStep, 1);
  caption("PRIMERO EN LOS PENALTIS", 24, 316);
  opt(scr, 24, 344, 368, 76, "BLANCO", 23, isFirst, (int)Team::White, onFirst);
  opt(scr, 408, 344, 368, 76, "AZUL", 23, isFirst, (int)Team::Blue, onFirst);
}

// ---------------------------------------------------------------- SONIDO (lista con scroll: está en Ajustes)
#if HAS_AUDIO
struct Moment { mfv3snd::Event ev; const char* name; const char* help; };
// Mismos nombres y explicaciones que la app (src/services/sound/soundEvents.ts).
static const Moment MOMENTS[] = {
    {mfv3snd::Event::Goal, "Gol", "El de siempre: las voces de comentarios de la tarjeta SD, según cómo va el partido. Si no hay voces, un pitido."},
    {mfv3snd::Event::GoalAnnulled, "Gol anulado", nullptr},
    {mfv3snd::Event::Streak3, "3 seguidos", "El mismo equipo marca 3 goles seguidos sin que el rival responda. Suena en el tercero."},
    {mfv3snd::Event::Comeback, "Remontada", "Un equipo que iba perdiendo por 2 goles o más se pone por delante."},
    {mfv3snd::Event::Equalizer, "Empate", "El gol que iguala el marcador (el 0-0 del principio no cuenta)."},
    {mfv3snd::Event::MatchPoint, "Bola de partido", "Un equipo gana si marca el siguiente gol. Suena justo después del gol."},
    {mfv3snd::Event::GoldenGoal, "Gol de oro", "El gol que gana el partido en la prórroga."},
    {mfv3snd::Event::PenaltyScored, "Penalti marcado", nullptr},
    {mfv3snd::Event::PenaltyMissed, "Penalti fallado", nullptr},
    {mfv3snd::Event::PenaltyAwarded, "Penalti señalado", "En Partido Loco, cuando un hándicap señala un penalti."},
    {mfv3snd::Event::MatchStart, "Empieza", nullptr},
    {mfv3snd::Event::PeriodEnd, "Fin de la parte", nullptr},
    {mfv3snd::Event::Victory, "Victoria", nullptr},
    {mfv3snd::Event::Rout, "Goleada", "Ganar dejando al rival a 0, o ganar por 4 goles o más. Suena en lugar de «Victoria»."},
};
static const int MOMENT_COUNT = sizeof(MOMENTS) / sizeof(MOMENTS[0]);
// arg = momento * 3 + opción (0 el de siempre, 1 ninguno, 2 propios de la app).
static bool isKind(int a) {
  const mfv3snd::Choice& c = sndChoices.c[(uint8_t)MOMENTS[a / 3].ev];
  return (int)c.kind == (a % 3 == 0 ? (int)mfv3snd::Kind::Default : a % 3 == 1 ? (int)mfv3snd::Kind::None : (int)mfv3snd::Kind::Custom);
}
static void onKind(lv_event_t* e) {
  int a = argOf(e);
  mfv3snd::Choice& c = sndChoices.c[(uint8_t)MOMENTS[a / 3].ev];
  // Los propios se quedan apuntados: volver a PROPIOS los recupera (hasta apagar, si no se eligen otra vez).
  c.kind = a % 3 == 0 ? mfv3snd::Kind::Default : a % 3 == 1 ? mfv3snd::Kind::None : mfv3snd::Kind::Custom;
  if (c.kind == mfv3snd::Kind::Custom && !c.n) c.kind = mfv3snd::Kind::Default;
  saveChoices();
  changed();
}
static void onHear(lv_event_t* e) {
  int i = argOf(e);
  mfv3snd::Event ev = MOMENTS[i].ev;
  const mfv3snd::Choice& c = sndChoices.c[(uint8_t)ev];
  if (c.kind == mfv3snd::Kind::None) return ui::toast("Este momento no suena");
  if (!sndVolume) return ui::toast("El volumen está a 0");
  if (soundManagementBusy()) return ui::toast("Espera a que terminen los sonidos");
  // El gol de siempre son las voces de comentarios: se oye una de las de cualquier gol.
  if (ev == mfv3snd::Event::Goal && c.kind == mfv3snd::Kind::Default) {
    uint32_t code = comentarios::catalog.pick(1u << comentarios::Comodin, esp_random(), esp_random());
    if (code) {
      player.play(audio::Item{false, code, mfv3snd::Tune{nullptr, 0}}, false);
      return;
    }
  }
  mfv3snd::EventList l;
  l.add(ev);
  playEvents(l);
}
static void fillVolume(char* b, size_t n, int) {
  if (sndVolume) snprintf(b, n, "%u %%", (unsigned)sndVolume);
  else snprintf(b, n, "!Sin sonido");
}
static void onVolume(lv_event_t* e) {
  int v = (int)sndVolume + ((argOf(e) & 1) ? 10 : -10);
  sndVolume = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
  prefs.putUChar("vol", sndVolume);
  player.setVolume(sndVolume);
  playTestTone();  // para oír cómo queda
  changed();
}

static void buildSound() {
  header("SONIDO", onBackToMenu);
  caption("VOLUMEN", 24, 96);
  stepper(250, 76, 526, 64, fillVolume, onVolume, 0);
  caption("QUÉ SUENA EN CADA MOMENTO", 24, 154);
  lv_obj_t* list = scroller(184);
  for (int i = 0; i < MOMENT_COUNT; i++) {
    const Moment& m = MOMENTS[i];
    int y = 4 + i * 72;
    lv_obj_t* name = ui::text(list, m.name, 800, 20, ui::color::TEXT);
    lv_obj_set_pos(name, 24, y + 20);
    lv_obj_set_width(name, 200);
    lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
    if (m.help) helpBtn(list, 226, y + 6, 52, m.help);
    fichas::btn(list, 286, y, 92, 64, "OÍR", 20, fichas::Kind::Normal, onHear, (void*)(intptr_t)i);
    bool own = sndChoices.c[(uint8_t)m.ev].n > 0;
    opt(list, 386, y, own ? 168 : 222, 64, "EL DE SIEMPRE", 17, isKind, i * 3, onKind);
    opt(list, own ? 562 : 616, y, own ? 112 : 160, 64, "NINGUNO", 17, isKind, i * 3 + 1, onKind);
    if (own) opt(list, 682, y, 102, 64, "PROPIOS", 17, isKind, i * 3 + 2, onKind);
  }
  lv_obj_t* end = ui::box(list, 0, 4 + MOMENT_COUNT * 72, 10, 8);  // margen al final del scroll
  (void)end;
}
#endif

// ---------------------------------------------------------------- PANTALLA
static const uint8_t SLEEP_OPTS[] = {1, 2, 5, 10, 0};
static bool isSleep(int i) { return sleepMinutes == SLEEP_OPTS[i]; }
static void onSleep(lv_event_t* e) {
  sleepMinutes = SLEEP_OPTS[argOf(e)];
  prefs.putUChar("slp", sleepMinutes);
  changed();
}
static void fillBright(char* b, size_t n, int) { snprintf(b, n, "%u %%", (unsigned)userBrightness); }
static void onBright(lv_event_t* e) {
  int v = (int)userBrightness + ((argOf(e) & 1) ? 10 : -10);
  userBrightness = (uint8_t)(v < 10 ? 10 : v > 100 ? 100 : v);
  prefs.putUChar("bri", userBrightness);
  if (!dimmed) lcd.backlight(backlightLevel());
  changed();
}
static void fillClock(char* b, size_t n, int) {
  if (!rtc.clockOk) snprintf(b, n, "!Sin hora");
  else snprintf(b, n, "%02u:%02u · %02u/%02u/%04u", rtc.now.hour, rtc.now.minute, rtc.now.day, rtc.now.month, rtc.now.year);
}
static void fillClockState(char* b, size_t n, int) {
  if (!wifiSsid[0]) snprintf(b, n, "Necesita el Wi-Fi");
  else if (checkState == CheckState::Running) snprintf(b, n, "Poniendo en hora...");
  else if (checkState == CheckState::NoConnect || checkState == CheckState::Failed) snprintf(b, n, "!No se pudo: vuelve a probar");
  else snprintf(b, n, "Se pone sola al buscar novedades");
}
static void onSetClock(lv_event_t*) {
  if (!wifiSsid[0]) return ui::toast("Primero, el Wi-Fi en AJUSTES");
  if (checkState == CheckState::Running || versionsRunning) return;
  if (nube::busy() || s3net::scanning) return ui::toast("Espera a que termine la conexión");
  checkState = CheckState::Never;
  startCheck();  // la misma consulta que las novedades: trae también la hora de Internet
  changed();
}

static void buildDisplay() {
  header("PANTALLA", onBackToMenu);
  caption("BRILLO", 24, 100);
  stepper(250, 80, 526, 72, fillBright, onBright, 0);
  caption("RELOJ DE REPOSO", 24, 176);
  helpBtn(scr, 250, 166, 44, "Sin tocarla ese tiempo, y nunca durante un partido, la pantalla baja la luz y enseña la hora. Se despierta al tocarla.");
  static const char* const SL[] = {"1 MIN", "2 MIN", "5 MIN", "10 MIN", "NUNCA"};
  for (int i = 0; i < 5; i++) opt(scr, 24 + i * 152, 216, 144, 72, SL[i], 20, isSleep, i, onSleep);
  caption("HORA", 24, 312);
  val(scr, 24, 340, 400, 900, 30, fillClock, 0, ui::color::TEXT, true);
  val(scr, 24, 392, 400, 400, 16, fillClockState, 0, ui::color::TEXT2, true);
  fichas::btn(scr, 440, 330, 336, 84, "PONER EN HORA", 23, fichas::Kind::Normal, onSetClock);
}

// ---------------------------------------------------------------- MANDO
static bool isModel(int m) { return mandoModel() == m; }
static void onModel(lv_event_t* e) {
  uint8_t m = (uint8_t)argOf(e);
  if (mandoModelSaid && mandoModelSaid != m) {
    ui::toast(mandoModelSaid == mfv3::ota::MODEL_C3_ZERO ? "El mando dice que es una C3-Zero" : "El mando dice que es una Super Mini");
    return;
  }
  setMandoModel(m);
  syncMandoModel();
  changed();
}
static void fillLink(char* b, size_t n, int) {
  if (!remoteLinked()) snprintf(b, n, "!Sin señal: pulsa un botón del mando");
  else if (mandoInfoFresh() && mandoBuild) snprintf(b, n, "Conectado · programa %lu", (unsigned long)mandoBuild);
  else snprintf(b, n, "Conectado");
}
static void fillMandoUpd(char* b, size_t n, int) {
  switch (mandoUpd()) {
    case MandoUpd::Unknown: snprintf(b, n, "Pulsa un botón del mando para comprobar su versión"); break;
    case MandoUpd::NeedsCable: snprintf(b, n, "!Necesita una primera actualización por cable"); break;
    case MandoUpd::NeedsModel: snprintf(b, n, "!Elige su modelo para poder actualizarlo"); break;
    case MandoUpd::Outdated: snprintf(b, n, "!Hay una versión nueva para este mando"); break;
    default: snprintf(b, n, pubMando.ok() ? "Al día" : "Busca novedades para comprobarlo"); break;
  }
}
static void onMandoUpdate(lv_event_t*) { openUpdate(); }

static void buildMando() {
  header("MANDO", onBackToMenu);
  val(scr, 24, 96, 752, 900, 26, fillLink, 0, ui::color::TEXT, true);
  caption("MODELO DE LA MINI", 24, 160);
  helpBtn(scr, 290, 150, 44,
          "Super Mini: la mini pequeña con LED azul. C3-Zero: la Waveshare con LED de colores. Cada una necesita su "
          "programa al actualizar. Los mandos actualizados lo dicen solos.");
  opt(scr, 24, 200, 368, 80, "SUPER MINI", 23, isModel, mfv3::ota::MODEL_SUPER_MINI, onModel);
  opt(scr, 408, 200, 368, 80, "C3-ZERO", 23, isModel, mfv3::ota::MODEL_C3_ZERO, onModel);
  caption("ACTUALIZACIÓN", 24, 304);
  val(scr, 24, 334, 752, 400, 22, fillMandoUpd, 0, ui::color::TEXT2, true);
  fichas::btn(scr, 24, 384, 752, 80, "ACTUALIZACIONES", 23, fichas::Kind::Normal, onMandoUpdate);
}

// ---------------------------------------------------------------- pantallas
static void onDelete(lv_event_t* e) {
  if (lv_event_get_target_obj(e) == scr) {
    scr = nullptr;
    help = askBox = workBox = workBar = workText = teamNameLbl = nullptr;
    optCount = valCount = 0;
  }
}

static void go(View v) {
#if !HAS_AUDIO
  if (v == View::Sound) v = View::Menu;
#endif
  view = v;
  lv_obj_t* old = scr;
  scr = ui::screen();
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  ui::gradient(scr, 0x0f2140, 0x05090f);
  help = askBox = workBox = workBar = workText = teamNameLbl = nullptr;
  optCount = valCount = 0;
  switch (v) {
    case View::Menu: buildMenu(); break;
    case View::Match: buildMatch(); break;
#if HAS_AUDIO
    case View::Sound: buildSound(); break;
#endif
    case View::Display: buildDisplay(); break;
    case View::Mando: buildMando(); break;
    case View::Teams: buildTeams(); break;
    case View::TeamDetail: buildTeamDetail(); break;
    case View::TeamName: buildTeamName(); break;
    case View::TeamPlayers: buildTeamPlayers(); break;
    case View::Data: buildData(); break;
    case View::Qr: buildQr(); break;
    default: buildMenu(); break;
  }
  refresh();
  refreshAt = millis();
  if (old) lv_obj_remove_event_cb(old, onDelete);  // la vieja se borra al cargar esta: que no limpie lo nuevo
  ui::show(scr);
}

static bool active() { return scr && lv_screen_active() == scr; }

// Al entrar desde Inicio (fresh: al menú) o al volver de Wi-Fi, Actualizaciones, Jugadores, Cuenta o del reposo.
static void show(bool fresh) {
  if (fresh) view = View::Menu;
  go(view);
}

// Lo que cambia solo (mando, versiones, hora): se repasa cada medio segundo, sin rehacer nada.
static void tick() {
  if (!active() || millis() - refreshAt < 500) return;
  refreshAt = millis();
  refresh();
}

}  // namespace opciones

#endif  // MFV3_S3_OPCIONES_H
