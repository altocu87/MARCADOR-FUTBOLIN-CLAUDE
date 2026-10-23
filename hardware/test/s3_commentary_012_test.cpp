// Regresión 0.12: frases de fases, límites exactos y prioridad de anulación.
#include <cassert>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"
using namespace comentarios;
static std::vector<uint8_t> wav(){
 std::vector<uint8_t>b(44+320000,0);auto u32=[&](unsigned at,uint32_t x){for(int i=0;i<4;i++)b[at+i]=(uint8_t)(x>>(8*i));};
 memcpy(b.data(),"RIFF",4);u32(4,b.size()-8);memcpy(b.data()+8,"WAVEfmt ",8);u32(16,16);u32(20,1|(1u<<16));u32(24,16000);u32(28,32000);u32(32,2|(16u<<16));memcpy(b.data()+36,"data",4);u32(40,b.size()-44);return b;
}
static void reset(){resultPending=false;goHome();player.stop();commentaryLastCode=0;catalog.clear();sndChoices=mfv3snd::Choices();cfg=mfv3::Config();cfg.endCondition=mfv3::EndCondition::Time;cfg.minutesPerPeriod=1;}
static void frame(){for(int i=0;i<40 && pendingCommentary;i++){sim::now+=4;loop();}assert(!pendingCommentary);}
static void begin(){startMatch();sim::now+=3000;loop();assert(eng.phase()==Phase::Playing);}
static void score(Team t){sim::now+=3100;loop();inputGoal(t);}
static void firstEnd(){sim::now+=60000;loop();assert(eng.phase()==Phase::PeriodEnd&&eng.period()==Period::First);}
static void secondStart(){inputButtonGoal(Team::White);assert(eng.phase()==Phase::Countdown);sim::now+=3000;loop();assert(eng.phase()==Phase::Playing&&eng.period()==Period::Second);}
int main(){
 sim::sdCard=true;sim::now=1000;setup();test.active=false;nube::phase=nube::OFF;goHome();
 for(uint32_t code=0x3001;code<=0x3008;code++){char path[64];snprintf(path,sizeof path,"/sonidos/%08lx.wav",(unsigned long)code);SD_MMC.disk.files[path]=wav();}
 // Inicio efectivo, no al pulsar continuar. Bordes: 0–1, 2–6, 7+.
 for(unsigned goals:{0u,1u,2u,6u,7u,8u}){
  reset();uint8_t pool=goals<2?SecondHalfFew:goals>6?SecondHalfMany:SecondHalf;assert(catalog.add(pool,0x3001));begin();
  for(unsigned i=0;i<goals;i++)score(i%2?Team::White:Team::Blue);
  firstEnd();auto mask=secondHalfMask(eng);assert(mask&(1u<<SecondHalf));
  assert(bool(mask&(1u<<SecondHalfFew))==(goals<2));assert(bool(mask&(1u<<SecondHalfMany))==(goals>6));
  inputButtonGoal(Team::White);assert(!pendingCommentary);sim::now+=3000;loop();assert(pendingCommentary==0x3001 || commentaryLastCode==0x3001);frame();
  auto generation=commentaryGeneration;eng.pause(sim::now);afterChange(false,Team::White);sim::now+=1000;eng.resume(sim::now);afterChange(false,Team::White);loop();
  assert(commentaryGeneration==generation); // ni pausa ni repintado repiten la frase
 }
 // El marcador válido de la primera parte descuenta una anulación: siete pasan a seis.
 reset();begin();for(unsigned i=0;i<7;i++)score(i%2?Team::White:Team::Blue);sim::now+=3100;loop();inputAnnul(Team::Blue);
 assert(!(secondHalfMask(eng)&(1u<<SecondHalfMany)));
 // None/Custom de inicio mandan también en segunda parte.
 for(auto kind:{mfv3snd::Kind::None,mfv3snd::Kind::Custom}){
  reset();catalog.add(SecondHalfFew,0x3001);sndChoices.c[(unsigned)mfv3snd::Event::MatchStart].kind=kind;begin();firstEnd();secondStart();assert(!pendingCommentary);
 }
 // Con contexto específico: 70/30 contra el comodín de segunda parte, nunca gol normal.
 Catalog c;assert(c.add(Comodin,0x3002)&&c.add(SecondHalf,0x3003)&&c.add(SecondHalfMany,0x3004));unsigned specific=0,generic=0;
 auto mask=(1u<<SecondHalf)|(1u<<SecondHalfMany);for(unsigned i=0;i<10000;i++){auto code=c.pick(mask,(uint32_t)(((uint64_t)(2*i+1)<<31)/10000),i,SecondHalf);assert(code!=0x3002);specific+=code==0x3004;generic+=code==0x3003;}
 assert(specific==7000 && generic==3000);
 // Anunciar prórroga al acabar segunda empatada y penaltis al acabar prórroga sin ganador, solo una vez.
 reset();catalog.add(ExtraTimeAnnounce,0x3005);catalog.add(ShootoutAnnounce,0x3006);begin();firstEnd();secondStart();
 sim::now+=60000;loop();assert(eng.phase()==Phase::PeriodEnd&&(pendingCommentary==0x3005||commentaryLastCode==0x3005));frame();auto generation=commentaryGeneration;loop();assert(commentaryGeneration==generation);
 inputButtonGoal(Team::Blue);sim::now+=3000;loop();assert(eng.period()==Period::Overtime&&eng.phase()==Phase::Playing);assert(commentaryGeneration==generation);
 sim::now+=(uint32_t)cfg.overtimeSeconds*1000;loop();assert(eng.phase()==Phase::PeriodEnd&&(pendingCommentary==0x3006||commentaryLastCode==0x3006));frame();generation=commentaryGeneration;
 inputButtonGoal(Team::White);assert(eng.phase()==Phase::Penalties&&eng.kickCount()==0);loop();assert(commentaryGeneration==generation);
 // Un gol de oro llega a victoria inmediatamente y conserva la voz combinada, sin final normal.
 reset();catalog.add(GoldenWinner,0x3007);begin();firstEnd();secondStart();sim::now+=60000;loop();inputButtonGoal(Team::Blue);sim::now+=3000;loop();score(Team::Blue);
 assert(eng.phase()==Phase::Finished&&eng.reason()==mfv3::Reason::GoldenGoal&&pendingCommentary==0x3007);frame();assert(commentaryLastCode==0x3007);
 // Ganar por objetivo de goles no se confunde con gol de oro de prórroga.
 reset();catalog.add(Comodin,0x3002);catalog.add(GoldenWinner,0x3007);cfg.endCondition=mfv3::EndCondition::Goals;cfg.goalsPerPeriod=1;begin();score(Team::White);
 assert(eng.phase()==Phase::Finished&&pendingCommentary==0x3002&&!(catalog.started&(1u<<GoldenWinner)));
 // Ventaja exacta de tres para ambos equipos: dos y cuatro no valen.
 for(Team team:{Team::White,Team::Blue}){mfv3::Engine e;mfv3::Config conf;conf.endCondition=mfv3::EndCondition::Time;conf.minutesPerPeriod=10;uint32_t t=1000;e.start(conf,t);t+=3000;e.tick(t);
  for(unsigned difference=1;difference<=4;difference++){t+=3100;e.tick(t);auto b=mfv3snd::before(e);assert(e.goal(team,t)==mfv3::Result::Accepted);assert(bool(goalMask(e,b,team)&(1u<<Lead3))==(difference==3));}
 }
 // Anular interrumpe una voz de gol en reproducción y selecciona exclusivamente Anulado.
 reset();catalog.add(Comodin,0x3002);catalog.add(Anulado,0x3008);begin();score(Team::White);frame();assert(player.busy());sim::now+=3100;loop();inputAnnul(Team::White);
 assert(pendingCommentary==0x3008);frame();assert(commentaryLastCode==0x3008);
 // Persistencia real del bit 30: guardar CMT1, recargar y consultar sin truncar el índice.
 catalog.clear();assert(catalog.add(Lead3,0x3007));uint8_t row[5];comentarios::row(Lead3,0x3007,row);uint32_t crc=mfv3snd::crc32Update(0,row,5);
 assert(save(sndStore,catalog,crc));catalog.clear();load(sndStore);assert(catalog.n==1&&catalog.counts[Lead3]==1);sim::out.clear();handleBoardLine("COMENTARIOS?");assert(sim::out.find("COMENTARIOS total=1 limite=1024 grupos=")!=std::string::npos);
 printf("Comentarios 0.12: fases, limites, 70/30 de segunda parte, gol de oro, anulacion y preferencias OK; Catalog=%zu B\n",sizeof(Catalog));
}
