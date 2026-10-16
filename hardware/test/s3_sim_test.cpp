// Comprobación del sketch de la pantalla de 7" (ESP32-S3) en el PC con LovyanGFX/NVS simulados:
// se juega un partido completo con toques y con líneas del protocolo MFV3 (USB y UART auxiliar).
#include <cassert>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static void run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10) loop(); }
static void tap(int x, int y) {
  sim::touchX = x; sim::touchY = y; sim::touchDown = true; loop();
  sim::touchDown = false; loop();
}
static void usb(const char* s) { for (const char* c = s; *c; c++) sim::in.push_back(*c); sim::in.push_back('\n'); loop(); }
// Placas sin UART auxiliar (Waveshare 7C): las mismas líneas llegan por el USB.
static void aux(const char* s) {
  if (AUX_UART_RX < 0) { usb(s); return; }
  for (const char* c = s; *c; c++) sim::auxIn.push_back(*c);
  sim::auxIn.push_back('\n');
  loop();
}
static bool shows(const char* s) { return sim::screen.find(s) != std::string::npos; }
static bool sent(const char* s) { return sim::out.find(s) != std::string::npos; }
static size_t count(const std::string& h, const char* s) {
  size_t n = 0;
  for (size_t p = h.find(s); p != std::string::npos; p = h.find(s, p + 1)) n++;
  return n;
}

int main() {
  sim::now = 1000;
  setup();
  loop();
  assert(sent("HELLO " S3_BOARD_ID " 1.0 caps=goles,anular,pausa,pantalla") && sent("STATE idle"));
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
  assert(shows("MARCADOR FUTBOLIN V3") && shows("JUGAR") && shows("PRUEBA"));
  tap(100, 428);                   // botón PRUEBA de inicio: vuelve a la prueba
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
  for (int i = 0; i < 3; i++) tap(76, 306);
  assert(cfg.goalsPerPeriod == 2 && shows("GOLES PARA GANAR"));
  tap(400, 420);  // JUGAR
  assert(inMatch && eng.phase() == Phase::Countdown && shows("TOCA PARA SALTAR"));
  assert(sim::nvs.count("goals"));

  usb("SALTAR");
  assert(eng.phase() == Phase::Playing && shows("BLANCO") && shows("AZUL") && shows("PARTIDO"));

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
  assert(eng.phase() == Phase::Paused && shows("CONTINUAR"));
  tap(400, 260);
  assert(eng.phase() == Phase::Playing);
  tap(160, 200);  // panel BLANCO: llega a 2 y gana
  assert(eng.phase() == Phase::Finished && sent("WIN BLANCO"));
  run(1000);
  assert(shows("VICTORIA BLANCO"));
  assert(matchesPlayed == 1 && historyCount == 1 && history[0].white == 2);

  // Revancha y abandono con doble confirmación.
  tap(530, 410);
  assert(eng.phase() == Phase::Countdown && eng.score().white == 0);
  tap(400, 240);
  tap(400, 430);  // PAUSA
  assert(eng.phase() == Phase::Paused);
  tap(400, 360);
  assert(inMatch && shows("Toca otra vez ABANDONAR"));
  tap(400, 360);
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
  for (int i = 0; i < 4; i++) tap(326, 306);  // 5 goles por parte
  tap(400, 420);  // JUGAR
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
  tap(400, 430); tap(400, 360); tap(400, 360);  // pausa y abandonar
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
    assert(shows("MANDO CONECTADO"));
    while (cfg.goalsPerPeriod < 5) tap(326, 306);
    tap(400, 420);                        // JUGAR
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
    tap(400, 430); tap(400, 360); tap(400, 360);  // pausa y abandonar
    assert(!inMatch);
  }

#if HAS_AUDIO
  // Sonidos propios (Waveshare 7C): altavoces, memoria del chip, órdenes de la app y momentos del partido.
  {
    assert(player.ok && sent(",brillo,sonido") && !sent(",sd"));  // sin tarjeta no ofrece «sd»
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
    snprintf(l, sizeof(l), "SONIDO_SUBIR chip 0000abcd %zu", wav.size());
    usb(l);
    assert(sent("SONIDO_LISTO 0000abcd"));
    for (size_t i = 0; i < wav.size(); i += 192) {
      size_t n = std::min<size_t>(192, wav.size() - i);
      usb(("SONIDO_DATOS " + b64(&wav[i], n)).c_str());
      assert(sent(("SONIDO_OK " + std::to_string(i + n)).c_str()));
    }
    snprintf(l, sizeof(l), "SONIDO_FIN %08lx", (unsigned long)mfv3snd::crc32Update(0, wav.data(), wav.size()));
    usb(l);
    assert(sent("SONIDO_GUARDADO 0000abcd") && FFat.disk.files["/sonidos/0000abcd.wav"] == wav);
    usb("SONIDOS?");
    assert(sent("SONIDOS chip=9216 sd=- 0000abcd@c"));
    usb("SONIDO_SUBIR sd 0000abce 10");
    assert(sent("SONIDO_ERROR 0000abce sin_sd"));

    // Momentos: el gol con el sonido propio, el gol anulado sin nada. Se guardan en la NVS.
    usb("MOMENTO goal 0000abcd");
    usb("MOMENTO goalAnnulled no");
    assert(sent("MOMENTO_OK goal") && sent("MOMENTO_OK goalAnnulled") && sim::nvs.count("snd"));

    // Partido: «empieza el partido» con el de siempre; el gol con el propio (0,1 s a 16 kHz ≈ 1600 muestras de 8000).
    sim::i2sRoom = 1000000;
    run(2000);  // termina de sonar lo que quedaba de antes
    sim::heard.clear();
    tap(400, 420);  // JUGAR
    tap(400, 240);  // saltar la cuenta atrás
    run(500);
    assert(sim::heard.size() >= 4700 && sim::heard.size() <= 4900);  // 300 ms de nota
    run(3000);
    sim::heard.clear();
    usb("GB");
    run(100);
    assert(sim::heard.size() >= 1580 && sim::heard.size() <= 1610);
    assert(sim::heard[800] == 8000);
    assert(sim::exioOut & (1u << exio::SPEAKER_AMP));  // amplificadores encendidos al sonar
    sim::heard.clear();
    usb("AB");  // anulado: «ninguno»
    run(100);
    assert(sim::heard.empty() && eng.score().white == 0);
    run(1000);
    assert(!(sim::exioOut & (1u << exio::SPEAKER_AMP)));  // y apagados en reposo

    // La app toca un sonido de la placa (en un partido de la app) y cambia el volumen.
    sim::heard.clear();
    usb("SONIDO 0000abcd");
    run(50);
    assert(sim::heard.size() >= 1580);
    usb("VOLUMEN 50");
    assert(sim::codecRegs[0x46] == audio::volumeReg(50) && sim::nvs.count("vol"));

    // Borrado: el gol vuelve al de siempre (660 ms de notas).
    usb("SONIDO_BORRAR 0000abcd");
    assert(sent("SONIDO_BORRADO 0000abcd") && !FFat.disk.files.count("/sonidos/0000abcd.wav"));
    run(3000);
    sim::heard.clear();
    usb("GA");
    run(100);
    assert(sim::heard.size() > 10000 && sim::heard.size() < 10700);
    tap(400, 430); tap(400, 360); tap(400, 360);  // pausa y abandonar
    assert(!inMatch);

    // Con tarjeta SD: se anuncia «sd» y los sonidos pueden ir a la tarjeta.
    sim::sdCard = true;
    sndStore.begin();
    usb("HELLO");
    assert(sent(",sonido,sd"));
    snprintf(l, sizeof(l), "SONIDO_SUBIR sd 0000beef %zu", wav.size());
    usb(l);
    for (size_t i = 0; i < wav.size(); i += 192) usb(("SONIDO_DATOS " + b64(&wav[i], std::min<size_t>(192, wav.size() - i))).c_str());
    snprintf(l, sizeof(l), "SONIDO_FIN %08lx", (unsigned long)mfv3snd::crc32Update(0, wav.data(), wav.size()));
    usb(l);
    assert(sent("SONIDO_GUARDADO 0000beef") && SD_MMC.disk.files.count("/sonidos/0000beef.wav") && !SD_MMC.disk.files.count("/sonidos/subida.tmp"));
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

  // Reinicio: la configuración y el historial salen de la NVS.
  uint8_t savedGoals = cfg.goalsPerPeriod;
  cfg = mfv3::Config(); cfg.goalsPerPeriod = 9; historyCount = 0; matchesPlayed = 0;
  loadPrefs();
  assert(cfg.goalsPerPeriod == savedGoals && historyCount == 1 && matchesPlayed == 1);

  std::printf("esp32s3_pantalla7.ino (placa %d): partido simulado con táctil y protocolo OK\n", MFV3_BOARD);
}
