// MARCADOR FUTBOLÍN V3 · tarjeta SD y altavoz de la Waveshare ESP32-S3-Touch-LCD-7C / 7C-BOX.
// -----------------------------------------------------------------------------------------------
// Pines del repositorio del fabricante (github.com/waveshareteam/ESP32-S3-Touch-LCD-7C, ejemplos 05_SD y
// 09_SPEAKER_MICROPHONE):
//   - SD: SDMMC de 1 bit, CLK GPIO12, CMD GPIO11, D0 GPIO13 (CS del expansor EXIO4 en alto).
//   - Audio: códec ES8389 (I2C 0x10, mismo bus que el táctil) + I2S: MCLK GPIO6, BCLK GPIO44, WS GPIO16,
//     DOUT GPIO15. El amplificador del altavoz se enciende con EXIO3.
//
// Sonidos propios en la SD: /sonidos/<momento>/*.wav (ver s3_sound.h para los nombres de carpeta). Si hay
// varios, suena uno al azar; si la carpeta tiene un archivo llamado «ninguno», ese momento no suena; si no hay
// ninguno (o no hay SD), suena el de siempre, como en la app. WAV PCM de 8/16 bits, mono o estéreo, 8–48 kHz.
// Suena en una tarea aparte: la pantalla nunca espera al altavoz. Un momento nuevo corta el que suena.
//
// En otras placas (sin este altavoz) todo queda desactivado y no hace nada.

#ifndef MFV3_S3_AUDIO_H
#define MFV3_S3_AUDIO_H

#include <Arduino.h>
#include "board_config.h"
#include "s3_sound.h"

#if MFV3_BOARD == BOARD_WAVESHARE_7C
#define HAS_AUDIO 1
#include <FS.h>
#include <SD_MMC.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#else
#define HAS_AUDIO 0
#endif

namespace s3audio {

using s3snd::Ev;

struct Status {
  bool sd = false;       // tarjeta montada
  bool codec = false;    // el ES8389 contesta por I2C
  bool i2s = false;      // salida de audio lista
  uint8_t custom = 0;    // momentos con sonido propio en la SD
  uint32_t sdMB = 0;     // tamaño de la tarjeta
};
inline Status status;
inline volatile uint8_t volume = 80;  // 0–100

#if HAS_AUDIO

const int SD_CLK = 12, SD_CMD = 11, SD_D0 = 13;
const gpio_num_t I2S_MCLK = GPIO_NUM_6, I2S_BCLK = GPIO_NUM_44, I2S_WS = GPIO_NUM_16, I2S_DOUT = GPIO_NUM_15;
const uint8_t CODEC_ADDR = 0x10;
const uint32_t TONE_RATE = 22050;
const uint8_t MAX_FILES = 8;

struct Req {
  uint8_t ev;
  uint32_t gen;
};

struct Folder {
  String files[MAX_FILES];
  uint8_t count = 0;
  bool silent = false;
};
inline Folder folders[s3snd::EV_COUNT];
inline QueueHandle_t queue = nullptr;
inline volatile uint32_t generation = 0;  // cada play() nuevo corta lo que suena
inline i2s_chan_handle_t tx = nullptr;
inline uint32_t currentRate = 0;

// ---------------------------------------------------------------- códec ES8389
// Misma secuencia que es8389_open() + es8389_enable() del driver de Espressif (esp_codec_dev, Apache-2.0) con
// la configuración del ejemplo del fabricante: esclavo, MCLK externo, I2S normal de 16 bits, DAC a 0 dB.
inline bool codecInit(LGFX& lcd) {
  static const uint8_t SEQ[][2] = {
      {0xF3, 0x00}, {0x00, 0x7E}, {0xF3, 0x38}, {0x24, 0x64}, {0x25, 0x04}, {0x45, 0x03}, {0x60, 0x2A}, {0x61, 0xC9},
      {0x62, 0x4F}, {0x63, 0x06}, {0x6B, 0x00}, {0x6D, 0x16}, {0x6E, 0xAA}, {0x6F, 0x66}, {0x70, 0x99}, {0x23, 0x80},
      {0x72, 0x10}, {0x73, 0x10}, {0x10, 0xC4}, {0x01, 0x08}, {0xF1, 0x00}, {0x12, 0x01}, {0x13, 0x01}, {0x14, 0x01},
      {0x15, 0x01}, {0x16, 0x35}, {0x17, 0x09}, {0x18, 0x91}, {0x19, 0x28}, {0x1A, 0x01}, {0x1B, 0x01}, {0x1C, 0x11},
      {0x2A, 0x00}, {0x20, 0x60}, {0x40, 0x60}, {0x02, 0x00}, {0x04, 0x00}, {0x05, 0x10}, {0x06, 0x00}, {0x07, 0xC0},
      {0x08, 0x00}, {0x09, 0xC0}, {0x0A, 0x80}, {0x0B, 0x04}, {0x0C, 0x01}, {0x0D, 0x00}, {0x0F, 0x10}, {0x21, 0x1F},
      {0x22, 0x7F}, {0x2F, 0xC0}, {0x30, 0xF4}, {0x31, 0x00}, {0x44, 0x00}, {0x41, 0x7F}, {0x42, 0x7F}, {0x43, 0x10},
      {0x49, 0x0F}, {0x4C, 0xC0}, {0x00, 0x00}, {0x03, 0xC1}, {0x00, 0x01}, {0x4D, 0x00}, {0x26, 0xBF}, {0x27, 0xBF},
      {0x28, 0xBF}, {0x46, 0xBF}, {0x47, 0xBF}, {0x48, 0xBE}, {0xF0, 0x1A},
      // Encender (es8389_set_bias_on) y quitar el silencio del DAC.
      {0x4D, 0x00}, {0x61, 0xD9}, {0x64, 0x8F}, {0x10, 0xE4}, {0x00, 0x01}, {0x03, 0xC3}, {0x24, 0x6A}, {0x25, 0x0A},
  };
  uint8_t id = 0;
  if (!lcd.i2cReadReg(CODEC_ADDR, 0x00, id)) return false;  // no contesta: sin códec
  for (auto& r : SEQ)
    if (!lcd.i2cWrite(CODEC_ADDR, r, 2)) return false;
  uint8_t v = 0;
  if (lcd.i2cReadReg(CODEC_ADDR, 0x69, v)) {
    uint8_t d[2] = {0x69, (uint8_t)(v | 0x20)};
    lcd.i2cWrite(CODEC_ADDR, d, 2);
  }
  return true;
}

// ---------------------------------------------------------------- I2S
inline bool i2sInit() {
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  cc.auto_clear = true;  // sin datos, silencio
  if (i2s_new_channel(&cc, &tx, nullptr) != ESP_OK) return false;
  i2s_std_config_t sc = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(TONE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg = {.mclk = I2S_MCLK, .bclk = I2S_BCLK, .ws = I2S_WS, .dout = I2S_DOUT, .din = I2S_GPIO_UNUSED,
                   .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}},
  };
  if (i2s_channel_init_std_mode(tx, &sc) != ESP_OK || i2s_channel_enable(tx) != ESP_OK) return false;
  currentRate = TONE_RATE;
  return true;
}

inline void setRate(uint32_t rate) {
  if (rate == currentRate) return;
  i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(rate);
  i2s_channel_disable(tx);
  i2s_channel_reconfig_std_clock(tx, &clk);
  i2s_channel_enable(tx);
  currentRate = rate;
}

// Mono o estéreo de 16 bits → estéreo con el volumen elegido, y al altavoz.
inline void writeFrames(const int16_t* in, uint32_t frames, uint16_t channels) {
  static int16_t out[512 * 2];
  uint32_t v = volume;
  int32_t gain = (int32_t)(v * v * 256 / 10000);  // curva aproximada al oído (0–256)
  for (uint32_t i = 0; i < frames; i++) {
    int32_t l = in[i * channels], r = channels == 2 ? in[i * 2 + 1] : l;
    out[i * 2] = (int16_t)(l * gain / 256);
    out[i * 2 + 1] = (int16_t)(r * gain / 256);
  }
  size_t done = 0;
  i2s_channel_write(tx, out, frames * 4, &done, 1000);
}

inline bool playFile(const String& path, uint32_t gen) {
  File f = SD_MMC.open(path.c_str());
  if (!f) return false;
  static uint8_t head[1024];
  uint32_t n = (uint32_t)f.read(head, sizeof(head));
  s3snd::Wav w;
  if (!s3snd::parseWav(head, n, (uint32_t)f.size(), w)) {
    f.close();
    return false;
  }
  setRate(w.rate);
  f.seek(w.dataOffset);
  static int16_t buf[512 * 2];
  uint32_t frameBytes = w.channels * (w.bits / 8), left = w.dataSize;
  while (left >= frameBytes && gen == generation) {
    uint32_t frames = left / frameBytes;
    if (frames > 512) frames = 512;
    if (w.bits == 16) {
      frames = (uint32_t)f.read((uint8_t*)buf, frames * frameBytes) / frameBytes;
    } else {  // 8 bits sin signo → 16 bits
      uint8_t* raw = (uint8_t*)buf + sizeof(buf) / 2;
      frames = (uint32_t)f.read(raw, frames * frameBytes) / frameBytes;
      for (uint32_t i = 0; i < frames * w.channels; i++) buf[i] = (int16_t)(((int)raw[i] - 128) << 8);
    }
    if (!frames) break;
    writeFrames(buf, frames, w.channels);
    left -= frames * frameBytes;
  }
  f.close();
  return true;
}

inline void playTone(Ev e, uint32_t gen) {
  s3snd::Pattern p = s3snd::defaultPattern(e);
  setRate(TONE_RATE);
  uint32_t total = s3snd::patternLengthMs(p) * TONE_RATE / 1000;
  static int16_t buf[256];
  for (uint32_t at = 0; at < total && gen == generation; at += 256) {
    uint32_t n = total - at < 256 ? total - at : 256;
    s3snd::renderPattern(p, TONE_RATE, at, buf, n);
    writeFrames(buf, n, 1);
  }
}

inline void task(void*) {
  Req r;
  for (;;) {
    if (xQueueReceive(queue, &r, portMAX_DELAY) != pdTRUE) continue;
    if (r.gen != generation || r.ev >= s3snd::EV_COUNT || volume == 0) continue;
    const Folder& fo = folders[r.ev];
    if (fo.silent) continue;
    bool ok = false;
    if (fo.count) ok = playFile(fo.files[esp_random() % fo.count], r.gen);
    if (!ok) playTone((Ev)r.ev, r.gen);
  }
}

// Lee qué sonidos propios hay en la SD.
inline void scanFolders() {
  status.custom = 0;
  for (uint8_t i = 0; i < s3snd::EV_COUNT; i++) {
    Folder& fo = folders[i];
    fo.count = 0;
    fo.silent = false;
    String dir = String("/sonidos/") + s3snd::info((Ev)i).folder;
    File d = SD_MMC.open(dir.c_str());
    if (!d || !d.isDirectory()) continue;
    for (File f = d.openNextFile(); f; f = d.openNextFile()) {
      String name = f.name();
      String low = name;
      low.toLowerCase();
      if (low.startsWith("ninguno")) fo.silent = true;
      else if (!f.isDirectory() && low.endsWith(".wav") && !low.startsWith(".") && fo.count < MAX_FILES)
        fo.files[fo.count++] = dir + "/" + name;
      f.close();
    }
    d.close();
    if (fo.count || fo.silent) status.custom++;
  }
}

// Al arrancar, después de lcd.init(): SD, códec, I2S y la tarea que toca los sonidos.
inline void begin(LGFX& lcd) {
  if (SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0) && SD_MMC.begin("/sdcard", true)) {
    status.sd = true;
    status.sdMB = (uint32_t)(SD_MMC.cardSize() / (1024ULL * 1024ULL));
    scanFolders();
  }
  status.i2s = i2sInit();
  status.codec = status.i2s && codecInit(lcd);
  if (!status.codec) return;
  lcd.exioWrite(exio::SPEAKER_AMP, true);
  queue = xQueueCreate(8, sizeof(Req));
  xTaskCreatePinnedToCore(task, "sonido", 6144, nullptr, 2, nullptr, 0);
}

// Toca los momentos de un cambio: el primero ya (corta lo que suene) y los demás detrás.
inline void play(const Ev* list, uint8_t n) {
  if (!queue || !n) return;
  uint32_t g = generation + 1;
  generation = g;
  for (uint8_t i = 0; i < n; i++) {
    Req r = {(uint8_t)list[i], g};
    xQueueSend(queue, &r, 0);
  }
}

#else  // placas sin altavoz

inline void begin(LGFX&) {}
inline void play(const Ev*, uint8_t) {}

#endif

inline void play(Ev e) { play(&e, 1); }

}  // namespace s3audio

#endif  // MFV3_S3_AUDIO_H
