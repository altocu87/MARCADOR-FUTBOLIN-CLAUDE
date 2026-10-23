// Comprobación del sketch de la pantalla de 7" (ESP32-S3) en el PC con LovyanGFX/NVS simulados:
// se juega un partido completo con toques y con líneas del protocolo MFV3 (USB y UART auxiliar).
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static void run(uint32_t ms) {
  uint32_t end = sim::now + ms;
  while ((int32_t)(end - sim::now) > 0) loop();
}
// Un toque: dedo abajo 60 ms y arriba 60 ms (LVGL lee el táctil cada 15 ms).
static void tap(int x, int y) {
  sim::touchX = x; sim::touchY = y; sim::touchDown = true; run(60);
  sim::touchDown = false; run(60);
}
static void usb(const char* s) { for (const char* c = s; *c; c++) sim::in.push_back(*c); sim::in.push_back('\n'); loop(); }
// Placas sin UART auxiliar (Waveshare 7C): las mismas líneas llegan por el USB.
static void aux(const char* s) {
  if (AUX_UART_RX < 0) { usb(s); return; }
  for (const char* c = s; *c; c++) sim::auxIn.push_back(*c);
  sim::auxIn.push_back('\n');
  loop();
}
// Textos que se ven ahora (etiquetas de LVGL de la pantalla activa y de la capa de arriba, separadas por «|»).
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
// Inicio (tarjetas): dos toques en RÁPIDO llevan a la configuración del partido.
static void toConfig() {
  assert(inicio::active());
  tap(140, 280); tap(140, 280);
  run(300);
  assert(!homeCards);
}
static bool sent(const char* s) { return sim::out.find(s) != std::string::npos; }
static size_t count(const std::string& h, const char* s) {
  size_t n = 0;
  for (size_t p = h.find(s); p != std::string::npos; p = h.find(s, p + 1)) n++;
  return n;
}


// Toca la tecla de esa letra en el teclado (en el modo que esté puesto).
static void tapChar(char c) {
  const char* const* rows = kbMode == 2 ? ui::KEYS_SYMBOLS : kbMode == 1 ? ui::KEYS_UPPER : ui::KEYS_LOWER;
  for (uint8_t r = 0; r < 4; r++)
    for (uint8_t i = 0; rows[r][i]; i++)
      if (rows[r][i] == c) {
        ui::Rect k = ui::keyRect(r, i, (uint8_t)strlen(rows[r]));
        tap(k.x + k.w / 2, k.y + k.h / 2);
        return;
      }
  assert(!"tecla no encontrada");
}
static void tapBtn(const ui::Rect& r) { tap(r.x + r.w / 2, r.y + r.h / 2); }

// «Mando» de verdad (receptor de mfv3_ota.h) escuchando lo que la pantalla envía por radio.
struct MemFlash : mfv3::ota::Flash {
  std::vector<uint8_t> data;
  char want[33] = {0};
  bool finished = false;
  bool begin(uint32_t, const uint8_t md5[16]) override {
    for (int i = 0; i < 16; i++) std::snprintf(want + i * 2, 3, "%02x", md5[i]);
    data.clear();
    return true;
  }
  bool write(const uint8_t* d, uint8_t n) override { data.insert(data.end(), d, d + n); return true; }
  bool end() override { finished = sim::fakeMd5(data) == want; return finished; }
  void abort() override { data.clear(); }
};
static const uint8_t MANDO_MAC[6] = {0xAA, 1, 2, 3, 4, 5};
static void mandoInfo(uint32_t build, uint8_t model = mfv3::ota::MODEL_SUPER_MINI) {
  uint8_t pk[mfv3::ota::MAX_PACKET];
  uint8_t n = mfv3::ota::encodeInfo(pk, GRUPO_MESA, build, false, model);
  sim::radioDeliver(MANDO_MAC, std::vector<uint8_t>(pk, pk + n));
  loop();
}

int main() {
  // Cada placa simulada escribe en un almacén propio; no depender de una carpeta
  // build preexistente ni dar por bueno el contador cuando el JSON no se guardó.
  struct MatchStore {
    std::filesystem::path dir;
    MatchStore() {
      dir = "mfv3sim-" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
      assert(std::filesystem::create_directory(dir));
      partidos::setDir(dir.generic_string().c_str());
    }
    ~MatchStore() { std::filesystem::remove_all(dir); }
  } matchStore;
  sim::now = 1000;
  assets::setRoot("../build/placa/fat");  // capas y letras (si no están, se ve sin ellas)
  setup();
  loop();
  assert(sent((std::string("HELLO " S3_BOARD_ID " ") + FW_VERSION + " caps=goles,anular,pausa,pantalla").c_str()) && sent("STATE idle"));
  // Primer arranque: pantalla de prueba, que no toca el marcador.
  assert(test.active && shows("PRUEBA DE LA PLACA") && shows("Toca las 4 esquinas") && sent("STATE test"));
  tap(400, 200);                   // un toque en medio: se registra, no juega
  assert(!inMatch && test.touches == 1 && !testCornersOk());
  tap(20, 20); tap(780, 20); tap(20, 460); tap(780, 460);
  assert(testCornersOk() && shows("OK"));
  {
    uint8_t mac[6] = {0xAB, 0, 0, 0, 0, 1};
    std::vector<uint8_t> pk(mfv3::RADIO_PACKET_SIZE);
    mfv3::radioEncode(pk.data(), GRUPO_MESA, 7, mfv3::RadioCmd::GoalWhite);
    sim::radioDeliver(mac, pk);
    mfv3::radioEncode(pk.data(), GRUPO_MESA, 8, mfv3::RadioCmd::AnnulBlue);  // también vale el toque largo
    sim::radioDeliver(mac, pk);
    loop();
    assert(test.remoteWhite && test.remoteBlue && !inMatch && shows("Mando oido"));
  }
#if HAS_ISOLATED_IO
  run(20);
  sim::exioIn ^= (1u << exio::DI0);
  run(20);
  sim::exioIn ^= (1u << exio::DI0);
  run(20);
  assert(test.sensorWhite && !test.sensorBlue && !inMatch);
#endif
  tap(400, 430);                   // TERMINAR
  assert(!test.active && sim::nvs.count("tested") && sent("STATE idle"));
  loop();
  assert(inicio::active());        // Inicio con las tarjetas de la app
  tap(750, 46);                    // engranaje → AJUSTES → PRUEBA DE LA PLACA: vuelve a la prueba
  assert(screen == Screen::Settings && shows("WI-FI") && shows("Sin configurar") && shows("PRUEBA DE LA PLACA"));
  tapBtn(ui::SETTINGS[5].r);  // PRUEBA DE LA PLACA
  assert(test.active);
  tap(400, 430);
  assert(!test.active);
  loop();

  // Brillo desde el panel rápido de la app: se aplica, se guarda y no pasa de 0–100.
  assert(sent(",brillo"));
  usb("BRILLO 40");
  assert(userBrightness == 40 && sim::nvs.count("bri") && backlightLevel() == 108);
  usb("BRILLO 250");
  assert(userBrightness == 100);

  // Configuración: POR GOLES, a 2 goles (desde 5). Sin partes: el primero que llega gana.
  toConfig();
  for (int i = 0; i < 3; i++) tap(204, 372);  // ▼ de GOLES PARA GANAR
  assert(cfg.goalsPerPeriod == 2 && config::active());
  tap(204, 220);  // tocar el número: no se puede apagar la única tarjeta encendida
  assert(cfg.endCondition == mfv3::EndCondition::Goals && shows("Deja activa al menos una"));
  tap(400, 443);  // CONTINUAR
  assert(inMatch && eng.phase() == Phase::Countdown && shows("TOCA PARA SALTAR"));
  assert(sim::nvs.count("goals"));

  usb("SALTAR");
  assert(eng.phase() == Phase::Playing && partido::active() && shows("Gana quien llegue a 2 goles"));

  // Gol por USB y bloqueo: el segundo impulso inmediato se ignora.
  usb("GB");
  assert(sent("GOAL BLANCO") && sent("LOCK 3000") && count(sim::out, "SCORE 1 0") == 1);
  assert(eng.phase() == Phase::Playing && eng.period() == Period::First);
  aux("GOL_AZUL");  // dentro del bloqueo de 3 s
  assert(eng.score().blue == 0);
  run(3000);
  aux("GOL_AZUL");
  assert((AUX_UART_RX < 0 ? sim::out : sim::auxOut).find("GOAL AZUL") != std::string::npos);
  assert(eng.phase() == Phase::Playing && eng.score().blue == 1);
  run(3000);
  // Pausa por protocolo, continuar con toque.
  usb("PAUSA");
  assert(eng.phase() == Phase::Paused && partido::active());
  tap(400, 260);
  assert(eng.phase() == Phase::Playing);
  tap(160, 200);  // panel BLANCO: llega a 2 y gana
  assert(eng.phase() == Phase::Finished && sent("WIN BLANCO"));
  run(1000);
  assert(victoria::active() && shows("Duración"));
  assert(matchesPlayed == 1 && historyCount == 1 && history[0].white == 2);

  // Revancha y abandono con doble confirmación.
  tap(720, 36);  // REVANCHA
  assert(eng.phase() == Phase::Countdown && eng.score().white == 0);
  tap(400, 240);
  tap(400, 314);  // PAUSA
  assert(eng.phase() == Phase::Paused);
  tap(400, 376);  // Abandonar partido → «¿Abandonar el partido?»
  assert(inMatch && confirmAbandon);
  tap(414, 284);  // Seguir jugando
  assert(inMatch && !confirmAbandon && eng.phase() == Phase::Paused);
  tap(400, 376);
  tap(588, 284);  // Abandonar
  assert(!inMatch && sent("STATE idle") && matchesPlayed == 1);

  // PING/HELLO y atenuación por inactividad (el toque que despierta no pulsa nada).
  usb("PING");
  assert(sent("PONG"));
  run(SLEEP_AFTER_MS + 100);
#if HAS_ISOLATED_IO
  assert(dimmed && sim::exioPwm > 0 && sim::exioPwm < 30);  // ~7 %
  tap(400, 420);  // sobre JUGAR, pero solo despierta
  assert(!dimmed && sim::exioPwm == 255 && !inMatch);
#else
  assert(dimmed && sim::brightness == 16);
  tap(400, 420);  // sobre JUGAR, pero solo despierta
  assert(!dimmed && sim::brightness == 255 && !inMatch);
#endif

#if HAS_ISOLATED_IO
  // Waveshare 7C: luz encendida por el expansor y sensores en las entradas aisladas DI0/DI1.
  assert(sim::exioMode == exio::OUTPUT_MASK && (sim::exioOut & (1u << exio::BACKLIGHT)) && sim::exioPwm == 255);
  toConfig();
  for (int i = 0; i < 4; i++) tap(204, 104);  // ▲: 5 goles
  tap(400, 443);  // CONTINUAR
  tap(400, 240);  // saltar cuenta atrás
  run(3000);
  sim::exioIn ^= (1u << exio::DI1);  // balón cortando el sensor del Azul
  run(20);
  assert(eng.score().blue == 0);     // aún no estable (25 ms)
  run(30);
  assert(eng.score().blue == 1 && (sim::exioOut & (1u << exio::DO1)));
  sim::exioIn ^= (1u << exio::DI1);
  run(1100);
  assert(!(sim::exioOut & (1u << exio::DO1)));
  sim::exioIn ^= (1u << exio::DI0);  // dentro del bloqueo de 3 s: no cuenta
  run(100);
  sim::exioIn ^= (1u << exio::DI0);
  run(2000);
  assert(eng.score().white == 0);
  sim::exioIn ^= (1u << exio::DI0);
  run(100);
  assert(eng.score().white == 1);
  sim::exioIn ^= (1u << exio::DI0);
  tap(400, 314); tap(400, 376); tap(588, 284);  // pausa y abandonar
  assert(!inMatch);
#endif

  // Mando inalámbrico (ESP-NOW): 3 copias de cada orden cuentan una vez; toque largo anula.
  {
    uint8_t mac[6] = {0xAA, 1, 2, 3, 4, 5};
    auto radio = [&](mfv3::RadioCmd c, uint16_t seq) {
      std::vector<uint8_t> pk(mfv3::RADIO_PACKET_SIZE);
      mfv3::radioEncode(pk.data(), GRUPO_MESA, seq, c);
      for (int i = 0; i < 3; i++) sim::radioDeliver(mac, pk);
      loop();
    };
    assert(sim::radioCb != nullptr && sim::wifiChannel == mfv3::RADIO_CHANNEL);
    radio(mfv3::RadioCmd::Hello, 100);
    run(50);
    assert(inicio::dotShown());           // punto del engranaje: mando conectado
    toConfig();
    while (cfg.goalsPerPeriod < 5) tap(204, 104);
    tap(400, 443);                        // CONTINUAR
    radio(mfv3::RadioCmd::GoalWhite, 101);  // en la cuenta atrás: solo la salta
    assert(eng.phase() == Phase::Playing && eng.score().white == 0);
    radio(mfv3::RadioCmd::GoalWhite, 102);
    assert(eng.score().white == 1);
    radio(mfv3::RadioCmd::GoalWhite, 102);  // repetición tardía de la misma orden: se ignora
    run(3000);
    radio(mfv3::RadioCmd::GoalBlue, 103);
    assert(eng.score().white == 1 && eng.score().blue == 1);
    radio(mfv3::RadioCmd::AnnulWhite, 104);
    assert(eng.score().white == 0 && eng.score().blue == 1);
    std::vector<uint8_t> other(mfv3::RADIO_PACKET_SIZE);  // mando de otra mesa (grupo distinto)
    mfv3::radioEncode(other.data(), GRUPO_MESA + 1, 200, mfv3::RadioCmd::AnnulBlue);
    sim::radioDeliver(mac, other);
    loop();
    assert(eng.score().blue == 1);
    usb("AA");                            // misma orden por USB
    assert(eng.score().blue == 0);
    tap(400, 314); tap(400, 376); tap(588, 284);  // pausa y abandonar
    assert(!inMatch);
  }

#if HAS_AUDIO
  // Sonidos propios (Waveshare 7C): altavoces, tarjeta SD, órdenes de la app y momentos del partido.
  {
    assert(player.ok && sent(",brillo,sonido") && !sent(",sd"));  // sin tarjeta no ofrece «sd»
    // La memoria del chip queda para imágenes: los sonidos solo van a la SD.
    usb("SONIDO_SUBIR chip 0000abcd 100");
    assert(sent("SONIDO_ERROR 0000abcd sin_memoria"));
    usb("SONIDO_SUBIR sd 0000abce 10");
    assert(sent("SONIDO_ERROR 0000abce sin_sd"));
    usb("SONIDOS?");
    assert(sent("SONIDOS chip=- sd=-"));
    sim::sdCard = true;
    sndStore.begin();
    usb("HELLO");
    assert(sent(",sonido,sd"));
    assert(sim::codecRegs[0x46] == audio::volumeReg(80) && sim::i2sRate == 16000 && sim::codecWrites > 60);
    // WAV de 0,1 s a 22 050 Hz con un valor fijo (se reconoce al sonar).
    std::vector<uint8_t> wav(44 + 2205 * 2);
    auto u32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) wav[at + i] = (uint8_t)(v >> (8 * i)); };
    memcpy(&wav[0], "RIFF", 4); u32(4, (uint32_t)wav.size() - 8); memcpy(&wav[8], "WAVEfmt ", 8);
    u32(16, 16); u32(20, 1 | (1u << 16)); u32(24, 22050); u32(28, 44100); u32(32, 2 | (16u << 16));
    memcpy(&wav[36], "data", 4); u32(40, 2205 * 2);
    for (size_t i = 44; i < wav.size(); i += 2) { wav[i] = 0x40; wav[i + 1] = 0x1F; }  // 8000
    static const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto b64 = [&](const uint8_t* d, size_t n) {
      std::string o;
      for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        o += A[(v >> 18) & 63]; o += A[(v >> 12) & 63];
        o += i + 1 < n ? A[(v >> 6) & 63] : '='; o += i + 2 < n ? A[v & 63] : '=';
      }
      return o;
    };
    char l[64];
    snprintf(l, sizeof(l), "SONIDO_SUBIR sd 0000abcd %zu", wav.size());
    usb(l);
    assert(sent("SONIDO_LISTO 0000abcd"));
    for (size_t i = 0; i < wav.size(); i += 192) {
      size_t n = std::min<size_t>(192, wav.size() - i);
      usb(("SONIDO_DATOS " + b64(&wav[i], n)).c_str());
      assert(sent(("SONIDO_OK " + std::to_string(i + n)).c_str()));
    }
    snprintf(l, sizeof(l), "SONIDO_FIN %08lx", (unsigned long)mfv3snd::crc32Update(0, wav.data(), wav.size()));
    usb(l);
    assert(sent("SONIDO_GUARDADO 0000abcd") && SD_MMC.disk.files["/sonidos/00/0000abcd.wav"] == wav);
    assert(!SD_MMC.disk.files.count("/sonidos/subida.tmp") && FFat.disk.files.empty());
    usb("SONIDOS?");
    assert(sent("SONIDOS chip=- sd=8387584 0000abcd@s"));

    // Momentos: el gol con el sonido propio, el gol anulado sin nada. Se guardan en la NVS.
    usb("MOMENTO goal 0000abcd");
    usb("MOMENTO goalAnnulled no");
    assert(sent("MOMENTO_OK goal") && sent("MOMENTO_OK goalAnnulled") && sim::nvs.count("snd"));

    // Partido: «empieza el partido» con el de siempre; el gol con el propio (0,1 s a 16 kHz ≈ 1600 muestras de 8000).
    sim::i2sRoom = 1000000;
    run(2000);  // termina de sonar lo que quedaba de antes
    toConfig();
    sim::heard.clear();
    tap(400, 443);  // CONTINUAR
    tap(400, 240);  // saltar la cuenta atrás
    run(500);
    assert(sim::heard.size() >= 4700 && sim::heard.size() <= 4900);  // 300 ms de nota
    run(3000);
    sim::heard.clear();
    usb("GB");
    run(100);
    assert(sim::heard.size() >= 1980 && sim::heard.size() <= 2010); // 1600 muestras + silencio hasta los 125 ms del DMA inicial
    assert(sim::heard[800] == 8000);
    assert(sim::exioOut & (1u << exio::SPEAKER_AMP));  // amplificadores encendidos al sonar
    sim::heard.clear();
    usb("AB");  // anulado: «ninguno»
    run(100);
    assert(sim::heard.empty() && eng.score().white == 0);
    run(1000);
    assert(!(sim::exioOut & (1u << exio::SPEAKER_AMP)));  // y apagados en reposo

    // La gestión de sonidos requiere reposo, nunca interrumpe un partido local.
    usb("SONIDO 0000abcd");assert(sent("SONIDO_ERROR - ocupado"));
    tap(400, 314); tap(400, 376); tap(588, 284);assert(!inMatch);
    // La app prueba un sonido y cambia el volumen en reposo.
    sim::heard.clear();
    usb("SONIDO 0000abcd");
    run(50);
    assert(sim::heard.size() >= 1580);
    usb("VOLUMEN 50");
    assert(sim::codecRegs[0x46] == audio::volumeReg(50) && sim::nvs.count("vol"));

    // Borrado: el gol vuelve al de siempre (660 ms de notas).
    usb("SONIDO_BORRAR 0000abcd");
    assert(sent("SONIDO_BORRADO 0000abcd") && !SD_MMC.disk.files.count("/sonidos/0000abcd.wav") && !SD_MMC.disk.files.count("/sonidos/00/0000abcd.wav"));
    run(3000);
    sim::heard.clear();
    toConfig();tap(400, 443);tap(400, 240);run(3500);sim::heard.clear();
    usb("GA");
    run(100);
    assert(sim::heard.size() > 10000 && sim::heard.size() < 10700);
    tap(400, 314); tap(400, 376); tap(588, 284);  // pausa y abandonar
    assert(!inMatch);

    // Otro sonido a la tarjeta: se lista y suena desde allí.
    snprintf(l, sizeof(l), "SONIDO_SUBIR sd 0000beef %zu", wav.size());
    usb(l);
    for (size_t i = 0; i < wav.size(); i += 192) usb(("SONIDO_DATOS " + b64(&wav[i], std::min<size_t>(192, wav.size() - i))).c_str());
    snprintf(l, sizeof(l), "SONIDO_FIN %08lx", (unsigned long)mfv3snd::crc32Update(0, wav.data(), wav.size()));
    usb(l);
    assert(sent("SONIDO_GUARDADO 0000beef") && SD_MMC.disk.files.count("/sonidos/00/0000beef.wav"));
    usb("SONIDOS?");
    assert(sent(" 0000beef@s"));
    sim::heard.clear();
    usb("SONIDO 0000beef");
    run(50);
    assert(sim::heard.size() >= 1580 && sim::heard[800] == 8000);

    // Al reiniciar se recuerdan los momentos y el volumen.
    sndChoices = mfv3snd::Choices();
    sndVolume = 0;
    loadPrefs();
    assert(sndChoices.c[(int)mfv3snd::Event::GoalAnnulled].kind == mfv3snd::Kind::None && sndVolume == 50);
  }
#endif

#if HAS_RTC
  // ---- Reloj con pila y batería (Waveshare 7C).
  {
    // Fechas guardadas como minutos desde 2000: ida y vuelta, también en años bisiestos.
    for (reloj::Clock c : {reloj::Clock{2026, 10, 17, 18, 42, 0}, reloj::Clock{2028, 2, 29, 23, 59, 0},
                           reloj::Clock{2100 - 1, 12, 31, 0, 0, 0}, reloj::Clock{2024, 1, 1, 0, 0, 0}}) {
      reloj::Clock b = reloj::fromStamp(reloj::stamp(c));
      assert(b.year == c.year && b.month == c.month && b.day == c.day && b.hour == c.hour && b.minute == c.minute);
    }
    assert(reloj::weekday(reloj::Clock{2026, 10, 17, 0, 0, 0}) == 6);  // sábado
    // De fábrica el reloj está parado: Inicio no enseña hora y los resultados se guardan sin fecha.
    assert(!inMatch && screen == Screen::Home && !rtc.clockOk && sent("salidas,reloj,brillo"));
    // La app la pone por el USB (las fechas imposibles no valen).
    usb("HORA 2026-02-30 10:00:00");
    assert(sent("HORA_ERROR") && !rtc.clockOk);
    usb("HORA 2026-10-17 18:42:05");
    assert(sent("HORA_OK") && rtc.clockOk);
    assert(sim::rtcRegs[4] == 0x05 && sim::rtcRegs[5] == 0x42 && sim::rtcRegs[6] == 0x18 && sim::rtcRegs[7] == 0x17 &&
           sim::rtcRegs[8] == 6 && sim::rtcRegs[9] == 0x10 && sim::rtcRegs[10] == 0x26);
    run(1100);
    assert(rtc.clockOk && rtc.now.hour == 18 && rtc.now.minute == 42);  // Inicio no enseña la hora (como la app)
    // Batería al 12 % sin cargar: aviso «BATERÍA BAJA» en Inicio (se relee cada 10 s).
    assert(!rtc.bat.present);
    sim::gaugeRegs[0x0A] = 0x0008; sim::gaugeRegs[0x2C] = 12; sim::gaugeRegs[0x08] = 3550; sim::gaugeRegs[0x0C] = (uint16_t)-300;
    run(10100);
    assert(rtc.bat.present && rtc.bat.percent == 12 && rtc.lowBattery() && shows("BATERÍA BAJA 12 %"));
    // Cargando: deja de avisar.
    sim::gaugeRegs[0x0C] = 800;
    run(10100);
    assert(rtc.bat.charging && !rtc.lowBattery() && !shows("BATERÍA BAJA"));
    // El partido de antes se guardó sin hora; uno nuevo lleva su fecha.
    assert(historyCount == 1 && history[0].at == 0);
    LastResult before = history[0];
    saveResult();
    assert(history[0].at == reloj::stamp(rtc.now) && sim::nvs.count("hist2"));
    history[0] = before;  // se deja el historial como estaba para lo que sigue
    historyCount = 1;
    matchesPlayed = 1;
    prefs.putBytes("hist2", history, sizeof(LastResult));
    prefs.putULong("played", 1);
    // Reposo: reloj grande y fecha; el primer toque despierta y vuelve a Inicio.
    sim::gaugeRegs[0x0C] = (uint16_t)-300;
    run(SLEEP_AFTER_MS + 11000);
    assert(dimmed && reposo::active() && shows("Sábado, 17 de octubre") && shows("BATERÍA BAJA: CONECTA EL CARGADOR"));
    tap(400, 240);
    assert(!dimmed && !inMatch && inicio::active());
    // Hora de Internet al conectarse al Wi-Fi (zona horaria de España).
    sim::ntpOk = true;
    sim::ntpTime = {};
    sim::ntpTime.tm_year = 2027 - 1900; sim::ntpTime.tm_mon = 0; sim::ntpTime.tm_mday = 2;
    sim::ntpTime.tm_hour = 9; sim::ntpTime.tm_min = 5; sim::ntpTime.tm_sec = 30;
    assert(rtc.syncInternet() && sim::ntpTz == reloj::TZ_SPAIN && sim::rtcRegs[10] == 0x27 && sim::rtcRegs[6] == 0x09);
    sim::ntpOk = false;
    sim::gaugeRegs[0x0A] = 0;  // batería quitada
    run(10100);
    assert(!rtc.bat.present && !rtc.lowBattery());
  }
#endif

  // ---- AJUSTES → WI-FI con teclado, versiones publicadas y actualización (pantalla y mando por radio).
  {
    const std::string base = "https://altocu87.github.io/MARCADOR-FUTBOLIN-CLAUDE/firmware/";
    std::string screenBin(150000, 'P'), mandoBin(9000, 'M');
    for (size_t i = 0; i < mandoBin.size(); i++) mandoBin[i] = (char)(i * 13);
    auto json = [](uint32_t build, const std::string& bin) {
      std::vector<uint8_t> v(bin.begin(), bin.end());
      return "{\"build\": " + std::to_string(build) + ", \"size\": " + std::to_string(bin.size()) + ", \"md5\": \"" + sim::fakeMd5(v) + "\"}";
    };
    sim::web[base + S3_BOARD_ID "/ota.json"] = json(60, screenBin);
    sim::web[base + S3_BOARD_ID "/" S3_BOARD_ID "-ota.bin"] = screenBin;
    sim::web[base + "mando-c3/ota.json"] = json(61, mandoBin);
    sim::web[base + "mando-c3/mando-c3-ota.bin"] = mandoBin;
    sim::networks = {{"CasaPepe", "Clave#1", -50}, {"Vecino", "x", -82}, {"Bar abierto", "", -70}};
    assert(!inMatch && screen == Screen::Home);
    tap(750, 46);  // engranaje → AJUSTES
    tapBtn(ui::SETTINGS[1].r);  // WI-FI: se dibuja «buscando» y, en la vuelta siguiente, busca y enseña la lista
    run(300);                   // los botones del menú actúan al levantar el dedo
    assert(sim::scans == 1 && netCount == 3 && shows("CasaPepe") && shows("abierta") && sim::wifiChannel == mfv3::RADIO_CHANNEL);
    tapBtn(ui::WIFI[1].r);  // CasaPepe → teclado
    assert(screen == Screen::Keyboard && shows("Clave de CasaPepe") && shows("CONECTAR"));
    // Clave equivocada: «No conecta» y se puede corregir.
    for (char c : std::string("clave")) tapChar(c);
    assert(!strcmp(kbText, "clave") && shows("*****") && !shows("clave|"));
    tapBtn(ui::KEYBOARD[1].r);  // VER
    assert(shows("clave"));
    tapBtn(ui::KEYBOARD[6].r);  // CONECTAR
    assert(screen == Screen::Connecting && shows("Conectando a CasaPepe"));
    run(2600);
    assert(screen == Screen::Keyboard && shows("No conecta") && !wifiSsid[0] && sim::wifiChannel == mfv3::RADIO_CHANNEL);
    for (int i = 0; i < 5; i++) tapBtn(ui::KEYBOARD[2].r);  // BORRAR
    assert(kbText[0] == 0);
    tapBtn(ui::KEYBOARD[3].r); tapChar('C'); tapBtn(ui::KEYBOARD[3].r);  // MAYUS solo para la C
    for (char c : std::string("lave")) tapChar(c);
    tapBtn(ui::KEYBOARD[4].r); tapChar('#'); tapBtn(ui::KEYBOARD[4].r);  // símbolos
    tapChar('1');
    assert(!strcmp(kbText, "Clave#1"));
    tapBtn(ui::KEYBOARD[6].r);
    run(400);
    // Conecta, guarda la red, mira las versiones y se desconecta (la radio vuelve a su canal).
    assert(screen == Screen::Update && !strcmp(wifiSsid, "CasaPepe") && sim::nvs.count("wssid") && sim::nvs.count("wpass"));
    // Del mando se mira lo publicado de cada modelo; aún no se sabe cuál es este (no ha dicho nada).
    assert(checkState == CheckState::Ok && pubScreen.build == 60 && pubMandoBy[mfv3::ota::MODEL_SUPER_MINI].build == 61 && !sim::wifiUp);
    assert(!pubMando.ok() && !pubMandoBy[mfv3::ota::MODEL_C3_ZERO].ok());
    assert(sim::wifiChannel == mfv3::RADIO_CHANNEL && !sim::insecure);
    assert(shows("PANTALLA") && shows("Disponible: nueva actualización") && shows("CONTINUAR"));
    // El mando dice su versión (10): también hay nueva para él. En Inicio sale ACTUALIZAR.
    mandoInfo(10);
    assert(mandoBuild == 10 && mandoModel() == mfv3::ota::MODEL_SUPER_MINI && pubMando.build == 61);
    assert(mandoUpd() == MandoUpd::Outdated && sim::nvs.count("mmod"));
    loop();
    assert(shows("Hay una actualización disponible"));
    tapBtn(ui::BACK);
    tapBtn(ui::BACK);
    assert(screen == Screen::Home && inicio::dotShown());  // punto dorado en el engranaje
    tap(750, 46);                                         // AJUSTES → ACTUALIZAR
    tapBtn(ui::SETTINGS[2].r);
    assert(screen == Screen::Update);
    tapBtn(ui::UPDATE[2].r);  // revisar los pasos antes de instalar
    assert(screen == Screen::Update && updateTab == UpdateTab::Confirm);
    size_t seenRadio = sim::radioSent.size();  // escuchar tambien el primer OFFER
    tapBtn(ui::UPDATE[2].r);  // INSTALAR AHORA
    assert(screen == Screen::Install && inst == Inst::MandoConnect && shows("Conectando al Wi-Fi"));
    run(400);
    loop();
    loop();
    // Descargado a la mitad libre de la memoria; luego, por radio al mando (que aún no está armado).
    assert(inst == Inst::MandoRadio && !sim::wifiUp);
    std::vector<uint8_t> mandoBytes(mandoBin.begin(), mandoBin.end());
    assert(std::equal(mandoBytes.begin(), mandoBytes.end(), sim::otaSlotData.begin()));
    MemFlash flash;
    mfv3::ota::Receiver rx(GRUPO_MESA, 1200000);
    auto mandoListens = [&](uint32_t ms) {
      uint8_t reply[mfv3::ota::MAX_PACKET];
      for (uint32_t t = 0; t < ms && inst == Inst::MandoRadio; t += 10) {
        loop();
        for (; seenRadio < sim::radioSent.size(); seenRadio++) {
          auto& p = sim::radioSent[seenRadio];
          uint8_t n = rx.handle(p.data(), (int)p.size(), sim::now, flash, reply);
          if (n) sim::radioDeliver(MANDO_MAC, std::vector<uint8_t>(reply, reply + n));
        }
      }
    };
    mandoListens(1500);
    assert(sender.waitingButtons() && shows("Mantén pulsados los dos botones") && shows("SALTAR EL MANDO"));
    rx.arm(sim::now);  // alguien mantiene los dos botones 3 s
    mandoListens(120000);
    assert(flash.finished && flash.data == mandoBytes);
    assert(inst == Inst::MandoConfirm);
    mandoInfo(61);  // el mando reinicia con la versión nueva y lo dice
    loop();
    // Después, la pantalla: conecta, se actualiza (con certificado) y reinicia.
    assert(inst == Inst::ScreenConnect);
    run(400);
    loop();
    loop();
    assert(sim::selfUpdatedFrom == base + S3_BOARD_ID "/" S3_BOARD_ID "-ota.bin" && inst == Inst::Restart);
    assert(shows("Listo. Reiniciando") && !sim::insecure && sim::restarts == 0);
    run(1600);
    assert(sim::restarts >= 1);

    // Al encender: comprobación sin bloquear con la red guardada. Si empieza un partido, se deja.
    sim::restarts = 0;
    screen = Screen::Home;
    inst = Inst::Done;
    pubScreen = mfv3::ota::Published();
    checkState = CheckState::Never;
    startCheck();
    assert(checkState == CheckState::Running);
    run(500);
    assert(checkState == CheckState::Ok && pubScreen.build == 60 && !sim::wifiUp);
    checkState = CheckState::Never;
    loop();
    toConfig();
    startCheck();
    tap(400, 443);  // CONTINUAR antes de que conecte
    run(500);
    assert(inMatch && checkState == CheckState::Never && !sim::wifiUp);
    tap(400, 240); tap(400, 314); tap(400, 376); tap(588, 284);  // saltar cuenta atrás, pausa y abandonar
    assert(!inMatch);
    // Sin internet: lo dice y no inventa versiones.
    sim::webDown = true;
    checkState = CheckState::Never;
    loop();
    tap(750, 46);               // engranaje → AJUSTES
    tapBtn(ui::SETTINGS[2].r);  // ACTUALIZAR (busca solo)
    run(500);
    if (!(screen == Screen::Update && checkState == CheckState::Failed && shows("No se pudo consultar."))) {
      fprintf(stderr, "Consulta sin Internet: screen=%u check=%u link=%u running=%d done=%d match=%d dimmed=%d\n%s\n",
              (unsigned)screen, (unsigned)checkState, (unsigned)s3net::link, (int)versionsRunning,
              (int)versionsDone.load(), (int)inMatch, (int)dimmed, screenText().c_str());
    }
    assert(screen == Screen::Update && checkState == CheckState::Failed && shows("No se pudo consultar."));
    sim::webDown = false;
    // Olvidar la red.
    tapBtn(ui::BACK);
    tapBtn(ui::SETTINGS[1].r);
    loop();
    loop();
    tapBtn(ui::WIFI[7].r);
    assert(!wifiSsid[0]);
    tapBtn(ui::BACK);
    tapBtn(ui::BACK);
    assert(screen == Screen::Home);
  }

  // Reinicio: la configuración y el historial salen de la NVS.
  uint8_t savedGoals = cfg.goalsPerPeriod, savedHistory = historyCount;
  uint32_t savedPlayed = matchesPlayed;
  assert(savedPlayed == 1 && savedHistory == savedPlayed);
  cfg = mfv3::Config(); cfg.goalsPerPeriod = 9; historyCount = 0; matchesPlayed = 0;
  loadPrefs();
  assert(cfg.goalsPerPeriod == savedGoals && historyCount == savedHistory && matchesPlayed == savedPlayed);

  std::printf("esp32s3_pantalla7.ino (placa %d): partido simulado con táctil y protocolo OK\n", MFV3_BOARD);
}
