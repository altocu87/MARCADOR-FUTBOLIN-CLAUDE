// Recorre las pantallas de la placa de 7" y guarda una captura PNG de cada una (ver test/capturas.sh).
#include <zlib.h>
#include <cstdio>
#include <cstring>
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

  // AJUSTES, Wi-Fi, teclado y actualizar (red y versiones de mentira).
  const std::string base = "https://altocu87.github.io/MARCADOR-FUTBOLIN-CLAUDE/firmware/";
  std::string bin(2000, 'x');
  std::vector<uint8_t> v(bin.begin(), bin.end());
  std::string js = "{\"build\": 61, \"size\": 2000, \"md5\": \"" + sim::fakeMd5(v) + "\"}";
  sim::web[base + "waveshare-7c/ota.json"] = js;
  sim::web[base + "mando-c3/ota.json"] = js;
  sim::web[base + "mando-c3/mando-c3-ota.bin"] = bin;
  sim::networks = {{"CasaPepe", "clave", -48}, {"MOVISTAR_8A21", "x", -66}, {"Vodafone-5G", "x", -74}, {"Bar abierto", "", -83}};
  tap(260, 410);  // INICIO
  tap(100, 428);  // AJUSTES
  shot("09-ajustes");
  tap(400, 116);  // WI-FI
  loop();
  shot("10-wifi");
  tap(400, 105);  // CasaPepe
  for (char c : std::string("cla")) {
    for (uint8_t r = 0; r < 4; r++)
      for (uint8_t i = 0; ui::KEYS_LOWER[r][i]; i++)
        if (ui::KEYS_LOWER[r][i] == c) {
          ui::Rect k = ui::keyRect(r, i, (uint8_t)strlen(ui::KEYS_LOWER[r]));
          tap(k.x + 10, k.y + 10);
        }
  }
  shot("11-teclado");
  strcpy(kbText, "clave");
  tap(664, 448);  // CONECTAR
  loopFor(600);
  // El mando dice su versión antigua: hay nueva para los dos.
  uint8_t pk[mfv3::ota::MAX_PACKET];
  uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
  uint8_t n = mfv3::ota::encodeInfo(pk, GRUPO_MESA, 57, false);
  sim::radioDeliver(mac, std::vector<uint8_t>(pk, pk + n));
  shot("12-actualizar");
  tap(590, 432);  // INSTALAR
  loopFor(600);
  loop();
  loop();
  // El mando contesta «no armado»: hay que pulsar sus dos botones.
  uint8_t acc[16];
  uint8_t an = mfv3::ota::header(acc, mfv3::ota::Type::Accept, GRUPO_MESA);
  acc[an] = (uint8_t)mfv3::ota::Status::NotArmed;
  sim::radioDeliver(mac, std::vector<uint8_t>(acc, acc + an + 1));
  shot("13-instalar-mando");
  screen = Screen::Home;
  lastKey = 0xFFFFFFFF;
  shot("14-inicio-con-aviso");

  // Hora y batería (en el PC no hay reloj ni medidor: se ponen a mano justo antes de cada captura).
  auto clockAndBattery = [](uint8_t pct, bool charging) {
    rtc.clockOk = true;
    rtc.now = reloj::Clock{2026, 10, 17, 18, 42, 0};
    rtc.bat = reloj::Battery{true, charging, pct, 3900};
    rtc.clockAt = rtc.batAt = millis();
  };
  pubScreen.build = 0;
  pubMando.build = 0;
  clockAndBattery(85, false);
  lastKey = 0xFFFFFFFF;
  shot("15-inicio-hora-bateria");
  clockAndBattery(12, false);
  shot("16-inicio-bateria-baja");
  clockAndBattery(12, false);
  dimmed = true;
  lastSleepKey = 0xFFFFFFFF;
  shot("17-reposo-reloj");
  return 0;
}
