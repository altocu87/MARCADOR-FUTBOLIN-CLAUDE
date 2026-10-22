// Integración real de sonidos de Partido Loco: motor público, .ino y WAV sintéticos.
#include <cassert>
#include <cstdio>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static const uint32_t GENERIC=0x1101,CONTEXT=0x2202,ANNUL=0x4404,AWARDED=0x6606;
static const int16_t GENERIC_SAMPLE=1101,CONTEXT_SAMPLE=2202,ANNUL_SAMPLE=4404,AWARDED_SAMPLE=6606;
struct Step { mfv3::Engine before; uint32_t at; };

static std::vector<uint8_t> wave(int16_t sample) {
  const unsigned frames=8000;
  std::vector<uint8_t> d(44+frames*2);
  auto u32=[&](unsigned p,uint32_t n){for(unsigned i=0;i<4;i++)d[p+i]=(uint8_t)(n>>(8*i));};
  memcpy(d.data(),"RIFF",4);u32(4,(uint32_t)d.size()-8);memcpy(d.data()+8,"WAVEfmt ",8);
  u32(16,16);u32(20,1|(1u<<16));u32(24,16000);u32(28,32000);u32(32,2|(16u<<16));
  memcpy(d.data()+36,"data",4);u32(40,frames*2);
  for(size_t p=44;p<d.size();p+=2){d[p]=(uint8_t)sample;d[p+1]=(uint8_t)(sample>>8);}
  return d;
}
static mfv3::Engine initial(uint32_t seed) {
  mfv3::Config c;c.chaos=true;c.endCondition=mfv3::EndCondition::Time;c.minutesPerPeriod=30;c.goalsPerPeriod=20;
  mfv3::Engine e;assert(e.start(c,1000));assert(e.skipCountdown(1000)==mfv3::Result::Accepted);
  assert(e.goal(Team::White,4500)==mfv3::Result::Accepted);
  assert(e.goal(Team::White,8000)==mfv3::Result::Accepted);
  assert(e.goal(Team::Blue,11500)==mfv3::Result::Accepted);
  e.seed(seed);return e;
}
// Búsqueda solo sobre el motor: no renderiza ni crea archivos durante los intentos.
static Step animalStep(mfv3::Animal kind,int team) {
  for(uint32_t seed=1;seed<=500;seed++){
    auto e=initial(seed);
    for(uint32_t t=12000;t<=600000;t+=500){
      auto before=e;e.tick(t);
      if(e.phase()==Phase::Visit && e.visitApplied() && !before.visitApplied() && before.phase()==Phase::Visit &&
         e.visitor().animal==kind && (kind==mfv3::Animal::Squirrel?e.visitor().from:e.visitor().to)==team)
        return Step{before,t};
      if(e.phase()==Phase::Handicap && e.handicapStage()==mfv3::HStage::Announce){
        if(e.handicap().kind==mfv3::HKind::Penalty)e.handicapPenalty(false,t);
        else e.handicapGo(t);
      }
    }
  }
  assert(false && "No se encontró visita con semillas públicas");return Step{};
}
static Step penaltyStep() {
  for(uint32_t seed=1;seed<=500;seed++){
    auto before=initial(seed),e=before;e.tick(50000);
    if(e.phase()==Phase::Handicap && e.handicapStage()==mfv3::HStage::Announce && e.handicap().kind==mfv3::HKind::Penalty)
      return Step{before,50000};
  }
  assert(false && "No se encontró penalti con semillas públicas");return Step{};
}
static void bind(const Step& step) {
  player.stop();sim::heard.clear();sim::i2sRoom=1000000;
  queueCommentary(0);sndChoices=mfv3snd::Choices();eng=step.before;cfg=eng.config();
  inMatch=true;chosenMode=inicio::CHAOS;haveTeams=false;confirmAbandon=false;
  screen=Screen::Home;hcIntroUntil=0;hcSoundSeq=0;resultPending=false;
  sim::now=step.at;snap=takeSnapshot();sndBefore=mfv3snd::before(eng);lastKey=0xffffffff;
}
static void startPending(uint32_t expected,int16_t sample) {
  assert(pendingCommentary==expected && sim::heard.empty());
  for(unsigned i=0;i<20 && pendingCommentary;i++){sim::now+=35;loop();}
  assert(!pendingCommentary && commentaryLastCode==expected && !sim::heard.empty());
  bool heard=false;for(auto v:sim::heard){assert(v==sample || !v);heard=heard || v==sample;}assert(heard);
}
static void noNewSound() {
  assert(!pendingCommentary && !player.busy());
  player.pump(millis());assert(sim::heard.empty());
}
static void testAnimal(mfv3::Animal kind,int team) {
  Step step=animalStep(kind,team);bind(step);auto before=eng.score();
  assert(eng.tick(step.at));afterChange(false,Team::White);auto after=eng.score();
  if(kind==mfv3::Animal::Cat){
    assert(after.of((Team)team)==before.of((Team)team)+1);
    startPending(GENERIC,GENERIC_SAMPLE); // no empate/racha/contexto de un animal
  }else{
    assert(after.of((Team)team)+1==before.of((Team)team));
    assert(after.of(mfv3::other((Team)team))==before.of(mfv3::other((Team)team))+1);
    startPending(ANNUL,ANNUL_SAMPLE); // transferencia mixta: un solo aviso, prioridad anulación
  }
  player.stop();sim::heard.clear();afterChange(false,Team::White);noNewSound();
}
static void testPenalty(bool ui,bool scored,mfv3snd::Kind choice) {
  Step step=penaltyStep();bind(step);
  auto& c=sndChoices.c[(uint8_t)mfv3snd::Event::PenaltyAwarded];c.kind=choice;
  if(choice==mfv3snd::Kind::Custom){c.n=1;c.codes[0]=AWARDED;}
  assert(eng.tick(step.at));assert(eng.phase()==Phase::Handicap);
  auto shooter=eng.handicap().team?Team::Blue:Team::White;
  afterChange(false,shooter);assert(!pendingCommentary);
  assert(hcSoundSeq==eng.handicap().seq);
  if(choice==mfv3snd::Kind::None)noNewSound();
  else{
    assert(player.busy());player.pump(millis());assert(!sim::heard.empty());
    if(choice==mfv3snd::Kind::Custom)for(auto v:sim::heard)assert(v==AWARDED_SAMPLE || !v);
    else {bool audible=false;for(auto v:sim::heard)audible=audible || v!=0;assert(audible);}
  }
  // Detener la pista de prueba permite detectar reinicios que antes quedarían tapados por su cola.
  player.stop();sim::heard.clear();
  afterChange(false,shooter);noNewSound();afterChange(false,shooter);noNewSound();
  auto before=eng.score();sim::now+=100;
  if(ui)onHcPenalty(scored);
  else inputButtonGoal(scored?shooter:mfv3::other(shooter));
  assert(eng.phase()==Phase::Playing && eng.handicapStage()==mfv3::HStage::None);
  assert(eng.score().of(shooter)==before.of(shooter)+(scored?1:0));
  noNewSound(); // al resolver no repetir aviso ni narrar un gol; no es la tanda de penaltis
  afterChange(false,shooter);noNewSound();
}
int main() {
  sim::now=1000;sim::sdCard=true;setup();test.active=false;nube::phase=nube::OFF;goHome();
  SD_MMC.disk.files["/sonidos/00001101.wav"]=wave(GENERIC_SAMPLE);
  SD_MMC.disk.files["/sonidos/00002202.wav"]=wave(CONTEXT_SAMPLE);
  SD_MMC.disk.files["/sonidos/00004404.wav"]=wave(ANNUL_SAMPLE);
  SD_MMC.disk.files["/sonidos/00006606.wav"]=wave(AWARDED_SAMPLE);
  comentarios::catalog.clear();
  assert(comentarios::catalog.add(0,GENERIC));assert(comentarios::catalog.add(2,CONTEXT));assert(comentarios::catalog.add(4,ANNUL));
  for(int t=0;t<2;t++){testAnimal(mfv3::Animal::Cat,t);testAnimal(mfv3::Animal::Squirrel,t);}
  for(bool ui:{false,true})for(bool scored:{false,true})testPenalty(ui,scored,mfv3snd::Kind::Custom);
  testPenalty(false,true,mfv3snd::Kind::Default);testPenalty(true,false,mfv3snd::Kind::None);
  goHome();player.stop();
  puts("Sonidos Partido Loco: gato/ardilla de ambos equipos, aviso único y prioridad, penalti señalado una vez, botón/UI y silencio OK");
}
