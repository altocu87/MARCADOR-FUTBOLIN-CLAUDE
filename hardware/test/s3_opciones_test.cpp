// AJUSTES de la pantalla de 7" (s3_opciones.h) con LVGL de verdad y toques: menú, partido, sonido, pantalla y
// mando, lo que se guarda en la NVS y la actualización del mando con el programa de su modelo de mini.
// Con MFV3_CAPTURAS=<carpeta> guarda además una imagen .ppm de cada pantalla.
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static void run(uint32_t ms) {
  uint32_t end = sim::now + ms;
  while ((int32_t)(end - sim::now) > 0) loop();
}
static void tap(int x, int y) {
  sim::touchX = x; sim::touchY = y; sim::touchDown = true; run(60);
  sim::touchDown = false; run(60);
}
static void tapBtn(const ui::Rect& r) { tap(r.x + r.w / 2, r.y + r.h / 2); }
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
static const ui::Rect& setting(ui::Btn id) {
  for (const ui::Button& b : ui::SETTINGS)
    if (b.id == id) return b.r;
  assert(false);
  return ui::BACK;
}
static void shot(const char* name) {
  const char* dir = getenv("MFV3_CAPTURAS");
  if (!dir) return;
  run(200);
  lv_obj_update_layout(lv_screen_active());
  lv_draw_buf_t* buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
  assert(buf);
  std::string path = std::string(dir) + "/" + name + ".ppm";
  FILE* f = fopen(path.c_str(), "wb");
  assert(f);
  fprintf(f, "P6\n%u %u\n255\n", buf->header.w, buf->header.h);
  for (uint32_t y = 0; y < buf->header.h; ++y)
    for (uint32_t x = 0; x < buf->header.w; ++x) {
      const uint8_t* p = buf->data + y * buf->header.stride + x * 3;
      uint8_t rgb[] = {p[2], p[1], p[0]};
      fwrite(rgb, 1, 3, f);
    }
  fclose(f);
  lv_draw_buf_destroy(buf);
}
static const uint8_t MANDO_MAC[6] = {0xAA, 1, 2, 3, 4, 5};
// Saludo del mando: los programas anteriores no dicen su modelo (MODEL_UNKNOWN).
static void mandoInfo(uint32_t build, uint8_t model) {
  uint8_t pk[mfv3::ota::MAX_PACKET];
  uint8_t n = mfv3::ota::encodeInfo(pk, GRUPO_MESA, build, false, model);
  sim::radioDeliver(MANDO_MAC, std::vector<uint8_t>(pk, pk + n));
  loop();
}
// Un saludo como el de los mandos anteriores (sin el byte del modelo).
static void oldMandoInfo(uint32_t build) {
  uint8_t pk[mfv3::ota::MAX_PACKET];
  uint8_t n = mfv3::ota::encodeInfo(pk, GRUPO_MESA, build, false);
  sim::radioDeliver(MANDO_MAC, std::vector<uint8_t>(pk, pk + n - 1));
  loop();
}

// Nube simulada: login, bajada con los equipos de la app en las preferencias, lectura de las preferencias, subidas
// y el perfil público. Guarda lo que se sube.
static std::string cloudPrefs, lastPrefsPost, lastProfile;
static int posts = 0;
static nube::Resp cloudReply(int code, const std::string& body) {
  nube::Resp r;
  r.code = code;
  r.body = (char*)malloc(body.size() + 1);
  memcpy(r.body, body.c_str(), body.size() + 1);
  r.len = body.size();
  return r;
}
static nube::Resp fakeCloud(const char* method, const char* url, const char*, const char*, const char* body) {
  std::string u = url;
  if (u.find("/auth/v1/token") != std::string::npos)
    return cloudReply(200, "{\"access_token\":\"a\",\"refresh_token\":\"r1\",\"user\":{\"id\":\"u1\",\"email\":\"dueno@mesa.es\"}}");
  if (u.find("rpc/publicar_perfil_jugador") != std::string::npos) {
    lastProfile = body;
    return cloudReply(204, "");
  }
  if (!strcmp(method, "GET") && u.find("select=datos&tipo=eq.prefs") != std::string::npos)
    return cloudReply(200, "[{\"datos\":" + cloudPrefs + "}]");
  if (!strcmp(method, "GET"))
    return cloudReply(200, "[{\"tipo\":\"prefs\",\"id\":\"mesa\",\"borrado\":false,\"actualizado\":\"2026-10-22T10:00:00+00:00\",\"datos\":" + cloudPrefs + "}]");
  posts++;
  if (body && strstr(body, "\"tipo\":\"prefs\"")) lastPrefsPost = body;
  return cloudReply(201, "");
}
static void keyTap(int row, int col) { tap(24 + col * 76 + 35, 150 + row * 66 + 30); }
static void askYes() { tap(90 + 326 + 135, 100 + 190 + 34); }

int main() {
  assets::setRoot(getenv("MFV3_ASSETS") ? getenv("MFV3_ASSETS") : "../build/placa/fat");
  sim::now = 1000;
  setup();
  test.active = false;
  nube::logout();  // sin la cuenta que pudiera dejar una prueba anterior
  nube::phase = nube::OFF;
  goHome();
  run(300);

  // ---- Menú: nueve botones grandes, con su valor debajo.
  tap(750, 46);  // engranaje
  assert(screen == Screen::Settings && opciones::active() && opciones::view == opciones::View::Menu);
  for (const char* s : {"PARTIDO", "SONIDO", "PANTALLA", "MANDO", "JUGADORES", "CUENTA", "WI-FI", "ACTUALIZACIONES",
                        "PRUEBA DE LA PLACA", "Sin configurar", "Sin cuenta", "Sin señal"})
    assert(shows(s));
  shot("ajustes-menu");

  // ---- PARTIDO: cómo se gana, goles, minutos y quién tira primero en los penaltis.
  tapBtn(setting(ui::Btn::SetMatch));
  assert(opciones::view == opciones::View::Match && shows("CÓMO SE GANA") && shows("PRIMERO EN LOS PENALTIS"));
  tap(24 + 256 + 120, 144);  // TIEMPO
  assert(cfg.endCondition == mfv3::EndCondition::Time && sim::nvs.count("cond"));
  uint8_t goals = cfg.goalsPerPeriod, mins = cfg.minutesPerPeriod;
  tap(24 + 360 - 48, 262);   // + goles
  tap(416 + 48, 262);        // - minutos
  assert(cfg.goalsPerPeriod == goals + 1 && cfg.minutesPerPeriod == mins - 1);
  tap(408 + 184, 382);       // AZUL
  assert(cfg.penaltyFirst == Team::Blue && sim::nvs.count("pfirst"));
  shot("ajustes-partido");
  // Solo cambian los botones tocados: la pantalla es la misma.
  lv_obj_t* same = lv_screen_active();
  tap(24 + 120, 144);        // GOLES
  assert(lv_screen_active() == same && cfg.endCondition == mfv3::EndCondition::Goals);
  tapBtn(ui::BACK);
  assert(opciones::view == opciones::View::Menu && shows("A ") && shows(" goles"));
  // Se recupera al encender.
  mfv3::Config before = cfg;
  cfg = mfv3::Config();
  loadPrefs();
  assert(cfg.penaltyFirst == Team::Blue && cfg.goalsPerPeriod == before.goalsPerPeriod &&
         cfg.minutesPerPeriod == before.minutesPerPeriod && cfg.endCondition == before.endCondition);

#if HAS_AUDIO
  // ---- SONIDO: volumen y qué suena en cada momento.
  tapBtn(setting(ui::Btn::SetSound));
  assert(opciones::view == opciones::View::Sound && shows("QUÉ SUENA EN CADA MOMENTO") && shows("Gol anulado"));
  assert(!shows("PROPIOS"));  // ningún momento con sonidos propios de la app
  uint8_t vol = sndVolume;
  tap(776 - 48, 108);        // + volumen
  assert(sndVolume == (vol + 10 > 100 ? 100 : vol + 10) && sim::nvs.count("vol"));
  tap(250 + 48, 108);        // - volumen
  assert(sndVolume == (vol + 10 > 100 ? 90 : vol));
  tap(616 + 80, 188 + 32);   // Gol → NINGUNO
  assert(sndChoices.c[(uint8_t)mfv3snd::Event::Goal].kind == mfv3snd::Kind::None && sim::nvs.count("snd"));
  tap(386 + 111, 188 + 32);  // Gol → EL DE SIEMPRE
  assert(sndChoices.c[(uint8_t)mfv3snd::Event::Goal].kind == mfv3snd::Kind::Default);
  tap(226 + 26, 188 + 32);   // «?» de Gol
  assert(shows("voces de comentarios") && shows("ENTENDIDO"));
  shot("ajustes-sonido-ayuda");
  tap(400, 90 + 214 + 32);   // ENTENDIDO
  assert(!shows("ENTENDIDO"));
  shot("ajustes-sonido");
  // Un momento con sonidos propios (elegidos en la app): sale PROPIOS y se puede volver a ellos.
  sndChoices.set("victory 1a2b3c4d");
  tapBtn(ui::BACK);
  tapBtn(setting(ui::Btn::SetSound));
  assert(shows("PROPIOS"));
  tapBtn(ui::BACK);
#endif

  // ---- PANTALLA: brillo, reloj de reposo y hora.
  tapBtn(setting(ui::Btn::SetScreen));
  assert(opciones::view == opciones::View::Display && shows("RELOJ DE REPOSO") && shows("PONER EN HORA"));
  uint8_t bri = userBrightness;
  tap(250 + 48, 116);        // - brillo
  assert(userBrightness == bri - 10 && sim::nvs.count("bri"));
  tap(24 + 4 * 152 + 72, 252);  // NUNCA
  assert(sleepMinutes == 0 && sim::nvs.count("slp"));
  shot("ajustes-pantalla");
  run(12UL * 60UL * 1000UL);
  assert(!dimmed && opciones::active());  // sin reposo
  tap(24 + 72, 252);         // 1 MIN
  assert(sleepMinutes == 1);
  run(61000);
  assert(dimmed);
  tap(400, 240);             // despierta, sin tocar nada
  run(300);
  assert(!dimmed && opciones::active() && opciones::view == opciones::View::Display);
  sleepMinutes = 0;
  loadPrefs();
  assert(sleepMinutes == 1 && userBrightness == bri - 10);
  tap(24 + 152 + 72, 252);   // 2 MIN otra vez
  tapBtn(ui::BACK);

  // ---- MANDO: modelo de la mini y su actualización.
  const std::string base = "https://altocu87.github.io/MARCADOR-FUTBOLIN-CLAUDE/firmware/";
  std::string superBin(7000, 'S'), zeroBin(8000, 'Z');
  auto json = [](uint32_t build, const std::string& bin) {
    std::vector<uint8_t> v(bin.begin(), bin.end());
    return "{\"build\": " + std::to_string(build) + ", \"size\": " + std::to_string(bin.size()) + ", \"md5\": \"" + sim::fakeMd5(v) + "\"}";
  };
  sim::web[base + S3_BOARD_ID "/ota.json"] = json(1, std::string(10, 'P'));  // la pantalla, al día
  sim::web[base + "mando-c3/ota.json"] = json(70, superBin);
  sim::web[base + "mando-c3/mando-c3-ota.bin"] = superBin;
  sim::web[base + "mando-c3zero/ota.json"] = json(71, zeroBin);
  sim::web[base + "mando-c3zero/mando-c3zero-ota.bin"] = zeroBin;
  sim::networks = {{"CasaPepe", "Clave#1", -50}};
  saveWifi("CasaPepe", "Clave#1");
  checkState = CheckState::Never;
  startCheck();
  run(500);
  assert(checkState == CheckState::Ok && pubMandoBy[1].build == 70 && pubMandoBy[2].build == 71);

  // Un mando con el programa anterior (no dice su modelo): no se le ofrece nada hasta elegirlo.
  oldMandoInfo(54);
  assert(mandoBuild == 54 && mandoModelSaid == 0 && mandoModel() == 0 && mandoUpd() == MandoUpd::NeedsModel);
  assert(pendingHistoryEntries() == (screenOutdated() ? 1u : 0u));  // el mando no cuenta como novedad
  run(600);
  assert(shows("Elige su modelo"));
  tapBtn(setting(ui::Btn::SetMando));
  assert(opciones::view == opciones::View::Mando && shows("SUPER MINI") && shows("C3-ZERO") && shows("programa 54"));
  shot("ajustes-mando");
  tap(408 + 184, 240);       // C3-ZERO
  assert(mandoModelChosen == mfv3::ota::MODEL_C3_ZERO && sim::nvs.count("mmod"));
  assert(pubMando.build == 71 && mandoUpd() == MandoUpd::Outdated && updateAvailable() && pendingHistoryEntries() >= 1);
  run(600);
  assert(shows("Hay una versión nueva"));
  shot("ajustes-mando-zero");
  // Se recuerda al apagar y encender.
  mandoModelChosen = 0;
  loadPrefs();
  assert(mandoModelChosen == mfv3::ota::MODEL_C3_ZERO);

  // Instalar: se descarga el programa de la C3-Zero, no el de la Super Mini.
  tap(400, 424);             // ACTUALIZACIONES
  assert(screen == Screen::Update);
  startInstall();
  assert(screen == Screen::Install && instMando && instMandoModel == mfv3::ota::MODEL_C3_ZERO);
  for (int i = 0; i < 40 && inst != Inst::MandoRadio && inst != Inst::Error; i++) run(100);
  assert(inst == Inst::MandoRadio);
  assert(std::equal(zeroBin.begin(), zeroBin.end(), sim::otaSlotData.begin()));
  sender.cancel();
  inst = Inst::Cancelled;
  screen = Screen::Settings;
  run(200);

  // Un mando nuevo dice su modelo: manda lo que dice y no se puede elegir otro.
  mandoInfo(72, mfv3::ota::MODEL_SUPER_MINI);
  run(100);
  assert(mandoModel() == mfv3::ota::MODEL_SUPER_MINI && mandoModelChosen == mfv3::ota::MODEL_SUPER_MINI);
  assert(pubMando.build == 70 && mandoUpd() == MandoUpd::UpToDate);
  if (opciones::view != opciones::View::Mando) tapBtn(setting(ui::Btn::SetMando));
  tap(408 + 184, 240);       // C3-ZERO: no
  assert(mandoModelChosen == mfv3::ota::MODEL_SUPER_MINI && shows("El mando dice que es una Super Mini"));

  // ---- EQUIPOS: crear una pareja con nombre; sale en el marcador; se une con los de la cuenta.
  tapBtn(ui::BACK);
  assert(opciones::view == opciones::View::Menu && shows("EQUIPOS") && shows("DATOS"));
  shot("ajustes-menu-completo");
  uint32_t pa = jugadores.add("Ana", nullptr), pb = jugadores.add("Bea", nullptr), pc = jugadores.add("Carla", nullptr);
  assert(pa && pb && pc);
  char ida[48], idb[48], idc[48];
  jugadores.idOf(pa, ida, sizeof(ida));
  jugadores.idOf(pb, idb, sizeof(idb));
  jugadores.idOf(pc, idc, sizeof(idc));
  tapBtn(setting(ui::Btn::SetTeams));
  assert(opciones::view == opciones::View::Teams && shows("NUEVO EQUIPO") && shows("Sin equipos"));
  tap(400, 76 + 36);  // + NUEVO EQUIPO
  assert(opciones::view == opciones::View::TeamName);
  keyTap(1, 3); keyTap(2, 0); keyTap(1, 5); keyTap(1, 8);  // R a y o (tras la primera, minúsculas)
  assert(!strcmp(opciones::teamName, "Rayo"));
  shot("ajustes-equipo-nombre");
  tapBtn(ui::BACK);  // SIGUIENTE
  assert(opciones::view == opciones::View::TeamPlayers);
  int ia = -1, ib = -1;
  for (int i = 0; i < jugadores.count; i++) {
    if (jugadores.list[i].code == pa) ia = i;
    if (jugadores.list[i].code == pb) ib = i;
  }
  auto card = [](int i) { tap(16 + (i % 4) * 196 + 90, 76 + 4 + (i / 4) * 172 + 80); };
  card(ia);
  card(ib);
  shot("ajustes-equipo-pareja");
  tapBtn(ui::BACK);  // GUARDAR
  assert(equipos::count == 1 && !strcmp(equipos::list[0].name, "Rayo") && equipos::dirty);
  assert(opciones::view == opciones::View::TeamDetail && shows("Rayo") && shows("Ana") && shows("Bea"));
  assert(FFat.exists("/jugadores/equipos.txt") && FFat.exists("/jugadores/equipos.pend"));
  shot("ajustes-equipo");
  // En el partido, el lado de Ana y Bea se llama «Rayo».
  chosenTeams = elegir::Teams();
  chosenTeams.n[0] = 2; chosenTeams.code[0][0] = pa; chosenTeams.code[0][1] = pb;
  chosenTeams.n[1] = 1; chosenTeams.code[1][0] = pc;
  haveTeams = true;
  screen = Screen::Home;
  startMatch();
  run(400);
  assert(inMatch && shows("Rayo") && shows("AZUL"));
  inMatch = false;
  haveTeams = false;
  goHome();
  run(200);
  // Con cuenta: lo de la placa se sube unido a lo de la app (sin tocar el resto de sus preferencias).
  cloudPrefs = std::string("{\"soundEvents\":{\"goal\":{\"kind\":\"none\"}},\"clubs\":[{\"id\":\"club_app\",\"name\":\"App FC\",\"logo\":\"emoji:x\",\"playerIds\":[\"") +
               ida + "\",\"" + idc + "\"],\"createdAt\":5}]}";
  nube::http = fakeCloud;
  nube::wifiUp = nullptr;
  nube::wifiDown = nullptr;
  snprintf(nube::saved.refresh, sizeof(nube::saved.refresh), "r0");
  nube::phase = nube::IDLE;
  nube::sync();
  for (int i = 0; i < 30 && nube::busy(); i++) nube::apply();
  assert(!nube::busy() && nube::phase == nube::OK);
  assert(lastPrefsPost.find("Rayo") != std::string::npos && lastPrefsPost.find("App FC") != std::string::npos);
  assert(lastPrefsPost.find("soundEvents") != std::string::npos);
  assert(equipos::count == 2 && !equipos::dirty && equipos::find(ida, idc) && !FFat.exists("/jugadores/equipos.pend"));
  // Borrar uno aquí: al subir, la cuenta no lo devuelve.
  equipos::removeAt(equipos::indexOf("club_app"));
  lastPrefsPost.clear();
  nube::sync();
  for (int i = 0; i < 30 && nube::busy(); i++) nube::apply();
  assert(lastPrefsPost.find("Rayo") != std::string::npos && lastPrefsPost.find("App FC") == std::string::npos);
  assert(equipos::count == 1);
  // Y lo que cambia en la cuenta (sin cambios aquí) llega a la placa.
  cloudPrefs = std::string("{\"clubs\":[{\"id\":\"club_x\",\"name\":\"Nuevo\",\"logo\":\"\",\"playerIds\":[\"") + idb + "\",\"" + idc + "\"],\"createdAt\":9}]}";
  nube::sync();
  for (int i = 0; i < 30 && nube::busy(); i++) nube::apply();
  assert(equipos::count == 1 && !strcmp(equipos::list[0].name, "Nuevo"));
  equipos::put("", "Rayo", ida, idb, 1);  // otra vez el de antes, para lo que sigue
  tap(750, 46);  // Inicio → engranaje → AJUSTES
  assert(screen == Screen::Settings && opciones::view == opciones::View::Menu);

  // ---- QR DEL PERFIL: código propio, perfil calculado en la placa y publicado.
  opciones::openQr(pa);
  run(800);  // la tarea termina y el texto se repasa cada medio segundo
  assert(opciones::view == opciones::View::Qr && opciones::qrHave && strlen(opciones::qrCreds.token) == 22);
  assert(lastProfile.find("\"p_token\":\"") != std::string::npos && lastProfile.find("\"name\":\"Ana\"") != std::string::npos);
  assert(lastProfile.find("\"stats\":{\"general\":") != std::string::npos && lastProfile.find("\"rewards\":[]") != std::string::npos);
  assert(shows("Listo") && FFat.exists("/jugadores/perfiles.txt"));
  shot("ajustes-qr");
  {
    FILE* f = fopen("../build/perfil-placa.json", "wb");
    if (f) { fputs(lastProfile.c_str(), f); fclose(f); }
  }
  perfil::Creds again;
  assert(perfil::credsFor(pa, again) && !strcmp(again.token, opciones::qrCreds.token));  // el mismo código siempre
  opciones::view = opciones::View::Menu;
  opciones::go(opciones::View::Menu);
  screen = Screen::Settings;

  // ---- DATOS: modo prueba.
  datos::removeStdioTree("../build/placa/copias");
  // Un torneo y un tipo de torneo de verdad: se apartan con el modo prueba y van en las copias.
  for (uint16_t i = torneos::nSummaries; i > 0; i--) torneos::discard(torneos::summaries[i - 1].id);
  {
    static torneos::Tournament real;
    assert(torneos::alloc(real));
    torneos::clear(real);
    strcpy(real.id, "t_real-1");
    strcpy(real.name, "Liga de verdad");
    real.nTeams = 3;
    for (int k = 0; k < 3; k++) {
      snprintf(real.teams[k].id, 48, "tt_%d", k);
      snprintf(real.teams[k].name, 48, "Equipo %d", k);
      real.teams[k].n = 1;
      snprintf(real.teams[k].player[0], 40, "p_%d", k);
    }
    real.nFix = 1;
    strcpy(real.fix[0].id, "fx_1");
    real.fix[0].white = 0;
    real.fix[0].blue = 1;
    real.createdAt = 1792000000000ull;
    assert(torneos::save(real));
    torneos::Template tpl;
    torneos::newTemplate(tpl, torneos::Format::League, nullptr);
    strcpy(tpl.name, "Liga de casa");
    assert(torneos::saveTemplate(tpl));
  }
  assert(torneos::nSummaries == 1 && torneos::nTemplates >= 1);
  int realTemplates = torneos::nTemplates;
  int realMatches = partidos::count, realPlayers = jugadores.count;
  tapBtn(setting(ui::Btn::SetData));
  assert(opciones::view == opciones::View::Data && shows("MODO PRUEBA") && shows("COPIA DE SEGURIDAD"));
  shot("ajustes-datos");
  tap(536 + 124, 76 + 34 + 36);  // ACTIVAR
  assert(shows("¿Activar el modo prueba?"));
  askYes();
  run(200);
#ifdef MFV3_TORNEOS_REGLAS
  // Con las reglas de los torneos: una liguilla terminada y otra en juego, con sus partidos.
  {
    int live = 0, done = 0, inTournaments = 0;
    for (uint16_t i = 0; i < torneos::nSummaries; i++) {
      live += torneos::summaries[i].status == torneos::Status::Active;
      done += torneos::summaries[i].status == torneos::Status::Finished && torneos::summaries[i].champion[0];
    }
    for (int i = 0; i < partidos::count; i++) inTournaments += partidos::rows[i].tournament[0] != 0;
    assert(live == 1 && done == 1 && inTournaments > 0 && partidos::count == 40 + inTournaments);
  }
#else
  assert(partidos::count == 40 && torneos::nSummaries == 0);
#endif
  assert(datos::demo && jugadores.count == 12 && partidos::count >= 40 && nube::paused && equipos::count == 0);
  assert(FFat.exists("/jugadores.real") && datos::stdioExists("../build/placa/partidos-sd.real"));
  assert(datos::stdioExists("../build/placa/torneos-sd.real") && !torneos::summaryOf("t_real-1") && torneos::nTemplates == 0);
  assert(shows("Activo"));
  shot("ajustes-datos-prueba");
  // El perfil de un jugador de prueba (con partidos), para compararlo con el de la app (tests/perfil-placa).
  {
    datos::removeStdioTree("../build/perfil-demo");
    datos::copyStdio(partidos::dir, "../build/perfil-demo");
    char* snap = perfil::snapshot(jugadores.list[0].code, 1792000000u);
    assert(snap);
    FILE* f = fopen("../build/perfil-demo/perfil.json", "wb");
    fputs(snap, f);
    fclose(f);
    free(snap);
    f = fopen("../build/perfil-demo/jugadores.json", "wb");
    fputs("[", f);
    for (int i = 0; i < jugadores.count; i++) {
      char id[48];
      jugadores.idOf(jugadores.list[i].code, id, sizeof(id));
      fprintf(f, "%s{\"id\":\"%s\",\"name\":\"%s\"}", i ? "," : "", id, jugadores.list[i].name);
    }
    fputs("]", f);
    fclose(f);
  }
  int postsBefore = posts;
  nube::sync();
  assert(!nube::busy() && posts == postsBefore);  // nada de prueba a la cuenta
  tap(536 + 124, 76 + 160 + 36);  // HACER COPIA: no en modo prueba
  assert(datos::copyCount == 0);
  tap(536 + 124, 76 + 34 + 36);  // QUITAR
  askYes();
  run(200);
  assert(!datos::demo && jugadores.count == realPlayers && partidos::count == realMatches && equipos::count == 2);
  assert(!FFat.exists("/jugadores.real") && !datos::stdioExists("../build/placa/partidos-sd.real") && !nube::paused);
  assert(!datos::stdioExists("../build/placa/torneos-sd.real") && torneos::summaryOf("t_real-1") && torneos::nSummaries == 1);
  assert(torneos::nTemplates == realTemplates);

  // ---- Copia de seguridad: hacer, cambiar cosas, restaurar y borrar.
  tap(536 + 124, 76 + 160 + 36);  // HACER COPIA
  run(200);
  assert(datos::copyCount == 1 && datos::copies[0].players == realPlayers && datos::copies[0].matches == realMatches);
  assert(shows("RESTAURAR"));
  shot("ajustes-datos-copia");
  jugadores.add("Dani", nullptr);
  equipos::removeAt(0);
  assert(datos::stdioExists("../build/placa/copias") && torneos::discard("t_real-1") && torneos::nSummaries == 0);
  cfg.goalsPerPeriod = 3;
  saveConfig();
  tap(16 + 400 + 105, 76 + 250 + 8 + 32);  // RESTAURAR
  askYes();
  run(200);
  assert(jugadores.count == realPlayers && equipos::count == 2 && partidos::count == realMatches && cfg.goalsPerPeriod != 3);
  assert(!FFat.exists("/jugadores.new") && !FFat.exists("/jugadores.old"));
  assert(torneos::summaryOf("t_real-1") && torneos::nTemplates == realTemplates);  // el torneo vuelve con la copia
  assert(!datos::stdioExists("../build/placa/torneos-sd.new") && !datos::stdioExists("../build/placa/torneos-sd.old"));
  tap(16 + 618 + 71, 76 + 250 + 8 + 32);  // BORRAR la copia
  askYes();
  run(200);
  assert(datos::copyCount == 0);

  // ---- Corte de luz a medias: al encender, todo como estaba.
  FFat.rename("/jugadores", "/jugadores.real");  // solo los jugadores apartados (modo prueba a medio poner)
  datos::recover();
  assert(!datos::demo && jugadores.count == realPlayers && FFat.exists("/jugadores") && !FFat.exists("/jugadores.real"));
  FFat.rename("/jugadores", "/jugadores.new");  // restaurar a medias: la de verdad ya fuera, la nueva lista
  datos::recover();
  assert(jugadores.count == realPlayers && !FFat.exists("/jugadores.new"));
  rename("../build/placa/torneos-sd", "../build/placa/torneos-sd.real");  // solo los torneos apartados (lo primero)
  datos::recover();
  assert(!datos::demo && torneos::summaryOf("t_real-1") && !datos::stdioExists("../build/placa/torneos-sd.real"));
  nube::logout();
  tapBtn(ui::BACK);
  assert(opciones::view == opciones::View::Menu);

  // ---- Wi-Fi en marcha: la pantalla va a menos ritmo y se realinea a menudo; al acabar, como antes.
  assert(!uiport::netMode && uiport::pcPclk == uiport::PCLK_HZ);
  s3net::link = s3net::Link::Connecting;
  loop();
  assert(uiport::netMode && uiport::pcPclk == uiport::PCLK_NET_HZ);
  uint32_t resyncs = uiport::pcResyncs;
  run(1100);
  assert(uiport::pcResyncs >= resyncs + 4);  // cada 250 ms
  s3net::link = s3net::Link::Off;
  loop();
  assert(!uiport::netMode && uiport::pcPclk == uiport::PCLK_HZ);
  resyncs = uiport::pcResyncs;
  run(1100);
  assert(uiport::pcResyncs <= resyncs + 1);  // sin red, solo cuando hace falta

  // Volver: menú y después Inicio.
  assert(opciones::view == opciones::View::Menu);
  tapBtn(ui::BACK);
  assert(screen == Screen::Home && !opciones::active());

  std::printf("Ajustes: menu, partido, sonido, pantalla, mando por modelo, equipos y nube, QR, modo prueba, copias y pantalla con Wi-Fi OK\n");
}
