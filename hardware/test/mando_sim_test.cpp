// Mando de 2 pulsadores (sketch mando_pulsadores) simulado en el PC: toque corto, largo y paquetes de radio.
#include <cassert>
#include <cstring>
#include <vector>
#include "Arduino.h"
#include "../mando_pulsadores/mando_pulsadores.ino"

static void run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 2) loop(); }
// Solo los paquetes de goles (MFV3); los de actualización (MFVO) van aparte.
static std::vector<std::vector<uint8_t>> cmds() {
  std::vector<std::vector<uint8_t>> v;
  for (auto& p : sim::radioSent)
    if (p.size() == mfv3::RADIO_PACKET_SIZE && !memcmp(p.data(), "MFV3", 4)) v.push_back(p);
  return v;
}
static mfv3::RadioCmd lastCmd() {
  auto v = cmds();
  assert(!v.empty());
  mfv3::RadioMsg m = mfv3::radioDecode(v.back().data(), (int)v.back().size(), GRUPO_MESA);
  assert(m.ok);
  return m.cmd;
}
static mfv3::ota::Info lastInfo() {
  mfv3::ota::Info last = {false, 0, false, 0};
  for (auto& p : sim::radioSent) {
    mfv3::ota::Info i = mfv3::ota::decodeInfo(p.data(), (int)p.size(), GRUPO_MESA);
    if (i.ok) last = i;
  }
  return last;
}
struct ImgSource : mfv3::ota::Source {
  const std::vector<uint8_t>& img;
  explicit ImgSource(const std::vector<uint8_t>& i) : img(i) {}
  bool read(uint32_t off, uint8_t* buf, uint8_t len) override {
    memcpy(buf, img.data() + off, len);
    return true;
  }
};

int main() {
  setup();
#if MFV3_MANDO_RGB_PIN >= 0
  assert(sim::rgbPin == 10 && sim::modes[8] != OUTPUT);
  ledWrite(true);
  assert(sim::rgbRed == 8 && sim::rgbGreen == 8 && sim::rgbBlue == 8);
  ledWrite(false);
  assert(sim::rgbRed == 0 && sim::rgbGreen == 0 && sim::rgbBlue == 0);
#else
  assert(sim::modes[8] == OUTPUT && sim::rgbPin == -1);
#endif
  assert(sim::modes[3] == INPUT_PULLUP && sim::modes[4] == INPUT_PULLUP);
  assert(cmds().size() == 3 && lastCmd() == RadioCmd::Hello && sim::wifiChannel == mfv3::RADIO_CHANNEL);
  // Con el saludo, su versión (compilado a mano: 0) y sin armar.
  assert(lastInfo().ok && lastInfo().build == MFV3_BUILD && !lastInfo().armed);
  // Y su modelo, para que la pantalla le pase el programa de su mini.
#if MFV3_MANDO_RGB_PIN >= 0
  assert(lastInfo().model == mfv3::ota::MODEL_C3_ZERO);
#else
  assert(lastInfo().model == mfv3::ota::MODEL_SUPER_MINI);
#endif
  sim::radioSent.clear();
  run(50);
  // Toque corto en BLANCO (pulsado = LOW): 3 copias de GOL al soltar.
  sim::pins[PIN_BTN_BLANCO] = LOW; run(200);
  assert(sim::radioSent.empty());
  sim::pins[PIN_BTN_BLANCO] = HIGH; run(100);
  assert(cmds().size() == 3 && lastCmd() == RadioCmd::GoalWhite);
  // Las 3 copias llevan la misma secuencia.
  assert(sim::radioSent[0] == sim::radioSent[2]);
  // Toque largo en AZUL: ANULAR mientras sigue pulsado; al soltar no hay gol.
  sim::radioSent.clear();
  sim::pins[PIN_BTN_AZUL] = LOW; run(900);
  assert(cmds().size() == 3 && lastCmd() == RadioCmd::AnnulBlue);
  sim::pins[PIN_BTN_AZUL] = HIGH; run(200);
  assert(cmds().size() == 3);
  // Saludo periódico.
  sim::radioSent.clear();
  run(SALUDO_CADA_MS + 100);
  assert(!cmds().empty() && lastCmd() == RadioCmd::Hello);

  // Actualización por radio. Sin armar, el mando contesta «no armado» y no escribe nada.
  std::vector<uint8_t> img(5000);
  for (size_t i = 0; i < img.size(); i++) img[i] = (uint8_t)(i * 7);
  uint8_t md5[16];
  assert(mfv3::ota::md5FromHex(sim::fakeMd5(img).c_str(), md5));
  ImgSource src(img);
  mfv3::ota::Sender tx;
  uint8_t mac[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
  size_t seen = sim::radioSent.size();
  auto exchange = [&](uint32_t ms) {
    uint8_t out[mfv3::ota::MAX_PACKET];
    for (uint32_t t = 0; t < ms; t += 2) {
      uint8_t n = tx.poll(sim::now, src, out);
      if (n) sim::radioDeliver(mac, std::vector<uint8_t>(out, out + n));
      loop();
      for (; seen < sim::radioSent.size(); seen++) tx.onPacket(sim::radioSent[seen].data(), (int)sim::radioSent[seen].size(), sim::now);
      if (tx.state() == mfv3::ota::Sender::State::Done || tx.state() == mfv3::ota::Sender::State::Failed) break;
    }
  };
  tx.start(GRUPO_MESA, 99, (uint32_t)img.size(), md5, sim::now);
  exchange(2000);
  assert(tx.waitingButtons() && !sim::updateOpen);
  // Los dos botones 3 s: queda armado; mantenerlos no anula ningún gol.
  sim::radioSent.clear();
  seen = 0;
  sim::pins[PIN_BTN_BLANCO] = LOW;
  sim::pins[PIN_BTN_AZUL] = LOW;
  run(3200);
  assert(cmds().empty() && lastInfo().armed);
  sim::pins[PIN_BTN_BLANCO] = HIGH;
  sim::pins[PIN_BTN_AZUL] = HIGH;
  run(100);
  assert(cmds().empty());  // al soltar tampoco hay gol
  seen = sim::radioSent.size();
  exchange(60000);
  assert(tx.state() == mfv3::ota::Sender::State::Done && sim::updateDone && sim::updateData == img);
  assert(sim::restarts == 0);
  run(1600);  // reinicia con el programa nuevo
  assert(sim::restarts >= 1);
  std::puts("mando_pulsadores.ino: toque corto = gol, toque largo = anular, radio y actualización por radio OK");
}
