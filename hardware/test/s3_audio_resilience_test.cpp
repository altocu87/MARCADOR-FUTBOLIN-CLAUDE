// Prueba aislada de almacenamiento/reproductor: no necesita .ino, LVGL ni placa.
#include <cassert>
#include <cstdio>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/mfv3_audio.h"
static std::vector<uint8_t> wav(int16_t value,size_t frames=6000){
  std::vector<uint8_t> v(44+frames*2);
  auto u32=[&](unsigned at,uint32_t n){for(unsigned i=0;i<4;i++)v[at+i]=(uint8_t)(n>>(8*i));};
  memcpy(v.data(),"RIFF",4);u32(4,(uint32_t)v.size()-8);memcpy(v.data()+8,"WAVEfmt ",8);
  u32(16,16);u32(20,1|(1u<<16));u32(24,16000);u32(28,32000);u32(32,2|(16u<<16));
  memcpy(v.data()+36,"data",4);u32(40,(uint32_t)frames*2);
  for(size_t i=44;i<v.size();i+=2){v[i]=(uint8_t)value;v[i+1]=(uint8_t)(value>>8);}return v;
}
static std::vector<uint8_t> txn(uint32_t code,uint8_t oldKind,const std::vector<uint8_t>& data){
  std::vector<uint8_t> b={'W','A','V','2',0,0,0,0,oldKind,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  uint32_t bytes=(uint32_t)data.size(),audioCrc=mfv3snd::crc32Update(0,data.data(),data.size());
  for(unsigned i=0;i<4;i++){
    b[4+i]=(uint8_t)(code>>(8*i));b[12+i]=(uint8_t)(bytes>>(8*i));b[16+i]=(uint8_t)(audioCrc>>(8*i));
  }
  uint32_t c=mfv3snd::crc32Update(0,b.data(),20);for(unsigned i=0;i<4;i++)b[20+i]=(uint8_t)(c>>(8*i));return b;
}
static void clean(){SD_MMC.disk.files.clear();SD_MMC.disk.dirs.clear();SD_MMC.disk.failRenameTo.clear();}
static void send(audio::FsStore& s,uint32_t code,const std::vector<uint8_t>& data){
  assert(s.begin(mfv3snd::Place::Sd,code));assert(s.write(data.data(),data.size()));
}
int main(){
  sim::sdCard=true;auto older=wav(1111),newer=wav(2222);
  const uint32_t code=0x12345678;const char* flat="/sonidos/12345678.wav";
  const char* bucket="/sonidos/12/12345678.wav";const char* bak="/sonidos/subida.bak";
  const char* marker="/sonidos/subida.txn";const char* temp="/sonidos/subida.tmp";
  clean();audio::FsStore store;store.begin();
  SD_MMC.disk.files[flat]=older;auto f=store.open(code);assert(f && f.size()==older.size());f.close();
  send(store,code,newer);SD_MMC.disk.failRenameTo=bucket;
  assert(!store.commit());assert(SD_MMC.disk.files[flat]==older && !SD_MMC.disk.files.count(bak));
  SD_MMC.disk.failRenameTo.clear();send(store,code,newer);assert(store.commit());
  assert(SD_MMC.disk.files[bucket]==newer && !SD_MMC.disk.files.count(flat));
  assert(!SD_MMC.disk.files.count(bak) && !SD_MMC.disk.files.count(marker));
  // Fallo del destino Y de la reversión: el anterior sigue íntegro en respaldo.
  send(store,code,older);SD_MMC.disk.failRenameTo=bucket;assert(!store.commit());
  assert(!store.recoveryOk() && SD_MMC.disk.files[bak]==newer && SD_MMC.disk.files.count(marker));
  SD_MMC.disk.failRenameTo.clear();audio::FsStore reboot;reboot.begin();
  assert(reboot.recoveryOk() && SD_MMC.disk.files[bucket]==newer && !SD_MMC.disk.files.count(bak));
  // No mover/borrar el anterior si ni siquiera se puede crear el respaldo.
  send(reboot,code,older);SD_MMC.disk.failRenameTo=bak;assert(!reboot.commit());
  assert(SD_MMC.disk.files[bucket]==newer);SD_MMC.disk.failRenameTo.clear();
  auto bad=newer;bad.resize(50);send(reboot,code,bad);assert(!reboot.commit());assert(SD_MMC.disk.files[bucket]==newer);
  // La tarjeta altera el PCM temporal sin alterar cabecera ni longitud: no tocar anterior.
  send(reboot,code,older);SD_MMC.disk.files[temp][100]^=0x20;
  assert(!reboot.commit() && SD_MMC.disk.files[bucket]==newer && !SD_MMC.disk.files.count(bak));
  // Corte entre marcador y movimiento: conservar el original plano.
  clean();SD_MMC.disk.files[flat]=older;SD_MMC.disk.files[temp]=newer;SD_MMC.disk.files[marker]=txn(code,1,newer);
  reboot.begin();assert(reboot.recoveryOk() && SD_MMC.disk.files[flat]==older && !SD_MMC.disk.files.count(temp));
  // Corte después de instalar: confirmar nuevo antes de retirar respaldo.
  clean();SD_MMC.disk.files[bucket]=newer;SD_MMC.disk.files[bak]=older;SD_MMC.disk.files[marker]=txn(code,1,newer);
  reboot.begin();assert(reboot.recoveryOk() && SD_MMC.disk.files[bucket]==newer && !SD_MMC.disk.files.count(bak));
  // Destino incompleto tras el corte: restaurar ruta y bytes anteriores.
  clean();SD_MMC.disk.files[bucket]=bad;SD_MMC.disk.files[bak]=older;SD_MMC.disk.files[marker]=txn(code,1,newer);
  reboot.begin();assert(reboot.recoveryOk() && SD_MMC.disk.files[flat]==older && !SD_MMC.disk.files.count(bucket));
  // Corrupción del cuerpo con cabecera y longitud intactas: CRC obliga a restaurar.
  clean();auto corrupt=newer;corrupt[100]^=0x20;
  SD_MMC.disk.files[bucket]=corrupt;SD_MMC.disk.files[bak]=older;SD_MMC.disk.files[marker]=txn(code,1,newer);
  reboot.begin();assert(reboot.recoveryOk() && SD_MMC.disk.files[flat]==older && !SD_MMC.disk.files.count(bucket));
  // Marcador corrupto: no adivinar su código ni borrar la única copia buena.
  clean();SD_MMC.disk.files[bak]=older;SD_MMC.disk.files[marker]={1,2,3};SD_MMC.disk.files[bucket]=newer;
  reboot.begin();assert(!reboot.recoveryOk());assert(!reboot.begin(mfv3snd::Place::Sd,code));assert(SD_MMC.disk.files[bak]==older);
  // Aunque el destino tenga cabecera válida, no se ofrece ni reproduce sin resolver el marcador.
  assert(!reboot.open(code));LGFX blockedLcd;audio::Player blocked;assert(blocked.begin(blockedLcd,reboot,80));
  sim::heard.clear();blocked.play({false,code,{nullptr,0}},false);blocked.pump(10);
  assert(!blocked.requestAccepted() && !blocked.lastStartSucceeded() && sim::heard.empty());
  assert(SD_MMC.disk.files[bucket]==newer && SD_MMC.disk.files[bak]==older);blocked.stop();
  clean();reboot.begin();SD_MMC.disk.files[bucket]=older;SD_MMC.disk.files[flat]=newer;
  f=reboot.open(code);uint8_t h[46];assert(f.read(h,sizeof h)==sizeof h && (int16_t)(h[44]|(h[45]<<8))==1111);f.close();

  LGFX lcd;sim::i2sRealtime=true;sim::heard.clear();audio::Player p;
  assert(p.begin(lcd,reboot,80));p.play({false,code,{nullptr,0}},false);
  assert(p.requestAccepted() && !p.lastStartSucceeded());p.pump(100);
  assert(p.ok && p.lastStartSucceeded() && sim::heard.empty() && sim::i2sDma.size()==2000);
  assert(p.writeTimeoutCount()>0);sim::advanceI2sFrames(256);p.pump(116);
  assert(p.ok && sim::i2sDma.size()==2000);
  for(auto v:sim::heard)assert(v==1111);
  // Reemplazo: descartar la cola DMA del anterior antes de habilitar el nuevo.
  SD_MMC.disk.files["/sonidos/12/12345679.wav"]=newer;sim::heard.clear();
  p.play({false,code+1,{nullptr,0}},false);p.pump(120);sim::advanceI2sFrames(2000);
  assert(sim::heard.size()==2000);for(auto v:sim::heard)assert(v==2222);
  // busy termina antes del drenaje: este test impide confundirlo con sonido físico.
  SD_MMC.disk.files["/sonidos/12/12345680.wav"]=wav(3333,16);sim::heard.clear();
  p.play({false,0x12345680,{nullptr,0}},false);p.pump(150);
  assert(!p.busy() && p.lastStartSucceeded() && sim::heard.empty() && sim::i2sDma.size()==2000);
  sim::advanceI2sFrames(16);for(auto v:sim::heard)assert(v==3333);
  // Error de archivo recuperable: no enciende el amplificador ni inutiliza el canal.
  p.play({false,0x99999999,{nullptr,0}},false);assert(p.ok && !p.requestAccepted());
  assert(p.failure()==audio::Player::Failure::NotFound && !(sim::exioOut&(1u<<exio::SPEAKER_AMP)));
  SD_MMC.disk.files["/sonidos/12/12345681.wav"]=bad;
  p.play({false,0x12345681,{nullptr,0}},false);assert(p.ok && p.failure()==audio::Player::Failure::BadWav);
  p.play({true,0,mfv3snd::defaultTune(mfv3snd::Event::Goal)},false);p.pump(180);assert(p.ok && p.lastStartSucceeded());
  // SD desaparece/se trunca después de abrir: fallo visible, sin generar PCM basura.
  p.play({false,code,{nullptr,0}},false);SD_MMC.disk.files[bucket].resize(46);p.pump(200);
  assert(p.ok && p.failure()==audio::Player::Failure::ReadError && !p.busy());SD_MMC.disk.files[bucket]=older;
  p.setVolume(0);auto errors=p.errorCount();p.play({false,code,{nullptr,0}},false);
  assert(!p.requestAccepted() && !p.lastStartSucceeded() && p.failure()==audio::Player::Failure::Muted && p.errorCount()==errors);
  p.setVolume(80);p.stop();
  // Timeout no fatal frente a errores reales: nunca recuperación/reinit automática en pump.
  audio::Player timeout;assert(timeout.begin(lcd,reboot,80));sim::i2sWriteResult=ESP_ERR_TIMEOUT;
  timeout.play({false,code,{nullptr,0}},false);timeout.pump(300);assert(timeout.ok && timeout.writeTimeoutCount()>0);
  sim::i2sWriteResult=-23;timeout.pump(301);assert(!timeout.ok && timeout.lastError()==-23);
  assert(timeout.failure()==audio::Player::Failure::I2SWrite && !timeout.lastStartSucceeded());sim::i2sWriteResult=ESP_OK;
  audio::Player partial;assert(partial.begin(lcd,reboot,80));sim::i2sPreloadLimit=100;
  partial.play({false,code,{nullptr,0}},false);partial.pump(400);
  assert(!partial.ok && partial.failure()==audio::Player::Failure::I2SPreload);sim::i2sPreloadLimit=~size_t(0);
  audio::Player enable;assert(enable.begin(lcd,reboot,80));enable.play({false,code,{nullptr,0}},false);sim::i2sEnableResult=-24;enable.pump(500);
  assert(!enable.ok && enable.lastError()==-24 && enable.failure()==audio::Player::Failure::I2SEnable);sim::i2sEnableResult=ESP_OK;
  audio::Player disable;assert(disable.begin(lcd,reboot,80));sim::failI2sStop=true;disable.stop();
  assert(!disable.ok && disable.failure()==audio::Player::Failure::I2SDisable);sim::failI2sStop=false;
  sim::i2sRealtime=false;
  puts("Audio resilience: replacement/recovery, bad WAV/read errors, DMA clock/stop, timeout/fatal diagnostics OK");
}
