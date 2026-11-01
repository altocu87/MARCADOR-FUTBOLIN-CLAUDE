// Voces de los hándicaps (s3_voces_hc.h): códigos iguales a voces-handicaps/subir-sd.py, qué se dice en cada
// hándicap y el orden en el partido real: aviso → cuenta atrás 3·2·1 → quién + frase → final.
#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

using voceshc::Seq;
using voceshc::Who;

static std::set<uint32_t> present;
static bool has(uint32_t c) { return present.count(c) > 0; }
static uint32_t C(const char* v) { return voceshc::code(v); }
static void all() {
  present.clear();
  for (const char* v : {"A1", "A2", "A3", "Q1", "Q2", "Q3", "Q4", "F01", "F02", "F03", "F04", "F05", "F06", "F07a", "F07b",
                        "F07c", "F07d", "F08a", "F08b", "F08c", "F08d", "F08e", "F08f", "F08g", "F08h", "F09a", "F09b",
                        "F10a", "F10b", "F11", "F12", "F13", "R1a", "R1b", "R1c", "R1d", "R4a", "R4b", "R4c", "R4d",
                        "R5a", "R5b", "R5c", "R5d", "C1", "C2", "C3", "N_vicky", "N_alex", "N_lucia", "N_alvaro"})
    present.insert(C(v));
}
static Who who(const char* name, char gender) {
  Who w;
  snprintf(w.name, sizeof(w.name), "%s", name);
  w.gender = gender;
  return w;
}
static void same(const Seq& s, std::vector<const char*> want) {
  assert(s.n == want.size());
  for (size_t i = 0; i < want.size(); i++) assert(s.c[i] == C(want[i]));
}
static mfv3::Handicap hc(mfv3::HKind k, uint8_t team, int8_t w, int8_t b, uint32_t dur = 15000) {
  mfv3::Handicap h;
  h.kind = k;
  h.team = team;
  h.playerW = w;
  h.playerB = b;
  h.durationMs = dur;
  return h;
}

static void units() {
  // Los mismos códigos que se subieron a la SD (voces-handicaps/codigos.json).
  assert(C("A1") == 0x5280fb10 && C("F01") == 0x86a50327 && C("N_alex") == 0x53d8fa09 && C("Q4") == 0xd75929ff);
  assert(C("F08h") == 0x5ead04fc && C("R5d") == 0x45ab4449 && C("N_alvaro") == 0x3c7067f6 && C("C1") == 0xe67bd3de);
  char v[40];
  voceshc::nameVoice("Álvaro", v, sizeof v);
  assert(!strcmp(v, "N_alvaro"));
  voceshc::nameVoice("Lucía", v, sizeof v);
  assert(!strcmp(v, "N_lucia"));
  voceshc::nameVoice(" María José ", v, sizeof v);
  assert(!strcmp(v, "N_maria_jose"));
  voceshc::nameVoice("NÚÑEZ", v, sizeof v);
  assert(!strcmp(v, "N_nunez"));

  all();
  Who vicky = who("Vicky", 'f'), alex = who("Alex", 'm'), lucia = who("Lucía", 'f'), nadie = who("", 0);
  using K = mfv3::HKind;
  same(voceshc::reveal(hc(K::DoubleAll, 2, -1, -1), nadie, nadie, has), {"F01"});
  same(voceshc::reveal(hc(K::DoubleTeam, 1, -1, -1), nadie, nadie, has), {"Q2", "F02"});
  same(voceshc::reveal(hc(K::Steal, 0, -1, -1, 0), nadie, nadie, has), {"Q1", "F04"});
  same(voceshc::reveal(hc(K::SwapPositions, 2, -1, -1, 0), nadie, nadie, has), {"Q3", "F06"});
  same(voceshc::reveal(hc(K::FrozenPlayer, 0, 0, -1), vicky, nadie, has), {"N_vicky", "F07b"});
  same(voceshc::reveal(hc(K::FrozenPlayer, 1, -1, 0), nadie, alex, has), {"N_alex", "F07a"});
  same(voceshc::reveal(hc(K::FrozenPlayer, 2, 0, 0), vicky, alex, has), {"N_vicky", "Q4", "N_alex", "F07c"});
  same(voceshc::reveal(hc(K::FrozenPlayer, 2, 0, 0), vicky, lucia, has), {"N_vicky", "Q4", "N_lucia", "F07d"});
  mfv3::Handicap bar = hc(K::BarLock, 2, 0, 0);
  bar.bar = mfv3::Bar::Defensa;
  same(voceshc::reveal(bar, vicky, alex, has), {"N_vicky", "Q4", "N_alex", "F08f"});
  bar = hc(K::BarLock, 1, -1, 0);
  bar.bar = mfv3::Bar::Delantero;
  same(voceshc::reveal(bar, nadie, alex, has), {"N_alex", "F08d"});
  same(voceshc::reveal(hc(K::Penalty, 1, -1, -1, 0), nadie, nadie, has), {"F09b"});
  same(voceshc::reveal(hc(K::Transfer, 2, 0, 0, 30000), vicky, alex, has), {"N_vicky", "F10a", "N_alex", "F10b"});
  same(voceshc::reveal(hc(K::OneHand, 2, -1, -1, 20000), nadie, nadie, has), {"F13"});
  // Sin su nombre grabado (o sin jugadores elegidos) dice su equipo; sin la frase, nada.
  same(voceshc::reveal(hc(K::FrozenPlayer, 1, -1, 0), nadie, who("Sergio", 'm'), has), {"Q2", "F07a"});
  present.erase(C("F07a"));
  assert(voceshc::reveal(hc(K::FrozenPlayer, 1, -1, 0), nadie, alex, has).n == 0);
  assert(voceshc::reveal(hc(K::WeakHand, 2, -1, -1), nadie, nadie, has).n == 0);
  // Finales: con tiempo, «todo vuelve a la normalidad»; cambio y traspaso, el suyo; los que se gastan, ninguno.
  all();
  for (uint32_t r = 0; r < 8; r++) {
    uint32_t e = voceshc::ending(hc(K::DoubleAll, 2, -1, -1, 30000), has, r);
    assert(e == C("R1a") || e == C("R1b") || e == C("R1c") || e == C("R1d"));
    e = voceshc::ending(hc(K::SwapPositions, 2, -1, -1, 0), has, r);
    assert(e == C("R4a") || e == C("R4b") || e == C("R4c") || e == C("R4d"));
    e = voceshc::ending(hc(K::Transfer, 2, 0, 0, 30000), has, r);
    assert(e == C("R5a") || e == C("R5b") || e == C("R5c") || e == C("R5d"));
  }
  assert(voceshc::ending(hc(K::TripleNext, 2, -1, -1, 0), has, 0) == 0);
  assert(voceshc::count(3, has) == C("C3") && voceshc::count(1, has) == C("C1") && voceshc::count(0, has) == 0);
  present.clear();
  assert(voceshc::aviso(has, 0) == 0 && voceshc::count(2, has) == 0);
}

// ---- En el partido, con el .ino de verdad
static const int16_t AVISO = 1111, FRASE = 2222, FINAL = 3333, CUENTA = 4444;
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
static void put(const char* voice, int16_t sample) {
  char path[40];
  snprintf(path, sizeof path, "/sonidos/%08lx.wav", (unsigned long)C(voice));
  SD_MMC.disk.files[path] = wave(sample, 16000);  // 1 s
}
static bool heard(int16_t s) {
  for (auto v : sim::heard)
    if (v == s) return true;
  return false;
}
// El altavoz simulado suena a su ritmo (16 000 muestras por segundo), como el de la placa.
static void step(unsigned ms) {
  sim::now += ms;
  sim::advanceI2sFrames(ms * 16);
  loop();
}
static void run(unsigned ms) {
  for (unsigned t = 0; t < ms; t += 50) step(50);
}

// Un partido loco hasta su primer hándicap con frase y duración (no penalti ni los que se gastan).
static void untilHandicap() {
  for (uint32_t seed = 1; seed <= 400; seed++) {
    cfg = mfv3::Config();
    cfg.chaos = true;
    cfg.endCondition = mfv3::EndCondition::Time;
    cfg.minutesPerPeriod = 30;
    cfg.goalsPerPeriod = 20;
    haveTeams = false;
    chosenMode = inicio::CHAOS;
    startMatch();
    eng.seed(seed);
    player.stop();
    sim::heard.clear();
    for (unsigned i = 0; i < 4000 && eng.phase() != Phase::Handicap; i++) step(250);
    if (eng.phase() == Phase::Handicap && eng.handicapStage() == mfv3::HStage::Announce && eng.handicap().durationMs &&
        eng.handicap().kind != mfv3::HKind::FrozenPlayer && eng.handicap().kind != mfv3::HKind::BarLock &&
        eng.handicap().kind != mfv3::HKind::Transfer)
      return;
    goHome();
  }
  assert(false && "Sin hándicap con duración en las semillas probadas");
}
static void match() {
  sim::now = 1000;
  sim::sdCard = true;
  setup();
  test.active = false;
  nube::phase = nube::OFF;
  goHome();
  sim::i2sRealtime = true;
  for (const char* v : {"A1", "A2", "A3"}) put(v, AVISO);
  for (const char* v : {"Q1", "Q2", "Q3", "F01", "F02", "F05", "F06", "F11", "F12", "F13"}) put(v, FRASE);
  for (const char* v : {"R1a", "R1b", "R1c", "R1d"}) put(v, FINAL);
  for (const char* v : {"C3", "C2", "C1"}) {
    char path[40];
    snprintf(path, sizeof path, "/sonidos/%08lx.wav", (unsigned long)C(v));
    SD_MMC.disk.files[path] = wave(CUENTA, 8000);  // medio segundo, como «¡Tres!»
  }
  untilHandicap();
  // 1. El aviso: «¡HÁNDICAP!» sin cifra mientras suena.
  assert(hcAvisoWait);
  run(100);
  assert(hcAvisoWait && heard(AVISO) && !heard(FRASE));
  partido::View v;
  matchView(v, sim::now);
  assert(v.hcIntroWait && v.hcIntroN == 0);
  // 2. Al acabar el aviso, la cuenta atrás 3·2·1 (nada más suena).
  run(1500);
  assert(!hcAvisoWait);
  matchView(v, sim::now);
  assert(!v.hcIntroWait && v.hcIntroN >= 1 && v.hcIntroN <= 3);
  sim::heard.clear();
  run(2000);
  assert(!heard(FRASE) && heard(CUENTA) && hcCountSaid == 1);
  // 3. Al verse el hándicap, su frase (y su equipo, si va con uno), seguida.
  run(1200);
  matchView(v, sim::now);
  assert(v.hcIntroN == 0 && !v.hcIntroWait && heard(FRASE) && !heard(AVISO));
  assert(hcRevealSeq == eng.handicap().seq);
  // 4. Se sigue jugando y, al acabarse el tiempo, «todo vuelve a la normalidad».
  sim::now += 100;
  onHcTap();
  assert(eng.phase() == Phase::Playing && eng.activeHandicap());
  sim::heard.clear();
  run(eng.handicap().durationMs + 500);
  assert(eng.phase() == Phase::Handicap && eng.handicapStage() == mfv3::HStage::Ending);
  run(300);
  assert(heard(FINAL));
  // Tocar durante el aviso lo salta: ni aviso ni cuenta atrás, directo a la frase.
  goHome();
  untilHandicap();
  assert(hcAvisoWait);
  sim::now += 100;
  onHcTap();
  assert(!hcAvisoWait && !hcIntroOn(sim::now));
  sim::heard.clear();
  run(200);
  assert(heard(FRASE));
  // Sin voces en la SD: como antes (cuenta atrás en cuanto sale).
  goHome();
  SD_MMC.disk.files.clear();
  untilHandicap();
  assert(!hcAvisoWait);
  matchView(v, sim::now);
  assert(v.hcIntroN == 3 && !v.hcIntroWait);
  goHome();
  player.stop();
}

int main() {
  units();
  match();
  puts("Voces de los hándicaps: códigos, quién + frase, chico/chica, dos jugadores, finales y aviso → 3·2·1 → frase OK");
}
