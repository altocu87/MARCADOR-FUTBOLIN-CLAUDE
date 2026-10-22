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
  // Capacidad por el volumen de SD, no por el disco 0 (FFat). Preservar más de 4 GB.
  sim::sdFatTotal=32ull<<30;sim::sdFatFree=(32ull<<30)-123456;
  uint64_t total,free;assert(sndStore.sdSpace(total,free));assert(total==(32ull<<30) && free==(32ull<<30)-123456);
  assert(sim::sdFatPath=="/sdcard" && total!=SD_MMC.totalBytes());
  assert(sndStore.freeKB(mfv3snd::Place::Sd)==free/1024);
  sim::out.clear();handleBoardLine("SDINFO?");assert(sim::out.find("total=34359738368 usado=123456 libre=34359614912")!=std::string::npos);
  sim::sdFatInfoOk=false;assert(!sndStore.sdSpace(total,free) && !total && !free);
  sim::out.clear();handleBoardLine("SDINFO?");assert(sim::out.find("disponible=0")!=std::string::npos);
  assert(!sndStore.freeKB(mfv3snd::Place::Sd));sim::sdFatInfoOk=true;
  sim::sdFatFree=sim::sdFatTotal+1;assert(!sndStore.sdSpace(total,free));
  sim::sdFatTotal=8ull<<30;sim::sdFatFree=(8ull<<30)-(1ull<<20);
  mfv3::Engine e;uint32_t t;fresh(e,t);goal(e,t,Team::White);goal(e,t,Team::White);auto b=goal(e,t,Team::White);
  assert(goalMask(e,b,Team::White)==((1u<<Comodin)|(1u<<Ventaja2))); // marcar tu tercero no significa haber encajado tres
  fresh(e,t);goal(e,t,Team::Blue);goal(e,t,Team::Blue);goal(e,t,Team::Blue);b=goal(e,t,Team::White);
  assert(goalMask(e,b,Team::White)&(1u<<Encajados3));
  fresh(e,t);goal(e,t,Team::White);goal(e,t,Team::White);goal(e,t,Team::White);b=goal(e,t,Team::Blue);
  assert(goalMask(e,b,Team::Blue)&(1u<<Encajados3));
  // Tres recibidos no tienen que ser consecutivos.
  fresh(e,t);goal(e,t,Team::Blue);goal(e,t,Team::White);goal(e,t,Team::Blue);goal(e,t,Team::White);goal(e,t,Team::Blue);b=goal(e,t,Team::White);
  assert((goalMask(e,b,Team::White)&(1u<<Encajados3)) && !(goalMask(e,b,Team::White)&(1u<<Respuesta2)));
  fresh(e,t);goal(e,t,Team::Blue);b=goal(e,t,Team::White);assert(goalMask(e,b,Team::White)==((1u<<Comodin)|(1u<<Empate)));
  fresh(e,t);goal(e,t,Team::Blue);goal(e,t,Team::Blue);b=goal(e,t,Team::White);assert(goalMask(e,b,Team::White)&(1u<<Respuesta2));
  // No exige ir ganando (1-2) ni basta con dos goles no consecutivos.
  fresh(e,t);goal(e,t,Team::Blue);goal(e,t,Team::White);goal(e,t,Team::Blue);b=goal(e,t,Team::White);assert(!(goalMask(e,b,Team::White)&(1u<<Respuesta2)));
  b.phase=Phase::Penalties;assert(!goalMask(e,b,Team::White));
  fresh(e,t);goal(e,t,Team::Blue);goal(e,t,Team::Blue);t+=3100;e.undo(t);b=goal(e,t,Team::White);assert(!(goalMask(e,b,Team::White)&(1u<<Respuesta2)));
  // Empate con DOS goles previos dentro de una ventana inclusiva de 60 s de juego.
  for(unsigned finalAt:{70000u,70001u}){
    fresh(e,t);
    for(auto at:{7000u,10000u,20000u}){e.tick(at);assert(e.goal(at==10000?Team::Blue:Team::White,at)==mfv3::Result::Accepted);}
    e.tick(finalAt);b=mfv3snd::before(e);assert(e.goal(Team::Blue,finalAt)==mfv3::Result::Accepted);
    auto mask=goalMask(e,b,Team::Blue);
    assert(mask&(1u<<Empate));assert(bool(mask&(1u<<EmpateMovido))==(finalAt==70000));
  }
  fresh(e,t);goal(e,t,Team::White);b=goal(e,t,Team::Blue);
  assert(!(goalMask(e,b,Team::Blue)&(1u<<EmpateMovido))); // empate actual no cuenta como previo
  // El registro de un gol anulado no completa los dos anteriores válidos.
  fresh(e,t);goal(e,t,Team::White);goal(e,t,Team::Blue);t+=3100;assert(e.undo(t)==mfv3::Result::Accepted);
  b=goal(e,t,Team::Blue);assert(!(goalMask(e,b,Team::Blue)&(1u<<EmpateMovido)));
  fresh(e,t);goal(e,t,Team::White);goal(e,t,Team::Blue);b=goal(e,t,Team::White);
  assert(!(goalMask(e,b,Team::White)&(1u<<EmpateMovido))); // los goles rápidos sin empate no bastan
  // Las pausas no consumen la ventana; sí se suman las partes cerradas.
  fresh(e,t);goal(e,t,Team::White);goal(e,t,Team::Blue);goal(e,t,Team::White);
  e.pause(t);t+=120000;e.resume(t);b=goal(e,t,Team::Blue);assert(goalMask(e,b,Team::Blue)&(1u<<EmpateMovido));
  mfv3::Config one;one.endCondition=mfv3::EndCondition::Time;one.minutesPerPeriod=1;t=1000;e.start(one,t);e.tick(4000);
  e.tick(40000);e.goal(Team::White,40000);e.tick(50000);e.goal(Team::Blue,50000);e.tick(60000);e.goal(Team::White,60000);
  e.tick(64000);assert(e.phase()==Phase::PeriodEnd);e.next(65000);e.tick(69000);
  b=mfv3snd::before(e);assert(e.goal(Team::Blue,72000)==mfv3::Result::Accepted);assert(goalMask(e,b,Team::Blue)&(1u<<EmpateMovido));
  { // Variedad: rondas completas por grupo, independientes y sin repetir en el borde.
  static_assert(sizeof(Catalog)<=MAX_FILES*4+PoolCount*4+8,"No añadir otro vector para las bolsas");
  Catalog bag;for(unsigned i=1;i<=46;i++)assert(bag.add(Comodin,i));
  for(unsigned i=0;i<3;i++)assert(bag.add(Empate,100+i));
  uint32_t rng=0x81234567;auto random32=[&](){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;};
  uint32_t prev=0;
  for(unsigned round=0;round<12;round++){
    std::set<uint32_t> used;
    for(unsigned i=0;i<46;i++){
      auto code=bag.pick(1u<<Comodin,random32(),random32());
      assert(code>=1 && code<=46 && code!=prev && used.insert(code).second);prev=code;
      // Intercalar otro grupo no reinicia ni consume la bolsa comodín.
      if(i%4==0)assert(bag.pick(1u<<Empate,random32(),random32())>=100);
    }
    assert(used.size()==46);
  }
  // Límites: 1 frase inevitablemente repite; 2 alternan incluso entre rondas.
  Catalog one;assert(one.add(Comodin,7));for(unsigned i=0;i<50;i++)assert(one.pick(1,random32(),random32())==7);
  Catalog two;assert(two.add(Empate,11));assert(two.add(Empate,12));prev=0;
  for(unsigned i=0;i<1000;i++){auto code=two.pick(1u<<Empate,random32(),random32());assert(code!=prev);prev=code;}
  // La probabilidad pertenece al grupo, no al número de WAV de ese grupo.
  Catalog mix;for(unsigned i=0;i<46;i++)assert(mix.add(Comodin,1+i));
  for(unsigned i=0;i<3;i++)assert(mix.add(Encajados3,100+i));
  for(unsigned i=0;i<3;i++)assert(mix.add(Empate,200+i));
  unsigned histogram[3]={};
  for(unsigned i=0;i<200000;i++){
    auto code=mix.pick((1u<<Comodin)|(1u<<Encajados3),random32(),random32());histogram[code>=100]++;
  }
  assert(histogram[0]>59000 && histogram[0]<61000 && histogram[1]>139000 && histogram[1]<141000);
  memset(histogram,0,sizeof histogram);
  for(unsigned i=0;i<200000;i++){
    auto code=mix.pick((1u<<Comodin)|(1u<<Encajados3)|(1u<<Empate),random32(),random32());histogram[code>=200?2:code>=100?1:0]++;
  }
  assert(histogram[0]>59000 && histogram[0]<61000);
  assert(histogram[1]>69000 && histogram[1]<71000 && histogram[2]>69000 && histogram[2]<71000);
  // Casillas exactas: 70 específicas y 30 comodín en una distribución de entradas uniforme.
  unsigned specific=0;
  for(uint32_t i=0;i<100;i++){
    uint32_t ticket=(uint32_t)((((uint64_t)i<<32)+(1ull<<31))/100);
    specific+=mix.pick((1u<<Comodin)|(1u<<Empate),ticket,random32())>=200;
  }
  assert(specific==70);
  Catalog missing;assert(missing.add(Empate,55));
  for(unsigned i=0;i<100;i++)assert(missing.pick((1u<<Comodin)|(1u<<Empate),random32(),random32())==55);
  assert(!missing.pick(1u<<Comodin,0,0));
  assert(!missing.pick((1u<<Anulado)|(1u<<Empate),0,0)); // sin anulación: no narrar un gol
  assert(missing.add(Anulado,66));
  for(unsigned i=0;i<100;i++)assert(missing.pick((1u<<Anulado)|(1u<<Empate)|(1u<<Comodin),random32(),random32())==66);
  Catalog full;for(unsigned i=1;i<=MAX_FILES;i++)assert(full.add(Comodin,1000+i));
  prev=0;
  for(unsigned round=0;round<3;round++){
    std::set<uint32_t> used;
    for(unsigned i=0;i<MAX_FILES;i++){
      // Incluir extremos de 32 bits comprueba los límites del escalado y del intercambio.
      uint32_t sample=i==0?0:i==1?UINT32_MAX:random32();
      auto code=full.pick(1u<<Comodin,random32(),sample);
      assert(code>=1001 && code<=1000+MAX_FILES && code!=prev && used.insert(code).second);prev=code;
    }
    assert(used.size()==MAX_FILES && full.remaining[Comodin]==0 && full.n==MAX_FILES);
  }
  // clear/reinstalación reinician las bolsas; los códigos persistidos siguen siendo CMT1.
  bag.clear();assert(!bag.n && !bag.started);for(auto left:bag.remaining)assert(!left);
  assert(bag.add(Comodin,1) && bag.add(Comodin,2));assert(bag.pick(1,0,0)==1);
  printf("Variedad: 70/30 y 30/35/35, rondas sin repeticiones, bordes y grupos ausentes OK; Catalog=%zu B\n",sizeof(Catalog));

  }
  // Ambos equipos y límites tras el gol: perder, empatar, ganar por uno/dos/tres.
  for(Team team:{Team::White,Team::Blue}){
    Team opponent=team==Team::White?Team::Blue:Team::White;
    fresh(e,t);goal(e,t,opponent);goal(e,t,opponent);b=goal(e,t,team);
    assert(goalMask(e,b,team)&(1u<<SiguePerdiendo));
    b=goal(e,t,team);assert(!(goalMask(e,b,team)&((1u<<SiguePerdiendo)|(1u<<Ventaja2))));
    b=goal(e,t,team);assert(!(goalMask(e,b,team)&(1u<<Ventaja2)));
    b=goal(e,t,team);assert(goalMask(e,b,team)&(1u<<Ventaja2));
    b=goal(e,t,team);assert(goalMask(e,b,team)&(1u<<Ventaja2));
    assert(!(goalMask(e,b,team)&(1u<<SiguePerdiendo)));
  }
  {Catalog extended;assert(extended.add(SiguePerdiendo,7001));assert(extended.add(Ventaja2,8001));assert(extended.add(Ventaja2,8002));
   uint32_t last=0;for(unsigned i=0;i<50;i++){auto code=extended.pick(1u<<Ventaja2,UINT32_MAX,UINT32_MAX);assert(code>=8001 && code<=8002 && code!=last);last=code;}
   assert(extended.pick(1u<<SiguePerdiendo,0,0)==7001);}
  for(Team team:{Team::White,Team::Blue}){
    fresh(e,t);
    for(unsigned i=1;i<=10;i++){
      b=goal(e,t,team);assert(bool(goalMask(e,b,team)&(1u<<MasDe8))==(i>8));
    }
  }
  {Catalog high;assert(high.add(MasDe8,9001));assert(high.add(MasDe8,9002));uint32_t last=0;
   for(unsigned i=0;i<50;i++){auto code=high.pick(1u<<MasDe8,UINT32_MAX,UINT32_MAX);assert(code>=9001 && code<=9002 && code!=last);last=code;}}
  Catalog c;
  for(unsigned p=0;p<4;p++)for(unsigned i=0;i<200;i++)assert(c.add(p,1+p*200+i));
  assert(c.n==800);std::set<unsigned> seen;uint32_t random=123;
  auto started=std::chrono::steady_clock::now();
  for(unsigned i=0;i<100000;i++){random=random*1664525u+1013904223u;uint32_t poolRandom=random;random=random*1664525u+1013904223u;auto code=c.pick(15,poolRandom,random);assert(code>=1 && code<=800);seen.insert((code-1)/200);}
  assert(seen.size()==4);printf("100000 elecciones/800 archivos: %lld us en PC (sin operaciones de SD)\n",(long long)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());
  uint32_t last=0;for(unsigned i=0;i<1000;i++){auto code=c.pick(1,0,i);assert(code!=last);last=code;}
  for(unsigned i=800;i<MAX_FILES;i++)assert(c.add(4,i+1));
  assert(!c.add(4,1025) && !c.add(4,100));
  Catalog empty;assert(!empty.pick(31,0,0));assert(!empty.add(9,1));
  SD_MMC.disk.files["/sonidos/00001001.wav"]=wav(1111); // compatibilidad con carpeta antigua
  SD_MMC.mkdir("/sonidos/00");SD_MMC.disk.files["/sonidos/00/00001002.wav"]=wav(2222);
  auto crc=putCatalog({{0,0x1001},{2,0x1002}});commit(crc);assert(replies.back()=="COMENTARIOS_GUARDADO" && catalog.n==2);
  auto original=SD_MMC.disk.files[CAT];catalog=Catalog();load(sndStore);assert(catalog.n==2 && catalog.counts[2]==1);
  // El bloque data cabe, pero RIFF declara bytes inexistentes: el reproductor lo rechaza.
  // Catálogo e instalación deben aplicar exactamente la misma validación.
  auto validSecond=SD_MMC.disk.files["/sonidos/00/00001002.wav"];
  auto oversizedRiff=validSecond;uint32_t declared=(uint32_t)oversizedRiff.size()+100;
  for(unsigned i=0;i<4;i++)oversizedRiff[4+i]=(uint8_t)(declared>>(8*i));
  mfv3snd::WavInfo parsed;assert(mfv3snd::parseWav(oversizedRiff.data(),256,parsed));
  assert((uint64_t)parsed.dataOffset+parsed.dataBytes<=oversizedRiff.size());
  SD_MMC.disk.files["/sonidos/00/00001002.wav"]=oversizedRiff;
  assert(!validAudio(sndStore,0x1002));
  crc=putCatalog({{0,0x1002}});commit(crc);
  assert(replies.back().find("ERROR")!=std::string::npos && catalog.n==2 && SD_MMC.disk.files[CAT]==original);
  catalog.clear();load(sndStore);assert(catalog.n==1 && !catalog.counts[2]);
  SD_MMC.disk.files["/sonidos/00/00001002.wav"]=validSecond;
  catalog.clear();load(sndStore);assert(catalog.n==2 && catalog.counts[2]==1);
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
  // Pool añadido al final: persistencia y consulta conservan el índice 4 de anulación.
  SD_MMC.disk.files["/sonidos/00001003.wav"]=wav(3333);
  crc=putCatalog({{5,0x1003}});commit(crc);assert(replies.back()=="COMENTARIOS_GUARDADO");
  catalog.clear();load(sndStore);assert(catalog.n==1 && catalog.counts[5]==1 && !catalog.counts[4]);
  assert(catalog.pick(1u<<EmpateMovido,0,0)==0x1003);
  command("COMENTARIOS?");assert(replies.back().find("grupos=0,0,0,0,0,1")!=std::string::npos);
  // Catálogo nuevo carga pools 6/7; índices 0-5 y CMT1 se conservan.
  crc=putCatalog({{6,0x1001},{7,0x1003}});commit(crc);assert(replies.back()=="COMENTARIOS_GUARDADO");
  catalog.clear();load(sndStore);assert(catalog.n==2 && catalog.counts[6]==1 && catalog.counts[7]==1);
  command("COMENTARIOS?");assert(replies.back().find("grupos=0,0,0,0,0,0,1,1")!=std::string::npos);
  // Bit 8 cruza el sketch hasta el comentario pendiente: no truncar máscaras a 8 bits.
  catalog.clear();assert(catalog.add(MasDe8,0x1003));sndChoices=mfv3snd::Choices();
  cfg=mfv3::Config();cfg.endCondition=mfv3::EndCondition::Time;cfg.minutesPerPeriod=10;
  startMatch();sim::now+=4000;loop();
  for(unsigned i=1;i<=9;i++){
    sim::now+=3100;loop();inputGoal(Team::White);
    assert(eng.score().white==i);assert((pendingCommentary==0x1003)==(i>8));
  }
  assert(catalog.started&(1u<<MasDe8));goHome();
  player.play({false,0x1001,{nullptr,0}},false);player.pump(millis());sim::failI2sStop=true;
  player.play({false,0x1002,{nullptr,0}},false);assert(!player.ok && !(sim::exioOut&(1u<<exio::SPEAKER_AMP)));
  sim::failI2sStop=false;
  puts("Comentarios: 800 WAV indexados/1024, contextos, mezcla, persistencia/CRC, ausentes, DMA, marcador y anulacion OK");
}
