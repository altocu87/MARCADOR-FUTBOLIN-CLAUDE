// Pruebas del catálogo, persistencia, selección y coordinación reales, sin placa física.
#include <cassert>
#include <chrono>
#include <set>
#include <string>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"
static std::vector<std::string> replies;
static void reply(const char* s){replies.emplace_back(s);}
static void command(const std::string& s,bool busy=false){assert(comentarios::handle(s.c_str(),sndStore,busy,reply));}
static std::vector<uint8_t> wav(int16_t sample){
  std::vector<uint8_t> v(44+1600*2);
  auto u32=[&](unsigned at,uint32_t n){for(int j=0;j<4;j++)v[at+j]=(uint8_t)(n>>(8*j));};
  memcpy(v.data(),"RIFF",4);u32(4,v.size()-8);memcpy(v.data()+8,"WAVEfmt ",8);u32(16,16);u32(20,1|(1u<<16));u32(24,16000);u32(28,32000);u32(32,2|(16u<<16));memcpy(v.data()+36,"data",4);u32(40,3200);
  for(size_t i=44;i<v.size();i+=2){v[i]=(uint8_t)sample;v[i+1]=(uint8_t)(sample>>8);}return v;
}
static void fresh(mfv3::Engine& e,uint32_t& t){mfv3::Config c;c.endCondition=mfv3::EndCondition::Time;c.minutesPerPeriod=10;t=1000;assert(e.start(c,t));t+=4000;e.tick(t);}
static mfv3snd::Before goal(mfv3::Engine& e,uint32_t& t,Team team){t+=3100;e.tick(t);auto b=mfv3snd::before(e);assert(e.goal(team,t)==mfv3::Result::Accepted);return b;}
static uint32_t putCatalog(const std::vector<std::pair<unsigned,uint32_t>>& rows){
  command("COMENTARIOS_INICIO "+std::to_string(rows.size()));assert(replies.back()=="COMENTARIOS_LISTO");uint32_t crc=0;
  for(auto p:rows){char line[90];snprintf(line,sizeof line,"COMENTARIOS_ITEM %u %08lx",p.first,(unsigned long)p.second);command(line);assert(replies.back()=="COMENTARIOS_ITEM_OK");uint8_t b[5];comentarios::row(p.first,p.second,b);crc=mfv3snd::crc32Update(crc,b,5);}
  return crc;
}
static void commit(uint32_t crc){char line[60];snprintf(line,sizeof line,"COMENTARIOS_FIN %08lx",(unsigned long)crc);command(line);}
int main(){
  using namespace comentarios;
  sim::sdCard=true;sim::now=1000;setup();test.active=false;nube::phase=nube::OFF;goHome();
  mfv3::Engine e;uint32_t t;fresh(e,t);goal(e,t,Team::White);goal(e,t,Team::White);auto b=goal(e,t,Team::White);
  assert(goalMask(e,b,Team::White)==((1u<<Comodin)|(1u<<Encajados3)));
  fresh(e,t);goal(e,t,Team::Blue);b=goal(e,t,Team::White);assert(goalMask(e,b,Team::White)==((1u<<Comodin)|(1u<<Empate)));
  fresh(e,t);goal(e,t,Team::Blue);goal(e,t,Team::Blue);b=goal(e,t,Team::White);assert(goalMask(e,b,Team::White)&(1u<<Respuesta2));
  // No exige ir ganando (1-2) ni basta con dos goles no consecutivos.
  fresh(e,t);goal(e,t,Team::Blue);goal(e,t,Team::White);goal(e,t,Team::Blue);b=goal(e,t,Team::White);assert(!(goalMask(e,b,Team::White)&(1u<<Respuesta2)));
  b.phase=Phase::Penalties;assert(!goalMask(e,b,Team::White));
  fresh(e,t);goal(e,t,Team::Blue);goal(e,t,Team::Blue);t+=3100;e.undo(t);b=goal(e,t,Team::White);assert(!(goalMask(e,b,Team::White)&(1u<<Respuesta2)));
  Catalog c;
  for(unsigned p=0;p<4;p++)for(unsigned i=0;i<200;i++)assert(c.add(p,1+p*200+i));
  assert(c.n==800);std::set<unsigned> seen;uint32_t random=123;
  auto started=std::chrono::steady_clock::now();
  for(unsigned i=0;i<100000;i++){random=random*1664525u+1013904223u;auto code=c.pick(15,random>>12,random);assert(code>=1 && code<=800);seen.insert((code-1)/200);}
  assert(seen.size()==4);printf("100000 elecciones/800 archivos: %lld us en PC (sin operaciones de SD)\n",(long long)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());
  uint32_t last=0;for(unsigned i=0;i<1000;i++){auto code=c.pick(1,0,i);assert(code!=last);last=code;}
  for(unsigned i=800;i<MAX_FILES;i++)assert(c.add(4,i+1));
  assert(!c.add(4,1025) && !c.add(4,100));
  Catalog empty;assert(!empty.pick(31,0,0));assert(!empty.add(9,1));
  SD_MMC.disk.files["/sonidos/00001001.wav"]=wav(1111); // compatibilidad con carpeta antigua
  SD_MMC.mkdir("/sonidos/00");SD_MMC.disk.files["/sonidos/00/00001002.wav"]=wav(2222);
  auto crc=putCatalog({{0,0x1001},{2,0x1002}});commit(crc);assert(replies.back()=="COMENTARIOS_GUARDADO" && catalog.n==2);
  auto original=SD_MMC.disk.files[CAT];catalog=Catalog();load(sndStore);assert(catalog.n==2 && catalog.counts[2]==1);
  crc=putCatalog({{0,0x1001}});commit(crc^1);assert(replies.back().find("ERROR")!=std::string::npos && catalog.n==2 && SD_MMC.disk.files[CAT]==original);
  command("COMENTARIOS_INICIO 1025");assert(replies.back()=="COMENTARIOS_ERROR cantidad");
  command("COMENTARIOS_INICIO 2");command("COMENTARIOS_ITEM 0 00001001");command("COMENTARIOS_ITEM 0 00001001");assert(replies.back()=="COMENTARIOS_ERROR item");command("COMENTARIOS_CANCELAR");
  crc=putCatalog({{0,0x9999}});commit(crc);assert(replies.back().find("ERROR")!=std::string::npos && catalog.n==2);
  command("COMENTARIOS_INICIO 1",true);assert(!staging && replies.back().find("ERROR")!=std::string::npos);
  command("COMENTARIOS_INICIO 1");tick(millis()+30001,false);assert(!staging);
  crc=putCatalog({{0,0x1001}});commit(crc);assert(catalog.n==1 && SD_MMC.disk.files.count(BAK));
  SD_MMC.disk.files[CAT][0]='X';catalog=Catalog();load(sndStore);assert(catalog.n==2); // recupera copia previa
  SD_MMC.disk.files.erase("/sonidos/00/00001002.wav");catalog=Catalog();load(sndStore);assert(catalog.n==1); // excluye fichero perdido en arranque
  // La sustitución fallida conserva la copia recuperable, incluso con el principal corrupto.
  auto backup=SD_MMC.disk.files[BAK];
  SD_MMC.disk.failRenameTo=CAT;
  crc=putCatalog({{0,0x1001}});commit(crc);
  assert(replies.back().find("ERROR")!=std::string::npos && SD_MMC.disk.files[BAK]==backup);
  SD_MMC.disk.failRenameTo.clear();catalog.clear();load(sndStore);assert(catalog.n==1); // el segundo WAV sigue ausente
  // DMA: sustituir pista detiene el canal y precarga únicamente audio nuevo.
  SD_MMC.disk.files["/sonidos/00001002.wav"]=wav(2222);sim::i2sRoom=1000000;
  player.play({false,0x1001,{nullptr,0}},false);player.pump(millis());sim::heard.clear();auto stops=sim::i2sStops;
  player.play({false,0x1002,{nullptr,0}},false);player.pump(millis());assert(sim::i2sStops>stops);
  for(auto v:sim::heard)assert(v==2222 || v==0);
  catalog=Catalog();assert(catalog.add(0,0x1001));sndChoices=mfv3snd::Choices();
  cfg=mfv3::Config();cfg.endCondition=mfv3::EndCondition::Time;cfg.minutesPerPeriod=1;startMatch();sim::now+=4000;loop();assert(eng.phase()==Phase::Playing);
  player.stop();sim::heard.clear();inputGoal(Team::White);assert(eng.score().white==1 && pendingCommentary==0x1001 && sim::heard.empty());
  uint32_t frame=uiport::presented;
  for(int i=0;i<20 && pendingCommentary;i++){sim::now+=35;loop();}
  assert(!pendingCommentary && uiport::presented!=frame && !sim::heard.empty());
  stops=sim::i2sStops;inputAnnul(Team::White);assert(eng.score().white==0 && !pendingCommentary && sim::i2sStops>stops);
  // Las preferencias explícitas siguen teniendo prioridad sobre el catálogo automático.
  sndChoices.c[(uint8_t)mfv3snd::Event::Goal].kind=mfv3snd::Kind::None;
  sim::now+=3100;inputGoal(Team::Blue);assert(!pendingCommentary);
  sndChoices=mfv3snd::Choices();
  // Si la SD pierde el archivo entre la indexación y el gol, queda un tono de respaldo.
  sim::now+=3100;catalog.clear();assert(catalog.add(0,0x9999));inputGoal(Team::Blue);
  for(int i=0;i<20 && pendingCommentary;i++){sim::now+=35;loop();}
  assert(!pendingCommentary && player.busy());
  // El listado nuevo alcanza más de 48, sin cambiar SONIDOS? de clientes anteriores.
  goHome();for(unsigned i=1;i<=800;i++){char path[70];snprintf(path,sizeof path,"/sonidos/%08x.wav",0x80000000u+i);SD_MMC.disk.files[path]={1};}
  sim::out.clear();handleBoardLine("SONIDOS_PAGINA 25");assert(sim::out.find("80000320@s")!=std::string::npos);
  // Arranque con un catálogo de 800 WAV válidos, no solo un vector de referencias.
  std::vector<std::pair<unsigned,uint32_t>> many;
  for(unsigned i=1;i<=800;i++){
    uint32_t code=0x90000000u+i;char path[70];snprintf(path,sizeof path,"/sonidos/%08x.wav",code);
    SD_MMC.disk.files[path]=wav(1234);many.push_back({(i-1)/200,code});
  }
  crc=putCatalog(many);commit(crc);assert(catalog.n==800 && replies.back()=="COMENTARIOS_GUARDADO");
  catalog.clear();load(sndStore);assert(catalog.n==800 && catalog.counts[0]==200 && catalog.counts[3]==200);
  player.play({false,0x1001,{nullptr,0}},false);player.pump(millis());sim::failI2sStop=true;
  player.play({false,0x1002,{nullptr,0}},false);assert(!player.ok && !(sim::exioOut&(1u<<exio::SPEAKER_AMP)));
  sim::failI2sStop=false;
  puts("Comentarios: 800 WAV indexados/1024, contextos, mezcla, persistencia/CRC, ausentes, DMA, marcador y anulacion OK");
}
