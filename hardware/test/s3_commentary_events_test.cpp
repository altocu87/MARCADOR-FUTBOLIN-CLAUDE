// Eventos 0.11: motor y reproductor reales con almacenamiento y reloj simulados.
#include <cassert>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"
using namespace comentarios;
static std::vector<uint8_t> wav(int16_t value){
  std::vector<uint8_t> b(44+320000,0); // diez segundos: intro continúa tras la cuenta atrás
  auto u32=[&](unsigned at,uint32_t x){for(int i=0;i<4;i++)b[at+i]=(uint8_t)(x>>(8*i));};
  memcpy(b.data(),"RIFF",4);u32(4,b.size()-8);memcpy(b.data()+8,"WAVEfmt ",8);u32(16,16);u32(20,1|(1u<<16));u32(24,16000);u32(28,32000);u32(32,2|(16u<<16));memcpy(b.data()+36,"data",4);u32(40,b.size()-44);
  for(unsigned i=44;i<b.size();i+=2){b[i]=(uint8_t)value;b[i+1]=(uint8_t)(value>>8);}return b;
}
static void frame(){for(unsigned i=0;i<30 && pendingCommentary;i++){sim::now+=4;loop();}assert(!pendingCommentary);}
static void reset(){resultPending=false;goHome();player.stop();catalog.clear();sndChoices=mfv3snd::Choices();cfg=mfv3::Config();cfg.endCondition=mfv3::EndCondition::Time;cfg.minutesPerPeriod=1;}
static mfv3snd::Before goal(mfv3::Engine& e,uint32_t& t,Team team){t+=3100;e.tick(t);auto b=mfv3snd::before(e);assert(e.goal(team,t)==mfv3::Result::Accepted);return b;}
int main(){
  sim::sdCard=true;sim::now=1000;setup();test.active=false;nube::phase=nube::OFF;goHome();
  SD_MMC.disk.files["/sonidos/00001001.wav"]=wav(1111);
  SD_MMC.disk.files["/sonidos/00001002.wav"]=wav(2222);
  SD_MMC.disk.files["/sonidos/00001003.wav"]=wav(3333);
  // Solo la primera cuenta atrás inicia la intro. No se vuelve a disparar al reanudar.
  reset();assert(catalog.add(Comodin,0x1001) && catalog.add(Intro,0x1002));startMatch();
  assert(eng.phase()==Phase::Countdown && pendingCommentary==0x1002 && introCommentary);
  frame();assert(commentaryLastCode==0x1002 && player.busy());
  unsigned stops=sim::i2sStops;sim::now+=3000;loop();
  assert(eng.phase()==Phase::Playing && player.busy() && sim::i2sStops==stops && !introCommentary);
  inputGoal(Team::White);assert(pendingCommentary==0x1001 && sim::i2sStops>stops);
  frame();assert(commentaryLastCode==0x1001);inputAnnul(Team::White);assert(!pendingCommentary);
  // None y Custom explícitos mandan sobre el catálogo de introducciones.
  for(auto kind:{mfv3snd::Kind::None,mfv3snd::Kind::Custom}){
    reset();catalog.add(Intro,0x1002);sndChoices.c[(unsigned)mfv3snd::Event::MatchStart].kind=kind;
    startMatch();assert(!pendingCommentary && !introCommentary);
  }
  // Descanso: una sola vez; ni la cuenta atrás siguiente ni la prórroga usan esta familia.
  reset();catalog.add(Descanso,0x1002);startMatch();sim::now+=3000;loop();sim::now+=60000;loop();
  assert(eng.phase()==Phase::PeriodEnd && (pendingCommentary==0x1002 || commentaryLastCode==0x1002));frame();
  auto generation=commentaryGeneration;loop();assert(commentaryGeneration==generation);
  inputButtonGoal(Team::White);assert(eng.phase()==Phase::Countdown && !pendingCommentary);
  sim::now+=3000;loop();sim::now+=60000;loop();assert(eng.phase()==Phase::PeriodEnd && !pendingCommentary);
  inputButtonGoal(Team::Blue);sim::now+=3000;loop();assert(eng.period()==Period::Overtime && !pendingCommentary);
  // Final sin prórroga: solo al agotarse el tiempo con ganador. No en empate que va a prórroga.
  reset();catalog.add(FinalNormal,0x1003);startMatch();sim::now+=3000;loop();inputGoal(Team::White);
  sim::now+=60000;loop();inputButtonGoal(Team::Blue);sim::now+=3000;loop();sim::now+=60000;loop();
  assert(eng.phase()==Phase::Finished && (pendingCommentary==0x1003 || commentaryLastCode==0x1003));frame();
  generation=commentaryGeneration;loop();assert(commentaryGeneration==generation);
  // Gol ganador por objetivo: pantalla de victoria y voz de gol, sin concatenar final largo.
  reset();catalog.add(Comodin,0x1001);catalog.add(FinalNormal,0x1003);cfg.endCondition=mfv3::EndCondition::Goals;cfg.goalsPerPeriod=1;
  startMatch();sim::now+=3000;loop();inputGoal(Team::White);
  assert(eng.phase()==Phase::Finished && pendingCommentary==0x1001);frame();
  assert(commentaryLastCode==0x1001 && !(catalog.started&(1u<<FinalNormal)));
  // Remontada de ambos lados: empate no basta; ponerse por delante sí, sin pedir -2.
  mfv3::Engine e;uint32_t t;mfv3::Config c;c.endCondition=mfv3::EndCondition::Time;c.minutesPerPeriod=10;
  for(Team team:{Team::White,Team::Blue}){
    Team rival=team==Team::White?Team::Blue:Team::White;t=1000;e.start(c,t);t+=3000;e.tick(t);
    goal(e,t,rival);auto b=goal(e,t,team);assert(!(goalMask(e,b,team)&(1u<<Remontada)));
    b=goal(e,t,team);assert(goalMask(e,b,team)&(1u<<Remontada));
    b=goal(e,t,team);assert(!(goalMask(e,b,team)&(1u<<Remontada)));
    // Empate tras perder por tres, no solo después de haber recibido tres.
    t=1000;e.start(c,t);t+=3000;e.tick(t);for(int i=0;i<3;i++)goal(e,t,rival);
    for(int i=0;i<2;i++){goal(e,t,team);}
    b=goal(e,t,team);assert(goalMask(e,b,team)&(1u<<EmpateTras3));
    // Anular reconstruye el déficit: un gol invalidado no inventa una remontada de tres.
    t=1000;e.start(c,t);t+=3000;e.tick(t);for(int i=0;i<3;i++)goal(e,t,rival);
    t+=3100;e.undo(t);goal(e,t,team);b=goal(e,t,team);assert(!(goalMask(e,b,team)&(1u<<EmpateTras3)));
    // Más de tres significa cuatro o más DESPUÉS de sumar, no antes.
    t=1000;e.start(c,t);t+=3000;e.tick(t);for(int i=0;i<5;i++)goal(e,t,rival);
    b=goal(e,t,team);assert(goalMask(e,b,team)&(1u<<Pierde4));b=goal(e,t,team);assert(!(goalMask(e,b,team)&(1u<<Pierde4)));
  }
  // Último minuto: borde inclusivo 60s y solo segunda parte/prórroga, sin tiempo por goles.
  for(uint32_t remain:{60001u,60000u}){
    c.minutesPerPeriod=2;t=1000;e.start(c,t);t+=3000;e.tick(t);for(int i=0;i<6;i++)goal(e,t,Team::Blue);
    auto b=goal(e,t,Team::White);assert(!(goalMask(e,b,Team::White)&(1u<<Pierde4Ultimo60)));
    e.tick(124000);e.next(125000);e.tick(128000);t=128000+120000-remain;e.tick(t);b=mfv3snd::before(e);e.goal(Team::White,t);
    assert(bool(goalMask(e,b,Team::White)&(1u<<Pierde4Ultimo60))==(remain==60000));
  }
  // El índice más alto cruza persistencia y sketch completos, no solo goalMask().
  reset();Catalog top;assert(top.add(Pierde4,0x1003));uint8_t rowBytes[5];row(Pierde4,0x1003,rowBytes);
  assert(save(sndStore,top,mfv3snd::crc32Update(0,rowBytes,5)));catalog.clear();load(sndStore);
  assert(catalog.n==1 && catalog.counts[Pierde4]==1 && !catalog.counts[Intro]);
  cfg.minutesPerPeriod=10;startMatch();sim::now+=3000;loop();
  for(int i=0;i<5;i++){sim::now+=3100;loop();inputGoal(Team::Blue);}
  sim::now+=3100;loop();inputGoal(Team::White);
  assert(pendingCommentary==0x1003 && (catalog.started&(1u<<Pierde4)));
  puts("Eventos 0.11: intro sin retraso, gol/anulacion prioritarios, descanso/final unicos, victoria inmediata y contextos OK");
}
