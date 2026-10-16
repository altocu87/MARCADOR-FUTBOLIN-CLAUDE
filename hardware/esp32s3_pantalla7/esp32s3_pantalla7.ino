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
//   - Sonido (Waveshare 7C): los mismos momentos que la app (gol, remontada, victoria…) por el altavoz de la
//     placa; sonidos propios en la tarjeta SD (/sonidos/<momento>/*.wav) o, si no hay, los de siempre.
//
// Antes de compilar: elige tu placa en board_config.h. Bibliotecas: LovyanGFX.
// NOTA: compila en GitHub Actions (núcleo esp32 3.x + LovyanGFX) y la lógica (motor, protocolo, botones)
// se prueba en PC; todavía no se ha probado en la placa real.

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_arduino_version.h>
#include "board_config.h"
#include "s3_audio.h"
#include "mfv3_core.h"
#include "mfv3_engine.h"
#include "mfv3_radio.h"
#include "ui_layout.h"
#include "s3_types.h"
#include "s3_sound.h"

using mfv3::Engine;
using mfv3::Phase;
using mfv3::Period;
using mfv3::Team;
using ui::Btn;

static const char* FW_VERSION = "1.0";
#if MFV3_BOARD == BOARD_WAVESHARE_7C
#define S3_BOARD_ID "waveshare-7c"
#define S3_CAPS "goles,anular,pausa,pantalla,radio,sensores,salidas,brillo"
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

LGFX lcd;
Engine eng;
s3snd::Tracker sounds;  // qué momento suena tras cada cambio
mfv3::Config cfg;
Preferences prefs;
HardwareSerial AuxSerial(1);
mfv3::LineBuffer usbRx, auxRx;

// ---------------------------------------------------------------- colores (RGB565)
static uint16_t C_BG, C_SURFACE, C_CARD, C_LINE, C_TEXT, C_TEXT2, C_ACCENT, C_WHITE_TEAM, C_WHITE_INK, C_BLUE_TEAM,
    C_GOLD, C_DANGER, C_OK;

static void initColors() {
  C_BG = lcd.color565(7, 11, 18);
  C_SURFACE = lcd.color565(9, 19, 36);
  C_CARD = lcd.color565(17, 34, 59);
  C_LINE = lcd.color565(34, 56, 90);
  C_TEXT = lcd.color565(243, 247, 255);
  C_TEXT2 = lcd.color565(166, 183, 207);
  C_ACCENT = lcd.color565(98, 214, 255);
  C_WHITE_TEAM = lcd.color565(238, 246, 255);
  C_WHITE_INK = lcd.color565(11, 22, 38);
  C_BLUE_TEAM = lcd.color565(23, 109, 188);
  C_GOLD = lcd.color565(242, 201, 76);
  C_DANGER = lcd.color565(255, 90, 110);
  C_OK = lcd.color565(61, 220, 151);
}

// ---------------------------------------------------------------- historial breve (NVS)
static LastResult history[8];
static uint8_t historyCount = 0;
static uint32_t matchesPlayed = 0;

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
  historyCount = prefs.getBytes("hist", history, sizeof(history)) / sizeof(LastResult);
  userBrightness = prefs.getUChar("bri", 100);
  if (userBrightness > 100) userBrightness = 100;
  s3audio::volume = prefs.getUChar("vol", 80);
  if (s3audio::volume > 100) s3audio::volume = 100;
}

static void saveConfig() {
  prefs.putUChar("cond", (uint8_t)cfg.endCondition);
  prefs.putUChar("goals", cfg.goalsPerPeriod);
  prefs.putUChar("mins", cfg.minutesPerPeriod);
}

static void saveResult() {
  mfv3::Score s = eng.score(), p = eng.penaltyScore();
  LastResult r = {s.white, s.blue, p.white, p.blue, (uint8_t)(eng.winner() == Team::White ? 0 : 1), (uint8_t)eng.reason()};
  for (int i = 7; i > 0; i--) history[i] = history[i - 1];
  history[0] = r;
  if (historyCount < 8) historyCount++;
  matchesPlayed++;
  prefs.putBytes("hist", history, historyCount * sizeof(LastResult));
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
// Sale sola la primera vez que se enciende la placa; después, con el botón PRUEBA de la pantalla de inicio.
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
  return Snapshot{eng.phase(), eng.period(), s.white, s.blue, eng.kickCount()};
}

static void afterChange(bool goalAccepted, Team goalTeam) {
  Snapshot now = takeSnapshot();
  char buf[32];
  if (now.phase != snap.phase) {
    snprintf(buf, sizeof(buf), "STATE %s", phaseName(now.phase));
    sendLine(buf);
  }
  if (goalAccepted) {
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
  s3snd::Ev evs[s3snd::Tracker::MAX_OUT];
  s3audio::play(evs, sounds.observe(eng, goalAccepted, goalTeam, evs));
  snap = now;
}

// ---------------------------------------------------------------- acciones
static void startMatch() {
  if (!eng.start(cfg, millis())) return;
  sounds.reset(eng);
  inMatch = true;
  confirmAbandon = false;
  snap = takeSnapshot();
  sendLine("STATE countdown");
  sendLine("SCORE 0 0");
  lastKey = 0xFFFFFFFF;
}

static void goHome() {
  inMatch = false;
  confirmAbandon = false;
  sendLine("STATE idle");
  lastKey = 0xFFFFFFFF;
}

static void leaveTest() {
  test.active = false;
  prefs.putBool("tested", true);  // ya no sale sola al encender (sigue en el botón PRUEBA)
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
  char buf[112];
  snprintf(buf, sizeof(buf), "HELLO %s %s caps=%s%s", S3_BOARD_ID, FW_VERSION, S3_CAPS, s3audio::status.codec ? ",altavoz,volumen" : "");
  sendLine(buf);
}

// «SONIDO gol» o «SONIDO goal»: toca ese momento (para probar el altavoz y los sonidos de la SD).
static void playNamed(const char* name) {
  while (*name == ' ') name++;
  for (uint8_t i = 0; i < s3snd::EV_COUNT; i++) {
    const s3snd::EvInfo& in = s3snd::info((s3snd::Ev)i);
    if (!strcasecmp(name, in.folder) || !strcasecmp(name, in.webId)) {
      s3audio::play((s3snd::Ev)i);
      return;
    }
  }
}

// Línea del protocolo MFV3 recibida de una placa auxiliar.
static void handleBoardLine(const char* line) {
  wakeScreen();
  if (test.active) test.usbSeen = true;
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
  } else if (!strcmp(word, "VOLUMEN")) {
    // Volumen del altavoz de la placa: 0–100 % (0 = sin sonido).
    int v = atoi(line + i);
    s3audio::volume = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
    prefs.putUChar("vol", s3audio::volume);
  } else if (!strcmp(word, "SONIDO")) playNamed(line + i);
  else if (!strcmp(word, "PING")) sendLine("PONG");
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

static void radioPush(const uint8_t* mac, const uint8_t* data, int len) {
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
  lcd.setFont(font);
  lcd.setTextSize(size);
  lcd.setTextColor(color);
  lcd.setTextDatum(lgfx::textdatum_t::middle_center);
  lcd.drawString(s, x, y);
}

static void button(const ui::Rect& r, const char* label, uint16_t fill, uint16_t ink, uint16_t border,
                   const lgfx::IFont* font = &lgfx::fonts::FreeSansBold12pt7b) {
  lcd.fillRoundRect(r.x, r.y, r.w, r.h, 12, fill);
  lcd.drawRoundRect(r.x, r.y, r.w, r.h, 12, border);
  text(label, r.x + r.w / 2, r.y + r.h / 2, font, ink);
}

static void drawHeader(const char* left, const char* right, uint16_t rightColor) {
  lcd.fillRect(0, 0, ui::W, 44, C_BG);
  lcd.setFont(&lgfx::fonts::FreeSansBold12pt7b);
  lcd.setTextSize(1);
  lcd.setTextColor(C_TEXT);
  lcd.setTextDatum(lgfx::textdatum_t::middle_left);
  lcd.drawString(left, 14, 24);
  lcd.setTextColor(rightColor);
  lcd.setTextDatum(lgfx::textdatum_t::middle_right);
  lcd.drawString(right, ui::W - 14, 24);
  lcd.drawFastHLine(0, 44, ui::W, C_LINE);
}

static void drawHome() {
  lcd.fillScreen(C_BG);
  text("MARCADOR FUTBOLIN V3", ui::W / 2, 46, &lgfx::fonts::FreeSansBold24pt7b, C_ACCENT);
  char sub[64];
  if (historyCount > 0) {
    const LastResult& r = history[0];
    snprintf(sub, sizeof(sub), "Ultimo: %u-%u  gana %s   |   %lu partidos", r.white, r.blue, r.winner ? "AZUL" : "BLANCO",
             (unsigned long)matchesPlayed);
  } else {
    snprintf(sub, sizeof(sub), "Partido rapido en la mesa  |  v%s", FW_VERSION);
  }
  text(sub, ui::W / 2, 100, &lgfx::fonts::FreeSans12pt7b, C_TEXT2);
  text(remoteLinked() ? "MANDO CONECTADO" : "MANDO: SIN SENAL", ui::W / 2, 128, &lgfx::fonts::FreeSansBold9pt7b,
       remoteLinked() ? C_OK : C_LINE);

  const char* conds[3] = {"POR GOLES", "POR TIEMPO", "AMBAS"};
  for (int i = 0; i < 3; i++) {
    bool on = (int)cfg.endCondition == i;
    button(ui::HOME[i].r, conds[i], on ? lcd.color565(20, 60, 85) : C_CARD, C_TEXT, on ? C_ACCENT : C_LINE);
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
  button(ui::HOME[8].r, "PRUEBA", C_SURFACE, C_TEXT2, C_LINE, &lgfx::fonts::FreeSansBold12pt7b);
}

static void testLine(int y, const char* label, bool ok, bool optional) {
  lcd.fillRoundRect(130, y - 18, 540, 36, 10, C_SURFACE);
  lcd.fillCircle(156, y, 11, ok ? C_OK : (optional ? C_LINE : C_DANGER));
  lcd.setFont(&lgfx::fonts::FreeSansBold12pt7b);
  lcd.setTextSize(1);
  lcd.setTextColor(ok ? C_TEXT : C_TEXT2);
  lcd.setTextDatum(lgfx::textdatum_t::middle_left);
  lcd.drawString(label, 180, y);
  lcd.setTextDatum(lgfx::textdatum_t::middle_right);
  lcd.setTextColor(ok ? C_OK : C_TEXT2);
  lcd.drawString(ok ? "OK" : (optional ? "opcional" : "pendiente"), 655, y);
}

static void drawTest() {
  lcd.fillScreen(C_BG);
  for (int i = 0; i < 4; i++) {
    const ui::Rect& r = ui::TEST_CORNERS[i];
    lcd.fillRoundRect(r.x + 6, r.y + 6, r.w - 12, r.h - 12, 14, test.corner[i] ? C_OK : C_CARD);
    lcd.drawRoundRect(r.x + 6, r.y + 6, r.w - 12, r.h - 12, 14, test.corner[i] ? C_OK : C_ACCENT);
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
  const s3audio::Status& au = s3audio::status;
  char sd[32];
  if (au.sd) snprintf(sd, sizeof(sd), "%lu GB, %u sonidos propios", (unsigned long)((au.sdMB + 512) / 1024), au.custom);
  else snprintf(sd, sizeof(sd), "sin tarjeta");
  snprintf(buf, sizeof(buf), "Altavoz %s  |  SD %s", au.codec ? "OK (suena al tocar)" : "ERROR", sd);
  text(buf, ui::W / 2, 351, &lgfx::fonts::FreeSans9pt7b, au.codec ? C_TEXT2 : C_DANGER);
#endif
  if (test.lastX >= 0) {
    snprintf(buf, sizeof(buf), "Ultimo toque: %d, %d", test.lastX, test.lastY);
    text(buf, ui::W / 2, 374, &lgfx::fonts::FreeSans9pt7b, C_TEXT2);
    lcd.drawLine(test.lastX - 12, test.lastY, test.lastX + 12, test.lastY, C_GOLD);
    lcd.drawLine(test.lastX, test.lastY - 12, test.lastX, test.lastY + 12, C_GOLD);
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
  lcd.fillRoundRect(r.x, r.y, r.w, r.h, 22, fill);
  uint8_t mp = eng.matchPoint();
  bool matchPoint = eng.period() != Period::Overtime && (mp & (t == Team::White ? 1 : 2));
  uint16_t border = highlight ? C_GOLD : matchPoint ? C_DANGER : (t == Team::White ? C_TEXT2 : C_ACCENT);
  for (int i = 0; i < (highlight || matchPoint ? 5 : 2); i++) lcd.drawRoundRect(r.x - i, r.y - i, r.w + 2 * i, r.h + 2 * i, 22 + i, border);
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
  lcd.fillRect(ui::CLOCK.x, ui::CLOCK.y, ui::CLOCK.w, ui::CLOCK.h, C_BG);
  bool warn = eng.clockCountsDown() && ms <= 10000 && eng.phase() == Phase::Playing;
  text(buf, ui::CLOCK.x + ui::CLOCK.w / 2, ui::CLOCK.y + ui::CLOCK.h / 2, &lgfx::fonts::Font7, warn ? C_DANGER : C_ACCENT, 0.7f);
  lastClockSecond = sec;
}

static void drawLock(uint32_t now) {
  uint32_t lock = eng.lockRemaining(now);
  lcd.fillRect(ui::LOCK_MSG.x, ui::LOCK_MSG.y, ui::LOCK_MSG.w, ui::LOCK_MSG.h, C_BG);
  if (lock > 0) {
    char buf[24];
    snprintf(buf, sizeof(buf), "BLOQUEO %.1f s", lock / 1000.0f);
    text(buf, ui::LOCK_MSG.x + ui::LOCK_MSG.w / 2, ui::LOCK_MSG.y + 15, &lgfx::fonts::FreeSansBold9pt7b, C_GOLD);
  }
  lastLockShown = lock > 0;
}

static void drawMatch(uint32_t now) {
  lcd.fillScreen(C_BG);
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
  button(ui::MATCH[2].r, "-1", lcd.color565(40, 18, 26), C_DANGER, C_DANGER, &lgfx::fonts::FreeSansBold18pt7b);
  button(ui::MATCH[3].r, "-1", lcd.color565(40, 18, 26), C_DANGER, C_DANGER, &lgfx::fonts::FreeSansBold18pt7b);
  button(ui::MATCH[4].r, "DESHACER", C_CARD, eng.canUndo() ? C_TEXT : C_LINE, C_LINE);
  button(ui::MATCH[5].r, eng.phase() == Phase::Paused ? "CONTINUAR" : "PAUSA", C_CARD, C_TEXT, C_LINE);
}

static void drawOverlayBox(const char* title, uint16_t color) {
  lcd.fillRoundRect(150, 70, 500, 340, 20, C_SURFACE);
  lcd.drawRoundRect(150, 70, 500, 340, 20, color);
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
  button(ui::PAUSED[1].r, "ABANDONAR", lcd.color565(40, 18, 26), C_DANGER, C_DANGER);
}

static void drawPeriodEnd() {
  lcd.fillScreen(C_BG);
  const char* title = eng.period() == Period::First ? "FINAL 1a PARTE" : eng.period() == Period::Second ? "EMPATE - FINAL 2a PARTE" : "FIN DE LA PRORROGA";
  const char* cta = eng.period() == Period::First ? "CONTINUAR 2a PARTE" : eng.period() == Period::Second ? "IR A PRORROGA" : "IR A PENALTIS";
  text(title, 400, 70, &lgfx::fonts::FreeSansBold18pt7b, C_TEXT);
  char buf[16];
  mfv3::Score s = eng.score();
  snprintf(buf, sizeof(buf), "%u", s.white);
  text(buf, 270, 200, &lgfx::fonts::Font8, C_WHITE_TEAM, 1.8f);
  text("-", 400, 200, &lgfx::fonts::FreeSansBold24pt7b, C_TEXT2);
  snprintf(buf, sizeof(buf), "%u", s.blue);
  text(buf, 530, 200, &lgfx::fonts::Font8, lcd.color565(79, 163, 240), 1.8f);
  button(ui::PERIOD_END[0].r, cta, C_ACCENT, C_BG, C_ACCENT, &lgfx::fonts::FreeSansBold18pt7b);
}

static void drawCountdown(uint32_t now) {
  uint32_t rem = eng.countdownRemaining(now);
  int digit = rem == 0 ? 1 : (int)((rem + 999) / 1000);
  lcd.fillScreen(C_BG);
  text(periodLabel(eng.period()), 400, 70, &lgfx::fonts::FreeSansBold18pt7b, C_TEXT);
  char buf[12];
  snprintf(buf, sizeof(buf), "%d", digit);
  lcd.drawCircle(400, 250, 130, C_ACCENT);
  lcd.drawCircle(400, 250, 129, C_ACCENT);
  text(buf, 400, 250, &lgfx::fonts::Font7, C_ACCENT, 3.0f);
  text("TOCA PARA SALTAR", 400, 440, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT2);
  lastCountdownDigit = digit;
  beep(60);
}

static void drawPenalties() {
  lcd.fillScreen(C_BG);
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
    lcd.fillRoundRect(x0, 56, 306, 300, 20, C_CARD);
    lcd.drawRoundRect(x0, 56, 306, 300, 20, isTurn ? (t == Team::White ? C_WHITE_TEAM : C_ACCENT) : C_LINE);
    text(t == Team::White ? "BLANCO" : "AZUL", x0 + 153, 84, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT);
    char buf[4];
    snprintf(buf, sizeof(buf), "%u", pen.of(t));
    text(buf, x0 + 153, 170, &lgfx::fonts::Font8, t == Team::White ? C_WHITE_TEAM : lcd.color565(79, 163, 240), 1.6f);
    // Puntos de lanzamientos
    int idx = 0;
    for (uint8_t k = 0; k < eng.kickCount() && idx < 10; k++) {
      const mfv3::Kick& kk = eng.kick(k);
      if (kk.team != t) continue;
      int cx = x0 + 40 + (idx % 5) * 56, cy = 270 + (idx / 5) * 40;
      lcd.fillCircle(cx, cy, 14, kk.scored ? C_OK : C_DANGER);
      idx++;
    }
    for (; idx < cfg.penaltyRounds; idx++) lcd.drawCircle(x0 + 40 + idx * 56, 270, 14, C_LINE);
    const ui::Rect& g = ui::PENALTIES[side * 2].r;
    const ui::Rect& m = ui::PENALTIES[side * 2 + 1].r;
    button(g, "GOL", isTurn ? lcd.color565(18, 60, 45) : C_SURFACE, isTurn ? C_OK : C_LINE, isTurn ? C_OK : C_LINE, &lgfx::fonts::FreeSansBold18pt7b);
    button(m, "FALLO", isTurn ? lcd.color565(60, 18, 28) : C_SURFACE, isTurn ? C_DANGER : C_LINE, isTurn ? C_DANGER : C_LINE, &lgfx::fonts::FreeSansBold18pt7b);
  }
  text("LANZA", 400, 120, &lgfx::fonts::FreeSansBold9pt7b, C_TEXT2);
  text(turn == Team::White ? "BLANCO" : "AZUL", 400, 160, &lgfx::fonts::FreeSansBold18pt7b, turn == Team::White ? C_WHITE_TEAM : C_ACCENT);
  button(ui::PENALTIES[4].r, "DESHACER", C_CARD, eng.kickCount() ? C_TEXT : C_LINE, C_LINE, &lgfx::fonts::FreeSansBold9pt7b);
}

static void drawFinished() {
  lcd.fillScreen(C_BG);
  for (int r = 0; r < 6; r++) lcd.drawCircle(400, 200, 180 + r * 18, lcd.color565(30 + r * 6, 26 + r * 4, 10));
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
  if (!inMatch)
    return 0x80000000UL | (remoteLinked() ? 0x01000000UL : 0) | ((uint32_t)cfg.endCondition << 16) | (cfg.goalsPerPeriod << 8) |
           cfg.minutesPerPeriod;
  mfv3::Score s = eng.score();
  uint32_t k = ((uint32_t)eng.phase() << 28) | ((uint32_t)eng.period() << 25) | ((uint32_t)s.white << 17) | ((uint32_t)s.blue << 9) |
               ((uint32_t)eng.kickCount() << 2) | (eng.canUndo() ? 2 : 0) | (confirmAbandon ? 1 : 0);
  k ^= (uint32_t)eng.matchPoint() << 30;
  if (now < flashUntil) k ^= 0x00400000UL;  // celebración del gol
  return k;
}

static void render() {
  uint32_t now = millis();
  uint32_t key = screenKey(now);
  if (key != lastKey) {
    lastKey = key;
    if (test.active) drawTest();
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
  if (!inMatch) return;
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
        s3audio::play(s3snd::Ev::Goal);
      }
    test.lastX = x;
    test.lastY = y;
    test.touches++;
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
      case Btn::Test: enterTest(); return;
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

static bool touching = false;

static void pollTouch() {
  int32_t x = 0, y = 0;
  bool now = lcd.getTouch(&x, &y) > 0;
  if (now && !touching) {
    lastActivity = millis();
    if (dimmed) {
      // El toque que despierta se consume.
      dimmed = false;
      lcd.backlight(backlightLevel());
    } else {
      onTap((int16_t)x, (int16_t)y);
    }
  }
  touching = now;
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
  lcd.detectTouchAddress();
  lcd.init();
  lcd.setRotation(0);
  lcd.backlight(backlightLevel());
  initColors();
  if (AUX_UART_RX >= 0) AuxSerial.begin(115200, SERIAL_8N1, AUX_UART_RX, AUX_UART_TX);
  loadPrefs();
  lcd.backlight(backlightLevel());  // el brillo guardado
  s3audio::begin(lcd);              // tarjeta SD y altavoz (Waveshare 7C)
  radioBegin();
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
  pollTouch();
#if HAS_ISOLATED_IO
  pollIsolatedInputs(now);
#endif

  if (inMatch && eng.tick(now)) afterChange(false, Team::White);

  if (buzzerUntil && now > buzzerUntil) {
    buzzerUntil = 0;
    if (BUZZER_PIN >= 0) digitalWrite(BUZZER_PIN, LOW);
  }

  // Atenuar la pantalla tras inactividad fuera de un partido en juego.
  bool idleScreen = !inMatch || eng.phase() == Phase::Finished;
  if (idleScreen && !dimmed && now - lastActivity > SLEEP_AFTER_MS) {
    dimmed = true;
    lcd.backlight(16);
  }

  render();
  delay(10);
}
