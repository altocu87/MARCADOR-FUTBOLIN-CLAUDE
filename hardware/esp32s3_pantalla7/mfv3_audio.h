// MARCADOR FUTBOLÍN V3 · altavoces, memoria del chip y tarjeta SD de la pantalla de 7"
// ----------------------------------------------------------------------------------
// Solo en la Waveshare ESP32-S3-Touch-LCD-7C, que trae de serie:
//   - Chip de sonido ES8389 (I2C 0x10, mismo bus que el táctil) y dos amplificadores NS4150B con su conector
//     de altavoz (PH 1,25 de 2 pines). Se enciende por la pata EXIO3 del expansor. Audio I2S: MCLK GPIO6,
//     BCLK GPIO44, LRCK GPIO16, DOUT GPIO15 (pines y registros del ejemplo oficial 09_SPEAKER_MICROPHONE).
//   - Ranura micro SD en modo SD_MMC de 1 bit: CLK GPIO12, CMD GPIO11, D0 GPIO13 (ejemplo oficial 05_SD).
//   - Memoria del chip: partición FAT de 19,9 MB (partitions.csv), con la biblioteca FFat. Queda para imágenes:
//     los sonidos van solo a la tarjeta SD (SOUNDS_ON_CHIP).
// Los sonidos se guardan como /sonidos/<código>.wav y suenan siempre a 16 kHz (se cambia la frecuencia al vuelo).
// Audio por bloques desde loop(), con 125 ms de DMA; cada sustitución descarta la pista anterior.

#ifndef MFV3_AUDIO_H
#define MFV3_AUDIO_H

#include "board_config.h"
#include "mfv3_sound.h"

#if MFV3_BOARD == BOARD_WAVESHARE_7C
#define HAS_AUDIO 1
#include <ctype.h>
#include <FS.h>
#include <FFat.h>
#include <SD_MMC.h>
#include <esp_vfs_fat.h>
#include <driver/i2s_std.h>
#else
#define HAS_AUDIO 0
#endif

#if HAS_AUDIO

namespace audio {

using mfv3snd::Place;

static const uint32_t RATE = 16000;
static const uint8_t CODEC_ADDR = 0x10;  // 0x20 en 8 bits, como lo escribe el fabricante
static const char* DIR = "/sonidos";
static const char* TMP = "/sonidos/subida.tmp";
static const bool SOUNDS_ON_CHIP = false;  // la memoria del chip se reserva para imágenes

// ---------------------------------------------------------------- almacén: tarjeta SD (y memoria del chip, FFat)
class FsStore : public mfv3snd::Store {
 public:
  bool chipOk = false, sdOk = false;

  void begin() {
    chipOk = FFat.begin(true);
    if (chipOk && SOUNDS_ON_CHIP && !FFat.exists(DIR)) FFat.mkdir(DIR);
    sdOk = SD_MMC.setPins(12, 11, 13) && SD_MMC.begin("/sdcard", true) && SD_MMC.cardType() != CARD_NONE;
    if (sdOk && !SD_MMC.exists(DIR)) SD_MMC.mkdir(DIR);
  }

  fs::FS& fsOf(Place p) { return p == Place::Sd ? (fs::FS&)SD_MMC : (fs::FS&)FFat; }

  static void path(uint32_t code, char* buf, size_t cap) { snprintf(buf, cap, "%s/%08lx.wav", DIR, (unsigned long)code); }

  static void bucketPath(uint32_t code,char* buf,size_t cap){snprintf(buf,cap,"%s/%02lx/%08lx.wav",DIR,(unsigned long)(code>>24),(unsigned long)code);}

  bool available(Place p) override { return p == Place::Sd ? sdOk : chipOk && SOUNDS_ON_CHIP; }

  bool sdSpace(uint64_t& total,uint64_t& free) {
    total=free=0;
    // Arduino 3.3.12 consulta "0:" en SD_MMC.totalBytes/usedBytes. Con FFat primero,
    // ese disco es la flash interna. Consultar explícitamente el volumen de la SD.
    if(!sdOk || esp_vfs_fat_info("/sdcard",&total,&free)!=ESP_OK || !total || free>total){total=free=0;return false;}
    return true;
  }

  uint32_t freeKB(Place p) override {
    if (!available(p)) return 0;
    if (p == Place::Chip) return (uint32_t)(FFat.freeBytes() / 1024);
    uint64_t total,free;
    return sdSpace(total,free) ? (uint32_t)(free/1024) : 0;
  }

  bool begin(Place p, uint32_t code) override {
    abort();
    place_ = p;
    code_ = code;
    char bucket[24];snprintf(bucket,sizeof bucket,"%s/%02lx",DIR,(unsigned long)(code>>24));
    if(!fsOf(p).exists(bucket) && !fsOf(p).mkdir(bucket))return false;
    file_ = fsOf(p).open(TMP, FILE_WRITE);
    return (bool)file_;
  }

  bool write(const uint8_t* d, size_t n) override { return file_ && file_.write(d, n) == n; }

  bool commit() override {
    if (!file_) return false;
    file_.close();
    remove(code_);  // un sonido solo vive en un sitio
    char p[40];
    bucketPath(code_, p, sizeof(p));
    return fsOf(place_).rename(TMP, p);
  }

  void abort() override {
    if (file_) {
      file_.close();
      fsOf(place_).remove(TMP);
    }
  }

  bool remove(uint32_t code) override {
    char p[40];
    path(code, p, sizeof(p));
    bool any = false;
    if (sdOk && SD_MMC.exists(p)) any = SD_MMC.remove(p) || any;
    if (available(Place::Chip) && FFat.exists(p)) any = FFat.remove(p) || any;
    bucketPath(code,p,sizeof(p));
    if(sdOk && SD_MMC.exists(p))any=SD_MMC.remove(p)||any;
    if(available(Place::Chip) && FFat.exists(p))any=FFat.remove(p)||any;
    return any;
  }

  bool find(uint32_t code, Place& where) override {
    char p[40];
    bucketPath(code,p,sizeof(p));
    if(sdOk && SD_MMC.exists(p)){where=Place::Sd;return true;}
    if(available(Place::Chip) && FFat.exists(p)){where=Place::Chip;return true;}
    path(code, p, sizeof(p));
    if (sdOk && SD_MMC.exists(p)) {
      where = Place::Sd;
      return true;
    }
    if (available(Place::Chip) && FFat.exists(p)) {
      where = Place::Chip;
      return true;
    }
    return false;
  }

  size_t list(uint32_t* codes, Place* places, size_t cap) override {
    size_t n = listIn(Place::Sd, codes, places, cap, 0);
    return listIn(Place::Chip, codes, places, cap, n);
  }

  fs::File open(uint32_t code) {
    Place where;
    if (!find(code, where)) return fs::File();
    char p[40];
    bucketPath(code,p,sizeof(p));
    if(fsOf(where).exists(p))return fsOf(where).open(p,FILE_READ);
    path(code,p,sizeof(p));return fsOf(where).open(p,FILE_READ);
  }

 private:
  size_t listIn(Place pl, uint32_t* codes, Place* places, size_t cap, size_t n) {
    if (!available(pl)) return n;
    return listDir(pl,DIR,codes,places,cap,n,0);
  }
  size_t listDir(Place pl,const char* folder,uint32_t* codes,Place* places,size_t cap,size_t n,unsigned depth){
    auto dir=fsOf(pl).open(folder);if(!dir || !dir.isDirectory())return n;
    for(auto f=dir.openNextFile();f && n<cap;f=dir.openNextFile()){
      const char* name=f.name();const char* slash=strrchr(name,'/');if(slash)name=slash+1;
      if(f.isDirectory()){
        if(depth==0 && strlen(name)==2 && isxdigit((unsigned char)name[0]) && isxdigit((unsigned char)name[1])){
          char child[32];snprintf(child,sizeof child,"%s/%s",DIR,name);n=listDir(pl,child,codes,places,cap,n,1);
        }
        continue;
      }
      uint32_t code;if(strlen(name)==12 && !strcmp(name+8,".wav") && mfv3snd::parseCode(name,8,code)){
        bool duplicate=false;for(size_t i=0;i<n;i++)if(codes[i]==code){duplicate=true;break;}
        if(!duplicate){codes[n]=code;places[n++]=pl;}
        if((n&15u)==0)delay(1);
      }
    }
    return n;
  }

  fs::File file_;
  Place place_ = Place::Sd;
  uint32_t code_ = 0;
};

// ---------------------------------------------------------------- chip de sonido ES8389 (solo salida)
static bool codecWrite(uint8_t reg, uint8_t val) {
  uint8_t d[2] = {reg, val};
  return !lgfx::i2c::transactionWrite(exio::I2C_PORT, CODEC_ADDR, d, 2).has_error();
}

static bool codecUpdate(uint8_t reg, uint8_t mask, uint8_t val) {
  uint8_t cur = 0;
  if (lgfx::i2c::transactionWriteRead(exio::I2C_PORT, CODEC_ADDR, &reg, 1, &cur, 1).has_error()) return false;
  return codecWrite(reg, (uint8_t)((cur & ~mask) | (val & mask)));
}

// Secuencia de es8389_open + set_fs + enable del controlador de Espressif (esp_codec_dev, Apache-2.0) para
// salida por los altavoces, reloj MCLK de la placa, I2S estándar de 16 bits y la referencia interna activada.
static bool codecInit() {
  static const uint8_t OPEN[][2] = {
      {0xF3, 0x00}, {0x00, 0x7E}, {0xF3, 0x38}, {0x24, 0x64}, {0x25, 0x04}, {0x45, 0x03}, {0x60, 0x2A}, {0x61, 0xC9},
      {0x62, 0x4F}, {0x63, 0x06}, {0x6B, 0x00}, {0x6D, 0x16}, {0x6E, 0xAA}, {0x6F, 0x66}, {0x70, 0x99}, {0x23, 0x00},
      {0x72, 0x10}, {0x73, 0x10}, {0x10, 0xC4}, {0x01, 0x08}, {0xF1, 0x00}, {0x12, 0x01}, {0x13, 0x01}, {0x14, 0x01},
      {0x15, 0x01}, {0x16, 0x35}, {0x17, 0x09}, {0x18, 0x91}, {0x19, 0x28}, {0x1A, 0x01}, {0x1B, 0x01}, {0x1C, 0x11},
      {0x2A, 0x00}, {0x20, 0x60}, {0x40, 0x60}, {0xF0, 0x01}, {0x02, 0x00}, {0x04, 0x00}, {0x05, 0x10}, {0x06, 0x00},
      {0x07, 0xC0}, {0x08, 0x00}, {0x09, 0xC0}, {0x0A, 0x80}, {0x0B, 0x04}, {0x0C, 0x01}, {0x0D, 0x00}, {0x0F, 0x10},
      {0x21, 0x1F}, {0x22, 0x7F}, {0x2F, 0xC0}, {0x30, 0xF4}, {0x31, 0x00}, {0x44, 0x00}, {0x41, 0x7F}, {0x42, 0x7F},
      {0x43, 0x10}, {0x49, 0x0F}, {0x4C, 0xC0}, {0x00, 0x00}, {0x03, 0xC1}, {0x00, 0x01}, {0x4D, 0x00}, {0x26, 0xBF},
      {0x27, 0xBF}, {0x28, 0xBF}, {0x46, 0xBF}, {0x47, 0xBF}, {0x48, 0xBE},
      // Esclavo, MCLK de la placa sin invertir, referencia interna (ADCL + DACR).
      {0x01, 0x08}, {0x02, 0x00}, {0x23, 0x80}, {0xF0, 0x1A},
      // enable → set_bias_on
      {0x4D, 0x00}};
  for (auto& r : OPEN)
    if (!codecWrite(r[0], r[1])) return false;
  codecUpdate(0x69, 0x20, 0x20);
  static const uint8_t ON[][2] = {{0x61, 0xD9}, {0x64, 0x8F}, {0x10, 0xE4}, {0x00, 0x01}, {0x03, 0xC3}, {0x24, 0x6A}, {0x25, 0x0A}};
  for (auto& r : ON)
    if (!codecWrite(r[0], r[1])) return false;
  return codecUpdate(0x20, 0x03, 0x00);  // sin silencio
}

// Volumen 0–100 % → registro del DAC (0,5 dB por paso; 191 = 0 dB). De −30 dB a 0 dB; 0 = silencio.
inline uint8_t volumeReg(uint8_t v) { return v == 0 ? 0 : (uint8_t)(191 - (100 - (v > 100 ? 100 : v)) * 60 / 100); }

// ---------------------------------------------------------------- reproductor
struct Item {
  bool tune;
  uint32_t code;
  mfv3snd::Tune notes;
};

class Player {
 public:
  bool ok = false;

  bool begin(LGFX& lcd, FsStore& store, uint8_t volume) {
    lcd_ = &lcd;
    store_ = &store;
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    cc.auto_clear = true;  // sin datos, silencio
    cc.dma_desc_num = 8;
    cc.dma_frame_num = 250;  // 8 × 250 tramas = 125 ms a 16 kHz (búferes en la memoria interna: el Wi-Fi la necesita)
    if (i2s_new_channel(&cc, &tx_, nullptr) != ESP_OK) return false;
    i2s_std_config_t sc;
    memset(&sc, 0, sizeof(sc));
    sc.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RATE);
    sc.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    sc.gpio_cfg.mclk = GPIO_NUM_6;
    sc.gpio_cfg.bclk = GPIO_NUM_44;
    sc.gpio_cfg.ws = GPIO_NUM_16;
    sc.gpio_cfg.dout = GPIO_NUM_15;
    sc.gpio_cfg.din = I2S_GPIO_UNUSED;
    if (i2s_channel_init_std_mode(tx_, &sc) != ESP_OK || i2s_channel_enable(tx_) != ESP_OK) return false;
    txRunning_ = true;
    ok = codecInit();
    setVolume(volume);
    return ok;
  }

  void setVolume(uint8_t v) {
    volume_ = v;
    if (!ok) return;
    codecWrite(0x46, volumeReg(v));
    codecWrite(0x47, volumeReg(v));
  }
  uint8_t volume() const { return volume_; }

  bool busy() const { return playing_ || qn_; }

  // `queue`: espera a que termine lo que suena; si no, lo corta.
  void play(const Item& it, bool queue) {
    if (!ok || volume_ == 0) return;
    if (!queue) {
      stop();
      if(!ok)return;
      start(it);
    } else if (!playing_) start(it);
    else if (qn_ < 3) q_[qn_++] = it;
  }

  void stop() {
    if (file_) file_.close();
    playing_ = false;
    pendingFrames_ = pendingAt_ = 0;
    qn_ = 0;
    // Parar DMA evita que continúe sonando la frase anulada. La siguiente pista
    // rellena TODOS los descriptores antes de habilitarla, sin muestras antiguas.
    if (txRunning_ && tx_) {
      if(i2s_channel_disable(tx_)!=ESP_OK){ok=false;amp(false);}
      txRunning_=false;
    }
  }

  // En cada vuelta de loop(): rellena el colchón de audio con lo que quepa.
  void pump(uint32_t now) {
    if (!ok) return;
    if (playing_ || pendingFrames_) quietSince_ = now;
    else if (ampOn_ && now - quietSince_ > 800) amp(false);  // amplificadores apagados en reposo (sin soplido)
    if(!txRunning_ && playing_){
      // 8 descriptores de 250 tramas. Leer como máximo 4 KB de PCM mono
      // antes del arranque; se reutilizan buffers existentes, sin heap por gol.
      for(unsigned d=0;d<8;d++){
        size_t n=playing_?fill(mono_,250):0;
        if(n<250)playing_=false;
        for(size_t i=0;i<250;i++)stereo_[2*i]=stereo_[2*i+1]=i<n?mono_[i]:0;
        size_t loaded=0;
        if(i2s_channel_preload_data(tx_,stereo_,1000,&loaded)!=ESP_OK || loaded!=1000){ok=false;amp(false);return;}
      }
      if(i2s_channel_enable(tx_)!=ESP_OK){ok=false;amp(false);return;}
      txRunning_=true;quietSince_=now;
    }
    for (int guard = 0; guard < 16; guard++) {
      if (pendingAt_ >= pendingFrames_) {
        pendingFrames_ = pendingAt_ = 0;
        if (!playing_ && !next()) return;
        size_t n = fill(mono_, BLOCK);
        if (!n) {
          playing_ = false;
          continue;
        }
        for (size_t i = 0; i < n; i++) stereo_[2 * i] = stereo_[2 * i + 1] = mono_[i];
        pendingFrames_ = n;
      }
      size_t bytes = (pendingFrames_ - pendingAt_) * 4, written = 0;
      i2s_channel_write(tx_, stereo_ + pendingAt_ * 2, bytes, &written, 0);
      pendingAt_ += written / 4;
      if (written < bytes) return;  // colchón lleno: el resto en la próxima vuelta
    }
  }

 private:
  static const size_t BLOCK = 256;

  void amp(bool on) {
    ampOn_ = on;
    lcd_->exioWrite(exio::SPEAKER_AMP, on);
  }

  bool next() {
    if (!qn_) return false;
    Item it = q_[0];
    for (uint8_t i = 1; i < qn_; i++) q_[i - 1] = q_[i];
    qn_--;
    start(it);
    return playing_;
  }

  void start(const Item& it) {
    playing_ = false;
    if (!ampOn_) amp(true);
    if (it.tune) {
      tone_.start(it.notes, RATE);
      fromFile_ = false;
      playing_ = true;
      return;
    }
    if (!store_) return;
    file_ = store_->open(it.code);
    if (!file_) return;
    uint8_t head[256];
    size_t got = file_.read(head, sizeof(head));
    if (!mfv3snd::parseWav(head, got, wav_) || !file_.seek(wav_.dataOffset)) {
      file_.close();
      return;
    }
    left_ = wav_.dataBytes;
    rs_.begin(wav_.rate, RATE);
    inAt_ = inLen_ = 0;
    fromFile_ = true;
    playing_ = true;
  }

  // Muestras de una pista a 16 kHz del sonido en curso.
  size_t fill(int16_t* out, size_t cap) {
    if (!fromFile_) return tone_.render(out, cap);
    size_t w = 0;
    while (w < cap) {
      if (inAt_ >= inLen_) {
        if (!left_ || !file_) break;
        uint8_t raw[BLOCK * 4];
        size_t want = left_ < sizeof(raw) ? left_ : sizeof(raw);
        size_t got = file_.read(raw, want);
        if (!got) break;
        left_ -= (uint32_t)got;
        size_t frames = got / (2 * wav_.channels);
        for (size_t i = 0; i < frames; i++) {
          const uint8_t* s = raw + i * 2 * wav_.channels;
          int32_t v = (int16_t)(s[0] | (s[1] << 8));
          if (wav_.channels == 2) v = (v + (int16_t)(s[2] | (s[3] << 8))) / 2;
          in_[i] = (int16_t)v;
        }
        inAt_ = 0;
        inLen_ = frames;
      }
      size_t used = 0;
      w += rs_.process(in_ + inAt_, inLen_ - inAt_, out + w, cap - w, used);
      inAt_ += used;
      if (!used && inAt_ < inLen_) break;
    }
    if (!w && file_) file_.close();
    return w;
  }

  LGFX* lcd_ = nullptr;
  FsStore* store_ = nullptr;
  i2s_chan_handle_t tx_ = nullptr;
  uint8_t volume_ = 80;
  bool playing_ = false, fromFile_ = false, ampOn_ = false, txRunning_ = false;
  uint32_t quietSince_ = 0;
  Item q_[3];
  uint8_t qn_ = 0;
  mfv3snd::ToneGen tone_;
  mfv3snd::Resampler rs_;
  mfv3snd::WavInfo wav_;
  fs::File file_;
  uint32_t left_ = 0;
  int16_t in_[BLOCK * 2];
  size_t inAt_ = 0, inLen_ = 0;
  int16_t mono_[BLOCK];
  int16_t stereo_[BLOCK * 2];
  size_t pendingFrames_ = 0, pendingAt_ = 0;
};

}  // namespace audio

#endif  // HAS_AUDIO
#endif  // MFV3_AUDIO_H
