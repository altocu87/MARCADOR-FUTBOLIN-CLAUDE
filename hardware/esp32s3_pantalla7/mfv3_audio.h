// MARCADOR FUTBOLÍN V3 · altavoces, memoria del chip y tarjeta SD de la pantalla de 7"
// ----------------------------------------------------------------------------------
// Solo en la Waveshare ESP32-S3-Touch-LCD-7C, que trae de serie:
//   - Chip de sonido ES8389 (I2C 0x10, mismo bus que el táctil) y dos amplificadores NS4150B con su conector
//     de altavoz (PH 1,25 de 2 pines). Se enciende por la pata EXIO3 del expansor. Audio I2S: MCLK GPIO6,
//     BCLK GPIO44, LRCK GPIO16, DOUT GPIO15 (pines y registros del ejemplo oficial 09_SPEAKER_MICROPHONE).
//   - Ranura micro SD en modo SD_MMC de 1 bit: CLK GPIO12, CMD GPIO11, D0 GPIO13 (ejemplo oficial 05_SD).
//   - Memoria del chip: partición FAT de 19,9 MB (partitions.csv), con la biblioteca FFat. Queda para imágenes:
//     los sonidos van solo a la tarjeta SD (SOUNDS_ON_CHIP).
// Sonidos en rutas planas heredadas o /sonidos/ab/<código>.wav; salida siempre a 16 kHz.
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
// Valida la pista completa declarada por la cabecera, sin leer el PCM entero.
inline bool validWavFile(fs::File& f,mfv3snd::WavInfo& w){
  if(!f || f.isDirectory())return false;
  uint8_t h[256];size_t n=f.read(h,sizeof h);
  if(!mfv3snd::parseWav(h,n,w) || !w.dataBytes || !w.channels)return false;
  uint64_t riff=8ull+(uint32_t)(h[4]|((uint32_t)h[5]<<8)|((uint32_t)h[6]<<16)|((uint32_t)h[7]<<24));
  uint64_t end=(uint64_t)w.dataOffset+w.dataBytes;
  return riff<=f.size() && end<=riff && end<=f.size() && !(w.dataBytes%(2u*w.channels)) && f.seek(w.dataOffset);
}

class FsStore : public mfv3snd::Store {
 public:
  bool chipOk=false,sdOk=false;
  void begin(){
    chipOk=FFat.begin(false); // un fallo de montaje nunca autoriza formatear fotos/datos
    if(chipOk && SOUNDS_ON_CHIP && !FFat.exists(DIR))FFat.mkdir(DIR);
    sdOk=SD_MMC.setPins(12,11,13) && SD_MMC.begin("/sdcard",true) && SD_MMC.cardType()!=CARD_NONE;
    if(sdOk && !SD_MMC.exists(DIR))SD_MMC.mkdir(DIR);
    recoveryOk_=true;
    if(sdOk && !recover(Place::Sd))recoveryOk_=false;
    if(available(Place::Chip) && !recover(Place::Chip))recoveryOk_=false;
  }
  fs::FS& fsOf(Place p){return p==Place::Sd?(fs::FS&)SD_MMC:(fs::FS&)FFat;}
  static void path(uint32_t code,char* buf,size_t cap){snprintf(buf,cap,"%s/%08lx.wav",DIR,(unsigned long)code);}
  static void bucketPath(uint32_t code,char* buf,size_t cap){snprintf(buf,cap,"%s/%02lx/%08lx.wav",DIR,(unsigned long)(code>>24),(unsigned long)code);}
  bool available(Place p) override{return p==Place::Sd?sdOk:chipOk && SOUNDS_ON_CHIP;}
  bool recoveryOk() const{return recoveryOk_;}
  bool sdSpace(uint64_t& total,uint64_t& free){
    total=free=0;
    if(!sdOk || esp_vfs_fat_info("/sdcard",&total,&free)!=ESP_OK || !total || free>total){total=free=0;return false;}
    return true;
  }
  uint32_t freeKB(Place p) override{
    if(!available(p))return 0;
    if(p==Place::Chip)return (uint32_t)(FFat.freeBytes()/1024);
    uint64_t total,free;return sdSpace(total,free)?(uint32_t)(free/1024):0;
  }
  bool begin(Place p,uint32_t code) override{
    abort();
    if(!available(p) || !code)return false;
    if(!recover(p)){recoveryOk_=false;return false;}
    recoveryOk_=true;place_=p;code_=code;received_=receivedCrc_=0;
    char bucket[24];snprintf(bucket,sizeof bucket,"%s/%02lx",DIR,(unsigned long)(code>>24));
    if(!fsOf(p).exists(bucket) && !fsOf(p).mkdir(bucket))return false;
    file_=fsOf(p).open(TMP,FILE_WRITE);return (bool)file_;
  }
  bool write(const uint8_t* d,size_t n) override{
    if(!file_ || n>mfv3snd::MAX_SOUND_BYTES-received_ || file_.write(d,n)!=n)return false;
    receivedCrc_=mfv3snd::crc32Update(receivedCrc_,d,n);received_+=(uint32_t)n;return true;
  }
  bool commit() override{
    if(!file_)return false;
    file_.close();auto& fs=fsOf(place_);
    if(!matches(fs,TMP,received_,receivedCrc_)){eraseIfPresent(fs,TMP);return false;}
    char dest[40],old[40];bucketPath(code_,dest,sizeof dest);path(code_,old,sizeof old);
    uint8_t kind=fs.exists(dest)?2:fs.exists(old)?1:0;
    if(kind==2)snprintf(old,sizeof old,"%s",dest);
    // El marcador se verifica antes de mover el anterior. Siempre hay una copia recuperable.
    if(!writeTxn(fs,code_,kind,received_,receivedCrc_))return false;
    if(kind && !fs.rename(old,BAK)){
      eraseIfPresent(fs,TXN);eraseIfPresent(fs,TMP);return false;
    }
    if(!fs.rename(TMP,dest)){
      bool restored=!kind || fs.rename(BAK,old);
      if(restored){eraseIfPresent(fs,TXN);eraseIfPresent(fs,TMP);}
      else recoveryOk_=false;
      return false;
    }
    // Confirmar la nueva cabecera/tamaño antes de retirar respaldo o ruta plana.
    if(!matches(fs,dest,received_,receivedCrc_)){
      if(kind && eraseIfPresent(fs,dest) && fs.rename(BAK,old)){
        eraseIfPresent(fs,TXN);eraseIfPresent(fs,TMP);
      }else recoveryOk_=false;
      return false;
    }
    // CRC completo confirmado: ahora se puede retirar el respaldo. Un fallo de
    // limpieza conserva el marcador para reintentarlo fuera del juego.
    recoveryOk_=cleanupConfirmed(fs,code_);return recoveryOk_;
  }
  void abort() override{
    if(file_){file_.close();fsOf(place_).remove(TMP);}
  }
  bool remove(uint32_t code) override{
    char p[40];path(code,p,sizeof p);bool any=false;
    if(sdOk && SD_MMC.exists(p))any=SD_MMC.remove(p)||any;
    if(available(Place::Chip) && FFat.exists(p))any=FFat.remove(p)||any;
    bucketPath(code,p,sizeof p);
    if(sdOk && SD_MMC.exists(p))any=SD_MMC.remove(p)||any;
    if(available(Place::Chip) && FFat.exists(p))any=FFat.remove(p)||any;
    return any;
  }
  bool find(uint32_t code,Place& where) override{
    char p[40];bucketPath(code,p,sizeof p);
    if(sdOk && SD_MMC.exists(p)){where=Place::Sd;return true;}
    if(available(Place::Chip) && FFat.exists(p)){where=Place::Chip;return true;}
    path(code,p,sizeof p);
    if(sdOk && SD_MMC.exists(p)){where=Place::Sd;return true;}
    if(available(Place::Chip) && FFat.exists(p)){where=Place::Chip;return true;}
    return false;
  }
  fs::File open(uint32_t code){
    // Una recuperación ambigua conserva las copias, pero ninguna ruta se ofrece
    // a reproducción o inspección como si fuera una pista confirmada.
    if(!recoveryOk_)return fs::File();
    char p[40];bucketPath(code,p,sizeof p);
    if(sdOk){auto f=SD_MMC.open(p,FILE_READ);if(f && !f.isDirectory())return f;}
    if(available(Place::Chip)){auto f=FFat.open(p,FILE_READ);if(f && !f.isDirectory())return f;}
    path(code,p,sizeof p);
    if(sdOk){auto f=SD_MMC.open(p,FILE_READ);if(f && !f.isDirectory())return f;}
    if(available(Place::Chip)){auto f=FFat.open(p,FILE_READ);if(f && !f.isDirectory())return f;}
    return fs::File();
  }
  size_t list(uint32_t* codes,Place* places,size_t cap) override{
    size_t n=listIn(Place::Sd,codes,places,cap,0);return listIn(Place::Chip,codes,places,cap,n);
  }
 private:
  static constexpr const char* TXN="/sonidos/subida.txn";
  static constexpr const char* BAK="/sonidos/subida.bak";
  static bool eraseIfPresent(fs::FS& fs,const char* p){return !fs.exists(p) || fs.remove(p);}
  static bool validAt(fs::FS& fs,const char* p){auto f=fs.open(p,FILE_READ);mfv3snd::WavInfo w;return validWavFile(f,w);}
  static bool matches(fs::FS& fs,const char* path,uint32_t bytes,uint32_t crc){
    auto f=fs.open(path,FILE_READ);mfv3snd::WavInfo w;
    if(bytes<44 || bytes>mfv3snd::MAX_SOUND_BYTES || !validWavFile(f,w) || f.size()!=bytes || !f.seek(0))return false;
    uint8_t block[1024];uint32_t count=0,actual=0;
    while(count<bytes){
      size_t want=bytes-count<sizeof block?bytes-count:sizeof block;
      size_t got=f.read(block,want);if(!got || got>want)return false;
      actual=mfv3snd::crc32Update(actual,block,got);count+=(uint32_t)got;
      if(!(count&16383u))delay(1);
    }
    return actual==crc;
  }
  static bool cleanupConfirmed(fs::FS& fs,uint32_t code){
    char flat[40];path(code,flat,sizeof flat);
    if(!eraseIfPresent(fs,flat) || !eraseIfPresent(fs,TMP) || !eraseIfPresent(fs,BAK))return false;
    return eraseIfPresent(fs,TXN);
  }
  static bool writeTxn(fs::FS& fs,uint32_t code,uint8_t kind,uint32_t bytes,uint32_t audioCrc){
    uint8_t b[24]={'W','A','V','2',0,0,0,0,kind,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    for(unsigned i=0;i<4;i++){
      b[4+i]=(uint8_t)(code>>(i*8));b[12+i]=(uint8_t)(bytes>>(i*8));b[16+i]=(uint8_t)(audioCrc>>(i*8));
    }
    uint32_t crc=mfv3snd::crc32Update(0,b,20);for(unsigned i=0;i<4;i++)b[20+i]=(uint8_t)(crc>>(i*8));
    auto f=fs.open(TXN,FILE_WRITE);if(!f)return false;bool ok=f.write(b,sizeof b)==sizeof b;f.close();
    uint32_t checkCode,checkBytes,checkCrc;uint8_t checkKind;
    return ok && readTxn(fs,checkCode,checkKind,checkBytes,checkCrc) && checkCode==code && checkKind==kind && checkBytes==bytes && checkCrc==audioCrc;
  }
  static bool readTxn(fs::FS& fs,uint32_t& code,uint8_t& kind,uint32_t& bytes,uint32_t& audioCrc){
    auto f=fs.open(TXN,FILE_READ);uint8_t b[24];
    if(!f || f.size()!=24 || f.read(b,24)!=24 || memcmp(b,"WAV2",4))return false;
    code=bytes=audioCrc=0;uint32_t crc=0;
    for(unsigned i=0;i<4;i++){
      code|=(uint32_t)b[4+i]<<(i*8);bytes|=(uint32_t)b[12+i]<<(i*8);
      audioCrc|=(uint32_t)b[16+i]<<(i*8);crc|=(uint32_t)b[20+i]<<(i*8);
    }
    kind=b[8];
    return code && kind<=2 && !b[9] && !b[10] && !b[11] && bytes>=44 && bytes<=mfv3snd::MAX_SOUND_BYTES && crc==mfv3snd::crc32Update(0,b,20);
  }
  bool recover(Place place){
    auto& fs=fsOf(place);
    if(!fs.exists(TXN)){
      // No se borra un respaldo huérfano cuyo código no se puede determinar.
      if(fs.exists(BAK))return false;
      return eraseIfPresent(fs,TMP);
    }
    uint32_t code,bytes,crc;uint8_t kind;if(!readTxn(fs,code,kind,bytes,crc))return false;
    char dest[40],old[40];bucketPath(code,dest,sizeof dest);
    if(kind==2)snprintf(old,sizeof old,"%s",dest);else path(code,old,sizeof old);
    if(matches(fs,dest,bytes,crc))return cleanupConfirmed(fs,code);
    if(kind && validAt(fs,BAK)){
      if(!eraseIfPresent(fs,dest) || !fs.rename(BAK,old))return false;
      if(!eraseIfPresent(fs,TMP))return false;
      return eraseIfPresent(fs,TXN);
    }
    // Corte antes de mover el fichero antiguo: conservarlo si aún es válido.
    if(kind && validAt(fs,old) && !fs.exists(BAK)){
      if(!eraseIfPresent(fs,TMP))return false;
      return eraseIfPresent(fs,TXN);
    }
    // Primer archivo, sin anterior: descartar únicamente su temporal interrumpido.
    if(!kind && !fs.exists(BAK) && !fs.exists(dest)){
      if(!eraseIfPresent(fs,TMP))return false;
      return eraseIfPresent(fs,TXN);
    }
    return false; // ambiguo/corrupto: guardar las copias para diagnóstico, no adivinar
  }
  size_t listIn(Place pl,uint32_t* codes,Place* places,size_t cap,size_t n){
    return available(pl)?listDir(pl,DIR,codes,places,cap,n,0):n;
  }
  size_t listDir(Place pl,const char* folder,uint32_t* codes,Place* places,size_t cap,size_t n,unsigned depth){
    auto dir=fsOf(pl).open(folder);if(!dir || !dir.isDirectory())return n;
    for(auto f=dir.openNextFile();f && n<cap;f=dir.openNextFile()){
      const char* name=f.name();const char* slash=strrchr(name,'/');if(slash)name=slash+1;
      if(f.isDirectory()){
        if(depth==0 && strlen(name)==2 && isxdigit((unsigned char)name[0]) && isxdigit((unsigned char)name[1])){
          char child[32];snprintf(child,sizeof child,"%s/%s",DIR,name);n=listDir(pl,child,codes,places,cap,n,1);
        }continue;
      }
      uint32_t code;if(strlen(name)==12 && !strcmp(name+8,".wav") && mfv3snd::parseCode(name,8,code)){
        bool duplicate=false;for(size_t i=0;i<n;i++)if(codes[i]==code){duplicate=true;break;}
        if(!duplicate){codes[n]=code;places[n++]=pl;}if((n&15u)==0)delay(1);
      }
    }return n;
  }
  fs::File file_;
  Place place_=Place::Sd;
  uint32_t code_=0,received_=0,receivedCrc_=0;
  bool recoveryOk_=true;
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
  enum class State : uint8_t { Idle,Preparing,Playing,Error };
  enum class Failure : uint8_t { None,Muted,NoStore,NotFound,BadWav,ReadError,CodecInit,CodecWrite,I2SInit,I2SDisable,I2SPreload,I2SEnable,I2SWrite,QueueFull };
  bool ok=false;
  State state() const{return state_;}
  Failure failure() const{return failure_;}
  esp_err_t lastError() const{return lastError_;}
  uint32_t errorCount() const{return errorCount_;}
  uint32_t writeTimeoutCount() const{return writeTimeoutCount_;}
  bool lastStartSucceeded() const{return lastStartSucceeded_;}
  bool requestAccepted() const{return requestAccepted_;}
  const char* statusName() const{
    switch(state_){case State::Preparing:return "preparing";case State::Playing:return "playing";case State::Error:return "error";default:return "idle";}
  }
  const char* failureName() const{
    switch(failure_){
      case Failure::Muted:return "muted";case Failure::NoStore:return "no_store";
      case Failure::NotFound:return "not_found";case Failure::BadWav:return "bad_wav";
      case Failure::ReadError:return "read_error";case Failure::CodecInit:return "codec_init";
      case Failure::CodecWrite:return "codec_write";case Failure::I2SInit:return "i2s_init";
      case Failure::I2SDisable:return "i2s_disable";case Failure::I2SPreload:return "i2s_preload";
      case Failure::I2SEnable:return "i2s_enable";case Failure::I2SWrite:return "i2s_write";
      case Failure::QueueFull:return "queue_full";default:return "none";
    }
  }
  bool begin(LGFX& lcd,FsStore& store,uint8_t volume){
    lcd_=&lcd;store_=&store;
    if(tx_)return false; // no reinicializar un canal vivo desde el partido
    i2s_chan_config_t cc=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1,I2S_ROLE_MASTER);
    cc.auto_clear=true;cc.dma_desc_num=8;cc.dma_frame_num=250;
    esp_err_t err=i2s_new_channel(&cc,&tx_,nullptr);
    if(err!=ESP_OK){fail(Failure::I2SInit,err,true);return false;}
    i2s_std_config_t sc;memset(&sc,0,sizeof sc);
    sc.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(RATE);
    sc.slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
    sc.gpio_cfg.mclk=GPIO_NUM_6;sc.gpio_cfg.bclk=GPIO_NUM_44;sc.gpio_cfg.ws=GPIO_NUM_16;
    sc.gpio_cfg.dout=GPIO_NUM_15;sc.gpio_cfg.din=I2S_GPIO_UNUSED;
    err=i2s_channel_init_std_mode(tx_,&sc);
    if(err==ESP_OK)err=i2s_channel_enable(tx_);
    if(err!=ESP_OK){fail(Failure::I2SInit,err,true);releaseFailedInit();return false;}
    txRunning_=true;
    if(!codecInit()){fail(Failure::CodecInit,ESP_FAIL,true);releaseFailedInit();return false;}
    ok=true;state_=State::Idle;setVolume(volume);
    if(!ok)releaseFailedInit();
    return ok;
  }
  void setVolume(uint8_t v){
    volume_=v>100?100:v;
    if(!ok)return;
    if(!codecWrite(0x46,volumeReg(volume_)) || !codecWrite(0x47,volumeReg(volume_)))fail(Failure::CodecWrite,ESP_FAIL,true);
  }
  uint8_t volume() const{return volume_;}
  // Estado del productor, NO confirmación de que el último descriptor DMA ya sonó.
  bool busy() const{return playing_ || qn_ || pendingFrames_>pendingAt_;}
  void play(const Item& it,bool queue){
    requestAccepted_=lastStartSucceeded_=false;
    if(!ok)return;
    if(!volume_){failure_=Failure::Muted;lastError_=ESP_OK;return;}
    if(!queue){stop();if(ok)start(it);}
    else if(!playing_)start(it);
    else if(qn_<3){q_[qn_++]=it;requestAccepted_=true;}
    else{failure_=Failure::QueueFull;lastError_=ESP_FAIL;errorCount_++;}
  }
  void stop(){
    if(file_)file_.close();
    playing_=false;pendingFrames_=pendingAt_=0;qn_=0;
    requestAccepted_=lastStartSucceeded_=false;
    if(txRunning_ && tx_){
      esp_err_t err=i2s_channel_disable(tx_);txRunning_=false;
      if(err!=ESP_OK){fail(Failure::I2SDisable,err,true);return;}
    }
    state_=ok?State::Idle:State::Error;
  }
  void pump(uint32_t now){
    if(!ok)return;
    if(playing_ || pendingFrames_)quietSince_=now;
    else if(ampOn_ && now-quietSince_>800)amp(false);
    if(!txRunning_ && playing_){
      // 8×250 tramas: conservar 125 ms de DMA y reutilizar los buffers DSP.
      for(unsigned d=0;d<8;d++){
        size_t n=playing_?fill(mono_,250):0;
        if(state_==State::Error)return;
        if(n<250)playing_=false;
        for(size_t i=0;i<250;i++)stereo_[2*i]=stereo_[2*i+1]=i<n?mono_[i]:0;
        size_t loaded=0;esp_err_t err=i2s_channel_preload_data(tx_,stereo_,1000,&loaded);
        if(err!=ESP_OK || loaded!=1000){fail(Failure::I2SPreload,err==ESP_OK?ESP_FAIL:err,true);return;}
      }
      esp_err_t err=i2s_channel_enable(tx_);
      if(err!=ESP_OK){fail(Failure::I2SEnable,err,true);return;}
      txRunning_=true;lastStartSucceeded_=true;state_=State::Playing;quietSince_=now;
    }
    for(int guard=0;guard<16;guard++){
      if(pendingAt_>=pendingFrames_){
        pendingFrames_=pendingAt_=0;
        if(!playing_ && !next()){
          // Puede seguir quedando audio en DMA aunque el productor esté libre.
          if(state_!=State::Error)state_=State::Idle;
          return;
        }
        size_t n=fill(mono_,BLOCK);if(state_==State::Error)return;
        if(!n){playing_=false;continue;}
        for(size_t i=0;i<n;i++)stereo_[2*i]=stereo_[2*i+1]=mono_[i];
        pendingFrames_=n;
      }
      size_t bytes=(pendingFrames_-pendingAt_)*4,written=0;
      esp_err_t err=i2s_channel_write(tx_,stereo_+pendingAt_*2,bytes,&written,0);
      if(err!=ESP_OK && err!=ESP_ERR_TIMEOUT){fail(Failure::I2SWrite,err,true);return;}
      if(written>bytes || written%4){fail(Failure::I2SWrite,ESP_FAIL,true);return;}
      pendingAt_+=written/4;
      if(written){lastStartSucceeded_=true;state_=State::Playing;}
      if(err==ESP_ERR_TIMEOUT)writeTimeoutCount_++;
      if(written<bytes)return; // timeout/colchón lleno no equivale a fallo permanente
    }
  }
 private:
  static const size_t BLOCK=256;
  void amp(bool on){ampOn_=on;if(lcd_)lcd_->exioWrite(exio::SPEAKER_AMP,on);}
  void fail(Failure why,esp_err_t error,bool hardware){
    if(file_)file_.close();
    playing_=false;qn_=0;pendingFrames_=pendingAt_=0;
    if(txRunning_ && tx_){
      esp_err_t stopped=i2s_channel_disable(tx_);txRunning_=false;
      if(stopped!=ESP_OK){why=Failure::I2SDisable;error=stopped;hardware=true;}
    }
    failure_=why;lastError_=error;errorCount_++;state_=State::Error;
    lastStartSucceeded_=requestAccepted_=false;
    if(hardware)ok=false;
    amp(false);
  }
  void releaseFailedInit(){
    if(tx_ && i2s_del_channel(tx_)==ESP_OK)tx_=nullptr;
  }
  bool next(){
    if(!qn_)return false;
    Item it=q_[0];for(uint8_t i=1;i<qn_;i++)q_[i-1]=q_[i];qn_--;
    start(it);return playing_;
  }
  void start(const Item& it){
    playing_=false;requestAccepted_=lastStartSucceeded_=false;
    failure_=Failure::None;lastError_=ESP_OK;state_=State::Preparing;
    if(it.tune){tone_.start(it.notes,RATE);fromFile_=false;playing_=true;requestAccepted_=true;amp(true);return;}
    if(!store_){fail(Failure::NoStore,ESP_FAIL,false);return;}
    file_=store_->open(it.code);
    if(!file_){fail(Failure::NotFound,ESP_FAIL,false);return;}
    if(!validWavFile(file_,wav_)){fail(Failure::BadWav,ESP_FAIL,false);return;}
    left_=wav_.dataBytes;rs_.begin(wav_.rate,RATE);inAt_=inLen_=0;
    fromFile_=true;playing_=requestAccepted_=true;amp(true);
  }
  size_t fill(int16_t* out,size_t cap){
    if(!fromFile_)return tone_.render(out,cap);
    size_t w=0;
    while(w<cap){
      if(inAt_>=inLen_){
        if(!left_ || !file_)break;
        uint8_t raw[BLOCK*4];size_t want=left_<sizeof raw?left_:sizeof raw;
        size_t got=file_.read(raw,want);
        if(!got || got>want || got%(2u*wav_.channels)){fail(Failure::ReadError,ESP_FAIL,false);return 0;}
        left_-=(uint32_t)got;size_t frames=got/(2u*wav_.channels);
        for(size_t i=0;i<frames;i++){
          const uint8_t* s=raw+i*2*wav_.channels;int32_t v=(int16_t)(s[0]|(s[1]<<8));
          if(wav_.channels==2)v=(v+(int16_t)(s[2]|(s[3]<<8)))/2;
          in_[i]=(int16_t)v;
        }
        inAt_=0;inLen_=frames;
      }
      size_t used=0;w+=rs_.process(in_+inAt_,inLen_-inAt_,out+w,cap-w,used);inAt_+=used;
      if(!used && inAt_<inLen_)break;
    }
    if(!w && file_)file_.close();
    return w;
  }
  LGFX* lcd_=nullptr;FsStore* store_=nullptr;i2s_chan_handle_t tx_=nullptr;
  uint8_t volume_=80;
  bool playing_=false,fromFile_=false,ampOn_=false,txRunning_=false;
  bool lastStartSucceeded_=false,requestAccepted_=false;
  State state_=State::Idle;Failure failure_=Failure::None;
  esp_err_t lastError_=ESP_OK;uint32_t errorCount_=0,writeTimeoutCount_=0,quietSince_=0;
  Item q_[3];uint8_t qn_=0;
  mfv3snd::ToneGen tone_;mfv3snd::Resampler rs_;mfv3snd::WavInfo wav_;
  fs::File file_;uint32_t left_=0;
  int16_t in_[BLOCK*2],mono_[BLOCK],stereo_[BLOCK*2];
  size_t inAt_=0,inLen_=0,pendingFrames_=0,pendingAt_=0;
};

} // namespace audio
#endif // HAS_AUDIO
#endif // MFV3_AUDIO_H
