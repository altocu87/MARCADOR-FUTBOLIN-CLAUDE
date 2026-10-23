// Integración 0.11 de penaltis y reloj de ambiente; nunca abre un puerto real.
#include <cassert>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"
using namespace comentarios;
static std::vector<uint8_t> wav(){
  std::vector<uint8_t> b(44+32000,0);auto u32=[&](unsigned at,uint32_t x){for(int i=0;i<4;i++)b[at+i]=(uint8_t)(x>>(8*i));};
  memcpy(b.data(),"RIFF",4);u32(4,b.size()-8);memcpy(b.data()+8,"WAVEfmt ",8);u32(16,16);u32(20,1|(1u<<16));u32(24,16000);u32(28,32000);u32(32,2|(16u<<16));memcpy(b.data()+36,"data",4);u32(40,b.size()-44);return b;
}
static void reset(){resultPending=false;goHome();player.stop();queueCommentary(0);catalog.clear();sndChoices=mfv3snd::Choices();cfg=mfv3::Config();cfg.endCondition=mfv3::EndCondition::Time;cfg.minutesPerPeriod=10;}
static void bind(const mfv3::Engine& e,uint32_t at){
  eng=e;cfg=eng.config();sim::now=at;inMatch=true;snap=takeSnapshot();sndBefore=mfv3snd::before(eng);lastKey=0xffffffff;hcIntroUntil=hcSoundSeq=0;
}
int main(){
  AmbienceClock clock;clock.reset(0);assert(clock.nextAt==30000);clock.reset(UINT32_MAX);assert(clock.nextAt==60000);
  clock.reset(0);assert(clock.due(1000,true,false,0)==-1 && !clock.openingDone);
  assert(clock.due(1000,true,true,0)==Opening20 && clock.nextAt==31000);
  assert(clock.due(30999,true,true,0)==-1);assert(clock.due(31000,true,true,UINT32_MAX)==Ambience && clock.nextAt==91000);
  clock.score(90000);assert(clock.due(99999,true,true,0)==-1);assert(clock.due(100000,true,true,0)==Ambience);
  clock.reset(0);assert(clock.due(20001,true,true,0)==-1 && clock.openingDone);
  assert(clock.due(30000,false,false,0)==-1 && clock.nextAt==30000);assert(clock.due(30000,false,true,0)==Ambience);
  // Pesos 70/30 usan comodín de PENALTI, no narraciones de gol normal.
  Catalog bag;bag.add(Comodin,1);bag.add(PenaltyGoal,2);bag.add(PenaltyEqualizer,3);bag.add(PenaltyEqualizer20,4);unsigned hist[5]={};
  for(uint32_t i=0;i<1000;i++){uint32_t random=(uint32_t)((((uint64_t)i<<32)+(1ull<<31))/1000);hist[bag.pick((1u<<PenaltyGoal)|(1u<<PenaltyEqualizer)|(1u<<PenaltyEqualizer20),random,0,PenaltyGoal)]++;}
  assert(!hist[1] && hist[2]==300 && hist[3]==350 && hist[4]==350);
  sim::sdCard=true;sim::now=1000;setup();test.active=false;nube::phase=nube::OFF;goHome();
  for(uint32_t code:{0x1001u,0x1002u,0x1003u}){char path[50];snprintf(path,sizeof path,"/sonidos/%08lx.wav",(unsigned long)code);SD_MMC.disk.files[path]=wav();}
  // Primera ventana sin necesidad de un repintado nuevo: el ambiente no modifica el marcador.
  reset();catalog.add(Ambience,0x1001);catalog.add(Opening20,0x1002);startMatch();sim::now+=3000;loop();player.stop();
  ambienceQuietAt=sim::now-500;sim::now+=100;loop();assert(commentaryLastCode==0x1002 && ambientCommentary && !commentaryHasFallback);
  auto before=eng.score();inputGoal(Team::White);assert(!ambientCommentary && eng.score().white==before.white+1);
  player.stop();ambienceClock.nextAt=playTime(eng,sim::now);ambienceQuietAt=sim::now-500;
  sim::now+=9900;loop();assert(!ambientCommentary);
  player.stop();ambienceQuietAt=sim::now-500;sim::now+=200;loop();assert(ambientCommentary && commentaryLastCode==0x1001);
  inputPause();assert(eng.phase()==Phase::Paused && !ambientCommentary && !player.busy());
  uint32_t elapsed=playTime(eng,sim::now);sim::now+=120000;loop();assert(playTime(eng,sim::now)==elapsed && !ambientCommentary);
  // Un archivo de ambiente perdido no se sustituye por el tono de GOL.
  reset();catalog.add(Ambience,0x9999);startMatch();sim::now+=3000;loop();player.stop();ambienceClock.openingDone=true;ambienceClock.nextAt=0;
  ambienceQuietAt=sim::now-500;sim::now+=100;loop();assert(!player.busy() && !commentaryHasFallback && !strcmp(commentaryOutcome,"fallo"));
  // Penalti del hándicap: anuncio único; al resolver, familia propia nueva, una sola vez.
  for(bool scored:{false,true}){
    reset();catalog.add(PenaltyMiss,0x1001);catalog.add(PenaltyGoal,0x1002);
    mfv3::Engine penalty;bool found=false;
    for(uint32_t seed=1;seed<=500 && !found;seed++){
      mfv3::Config c;c.chaos=true;c.endCondition=mfv3::EndCondition::Time;c.minutesPerPeriod=30;
      penalty.start(c,1000);penalty.skipCountdown(1000);penalty.seed(seed);penalty.tick(50000);
      found=penalty.phase()==Phase::Handicap && penalty.handicapStage()==mfv3::HStage::Announce && penalty.handicap().kind==mfv3::HKind::Penalty;
    }
    assert(found);bind(penalty,50000);afterChange(false,Team::White);player.stop();queueCommentary(0);onHcPenalty(scored);
    assert(pendingCommentary==(scored?0x1002u:0x1001u));auto generation=commentaryGeneration;afterChange(false,Team::White);assert(commentaryGeneration==generation);
  }
  // Tanda: jamás inferir empate del partido a partir de su marcador 0-0 permanente.
  reset();catalog.add(PenaltyMiss,0x1001);catalog.add(PenaltyGoal,0x1002);catalog.add(PenaltyEqualizer,0x1003);
  mfv3::Config c;c.endCondition=mfv3::EndCondition::Time;c.minutesPerPeriod=1;mfv3::Engine e;e.start(c,1000);e.tick(4000);e.tick(64000);e.next(65000);e.tick(68000);e.tick(128000);e.next(129000);e.tick(132000);e.tick(192000);e.next(193000);
  assert(e.phase()==Phase::Penalties);bind(e,193000);inputGoal(Team::White);assert(pendingCommentary==0x1002 && penaltyMask(eng,true)==(1u<<PenaltyGoal));
  player.stop();queueCommentary(0);inputGoal(Team::Blue);assert(pendingCommentary==0x1002 && eng.penaltyScore().white==eng.penaltyScore().blue);
  // Igualar de penalti en juego sí añade ambos contextos al llegar a los últimos 20 s.
  e.start(c,1000);e.tick(4000);e.tick(64000);e.next(65000);e.tick(68000);
  e.goal(Team::White,78000);e.goal(Team::Blue,108000);
  auto mask=penaltyMask(e,true);assert((mask&(1u<<PenaltyEqualizer)) && (mask&(1u<<PenaltyEqualizer20)));
  // Simular una anulación elimina la pista pendiente y tiene prioridad sobre ambiente/penaltis.
  reset();catalog.add(Anulado,0x1003);startMatch();sim::now+=3000;loop();inputGoal(Team::White);queueCommentary(0x1001);ambientCommentary=true;
  inputAnnul(Team::White);assert(!ambientCommentary && pendingCommentary==0x1003);
  puts("Penaltis/ambiente: 30-60s de juego, pausa, 10s tras gol, ventana unica, no tono falso, prioridad y 70/30 de penalti OK");
}
