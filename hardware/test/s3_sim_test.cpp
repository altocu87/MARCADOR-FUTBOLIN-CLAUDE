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

#if HAS_AUDIO
// WAV PCM mínimo de 16 bits (para la tarjeta SD simulada).
static std::vector<uint8_t> wav16(uint32_t rate, uint32_t frames) {
  std::vector<uint8_t> v;
  auto u32 = [&](uint32_t x) { for (int i = 0; i < 4; i++) v.push_back((uint8_t)(x >> (8 * i))); };
  auto u16 = [&](uint16_t x) { v.push_back((uint8_t)x); v.push_back((uint8_t)(x >> 8)); };
  auto tag = [&](const char* t) { v.insert(v.end(), t, t + 4); };
  tag("RIFF"); u32(36 + frames * 2); tag("WAVE");
  tag("fmt "); u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
  tag("data"); u32(frames * 2);
  for (uint32_t i = 0; i < frames; i++) { v.push_back(0); v.push_back(i % 2 ? 0x40 : 0xC0); }
  return v;
}

// Momentos enviados a la tarea de sonido desde la última llamada.
static size_t queued = 0;
static std::vector<s3snd::Ev> newSounds() {
  std::vector<s3snd::Ev> v;
  for (; queued < sim::queueLog.size(); queued++) {
    s3audio::Req r;
    std::memcpy(&r, sim::queueLog[queued].data(), sizeof(r));
    v.push_back((s3snd::Ev)r.ev);
  }
  return v;
}
#endif

int main() {
  sim::now = 1000;
  // Tarjeta SD: un gol propio, «ninguno» en la goleada y un archivo que no es WAV (se ignora).
#if HAS_AUDIO
  sim::sdFiles["/sonidos/gol/bocina.wav"] = wav16(16000, 1600);
  sim::sdFiles["/sonidos/gol/notas.txt"] = {'h', 'o', 'l', 'a'};
  sim::sdFiles["/sonidos/goleada/ninguno"] = {};
#endif
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
  // Altavoz y SD (Waveshare 7C): el códec se configura como el ejemplo del fabricante y el amplificador se enciende.
  {
    const s3audio::Status& au = s3audio::status;
    assert(au.sd && au.codec && au.i2s && au.custom == 2 && au.sdMB == 32768);
    assert(sim::codecRegs[0x46] == 0xBF && sim::codecRegs[0x03] == 0xC3 && (sim::codecRegs[0x69] & 0x20));
    assert((sim::exioOut & (1u << exio::SPEAKER_AMP)) && sent(",altavoz,volumen"));
    assert(s3audio::folders[(int)s3snd::Ev::Goal].count == 1 && s3audio::folders[(int)s3snd::Ev::Rout].silent);
    newSounds();
    sim::runTask();
    while (cfg.goalsPerPeriod > 2) tap(76, 306);
    tap(400, 420);  // JUGAR
    tap(400, 240);  // saltar la cuenta atrás: «empieza el partido», tono de siempre (sin carpeta en la SD)
    auto v = newSounds();
    assert(v.size() == 1 && v[0] == s3snd::Ev::MatchStart);
    size_t before = sim::i2sBytes;
    sim::runTask();
    assert(sim::i2sBytes - before == 22050 * 300 / 1000 / 256 * 256 * 4 + (22050 * 300 / 1000) % 256 * 4 && sim::i2sRate == 22050);
    usb("GB");  // gol: el WAV de la SD (16 kHz, 1600 muestras → 6400 bytes en estéreo)
    v = newSounds();
    assert(v.size() == 2 && v[0] == s3snd::Ev::Goal && v[1] == s3snd::Ev::MatchPoint);
    before = sim::i2sBytes;
    sim::runTask();
    assert(sim::i2sRate == 22050);  // terminó con el tono de la bola de partido
    assert(sim::i2sBytes - before > 6400);
    run(3100);
    usb("GB");  // gana 2-0, goleada: su carpeta tiene «ninguno» → no suena
    v = newSounds();
    assert(eng.phase() == Phase::Finished && v.size() == 1 && v[0] == s3snd::Ev::Rout);
    before = sim::i2sBytes;
    sim::runTask();
    assert(sim::i2sBytes == before);
    // Volumen desde la app, y «SONIDO» para probar un momento.
    usb("VOLUMEN 150");
    assert(s3audio::volume == 100 && sim::nvs.count("vol"));
    usb("SONIDO remontada");
    usb("SONIDO goldenGoal");
    v = newSounds();
    assert(v.size() == 2 && v[0] == s3snd::Ev::Comeback && v[1] == s3snd::Ev::GoldenGoal);
    usb("VOLUMEN 0");
    usb("SONIDO gol");
    before = sim::i2sBytes;
    sim::runTask();
    assert(sim::i2sBytes == before);  // a volumen 0 no se toca nada
    usb("VOLUMEN 80");
    tap(260, 410);  // INICIO
    assert(!inMatch);
  }
#endif

  // Reinicio: la configuración y el historial salen de la NVS.
  uint8_t savedGoals = cfg.goalsPerPeriod, savedHistory = historyCount;
  uint32_t savedPlayed = matchesPlayed;
  assert(savedPlayed == (HAS_AUDIO ? 2 : 1) && savedHistory == savedPlayed);
  cfg = mfv3::Config(); cfg.goalsPerPeriod = 9; historyCount = 0; matchesPlayed = 0;
  loadPrefs();
  assert(cfg.goalsPerPeriod == savedGoals && historyCount == savedHistory && matchesPlayed == savedPlayed);

  std::printf("esp32s3_pantalla7.ino (placa %d): partido simulado con táctil y protocolo OK\n", MFV3_BOARD);
}
