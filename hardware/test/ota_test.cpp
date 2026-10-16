// Pruebas en PC de mfv3_ota.h: la pantalla pasa un programa al mando por radio, con paquetes perdidos y
// repetidos; el mando solo lo acepta armado (dos botones) y lo comprueba antes de quedárselo.
#include <cassert>
#include <cstdio>
#include <vector>
#include "mfv3_ota.h"

using namespace mfv3::ota;

static const uint8_t GROUP = 1;

// «Flash» del mando en memoria; el MD5 se sustituye por una suma sencilla (la placa usa el MD5 real).
struct MemFlash : Flash {
  std::vector<uint8_t> data;
  uint8_t want[16] = {0};
  uint32_t size = 0;
  bool open = false, finished = false, failWrite = false;
  bool begin(uint32_t s, const uint8_t md5[16]) override {
    size = s;
    memcpy(want, md5, 16);
    data.clear();
    open = true;
    return true;
  }
  bool write(const uint8_t* d, uint8_t n) override {
    if (failWrite) return false;
    data.insert(data.end(), d, d + n);
    return true;
  }
  bool end() override {
    open = false;
    uint8_t got[16];
    sum(data, got);
    finished = data.size() == size && memcmp(got, want, 16) == 0;
    return finished;
  }
  void abort() override { open = false; data.clear(); }
  static void sum(const std::vector<uint8_t>& v, uint8_t out[16]) {
    uint32_t h = 2166136261u;
    for (uint8_t b : v) h = (h ^ b) * 16777619u;
    for (int i = 0; i < 16; i++) out[i] = (uint8_t)(h >> (8 * (i % 4))) ^ (uint8_t)i;
  }
};

struct MemSource : Source {
  const std::vector<uint8_t>& img;
  explicit MemSource(const std::vector<uint8_t>& i) : img(i) {}
  bool read(uint32_t off, uint8_t* buf, uint8_t len) override {
    if (off + len > img.size()) return false;
    memcpy(buf, img.data() + off, len);
    return true;
  }
};

// Radio simulada: pierde un paquete de cada `lossEvery` (0 = ninguno) en cada sentido y repite otros.
struct Link {
  int lossEvery = 0, n = 0;
  bool drop() { return lossEvery && (++n % lossEvery) == 0; }
};

static std::vector<uint8_t> image(size_t n) {
  std::vector<uint8_t> v(n);
  for (size_t i = 0; i < n; i++) v[i] = (uint8_t)(i * 31 + (i >> 8));
  return v;
}

// Corre emisor y receptor hasta que el emisor termina (o pasa el tiempo). Devuelve los ms simulados.
static uint32_t run(Sender& s, Receiver& r, MemFlash& f, MemSource& src, Link& down, Link& up, uint32_t& now,
                    uint32_t maxMs, uint32_t armAtMs = 0) {
  uint8_t out[MAX_PACKET], reply[MAX_PACKET];
  uint32_t start = now;
  while (now - start < maxMs) {
    if (armAtMs && now - start >= armAtMs) {
      r.arm(now);
      armAtMs = 0;
    }
    uint8_t n = s.poll(now, src, out);
    if (n && !down.drop()) {
      uint8_t m = r.handle(out, n, now, f, reply);
      if (n > HEADER && out[5] == (uint8_t)Type::Data && (now % 13) == 0) r.handle(out, n, now, f, reply);  // duplicado
      if (m && !up.drop()) s.onPacket(reply, m, now);
    }
    r.tick(now, f);
    if (s.state() == Sender::State::Done || s.state() == Sender::State::Failed) break;
    now += 2;
  }
  return now - start;
}

int main() {
  // Paquetes INFO: versión del mando y si está armado; otras mesas y paquetes de goles no valen.
  uint8_t p[MAX_PACKET];
  uint8_t n = encodeInfo(p, GROUP, 57, true);
  Info i = decodeInfo(p, n, GROUP);
  assert(i.ok && i.build == 57 && i.armed);
  assert(!decodeInfo(p, n, GROUP + 1).ok);
  const uint8_t goal[10] = {'M', 'F', 'V', '3', 1, 1, 0, 0, 1, 0};
  assert(!decodeInfo(goal, 10, GROUP).ok && typeOf(goal, 10, GROUP) == 0);
  assert(MAX_PACKET <= 250);

  std::vector<uint8_t> img = image(23456);  // no es múltiplo de 200: el último trozo es más corto
  uint8_t md5[16];
  MemFlash::sum(img, md5);
  MemSource src(img);

  // 1) Sin armar: el mando contesta «no armado» y la pantalla sigue ofreciendo; al armarlo, pasa todo.
  {
    MemFlash f;
    Receiver r(GROUP, 1200000);
    Sender s;
    Link down, up;
    uint32_t now = 1000;
    s.start(GROUP, 58, (uint32_t)img.size(), md5, now);
    run(s, r, f, src, down, up, now, 3000);
    assert(s.state() == Sender::State::Offering && s.waitingButtons() && !f.open);
    run(s, r, f, src, down, up, now, 60000, 1);
    assert(s.state() == Sender::State::Done && f.finished && f.data == img && s.percent() == 100);
    assert(r.state() == Receiver::State::Finished && !r.armed(now));
  }

  // 2) Con pérdidas en los dos sentidos (1 de cada 5 y 1 de cada 7) y duplicados: llega igual.
  {
    MemFlash f;
    Receiver r(GROUP, 1200000);
    Sender s;
    Link down, up;
    down.lossEvery = 5;
    up.lossEvery = 7;
    uint32_t now = 5000;
    r.arm(now);
    s.start(GROUP, 58, (uint32_t)img.size(), md5, now);
    uint32_t ms = run(s, r, f, src, down, up, now, 120000);
    assert(s.state() == Sender::State::Done && f.finished && f.data == img);
    std::printf("  23 KB con pérdidas: %u ms simulados\n", ms);
  }

  // 3) Programa demasiado grande para el mando: lo rechaza.
  {
    MemFlash f;
    Receiver r(GROUP, 10000);
    Sender s;
    Link down, up;
    uint32_t now = 0;
    r.arm(now);
    s.start(GROUP, 58, (uint32_t)img.size(), md5, now);
    run(s, r, f, src, down, up, now, 5000);
    assert(s.state() == Sender::State::Failed && s.error() == Sender::Error::Refused && !f.open);
  }

  // 4) MD5 que no cuadra (programa corrupto): el mando no se lo queda y la pantalla lo sabe.
  {
    MemFlash f;
    Receiver r(GROUP, 1200000);
    Sender s;
    Link down, up;
    uint32_t now = 0;
    r.arm(now);
    uint8_t bad[16];
    memcpy(bad, md5, 16);
    bad[3] ^= 0xFF;
    s.start(GROUP, 58, (uint32_t)img.size(), bad, now);
    run(s, r, f, src, down, up, now, 60000);
    assert(s.state() == Sender::State::Failed && s.error() == Sender::Error::Verify && !f.finished);
    assert(r.state() == Receiver::State::Idle);
  }

  // 5) El mando se apaga a medias: la pantalla se rinde tras los reintentos y el mando, al rato, lo abandona.
  {
    MemFlash f;
    Receiver r(GROUP, 1200000);
    Sender s;
    Link down, up;
    uint32_t now = 0;
    r.arm(now);
    s.start(GROUP, 58, (uint32_t)img.size(), md5, now);
    run(s, r, f, src, down, up, now, 100);
    assert(s.state() == Sender::State::Sending && f.open && r.received() > 0);
    down.lossEvery = 1;  // ya no llega nada
    run(s, r, f, src, down, up, now, 60000);
    assert(s.state() == Sender::State::Failed && s.error() == Sender::Error::Lost);
    assert(r.state() == Receiver::State::Receiving);
    r.tick(now + RX_IDLE_MS + 1, f);  // más de 15 s sin datos
    assert(r.state() == Receiver::State::Idle && !f.open);
  }

  // 6) Nadie contesta (mando apagado): se acaba el tiempo de espera de la oferta.
  {
    MemFlash f;
    Receiver r(GROUP, 1200000);
    Sender s;
    Link down, up;
    down.lossEvery = 1;
    uint32_t now = 0;
    s.start(GROUP, 58, (uint32_t)img.size(), md5, now);
    run(s, r, f, src, down, up, now, Sender::OFFER_WAIT_MS + 1000);
    assert(s.state() == Sender::State::Failed && s.error() == Sender::Error::NoAnswer);
  }

  // 7) Fallo al escribir en el mando: avisa y no se queda a medias.
  {
    MemFlash f;
    f.failWrite = true;
    Receiver r(GROUP, 1200000);
    Sender s;
    Link down, up;
    uint32_t now = 0;
    r.arm(now);
    s.start(GROUP, 58, (uint32_t)img.size(), md5, now);
    run(s, r, f, src, down, up, now, 5000);
    assert(s.state() == Sender::State::Failed && r.state() == Receiver::State::Idle);
  }

  // 8) El armado caduca a los 2 minutos.
  {
    Receiver r(GROUP, 1000);
    r.arm(0);
    assert(r.armed(ARM_MS - 1) && !r.armed(ARM_MS + 1));
  }

  // ota.json publicado por GitHub.
  Published pub;
  assert(parsePublished("{\"build\": 57, \"size\": 1234567, \"md5\": \"0123456789ABCDEF0123456789abcdef\"}", pub));
  assert(pub.build == 57 && pub.size == 1234567 && !strcmp(pub.md5, "0123456789abcdef0123456789abcdef"));
  assert(!parsePublished("{\"build\": 57, \"size\": 10, \"md5\": \"0123\"}", pub));
  assert(!parsePublished("<html>404</html>", pub));
  assert(!parsePublished("{\"size\": 10, \"md5\": \"0123456789abcdef0123456789abcdef\"}", pub));
  uint8_t raw[16];
  assert(md5FromHex("000102030405060708090a0b0c0d0eff", raw) && raw[1] == 1 && raw[15] == 0xFF);
  assert(!md5FromHex("00zz", raw));

  std::printf("mfv3_ota: radio pantalla → mando con pérdidas, armado y comprobación OK\n");
}
