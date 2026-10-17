// MARCADOR FUTBOLÍN V3 · versión ACTIVA para placa ESP32-S3 con pantalla táctil de 7" (800×480)
// -----------------------------------------------------------------------------------------------
// El marcador funciona entero en la placa, sin PC, móvil ni Internet:
//   - Inicio: condición POR GOLES / POR TIEMPO / AMBAS, objetivo de goles y minutos, JUGAR.
//   - Partido: el número de cada equipo es el botón de gol, −1, DESHACER, PAUSA, reloj, bloqueo de 3 s,
//     cuenta atrás, 1ª y 2ª parte, prórroga con gol de oro, penaltis y pantalla de victoria.
//   - Mismas reglas que la app (motor mfv3_engine.h, probado en PC con los criterios A01–A13).
//   - Mando inalámbrico (sketch `mando_pulsadores`): 2 pulsadores arcade por radio ESP-NOW; toque corto = gol,
//     toque largo = anular el último gol de ese equipo. Sin Wi-Fi ni router.
//   - Sensores de gol: en la Waveshare 7C, directamente a las entradas aisladas DI0 (Blanco) y DI1 (Azul);
//     las salidas DO0/DO1 se activan 1 s con cada gol (luces, relé). En otras placas, por UART desde un
//     Arduino o ESP32-C3 con el sketch `arduino_usb`. Siempre también por el USB (protocolo MFV3).
//   - Guarda la configuración y los últimos resultados en la memoria interna (NVS).
//   - AJUSTES: Wi-Fi de casa (se elige la red y se escribe la clave en la pantalla), volumen, prueba y
//     ACTUALIZAR: al encender mira si hay versión nueva de la pantalla o del mando; se instala cuando se pulse
//     (el mando, por radio desde la pantalla). Ver s3_ajustes.h y s3_net.h.
//   - Sonidos propios (Waveshare 7C, con sus dos altavoces): cada momento del partido (gol, remontada, victoria…)
//     suena con el sonido elegido en la app, guardado en la memoria del chip o en la tarjeta SD; si no hay ninguno,
//     suena el de siempre. La app los pasa por el USB (ver mfv3_audio.h y mfv3_sound.h).
//
// Antes de compilar: elige tu placa en board_config.h. Bibliotecas: LovyanGFX.
// NOTA: compila en GitHub Actions (núcleo esp32 3.x + LovyanGFX) y la lógica (motor, protocolo, botones)
// se prueba en PC; todavía no se ha probado en la placa real.

#include <Arduino.h>
#include <Preferences.h>
#include <FFat.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_arduino_version.h>
#include "board_config.h"
#include "mfv3_core.h"
#include "mfv3_engine.h"
#include "mfv3_ota.h"
#include "mfv3_radio.h"
#include "s3_net.h"
#include "mfv3_sound.h"
#include "mfv3_audio.h"
#include "s3_reloj.h"
#include "ui_layout.h"
#include "ui_port.h"
#include "ui_base.h"
#include "ui_legacy.h"
#include "ui_inicio.h"
#include "ui_config.h"
#include "ui_partido.h"
#include "ui_penaltis.h"
#include "ui_reposo.h"
#include "ui_victoria.h"
#include "s3_types.h"

using mfv3::Engine;
using mfv3::Phase;
using mfv3::Period;
using mfv3::Team;
using ui::Btn;

static const char* FW_VERSION = "1.0";
// Número de compilación: lo pone GitHub al compilar (-DMFV3_BUILD=…) y sirve para saber si hay versión nueva.
// Compilado a mano vale 0 («versión de prueba»): cualquier versión publicada se ofrece como nueva.
#ifndef MFV3_BUILD
#define MFV3_BUILD 0
#endif
#if MFV3_BOARD == BOARD_WAVESHARE_7C
#define S3_BOARD_ID "waveshare-7c"
#define S3_CAPS "goles,anular,pausa,pantalla,radio,sensores,salidas,reloj,brillo"
#elif MFV3_BOARD == BOARD_ELECROW_7
#define S3_BOARD_ID "elecrow-7"
#define S3_CAPS "goles,anular,pausa,pantalla,radio,uart,brillo"
#elif MFV3_BOARD == BOARD_WAVESHARE_7
#define S3_BOARD_ID "waveshare-7"
#define S3_CAPS "goles,anular,pausa,pantalla,radio,uart,brillo"
#else
#define S3_BOARD_ID "s3-pantalla"
#define S3_CAPS "goles,anular,pausa,pantalla,radio,brillo"
#endif
static const uint8_t GRUPO_MESA = 1;  // MISMO número que en el mando (cámbialo si hay dos mesas cerca)
static const uint32_t SLEEP_AFTER_MS = 5UL * 60UL * 1000UL;  // atenuar pantalla tras 5 min sin uso fuera de partido

LGFX lcd;        // expansor, brillo y bus I2C de la placa (el panel lo lleva ui_port.h con LVGL)
static LegacyGfx gfx;  // pantallas aún sin rehacer: su dibujo antiguo pasa a objetos LVGL (ui_legacy.h)
Engine eng;
mfv3::Config cfg;
Preferences prefs;
HardwareSerial AuxSerial(1);
mfv3::LineBufferN<384> usbRx;  // las líneas de datos de los sonidos son largas
mfv3::LineBuffer auxRx;

// ---------------------------------------------------------------- sonidos
mfv3snd::Choices sndChoices;
mfv3snd::Before sndBefore;
static uint8_t sndVolume = 80;
#if HAS_AUDIO
audio::FsStore sndStore;
audio::Player player;
mfv3snd::Commands sndCmd(sndStore, sndChoices);
#endif

// ---------------------------------------------------------------- colores (RGB565)
static uint16_t C_BG, C_SURFACE, C_CARD, C_LINE, C_TEXT, C_TEXT2, C_ACCENT, C_WHITE_TEAM, C_WHITE_INK, C_BLUE_TEAM,
    C_GOLD, C_DANGER, C_OK;

static void initColors() {
  C_BG = gfx.color565(7, 11, 18);
  C_SURFACE = gfx.color565(9, 19, 36);
  C_CARD = gfx.color565(17, 34, 59);
  C_LINE = gfx.color565(34, 56, 90);
  C_TEXT = gfx.color565(243, 247, 255);
  C_TEXT2 = gfx.color565(166, 183, 207);
  C_ACCENT = gfx.color565(98, 214, 255);
  C_WHITE_TEAM = gfx.color565(238, 246, 255);
  C_WHITE_INK = gfx.color565(11, 22, 38);
  C_BLUE_TEAM = gfx.color565(23, 109, 188);
  C_GOLD = gfx.color565(242, 201, 76);
  C_DANGER = gfx.color565(255, 90, 110);
  C_OK = gfx.color565(61, 220, 151);
}

// ---------------------------------------------------------------- historial breve (NVS)
static LastResult history[8];
static reloj::State rtc;  // hora (RTC con pila) y batería
static uint8_t historyCount = 0;
static uint32_t matchesPlayed = 0;

// Wi-Fi de casa (solo para actualizar): se escribe en AJUSTES → WI-FI.
static char wifiSsid[33] = "", wifiPass[65] = "";

// Brillo elegido desde la app (orden «BRILLO 0–100»); se guarda y se recupera al despertar del reposo.
static uint8_t userBrightness = 100;
static uint8_t backlightLevel() { return (uint8_t)(10 + (uint16_t)userBrightness * 245 / 100); }

static void loadPrefs() {
  prefs.begin("mfv3", false);
  cfg.endCondition = (mfv3::EndCondition)prefs.getUChar("cond", 0);
  cfg.goalsPerPeriod = prefs.getUChar("goals", 5);
  cfg.minutesPerPeriod = prefs.getUChar("mins", 5);
  if (!cfg.valid()) cfg = mfv3::Config();
  matchesPlayed = prefs.getULong("played", 0);
  historyCount = prefs.getBytes("hist2", history, sizeof(history)) / sizeof(LastResult);  // «hist2»: con fecha
  userBrightness = prefs.getUChar("bri", 100);
  if (userBrightness > 100) userBrightness = 100;
  sndVolume = prefs.getUChar("vol", 80);
  if (sndVolume > 100) sndVolume = 100;
  char text[700] = {0};
  size_t n = prefs.getBytes("snd", text, sizeof(text) - 1);
  text[n] = 0;
  sndChoices.parse(text);
  strncpy(wifiSsid, prefs.getString("wssid", "").c_str(), sizeof(wifiSsid) - 1);
  strncpy(wifiPass, prefs.getString("wpass", "").c_str(), sizeof(wifiPass) - 1);
}

#if HAS_AUDIO
static void saveChoices() {
  char text[700];
  size_t n = sndChoices.serialize(text, sizeof(text));
  prefs.putBytes("snd", text, n);
}
#endif

// Toca los momentos de un cambio: el primero ya (corta lo que sonara) y los demás, en cola.
static void playEvents(const mfv3snd::EventList& list) {
#if HAS_AUDIO
  for (uint8_t i = 0; i < list.n; i++) {
    const mfv3snd::Choice& c = sndChoices.c[(uint8_t)list.ev[i]];
    if (c.kind == mfv3snd::Kind::None) continue;
    audio::Item it = {true, 0, mfv3snd::defaultTune(list.ev[i])};
    if (c.kind == mfv3snd::Kind::Custom) {
      // Uno al azar entre los que de verdad están en la placa; si no hay ninguno, el de siempre.
      uint32_t have[4];
      uint8_t k = 0;
      mfv3snd::Place where;
      for (uint8_t j = 0; j < c.n; j++)
        if (sndStore.find(c.codes[j], where)) have[k++] = c.codes[j];
      if (k) it = {false, have[esp_random() % k], mfv3snd::Tune{nullptr, 0}};
    }
    player.play(it, i > 0);
  }
#else
  (void)list;
#endif
}

// El gol de siempre: para probar el altavoz (pantalla de prueba) y oír el volumen elegido (AJUSTES).
static void playTestTone() {
#if HAS_AUDIO
  player.play(audio::Item{true, 0, mfv3snd::defaultTune(mfv3snd::Event::Goal)}, false);
#endif
}

static void saveConfig() {
  prefs.putUChar("cond", (uint8_t)cfg.endCondition);
  prefs.putUChar("goals", cfg.goalsPerPeriod);
  prefs.putUChar("mins", cfg.minutesPerPeriod);
}

static void saveResult() {
  mfv3::Score s = eng.score(), p = eng.penaltyScore();
  LastResult r = {s.white, s.blue, p.white, p.blue, (uint8_t)(eng.winner() == Team::White ? 0 : 1), (uint8_t)eng.reason(),
                  rtc.clockOk ? reloj::stamp(rtc.now) : 0};
  for (int i = 7; i > 0; i--) history[i] = history[i - 1];
  history[0] = r;
  if (historyCount < 8) historyCount++;
  matchesPlayed++;
  prefs.putBytes("hist2", history, historyCount * sizeof(LastResult));
  prefs.putULong("played", matchesPlayed);
}

// ---------------------------------------------------------------- salida hacia placas auxiliares
static void sendLine(const char* line) {
  Serial.print(line);
  Serial.print('\n');
  if (AUX_UART_RX >= 0) {
    AuxSerial.print(line);
    AuxSerial.print('\n');
  }
}

static const char* phaseName(Phase p) {
  switch (p) {
    case Phase::Countdown: return "countdown";
    case Phase::Playing: return "playing";
    case Phase::Paused: return "paused";
    case Phase::PeriodEnd: return "periodEnd";
    case Phase::Penalties: return "penalties";
    case Phase::Finished: return "finished";
  }
  return "idle";
}

// ---------------------------------------------------------------- estado de la interfaz
static bool inMatch = false;       // false = pantalla de inicio
static bool homeCards = true;      // Inicio nuevo (tarjetas de modo, igual que la app) antes de la configuración
static uint8_t chosenMode = 0;     // inicio::QUICK, CHAOS o RANKED
static Screen screen = Screen::Home;  // fuera del partido: Inicio o una de AJUSTES
static uint32_t lastKey = 0xFFFFFFFF;
static uint32_t flashUntil = 0;
static Team flashTeam = Team::White;
static uint32_t buzzerUntil = 0;
static uint32_t lastActivity = 0;
static bool dimmed = false;
static bool confirmAbandon = false;
static int lastCountdownDigit = -1;
static uint32_t lastClockSecond = 0xFFFFFFFF;
static bool lastLockShown = false;

// ---------------------------------------------------------------- pantalla de prueba (primer arranque)
// Comprueba el táctil (4 esquinas), el mando (pulsador BLANCO y AZUL) y los sensores, sin afectar al marcador.
// Sale sola la primera vez que se enciende la placa; después, en AJUSTES → PRUEBA DE LA PLACA.
static TestState test = {};

static bool testCornersOk() { return test.corner[0] && test.corner[1] && test.corner[2] && test.corner[3]; }

static void enterTest() {
  test = TestState();
  test.active = true;
  test.lastX = test.lastY = -1;
  sendLine("STATE test");
}

static void leaveTest();

static void wakeScreen() {
  lastActivity = millis();
  if (dimmed) {
    dimmed = false;
    lcd.backlight(backlightLevel());
  }
}

#if HAS_ISOLATED_IO
// Entradas aisladas: el nivel que tienen al arrancar se toma como «sin balón» (vale para sensores NPN o PNP,
// normalmente abiertos o cerrados). Cualquier cambio respecto a él, estable 25 ms, es un gol.
static mfv3::EdgeTrigger diWhite(25, 400), diBlue(25, 400);
static uint16_t diIdle = 0;
static bool diReady = false;
static uint32_t doOffAt = 0;

static void goalOutput(Team t) {
  lcd.exioWrite(t == Team::White ? exio::DO0 : exio::DO1, true);
  doOffAt = millis() + 1000;
}
#endif

static void beep(uint16_t ms) {
  if (BUZZER_PIN < 0) return;
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerUntil = millis() + ms;
}

// Tras cualquier cambio del motor: avisos a las placas, sonido, guardado.
static Snapshot snap;

static Snapshot takeSnapshot() {
  mfv3::Score s = eng.score();
  return Snapshot{eng.phase(), eng.period(), s.white, s.blue, eng.kickCount(), eng.suddenDeath()};
}

static void afterChange(bool goalAccepted, Team goalTeam) {
  Snapshot now = takeSnapshot();
  char buf[32];
  if (now.phase != snap.phase) {
    snprintf(buf, sizeof(buf), "STATE %s", phaseName(now.phase));
    sendLine(buf);
    // Carteles de la app al empezar cada parte y la tanda de penaltis.
    if (now.phase == Phase::Playing && snap.phase == Phase::Countdown) {
      bool single = eng.config().endCondition == mfv3::EndCondition::Goals;
      partido::banner(now.period == Period::Overtime ? partido::B_PRORROGA
                      : now.period == Period::Second ? partido::B_2_PARTE
                      : !single                      ? partido::B_1_PARTE
                      : chosenMode == inicio::CHAOS  ? partido::B_PARTIDO_LOCO
                                                     : partido::B_PARTIDO);
    }
    if (now.phase == Phase::Penalties && snap.phase != Phase::Penalties) partido::banner(partido::B_PENALTIS);
  }
  if (now.phase == Phase::Penalties && now.sudden && !snap.sudden) partido::banner(partido::B_MUERTE_SUBITA);
  if (goalAccepted) {
    // ¡GOL! a pantalla completa (si el partido sigue: el gol que lo gana va directo a la victoria).
    if (now.phase != Phase::Finished && now.phase != Phase::Penalties) partido::celebrate((uint8_t)goalTeam);
    sendLine(goalTeam == Team::White ? "GOAL BLANCO" : "GOAL AZUL");
    sendLine("LOCK 3000");
    flashTeam = goalTeam;
    flashUntil = millis() + 900;
    beep(90);
#if HAS_ISOLATED_IO
    goalOutput(goalTeam);
#endif
  }
  if (now.w != snap.w || now.b != snap.b) {
    snprintf(buf, sizeof(buf), "SCORE %u %u", now.w, now.b);
    sendLine(buf);
  }
  if (now.phase == Phase::Finished && snap.phase != Phase::Finished) {
    sendLine(eng.winner() == Team::White ? "WIN BLANCO" : "WIN AZUL");
    saveResult();
    beep(400);
  }
  if (now.phase == Phase::PeriodEnd && snap.phase != Phase::PeriodEnd) beep(250);
  snap = now;
  playEvents(mfv3snd::detect(eng, sndBefore, goalAccepted));
  sndBefore = mfv3snd::before(eng);
}

// ---------------------------------------------------------------- acciones
static void startMatch() {
  if (!eng.start(cfg, millis())) return;
  inMatch = true;
  confirmAbandon = false;
  snap = takeSnapshot();
  sndBefore = mfv3snd::before(eng);
  sendLine("STATE countdown");
  sendLine("SCORE 0 0");
  lastKey = 0xFFFFFFFF;
}

static void goHome() {
  inMatch = false;
  homeCards = true;
  screen = Screen::Home;
  confirmAbandon = false;
  sendLine("STATE idle");
  lastKey = 0xFFFFFFFF;
}

static void leaveTest() {
  test.active = false;
  prefs.putBool("tested", true);  // ya no sale sola al encender (sigue en AJUSTES)
  goHome();
}

// Entrada común (táctil, UART, USB): misma lógica que el controlador de la app.
static void inputGoal(Team t) {
  if (!inMatch) return;
  uint32_t now = millis();
  if (eng.phase() == Phase::Countdown) {
    eng.skipCountdown(now);  // la pulsación solo salta la cuenta atrás
    afterChange(false, t);
    return;
  }
  if (eng.phase() == Phase::Penalties) {
    if (eng.penalty(t, true, now) == mfv3::Result::Accepted) afterChange(false, t);
    return;
  }
  mfv3::Result r = eng.goal(t, now);
  afterChange(r == mfv3::Result::Accepted, t);
}

// Pulsación larga del mando: anular el último gol de ese equipo (en penaltis, deshacer el último lanzamiento).
static void inputAnnul(Team t) {
  if (!inMatch) return;
  uint32_t now = millis();
  if (eng.phase() == Phase::Playing || eng.phase() == Phase::Paused) eng.minusOne(t, now);
  else if (eng.phase() == Phase::Penalties) eng.undoPenalty();
  else return;
  afterChange(false, t);
}

static void inputPause() {
  if (!inMatch) return;
  uint32_t now = millis();
  if (eng.phase() == Phase::Playing) eng.pause(now);
  else if (eng.phase() == Phase::Paused) eng.resume(now);
  afterChange(false, Team::White);
}

static void sendHello() {
  char buf[128];
  // «sonido»: tiene altavoz y guarda sonidos; «sd»: además, en la tarjeta SD.
#if HAS_AUDIO
  const char* extra = player.ok ? (sndStore.sdOk ? ",sonido,sd" : ",sonido") : "";
#else
  const char* extra = "";
#endif
  snprintf(buf, sizeof(buf), "HELLO %s %s caps=%s%s", S3_BOARD_ID, FW_VERSION, S3_CAPS, extra);
  sendLine(buf);
}

// Órdenes de sonido de la app (subir, listar, borrar, tocar, momentos, volumen).
static bool handleSoundLine(const char* line) {
#if HAS_AUDIO
  mfv3snd::Action act;
  if (!sndCmd.handle(line, millis(), sendLine, act)) return false;
  if (act.play) {
    mfv3snd::Place where;
    if (sndStore.find(act.code, where)) player.play(audio::Item{false, act.code, mfv3snd::Tune{nullptr, 0}}, act.queue);
  }
  if (act.choicesChanged) saveChoices();
  if (act.volumeChanged) {
    sndVolume = act.volume;
    prefs.putUChar("vol", sndVolume);
    player.setVolume(sndVolume);
  }
  return true;
#else
  (void)line;
  return false;
#endif
}

static void onTap(int16_t x, int16_t y);

// Línea del protocolo MFV3 recibida de una placa auxiliar.
static void handleBoardLine(const char* line) {
  if (test.active) test.usbSeen = true;
  if (handleSoundLine(line)) return;  // sin despertar la pantalla: la app puede estar pasando sonidos
  if (!strcmp(line, "CAPTURA")) { uiport::sendCapture(); return; }  // lo que se ve, por el USB (scripts/placa.py)
  // «TOQUE x y»: un toque simulado desde el PC (para recorrer las pantallas y capturarlas sin tocar la placa).
  if (!strncmp(line, "TOQUE ", 6)) {
    int x = 0, y = 0;
    if (sscanf(line + 6, "%d %d", &x, &y) == 2) {
      uiport::injectTouch((int16_t)x, (int16_t)y);
      sendLine("TOQUE_OK");
    }
    return;
  }
  wakeScreen();
  char word[16] = {0};
  size_t i = 0;
  while (line[i] && line[i] != ' ' && i < sizeof(word) - 1) {
    word[i] = (char)toupper((unsigned char)line[i]);
    i++;
  }
  if (!strcmp(word, "GOL_BLANCO") || !strcmp(word, "GB")) inputGoal(Team::White);
  else if (!strcmp(word, "GOL_AZUL") || !strcmp(word, "GA")) inputGoal(Team::Blue);
  else if (!strcmp(word, "ANULAR_BLANCO") || !strcmp(word, "AB")) inputAnnul(Team::White);
  else if (!strcmp(word, "ANULAR_AZUL") || !strcmp(word, "AA")) inputAnnul(Team::Blue);
  else if (!strcmp(word, "PAUSA")) inputPause();
  else if (!strcmp(word, "SALTAR") && inMatch && eng.phase() == Phase::Countdown) {
    eng.skipCountdown(millis());
    afterChange(false, Team::White);
  } else if (!strcmp(word, "BRILLO")) {
    // Brillo de la pantalla que manda la app (panel rápido): 0–100 %.
    int v = atoi(line + i);
    userBrightness = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    prefs.putUChar("bri", userBrightness);
    if (!dimmed) lcd.backlight(backlightLevel());
  } else if (!strcmp(word, "HORA")) {
    // La app pone en hora la placa al conectarse: «HORA 2026-10-17 18:42:05» (hora local).
    reloj::Clock c;
    bool ok = reloj::parseClock(line + i + (line[i] == ' ' ? 1 : 0), c) && rtc.set(c);
    sendLine(ok ? "HORA_OK" : "HORA_ERROR");
  } else if (!strcmp(word, "PING")) sendLine("PONG");
  else if (!strcmp(word, "HELLO")) {
    sendHello();
  }
}

// ---------------------------------------------------------------- radio (ESP-NOW) desde el mando
// El aviso llega en otra tarea del sistema: solo se valida y se guarda en una cola; loop() lo procesa.
static mfv3::RadioDedup radioDedup;
static volatile uint8_t radioHead = 0, radioTail = 0;
static mfv3::RadioCmd radioQueue[8];
static volatile uint32_t lastRadioAt = 0;
static bool radioOk = false;

// Actualización por radio (mfv3_ota.h): la versión del mando llega con cada saludo; las respuestas del mando
// durante una instalación esperan en una cola a que loop() las atienda.
static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static volatile uint32_t mandoBuild = 0, mandoInfoAt = 0;
static OtaPacket otaInbox[6];
static volatile uint8_t otaHead = 0, otaTail = 0;

static void otaPush(const uint8_t* data, int len) {
  mfv3::ota::Info in = mfv3::ota::decodeInfo(data, len, GRUPO_MESA);
  if (in.ok) {
    uint32_t t = millis();
    mandoBuild = in.build;
    mandoInfoAt = t ? t : 1;  // 0 = «nunca»
    lastRadioAt = t;
    return;
  }
  if (!mfv3::ota::typeOf(data, len, GRUPO_MESA) || len > (int)sizeof(otaInbox[0].data)) return;
  uint8_t next = (uint8_t)((otaHead + 1) % 6);
  if (next == otaTail) return;  // llena: el emisor repetirá
  otaInbox[otaHead].len = (uint8_t)len;
  memcpy(otaInbox[otaHead].data, data, len);
  otaHead = next;
}

static void otaDrain(OtaHandler fn) {
  while (otaTail != otaHead) {
    fn(otaInbox[otaTail].data, otaInbox[otaTail].len);
    otaTail = (uint8_t)((otaTail + 1) % 6);
  }
}

static void radioSend(const uint8_t* data, uint8_t len) { esp_now_send(BROADCAST, data, len); }

static void radioPush(const uint8_t* mac, const uint8_t* data, int len) {
  if (len >= mfv3::ota::HEADER && memcmp(data, "MFVO", 4) == 0) {
    otaPush(data, len);
    return;
  }
  mfv3::RadioMsg m = mfv3::radioDecode(data, len, GRUPO_MESA);
  if (!m.ok || !radioDedup.fresh(mac, m.seq)) return;
  lastRadioAt = millis();
  if (m.cmd == mfv3::RadioCmd::Hello) return;
  uint8_t next = (uint8_t)((radioHead + 1) % 8);
  if (next == radioTail) return;  // cola llena: se pierde (no debería pasar)
  radioQueue[radioHead] = m.cmd;
  radioHead = next;
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void onRadio(const esp_now_recv_info_t* info, const uint8_t* data, int len) { radioPush(info->src_addr, data, len); }
#else
static void onRadio(const uint8_t* mac, const uint8_t* data, int len) { radioPush(mac, data, len); }
#endif

static void radioBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(mfv3::RADIO_CHANNEL, WIFI_SECOND_CHAN_NONE);
  radioOk = esp_now_init() == ESP_OK && esp_now_register_recv_cb(onRadio) == ESP_OK;
  esp_now_peer_info_t peer = {};  // para enviar al mando (actualizaciones)
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = mfv3::RADIO_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
  if (!radioOk) sendLine("LOG radio ESP-NOW no disponible");
}

static void radioPoll() {
  while (radioTail != radioHead) {
    mfv3::RadioCmd c = radioQueue[radioTail];
    radioTail = (uint8_t)((radioTail + 1) % 8);
    if (test.active) {
      if (c == mfv3::RadioCmd::GoalWhite || c == mfv3::RadioCmd::AnnulWhite) test.remoteWhite = true;
      if (c == mfv3::RadioCmd::GoalBlue || c == mfv3::RadioCmd::AnnulBlue) test.remoteBlue = true;
    }
    handleBoardLine(mfv3::radioLine(c));
  }
}

// El mando saluda cada 20 s: «conectado» si se ha oído en el último minuto.
static bool remoteLinked() { return lastRadioAt != 0 && millis() - lastRadioAt < 60000UL; }

// ---------------------------------------------------------------- dibujo
static void text(const char* s, int x, int y, const lgfx::IFont* font, uint16_t color, float size = 1.0f) {
  gfx.setFont(font);
  gfx.setTextSize(size);
  gfx.setTextColor(color);
  gfx.setTextDatum(lgfx::textdatum_t::middle_center);
  gfx.drawString(s, x, y);
}

static void button(const ui::Rect& r, const char* label, uint16_t fill, uint16_t ink, uint16_t border,
                   const lgfx::IFont* font = &lgfx::fonts::FreeSansBold12pt7b) {
  gfx.fillRoundRect(r.x, r.y, r.w, r.h, 12, fill);
  gfx.drawRoundRect(r.x, r.y, r.w, r.h, 12, border);
  text(label, r.x + r.w / 2, r.y + r.h / 2, font, ink);
}

static void drawHeader(const char* left, const char* right, uint16_t rightColor) {
  gfx.fillRect(0, 0, ui::W, 44, C_BG);
  gfx.setFont(&lgfx::fonts::FreeSansBold12pt7b);
  gfx.setTextSize(1);
  gfx.setTextColor(C_TEXT);
  gfx.setTextDatum(lgfx::textdatum_t::middle_left);
  gfx.drawString(left, 14, 24);
  gfx.setTextColor(rightColor);
  gfx.setTextDatum(lgfx::textdatum_t::middle_right);
  gfx.drawString(right, ui::W - 14, 24);
  gfx.drawFastHLine(0, 44, ui::W, C_LINE);
}

#include "s3_ajustes.h"  // AJUSTES: Wi-Fi, actualizar, volumen (usa lo de arriba)

// ---------------------------------------------------------------- hora y batería (Inicio y reposo)
static const char* const DIAS[7] = {"DOMINGO", "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO"};
static const char* const MESES[12] = {"ENERO", "FEBRERO", "MARZO", "ABRIL", "MAYO", "JUNIO",
                                      "JULIO", "AGOSTO", "SEPTIEMBRE", "OCTUBRE", "NOVIEMBRE", "DICIEMBRE"};
static uint32_t lastStatusKey = 0xFFFFFFFF;
static uint32_t lastSleepKey = 0xFFFFFFFF;

// Qué enseña la franja de arriba de Inicio: si cambia, se redibuja solo esa franja.
static uint32_t statusKey() {
  uint32_t k = rtc.clockOk ? (uint32_t)rtc.now.hour * 60 + rtc.now.minute : 0xFFFF;
  if (rtc.bat.present) k |= (uint32_t)(rtc.bat.percent + 1) << 16 | (uint32_t)rtc.bat.charging << 24;
  return k;
}

// Pila dibujada: contorno, borne y relleno según la carga (verde, azul cargando, rojo si queda poca).
static void drawBatteryIcon(int x, int y, uint8_t pct, uint16_t color) {
  gfx.drawRoundRect(x, y, 34, 18, 3, color);
  gfx.fillRect(x + 34, y + 5, 3, 8, color);
  int w = (int)pct * 28 / 100;
  if (w < 2) w = 2;
  gfx.fillRect(x + 3, y + 3, w, 12, color);
}

// Franja de arriba de Inicio: la hora a la izquierda y la batería a la derecha (solo lo que la placa sabe).
static void drawStatus() {
  lastStatusKey = statusKey();
  gfx.fillRect(0, 0, ui::W, 26, C_BG);
  char buf[32];
  if (rtc.clockOk) {
    snprintf(buf, sizeof(buf), "%02u:%02u", rtc.now.hour, rtc.now.minute);
    gfx.setFont(&lgfx::fonts::FreeSansBold12pt7b);
    gfx.setTextSize(1);
    gfx.setTextColor(C_TEXT2);
    gfx.setTextDatum(lgfx::textdatum_t::middle_left);
    gfx.drawString(buf, 14, 14);
  }
  if (rtc.bat.present) {
    uint16_t color = rtc.lowBattery() ? C_DANGER : rtc.bat.charging ? C_ACCENT : C_OK;
    drawBatteryIcon(ui::W - 14 - 37, 5, rtc.bat.percent, color);
    if (rtc.lowBattery()) snprintf(buf, sizeof(buf), "BATERIA BAJA %u%%", rtc.bat.percent);
    else if (rtc.bat.charging) snprintf(buf, sizeof(buf), "CARGANDO %u%%", rtc.bat.percent);
    else snprintf(buf, sizeof(buf), "%u%%", rtc.bat.percent);
    gfx.setFont(&lgfx::fonts::FreeSansBold12pt7b);
    gfx.setTextSize(1);
    gfx.setTextColor(color);
    gfx.setTextDatum(lgfx::textdatum_t::middle_right);
    gfx.drawString(buf, ui::W - 14 - 37 - 10, 14);
  }
}

// Reposo con hora: reloj grande y fecha (la pantalla está atenuada; el primer toque la despierta).
static bool sleepClock() { return dimmed && !inMatch && !test.active && rtc.clockOk; }

static void drawSleep() {
  lastSleepKey = (uint32_t)rtc.now.hour * 60 + rtc.now.minute;
  gfx.fillScreen(C_BG);
  char buf[40];
  snprintf(buf, sizeof(buf), "%02u:%02u", rtc.now.hour, rtc.now.minute);
  text(buf, ui::W / 2, 200, &lgfx::fonts::Font7, C_TEXT, 3.0f);
  snprintf(buf, sizeof(buf), "%s %u DE %s", DIAS[reloj::weekday(rtc.now)], rtc.now.day, MESES[rtc.now.month - 1]);
  text(buf, ui::W / 2, 350, &lgfx::fonts::FreeSansBold18pt7b, C_TEXT2);
  if (rtc.lowBattery()) text("BATERIA BAJA: CONECTA EL CARGADOR", ui::W / 2, 420, &lgfx::fonts::FreeSansBold12pt7b, C_DANGER);
}

static void drawHome() {
  gfx.fillScreen(C_BG);
  drawStatus();
  text("MARCADOR FUTBOLIN V3", ui::W / 2, 54, &lgfx::fonts::FreeSansBold24pt7b, C_ACCENT);
  char sub[96];
  if (historyCount > 0) {
    const LastResult& r = history[0];
    char when[24] = "";
    if (r.at) {
      reloj::Clock c = reloj::fromStamp(r.at);
      snprintf(when, sizeof(when), "  %02u/%02u %02u:%02u", c.day, c.month, c.hour, c.minute);
    }
    snprintf(sub, sizeof(sub), "Ultimo: %u-%u  gana %s%s   |   %lu partidos", r.white, r.blue, r.winner ? "AZUL" : "BLANCO",
             when, (unsigned long)matchesPlayed);
  } else {
    snprintf(sub, sizeof(sub), "Partido rapido en la mesa  |  v%s", FW_VERSION);
  }
  text(sub, ui::W / 2, 102, &lgfx::fonts::FreeSans12pt7b, C_TEXT2);
  text(remoteLinked() ? "MANDO CONECTADO" : "MANDO: SIN SENAL", ui::W / 2, 128, &lgfx::fonts::FreeSansBold9pt7b,
       remoteLinked() ? C_OK : C_LINE);

  const char* conds[3] = {"POR GOLES", "POR TIEMPO", "AMBAS"};
  for (int i = 0; i < 3; i++) {
    bool on = (int)cfg.endCondition == i;
    button(ui::HOME[i].r, conds[i], on ? gfx.color565(20, 60, 85) : C_CARD, C_TEXT, on ? C_ACCENT : C_LINE);
  }
  bool useGoals = cfg.endCondition != mfv3::EndCondition::Time;
  bool useTime = cfg.endCondition != mfv3::EndCondition::Goals;
  char v[24];
  text("GOLES PARA GANAR", 201, 250, &lgfx::fonts::FreeSansBold9pt7b, C_TEXT2);
  text("MINUTOS POR PARTE", 599, 250, &lgfx::fonts::FreeSansBold9pt7b, C_TEXT2);
  button(ui::HOME[3].r, "-", C_CARD, useGoals ? C_TEXT : C_LINE, C_LINE, &lgfx::fonts::FreeSansBold24pt7b);
  button(ui::HOME[4].r, "+", C_CARD, useGoals ? C_TEXT : C_LINE, C_LINE, &lgfx::fonts::FreeSansBold24pt7b);
  snprintf(v, sizeof(v), "%u", cfg.goalsPerPeriod);
  text(v, 201, 306, &lgfx::fonts::FreeSansBold24pt7b, useGoals ? C_TEXT : C_LINE);
  button(ui::HOME[5].r, "-", C_CARD, useTime ? C_TEXT : C_LINE, C_LINE, &lgfx::fonts::FreeSansBold24pt7b);
  button(ui::HOME[6].r, "+", C_CARD, useTime ? C_TEXT : C_LINE, C_LINE, &lgfx::fonts::FreeSansBold24pt7b);
  snprintf(v, sizeof(v), "%u", cfg.minutesPerPeriod);
  text(v, 599, 306, &lgfx::fonts::FreeSansBold24pt7b, useTime ? C_TEXT : C_LINE);
  button(ui::HOME[7].r, "JUGAR", C_ACCENT, C_BG, C_ACCENT, &lgfx::fonts::FreeSansBold24pt7b);
  button(ui::HOME[8].r, "AJUSTES", C_SURFACE, C_TEXT2, C_LINE, &lgfx::fonts::FreeSansBold12pt7b);
  if (updateAvailable()) button(ui::HOME[9].r, "ACTUALIZAR", gfx.color565(60, 50, 12), C_GOLD, C_GOLD, &lgfx::fonts::FreeSansBold12pt7b);
}

static void testLine(int y, const char* label, bool ok, bool optional) {
  gfx.fillRoundRect(130, y - 18, 540, 36, 10, C_SURFACE);
  gfx.fillCircle(156, y, 11, ok ? C_OK : (optional ? C_LINE : C_DANGER));
  gfx.setFont(&lgfx::fonts::FreeSansBold12pt7b);
  gfx.setTextSize(1);
  gfx.setTextColor(ok ? C_TEXT : C_TEXT2);
  gfx.setTextDatum(lgfx::textdatum_t::middle_left);
  gfx.drawString(label, 180, y);
  gfx.setTextDatum(lgfx::textdatum_t::middle_right);
  gfx.setTextColor(ok ? C_OK : C_TEXT2);
  gfx.drawString(ok ? "OK" : (optional ? "opcional" : "pendiente"), 655, y);
}

static void drawTest() {
  gfx.fillScreen(C_BG);
  for (int i = 0; i < 4; i++) {
    const ui::Rect& r = ui::TEST_CORNERS[i];
    gfx.fillRoundRect(r.x + 6, r.y + 6, r.w - 12, r.h - 12, 14, test.corner[i] ? C_OK : C_CARD);
    gfx.drawRoundRect(r.x + 6, r.y + 6, r.w - 12, r.h - 12, 14, test.corner[i] ? C_OK : C_ACCENT);
    text(test.corner[i] ? "OK" : "TOCA", r.x + r.w / 2, r.y + r.h / 2, &lgfx::fonts::FreeSansBold9pt7b, test.corner[i] ? C_BG : C_ACCENT);
  }
  text("PRUEBA DE LA PLACA", ui::W / 2, 34, &lgfx::fonts::FreeSansBold18pt7b, C_ACCENT);
  text("Comprueba cada punto. No afecta al marcador.", ui::W / 2, 70, &lgfx::fonts::FreeSans9pt7b, C_TEXT2);
  testLine(120, "Toca las 4 esquinas", testCornersOk(), false);
  testLine(165, "Mando: pulsa el BLANCO", test.remoteWhite, false);
  testLine(210, "Mando: pulsa el AZUL", test.remoteBlue, false);
#if HAS_ISOLATED_IO
  testLine(255, "Sensor BLANCO (DI0)", test.sensorWhite, true);
  testLine(300, "Sensor AZUL (DI1)", test.sensorBlue, true);
#endif
  char buf[96];
  snprintf(buf, sizeof(buf), "Radio %s  |  Mando %s  |  USB %s", radioOk ? "OK" : "ERROR", remoteLinked() ? "oido" : "sin senal",
           test.usbSeen ? "con datos" : "-");
  text(buf, ui::W / 2, 328, &lgfx::fonts::FreeSans9pt7b, radioOk ? C_TEXT2 : C_DANGER);
#if HAS_AUDIO
  // Cada esquina tocada suena: si se oye, el altavoz funciona.
  snprintf(buf, sizeof(buf), "Altavoz %s  |  Tarjeta SD %s", player.ok ? "OK (suena al tocar)" : "ERROR",
           sndStore.sdOk ? "OK" : "-");
  text(buf, ui::W / 2, 351, &lgfx::fonts::FreeSans9pt7b, player.ok ? C_TEXT2 : C_DANGER);
#endif
  if (test.lastX >= 0) {
    snprintf(buf, sizeof(buf), "Ultimo toque: %d, %d", test.lastX, test.lastY);
    text(buf, ui::W / 2, 374, &lgfx::fonts::FreeSans9pt7b, C_TEXT2);
    gfx.drawLine(test.lastX - 12, test.lastY, test.lastX + 12, test.lastY, C_GOLD);
    gfx.drawLine(test.lastX, test.lastY - 12, test.lastX, test.lastY + 12, C_GOLD);
  }
  bool ready = testCornersOk();
  button(ui::TEST[0].r, "TERMINAR", ready ? C_ACCENT : C_CARD, ready ? C_BG : C_TEXT, ready ? C_ACCENT : C_LINE,
         &lgfx::fonts::FreeSansBold18pt7b);
}

static const char* periodLabel(Period p) {
  switch (p) {
    // Por goles no hay partes: se juega un único periodo.
    case Period::First: return cfg.endCondition == mfv3::EndCondition::Goals ? "PARTIDO" : "1a PARTE";
    case Period::Second: return "2a PARTE";
    case Period::Overtime: return "PRORROGA - GOL DE ORO";
    case Period::Shootout: return "PENALTIS";
  }
  return "";
}

static void drawScorePanel(Team t, bool highlight) {
  const ui::Rect& r = t == Team::White ? ui::PANEL_WHITE : ui::PANEL_BLUE;
  uint16_t fill = t == Team::White ? C_WHITE_TEAM : C_BLUE_TEAM;
  uint16_t ink = t == Team::White ? C_WHITE_INK : C_TEXT;
  gfx.fillRoundRect(r.x, r.y, r.w, r.h, 22, fill);
  uint8_t mp = eng.matchPoint();
  bool matchPoint = eng.period() != Period::Overtime && (mp & (t == Team::White ? 1 : 2));
  uint16_t border = highlight ? C_GOLD : matchPoint ? C_DANGER : (t == Team::White ? C_TEXT2 : C_ACCENT);
  for (int i = 0; i < (highlight || matchPoint ? 5 : 2); i++) gfx.drawRoundRect(r.x - i, r.y - i, r.w + 2 * i, r.h + 2 * i, 22 + i, border);
  text(t == Team::White ? "BLANCO" : "AZUL", r.x + r.w / 2, r.y + 26, &lgfx::fonts::FreeSansBold12pt7b, ink);
  char n[4];
  uint8_t v = eng.score().of(t);
  snprintf(n, sizeof(n), "%u", v);
  text(n, r.x + r.w / 2, r.y + r.h / 2 + 14, &lgfx::fonts::Font8, ink, v >= 10 ? 2.2f : 3.0f);
  if (highlight) text("GOL!", r.x + r.w / 2, r.y + r.h - 34, &lgfx::fonts::FreeSansBold24pt7b, C_GOLD);
  else if (matchPoint) text("BOLA DE PARTIDO", r.x + r.w / 2, r.y + r.h - 28, &lgfx::fonts::FreeSansBold9pt7b, C_DANGER);
}

static void drawClock(uint32_t now) {
  uint32_t ms = eng.clockMs(now);
  uint32_t sec = (ms + (eng.clockCountsDown() ? 999 : 0)) / 1000;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
  gfx.fillRect(ui::CLOCK.x, ui::CLOCK.y, ui::CLOCK.w, ui::CLOCK.h, C_BG);
  bool warn = eng.clockCountsDown() && ms <= 10000 && eng.phase() == Phase::Playing;
  text(buf, ui::CLOCK.x + ui::CLOCK.w / 2, ui::CLOCK.y + ui::CLOCK.h / 2, &lgfx::fonts::Font7, warn ? C_DANGER : C_ACCENT, 0.7f);
  lastClockSecond = sec;
}

static void drawLock(uint32_t now) {
  uint32_t lock = eng.lockRemaining(now);
  gfx.fillRect(ui::LOCK_MSG.x, ui::LOCK_MSG.y, ui::LOCK_MSG.w, ui::LOCK_MSG.h, C_BG);
  if (lock > 0) {
    char buf[24];
    snprintf(buf, sizeof(buf), "BLOQUEO %.1f s", lock / 1000.0f);
    text(buf, ui::LOCK_MSG.x + ui::LOCK_MSG.w / 2, ui::LOCK_MSG.y + 15, &lgfx::fonts::FreeSansBold9pt7b, C_GOLD);
  }
  lastLockShown = lock > 0;
}

static void drawMatch(uint32_t now) {
  gfx.fillScreen(C_BG);
  char right[32];
  snprintf(right, sizeof(right), cfg.endCondition == mfv3::EndCondition::Time ? "%u min" : "%u goles", cfg.endCondition == mfv3::EndCondition::Time ? cfg.minutesPerPeriod : cfg.goalsPerPeriod);
  drawHeader(periodLabel(eng.period()), right, C_TEXT2);
  bool flash = millis() < flashUntil;
  drawScorePanel(Team::White, flash && flashTeam == Team::White);
  drawScorePanel(Team::Blue, flash && flashTeam == Team::Blue);
  text(eng.clockCountsDown() ? "RESTANTE" : "TIEMPO", 400, 56, &lgfx::fonts::FreeSansBold9pt7b, C_TEXT2);
  drawClock(now);
  drawLock(now);
  if (cfg.endCondition != mfv3::EndCondition::Time && eng.period() != Period::Overtime) {
    char buf[24];
    snprintf(buf, sizeof(buf), "A %u goles", cfg.goalsPerPeriod);
    text(buf, 400, 190, &lgfx::fonts::FreeSans9pt7b, C_TEXT2);
  }
  button(ui::MATCH[2].r, "-1", gfx.color565(40, 18, 26), C_DANGER, C_DANGER, &lgfx::fonts::FreeSansBold18pt7b);
  button(ui::MATCH[3].r, "-1", gfx.color565(40, 18, 26), C_DANGER, C_DANGER, &lgfx::fonts::FreeSansBold18pt7b);
  button(ui::MATCH[4].r, "DESHACER", C_CARD, eng.canUndo() ? C_TEXT : C_LINE, C_LINE);
  button(ui::MATCH[5].r, eng.phase() == Phase::Paused ? "CONTINUAR" : "PAUSA", C_CARD, C_TEXT, C_LINE);
}

static void drawOverlayBox(const char* title, uint16_t color) {
  gfx.fillRoundRect(150, 70, 500, 340, 20, C_SURFACE);
  gfx.drawRoundRect(150, 70, 500, 340, 20, color);
  text(title, 400, 120, &lgfx::fonts::FreeSansBold24pt7b, color);
}

static void drawPaused() {
  drawOverlayBox("PAUSA", C_TEXT);
  if (confirmAbandon) {
    text("Toca otra vez ABANDONAR", 400, 180, &lgfx::fonts::FreeSans12pt7b, C_DANGER);
    text("para descartar el partido", 400, 205, &lgfx::fonts::FreeSans12pt7b, C_DANGER);
  } else {
    text("El reloj esta detenido", 400, 180, &lgfx::fonts::FreeSans12pt7b, C_TEXT2);
  }
  button(ui::PAUSED[0].r, "CONTINUAR", C_ACCENT, C_BG, C_ACCENT, &lgfx::fonts::FreeSansBold18pt7b);
  button(ui::PAUSED[1].r, "ABANDONAR", gfx.color565(40, 18, 26), C_DANGER, C_DANGER);
}

static void drawPeriodEnd() {
  gfx.fillScreen(C_BG);
  const char* title = eng.period() == Period::First ? "FINAL 1a PARTE" : eng.period() == Period::Second ? "EMPATE - FINAL 2a PARTE" : "FIN DE LA PRORROGA";
  const char* cta = eng.period() == Period::First ? "CONTINUAR 2a PARTE" : eng.period() == Period::Second ? "IR A PRORROGA" : "IR A PENALTIS";
  text(title, 400, 70, &lgfx::fonts::FreeSansBold18pt7b, C_TEXT);
  char buf[16];
  mfv3::Score s = eng.score();
  snprintf(buf, sizeof(buf), "%u", s.white);
  text(buf, 270, 200, &lgfx::fonts::Font8, C_WHITE_TEAM, 1.8f);
  text("-", 400, 200, &lgfx::fonts::FreeSansBold24pt7b, C_TEXT2);
  snprintf(buf, sizeof(buf), "%u", s.blue);
  text(buf, 530, 200, &lgfx::fonts::Font8, gfx.color565(79, 163, 240), 1.8f);
  button(ui::PERIOD_END[0].r, cta, C_ACCENT, C_BG, C_ACCENT, &lgfx::fonts::FreeSansBold18pt7b);
}

static void drawCountdown(uint32_t now) {
  uint32_t rem = eng.countdownRemaining(now);
  int digit = rem == 0 ? 1 : (int)((rem + 999) / 1000);
  gfx.fillScreen(C_BG);
  text(periodLabel(eng.period()), 400, 70, &lgfx::fonts::FreeSansBold18pt7b, C_TEXT);
  char buf[12];
  snprintf(buf, sizeof(buf), "%d", digit);
  gfx.drawCircle(400, 250, 130, C_ACCENT);
  gfx.drawCircle(400, 250, 129, C_ACCENT);
  text(buf, 400, 250, &lgfx::fonts::Font7, C_ACCENT, 3.0f);
  text("TOCA PARA SALTAR", 400, 440, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT2);
  lastCountdownDigit = digit;
  beep(60);
}

static void drawPenalties() {
  gfx.fillScreen(C_BG);
  mfv3::Score s = eng.score();
  char right[32];
  snprintf(right, sizeof(right), "Marcador %u-%u", s.white, s.blue);
  drawHeader(eng.suddenDeath() ? "PENALTIS - MUERTE SUBITA" : "TANDA DE PENALTIS", right, C_TEXT2);
  mfv3::Score pen = eng.penaltyScore();
  Team turn = eng.nextKicker();
  for (int side = 0; side < 2; side++) {
    Team t = side == 0 ? Team::White : Team::Blue;
    int x0 = side == 0 ? 12 : 482;
    bool isTurn = eng.phase() == Phase::Penalties && turn == t;
    gfx.fillRoundRect(x0, 56, 306, 300, 20, C_CARD);
    gfx.drawRoundRect(x0, 56, 306, 300, 20, isTurn ? (t == Team::White ? C_WHITE_TEAM : C_ACCENT) : C_LINE);
    text(t == Team::White ? "BLANCO" : "AZUL", x0 + 153, 84, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT);
    char buf[4];
    snprintf(buf, sizeof(buf), "%u", pen.of(t));
    text(buf, x0 + 153, 170, &lgfx::fonts::Font8, t == Team::White ? C_WHITE_TEAM : gfx.color565(79, 163, 240), 1.6f);
    // Puntos de lanzamientos
    int idx = 0;
    for (uint8_t k = 0; k < eng.kickCount() && idx < 10; k++) {
      const mfv3::Kick& kk = eng.kick(k);
      if (kk.team != t) continue;
      int cx = x0 + 40 + (idx % 5) * 56, cy = 270 + (idx / 5) * 40;
      gfx.fillCircle(cx, cy, 14, kk.scored ? C_OK : C_DANGER);
      idx++;
    }
    for (; idx < cfg.penaltyRounds; idx++) gfx.drawCircle(x0 + 40 + idx * 56, 270, 14, C_LINE);
    const ui::Rect& g = ui::PENALTIES[side * 2].r;
    const ui::Rect& m = ui::PENALTIES[side * 2 + 1].r;
    button(g, "GOL", isTurn ? gfx.color565(18, 60, 45) : C_SURFACE, isTurn ? C_OK : C_LINE, isTurn ? C_OK : C_LINE, &lgfx::fonts::FreeSansBold18pt7b);
    button(m, "FALLO", isTurn ? gfx.color565(60, 18, 28) : C_SURFACE, isTurn ? C_DANGER : C_LINE, isTurn ? C_DANGER : C_LINE, &lgfx::fonts::FreeSansBold18pt7b);
  }
  text("LANZA", 400, 120, &lgfx::fonts::FreeSansBold9pt7b, C_TEXT2);
  text(turn == Team::White ? "BLANCO" : "AZUL", 400, 160, &lgfx::fonts::FreeSansBold18pt7b, turn == Team::White ? C_WHITE_TEAM : C_ACCENT);
  button(ui::PENALTIES[4].r, "DESHACER", C_CARD, eng.kickCount() ? C_TEXT : C_LINE, C_LINE, &lgfx::fonts::FreeSansBold9pt7b);
}

static void drawFinished() {
  gfx.fillScreen(C_BG);
  for (int r = 0; r < 6; r++) gfx.drawCircle(400, 200, 180 + r * 18, gfx.color565(30 + r * 6, 26 + r * 4, 10));
  Team w = eng.winner();
  text("FINAL DEL PARTIDO", 400, 60, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT2);
  text(w == Team::White ? "VICTORIA BLANCO" : "VICTORIA AZUL", 400, 140, &lgfx::fonts::FreeSansBold24pt7b,
       w == Team::White ? C_WHITE_TEAM : C_ACCENT, 1.4f);
  mfv3::Score s = eng.score(), p = eng.penaltyScore();
  char buf[48];
  if (eng.reason() == mfv3::Reason::Penalties) snprintf(buf, sizeof(buf), "%u-%u   Penaltis %u-%u", s.white, s.blue, p.white, p.blue);
  else if (eng.reason() == mfv3::Reason::GoldenGoal) snprintf(buf, sizeof(buf), "%u-%u   Gol de oro", s.white, s.blue);
  else snprintf(buf, sizeof(buf), "%u-%u", s.white, s.blue);
  text(buf, 400, 230, &lgfx::fonts::FreeSansBold24pt7b, C_GOLD);
  uint32_t secs = eng.totalTimeMs() / 1000;
  snprintf(buf, sizeof(buf), "Duracion %02lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  text(buf, 400, 290, &lgfx::fonts::FreeSans12pt7b, C_TEXT2);
  button(ui::FINISHED[0].r, "INICIO", C_CARD, C_TEXT, C_LINE, &lgfx::fonts::FreeSansBold18pt7b);
  button(ui::FINISHED[1].r, "REVANCHA", C_ACCENT, C_BG, C_ACCENT, &lgfx::fonts::FreeSansBold18pt7b);
}

// Clave de lo que hay en pantalla: si cambia, se redibuja todo.
static uint32_t screenKey(uint32_t now) {
  if (test.active) {
    uint32_t k = 0x40000000UL | ((uint32_t)test.touches << 12);
    for (int i = 0; i < 4; i++) k |= (uint32_t)test.corner[i] << i;
    k |= (uint32_t)test.remoteWhite << 4 | (uint32_t)test.remoteBlue << 5 | (uint32_t)test.sensorWhite << 6 |
         (uint32_t)test.sensorBlue << 7 | (uint32_t)test.usbSeen << 8 | (uint32_t)remoteLinked() << 9 | (uint32_t)radioOk << 10;
    return k;
  }
  if (!inMatch && screen != Screen::Home) return ajustesKey();
  if (!inMatch)
    return 0x80000000UL | (remoteLinked() ? 0x01000000UL : 0) | (updateAvailable() ? 0x02000000UL : 0) |
           ((uint32_t)cfg.endCondition << 16) | (cfg.goalsPerPeriod << 8) | cfg.minutesPerPeriod;
  mfv3::Score s = eng.score();
  uint32_t k = ((uint32_t)eng.phase() << 28) | ((uint32_t)eng.period() << 25) | ((uint32_t)s.white << 17) | ((uint32_t)s.blue << 9) |
               ((uint32_t)eng.kickCount() << 2) | (eng.canUndo() ? 2 : 0) | (confirmAbandon ? 1 : 0);
  k ^= (uint32_t)eng.matchPoint() << 30;
  if (now < flashUntil) k ^= 0x00400000UL;  // celebración del gol
  return k;
}

static void onModeChosen(uint8_t mode) {
  chosenMode = mode;
  homeCards = false;
  lastKey = 0xFFFFFFFF;
}

// Configuración (ui_config.h): lo que se toca pasa a cfg; CONTINUAR la guarda y empieza el partido.
static config::State configState() {
  config::State s;
  s.mode = chosenMode;
  s.goals = cfg.goalsPerPeriod;
  s.minutes = cfg.minutesPerPeriod;
  s.useGoals = cfg.endCondition != mfv3::EndCondition::Time;
  s.useTime = cfg.endCondition != mfv3::EndCondition::Goals;
  return s;
}
static void onConfigChanged(const config::State& s) {
  cfg.goalsPerPeriod = s.goals;
  cfg.minutesPerPeriod = s.minutes;
  cfg.endCondition = s.useGoals && s.useTime ? mfv3::EndCondition::Both
                     : s.useTime            ? mfv3::EndCondition::Time
                                            : mfv3::EndCondition::Goals;
}
static void onConfigBack() { homeCards = true; }
static void onConfigNext() {
  saveConfig();
  startMatch();
}
// ---------------------------------------------------------------- partido (ui_partido.h)
static const char* const PERIOD_LABEL[4] = {"1ª PARTE", "2ª PARTE", "PRÓRROGA", "PENALTIS"};
static void goalsText(char* out, size_t n, uint8_t g) { snprintf(out, n, "%u %s", g, g == 1 ? "gol" : "goles"); }
static void formatClock(char* out, size_t n, uint32_t ms) {
  uint32_t s = ms / 1000;
  snprintf(out, n, "%02lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

// Lo que enseña la pantalla del partido, sacado del motor (mismos textos y colores que MatchScreen.tsx).
static void matchView(partido::View& v, uint32_t now) {
  memset(&v, 0, sizeof(v));
  mfv3::Score s = eng.score();
  const mfv3::Config& c = eng.config();
  v.mode = chosenMode;
  v.phase = (uint8_t)eng.phase();
  v.period = (uint8_t)eng.period();
  v.white = s.white;
  v.blue = s.blue;
  mfv3::Score ps = eng.periodScore(eng.period());
  bool canCorrect = eng.phase() == Phase::Playing || eng.phase() == Phase::Paused;
  v.annulWhite = canCorrect && ps.white > 0;
  v.annulBlue = canCorrect && ps.blue > 0;
  v.singlePeriod = c.endCondition == mfv3::EndCondition::Goals;
  uint32_t ms = eng.clockMs(now);
  // El reloj que cuenta hacia atrás redondea hacia arriba (como la app): 4:59,2 se ve 05:00 hasta llegar a 4:59.
  formatClock(v.clock, sizeof(v.clock), eng.clockCountsDown() ? ms + 999 : ms);
  bool overtime = eng.period() == Period::Overtime;
  v.clockColor = overtime ? 0x7dffd8 : eng.clockCountsDown() && ms <= 20000 ? 0xff4d5e
                 : eng.clockCountsDown() && ms <= 60000 ? 0xffd43b : 0x3dff7a;
  v.lockMs = eng.phase() == Phase::Playing ? eng.lockRemaining(now) : 0;
  v.matchPoint = overtime ? 0 : eng.matchPoint();
  // Racha (goalStreak de la app): goles seguidos del último equipo que marcó, sin contar los anulados.
  for (int i = (int)eng.goalCount() - 1; i >= 0; i--) {
    const mfv3::Goal& g = eng.goalAt((uint8_t)i);
    if (g.annulled) continue;
    if (!v.streak) v.streakTeam = (uint8_t)g.team;
    else if ((uint8_t)g.team != v.streakTeam) break;
    v.streak++;
  }
  char g[16];
  goalsText(g, sizeof(g), c.goalsPerPeriod);
  bool showPeriod = !(v.singlePeriod && eng.period() == Period::First);
  bool showGoals = c.endCondition != mfv3::EndCondition::Time && !overtime;
  snprintf(v.info, sizeof(v.info), "%s%s%s%s", showPeriod ? PERIOD_LABEL[(int)eng.period()] : "",
           showPeriod && showGoals ? " · " : "", showGoals ? "A " : "", showGoals ? g : "");
  if (c.endCondition == mfv3::EndCondition::Goals) snprintf(v.cond, sizeof(v.cond), "Gana quien llegue a %s", g);
  else if (c.endCondition == mfv3::EndCondition::Time) snprintf(v.cond, sizeof(v.cond), "2 partes de %u min", c.minutesPerPeriod);
  else snprintf(v.cond, sizeof(v.cond), "A %s o 2 partes de %u min", g, c.minutesPerPeriod);
  // Cuenta atrás: cifra 3-2-1 y anillo que se vacía.
  uint32_t rem = eng.countdownRemaining(now);
  v.countdownN = (uint8_t)(rem == 0 ? 1 : (rem + 999) / 1000);
  v.countdownPermille = (uint16_t)(rem * 1000 / Engine::COUNTDOWN_MS);
  snprintf(v.countdownTitle, sizeof(v.countdownTitle), "%s%s",
           eng.period() == Period::First && v.singlePeriod ? "PARTIDO" : PERIOD_LABEL[(int)eng.period()],
           overtime ? " · GOL DE ORO · 60 s" : "");
  // Final de parte (el motor sigue en la parte que acaba de terminar).
  if (eng.phase() == Phase::PeriodEnd) {
    bool byTime = eng.clockCountsDown() && eng.clockMs(now) == 0;
    const char* reason = byTime ? "por tiempo" : "por goles";
    if (eng.period() == Period::Second) {
      snprintf(v.endTitle, sizeof(v.endTitle), "FINAL 2ª PARTE #6f84a3 · %s#", reason);
      snprintf(v.endCta, sizeof(v.endCta), "IR A PRÓRROGA");
      snprintf(v.endNote, sizeof(v.endNote), "Empate: prórroga de 60 segundos con gol de oro.");
    } else if (overtime) {
      snprintf(v.endTitle, sizeof(v.endTitle), "FIN DE LA PRÓRROGA #6f84a3 · %s#", reason);
      snprintf(v.endCta, sizeof(v.endCta), "IR A PENALTIS");
      snprintf(v.endNote, sizeof(v.endNote), "Sin gol en la prórroga: tanda de penaltis.");
    } else {
      snprintf(v.endTitle, sizeof(v.endTitle), "FINAL 1ª PARTE #6f84a3 · %s#", reason);
      snprintf(v.endCta, sizeof(v.endCta), "CONTINUAR 2ª PARTE");
    }
    char d[8], lower[16];
    formatClock(d, sizeof(d), eng.periodElapsed(now));
    snprintf(lower, sizeof(lower), "%s", eng.period() == Period::Second ? "2ª parte" : overtime ? "prórroga" : "1ª parte");
    snprintf(v.endPartial, sizeof(v.endPartial), "Parcial %s: %u–%u · %s", lower, ps.white, ps.blue, d);
    v.endWhite = s.white;
    v.endBlue = s.blue;
  }
  v.confirmAbandon = confirmAbandon;
}

static void onMatchGoal(uint8_t t) { inputGoal(t ? Team::Blue : Team::White); }
static void onMatchAnnul(uint8_t t) {
  eng.minusOne(t ? Team::Blue : Team::White, millis());
  afterChange(false, t ? Team::Blue : Team::White);
}
static void onMatchSkip() {
  eng.skipCountdown(millis());
  afterChange(false, Team::White);
}
static void onMatchNext() {
  eng.next(millis());
  afterChange(false, Team::White);
}
static void onMatchAskAbandon(bool on) { confirmAbandon = on; }
static void onMatchAbandon() { goHome(); }  // el partido se descarta (no se guarda)

// ---------------------------------------------------------------- penaltis (ui_penaltis.h)
static void penaltyView(penaltis::View& v) {
  memset(&v, 0, sizeof(v));
  mfv3::Score p = eng.penaltyScore(), s = eng.score();
  v.penWhite = p.white;
  v.penBlue = p.blue;
  v.white = s.white;
  v.blue = s.blue;
  v.rounds = eng.config().penaltyRounds;
  v.kicks = eng.kickCount() < penaltis::MAX_KICKS ? eng.kickCount() : penaltis::MAX_KICKS;
  for (uint8_t i = 0; i < v.kicks; i++) {
    v.scored[i] = eng.kick(i).scored;
    v.team[i] = (uint8_t)eng.kick(i).team;
  }
  v.active = eng.phase() == Phase::Penalties;
  v.turn = (uint8_t)eng.nextKicker();
  v.sudden = eng.suddenDeath();
}
static void onPenaltyKick(uint8_t t, bool scored) {
  eng.penalty(t ? Team::Blue : Team::White, scored, millis());
  afterChange(false, Team::White);
}
static void onPenaltyUndo() {
  eng.undoPenalty();
  afterChange(false, Team::White);
}

// ---------------------------------------------------------------- victoria (ui_victoria.h)
// Datos del partido como matchFacts() de la app: duración, remontada, racha, portería a cero, gol de oro, penaltis.
static void victoryView(victoria::View& v) {
  memset(&v, 0, sizeof(v));
  mfv3::Score s = eng.score();
  v.winner = (uint8_t)eng.winner();
  v.white = s.white;
  v.blue = s.blue;
  char d[8];
  formatClock(d, sizeof(d), eng.totalTimeMs());
  int n = snprintf(v.facts, sizeof(v.facts), "Duración %s", d);
  int run[2] = {0, 0}, streak = 0, best = 0, deficit = 0, w = v.winner;
  int streakTeam = -1;
  for (uint8_t i = 0; i < eng.goalCount(); i++) {
    const mfv3::Goal& g = eng.goalAt(i);
    if (g.annulled) continue;
    int t = (int)g.team;
    run[t]++;
    streak = t == streakTeam ? streak + 1 : 1;
    streakTeam = t;
    if (streak > best) best = streak;
    if (run[1 - w] - run[w] > deficit) deficit = run[1 - w] - run[w];
  }
  auto add = [&](const char* f) {
    if (n < (int)sizeof(v.facts) - 4) n += snprintf(v.facts + n, sizeof(v.facts) - n, " · %s", f);
  };
  char b[32];
  if (deficit >= 2) {
    snprintf(b, sizeof(b), "¡Remontada de %d goles!", deficit);
    add(b);
  } else if (deficit == 1) add("Remontada");
  if (best >= 3) {
    snprintf(b, sizeof(b), "%d goles seguidos", best);
    add(b);
  }
  uint8_t mine = w ? s.blue : s.white, theirs = w ? s.white : s.blue;
  if (theirs == 0 && mine > 0) add("Portería a cero");
  if (eng.reason() == mfv3::Reason::GoldenGoal) add("Gol de oro");
  if (eng.kickCount()) {
    mfv3::Score p = eng.penaltyScore();
    snprintf(b, sizeof(b), "Penaltis %u–%u", p.white, p.blue);
    add(b);
  }
}

// Reposo (ui_reposo.h): «Último: BLANCO 5 – 3 AZUL» con el último resultado guardado.
static void sleepView() {
  char last[64] = "";
  if (historyCount)
    snprintf(last, sizeof(last), "Último: #4f7ea3 BLANCO %u – %u AZUL#", history[0].white, history[0].blue);
  reposo::update(rtc.now.hour, rtc.now.minute, rtc.now.second, reloj::weekday(rtc.now), rtc.now.day, rtc.now.month, last,
                 rtc.lowBattery());
}

static void onConfigRefused() { ui::toast("Deja activa al menos una: goles o tiempo"); }

static void render() {
  uint32_t now = millis();
  if (!test.active && !inMatch && screen == Screen::Home && homeCards && !sleepClock()) {
    inicio::show();
    char warn[32];
    bool low = rtc.lowBattery();
    if (low) snprintf(warn, sizeof(warn), "BATERÍA BAJA %u %%", rtc.bat.percent);
    inicio::setStatus(remoteLinked(), updateAvailable(), low ? warn : nullptr);
    lastKey = lastSleepKey = 0xFFFFFFFF;
    return;
  }
  if (!test.active && !inMatch && screen == Screen::Home && !sleepClock()) {
    config::show(configState());
    config::tickClock(rtc.clockOk, rtc.now.hour, rtc.now.minute, rtc.now.second);
    lastKey = lastSleepKey = 0xFFFFFFFF;
    return;
  }
  if (sleepClock()) {
    sleepView();
    lastKey = lastSleepKey = 0xFFFFFFFF;
    return;
  }
  if (inMatch && !test.active && eng.phase() == Phase::Finished) {
    victoria::View v;
    victoryView(v);
    victoria::update(v);
    lastKey = lastSleepKey = 0xFFFFFFFF;
    return;
  }
  if (inMatch && !test.active && eng.phase() == Phase::Penalties) {
    penaltis::View v;
    penaltyView(v);
    penaltis::update(v);
    lastKey = lastSleepKey = 0xFFFFFFFF;
    return;
  }
  if (inMatch && !test.active && eng.phase() != Phase::Penalties && eng.phase() != Phase::Finished) {
    partido::View v;
    matchView(v, now);
    partido::update(v);
    lastKey = lastSleepKey = 0xFFFFFFFF;
    return;
  }
  // Pantallas antiguas: si se viene de una nueva, se dibujan enteras otra vez.
  if (!gfx.active()) lastKey = lastSleepKey = 0xFFFFFFFF;
  if (sleepClock()) {
    if ((uint32_t)rtc.now.hour * 60 + rtc.now.minute != lastSleepKey) drawSleep();
    lastKey = 0xFFFFFFFF;  // al despertar se redibuja lo que hubiera
    return;
  }
  lastSleepKey = 0xFFFFFFFF;
  uint32_t key = screenKey(now);
  if (key != lastKey) {
    lastKey = key;
    if (test.active) drawTest();
    else if (!inMatch && screen != Screen::Home) drawAjustes();
    else if (!inMatch) drawHome();
    else {
      switch (eng.phase()) {
        case Phase::Countdown: drawCountdown(now); break;
        case Phase::Playing: drawMatch(now); break;
        case Phase::Paused: drawMatch(now); drawPaused(); break;
        case Phase::PeriodEnd: drawPeriodEnd(); break;
        case Phase::Penalties: drawPenalties(); break;
        case Phase::Finished: drawFinished(); break;
      }
    }
    return;
  }
  // Actualizaciones parciales (sin parpadeo).
  if (!inMatch) {
    if (!test.active && screen == Screen::Home && statusKey() != lastStatusKey) drawStatus();
    return;
  }
  if (eng.phase() == Phase::Countdown) {
    uint32_t rem = eng.countdownRemaining(now);
    int digit = rem == 0 ? 1 : (int)((rem + 999) / 1000);
    if (digit != lastCountdownDigit) drawCountdown(now);
  } else if (eng.phase() == Phase::Playing) {
    uint32_t ms = eng.clockMs(now);
    uint32_t sec = (ms + (eng.clockCountsDown() ? 999 : 0)) / 1000;
    if (sec != lastClockSecond) drawClock(now);
    if (eng.lockRemaining(now) > 0 || lastLockShown) drawLock(now);
  }
}

// ---------------------------------------------------------------- toques
static void onTap(int16_t x, int16_t y) {
  uint32_t now = millis();
  if (test.active) {
    if (ui::hit(ui::TEST, x, y) == Btn::TestDone) {
      leaveTest();
      return;
    }
    for (int i = 0; i < 4; i++)
      if (ui::TEST_CORNERS[i].contains(x, y)) {
        test.corner[i] = true;
        playTestTone();
      }
    test.lastX = x;
    test.lastY = y;
    test.touches++;
    return;
  }
  if (!inMatch && screen != Screen::Home) {
    ajustesTap(x, y);
    return;
  }
  if (!inMatch) {
    Btn b = ui::hit(ui::HOME, x, y);
    bool useGoals = cfg.endCondition != mfv3::EndCondition::Time;
    bool useTime = cfg.endCondition != mfv3::EndCondition::Goals;
    switch (b) {
      case Btn::CondGoals: cfg.endCondition = mfv3::EndCondition::Goals; break;
      case Btn::CondTime: cfg.endCondition = mfv3::EndCondition::Time; break;
      case Btn::CondBoth: cfg.endCondition = mfv3::EndCondition::Both; break;
      case Btn::GoalsMinus: if (useGoals && cfg.goalsPerPeriod > 1) cfg.goalsPerPeriod--; break;
      case Btn::GoalsPlus: if (useGoals && cfg.goalsPerPeriod < 20) cfg.goalsPerPeriod++; break;
      case Btn::MinutesMinus: if (useTime && cfg.minutesPerPeriod > 1) cfg.minutesPerPeriod--; break;
      case Btn::MinutesPlus: if (useTime && cfg.minutesPerPeriod < 30) cfg.minutesPerPeriod++; break;
      case Btn::Play: saveConfig(); startMatch(); return;
      case Btn::Settings: openSettings(); return;
      case Btn::UpdateAvail: if (updateAvailable()) openUpdate(); return;
      default: return;
    }
    return;
  }
  switch (eng.phase()) {
    case Phase::Countdown:
      eng.skipCountdown(now);
      afterChange(false, Team::White);
      break;
    case Phase::Playing: {
      Btn b = ui::hit(ui::MATCH, x, y);
      if (b == Btn::ScoreWhite) inputGoal(Team::White);
      else if (b == Btn::ScoreBlue) inputGoal(Team::Blue);
      else if (b == Btn::MinusWhite) { eng.minusOne(Team::White, now); afterChange(false, Team::White); }
      else if (b == Btn::MinusBlue) { eng.minusOne(Team::Blue, now); afterChange(false, Team::Blue); }
      else if (b == Btn::Undo) { eng.undo(now); afterChange(false, Team::White); }
      else if (b == Btn::Pause) inputPause();
      break;
    }
    case Phase::Paused: {
      Btn b = ui::hit(ui::PAUSED, x, y);
      if (b == Btn::Resume) { confirmAbandon = false; inputPause(); }
      else if (b == Btn::Abandon) {
        if (confirmAbandon) goHome();  // el partido se descarta (no se guarda)
        else confirmAbandon = true;
      } else {
        // En pausa siguen disponibles −1 y DESHACER.
        Btn m = ui::hit(ui::MATCH, x, y);
        if (m == Btn::MinusWhite) eng.minusOne(Team::White, now);
        else if (m == Btn::MinusBlue) eng.minusOne(Team::Blue, now);
        else if (m == Btn::Undo) eng.undo(now);
        afterChange(false, Team::White);
      }
      break;
    }
    case Phase::PeriodEnd:
      if (ui::hit(ui::PERIOD_END, x, y) == Btn::Next) { eng.next(now); afterChange(false, Team::White); }
      break;
    case Phase::Penalties: {
      Btn b = ui::hit(ui::PENALTIES, x, y);
      if (b == Btn::PenWhiteGoal) eng.penalty(Team::White, true, now);
      else if (b == Btn::PenWhiteMiss) eng.penalty(Team::White, false, now);
      else if (b == Btn::PenBlueGoal) eng.penalty(Team::Blue, true, now);
      else if (b == Btn::PenBlueMiss) eng.penalty(Team::Blue, false, now);
      else if (b == Btn::PenUndo) eng.undoPenalty();
      afterChange(false, Team::White);
      break;
    }
    case Phase::Finished: {
      Btn b = ui::hit(ui::FINISHED, x, y);
      if (b == Btn::NewMatch) goHome();
      else if (b == Btn::Rematch) startMatch();
      break;
    }
  }
}

#if HAS_ISOLATED_IO
static void pollIsolatedInputs(uint32_t now) {
  uint16_t v;
  if (!lcd.exioRead(v)) return;
  if (!diReady) {
    diIdle = v;
    diReady = true;
    return;
  }
  uint16_t changed = v ^ diIdle;
  if (test.active) {
    if (changed & (1u << exio::DI0)) test.sensorWhite = true;
    if (changed & (1u << exio::DI1)) test.sensorBlue = true;
  }
  if (diWhite.update(changed & (1u << exio::DI0), now)) { wakeScreen(); inputGoal(Team::White); }
  if (diBlue.update(changed & (1u << exio::DI1), now)) { wakeScreen(); inputGoal(Team::Blue); }
  if (doOffAt && now > doOffAt) {
    doOffAt = 0;
    lcd.exioWrite(exio::DO0, false);
    lcd.exioWrite(exio::DO1, false);
  }
}
#endif

// El primer toque con la pantalla atenuada solo la despierta (LVGL no lo ve).
static bool onTouchDown() {
  lastActivity = millis();
  if (!dimmed) return false;
  dimmed = false;
  lcd.backlight(backlightLevel());
  return true;
}

// ---------------------------------------------------------------- arranque y bucle
void setup() {
  Serial.begin(115200);
  if (BUZZER_PIN >= 0) pinMode(BUZZER_PIN, OUTPUT);
#if MFV3_BOARD == BOARD_WAVESHARE_7C
  lcd.expanderInit();
#elif MFV3_BOARD == BOARD_WAVESHARE_7
  lcd.waveshareExpanderInit();
#endif
  lcd.detectTouchAddress();  // GT911 en 0x5D o 0x14 (la 7C ya lo fija al arrancar el expansor)
  FFat.begin(true);  // capas de la app y letras (scripts/placa_recursos.py); también la usa el sonido
  uiport::begin(lcd);
  uiport::touchDownHook = onTouchDown;
  gfx.tapCb = onTap;
  inicio::hooks.mode = onModeChosen;
  inicio::hooks.settings = openSettings;
  config::hooks.back = onConfigBack;
  config::hooks.next = onConfigNext;
  config::hooks.changed = onConfigChanged;
  config::hooks.refused = onConfigRefused;
  partido::hooks.goal = onMatchGoal;
  partido::hooks.annul = onMatchAnnul;
  partido::hooks.pause = inputPause;
  partido::hooks.skip = onMatchSkip;
  partido::hooks.next = onMatchNext;
  partido::hooks.askAbandon = onMatchAskAbandon;
  partido::hooks.abandon = onMatchAbandon;
  penaltis::hooks.kick = onPenaltyKick;
  penaltis::hooks.undo = onPenaltyUndo;
  victoria::hooks.home = goHome;
  victoria::hooks.rematch = startMatch;
  lcd.backlight(backlightLevel());
  initColors();
  if (AUX_UART_RX >= 0) AuxSerial.begin(115200, SERIAL_8N1, AUX_UART_RX, AUX_UART_TX);
  loadPrefs();
  lcd.backlight(backlightLevel());  // el brillo guardado
#if HAS_AUDIO
  sndStore.begin();
  player.begin(lcd, sndStore, sndVolume);
#endif
  radioBegin();
  startCheck();  // ¿hay versión nueva? (si hay Wi-Fi guardado; sin bloquear)
  lastActivity = millis();
  sendHello();
  sendLine("STATE idle");
  if (!prefs.getBool("tested", false)) enterTest();  // primer arranque: asistente de prueba
}

void loop() {
  uint32_t now = millis();

  // Entradas de placas auxiliares (UART) y del USB.
  while (Serial.available() > 0)
    if (usbRx.push((char)Serial.read())) handleBoardLine(usbRx.line());
  if (AUX_UART_RX >= 0)
    while (AuxSerial.available() > 0)
      if (auxRx.push((char)AuxSerial.read())) handleBoardLine(auxRx.line());

  radioPoll();
  rtc.poll(now);
#if HAS_AUDIO
  sndCmd.tick(now, sendLine);
  player.pump(now);
#endif
#if HAS_ISOLATED_IO
  pollIsolatedInputs(now);
#endif

  if (inMatch && eng.tick(now)) afterChange(false, Team::White);
  ajustesLoop(now);

  if (buzzerUntil && now > buzzerUntil) {
    buzzerUntil = 0;
    if (BUZZER_PIN >= 0) digitalWrite(BUZZER_PIN, LOW);
  }

  // Atenuar la pantalla tras inactividad fuera de un partido en juego.
  bool idleScreen = (!inMatch && screen != Screen::Install) || (inMatch && eng.phase() == Phase::Finished);
  if (idleScreen && !dimmed && now - lastActivity > SLEEP_AFTER_MS) {
    dimmed = true;
    lcd.backlight(16);
  }

  render();
  uiport::step();
  delay(2);
}
