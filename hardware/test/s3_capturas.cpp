// Capturas de la pantalla de 7" en el PC: el mismo sketch con LVGL dibujando en memoria, con las capas y letras de
// build/placa/fat (scripts/placa_recursos.py). Guarda <carpeta>/<escena>.rgb565 (800×480, RGB565) por escena; el
// script scripts/capturas_placa_pc.py las pasa a PNG. Uso: sh test/s3_build.sh 4 test/s3_capturas.cpp
// Las escenas siguen las mismas jugadas que scripts/capturas-app.mjs, para compararlas una a una.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
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
    tap(400, 444);  // JUGADORES
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
    toMatch(false);
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
    tap(470, 46);  // RANKING
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

  // Reposo: Inicio sin tocar nada durante más de 5 minutos.
  goHome();
  run(SLEEP_AFTER_MS + 2000);
  shot("reposo", 0);
  return 0;
}
