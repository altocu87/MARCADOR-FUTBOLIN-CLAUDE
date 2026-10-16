// Recorre las pantallas de la placa de 7" y guarda una captura PNG de cada una (ver test/capturas.sh).
#include <zlib.h>
#include <cstdio>
#include <string>
#include <vector>
#include "Arduino.h"
#include "esp32s3_pantalla7.ino"

static std::string outDir = ".";

static void loopFor(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10) loop(); }
static void tap(int x, int y) {
  sim::touchX = x; sim::touchY = y; sim::touchDown = true; loop();
  sim::touchDown = false; loop();
}
static void usb(const char* s) { for (const char* c = s; *c; c++) sim::in.push_back(*c); sim::in.push_back('\n'); loop(); }

// PNG de 24 bits sin dependencias aparte de zlib.
static void chunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
  uint8_t len[4] = {(uint8_t)(data.size() >> 24), (uint8_t)(data.size() >> 16), (uint8_t)(data.size() >> 8), (uint8_t)data.size()};
  fwrite(len, 1, 4, f);
  uLong crc = crc32(0, (const Bytef*)type, 4);
  fwrite(type, 1, 4, f);
  if (!data.empty()) { fwrite(data.data(), 1, data.size(), f); crc = crc32(crc, data.data(), (uInt)data.size()); }
  uint8_t c[4] = {(uint8_t)(crc >> 24), (uint8_t)(crc >> 16), (uint8_t)(crc >> 8), (uint8_t)crc};
  fwrite(c, 1, 4, f);
}

static void shot(const char* name) {
  loop();
  const int W = 800, H = 480;
  std::vector<uint8_t> raw;
  raw.reserve((W * 3 + 1) * H);
  for (int y = 0; y < H; y++) {
    raw.push_back(0);
    for (int x = 0; x < W; x++) {
      uint16_t c = lcd.readPixel(x, y);
      raw.push_back((uint8_t)(((c >> 11) & 31) * 255 / 31));
      raw.push_back((uint8_t)(((c >> 5) & 63) * 255 / 63));
      raw.push_back((uint8_t)((c & 31) * 255 / 31));
    }
  }
  uLongf zlen = compressBound((uLong)raw.size());
  std::vector<uint8_t> z(zlen);
  compress2(z.data(), &zlen, raw.data(), (uLong)raw.size(), 9);
  z.resize(zlen);
  std::string path = outDir + "/" + name + ".png";
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) { perror(path.c_str()); return; }
  static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  fwrite(sig, 1, 8, f);
  std::vector<uint8_t> ihdr = {0, 0, 3, 0x20, 0, 0, 1, 0xE0, 8, 2, 0, 0, 0};  // 800×480, RGB de 8 bits
  chunk(f, "IHDR", ihdr);
  chunk(f, "IDAT", z);
  chunk(f, "IEND", {});
  fclose(f);
  printf("  %s\n", path.c_str());
}

int main(int argc, char** argv) {
  if (argc > 1) outDir = argv[1];
  sim::now = 1000;
  setup();
  loop();
  shot("01-prueba-placa");
  tap(20, 20); tap(780, 20); tap(20, 460); tap(780, 460);
  shot("02-prueba-placa-esquinas");
  tap(400, 430);  // TERMINAR
  shot("03-inicio");
  tap(ui::HOME[7].r.x + 5, ui::HOME[7].r.y + 5);  // JUGAR
  shot("04-cuenta-atras");
  loopFor(4000);
  usb("GB"); loopFor(3500); usb("GB"); loopFor(3500); usb("GA");
  shot("05-partido-gol");
  loopFor(3500); usb("GB"); loopFor(3500); usb("GB"); loopFor(3500);
  shot("06-partido-bola-de-partido");
  usb("PAUSA");
  shot("07-pausa");
  usb("PAUSA");
  usb("GB");
  loopFor(1000);
  shot("08-victoria");
  return 0;
}
