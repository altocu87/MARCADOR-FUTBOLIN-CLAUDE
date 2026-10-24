// Empate de liga: motor, preferencias, catálogo y reproductor reales con LVGL.
#include <cassert>
#include <algorithm>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"
using namespace comentarios;
static std::vector<uint8_t> wav(int16_t sample) {
  std::vector<uint8_t> b(44+8000,0);
  auto u32=[&](unsigned at,uint32_t n){for(int i=0;i<4;i++)b[at+i]=(uint8_t)(n>>(8*i));};
  memcpy(b.data(),"RIFF",4);u32(4,b.size()-8);memcpy(b.data()+8,"WAVEfmt ",8);
  u32(16,16);u32(20,1|(1u<<16));u32(24,16000);u32(28,32000);u32(32,2|(16u<<16));
  memcpy(b.data()+36,"data",4);u32(40,b.size()-44);
  for(size_t i=44;i<b.size();i+=2){b[i]=(uint8_t)sample;b[i+1]=(uint8_t)(sample>>8);}return b;
}
static void advance(uint32_t ms) {
  sim::now+=ms;
  if(eng.tick(sim::now))afterChange(false,Team::White);
}
static void secondHalf(mfv3::EndCondition condition,unsigned white,unsigned blue) {
  resultPending=false;goHome();player.stop();queueCommentary(0);catalog.clear();
  sndChoices=mfv3snd::Choices();cfg=mfv3::Config();cfg.endCondition=condition;
  cfg.minutesPerPeriod=1;cfg.allowDraw=true;
  startMatch();advance(3000);assert(eng.phase()==Phase::Playing);
  for(unsigned i=0;i<white;i++){advance(3100);inputGoal(Team::White);}
  for(unsigned i=0;i<blue;i++){advance(3100);inputGoal(Team::Blue);}
  advance(60000);assert(eng.phase()==Phase::PeriodEnd && eng.period()==Period::First);
  inputButtonGoal(Team::White);advance(3000);
  assert(eng.phase()==Phase::Playing && eng.period()==Period::Second);
  player.stop();queueCommentary(0);commentaryLastCode=0;sim::heard.clear();sim::i2sRoom=10000;
  // También hay voces nuevas de tanda/campeón: ninguna pertenece al empate de liga.
  assert(catalog.add(FinalNormal,0x5001));
  assert(catalog.add(ExtraTimeAnnounce,0x5004));
  assert(catalog.add(ShootoutAnnounce,0x5005));
  assert(catalog.add(ShootoutChampion,0x5006));
}
int main() {
  sim::sdCard=true;sim::now=1000;setup();test.active=false;nube::phase=nube::OFF;goHome();
  SD_MMC.disk.files["/sonidos/00005001.wav"]=wav(1111);
  SD_MMC.disk.files["/sonidos/00005002.wav"]=wav(2222);
  SD_MMC.disk.files["/sonidos/00005003.wav"]=wav(3333);
  for(auto condition:{mfv3::EndCondition::Time,mfv3::EndCondition::Both}) {
    for(unsigned goals:{0u,1u}) {
      for(auto kind:{mfv3snd::Kind::Default,mfv3snd::Kind::None,mfv3snd::Kind::Custom}) {
        secondHalf(condition,goals,goals);
        auto& end=sndChoices.c[(unsigned)mfv3snd::Event::PeriodEnd];end.kind=kind;
        end.n=1;end.codes[0]=0x5002;
        // Victoria propia no debe reproducirse ni bloquear la preferencia de final neutro.
        assert(sndChoices.set("victory 00005003"));
        advance(60000);
        assert(eng.phase()==Phase::Finished && eng.draw());
        assert(!pendingCommentary && !catalog.started && !shootoutPreviewPending);
        // Esta prueba no persiste un partido: solo comprueba el fotograma y el audio.
        resultPending=false;render();uiport::step();serviceAudio();
        if(kind==mfv3snd::Kind::None)assert(sim::heard.empty() && !player.busy());
        else {
          assert(!sim::heard.empty());
          if(kind==mfv3snd::Kind::Custom)assert(sim::heard.front()==2222);
        }
        assert(std::find(sim::heard.begin(),sim::heard.end(),3333)==sim::heard.end());
        auto generation=commentaryGeneration;auto stops=sim::i2sStops;
        advance(100);assert(commentaryGeneration==generation && sim::i2sStops==stops);
      }
    }
  }
  // La configuración por defecto tampoco elige FinalNormal para el empate.
  secondHalf(mfv3::EndCondition::Time,0,0);advance(60000);
  assert(eng.draw() && !pendingCommentary && !catalog.started);resultPending=false;
  // Una liga con ganador conserva la voz de final y la victoria inmediata.
  secondHalf(mfv3::EndCondition::Time,1,0);advance(60000);
  assert(eng.phase()==Phase::Finished && !eng.draw() && pendingCommentary==0x5001);
  resultPending=false;
  for(unsigned i=0;i<40 && pendingCommentary;i++){sim::now+=4;loop();}
  assert(!pendingCommentary && commentaryLastCode==0x5001);
  puts("Audio empate liga: 0-0/1-1, tiempo/ambas, preferencias, familias de tanda y ganador real OK");
}
