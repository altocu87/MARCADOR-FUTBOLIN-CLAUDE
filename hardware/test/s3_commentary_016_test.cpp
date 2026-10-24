// Tanda: predicción contrastada contra el motor, prioridades, serie y persistencia de bits altos.
#include <cassert>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"
using namespace comentarios;
static void toShootout(mfv3::Engine& e,mfv3::Config c,uint32_t& t){
 c.endCondition=mfv3::EndCondition::Time;c.minutesPerPeriod=1;c.overtimeSeconds=10;
 assert(e.start(c,t));
 for(int i=0;i<3;i++){t+=3000;e.tick(t);t+=i==2?10000:60000;e.tick(t);assert(e.phase()==Phase::PeriodEnd);e.next(t);}
 assert(e.phase()==Phase::Penalties);
}
static void prediction(mfv3::Engine e,uint32_t t,unsigned depth){
 if(e.phase()!=Phase::Penalties || !depth)return;
 for(bool scored:{false,true}){
  mfv3::Team predicted;bool wins=winsAfterKick(e,scored,predicted);auto actual=e;
  assert(actual.penalty(actual.nextKicker(),scored,t+1)==mfv3::Result::Accepted);
  assert(wins==(actual.phase()==Phase::Finished));if(wins)assert(predicted==actual.winner());
  prediction(actual,t+1,depth-1);
 }
}
static std::vector<uint8_t> wav(){
 std::vector<uint8_t>b(44+32000,0);auto u32=[&](unsigned at,uint32_t x){for(int i=0;i<4;i++)b[at+i]=(uint8_t)(x>>(8*i));};
 memcpy(b.data(),"RIFF",4);u32(4,b.size()-8);memcpy(b.data()+8,"WAVEfmt ",8);u32(16,16);u32(20,1|(1u<<16));u32(24,16000);u32(28,32000);u32(32,2|(16u<<16));memcpy(b.data()+36,"data",4);u32(40,b.size()-44);return b;
}
int main(){
 for(unsigned rounds=1;rounds<=5;rounds++)for(Team first:{Team::White,Team::Blue}){
  mfv3::Engine e;mfv3::Config c;c.penaltyRounds=rounds;c.penaltyFirst=first;uint32_t t=1;toShootout(e,c,t);prediction(e,t,12);
  assert(shootoutBeforeMask(e)==(PoolMask(1)<<ShootoutFirst));
 }
 sim::sdCard=true;sim::now=1000;setup();test.active=false;nube::phase=nube::OFF;goHome();
 catalog.clear();for(unsigned p=31;p<PoolCount;p++){unsigned code=0x9000+p;char path[64];snprintf(path,sizeof path,"/sonidos/%08x.wav",code);SD_MMC.disk.files[path]=wav();assert(catalog.add(p,code));}
 // Todos los grupos altos sobreviven a escritura/recarga y al sorteo.
 uint32_t crc=0;for(unsigned p=31;p<PoolCount;p++){uint8_t b[5];row(p,0x9000+p,b);crc=mfv3snd::crc32Update(crc,b,5);}
 assert(save(sndStore,catalog,crc));catalog.clear();load(sndStore);
 for(unsigned p=31;p<PoolCount;p++)assert(catalog.pick(PoolMask(1)<<p,0,0)==0x9000+p);
 cfg=mfv3::Config();cfg.penaltyRounds=1;uint32_t t=sim::now;toShootout(eng,cfg,t);sim::now=t;inMatch=true;
 sndBefore=mfv3snd::before(eng);sndBefore.period=Period::Overtime;sndBefore.phase=Phase::PeriodEnd;
 afterChange(false,Team::White);assert(shootoutPreviewPending);
 player.stop();queueCommentary(0);loop();assert(pendingCommentary==0x9000+ShootoutFirst || commentaryLastCode==0x9000+ShootoutFirst);
 // Primer tiro marcado: todavía no gana; el segundo debe marcar para no perder.
 assert(eng.penalty(Team::White,true,sim::now)==mfv3::Result::Accepted);afterChange(false,Team::White);
 assert(pendingCommentary==0x9000+ShootoutGoal);assert(shootoutPreviewPending);
 assert(shootoutBeforeMask(eng)==(PoolMask(1)<<ShootoutMustScore));
 for(int i=0;i<50 && pendingCommentary;i++){sim::now+=4;loop();}
 assert(player.busy());auto generation=commentaryGeneration;loop();assert(commentaryGeneration==generation); // espera resultado
 player.stop();loop();assert(!shootoutPreviewPending);assert(!pendingCommentary && commentaryLastCode==0x9000+ShootoutMustScore);
 // Un fallo termina: solo fallo, sin aviso futuro ni voz positiva.
 assert(eng.penalty(Team::Blue,false,sim::now)==mfv3::Result::Accepted);afterChange(false,Team::Blue);
 assert(!shootoutPreviewPending && pendingCommentary==0x9000+ShootoutMiss);
 assert(shootoutResultMask(eng,false,false)==(PoolMask(1)<<ShootoutMiss));
 // Final al mejor de tres: primera victoria no es título; segunda sí.
 tourMatch=true;tor::curOk=true;tor::cur.nFix=1;tourFixture=0;tor::cur.format=torneos::Format::League;
 static torneos::Fixture fixture;tor::cur.fix=&fixture;
 auto& f=tor::cur.fix[0];f=torneos::Fixture();f.isFinal=true;f.bestOf=3;f.serW=0;f.serB=0;
 assert(!shootoutWinsTournament());f.serW=1;assert(shootoutWinsTournament());
 assert(shootoutResultMask(eng,true,true)==(PoolMask(1)<<ShootoutChampion));
 assert(shootoutResultMask(eng,true,false)==(PoolMask(1)<<ShootoutWin));
 // En muerte súbita el primer gol del par no gana; tras fallo rival sí puede ganar.
 mfv3::Engine e;mfv3::Config c;c.penaltyRounds=1;t=1;toShootout(e,c,t);e.penalty(Team::White,false,t);e.penalty(Team::Blue,false,t);
 assert(shootoutBeforeMask(e)==(PoolMask(1)<<ShootoutBefore));e.penalty(Team::White,false,t);
 assert(shootoutBeforeMask(e)==(PoolMask(1)<<ShootoutCanWin));
 // Recorrido real del gol ganador: partido, primer triunfo de una serie y campeonato.
 for(unsigned scenario=0;scenario<3;scenario++){
  eng=e;sndBefore=mfv3snd::before(eng);snap=takeSnapshot();player.stop();queueCommentary(0);resultPending=false;
  tourMatch=scenario>0;f.serB=scenario==2?1:0;
  assert(eng.penalty(Team::Blue,true,sim::now)==mfv3::Result::Accepted);afterChange(false,Team::Blue);
  assert(eng.phase()==Phase::Finished && !shootoutPreviewPending);
  assert(pendingCommentary==0x9000+(scenario==2?ShootoutChampion:ShootoutWin));
 }
 // Las preferencias explícitas de victoria conservan prioridad sobre las voces automáticas.
 eng=e;sndBefore=mfv3snd::before(eng);snap=takeSnapshot();queueCommentary(0);player.stop();
 sndChoices.c[(unsigned)mfv3snd::Event::Victory].kind=mfv3snd::Kind::None;
 eng.penalty(Team::Blue,true,sim::now);afterChange(false,Team::Blue);assert(!pendingCommentary);
 printf("Tanda 0.16 OK: predicciones contra motor, bits31-38 persistidos, audio previo/resultados, muerte subita y series; Catalog=%zu\n",sizeof(Catalog));
}
