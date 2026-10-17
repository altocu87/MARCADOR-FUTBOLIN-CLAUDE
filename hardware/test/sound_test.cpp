// Pruebas en PC de los sonidos propios de la placa (mfv3_sound.h): momentos del partido, qué suena en cada uno,
// recepción de archivos por el protocolo, WAV, cambio de frecuencia y notas «de siempre».
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "mfv3_core.h"
#include "mfv3_sound.h"

using namespace mfv3snd;
using mfv3::Config;
using mfv3::EndCondition;

// ---------------------------------------------------------------- partido simulado
struct Sim {
  Engine e;
  uint32_t t = 1000;
  std::vector<std::vector<Event>> heard;
  explicit Sim(Config c) { e.start(c, t); }
  std::vector<Event> apply(bool goal, Before b) {
    EventList l = detect(e, b, goal);
    std::vector<Event> v(l.ev, l.ev + l.n);
    heard.push_back(v);
    return v;
  }
  std::vector<Event> start() {
    Before b = before(e);
    e.skipCountdown(t);
    return apply(false, b);
  }
  std::vector<Event> goal(Team team) {
    t += 3500;
    Before b = before(e);
    bool ok = e.goal(team, t) == mfv3::Result::Accepted;
    assert(ok);
    return apply(true, b);
  }
  std::vector<Event> goals(const char* seq) {
    std::vector<Event> last;
    for (const char* p = seq; *p; p++) last = goal(*p == 'W' ? Team::White : Team::Blue);
    return last;
  }
  std::vector<Event> wait(uint32_t ms) {
    t += ms;
    Before b = before(e);
    e.tick(t);
    return apply(false, b);
  }
};

static Config goalsCfg(uint8_t n) {
  Config c;
  c.endCondition = EndCondition::Goals;
  c.goalsPerPeriod = n;
  return c;
}

static bool eq(const std::vector<Event>& v, std::initializer_list<Event> want) { return v == std::vector<Event>(want); }

static void testDetect() {
  Sim s(goalsCfg(10));
  assert(eq(s.start(), {Event::MatchStart}));
  assert(eq(s.goal(Team::White), {Event::Goal}));
  assert(eq(s.goal(Team::Blue), {Event::Equalizer}));
  s.goal(Team::White);
  s.goal(Team::White);
  assert(eq(s.goal(Team::White), {Event::Streak3}));
  assert(eq(s.goal(Team::White), {Event::Goal}));  // el 4º seguido ya no

  Sim c(goalsCfg(10));
  c.start();
  c.goals("BB");
  c.goal(Team::White);
  assert(eq(c.goal(Team::White), {Event::Equalizer}));  // igualar no es remontada
  assert(eq(c.goal(Team::White), {Event::Comeback}));   // ponerse por delante tras ir 2 abajo (y gana a «3 seguidos»)

  // Bola de partido en cola tras el gol; victoria o goleada en lugar del gol que termina.
  Sim m(goalsCfg(3));
  m.start();
  m.goal(Team::White);
  assert(eq(m.goal(Team::White), {Event::Goal, Event::MatchPoint}));
  assert(eq(m.goal(Team::White), {Event::Rout}));  // 3-0
  Sim v(goalsCfg(5));
  v.start();
  assert(eq(v.goals("BBWWWWW"), {Event::Victory}));  // 5-2
  Sim r(goalsCfg(5));
  r.start();
  assert(eq(r.goals("BWWWWW"), {Event::Rout}));  // 5-1

  // Gol anulado (−1).
  Sim a(goalsCfg(10));
  a.start();
  a.goal(Team::White);
  Before b = before(a.e);
  a.e.minusOne(Team::White, a.t);
  assert(eq(a.apply(false, b), {Event::GoalAnnulled}));

  // Por tiempo: fin de la parte, prórroga con gol de oro y victoria; penaltis.
  Config tc;
  tc.endCondition = EndCondition::Time;
  tc.minutesPerPeriod = 1;
  Sim g(tc);
  g.start();
  g.goal(Team::White);
  assert(eq(g.wait(60000), {Event::PeriodEnd}));
  g.e.next(g.t);
  g.start();
  g.goal(Team::Blue);
  g.wait(60000);
  g.e.next(g.t);
  assert(g.e.period() == Period::Overtime);
  g.start();
  assert(eq(g.goal(Team::Blue), {Event::GoldenGoal, Event::Victory}));

  Sim p(tc);
  p.start();
  p.wait(60000);
  p.e.next(p.t);
  p.start();
  p.wait(60000);
  p.e.next(p.t);
  p.start();
  p.wait(60000);
  p.e.next(p.t);
  assert(p.e.phase() == Phase::Penalties);
  Before pb = before(p.e);
  p.e.penalty(p.e.nextKicker(), true, p.t);
  assert(eq(p.apply(false, pb), {Event::PenaltyScored}));
  pb = before(p.e);
  p.e.penalty(p.e.nextKicker(), false, p.t);
  assert(eq(p.apply(false, pb), {Event::PenaltyMissed}));
}

static void testChoices() {
  Choices ch;
  assert(ch.set("goal 1a2b3c4d,0000beef"));
  assert(ch.set("rout no"));
  assert(!ch.set("nada no"));
  assert(!ch.set("goal zz"));
  assert(ch.c[(int)Event::Goal].kind == Kind::Custom && ch.c[(int)Event::Goal].n == 2 && ch.c[(int)Event::Goal].codes[1] == 0xbeef);
  char buf[256];
  ch.serialize(buf, sizeof(buf));
  assert(!strcmp(buf, "goal 1a2b3c4d,0000beef;rout no"));
  Choices back;
  back.parse(buf);
  assert(back.c[(int)Event::Goal].n == 2 && back.c[(int)Event::Rout].kind == Kind::None);
  assert(back.c[(int)Event::Victory].kind == Kind::Default);
  assert(ch.set("goal def") && ch.c[(int)Event::Goal].kind == Kind::Default);
  assert(eventFromName("matchPoint", 10) == (int)Event::MatchPoint);
}

static void testCodec() {
  // Mismos valores que calcula la app (tests/boardSounds.test.ts).
  assert(soundCode("snd_abc") == 0xa9f86d43u);
  assert(soundCode("") == 2166136261u);
  const uint8_t nine[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  assert(crc32Update(0, nine, 9) == 0xCBF43926u);
  assert(crc32Update(crc32Update(0, nine, 4), nine + 4, 5) == 0xCBF43926u);  // por trozos
  uint8_t out[16];
  assert(base64Decode("aG9sYQ==", 8, out, sizeof(out)) == 4 && !memcmp(out, "hola", 4));
  assert(base64Decode("YWJj", 4, out, sizeof(out)) == 3);
  assert(base64Decode("YWJ", 3, out, sizeof(out)) == -1);
  assert(base64Decode("Y=Jj", 4, out, sizeof(out)) == -1);
  uint32_t code;
  assert(parseCode("00C0FFEE", 8, code) && code == 0xC0FFEE);
  assert(!parseCode("C0FFEE", 6, code));

  // Línea larga de datos (pantalla de 7"): cabe en el búfer grande y una demasiado larga se descarta entera.
  mfv3::LineBufferN<384> lb;
  std::string big = "SONIDO_DATOS " + std::string(256, 'A');
  bool got = false;
  for (char ch : big) got = lb.push(ch) || got;
  assert(lb.push('\n') && big == lb.line());
  std::string huge(500, 'B');
  for (char ch : huge) lb.push(ch);
  assert(!lb.push('\n'));
  for (char ch : std::string("PING")) lb.push(ch);
  assert(lb.push('\n') && !strcmp(lb.line(), "PING"));
}

static std::vector<uint8_t> wav16(uint32_t rate, uint16_t ch, uint32_t frames) {
  std::vector<uint8_t> d(44 + frames * ch * 2);
  auto u32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) d[at + i] = (uint8_t)(v >> (8 * i)); };
  auto u16 = [&](size_t at, uint16_t v) { d[at] = (uint8_t)v; d[at + 1] = (uint8_t)(v >> 8); };
  memcpy(&d[0], "RIFF", 4);
  u32(4, (uint32_t)d.size() - 8);
  memcpy(&d[8], "WAVEfmt ", 8);
  u32(16, 16);
  u16(20, 1);
  u16(22, ch);
  u32(24, rate);
  u32(28, rate * ch * 2);
  u16(32, (uint16_t)(ch * 2));
  u16(34, 16);
  memcpy(&d[36], "data", 4);
  u32(40, frames * ch * 2);
  return d;
}

static void testWavAndResample() {
  std::vector<uint8_t> w = wav16(22050, 1, 100);
  WavInfo info;
  assert(parseWav(w.data(), w.size(), info) && info.rate == 22050 && info.channels == 1 && info.dataOffset == 44 && info.dataBytes == 200);
  w[20] = 3;  // no PCM
  assert(!parseWav(w.data(), w.size(), info));
  assert(!parseWav((const uint8_t*)"hola", 4, info));

  // 22 050 → 16 000: el número de muestras sale en proporción y una rampa sigue siendo rampa.
  std::vector<int16_t> in(22050);
  for (size_t i = 0; i < in.size(); i++) in[i] = (int16_t)(i % 1000);
  Resampler rs;
  rs.begin(22050, 16000);
  std::vector<int16_t> out;
  int16_t buf[300];
  size_t pos = 0;
  while (pos < in.size()) {
    size_t used = 0;
    size_t n = rs.process(&in[pos], std::min<size_t>(500, in.size() - pos), buf, 300, used);
    out.insert(out.end(), buf, buf + n);
    pos += used;
    if (!n && !used) break;
  }
  assert(out.size() > 15900 && out.size() <= 16000);
  assert(out[100] > out[50]);
  // Misma frecuencia: copia exacta.
  rs.begin(16000, 16000);
  size_t used = 0;
  int16_t a[4] = {1, 2, 3, 4}, b[8];
  size_t n = rs.process(a, 4, b, 8, used);
  assert(n == 3 && b[0] == 1 && b[2] == 3);

  // Notas: duran lo que deben y suenan.
  ToneGen tg;
  tg.start(defaultTune(Event::Goal), 16000);
  size_t total = 0;
  int16_t peak = 0;
  int16_t t[256];
  while (tg.active()) {
    size_t k = tg.render(t, 256);
    for (size_t i = 0; i < k; i++) peak = std::max(peak, t[i]);
    total += k;
    if (!k) break;
  }
  assert(total > 16000 * 600 / 1000 && total < 16000 * 700 / 1000 && peak > 3000);
}

// ---------------------------------------------------------------- recepción por el protocolo
struct MemStore : Store {
  std::map<uint32_t, std::pair<Place, std::vector<uint8_t>>> files;
  bool sd = false;
  uint32_t chipKB = 2000;
  Place curPlace = Place::Chip;
  uint32_t curCode = 0;
  std::vector<uint8_t> cur;
  bool open = false;
  bool available(Place p) override { return p == Place::Chip || sd; }
  uint32_t freeKB(Place p) override { return p == Place::Chip ? chipKB : 8000; }
  bool begin(Place p, uint32_t code) override { curPlace = p; curCode = code; cur.clear(); open = true; return true; }
  bool write(const uint8_t* d, size_t n) override { cur.insert(cur.end(), d, d + n); return true; }
  bool commit() override { files[curCode] = {curPlace, cur}; open = false; return true; }
  void abort() override { open = false; cur.clear(); }
  bool remove(uint32_t code) override { return files.erase(code) > 0; }
  bool find(uint32_t code, Place& where) override {
    auto it = files.find(code);
    if (it == files.end()) return false;
    where = it->second.first;
    return true;
  }
  size_t list(uint32_t* codes, Place* places, size_t cap) override {
    size_t n = 0;
    for (auto& f : files) {
      if (n >= cap) break;
      codes[n] = f.first;
      places[n++] = f.second.first;
    }
    return n;
  }
};

static std::vector<std::string> replies;
static void reply(const char* l) { replies.push_back(l); }

static std::string b64(const uint8_t* d, size_t n) {
  static const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string s;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
    s += A[(v >> 18) & 63];
    s += A[(v >> 12) & 63];
    s += i + 1 < n ? A[(v >> 6) & 63] : '=';
    s += i + 2 < n ? A[v & 63] : '=';
  }
  return s;
}

static void testCommands() {
  MemStore st;
  Choices ch;
  Commands cmd(st, ch);
  Action act;
  std::vector<uint8_t> file = wav16(16000, 1, 1000);
  for (size_t i = 44; i < file.size(); i++) file[i] = (uint8_t)(i * 7);
  char line[400];

  // SD sin tarjeta: error claro.
  replies.clear();
  snprintf(line, sizeof(line), "SONIDO_SUBIR sd 0000abcd %zu", file.size());
  assert(cmd.handle(line, 0, reply, act) && replies.back() == "SONIDO_ERROR 0000abcd sin_sd");

  // En la memoria del chip, en trozos de 192 bytes con su respuesta.
  snprintf(line, sizeof(line), "SONIDO_SUBIR chip 0000abcd %zu", file.size());
  cmd.handle(line, 0, reply, act);
  assert(replies.back() == "SONIDO_LISTO 0000abcd" && cmd.receiving());
  for (size_t i = 0; i < file.size(); i += 192) {
    size_t n = std::min<size_t>(192, file.size() - i);
    std::string l = "SONIDO_DATOS " + b64(&file[i], n);
    cmd.handle(l.c_str(), 10, reply, act);
    assert(replies.back() == "SONIDO_OK " + std::to_string(i + n));
  }
  snprintf(line, sizeof(line), "SONIDO_FIN %08lx", (unsigned long)crc32Update(0, file.data(), file.size()));
  cmd.handle(line, 20, reply, act);
  assert(replies.back() == "SONIDO_GUARDADO 0000abcd" && !cmd.receiving());
  assert(st.files[0xabcd].second == file && st.files[0xabcd].first == Place::Chip);

  // CRC mal: no se guarda.
  st.sd = true;
  snprintf(line, sizeof(line), "SONIDO_SUBIR sd 00001234 3");
  cmd.handle(line, 0, reply, act);
  cmd.handle("SONIDO_DATOS YWJj", 0, reply, act);
  cmd.handle("SONIDO_FIN 00000000", 0, reply, act);
  assert(replies.back() == "SONIDO_ERROR 00001234 crc" && !st.files.count(0x1234));

  // Sin datos durante 8 s: se cancela.
  cmd.handle("SONIDO_SUBIR sd 00001234 3", 0, reply, act);
  cmd.tick(9000, reply);
  assert(replies.back() == "SONIDO_ERROR 00001234 tiempo" && !cmd.receiving());

  // Demasiado grande o sin espacio.
  cmd.handle("SONIDO_SUBIR chip 00001234 5000000", 0, reply, act);
  assert(replies.back() == "SONIDO_ERROR 00001234 tamano");
  st.chipKB = 1;
  cmd.handle("SONIDO_SUBIR chip 00001234 4000", 0, reply, act);
  assert(replies.back() == "SONIDO_ERROR 00001234 sin_espacio");

  // Lista, tocar, borrar, momentos y volumen.
  cmd.handle("SONIDOS?", 0, reply, act);
  assert(replies.back() == "SONIDOS chip=1 sd=8000 0000abcd@c");
  assert(cmd.handle("SONIDO 0000abcd", 0, reply, act) && act.play && act.code == 0xabcd && !act.queue);
  assert(cmd.handle("SONIDO 0000abcd cola", 0, reply, act) && act.play && act.code == 0xabcd && act.queue);
  assert(cmd.handle("MOMENTO goal 0000abcd", 0, reply, act) && act.choicesChanged && replies.back() == "MOMENTO_OK goal");
  assert(ch.c[(int)Event::Goal].kind == Kind::Custom);
  assert(cmd.handle("VOLUMEN 150", 0, reply, act) && act.volumeChanged && act.volume == 100);
  cmd.handle("SONIDO_BORRAR 0000abcd", 0, reply, act);
  assert(replies.back() == "SONIDO_BORRADO 0000abcd" && st.files.empty());
  assert(!cmd.handle("GOL_BLANCO", 0, reply, act));
}

int main() {
  testDetect();
  testChoices();
  testCodec();
  testWavAndResample();
  testCommands();
  printf("sonidos de la placa: OK\n");
  return 0;
}
