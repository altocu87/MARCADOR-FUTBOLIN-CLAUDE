// Pitido del árbitro (s3_pitido.h): cuándo suena y que va primero, con la voz del descanso o del gol detrás.
#include <cassert>
#include <cstdio>
#include <vector>
#include <string>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

using namespace comentarios;
static const int16_t PITA = 1212, DESC = 3434, GOL = 5656;

static std::vector<uint8_t> wave(int16_t sample, unsigned frames) {
  std::vector<uint8_t> d(44 + frames * 2);
  auto u32 = [&](unsigned p, uint32_t n) { for (unsigned i = 0; i < 4; i++) d[p + i] = (uint8_t)(n >> (8 * i)); };
  memcpy(d.data(), "RIFF", 4);
  u32(4, (uint32_t)d.size() - 8);
  memcpy(d.data() + 8, "WAVEfmt ", 8);
  u32(16, 16);
  u32(20, 1 | (1u << 16));
  u32(24, 16000);
  u32(28, 32000);
  u32(32, 2 | (16u << 16));
  memcpy(d.data() + 36, "data", 4);
  u32(40, frames * 2);
  for (size_t p = 44; p < d.size(); p += 2) {
    d[p] = (uint8_t)sample;
    d[p + 1] = (uint8_t)(sample >> 8);
  }
  return d;
}
static void put(uint32_t code, int16_t sample, unsigned frames = 16000) {
  char path[40];
  snprintf(path, sizeof path, "/sonidos/%08lx.wav", (unsigned long)code);
  SD_MMC.disk.files[path] = wave(sample, frames);
}
static void drop(uint32_t code) {
  char path[40];
  snprintf(path, sizeof path, "/sonidos/%08lx.wav", (unsigned long)code);
  SD_MMC.disk.files.erase(path);
}
// La web simulada: sirve `web` (o nada).
static std::vector<uint8_t> web;
static int fetches = 0;
static bool fakeFetch(const char* url, uint8_t* out, size_t cap, size_t& n) {
  fetches++;
  assert(std::string(url).find("/sonidos/pitido.wav") != std::string::npos);
  n = web.size() < cap ? web.size() : cap;
  memcpy(out, web.data(), n);
  return !web.empty();
}
static int first(int16_t s) {
  for (size_t i = 0; i < sim::heard.size(); i++)
    if (sim::heard[i] == s) return (int)i;
  return -1;
}
static void step(unsigned ms) {
  sim::now += ms;
  sim::advanceI2sFrames(ms * 16);
  loop();
}
static void run(unsigned ms) {
  for (unsigned t = 0; t < ms; t += 50) step(50);
}
static void reset(mfv3::EndCondition end) {
  resultPending = false;
  goHome();
  player.stop();
  catalog.clear();
  sndChoices = mfv3snd::Choices();
  cfg = mfv3::Config();
  cfg.endCondition = end;
  cfg.minutesPerPeriod = 1;
  cfg.goalsPerPeriod = 1;
  haveTeams = false;
  chosenMode = inicio::QUICK;
}
static void begin() {
  startMatch();
  run(3100);
  assert(eng.phase() == Phase::Playing);
  player.stop();
  sim::heard.clear();
}

static void units() {
  using mfv3::Period;
  assert(pitido::code() == 0x194809f5u);  // FNV-1a de «arbitro/pitido_final» (scripts/placa.py pitido)
  assert(pitido::due(Phase::Playing, Phase::PeriodEnd, Period::First));
  assert(pitido::due(Phase::Playing, Phase::PeriodEnd, Period::Second));
  assert(pitido::due(Phase::Playing, Phase::PeriodEnd, Period::Overtime));
  assert(pitido::due(Phase::Playing, Phase::Finished, Period::First));     // partido a goles
  assert(pitido::due(Phase::Playing, Phase::Finished, Period::Second));    // 2ª parte con ganador
  assert(pitido::due(Phase::Playing, Phase::Finished, Period::Overtime));  // gol de oro
  assert(!pitido::due(Phase::Penalties, Phase::Finished, Period::Shootout));
  assert(!pitido::due(Phase::PeriodEnd, Phase::PeriodEnd, Period::First));  // solo al entrar
  assert(!pitido::due(Phase::Finished, Phase::Finished, Period::First));
  assert(!pitido::due(Phase::Playing, Phase::Paused, Period::First));
  assert(!pitido::due(Phase::PeriodEnd, Phase::Countdown, Period::Second));
}

int main() {
  units();
  sim::now = 1000;
  sim::sdCard = true;
  setup();
  test.active = false;
  nube::phase = nube::OFF;
  goHome();
  sim::i2sRealtime = true;
  put(pitido::code(), PITA, 24000);  // 1,5 s, como el del dueño recortado
  put(0x3101, DESC);
  put(0x3102, GOL);

  // Fin de la 1ª parte por tiempo: pitido y, detrás, la voz del descanso.
  reset(mfv3::EndCondition::Time);
  assert(catalog.add(Descanso, 0x3101));
  begin();
  run(60000);
  assert(eng.phase() == Phase::PeriodEnd && eng.period() == Period::First);
  run(4000);
  assert(first(PITA) >= 0 && first(DESC) > first(PITA));

  // Partido a goles: el gol que gana pita y la voz del gol va detrás. La victoria se ve al momento.
  reset(mfv3::EndCondition::Goals);
  assert(catalog.add(Comodin, 0x3102));
  begin();
  inputGoal(Team::White);
  assert(eng.phase() == Phase::Finished);
  run(4000);
  assert(first(PITA) >= 0 && first(GOL) > first(PITA));

  // Un gol que no acaba nada no pita.
  reset(mfv3::EndCondition::Time);
  assert(catalog.add(Comodin, 0x3102));
  begin();
  inputGoal(Team::Blue);
  run(3000);
  assert(first(PITA) < 0 && first(GOL) >= 0);

  // Sin el pitido en la SD, todo como antes: la voz del descanso suena sola.
  drop(pitido::code());
  reset(mfv3::EndCondition::Time);
  assert(catalog.add(Descanso, 0x3101));
  begin();
  run(60000);
  run(2000);
  assert(first(PITA) < 0 && first(DESC) >= 0);

  // Pitido dañado (está en la SD pero no se puede tocar): suena el tono de siempre del fin de parte, no silencio,
  // y la próxima búsqueda lo vuelve a bajar.
  {
    char path[40];
    snprintf(path, sizeof path, "/sonidos/%08lx.wav", (unsigned long)pitido::code());
    SD_MMC.disk.files[path] = std::vector<uint8_t>(60, 0x33);
    reset(mfv3::EndCondition::Time);
    begin();
    run(60000);
    run(2000);
    bool tone = false;
    for (int16_t v : sim::heard) tone |= v != 0;
    assert(first(PITA) < 0 && tone);
    assert(pitidoNeeded());
    drop(pitido::code());
  }

  // 0.27: sin el pitido en la SD, la placa lo baja de la web al buscar actualizaciones y lo guarda comprobado.
  goHome();
  FILE* f = fopen("../web/public/sonidos/pitido.wav", "rb");
  assert(f);
  web.resize(pitido::FILE_SIZE + 10);
  web.resize(fread(web.data(), 1, web.size(), f));
  fclose(f);
  assert(web.size() == pitido::FILE_SIZE && pitido::crc32(web.data(), web.size()) == pitido::FILE_CRC);
  pitido::fetch = fakeFetch;
  // Llega dañado: no se guarda nada.
  std::vector<uint8_t> good = web;
  web[100] ^= 0x55;
  pitido::want = pitidoNeeded();
  assert(pitido::want);
  pitido::downloadWork();
  assert(!pitido::got && !pitidoInstall() && !hcHas(pitido::code()) && fetches == 1);
  // Llega bien: a la SD.
  web = good;
  pitido::want = pitidoNeeded();
  pitido::downloadWork();
  assert(pitido::got && pitidoInstall() && hcHas(pitido::code()) && !pitido::got);
  // Ya está: no se vuelve a pedir.
  assert(!pitidoNeeded());
  // Como mucho tres intentos por encendido si la web no lo da.
  assert(sndStore.remove(pitido::code()) && !hcHas(pitido::code()));
  web.clear();
  for (int i = 0; i < 5; i++) {
    pitido::want = pitidoNeeded();
    pitido::downloadWork();
  }
  assert(pitido::tries == 3 && !pitidoNeeded());
  // Empezar un partido corta la búsqueda: la tarea ya no baja el pitido (48 KB y otra conexión).
  pitido::tries = 0;
  web = good;
  int before = fetches;
  pitido::want = pitidoNeeded();
  assert(pitido::want);
  versionsCancel = true;
  fetchVersionsWork();
  assert(fetches == before && !pitido::got && versionsDone.load());
  versionsCancel = false;

  puts("Pitido: fin de parte, prórroga y partido; primero el pitido y después la voz; sin archivo, como antes; se baja solo de la web; dañado, suena el tono de siempre y se vuelve a bajar; al empezar a jugar no se baja OK");
}
