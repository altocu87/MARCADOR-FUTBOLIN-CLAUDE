// Catálogo local por grupos. Nunca recorre la SD ni los 1024 códigos al elegir una frase.
#pragma once
#include "mfv3_sound.h"
#include <new>
namespace comentarios {
static const uint16_t MAX_FILES = 1024;
enum Pool : uint8_t { Comodin, Encajados3, Empate, Respuesta2, Anulado, EmpateMovido, SiguePerdiendo, Ventaja2, MasDe8, VaGanando, Ultimos20, Intro, Descanso, FinalNormal, Remontada, EmpateTras3, Pierde4Ultimo60, Pierde4, PenaltyMiss, PenaltyGoal, PenaltyEqualizer, PenaltyEqualizer20, Ambience, Opening20, SecondHalf, SecondHalfMany, SecondHalfFew, GoldenWinner, ExtraTimeAnnounce, ShootoutAnnounce, Lead3, ShootoutCanWin, ShootoutMustScore, ShootoutBefore, ShootoutMiss, ShootoutGoal, ShootoutFirst, ShootoutWin, ShootoutChampion, PoolCount };
using PoolMask=uint64_t;
static_assert(PoolCount<=64,"Ampliar las mascaras antes de añadir otro grupo");
struct Catalog {
  uint32_t codes[MAX_FILES] = {};
  uint16_t counts[PoolCount] = {}, n = 0;
  // Fisher-Yates incremental: solo códigos en RAM, sin otro vector ni lecturas SD.
  uint16_t remaining[PoolCount] = {};
  PoolMask started = 0;
  void clear(){
    n=0;
    memset(counts,0,sizeof counts);
    memset(remaining,0,sizeof remaining);started=0;
  }
  uint16_t offset(uint8_t p) const { uint16_t o=0; for(uint8_t i=0;i<p;i++)o+=counts[i];return o; }
  bool add(uint8_t p,uint32_t code) {
    if(p>=PoolCount || !code || n>=MAX_FILES)return false;
    for(uint8_t i=p+1;i<PoolCount;i++)if(counts[i])return false; // transferencia ordenada por grupo
    for(uint16_t i=0;i<n;i++)if(codes[i]==code)return false;
    codes[n++]=code;counts[p]++;remaining[p]=0;started&=~(PoolMask(1)<<p);return true;
  }
  // Escalado de 32 bits: reparto equilibrado sin divisiones ni reintentos de azar.
  static uint16_t index(uint32_t random,uint16_t bound){return (uint16_t)(((uint64_t)random*bound)>>32);}
  uint32_t draw(uint8_t p,uint32_t random){
    uint16_t count=counts[p],at;
    if(!remaining[p]){
      remaining[p]=count;
      // Al agotar una ronda, su última frase está en la posición 0.
      // Excluirla del primer sorteo nuevo evita la repetición entre rondas.
      at=(started&(PoolMask(1)<<p)) && count>1 ? 1+index(random,count-1) : index(random,count);
      started|=(PoolMask(1)<<p);
    }else at=index(random,remaining[p]);
    uint16_t base=offset(p),last=--remaining[p];uint32_t code=codes[base+at];
    codes[base+at]=codes[base+last];codes[base+last]=code;
    return code;
  }
  uint32_t pick(PoolMask mask,uint32_t poolRandom,uint32_t clipRandom,uint8_t genericPool=Comodin) {
    if(genericPool>=PoolCount)return 0;
    // Anulación nunca se mezcla con voces de gol, incluso ante una máscara errónea.
    if(mask&(PoolMask(1)<<Anulado))return counts[Anulado]?draw(Anulado,clipRandom):0;
    uint8_t pools[PoolCount],k=0;
    for(uint8_t p=0;p<PoolCount;p++)if(p!=genericPool && (mask&(PoolMask(1)<<p)) && counts[p])pools[k++]=p;
    bool generic=(mask&(PoolMask(1)<<genericPool)) && counts[genericPool];
    if(!k)return generic?draw(genericPool,clipRandom):0;
    uint8_t p;
    if(generic){
      // 7*k casillas específicas y 3*k comodín: 70/30 total, también con varios contextos.
      uint16_t ticket=index(poolRandom,10*k);
      p=ticket<7*k?pools[ticket%k]:genericPool;
    }else p=pools[index(poolRandom,k)];
    return draw(p,clipRandom);
  }
};
inline uint32_t goalTime(const mfv3::Engine& e,const mfv3::Goal& g){
  uint32_t time=g.periodTimeMs;
  for(uint8_t p=0;p<(uint8_t)g.period && p<3;p++)time+=e.periodDuration((mfv3::Period)p);
  return time;
}
inline bool twoRecentGoals(const mfv3::Engine& e){
  int current=(int)e.goalCount()-1;
  while(current>=0 && e.goalAt((uint8_t)current).annulled)current--;
  if(current<0)return false;
  uint32_t at=goalTime(e,e.goalAt((uint8_t)current));
  unsigned count=0;
  for(int i=current-1;i>=0;i--){const auto& g=e.goalAt((uint8_t)i);
    if(g.annulled || !g.value)continue;
    uint32_t previous=goalTime(e,g);
    if(previous<=at && at-previous<=60000 && ++count>=2)return true;
  }
  return false;
}
// Usar el instante registrado del gol: un gol ganador ya puede haber parado el reloj.
// Nunca confundir fin de la primera parte ni partidos a goles con el final del partido.
inline bool lastTwentySeconds(const mfv3::Engine& e){
  if(e.config().endCondition==mfv3::EndCondition::Goals || !e.goalCount())return false;
  const auto& g=e.goalAt(e.goalCount()-1);
  if(g.annulled || !g.value || g.period!=e.period() ||
     (g.period!=mfv3::Period::Second && g.period!=mfv3::Period::Overtime))return false;
  uint32_t limit=e.periodLimitMs();
  return limit && g.periodTimeMs<=limit && limit-g.periodTimeMs<=20000;
}
// Reconstruir déficits solo con goles válidos: anular también deshace el contexto.
// Partido Loco conserva la simplificación de animales fuera de estos contextos.
inline unsigned worstDeficit(const mfv3::Engine& e,mfv3::Team team){
  int own=0,other=0;unsigned worst=0;
  for(uint8_t i=0;i<e.goalCount();i++){
    const auto& g=e.goalAt(i);if(g.annulled || !g.value)continue;
    if(g.team==team){own+=g.value;if(g.steal && other>0)other--;}
    else {other+=g.value;if(g.steal && own>0)own--;}
    if(other-own>(int)worst)worst=(unsigned)(other-own);
  }
  return worst;
}
inline bool lastMinute(const mfv3::Engine& e){
  if(e.config().endCondition==mfv3::EndCondition::Goals || !e.goalCount())return false;
  const auto& g=e.goalAt(e.goalCount()-1);uint32_t limit=e.periodLimitMs();
  return !g.annulled && g.value && g.period==e.period() &&
    (g.period==mfv3::Period::Second || g.period==mfv3::Period::Overtime) &&
    limit && g.periodTimeMs<=limit && limit-g.periodTimeMs<=60000;
}
inline PoolMask goalMask(const mfv3::Engine& e,const mfv3snd::Before& b,mfv3::Team t) {
  auto s=e.score();if(b.phase==mfv3::Phase::Penalties || e.phase()==mfv3::Phase::Penalties || s.of(t)<=b.score.of(t))return 0;
  PoolMask mask=PoolMask(1)<<Comodin;
  uint16_t own=s.of(t),other=s.of(t==mfv3::Team::White?mfv3::Team::Blue:mfv3::Team::White);
  if(own>other)mask|=PoolMask(1)<<VaGanando;
  if(own==other+3)mask|=PoolMask(1)<<Lead3; // exactamente tres, confirmado por el dueño
  if(e.phase()==mfv3::Phase::Finished && e.period()==mfv3::Period::Overtime && e.reason()==mfv3::Reason::GoldenGoal)mask|=PoolMask(1)<<GoldenWinner;
  if(lastTwentySeconds(e))mask|=PoolMask(1)<<Ultimos20;
  if(!e.config().chaos){ // no inferir una remontada de las travesuras no registradas como goles
    unsigned worst=worstDeficit(e,t);
    if(own>other && b.score.of(t)<=b.score.of(t==mfv3::Team::White?mfv3::Team::Blue:mfv3::Team::White) && worst)mask|=PoolMask(1)<<Remontada;
    if(own==other && worst>=3)mask|=PoolMask(1)<<EmpateTras3;
  }
  if(other>=own+4){mask|=PoolMask(1)<<Pierde4;if(lastMinute(e))mask|=PoolMask(1)<<Pierde4Ultimo60;}
  if(own<other)mask|=PoolMask(1)<<SiguePerdiendo;
  if(own>8)mask|=PoolMask(1)<<MasDe8;
  if(own>=other+2)mask|=PoolMask(1)<<Ventaja2; // criterio del dueño: DOS o más, tras sumar el gol

  if(b.score.of(t==mfv3::Team::White?mfv3::Team::Blue:mfv3::Team::White)>=3)mask|=PoolMask(1)<<Encajados3; // marca quien ya había encajado al menos tres
  if(s.white==s.blue){
    mask|=PoolMask(1)<<Empate;
    if(twoRecentGoals(e))mask|=PoolMask(1)<<EmpateMovido;
  }
  int streak=0;bool skipCurrent=true;
  for(int i=(int)e.goalCount()-1;i>=0;i--){const auto& g=e.goalAt((uint8_t)i);if(g.annulled || !g.value)continue;
    if(skipCurrent){skipCurrent=false;continue;}if(g.team==t)break;if(++streak>=2){mask|=PoolMask(1)<<Respuesta2;break;}}
  return mask;
}
// Contexto del comienzo efectivo de segunda parte, con el marcador válido de la primera.
inline PoolMask secondHalfMask(const mfv3::Engine& e){
  auto first=e.periodScore(mfv3::Period::First);unsigned goals=first.white+first.blue;
  PoolMask mask=PoolMask(1)<<SecondHalf;
  if(goals>6)mask|=PoolMask(1)<<SecondHalfMany;
  else if(goals<2)mask|=PoolMask(1)<<SecondHalfFew;
  return mask;
}
// Reloj de ambiente en tiempo de juego: no acumula frases ni consume tiempo en pausa.
struct AmbienceClock {
  uint32_t nextAt=0,lastScoreAt=0;bool openingDone=false,hasScore=false;
  static uint32_t gap(uint32_t random){return 30000u+Catalog::index(random,30001);}
  void reset(uint32_t random){nextAt=gap(random);lastScoreAt=0;openingDone=hasScore=false;}
  void score(uint32_t play){lastScoreAt=play;hasScore=true;}
  int due(uint32_t play,bool first,bool quiet,uint32_t random){
    if(!first || play>20000)openingDone=true;
    if(!quiet || (hasScore && play-lastScoreAt<10000))return -1;
    if(!openingDone){openingDone=true;nextAt=play+gap(random);return Opening20;}
    if(play<nextAt)return -1;
    nextAt=play+gap(random);return Ambience;
  }
};
inline uint32_t playTime(const mfv3::Engine& e,uint32_t now){
  uint32_t at=e.periodElapsed(now);
  for(uint8_t p=0;p<(uint8_t)e.period() && p<3;p++)at+=e.periodDuration((mfv3::Period)p);
  return at;
}
// Predicción del siguiente tiro con contadores pequeños: no copiar Engine ni reservar RAM.
inline bool winsAfterKick(const mfv3::Engine& e,bool scored,mfv3::Team& winner){
  auto g=e.penaltyScore(), n=e.penaltyAttempts();auto t=e.nextKicker();
  if(t==mfv3::Team::White){n.white++;if(scored)g.white++;}
  else {n.blue++;if(scored)g.blue++;}
  unsigned r=e.config().penaltyRounds;
  if(n.white<=r && n.blue<=r){
    if(g.white>g.blue+r-n.blue){winner=mfv3::Team::White;return true;}
    if(g.blue>g.white+r-n.white){winner=mfv3::Team::Blue;return true;}
  }else if(n.white==n.blue && g.white!=g.blue){winner=g.white>g.blue?mfv3::Team::White:mfv3::Team::Blue;return true;}
  return false;
}
inline PoolMask shootoutBeforeMask(const mfv3::Engine& e){
  if(e.phase()!=mfv3::Phase::Penalties)return 0;
  if(!e.kickCount())return PoolMask(1)<<ShootoutFirst;
  mfv3::Team winner;
  if(winsAfterKick(e,true,winner) && winner==e.nextKicker())return PoolMask(1)<<ShootoutCanWin;
  if(winsAfterKick(e,false,winner) && winner!=e.nextKicker())return PoolMask(1)<<ShootoutMustScore;
  return PoolMask(1)<<ShootoutBefore;
}
inline PoolMask shootoutResultMask(const mfv3::Engine& e,bool scored,bool champion){
  if(!scored)return PoolMask(1)<<ShootoutMiss;
  if(e.phase()==mfv3::Phase::Finished && e.reason()==mfv3::Reason::Penalties)
    return PoolMask(1)<<(champion?ShootoutChampion:ShootoutWin);
  return PoolMask(1)<<ShootoutGoal;
}
// Los empates de penalti se refieren al partido, no a la cuenta de una tanda.
inline PoolMask penaltyMask(const mfv3::Engine& e,bool scored){
  if(!scored)return PoolMask(1)<<PenaltyMiss;
  PoolMask mask=PoolMask(1)<<PenaltyGoal;
  if(e.period()!=mfv3::Period::Shootout && e.score().white==e.score().blue){
    mask|=PoolMask(1)<<PenaltyEqualizer;if(lastTwentySeconds(e))mask|=PoolMask(1)<<PenaltyEqualizer20;
  }
  return mask;
}
#if HAS_AUDIO
static Catalog catalog;
static Catalog* staging=nullptr;
static uint16_t expected=0;
static uint32_t receivingCrc=0,receivingAt=0;
static void tick(uint32_t now,bool busy){if(staging && (busy || (int32_t)(now-receivingAt)>30000)){delete staging;staging=nullptr;}}
static const char* CAT="/sonidos/catalogo.cmt";
static const char* BAK="/sonidos/catalogo.bak";
static const char* TEMP="/sonidos/catalogo.tmp";
static void row(uint8_t p,uint32_t code,uint8_t* b){b[0]=p;for(int i=0;i<4;i++)b[i+1]=(uint8_t)(code>>(8*i));}
static bool validAudio(audio::FsStore& store,uint32_t code) {
  auto f=store.open(code);mfv3snd::WavInfo w;
  return audio::validWavFile(f,w); // mismo criterio de formato/tamaño que al reproducir
}
static bool readCatalog(audio::FsStore& store,const char* path,Catalog& out) {
  auto f=store.fsOf(mfv3snd::Place::Sd).open(path,FILE_READ);uint8_t head[10];
  if(!f || f.read(head,10)!=10 || memcmp(head,"CMT1",4))return false;
  uint16_t n=head[4]|(head[5]<<8);uint32_t want=0;for(int j=0;j<4;j++)want|=(uint32_t)head[6+j]<<(j*8);
  if(n>MAX_FILES || f.size()!=10u+5u*n)return false;
  out.clear();uint32_t crc=0;
  for(uint16_t i=0;i<n;i++){uint8_t b[5];if(f.read(b,5)!=5)return false;crc=mfv3snd::crc32Update(crc,b,5);
    uint32_t code=0;for(int j=0;j<4;j++)code|=(uint32_t)b[j+1]<<(j*8);
    if(!out.add(b[0],code))return false;
    if((i&15u)==15u)delay(1);
  }
  return crc==want;
}
static void availableOnly(audio::FsStore& store,Catalog& c) {
  uint16_t write=0;
  uint16_t read=0;c.started=0;
  for(uint8_t p=0;p<PoolCount;p++){uint16_t old=c.counts[p],kept=0;
    for(uint16_t i=0;i<old;i++){uint32_t code=c.codes[read++];if(validAudio(store,code)){c.codes[write++]=code;kept++;}if((i&15u)==15u)delay(1);}
    c.counts[p]=kept;c.remaining[p]=0;}
  c.n=write;
}
static void load(audio::FsStore& store){
  if(!store.sdOk)return;
  Catalog* tmp=new(std::nothrow) Catalog;
  if(!tmp)return;
  if(readCatalog(store,CAT,*tmp) || readCatalog(store,BAK,*tmp)){availableOnly(store,*tmp);catalog=*tmp;}delete tmp;
}
static bool save(audio::FsStore& store,const Catalog& c,uint32_t crc){
  auto& fs=store.fsOf(mfv3snd::Place::Sd);auto f=fs.open(TEMP,FILE_WRITE);if(!f)return false;
  uint8_t h[10]={'C','M','T','1',(uint8_t)c.n,(uint8_t)(c.n>>8),0,0,0,0};for(int i=0;i<4;i++)h[6+i]=(uint8_t)(crc>>(8*i));
  bool ok=f.write(h,10)==10;uint16_t at=0;
  for(uint8_t p=0;p<PoolCount;p++)for(uint16_t i=0;i<c.counts[p];i++){uint8_t b[5];row(p,c.codes[at++],b);if(f.write(b,5)!=5)ok=false;}
  f.close();if(!ok){fs.remove(TEMP);return false;}
  // Primero verificar el archivo entero. La versión previa permanece recuperable durante la sustitución.
  Catalog* check=new(std::nothrow) Catalog;if(!check)return false;ok=readCatalog(store,TEMP,*check);
  bool previousValid=ok && readCatalog(store,CAT,*check);
  delete check;
  if(!ok)return false;
  if(fs.exists(CAT)){
    if(previousValid){fs.remove(BAK);if(!fs.rename(CAT,BAK))return false;}
    else if(!fs.remove(CAT))return false; // preservar BAK si el principal estaba corrupto
  }
  if(!fs.rename(TEMP,CAT))return false; // BAK permanece intacto; load() lo recupera sin consumirlo
  return true;
}
static bool handle(const char* line,audio::FsStore& store,bool busy,mfv3snd::ReplyFn reply){
  if(strncmp(line,"COMENTARIOS",11))return false;
  char buf[320];
  if(!strcmp(line,"COMENTARIOS?")){
    int at=snprintf(buf,sizeof buf,"COMENTARIOS total=%u limite=%u grupos=",catalog.n,MAX_FILES);
    for(uint8_t p=0;p<PoolCount;p++)at+=snprintf(buf+at,sizeof buf-at,"%s%u",p?",":"",catalog.counts[p]);
    reply(buf);return true;
  }
  if(busy || !store.sdOk){reply("COMENTARIOS_ERROR ocupado_o_sin_sd");return true;}
  if(!strncmp(line,"COMENTARIOS_INICIO ",19)){
    unsigned count=0;char tail=0;if(sscanf(line+19,"%u %c",&count,&tail)!=1 || !count || count>MAX_FILES){reply("COMENTARIOS_ERROR cantidad");return true;}
    delete staging;staging=new(std::nothrow) Catalog;expected=(uint16_t)count;receivingCrc=0;receivingAt=millis();reply(staging?"COMENTARIOS_LISTO":"COMENTARIOS_ERROR memoria");return true;
  }
  if(!strncmp(line,"COMENTARIOS_ITEM ",17)){
    unsigned pool;char code[9],tail;uint32_t value;
    if(!staging || staging->n>=expected || sscanf(line+17,"%u %8s %c",&pool,code,&tail)!=2 || pool>=PoolCount || !mfv3snd::parseCode(code,strlen(code),value) || !staging->add((uint8_t)pool,value)){reply("COMENTARIOS_ERROR item");return true;}
    receivingAt=millis();uint8_t b[5];row((uint8_t)pool,value,b);receivingCrc=mfv3snd::crc32Update(receivingCrc,b,5);reply("COMENTARIOS_ITEM_OK");return true;
  }
  if(!strncmp(line,"COMENTARIOS_FIN ",16)){
    uint32_t crc;bool ok=staging && staging->n==expected && mfv3snd::parseCode(line+16,strlen(line+16),crc) && crc==receivingCrc;
    if(ok)for(uint16_t i=0;i<staging->n;i++){
      if(!validAudio(store,staging->codes[i])){ok=false;break;}
      if((i&15u)==15u)delay(1);
    }
    if(ok)ok=save(store,*staging,crc);
    if(ok)catalog=*staging;
    delete staging;
    staging=nullptr;
    reply(ok?"COMENTARIOS_GUARDADO":"COMENTARIOS_ERROR validar_o_guardar");
    return true;
  }
  if(!strcmp(line,"COMENTARIOS_CANCELAR")){delete staging;staging=nullptr;reply("COMENTARIOS_CANCELADO");return true;}
  reply("COMENTARIOS_ERROR orden");return true;
}
#endif
} // namespace comentarios
