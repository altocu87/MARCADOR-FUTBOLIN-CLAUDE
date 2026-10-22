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

int main() {
  assets::setRoot(getenv("MFV3_ASSETS") ? getenv("MFV3_ASSETS") : "../build/placa/fat");
  sim::now = 1000;
  setup();
  test.active = false;
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

  // Volver: menú y después Inicio.
  tapBtn(ui::BACK);
  assert(opciones::view == opciones::View::Menu);
  tapBtn(ui::BACK);
  assert(screen == Screen::Home && !opciones::active());

  std::printf("Ajustes: menu, partido, sonido, pantalla, mando y su programa por modelo OK\n");
}
