// Integration of management exclusion, remote integrity, timing and immediate feedback.
#include <cassert>
#include <string>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"
static std::vector<uint8_t> sound(){
  std::vector<uint8_t> b(44+32000,0);
  auto u32=[&](unsigned at,uint32_t x){for(int i=0;i<4;i++)b[at+i]=(uint8_t)(x>>(8*i));};
  memcpy(b.data(),"RIFF",4);u32(4,b.size()-8);memcpy(b.data()+8,"WAVEfmt ",8);u32(16,16);u32(20,1|(1u<<16));u32(24,16000);u32(28,32000);u32(32,2|(16u<<16));memcpy(b.data()+36,"data",4);u32(40,32000);
  for(unsigned i=44;i<b.size();i+=2){b[i]=0x12;b[i+1]=0x05;}return b;
}
static void tick(unsigned n=25){for(unsigned i=0;i<n;i++){sim::now+=3;loop();}}
static void expectBusy(const char* cmd){sim::out.clear();handleBoardLine(cmd);assert(sim::out.find("ocupado")!=std::string::npos);}
int main(){
  sim::sdCard=true;sim::now=1000;setup();test.active=false;nube::phase=nube::OFF;goHome();
  auto b=sound();SD_MMC.mkdir("/sonidos/12");SD_MMC.disk.files["/sonidos/12/12345678.wav"]=b;
  uint32_t crc=mfv3snd::crc32Update(0,b.data(),b.size());
  // CRC is computed incrementally from the physical File API; arbitrary names are rejected.
  sim::out.clear();handleBoardLine("SONIDO_INFO 12345678");assert(soundInspection.active);
  inspectSoundTick();assert(soundInspection.active && soundInspection.bytes<=2048);
  startMatch();assert(!inMatch); // no match can start while the check owns the file
  for(int i=0;i<100 && soundInspection.active;i++)inspectSoundTick();
  char expected[130];snprintf(expected,sizeof expected,"SONIDO_INFO 12345678 tam=%u crc32=%08lx",(unsigned)b.size(),(unsigned long)crc);
  assert(!soundInspection.active && sim::out.find(expected)!=std::string::npos);
  sim::out.clear();handleBoardLine("SONIDO_INFO aaaaaaaa");assert(sim::out.find("ausente=1")!=std::string::npos);
  sim::out.clear();handleBoardLine("SD_IDENTIDAD?");assert(sim::out.find("placa=80456b625a24")!=std::string::npos);
  // A transaction prevents a new match. During a match, writes/scans/captures are rejected.
  handleBoardLine("SONIDO_SUBIR sd aaaaaaaa 100");assert(sndCmd.receiving());startMatch();assert(!inMatch);
  sndCmd.tick(millis()+8001,sendLine);assert(!sndCmd.receiving());
  cfg=mfv3::Config();cfg.endCondition=mfv3::EndCondition::Time;startMatch();assert(inMatch);
  expectBusy("SONIDO_SUBIR sd aaaaaaaa 100");expectBusy("SONIDO_BORRAR 12345678");expectBusy("SONIDOS_PAGINA 0");expectBusy("SONIDO_INFO 12345678");expectBusy("CAPTURA");expectBusy("PERFIL");
  expectBusy("sonido_subir sd aaaaaaaa 100");expectBusy("SoNiDo_BoRrAr 12345678");expectBusy("sonidos?");expectBusy("sonidos_pagina 0");expectBusy("perfil");expectBusy("SoNiDo 12345678");expectBusy("volumen 30");
  assert(SD_MMC.disk.files["/sonidos/12/12345678.wav"]==b);
  // Winner feedback is scheduled before saving. Starting another game cannot overwrite it.
  goHome();cfg=mfv3::Config();cfg.goalsPerPeriod=1;comentarios::catalog.clear();assert(comentarios::catalog.add(0,0x12345678));
  sndChoices=mfv3snd::Choices();startMatch();sim::now+=4000;loop();unsigned prior=matchesPlayed;
  inputGoal(Team::White);assert(eng.phase()==Phase::Finished && resultPending && matchesPlayed==prior);
  uint32_t frame=resultFrame;startMatch();assert(eng.phase()==Phase::Finished && resultPending);
  for(int i=0;i<150 && resultPending;i++){sim::now+=3;loop();}
  assert(!resultPending && matchesPlayed==prior+1 && uiport::presented!=frame);
  assert(commentaryLastCode==0x12345678 && !strcmp(commentaryOutcome,"iniciado"));
  unsigned saved=matchesPlayed;tick();assert(matchesPlayed==saved);goHome();
  // A winning input received inside the LVGL step must wait for the next render,
  // even when its voice is disabled and the old view was flushed in that step.
  comentarios::catalog.clear();sndChoices.c[(int)mfv3snd::Event::Goal].kind=mfv3snd::Kind::None;
  startMatch();sim::now+=4000;loop();prior=matchesPlayed;
  auto timer=lv_timer_create([](lv_timer_t* t){lv_timer_delete(t);inputGoal(Team::White);},1,nullptr);
  lv_timer_ready(timer);lv_obj_invalidate(lv_screen_active());sim::now+=5;loop();
  assert(eng.phase()==Phase::Finished && resultPending && matchesPlayed==prior);
  for(int i=0;i<150 && resultPending;i++){sim::now+=3;loop();}
  assert(!resultPending && matchesPlayed==prior+1);goHome();
  // Silence must never be reported as a successful audible start.
  player.setVolume(0);lv_obj_invalidate(lv_screen_active());queueCommentary(0x12345678);lastKey=0xFFFFFFFF;tick(30);
  assert(!pendingCommentary && !strcmp(commentaryOutcome,"silencio") && !commentaryLastCode && !commentaryStartedAt);
  player.setVolume(80);
  // Missing files are reported as fallback, never the requested voice.
  lv_obj_invalidate(lv_screen_active());queueCommentary(0x99999999);lastKey=0xFFFFFFFF;tick(30);
  assert(!pendingCommentary && !strcmp(commentaryOutcome,"respaldo") && !commentaryLastCode);
  // The catalogue limit must not hide excess physical files in a diagnostic.
  goHome();for(unsigned i=0;i<1025;i++){char path[70];snprintf(path,sizeof path,"/sonidos/%08x.wav",0x70000000u+i);SD_MMC.disk.files[path]={1};}
  sim::out.clear();handleBoardLine("SONIDOS_PAGINA 0");
  assert(sim::out.find("total=1024 truncado=1")!=std::string::npos);
  puts("Sound safety: incremental remoteCRC, management exclusion, frame-before-save, no double result, silence/fallback OK");
}
