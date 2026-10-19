// MARCADOR FUTBOLÍN V3 · pantalla de 7": AJUSTES (Wi-Fi con teclado, actualizar, volumen, prueba).
// -------------------------------------------------------------------------------------------------
// Se incluye A MITAD del sketch (después de los colores, lcd, prefs, text() y button()), porque dibuja con
// ellos. Todo lo de red está en s3_net.h; el protocolo para pasar el programa al mando, en mfv3_ota.h.
//
// Actualizar:
//   - Al encender (si hay Wi-Fi guardado) y con BUSCAR, la placa se conecta un momento y lee qué versión hay
//     publicada de la pantalla y del mando. Si hay alguna nueva, en Inicio sale el botón ACTUALIZAR.
//   - INSTALAR: primero el mando (la pantalla descarga su programa y se lo pasa por radio; hay que mantener
//     pulsados los dos botones del mando 3 s), después la propia pantalla, que se reinicia con la versión nueva.
//   - Nunca durante un partido: si empieza uno, la comprobación se deja.

#ifndef MFV3_S3_AJUSTES_H
#define MFV3_S3_AJUSTES_H

static const char* MANDO_ID = "mando-c3";  // programa del mando en la web (ESP32-C3 Super Mini)

// ---------------------------------------------------------------- Wi-Fi
static s3net::Network nets[ui::WIFI_ROWS];
static uint8_t netCount = 0;
static bool netScanned = false;

static char kbSsid[33] = "";
static char kbText[65] = "";
static uint8_t kbMode = 0;  // 0 minúsculas, 1 mayúsculas, 2 símbolos
static bool kbShow = false;
static const char* kbError = nullptr;

static void saveWifi(const char* ssid, const char* pass) {
  strncpy(wifiSsid, ssid, sizeof(wifiSsid) - 1);
  strncpy(wifiPass, pass, sizeof(wifiPass) - 1);
  prefs.putString("wssid", wifiSsid);
  prefs.putString("wpass", wifiPass);
}

static void forgetWifi() {
  wifiSsid[0] = wifiPass[0] = 0;
  prefs.putString("wssid", "");
  prefs.putString("wpass", "");
}

// ---------------------------------------------------------------- versiones
static mfv3::ota::Published pubScreen, pubMando;
enum class CheckState : uint8_t { Never, Running, Ok, NoConnect, Failed };
static CheckState checkState = CheckState::Never;

enum class MandoUpd : uint8_t { Unknown, NeedsCable, UpToDate, Outdated };

// El mando dice su versión con cada saludo (cada 20 s). Si se le oye pero nunca la dice, tiene un programa
// anterior a las actualizaciones por radio: hay que ponérselo una vez por USB.
static bool mandoInfoFresh() { return mandoInfoAt != 0 && millis() - mandoInfoAt < 90000UL; }
static MandoUpd mandoUpd() {
  if (!mandoInfoFresh()) return remoteLinked() ? MandoUpd::NeedsCable : MandoUpd::Unknown;
  return pubMando.ok() && pubMando.build > mandoBuild ? MandoUpd::Outdated : MandoUpd::UpToDate;
}
static bool screenOutdated() { return pubScreen.ok() && pubScreen.build > (uint32_t)MFV3_BUILD; }
static bool updateAvailable() { return screenOutdated() || mandoUpd() == MandoUpd::Outdated; }

static void fetchVersions() {
  mfv3::ota::Published a, b;
  bool ok = s3net::fetchPublished(S3_BOARD_ID, a);
  s3net::fetchPublished(MANDO_ID, b);  // si no está publicado, el mando se queda como está
  if (ok) {
    pubScreen = a;
    pubMando = b;
  }
  checkState = ok ? CheckState::Ok : CheckState::Failed;
}

// Comprobación sin bloquear: conectar (en loop()), leer las versiones y desconectar.
static void startCheck() {
  if (!wifiSsid[0] || inMatch || s3net::link != s3net::Link::Off || nube::phase == nube::BUSY) return;
  s3net::connect(wifiSsid, wifiPass);
  checkState = CheckState::Running;
}

static void checkPoll() {
  if (checkState != CheckState::Running || screen == Screen::Connecting || screen == Screen::Install) return;
  if (nube::phase == nube::BUSY) return;  // la nube tiene el Wi-Fi
  if (inMatch) {  // nunca durante un partido
    s3net::stop();
    checkState = CheckState::Never;
    return;
  }
  s3net::Link l = s3net::poll();
  if (l == s3net::Link::Up) {
    fetchVersions();
    rtc.syncInternet();  // de paso, la hora de Internet al reloj con pila
    s3net::stop();
  } else if (l == s3net::Link::Failed) {
    s3net::stop();
    checkState = CheckState::NoConnect;
  }
}

// ---------------------------------------------------------------- instalar
enum class Inst : uint8_t { MandoConnect, MandoDownload, MandoRadio, MandoConfirm, ScreenConnect, ScreenDownload, Restart, Done, Error };
static Inst inst = Inst::Done;
static const char* instError = "";
static uint8_t instPct = 0;
static uint32_t instAt = 0;
static bool instMando = false, instScreen = false, instStepDrawn = false;
static mfv3::ota::Sender sender;
static s3net::SlotSource slotSource;

static void instGoScreen() {
  if (instScreen) {
    inst = Inst::ScreenConnect;
    s3net::connect(wifiSsid, wifiPass);
  } else {
    inst = Inst::Done;
  }
  instPct = 0;
  instStepDrawn = false;
}

static void instFail(const char* why) {
  s3net::stop();
  instError = why;
  inst = Inst::Error;
}

static void startInstall() {
  instMando = mandoUpd() == MandoUpd::Outdated;
  instScreen = screenOutdated();
  if (!instMando && !instScreen) return;
  s3net::stop();
  checkState = CheckState::Ok;
  screen = Screen::Install;
  instPct = 0;
  instStepDrawn = false;
  if (instMando) {
    inst = Inst::MandoConnect;
    s3net::connect(wifiSsid, wifiPass);
  } else {
    instGoScreen();
  }
}

static void drawProgressBar(uint8_t pct);
static void instProgress(uint32_t done, uint32_t total) {
  uint8_t p = total ? (uint8_t)((uint64_t)done * 100 / total) : 0;
  if (p != instPct) {
    instPct = p;
    drawProgressBar(p);
  }
}

static void installPoll(uint32_t now) {
  if (screen != Screen::Install) return;
  switch (inst) {
    case Inst::MandoConnect:
    case Inst::ScreenConnect: {
      s3net::Link l = s3net::poll();
      if (l == s3net::Link::Up) {
        inst = inst == Inst::MandoConnect ? Inst::MandoDownload : Inst::ScreenDownload;
        instStepDrawn = false;
      } else if (l == s3net::Link::Failed) {
        instFail("No se pudo conectar al Wi-Fi");
      }
      break;
    }
    case Inst::MandoDownload: {
      if (!instStepDrawn) return;  // primero se dibuja la pantalla de «descargando»; después, a descargar
      bool ok = s3net::downloadToSlot(MANDO_ID, pubMando, instProgress);
      s3net::stop();  // la radio vuelve a su canal para hablar con el mando
      uint8_t md5[16];
      if (!ok || !mfv3::ota::md5FromHex(pubMando.md5, md5)) {
        instFail("No se pudo descargar el programa del mando");
        break;
      }
      sender.start(GRUPO_MESA, pubMando.build, pubMando.size, md5, now);
      inst = Inst::MandoRadio;
      instPct = 0;
      break;
    }
    case Inst::MandoRadio: {
      otaDrain([](const uint8_t* p, int len) { sender.onPacket(p, len, millis()); });
      uint8_t buf[mfv3::ota::MAX_PACKET];
      uint8_t n = sender.poll(now, slotSource, buf);
      if (n) radioSend(buf, n);
      instPct = sender.percent();
      if (sender.state() == mfv3::ota::Sender::State::Done ||
          (sender.state() == mfv3::ota::Sender::State::Failed && sender.error() == mfv3::ota::Sender::Error::Lost &&
           sender.sent() == sender.size())) {
        inst = Inst::MandoConfirm;  // si la última confirmación se perdió, lo dirá el mando al reiniciar
        instAt = now;
      } else if (sender.state() == mfv3::ota::Sender::State::Failed) {
        switch (sender.error()) {
          case mfv3::ota::Sender::Error::NoAnswer: instFail("El mando no contesta: acercalo y vuelve a probar"); break;
          case mfv3::ota::Sender::Error::Verify: instFail("El programa llego mal al mando (sigue con el anterior)"); break;
          case mfv3::ota::Sender::Error::Refused: instFail("El mando no lo acepta (sigue con el anterior)"); break;
          default: instFail("Se perdio la conexion con el mando"); break;
        }
      }
      break;
    }
    case Inst::MandoConfirm:
      if ((mandoBuild == pubMando.build && mandoInfoAt > instAt) || now - instAt > 15000UL) {
        if (mandoBuild != pubMando.build && sender.state() != mfv3::ota::Sender::State::Done)
          instFail("El mando no confirmo la version nueva");
        else
          instGoScreen();
      }
      break;
    case Inst::ScreenDownload: {
      if (!instStepDrawn) return;
      if (s3net::selfUpdate(S3_BOARD_ID, pubScreen, instProgress)) {
        s3net::stop();
        inst = Inst::Restart;
        instAt = now;
      } else {
        instFail("No se pudo descargar la version nueva");
      }
      break;
    }
    case Inst::Restart:
      if (now - instAt > 1500) ESP.restart();
      break;
    default: break;
  }
}

// ---------------------------------------------------------------- dibujo
static void textLeft(const char* s, int x, int y, const lgfx::IFont* font, uint16_t color) {
  gfx.setFont(font);
  gfx.setTextSize(1);
  gfx.setTextColor(color);
  gfx.setTextDatum(lgfx::textdatum_t::middle_left);
  gfx.drawString(s, x, y);
}
static void textRight(const char* s, int x, int y, const lgfx::IFont* font, uint16_t color) {
  gfx.setFont(font);
  gfx.setTextSize(1);
  gfx.setTextColor(color);
  gfx.setTextDatum(lgfx::textdatum_t::middle_right);
  gfx.drawString(s, x, y);
}

static void titleBar(const char* title, uint16_t color) {
  gfx.fillScreen(C_BG);
  textLeft(title, 40, 36, &lgfx::fonts::FreeSansBold18pt7b, color);
  button(ui::BACK, "VOLVER", C_CARD, C_TEXT, C_LINE);
}

// Fila de ajustes: nombre a la izquierda y su valor a la derecha.
static void settingRow(const ui::Rect& r, const char* label, const char* value, uint16_t valueColor) {
  gfx.fillRoundRect(r.x, r.y, r.w, r.h, 14, C_CARD);
  gfx.drawRoundRect(r.x, r.y, r.w, r.h, 14, C_LINE);
  textLeft(label, r.x + 24, r.y + r.h / 2, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT);
  if (value) textRight(value, r.x + r.w - 24, r.y + r.h / 2, &lgfx::fonts::FreeSans12pt7b, valueColor);
}

static void buildLabel(char* out, size_t n, uint32_t build) {
  if (build) snprintf(out, n, "Version %lu", (unsigned long)build);
  else snprintf(out, n, "Version de prueba");
}

static void drawSettings() {
  titleBar("AJUSTES", C_ACCENT);
  settingRow(ui::SETTINGS[1].r, "WI-FI", wifiSsid[0] ? wifiSsid : "Sin configurar", wifiSsid[0] ? C_TEXT2 : C_GOLD);
  char v[40];
  uint16_t vc = C_TEXT2;
  if (updateAvailable()) {
    snprintf(v, sizeof(v), "Hay version nueva");
    vc = C_GOLD;
  } else if (checkState == CheckState::Running) {
    snprintf(v, sizeof(v), "Buscando...");
  } else {
    buildLabel(v, sizeof(v), MFV3_BUILD);
  }
  settingRow(ui::SETTINGS[2].r, "ACTUALIZAR", v, vc);
#if HAS_AUDIO
  const ui::Rect vr = {40, 208, 720, 60};
  settingRow(vr, "VOLUMEN", nullptr, C_TEXT2);
  button(ui::SETTINGS[3].r, "-", C_SURFACE, C_TEXT, C_LINE, &lgfx::fonts::FreeSansBold24pt7b);
  button(ui::SETTINGS[4].r, "+", C_SURFACE, C_TEXT, C_LINE, &lgfx::fonts::FreeSansBold24pt7b);
  snprintf(v, sizeof(v), "%u%%", (unsigned)sndVolume);
  text(v, 610, 238, &lgfx::fonts::FreeSansBold18pt7b, sndVolume ? C_TEXT : C_LINE);
#endif
  settingRow(ui::SETTINGS[5].r, "PRUEBA DE LA PLACA", nullptr, C_TEXT2);
  snprintf(v, sizeof(v), "%u jugadores", (unsigned)jugadores.count);
  settingRow(ui::SETTINGS[6].r, "JUGADORES", v, C_TEXT2);
  settingRow(ui::SETTINGS[7].r, "CUENTA", nube::loggedIn() ? nube::saved.email : "Sin cuenta", nube::loggedIn() ? C_TEXT2 : C_GOLD);
}

static void signalBars(int x, int y, int8_t rssi) {
  uint8_t bars = rssi > -55 ? 4 : rssi > -67 ? 3 : rssi > -78 ? 2 : 1;
  for (uint8_t i = 0; i < 4; i++) {
    int h = 8 + i * 7;
    gfx.fillRect(x + i * 9, y + 14 - h, 6, h, i < bars ? C_TEXT2 : C_LINE);
  }
}

static void drawWifi() {
  titleBar("WI-FI", C_ACCENT);
  if (!netScanned) {
    text("Buscando redes...", ui::W / 2, 240, &lgfx::fonts::FreeSansBold18pt7b, C_TEXT2);
    return;
  }
  if (!netCount) text("No se ve ninguna red", ui::W / 2, 200, &lgfx::fonts::FreeSansBold18pt7b, C_TEXT2);
  for (uint8_t i = 0; i < netCount; i++) {
    const ui::Rect& r = ui::WIFI[1 + i].r;
    bool saved = wifiSsid[0] && !strcmp(nets[i].ssid, wifiSsid);
    gfx.fillRoundRect(r.x, r.y, r.w, r.h, 12, saved ? gfx.color565(20, 60, 85) : C_CARD);
    gfx.drawRoundRect(r.x, r.y, r.w, r.h, 12, saved ? C_ACCENT : C_LINE);
    textLeft(nets[i].ssid, r.x + 20, r.y + r.h / 2, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT);
    if (nets[i].open) textRight("abierta", r.x + r.w - 70, r.y + r.h / 2, &lgfx::fonts::FreeSans9pt7b, C_TEXT2);
    signalBars(r.x + r.w - 56, r.y + r.h / 2 - 8, nets[i].rssi);
  }
  button(ui::WIFI[6].r, "BUSCAR OTRA VEZ", C_SURFACE, C_TEXT, C_LINE);
  if (wifiSsid[0]) button(ui::WIFI[7].r, "OLVIDAR LA RED", C_SURFACE, C_DANGER, C_LINE);
}

static void drawKeyboard() {
  gfx.fillScreen(C_BG);
  char t[64];
  if (kbError) {
    textLeft(kbError, 40, 36, &lgfx::fonts::FreeSansBold12pt7b, C_DANGER);
  } else {
    snprintf(t, sizeof(t), "Clave de %s", kbSsid);
    textLeft(t, 40, 36, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT);
  }
  button(ui::BACK, "VOLVER", C_CARD, C_TEXT, C_LINE);
  const ui::Rect& f = ui::KEY_FIELD;
  gfx.fillRoundRect(f.x, f.y, f.w, f.h, 10, C_SURFACE);
  gfx.drawRoundRect(f.x, f.y, f.w, f.h, 10, C_ACCENT);
  char shown[65];
  size_t n = strlen(kbText);
  for (size_t i = 0; i < n; i++) shown[i] = kbShow ? kbText[i] : '*';
  shown[n] = 0;
  // Si no cabe, se ve el final (lo último que se ha escrito).
  const char* vis = shown;
  gfx.setFont(&lgfx::fonts::FreeSansBold12pt7b);
  while (*vis && gfx.textWidth(vis) > f.w - 30) vis++;
  textLeft(vis, f.x + 16, f.y + f.h / 2, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT);
  button(ui::KEYBOARD[1].r, kbShow ? "OCULTAR" : "VER", C_CARD, C_TEXT, C_LINE, &lgfx::fonts::FreeSansBold9pt7b);
  const char* const* rows = kbMode == 2 ? ui::KEYS_SYMBOLS : kbMode == 1 ? ui::KEYS_UPPER : ui::KEYS_LOWER;
  for (uint8_t r = 0; r < 4; r++) {
    uint8_t len = (uint8_t)strlen(rows[r]);
    for (uint8_t c = 0; c < len; c++) {
      ui::Rect k = ui::keyRect(r, c, len);
      char s[2] = {rows[r][c], 0};
      button(k, s, C_CARD, C_TEXT, C_LINE, &lgfx::fonts::FreeSansBold18pt7b);
    }
  }
  button(ui::KEYBOARD[2].r, "BORRAR", C_SURFACE, C_TEXT, C_LINE);
  bool caps = kbMode == 1;
  button(ui::KEYBOARD[3].r, "MAYUS", caps ? gfx.color565(20, 60, 85) : C_SURFACE, C_TEXT, caps ? C_ACCENT : C_LINE);
  button(ui::KEYBOARD[4].r, kbMode == 2 ? "ABC" : "#+=", C_SURFACE, C_TEXT, C_LINE);
  button(ui::KEYBOARD[5].r, "ESPACIO", C_SURFACE, C_TEXT, C_LINE);
  button(ui::KEYBOARD[6].r, "CONECTAR", C_ACCENT, C_BG, C_ACCENT);
}

static void drawConnecting() {
  gfx.fillScreen(C_BG);
  char t[64];
  snprintf(t, sizeof(t), "Conectando a %s...", kbSsid);
  text(t, ui::W / 2, 220, &lgfx::fonts::FreeSansBold18pt7b, C_TEXT);
}

static void versionCard(int y, const char* name, const char* now, const char* status, uint16_t statusColor) {
  gfx.fillRoundRect(40, y, 720, 104, 16, C_CARD);
  gfx.drawRoundRect(40, y, 720, 104, 16, C_LINE);
  textLeft(name, 64, y + 34, &lgfx::fonts::FreeSansBold18pt7b, C_TEXT);
  textLeft(now, 64, y + 74, &lgfx::fonts::FreeSans12pt7b, C_TEXT2);
  textRight(status, 736, y + 52, &lgfx::fonts::FreeSansBold18pt7b, statusColor);
}

static void drawUpdate() {
  titleBar("ACTUALIZAR", C_ACCENT);
  char now[32], st[32];
  buildLabel(now, sizeof(now), MFV3_BUILD);
  uint16_t c = C_OK;
  if (screenOutdated()) {
    snprintf(st, sizeof(st), "Nueva: %lu", (unsigned long)pubScreen.build);
    c = C_GOLD;
  } else if (pubScreen.ok()) {
    snprintf(st, sizeof(st), "Al dia");
  } else {
    snprintf(st, sizeof(st), "-");
    c = C_LINE;
  }
  versionCard(80, "PANTALLA", now, st, c);
  MandoUpd m = mandoUpd();
  if (m == MandoUpd::Unknown) {
    snprintf(now, sizeof(now), "No se oye: pulsa un boton");
    snprintf(st, sizeof(st), "-");
    c = C_LINE;
  } else if (m == MandoUpd::NeedsCable) {
    snprintf(now, sizeof(now), "Version antigua");
    snprintf(st, sizeof(st), "Una vez por USB");
    c = C_GOLD;
  } else {
    buildLabel(now, sizeof(now), mandoBuild);
    if (m == MandoUpd::Outdated) {
      snprintf(st, sizeof(st), "Nueva: %lu", (unsigned long)pubMando.build);
      c = C_GOLD;
    } else if (pubMando.ok()) {
      snprintf(st, sizeof(st), "Al dia");
      c = C_OK;
    } else {
      snprintf(st, sizeof(st), "-");
      c = C_LINE;
    }
  }
  versionCard(198, "MANDO", now, st, c);
  const char* msg = nullptr;
  uint16_t mc = C_TEXT2;
  if (!wifiSsid[0]) msg = "Primero, el Wi-Fi en AJUSTES";
  else if (checkState == CheckState::Running) msg = "Buscando versiones...";
  else if (checkState == CheckState::NoConnect) { msg = "No se pudo conectar al Wi-Fi"; mc = C_DANGER; }
  else if (checkState == CheckState::Failed) { msg = "No se pudo consultar la web"; mc = C_DANGER; }
  if (msg) text(msg, ui::W / 2, 342, &lgfx::fonts::FreeSansBold12pt7b, mc);
  button(ui::UPDATE[1].r, "BUSCAR", C_SURFACE, wifiSsid[0] ? C_TEXT : C_LINE, C_LINE, &lgfx::fonts::FreeSansBold18pt7b);
  bool can = updateAvailable() && wifiSsid[0];
  button(ui::UPDATE[2].r, "INSTALAR", can ? C_ACCENT : C_SURFACE, can ? C_BG : C_LINE, can ? C_ACCENT : C_LINE,
         &lgfx::fonts::FreeSansBold18pt7b);
}

static const ui::Rect PROGRESS = {60, 250, 680, 40};
static void drawProgressBar(uint8_t pct) {
  gfx.fillRoundRect(PROGRESS.x, PROGRESS.y, PROGRESS.w, PROGRESS.h, 12, C_SURFACE);
  gfx.drawRoundRect(PROGRESS.x, PROGRESS.y, PROGRESS.w, PROGRESS.h, 12, C_LINE);
  int w = (int)(PROGRESS.w - 8) * pct / 100;
  if (w > 0) gfx.fillRoundRect(PROGRESS.x + 4, PROGRESS.y + 4, w, PROGRESS.h - 8, 9, C_ACCENT);
  char t[8];
  snprintf(t, sizeof(t), "%u%%", pct);
  gfx.fillRect(PROGRESS.x, PROGRESS.y + PROGRESS.h + 6, PROGRESS.w, 30, C_BG);
  text(t, ui::W / 2, PROGRESS.y + PROGRESS.h + 22, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT2);
}

static void drawInstall() {
  gfx.fillScreen(C_BG);
  uint8_t steps = (instMando ? 1 : 0) + (instScreen ? 1 : 0);
  bool mandoStep = inst <= Inst::MandoConfirm;
  char t[48];
  if (inst != Inst::Done && inst != Inst::Error) {
    snprintf(t, sizeof(t), "%u de %u  |  %s", (unsigned)(mandoStep || !instMando ? 1 : 2), (unsigned)steps,
             mandoStep ? "MANDO" : "PANTALLA");
    text(t, ui::W / 2, 40, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT2);
  }
  const char* big = "";
  const char* small = nullptr;
  bool bar = false;
  switch (inst) {
    case Inst::MandoConnect:
    case Inst::ScreenConnect: big = "Conectando al Wi-Fi..."; break;
    case Inst::MandoDownload: big = "Descargando el programa del mando"; bar = true; break;
    case Inst::MandoRadio:
      if (sender.state() == mfv3::ota::Sender::State::Offering) {
        big = sender.waitingButtons() ? "Manten pulsados los dos botones" : "Buscando el mando...";
        small = sender.waitingButtons() ? "del mando 3 segundos" : "Pulsa un boton del mando para despertarlo";
      } else {
        big = "Pasando el programa al mando";
        small = "Deja el mando cerca y encendido";
        bar = true;
      }
      break;
    case Inst::MandoConfirm: big = "El mando se esta reiniciando"; break;
    case Inst::ScreenDownload: big = "Descargando la version nueva"; small = "No apagues la placa"; bar = true; break;
    case Inst::Restart: big = "Listo. Reiniciando..."; break;
    case Inst::Done: big = "Listo, todo al dia"; break;
    case Inst::Error: big = "No se pudo actualizar"; small = instError; break;
  }
  text(big, ui::W / 2, 150, &lgfx::fonts::FreeSansBold18pt7b, inst == Inst::Error ? C_DANGER : inst == Inst::Done ? C_OK : C_TEXT);
  if (small) text(small, ui::W / 2, 196, &lgfx::fonts::FreeSansBold12pt7b, C_TEXT2);
  if (bar) drawProgressBar(instPct);
  if (inst == Inst::MandoRadio && sender.state() == mfv3::ota::Sender::State::Offering)
    button(ui::INSTALL[0].r, instScreen ? "SALTAR EL MANDO" : "CANCELAR", C_SURFACE, C_TEXT, C_LINE);
  if (inst == Inst::Done || inst == Inst::Error) button(ui::INSTALL[1].r, "VOLVER", C_ACCENT, C_BG, C_ACCENT, &lgfx::fonts::FreeSansBold18pt7b);
  instStepDrawn = true;
}

static void drawAjustes() {
  switch (screen) {
    case Screen::Settings: drawSettings(); break;
    case Screen::Wifi: drawWifi(); break;
    case Screen::Keyboard: drawKeyboard(); break;
    case Screen::Connecting: drawConnecting(); break;
    case Screen::Update: drawUpdate(); break;
    case Screen::Install: drawInstall(); break;
    default: break;
  }
}

// Lo que se ve en estas pantallas, resumido: si cambia, se redibuja.
static uint32_t ajustesKey() {
  uint32_t h = 2166136261u;
  auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
  mix((uint32_t)screen);
  mix((uint32_t)checkState);
  mix(wifiSsid[0] ? (uint32_t)strlen(wifiSsid) * 31u + (uint8_t)wifiSsid[0] : 0);
  mix(pubScreen.build);
  mix(pubMando.build);
  mix((uint32_t)mandoUpd());
  mix(mandoBuild);
  switch (screen) {
    case Screen::Settings:
      mix(sndVolume);
      mix(jugadores.count);
      mix(nube::loggedIn());
      break;
    case Screen::Wifi: mix(netScanned); mix(netCount); break;
    case Screen::Keyboard:
      mix(kbMode); mix(kbShow); mix((uint32_t)(uintptr_t)kbError);
      for (const char* p = kbText; *p; p++) mix((uint8_t)*p);
      break;
    case Screen::Install:
      mix((uint32_t)inst);
      mix((uint32_t)sender.state());
      mix(sender.waitingButtons());
      mix(inst == Inst::MandoRadio ? instPct : 0);  // en las descargas, la barra se pinta sola
      break;
    default: break;
  }
  return h | 1;  // nunca 0xFFFFFFFF ni las claves del marcador (que llevan el bit 31 o la fase)
}

// ---------------------------------------------------------------- toques
static void openSettings() { screen = Screen::Settings; }

// JUGADORES: crear, cambiar y borrar (ui_fichas.h, pantalla nueva de LVGL).
static bool fichasFresh = false;
static bool cuentaFresh = false;
static void openAccount() {
  screen = Screen::Account;
  cuentaFresh = true;
}
static void openPlayers() {
  screen = Screen::Players;
  fichasFresh = true;
}

static void openWifi() {
  screen = Screen::Wifi;
  netScanned = false;
  netCount = 0;
}

static void openUpdate() {
  screen = Screen::Update;
  if (checkState != CheckState::Running && checkState != CheckState::Ok) startCheck();
}

static void openKeyboard(const char* ssid) {
  strncpy(kbSsid, ssid, sizeof(kbSsid) - 1);
  kbSsid[sizeof(kbSsid) - 1] = 0;
  bool same = wifiSsid[0] && !strcmp(ssid, wifiSsid);
  strncpy(kbText, same ? wifiPass : "", sizeof(kbText) - 1);
  kbMode = 0;
  kbShow = false;
  kbError = nullptr;
  screen = Screen::Keyboard;
}

static void startConnect() {
  s3net::stop();  // por si había una comprobación a medias
  if (checkState == CheckState::Running) checkState = CheckState::Never;
  s3net::connect(kbSsid, kbText);
  screen = Screen::Connecting;
}

// Conexión desde el teclado: si funciona, se guarda y se miran las versiones.
static void connectingPoll() {
  if (screen != Screen::Connecting) return;
  s3net::Link l = s3net::poll();
  if (l == s3net::Link::Up) {
    saveWifi(kbSsid, kbText);
    fetchVersions();
    rtc.syncInternet();
    s3net::stop();
    screen = Screen::Update;
  } else if (l == s3net::Link::Failed) {
    s3net::stop();
    kbError = "No conecta: revisa la clave";
    screen = Screen::Keyboard;
  }
}

static void ajustesTap(int16_t x, int16_t y) {
  switch (screen) {
    case Screen::Settings: {
      ui::Btn b = ui::hit(ui::SETTINGS, x, y);
      if (b == ui::Btn::Back) goHome();
      else if (b == ui::Btn::SetWifi) openWifi();
      else if (b == ui::Btn::SetUpdate) openUpdate();
      else if (b == ui::Btn::SetPlayers) openPlayers();
      else if (b == ui::Btn::SetAccount) openAccount();
      else if (b == ui::Btn::SetTest) {
        screen = Screen::Home;
        enterTest();
      }
#if HAS_AUDIO
      else if (b == ui::Btn::VolMinus || b == ui::Btn::VolPlus) {
        int v = (int)sndVolume + (b == ui::Btn::VolPlus ? 10 : -10);
        sndVolume = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
        prefs.putUChar("vol", sndVolume);
        player.setVolume(sndVolume);
        playTestTone();  // para oír cómo queda
      }
#endif
      break;
    }
    case Screen::Wifi: {
      ui::Btn b = ui::hit(ui::WIFI, x, y);
      if (b == ui::Btn::Back) screen = Screen::Settings;
      else if (b == ui::Btn::WifiRescan) openWifi();
      else if (b == ui::Btn::WifiForget && wifiSsid[0]) forgetWifi();
      else if (b >= ui::Btn::WifiNet0 && b <= ui::Btn::WifiNet4) {
        uint8_t i = (uint8_t)b - (uint8_t)ui::Btn::WifiNet0;
        if (i < netCount) {
          openKeyboard(nets[i].ssid);
          if (nets[i].open) startConnect();  // red sin clave: directamente
        }
      }
      break;
    }
    case Screen::Keyboard: {
      ui::Btn b = ui::hit(ui::KEYBOARD, x, y);
      size_t n = strlen(kbText);
      if (b == ui::Btn::Back) screen = Screen::Wifi;
      else if (b == ui::Btn::KeyShow) kbShow = !kbShow;
      else if (b == ui::Btn::KeyBack) { if (n) kbText[n - 1] = 0; }
      else if (b == ui::Btn::KeyShift) kbMode = kbMode == 1 ? 0 : 1;
      else if (b == ui::Btn::KeyMode) kbMode = kbMode == 2 ? 0 : 2;
      else if (b == ui::Btn::KeySpace) { if (n < sizeof(kbText) - 1) { kbText[n] = ' '; kbText[n + 1] = 0; } }
      else if (b == ui::Btn::KeyConnect) startConnect();
      else {
        char c = ui::keyAt(kbMode, x, y);
        if (c && n < sizeof(kbText) - 1) {
          kbText[n] = c;
          kbText[n + 1] = 0;
        }
      }
      if (b != ui::Btn::None || n != strlen(kbText)) kbError = nullptr;
      break;
    }
    case Screen::Update: {
      ui::Btn b = ui::hit(ui::UPDATE, x, y);
      if (b == ui::Btn::Back) screen = Screen::Settings;
      else if (b == ui::Btn::UpdSearch && wifiSsid[0] && checkState != CheckState::Running) {
        checkState = CheckState::Never;
        startCheck();
      } else if (b == ui::Btn::UpdInstall && updateAvailable() && wifiSsid[0]) startInstall();
      break;
    }
    case Screen::Install:
      if (inst == Inst::MandoRadio && sender.state() == mfv3::ota::Sender::State::Offering && ui::INSTALL[0].r.contains(x, y)) {
        sender.cancel();
        uint8_t buf[mfv3::ota::MAX_PACKET];
        uint8_t n = sender.poll(millis(), slotSource, buf);  // avisa al mando
        if (n) radioSend(buf, n);
        instMando = false;
        instGoScreen();
      } else if ((inst == Inst::Done || inst == Inst::Error) && ui::INSTALL[1].r.contains(x, y)) {
        screen = Screen::Update;
      }
      break;
    default: break;
  }
}

// En loop(): buscar redes (bloquea 2–4 s, con la pantalla ya dibujada), conexiones e instalación.
static void ajustesLoop(uint32_t now) {
  if (screen == Screen::Wifi && !netScanned && lastKey == ajustesKey()) {
    s3net::stop();
    netCount = s3net::scan(nets, ui::WIFI_ROWS);
    netScanned = true;
  }
  connectingPoll();
  checkPoll();
  installPoll(now);
}

#endif  // MFV3_S3_AJUSTES_H
