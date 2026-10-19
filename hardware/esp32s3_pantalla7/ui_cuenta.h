// MARCADOR FUTBOLÍN V3 · AJUSTES → CUENTA: la placa con la cuenta del dueño de la mesa (s3_nube.h).
// -------------------------------------------------------------------------------------------------
// Sin cuenta: correo y contraseña con el teclado de la pantalla (letras, números y símbolos) y ENTRAR. La
// contraseña no se guarda: solo el permiso que da la nube al entrar. Con cuenta: el correo, cómo va la
// sincronización, lo que falta por subir, SINCRONIZAR AHORA y CERRAR SESIÓN.
// Mismo estilo que AJUSTES → JUGADORES (ui_fichas.h): cada tecla solo repinta su sitio y el texto.

#ifndef MFV3_UI_CUENTA_H
#define MFV3_UI_CUENTA_H

#include "ui_base.h"
#include "ui_fichas.h"
#include "s3_nube.h"

namespace cuenta {

struct Hooks {
  void (*back)() = nullptr;
  bool (*hasWifi)() = nullptr;
  bool (*networkReady)() = nullptr;
};
static Hooks hooks;

static lv_obj_t* scr = nullptr;
static lv_obj_t *mailBox = nullptr, *passBox = nullptr, *mailText = nullptr, *passText = nullptr, *status = nullptr, *pending = nullptr;
static lv_obj_t* keys[4][11];
static char email[96] = "", pass[96] = "";
static bool editingPass = false, showPass = false, caps = false, symbols = false;
static lv_obj_t* confirmOut = nullptr;
static lv_obj_t *qrObj = nullptr, *qrStatus = nullptr;
static bool qrMode = false;  // enseñando el QR (esperando al móvil)
static uint32_t shownKey = 0;

static void go();

// ---------------------------------------------------------------- teclado
static const char* const LOWER[4][11] = {{"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "BORRAR"},
                                         {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "@"},
                                         {"a", "s", "d", "f", "g", "h", "j", "k", "l", "ñ", "."},
                                         {"MAY", "#+=", "z", "x", "c", "v", "b", "n", "m", "_", "-"}};
static const char* const SYMBOLS[4][11] = {{"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "BORRAR"},
                                           {"!", "\"", "#", "$", "%", "&", "/", "(", ")", "=", "?"},
                                           {"'", "¿", "¡", "*", "+", ",", ";", ":", "<", ">", "~"},
                                           {"MAY", "abc", "[", "]", "{", "}", "\\", "|", "^", "_", "-"}};

static const char* keyText(int r, int c) {
  static char up[4];
  const char* k = symbols ? SYMBOLS[r][c] : LOWER[r][c];
  if (!caps || symbols || r == 0 || (r == 3 && c < 2)) return k;
  if (!strcmp(k, "ñ")) return "Ñ";
  if (k[0] >= 'a' && k[0] <= 'z' && !k[1]) {
    up[0] = (char)(k[0] - 32);
    up[1] = 0;
    return up;
  }
  return k;
}

static void showFields() {
  if (!mailText || !passText) return;
  char b[110];
  snprintf(b, sizeof(b), "%s%s", email, editingPass ? "" : "|");
  lv_label_set_text(mailText, email[0] || !editingPass ? b : "Tu correo");
  if (showPass) snprintf(b, sizeof(b), "%s%s", pass, editingPass ? "|" : "");
  else {
    size_t n = 0;
    for (const char* p = pass; *p; p++)
      if (((uint8_t)*p & 0xC0) != 0x80 && n < 90) b[n++] = '*';
    b[n] = 0;
    if (editingPass) strcat(b, "|");
  }
  lv_label_set_text(passText, b);
  lv_obj_set_style_border_color(mailBox, ui::hex(editingPass ? ui::color::LINE : ui::color::ACCENT), 0);
  lv_obj_set_style_border_color(passBox, ui::hex(editingPass ? ui::color::ACCENT : ui::color::LINE), 0);
}

static void relabel() {
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 11; c++)
      if (keys[r][c]) lv_label_set_text(lv_obj_get_child(keys[r][c], 0), keyText(r, c));
}

static void onKey(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  int r = idx / 11, c = idx % 11;
  const char* k = keyText(r, c);
  char* t = editingPass ? pass : email;
  size_t n = strlen(t);
  if (!strcmp(k, "BORRAR")) {
    while (n && ((uint8_t)t[n - 1] & 0xC0) == 0x80) n--;
    if (n) n--;
    t[n] = 0;
  } else if (!strcmp(k, "MAY")) {
    caps = !caps;
    relabel();
    return;
  } else if (!strcmp(k, "#+=") || !strcmp(k, "abc")) {
    symbols = !symbols;
    relabel();
    return;
  } else if (n + strlen(k) < 95) {
    strcat(t, k);
  }
  showFields();
}

static void onField(lv_event_t* e) {
  editingPass = lv_event_get_user_data(e) != nullptr;
  showFields();
}
static void onShow(lv_event_t*) {
  showPass = !showPass;
  showFields();
}
static void onBack(lv_event_t*) {
  memset(pass, 0, sizeof(pass));
  if (hooks.back) hooks.back();
}
static void onEnter(lv_event_t*) {
  if (hooks.networkReady && !hooks.networkReady()) return ui::toast("Espera a que termine la consulta de red");
  if (hooks.hasWifi && !hooks.hasWifi()) return ui::toast("Primero, el Wi-Fi en AJUSTES");
  if (!strchr(email, '@')) return ui::toast("Escribe tu correo");
  if (strlen(pass) < 6) return ui::toast("La contraseña tiene al menos 6 caracteres");
  nube::login(email, pass);
  memset(pass, 0, sizeof(pass));
  go();
}

static void onQr(lv_event_t*) {
  if (hooks.networkReady && !hooks.networkReady()) return ui::toast("Espera a que termine la consulta de red");
  if (hooks.hasWifi && !hooks.hasWifi()) return ui::toast("Primero, el Wi-Fi en AJUSTES");
  qrMode = true;
  nube::loginWithQr();
  go();
}
static void onQrBack(lv_event_t*) {
  nube::cancelQr();
  qrMode = false;
  go();
}

static lv_obj_t* field(int x, int y, int w, bool isPass) {
  lv_obj_t* f = ui::box(scr, x, y, w, 54);
  lv_obj_set_style_radius(f, ui::RADIUS_SM, 0);
  lv_obj_set_style_bg_color(f, ui::hex(ui::color::SURFACE), 0);
  lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(f, 2, 0);
  lv_obj_add_flag(f, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(f, onField, LV_EVENT_PRESSED, isPass ? (void*)1 : nullptr);
  lv_obj_t* lab = ui::text(f, isPass ? "CONTRASEÑA" : "CORREO", 700, 12, ui::color::TEXT3, 1);
  lv_obj_set_pos(lab, 12, 4);
  lv_obj_t* t = ui::text(f, "", 800, 20, ui::color::TEXT);
  lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_width(t, w - 24);
  lv_obj_set_pos(t, 12, 22);
  if (isPass) passText = t, passBox = f;
  else mailText = t, mailBox = f;
  return f;
}

static void buildLogin() {
  fichas::header("CUENTA DE LA MESA", onBack, "ENTRAR", onEnter);
  field(16, 72, 640, false);
  fichas::btn(scr, 664, 72, 120, 54, "CON QR", 14, fichas::Kind::Primary, onQr);
  field(16, 132, 640, true);
  fichas::btn(scr, 664, 132, 120, 54, "VER", 14, fichas::Kind::Normal, onShow);
  const int KW = 66, KH = 62, X0 = 13, Y0 = 196, DX = 70, DY = 70;
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 11; c++) {
      const char* k = keyText(r, c);
      bool small = strlen(k) > 2;
      keys[r][c] = fichas::btn(scr, X0 + c * DX, Y0 + r * DY, KW, KH, k, small ? 12 : 23, fichas::Kind::Normal, onKey,
                               (void*)(intptr_t)(r * 11 + c), LV_EVENT_PRESSED);
    }
  editingPass = false;
  showFields();
}

// ---------------------------------------------------------------- entrar con el QR (el móvil confirma)
static void buildQr() {
  fichas::header("ENTRA CON EL MÓVIL", onQrBack);
  lv_obj_t* panel = ui::box(scr, 40, 88, 336, 336);
  lv_obj_set_style_radius(panel, 18, 0);
  lv_obj_set_style_bg_color(panel, ui::hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  qrObj = lv_qrcode_create(panel);
  lv_qrcode_set_size(qrObj, 300);
  lv_qrcode_set_dark_color(qrObj, ui::hex(0x000000));
  lv_qrcode_set_light_color(qrObj, ui::hex(0xffffff));
  char url[160];
  nube::qrUrl(url, sizeof(url));
  lv_qrcode_update(qrObj, url, strlen(url));
  lv_obj_center(qrObj);
  static const char* STEPS[3] = {"Abre la cámara del móvil y apunta al QR\n(o en la app: Ajustes > Cuenta > Conectar una placa).",
                                 "En la app, con tu cuenta, pulsa CONECTAR.", "La placa entra sola, sin escribir nada."};
  for (int i = 0; i < 3; i++) {
    lv_obj_t* n = ui::box(scr, 410, 96 + i * 92, 44, 44);
    lv_obj_set_style_radius(n, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(n, ui::hex(0x1aa3dc), 0);
    lv_obj_set_style_bg_opa(n, LV_OPA_COVER, 0);
    char d[4];
    snprintf(d, sizeof(d), "%d", i + 1);
    lv_obj_t* dl = ui::text(n, d, 900, 20, 0x03121e);
    lv_obj_center(dl);
    lv_obj_t* t = ui::text(scr, STEPS[i], 700, 15, ui::color::TEXT);
    lv_obj_set_width(t, 320);
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_pos(t, 466, 98 + i * 92);
  }
  qrStatus = ui::text(scr, "", 800, 17, ui::color::ACCENT);
  lv_obj_set_pos(qrStatus, 410, 380);
  shownKey = 0;
}

// ---------------------------------------------------------------- con cuenta
static void statusText(char* out, size_t n) {
  if (nube::phase == nube::BUSY) snprintf(out, n, "%s", nube::message[0] ? nube::message : "Sincronizando…");
  else if (nube::phase == nube::FAILED) snprintf(out, n, "%s", nube::message[0] ? nube::message : "No se pudo sincronizar");
  else if (!nube::lastOkAt) snprintf(out, n, "Todavía no se ha sincronizado");
  else {
    uint32_t s = (millis() - nube::lastOkAt) / 1000;
    if (s < 60) snprintf(out, n, "Sincronizado ahora mismo");
    else if (s < 3600) snprintf(out, n, "Sincronizado hace %lu min", (unsigned long)(s / 60));
    else snprintf(out, n, "Sincronizado hace %lu h", (unsigned long)(s / 3600));
  }
}

static void onSync(lv_event_t*) {
  if (hooks.networkReady && !hooks.networkReady()) return ui::toast("Espera a que termine la consulta de red");
  if (hooks.hasWifi && !hooks.hasWifi()) return ui::toast("Primero, el Wi-Fi en AJUSTES");
  nube::sync();
}
static void onOutAsk(lv_event_t*) {
  if (confirmOut) lv_obj_remove_flag(confirmOut, LV_OBJ_FLAG_HIDDEN);
}
static void onOutNo(lv_event_t*) {
  if (confirmOut) lv_obj_add_flag(confirmOut, LV_OBJ_FLAG_HIDDEN);
}
static void onOutYes(lv_event_t*) {
  nube::logout();
  ui::toast("Sesión cerrada");
  go();
}

static void buildAccount() {
  fichas::header("CUENTA DE LA MESA", onBack);
  lv_obj_t* m = ui::text(scr, nube::loggedIn() ? nube::saved.email : email, 900, 30, ui::color::TEXT);
  lv_label_set_long_mode(m, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(m, 768);
  lv_obj_set_pos(m, 16, 86);
  status = ui::text(scr, "", 800, 17, ui::color::TEXT2);
  lv_obj_set_pos(status, 16, 134);
  pending = ui::text(scr, "", 400, 16, ui::color::TEXT3);
  lv_obj_set_pos(pending, 16, 162);
  lv_obj_t* info = ui::text(scr, "Los partidos que se juegan aquí se suben solos a tu cuenta, y los jugadores\n"
                                 "y partidos de la app bajan a la placa. Se sincroniza al acabar cada partido.",
                            400, 16, ui::color::TEXT2);
  lv_obj_set_pos(info, 16, 200);
  if (!nube::loggedIn()) return;  // entrando: solo el aviso (los botones, cuando ya hay cuenta)
  fichas::btn(scr, 16, 290, 380, 76, "SINCRONIZAR AHORA", 18, fichas::Kind::Primary, onSync);
  fichas::btn(scr, 404, 290, 380, 76, "CERRAR SESIÓN", 18, fichas::Kind::Danger, onOutAsk);
  confirmOut = ui::box(scr, 0, 0, 800, 480);
  lv_obj_set_style_bg_color(confirmOut, ui::hex(0x03070d), 0);
  lv_obj_set_style_bg_opa(confirmOut, LV_OPA_COVER, 0);
  lv_obj_add_flag(confirmOut, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN));
  lv_obj_t* q = ui::text(confirmOut, "¿Cerrar sesión?", 900, 34, ui::color::TEXT);
  lv_obj_align(q, LV_ALIGN_TOP_MID, 0, 120);
  lv_obj_t* qs = ui::text(confirmOut, "Lo de la placa se queda aquí, pero deja de subirse a la cuenta.", 400, 16, ui::color::TEXT2);
  lv_obj_align(qs, LV_ALIGN_TOP_MID, 0, 176);
  fichas::btn(confirmOut, 110, 260, 280, 80, "NO", 20, fichas::Kind::Normal, onOutNo);
  fichas::btn(confirmOut, 410, 260, 280, 80, "SÍ, CERRAR", 20, fichas::Kind::Danger, onOutYes);
  shownKey = 0;
}

// ---------------------------------------------------------------- pantallas
static void onDelete(lv_event_t*) {
  scr = nullptr;
  mailBox = passBox = mailText = passText = status = pending = confirmOut = nullptr;
  qrObj = qrStatus = nullptr;
  memset(keys, 0, sizeof(keys));
}

static bool inAccount = false;
static void go() {
  lv_obj_t* old = scr;
  scr = ui::screen();
  fichas::scr = scr;  // las piezas de ui_fichas.h dibujan en fichas::scr
  lv_obj_add_event_cb(scr, onDelete, LV_EVENT_DELETE, nullptr);
  ui::gradient(scr, 0x0f2140, 0x05090f);
  mailBox = passBox = mailText = passText = status = pending = confirmOut = nullptr;
  memset(keys, 0, sizeof(keys));
  qrObj = qrStatus = nullptr;
  if (qrMode && !nube::loggedIn() && nube::phase == nube::BUSY) {
    inAccount = true;  // esperando al móvil
    buildQr();
  } else {
    qrMode = false;
    inAccount = nube::loggedIn() || nube::phase == nube::BUSY;
    if (inAccount) buildAccount();
    else buildLogin();
  }
  if (old) lv_obj_remove_event_cb(old, onDelete);
  ui::show(scr);
  fichas::scr = nullptr;
}

static bool active() { return scr && lv_screen_active() == scr; }

static void show(bool fresh) {
  if (fresh) {
    if (qrMode) nube::cancelQr();
    qrMode = false;
    memset(pass, 0, sizeof(pass));
    caps = symbols = showPass = false;
  }
  go();
}

// En cada vuelta: el estado de la sincronización (solo si cambia) y, al acabar de entrar, la pantalla que toca.
static void tick() {
  bool want = nube::loggedIn() || nube::phase == nube::BUSY;
  if (want != inAccount || (qrMode && nube::loggedIn())) {
    if (!want && nube::message[0]) ui::toast(nube::message);
    qrMode = false;
    return go();
  }
  if (qrStatus) {
    const char* m = nube::message[0] ? nube::message : "Esperando al móvil…";
    if (strcmp(lv_label_get_text(qrStatus), m)) lv_label_set_text(qrStatus, m);
    return;
  }
  if (!status) return;
  char s[120], p[60];
  statusText(s, sizeof(s));
  if (!nube::loggedIn()) p[0] = 0;
  else if (nube::pendingCount) snprintf(p, sizeof(p), "Pendiente de subir: %d", nube::pendingCount);
  else snprintf(p, sizeof(p), "Todo subido");
  uint32_t k = 2166136261u;
  for (const char* c = s; *c; c++) k = (k ^ (uint8_t)*c) * 16777619u;
  for (const char* c = p; *c; c++) k = (k ^ (uint8_t)*c) * 16777619u;
  if (k == shownKey) return;
  shownKey = k;
  lv_label_set_text(status, s);
  lv_obj_set_style_text_color(status, ui::hex(nube::phase == nube::FAILED ? ui::color::DANGER : ui::color::TEXT2), 0);
  lv_label_set_text(pending, p);
}

}  // namespace cuenta

#endif  // MFV3_UI_CUENTA_H
