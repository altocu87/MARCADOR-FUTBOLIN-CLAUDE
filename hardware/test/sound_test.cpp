// Pruebas en PC de s3_sound.h: qué momento suena tras cada cambio del motor (mismas reglas que
// soundsForCommit() de la app), lectura de WAV y tonos de siempre.
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include "../esp32s3_pantalla7/s3_sound.h"

using mfv3::Engine;
using mfv3::Phase;
using mfv3::Team;
using s3snd::Ev;

static Engine eng;
static s3snd::Tracker tr;
static uint32_t now = 1000;
static Ev out[s3snd::Tracker::MAX_OUT];
static uint8_t n = 0;

static void start(mfv3::Config c) {
  assert(eng.start(c, now));
  tr.reset(eng);
}
static void skip() {
  eng.skipCountdown(now);
  n = tr.observe(eng, false, Team::White, out);
}
static void goal(Team t) {
  now += 3100;  // fuera del bloqueo
  bool ok = eng.goal(t, now) == mfv3::Result::Accepted;
  assert(ok);
  n = tr.observe(eng, ok, t, out);
}
static void minus(Team t) {
  eng.minusOne(t, now);
  n = tr.observe(eng, false, t, out);
}
static bool is(std::initializer_list<Ev> want) {
  if (want.size() != n) return false;
  uint8_t i = 0;
  for (Ev e : want)
    if (out[i++] != e) return false;
  return true;
}

static std::vector<uint8_t> wav(uint32_t rate, uint16_t ch, uint16_t bits, uint32_t frames, bool listChunk) {
  std::vector<uint8_t> v;
  auto u32 = [&](uint32_t x) { for (int i = 0; i < 4; i++) v.push_back((uint8_t)(x >> (8 * i))); };
  auto u16 = [&](uint16_t x) { v.push_back((uint8_t)x); v.push_back((uint8_t)(x >> 8)); };
  auto tag = [&](const char* t) { v.insert(v.end(), t, t + 4); };
  uint32_t data = frames * ch * bits / 8;
  tag("RIFF"); u32(36 + data + (listChunk ? 14 : 0)); tag("WAVE");
  if (listChunk) { tag("LIST"); u32(5); tag("INFO"); v.push_back('x'); v.push_back(0); }  // tamaño impar + relleno
  tag("fmt "); u32(16); u16(1); u16(ch); u32(rate); u32(rate * ch * bits / 8); u16(ch * bits / 8); u16(bits);
  tag("data"); u32(data);
  v.resize(v.size() + data, 0);
  return v;
}

int main() {
  // Por goles a 5: empieza, gol, empate, 3 seguidos, bola de partido, victoria.
  mfv3::Config c;
  start(c);
  skip();
  assert(is({Ev::MatchStart}));
  goal(Team::White);
  assert(is({Ev::Goal}));
  goal(Team::Blue);
  assert(is({Ev::Equalizer}));
  goal(Team::Blue);
  goal(Team::Blue);
  assert(is({Ev::Streak3}));  // 1-3: el tercero seguido del Azul
  // Remontada: el Blanco iba perdiendo de 2 y se pone por delante.
  goal(Team::White);
  goal(Team::White);
  assert(is({Ev::Equalizer}));
  goal(Team::White);  // 4-3: remontada (gana a «3 seguidos») y bola de partido del Blanco
  assert(is({Ev::Comeback, Ev::MatchPoint}));
  // −1: gol anulado; la bola de partido vuelve a sonar al recuperarla.
  minus(Team::White);
  assert(is({Ev::GoalAnnulled}));
  goal(Team::Blue);  // 3-4: bola de partido del Azul (ya la tenía el Blanco antes, no)
  assert(is({Ev::Goal, Ev::MatchPoint}));
  goal(Team::Blue);  // 3-5: gana el Azul, sin goleada
  assert(eng.phase() == Phase::Finished && is({Ev::Victory}));

  // Goleada: 5-0.
  start(c);
  skip();
  for (int i = 0; i < 4; i++) goal(Team::White);
  goal(Team::White);
  assert(is({Ev::Rout}) && s3snd::Tracker::isRout(eng));

  // Por tiempo con empate: fin de parte, prórroga y gol de oro.
  c.endCondition = mfv3::EndCondition::Time;
  c.minutesPerPeriod = 1;
  start(c);
  skip();
  goal(Team::White);
  goal(Team::Blue);
  now += 61000;
  eng.tick(now);
  n = tr.observe(eng, false, Team::White, out);
  assert(eng.phase() == Phase::PeriodEnd && is({Ev::PeriodEnd}));
  eng.next(now);
  n = tr.observe(eng, false, Team::White, out);
  assert(n == 0);
  skip();  // 2ª parte: no es el «Empieza el partido»
  assert(n == 0);
  now += 61000;
  eng.tick(now);
  n = tr.observe(eng, false, Team::White, out);
  assert(is({Ev::PeriodEnd}));
  eng.next(now);
  tr.observe(eng, false, Team::White, out);
  skip();
  goal(Team::Blue);
  assert(eng.phase() == Phase::Finished && is({Ev::GoldenGoal, Ev::Victory}));

  // Penaltis: marcado y fallado.
  start(c);
  skip();
  for (int p = 0; p < 2; p++) {
    now += 61000;
    eng.tick(now);
    tr.observe(eng, false, Team::White, out);
    eng.next(now);
    tr.observe(eng, false, Team::White, out);
    skip();
  }
  now += 61000;
  eng.tick(now);
  tr.observe(eng, false, Team::White, out);
  eng.next(now);  // a penaltis
  tr.observe(eng, false, Team::White, out);
  assert(eng.phase() == Phase::Penalties);
  eng.penalty(Team::White, true, now);
  n = tr.observe(eng, false, Team::White, out);
  assert(is({Ev::PenaltyScored}));
  eng.penalty(Team::Blue, false, now);
  n = tr.observe(eng, false, Team::Blue, out);
  assert(is({Ev::PenaltyMissed}));

  // Nombres: carpetas de la SD y mismos ids que la app.
  assert(!strcmp(s3snd::info(Ev::Goal).folder, "gol") && !strcmp(s3snd::info(Ev::Rout).webId, "rout"));

  // WAV: cabecera mínima, con bloque LIST de tamaño impar, 8 bits estéreo; rechaza lo que no es PCM válido.
  s3snd::Wav w;
  auto a = wav(22050, 1, 16, 100, false);
  assert(s3snd::parseWav(a.data(), (uint32_t)a.size(), (uint32_t)a.size(), w) && w.rate == 22050 && w.channels == 1 &&
         w.bits == 16 && w.dataOffset == 44 && w.dataSize == 200);
  auto b = wav(44100, 2, 8, 50, true);
  assert(s3snd::parseWav(b.data(), (uint32_t)b.size(), (uint32_t)b.size(), w) && w.channels == 2 && w.bits == 8 &&
         w.dataOffset == 44 + 14 && w.dataSize == 100);
  // Archivo cortado: el tamaño de los datos no pasa del final del archivo.
  assert(s3snd::parseWav(a.data(), 44, 100, w) && w.dataSize == 56);
  auto bad = wav(96000, 1, 16, 10, false);
  assert(!s3snd::parseWav(bad.data(), (uint32_t)bad.size(), 0, w));
  bad = wav(22050, 1, 24, 10, false);
  assert(!s3snd::parseWav(bad.data(), (uint32_t)bad.size(), 0, w));
  const char junk[] = "RIFF....AVI LIST";
  assert(!s3snd::parseWav((const uint8_t*)junk, sizeof(junk), 0, w));

  // Tonos de siempre: duran lo de la app, suenan (no es silencio) y no saturan.
  s3snd::Pattern g = s3snd::defaultPattern(Ev::Goal);
  assert(s3snd::patternLengthMs(g) == 650 && s3snd::patternLengthMs(s3snd::defaultPattern(Ev::Victory)) == 1140);
  std::vector<int16_t> pcm(22050 * 650 / 1000);
  s3snd::renderPattern(g, 22050, 0, pcm.data(), (uint32_t)pcm.size());
  int peak = 0;
  for (int16_t s : pcm) peak = std::max(peak, std::abs((int)s));
  assert(peak > 8000 && peak <= 32000 && pcm[0] == 0);
  for (Ev e : {Ev::GoalAnnulled, Ev::MatchPoint, Ev::MatchStart, Ev::PeriodEnd, Ev::Victory})
    assert(s3snd::defaultPattern(e).count > 0);

  std::printf("s3_sound: momentos del partido, WAV y tonos OK\n");
}
