// Capturas de la pantalla de 7" en el PC: el mismo sketch con LVGL dibujando en memoria, con las capas y letras de
// build/placa/fat (scripts/placa_recursos.py). Guarda <carpeta>/<escena>.rgb565 (800×480, RGB565) por escena; el
// script scripts/capturas_placa_pc.py las pasa a PNG. Uso: sh test/s3_build.sh 4 test/s3_capturas.cpp
// Las escenas siguen las mismas jugadas que scripts/capturas-app.mjs, para compararlas una a una.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <map>
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
static void usb(const char* s) { for (const char* c = s; *c; c++) sim::in.push_back(*c); sim::in.push_back('\n'); loop(); }

static const char* OUT = "../build/placa/capturas-pc";
static void shot(const char* name, uint32_t settle = 400) {
  run(settle);
  lv_refr_now(nullptr);
  char path[256];
  snprintf(path, sizeof(path), "%s/%s.rgb565", OUT, name);
  FILE* f = fopen(path, "wb");
  assert(f);
  fwrite(uiport::shown, 2, 800 * 480, f);
  fclose(f);
  std::printf("ok  %s\n", name);
}

// Elegir jugadores (si los hay): Alex en el Blanco y el primero libre en el Azul.
static void pickPlayers() {
  if (!elegir::active()) return;
  tap(267, 154);   // primera ficha de la lista (Alex)
  tap(670, 438);   // SIGUIENTE: AZUL
  tap(267, 154);   // primera libre
  tap(695, 438);   // CONTINUAR
}

// Inicio → RÁPIDO → Configuración (por goles a 5, o por tiempo con 5 min) → CONTINUAR.
static void toMatch(bool time) {
  if (!inicio::active()) {
    goHome();
    run(300);
  }
  tap(140, 280); tap(140, 280);
  run(300);
  cfg.goalsPerPeriod = 5;
  cfg.minutesPerPeriod = 5;
  cfg.endCondition = time ? mfv3::EndCondition::Time : mfv3::EndCondition::Goals;
  goHome();
  run(100);
  tap(140, 280); tap(140, 280);
  run(300);
  tap(400, 443);  // CONTINUAR
  run(200);
  pickPlayers();
}
// Las fotos guardadas en la FAT simulada, al disco del PC (LVGL las lee de ahí, como en la placa).
static void syncFat() {
  std::filesystem::create_directories("../build/placa/fat/jugadores");
  for (auto& f : FFat.disk.files)
    if (f.first.rfind("/jugadores/", 0) == 0) {
      std::string path = std::string("../build/placa/fat") + f.first;
      if (FILE* o = fopen(path.c_str(), "wb")) {
        fwrite(f.second.data(), 1, f.second.size(), o);
        fclose(o);
      }
    }
}

// ---------------------------------------------------------------- nube simulada (Supabase: auth y mesa_datos)
namespace fake {
struct Row {
  std::string tipo, id, datos, actualizado;
  bool borrado;
};
static std::map<std::string, Row> rows;  // «tipo:id»
static int clock = 0, logins = 0, posts = 0;
static bool approved = false;
static std::string stamp() {
  char b[40];
  snprintf(b, sizeof(b), "2026-10-19T10:%02d:%02d.%06d+00:00", clock / 60, clock % 60, clock);
  clock++;
  return b;
}
static nube::Resp reply(int code, const std::string& body) {
  nube::Resp r;
  r.code = code;
  r.body = (char*)malloc(body.size() + 1);
  memcpy(r.body, body.c_str(), body.size() + 1);
  r.len = body.size();
  return r;
}
static nube::Resp http(const char* method, const char* url, const char* bearer, const char* prefer, const char* body) {
  std::string u = url;
  if (u.find("/auth/v1/token?grant_type=password") != std::string::npos) {
    logins++;
    if (!strstr(body, "\"password\":\"clave-de-prueba\"")) return reply(400, "{\"error_code\":\"invalid_credentials\",\"msg\":\"Invalid login credentials\"}");
    return reply(200, "{\"access_token\":\"acc1\",\"refresh_token\":\"ref1\",\"expires_in\":3600,\"user\":{\"id\":\"u1\",\"email\":\"dueno@mesa.test\"}}");
  }
  if (u.find("/auth/v1/token?grant_type=refresh_token") != std::string::npos)
    return reply(200, "{\"access_token\":\"acc2\",\"refresh_token\":\"ref2\",\"expires_in\":3600,\"user\":{\"id\":\"u1\",\"email\":\"dueno@mesa.test\"}}");
  if (u.find("/rest/v1/rpc/vinculo_placa") != std::string::npos)  // el móvil ya ha confirmado el QR
    return reply(200, approved ? "{\"token_hash\":\"h1\",\"email\":\"dueno@mesa.test\"}" : "null");
  if (u.find("/auth/v1/verify") != std::string::npos)
    return reply(strstr(body, "\"h1\"") ? 200 : 403,
                 "{\"access_token\":\"acc1\",\"refresh_token\":\"ref1\",\"expires_in\":3600,\"user\":{\"id\":\"u1\",\"email\":\"dueno@mesa.test\"}}");
  if (u.find("/rest/v1/mesa_datos") == std::string::npos) return reply(404, "");
  if (strcmp(bearer, "acc1") && strcmp(bearer, "acc2")) return reply(401, "{}");
  if (!strcmp(method, "POST")) {
    posts++;
    const char* arr = body;
    for (const char* o = partidos::nextIn(arr, nullptr); o; o = partidos::nextIn(arr, o)) {
      Row r;
      char t[16], id[64];
      partidos::strOf(partidos::keyIn(o, "tipo"), t, sizeof(t));
      partidos::strOf(partidos::keyIn(o, "id"), id, sizeof(id));
      r.tipo = t;
      r.id = id;
      r.borrado = partidos::isTrue(partidos::keyIn(o, "borrado"));
      const char* d = partidos::keyIn(o, "datos");
      r.datos = std::string(d, partidos::closeOf(d) + 1);
      r.actualizado = stamp();
      rows[r.tipo + ":" + r.id] = r;
    }
    return reply(201, "");
  }
  // GET: lo cambiado desde una fecha, por orden, en páginas.
  std::string since;
  size_t g = u.find("actualizado=gt.");
  if (g != std::string::npos) {
    since = u.substr(g + 15);
    size_t a = since.find('&');
    if (a != std::string::npos) since = since.substr(0, a);
    for (size_t k; (k = since.find("%2B")) != std::string::npos;) since.replace(k, 3, "+");
  }
  int offset = atoi(u.substr(u.find("offset=") + 7).c_str()), limit = atoi(u.substr(u.find("limit=") + 6).c_str());
  std::vector<const Row*> list;
  for (auto& kv : rows)
    if ((kv.second.tipo == "player" || kv.second.tipo == "match") && kv.second.actualizado > since) list.push_back(&kv.second);
  std::sort(list.begin(), list.end(), [](const Row* a, const Row* b) { return a->actualizado < b->actualizado; });
  std::string out = "[";
  for (int i = offset; i < (int)list.size() && i < offset + limit; i++) {
    const Row& r = *list[i];
    if (out.size() > 1) out += ",";
    out += "{\"tipo\":\"" + r.tipo + "\",\"id\":\"" + r.id + "\",\"datos\":" + r.datos + ",\"borrado\":" + (r.borrado ? "true" : "false") +
           ",\"actualizado\":\"" + r.actualizado + "\"}";
  }
  return reply(200, out + "]");
}
static void seed(const std::string& tipo, const std::string& id, const std::string& datos) {
  rows[tipo + ":" + id] = Row{tipo, id, datos, stamp(), false};
}
}  // namespace fake

// Tecla del teclado de CUENTA (ui_cuenta.h): fila y columna.
static void ckey(char ch) {
  static const char* R[4] = {"1234567890", "qwertyuiop", "asdfghjkl", "..zxcvbnm_-"};
  if (ch == '@') return tap(13 + 10 * 70 + 33, 196 + 1 * 70 + 31);
  if (ch == '.') return tap(13 + 10 * 70 + 33, 196 + 2 * 70 + 31);
  for (int r = 0; r < 4; r++)
    for (int c = 0; R[r][c]; c++)
      if (R[r][c] == ch && !(r == 3 && c < 2)) return tap(13 + c * 70 + 33, 196 + r * 70 + 31);
}

// Una tecla del teclado de JUGADORES (ui_fichas.h).
static void key(const char* k) {
  static const char* rows[4][10] = {{"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
                                    {"A", "S", "D", "F", "G", "H", "J", "K", "L", "Ñ"},
                                    {"Z", "X", "C", "V", "B", "N", "M"},
                                    {"Á", "É", "Í", "Ó", "Ú", "Ü"}};
  if (!strcmp(k, " ")) return tap(13 + 6 * 78 + 147, 152 + 3 * 78 + 35);
  for (int r = 0; r < 4; r++)
    for (int i = 0; i < 10; i++)
      if (rows[r][i] && !strcmp(rows[r][i], k)) return tap(13 + i * 78 + 36, 152 + r * 78 + 35);
}

static void tapBtn(const ui::Rect& r) { tap(r.x + r.w / 2, r.y + r.h / 2); }
static void goal(bool white, uint32_t after = 3300) {
  tap(white ? 128 : 671, 186);
  run(after);
}

int main(int argc, char** argv) {
  if (argc > 1) OUT = argv[1];
  sim::now = 1000;
  assets::setRoot("../build/placa/fat");
  std::filesystem::remove_all("../build/placa/partidos-sd");  // historial vacío: solo los partidos de estas escenas
  setup();
  usb("HORA 2026-10-17 18:30:00");
  tap(400, 430);  // TERMINAR la prueba de la placa
  run(500);
  shot("inicio");
  tap(140, 280);
  shot("inicio-elegida");
  tap(140, 280);
  run(300);
  shot("configuracion");
  tap(595, 220);  // también por tiempo
  shot("configuracion-ambas");
  tap(204, 220);  // solo por tiempo
  for (int i = 0; i < 10; i++) tap(595, 104);
  shot("configuracion-tiempo-15");

  // Jugadores del modo prueba de la app, con sus fotos (scripts/jugadores-demo-placa.mjs).
  if (FILE* jf = fopen("../build/placa/jugadores-demo.txt", "rb")) {
    char ln[600];
    while (fgets(ln, sizeof(ln), jf)) {
      ln[strcspn(ln, "\r\n")] = 0;
      usb(ln);
    }
    fclose(jf);
    // Las fotos quedaron en la FAT simulada: se copian al disco del PC para que LVGL las lea como en la placa.
    std::filesystem::create_directories("../build/placa/fat/jugadores");
    for (auto& f : FFat.disk.files)
      if (f.first.rfind("/jugadores/", 0) == 0) {
        std::string path = std::string("../build/placa/fat") + f.first;
        if (FILE* o = fopen(path.c_str(), "wb")) {
          fwrite(f.second.data(), 1, f.second.size(), o);
          fclose(o);
        }
      }
    goHome();
    run(300);
    tap(140, 280); tap(140, 280);
    run(300);
    tap(204, 220);  // vuelve a solo goles (la escena anterior lo dejó por tiempo)
    tap(595, 220);
    tap(400, 443);  // CONTINUAR → elegir jugadores
    shot("jugadores-demo");
    tap(267, 154);  // Alex al Blanco
    tap(670, 438);  // SIGUIENTE: AZUL
    shot("jugadores-azul");
    tap(60, 28);    // volver al Blanco
    run(200);
    tap(60, 28);    // y a la configuración
    run(300);
    goHome();
    run(300);
  }

  // Partido por goles: las mismas jugadas que scripts/capturas-app.mjs.
  toMatch(false);
  shot("cuenta-atras", 1100);
  tap(400, 240);  // saltar
  shot("partido", 1080);
  goal(true, 600);
  shot("partido-gol", 0);
  // Los demás carteles en franja (ui_cartel.h), uno tras otro sobre el mismo partido.
  partido::celebrate(1, 3, false);
  shot("cartel-racha", 0);
  partido::celebrate(0, 1, true);
  shot("cartel-empate", 0);
  partido::celebrate(0, 2, false);
  shot("cartel-otro-mas", 0);
  partido::annulled(0);
  shot("cartel-anulado", 0);
  partido::banner(partido::B_PRORROGA);
  shot("cartel-prorroga", 0);
  partido::banner(partido::B_PENALTIS);
  shot("cartel-penaltis", 0);
  partido::banner(partido::B_MUERTE_SUBITA);
  shot("cartel-muerte-subita", 0);
  partido::banner(partido::B_PARTIDO_LOCO);
  shot("cartel-partido-loco", 0);
  cartel::hide();
  run(2700);
  for (bool w : {false, true, false, true}) goal(w);
  run(65000);
  shot("partido-marcador", 0);
  goal(true);
  shot("bola-de-partido");  // 4–2: a un gol
  tap(400, 314);            // PAUSA
  shot("pausa");
  tap(400, 260);            // CONTINUAR
  goal(true, 2500);
  shot("victoria");

  // Por tiempo: descanso, prórroga y penaltis.
  toMatch(true);
  run(1500);
  tap(400, 240);
  goal(true);
  goal(false);
  goal(false);
  run(5 * 60000);
  shot("descanso", 0);
  toMatch(true);
  run(1500);
  tap(400, 240);
  goal(true);
  run(5 * 60000);
  tap(400, 326);  // CONTINUAR 2ª PARTE
  run(4000);
  goal(false);
  run(5 * 60000);
  tap(400, 340);  // IR A PRÓRROGA
  run(4000);
  run(20000);
  shot("prorroga", 0);
  run(60000);
  tap(400, 340);  // IR A PENALTIS
  run(500);
  tap(88, 422);   // GOL del Blanco
  run(1000);
  tap(712, 422);  // FALLO del Azul
  shot("penaltis");

  // Partido Loco: lo que salga en 5 minutos de juego (cuenta atrás del hándicap, su tarjeta, el botón en el partido,
  // la vuelta a la normalidad, el penalti y las visitas), cada cosa la primera vez.
  {
    goHome();
    run(300);
    tap(400, 280); tap(400, 280);  // LOCO
    run(300);
    tap(400, 443);  // CONTINUAR
    run(200);
    pickPlayers();
    run(200);
    usb("SALTAR");
    run(200);
    bool intro = false, card = false, button = false, ending = false, penalty = false, visit = false;
    for (int i = 0; i < 1200 && eng.phase() != Phase::Finished; i++) {
      run(250);
      if (eng.phase() == Phase::Handicap && eng.handicapStage() == mfv3::HStage::Announce) {
        if (!intro) { intro = true; run(300); shot("loco-cuenta", 0); }
        run(3200);
        bool pen = eng.handicap().kind == mfv3::HKind::Penalty;
        if (pen && !penalty) { penalty = true; shot("loco-penalti", 0); }
        if (!pen && !card) { card = true; shot("loco-handicap", 0); }
        if (pen) tap(290, 392);  // GOL
        else tap(400, 120);      // seguir
        run(400);
        if (!pen && !button && partido::shown.hcButton) { button = true; shot("loco-boton", 0); }
      }
      if (eng.phase() == Phase::Handicap && eng.handicapStage() == mfv3::HStage::Ending && !ending) {
        ending = true; run(500); shot("loco-normalidad", 0);
      }
      if (eng.phase() == Phase::Visit && !visit) { visit = true; run(500); shot("loco-visita", 0); }
      if (i % 24 == 0 && eng.phase() == Phase::Playing && eng.lockRemaining(sim::now) == 0) usb(i % 48 ? "GB" : "GA");
    }
    std::printf("loco: cuenta %d tarjeta %d boton %d normalidad %d penalti %d visita %d\n", intro, card, button, ending, penalty, visit);
  }

  // AJUSTES → JUGADORES: crear uno con foto de la tarjeta (build/placa/fotos-sd), verlo y borrarlo.
  {
    goHome();
    run(300);
    openSettings();
    run(300);
    tapBtn(ui::SETTINGS[6].r);  // JUGADORES
    shot("fichas-lista");
    tap(106, 166);  // + NUEVO JUGADOR
    for (const char* k : {"A", "N", "A", " ", "M", "A", "R", "Í", "A"}) key(k);
    shot("fichas-nombre");
    tap(708, 32);   // SIGUIENTE
    run(1500);      // en el PC las fotos se abren al momento, de una en una
    shot("fichas-fotos");
    tap(16 + 3 * 157 + 70, 150);  // IMG_0002 (vertical, guardada tumbada)
    shot("fichas-foto");
    tap(708, 32);   // GUARDAR
    syncFat();
    shot("fichas-guardado");
    const jug::Player* nuevo = jugadores.count ? &jugadores.list[jugadores.count - 1] : nullptr;
    std::printf("fichas: %u jugadores, ultimo «%s» foto %d\n", jugadores.count, nuevo ? nuevo->name : "", nuevo ? (int)jugadores.photoOk(nuevo->code) : 0);
    if (nuevo) {
      fichas::editCode = nuevo->code;
      fichas::go(fichas::View::Detail);
      shot("fichas-jugador");
      tap(550, 379);  // BORRAR JUGADOR
      shot("fichas-borrar");
      tap(550, 390);  // SÍ, BORRAR
      shot("fichas-borrado");
      std::printf("fichas: quedan %u\n", jugadores.count);
    }
    tap(78, 32);    // VOLVER → AJUSTES
    shot("fichas-ajustes");
  }

  // RANKING → HISTORIAL con los partidos que se han jugado arriba (s3_partidos.h guarda cada uno) y siete más.
  for (int k = 0; k < 7; k++) {
    if (k % 2) {  // CLASIFICATORIO (la tercera tarjeta de Inicio), para la clasificación
      goHome();
      run(300);
      tap(660, 280); tap(660, 280);
      run(300);
      tap(400, 443);
      run(200);
      pickPlayers();
    } else {
      toMatch(false);
    }
    tap(400, 240);  // saltar la cuenta atrás
    run(500);
    for (int g = 0; g < 12 && eng.phase() != Phase::Finished; g++) {
      usb(((g + k) % 3) ? "GB" : "GA");
      run(3200);
      if (k == 0 && g == 1) usb("AB");  // un gol anulado (para el partido de ejemplo de tests/fixtures)
    }
    run(500);
    goHome();
    run(300);
  }
  {
    goHome();
    run(300);
    tap(470, 46);  // RANKING (empieza en CLASIFICACIÓN)
    run(300);
    shot("ranking-clasificacion");
    tap(277, 155);  // HISTÓRICO
    shot("ranking-historico");
    tap(16 + 124 + 6 + 48, 101);  // JUGADORES
    shot("ranking-jugadores");
    tap(419, 101);  // SALÓN DE LA FAMA
    shot("ranking-fama");
    tap(540, 101);  // RESUMEN
    shot("ranking-resumen");
    tap(628, 101);  // PAREJAS
    shot("ranking-parejas");
    tap(16 + 124 + 6 + 96 + 6 + 46, 101);  // HISTORIAL
    run(300);
    shot("historial");
    tap(400, 214);  // el partido más nuevo
    shot("historial-detalle");
    tap(35, 35);    // volver
    run(200);
    tap(16 + 78 + 6 + 82 + 6 + 33, 155);  // LOCO
    shot("historial-loco");
    tap(16 + 78 + 6 + 82 + 6 + 66 + 6 + 150 + 2 + 100, 155);  // JUGADOR
    shot("historial-quien");
    std::printf("historial: %d partidos guardados\n", partidos::count);
    tap(400, 470);  // fuera del recuadro: cierra
    tap(35, 35);    // volver a Inicio
    run(300);
  }

  // AJUSTES → CUENTA con una nube simulada: entrar con el teclado, bajar jugadores y partidos de la cuenta (Alex se
  // une con su ficha de la cuenta) y subir lo de la placa.
  {
    nube::http = fake::http;
    snprintf(wifiSsid, sizeof(wifiSsid), "CasaPrueba");  // en el PC no hay Wi-Fi de verdad (wifiUp sin poner = conectado)
    // En la cuenta: Alex (con foto JPEG de la app), Nuria (nueva) y un partido jugado en la app.
    std::string jpg;
    if (FILE* f = fopen("../build/placa/fotos-sd/foto pequeña.jpg", "rb")) {
      char b[4096];
      size_t n;
      while ((n = fread(b, 1, sizeof(b), f)) > 0) jpg.append(b, n);
      fclose(f);
    }
    std::string b64(jpg.size() / 3 * 4 + 8, 0);
    b64.resize(nube::base64Encode((const uint8_t*)jpg.data(), jpg.size(), &b64[0]));
    fake::seed("player", "p_alex", "{\"id\":\"p_alex\",\"name\":\"Alex\",\"active\":true,\"createdAt\":1,\"updatedAt\":2,\"photo\":\"data:image/jpeg;base64," + b64 + "\"}");
    fake::seed("player", "p_nuria", "{\"id\":\"p_nuria\",\"name\":\"Nuria\",\"active\":true,\"createdAt\":1,\"updatedAt\":2}");
    if (FILE* f = fopen("../tests/fixtures/progresion/partidos.jsonl", "rb")) {
      char line[200000];
      if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char id[64];
        partidos::strOf(partidos::keyIn(line, "id"), id, sizeof(id));
        fake::seed("match", id, line);
      }
      fclose(f);
    }
    uint32_t alexBefore = 0;
    for (int i = 0; i < jugadores.count; i++)
      if (!strcmp(jugadores.list[i].name, "Alex")) alexBefore = jugadores.list[i].code;
    int matchesBefore = partidos::count;
    goHome();
    run(300);
    openSettings();
    run(300);
    tapBtn(ui::SETTINGS[7].r);  // CUENTA
    run(300);
    for (char c : std::string("dueno@mesa.test")) ckey(c);
    tap(300, 159);  // contraseña
    for (char c : std::string("clave-de-prueba")) ckey(c);
    shot("cuenta-entrar");
    tap(708, 32);   // ENTRAR (en el PC, la nube responde al momento)
    for (int i = 0; i < 100; i++) run(100);  // y la segunda vuelta (los partidos de Alex, ya con su id de la cuenta)
    shot("cuenta-dentro");
    int stale = 0;
    for (auto& kv : fake::rows)
      if (kv.second.tipo == "match" && kv.second.datos.find("placa-a6d4eae1") != std::string::npos) stale++;
    std::printf("nube: partidos en la nube con el id viejo de Alex: %d\n", stale);
    uint32_t alexAfter = 0;
    for (int i = 0; i < jugadores.count; i++)
      if (!strcmp(jugadores.list[i].name, "Alex")) alexAfter = jugadores.list[i].code;
    char aid[48] = "";
    jugadores.idOf(alexAfter, aid, sizeof(aid));
    int upMatches = 0, upPlayers = 0;
    for (auto& kv : fake::rows) (kv.second.tipo == "match" ? upMatches : upPlayers) += kv.second.tipo == "match" || kv.second.tipo == "player";
    std::printf("nube: %d jugadores en la placa, entrar %d, sesión %d, Alex %08lx→%08lx (%s, foto %d), Nuria %d, partidos %d→%d, en la nube %d jugadores y %d partidos, pendientes %d\n",
                jugadores.count, fake::logins, (int)nube::loggedIn(), (unsigned long)alexBefore, (unsigned long)alexAfter, aid,
                (int)jugadores.photoOk(alexAfter), jugadores.findById("p_nuria") != nullptr, matchesBefore, partidos::count, upPlayers, upMatches,
                nube::pendingCount);
    // Entrar con QR: se cierra la sesión y se entra otra vez sin escribir (el «móvil» confirma).
    nube::logout();
    tap(78, 32);
    run(300);
    tapBtn(ui::SETTINGS[7].r);  // CUENTA (sin sesión: el teclado)
    run(300);
    // La pantalla del QR, tal cual se ve mientras espera al móvil.
    cuenta::qrMode = true;
    nube::phase = nube::BUSY;
    snprintf(nube::linkCode, sizeof(nube::linkCode), "Ab3dEf6hIj9kLm2nOp5qRs8tUv1wXy4z");
    cuenta::go();
    shot("cuenta-qr", 0);  // sin dar vueltas a loop() (si no, la nube vería que no hay nada en marcha)
    nube::phase = nube::OFF;
    cuenta::qrMode = false;
    cuenta::go();
    run(200);
    fake::approved = true;
    tap(724, 99);   // CON QR
    for (int i = 0; i < 40; i++) run(100);
    std::printf("nube: con QR, sesión %d (%s)\n", (int)nube::loggedIn(), nube::saved.email);
    tap(78, 32);    // VOLVER → AJUSTES
    shot("cuenta-ajustes");
    tapBtn(ui::BACK);
    wifiSsid[0] = 0;
    nube::logout();  // que no quede la sesión guardada para las demás pruebas del PC
    nube::http = nullptr;
    run(300);
  }

  // Reposo: Inicio sin tocar nada durante más de 5 minutos.
  goHome();
  run(SLEEP_AFTER_MS + 2000);
  shot("reposo", 0);
  return 0;
}
