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

  // Reinicio: la configuración y el historial salen de la NVS.
  uint8_t savedGoals = cfg.goalsPerPeriod;
  cfg = mfv3::Config(); cfg.goalsPerPeriod = 9; historyCount = 0; matchesPlayed = 0;
  loadPrefs();
  assert(cfg.goalsPerPeriod == savedGoals && historyCount == 1 && matchesPlayed == 1);

  std::printf("esp32s3_pantalla7.ino (placa %d): partido simulado con táctil y protocolo OK\n", MFV3_BOARD);
}
