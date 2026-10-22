// Catálogo local por grupos. Nunca recorre la SD ni los 1024 códigos al elegir una frase.
#pragma once
#include "mfv3_sound.h"
#include <new>
namespace comentarios {
static const uint16_t MAX_FILES = 1024;
enum Pool : uint8_t { Comodin, Encajados3, Empate, Respuesta2, Anulado, EmpateMovido, PoolCount };
struct Catalog {
  uint32_t codes[MAX_FILES] = {};
  uint16_t counts[PoolCount] = {}, n = 0;
  uint16_t previous[PoolCount] = {65535,65535,65535,65535,65535,65535};
  void clear(){
    n=0;
    memset(counts,0,sizeof counts);
    for(uint8_t p=0;p<PoolCount;p++)previous[p]=65535;
  }
  uint16_t offset(uint8_t p) const { uint16_t o=0; for(uint8_t i=0;i<p;i++)o+=counts[i];return o; }
  bool add(uint8_t p,uint32_t code) {
    if(p>=PoolCount || !code || n>=MAX_FILES)return false;
    for(uint8_t i=p+1;i<PoolCount;i++)if(counts[i])return false; // transferencia ordenada por grupo
    for(uint16_t i=0;i<n;i++)if(codes[i]==code)return false;
    codes[n++]=code;counts[p]++;return true;
  }
  uint32_t pick(uint8_t mask,uint32_t poolRandom,uint32_t clipRandom) {
    uint8_t pools[PoolCount],k=0;
    for(uint8_t p=0;p<PoolCount;p++)if((mask&(1u<<p)) && counts[p])pools[k++]=p;
    if(!k)return 0;
    uint8_t p=pools[poolRandom%k];uint16_t count=counts[p],at;
    if(count>1 && previous[p]<count){at=clipRandom%(count-1);if(at>=previous[p])at++;}
    else at=clipRandom%count;
    previous[p]=at;return codes[offset(p)+at];
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
inline uint8_t goalMask(const mfv3::Engine& e,const mfv3snd::Before& b,mfv3::Team t) {
  auto s=e.score();if(b.phase==mfv3::Phase::Penalties || e.phase()==mfv3::Phase::Penalties || s.of(t)<=b.score.of(t))return 0;
  uint8_t mask=1u<<Comodin;
  if(b.score.of(t==mfv3::Team::White?mfv3::Team::Blue:mfv3::Team::White)>=3)mask|=1u<<Encajados3; // marca quien ya había encajado al menos tres
  if(s.white==s.blue){
    mask|=1u<<Empate;
    if(twoRecentGoals(e))mask|=1u<<EmpateMovido;
  }
  int streak=0;bool skipCurrent=true;
  for(int i=(int)e.goalCount()-1;i>=0;i--){const auto& g=e.goalAt((uint8_t)i);if(g.annulled || !g.value)continue;
    if(skipCurrent){skipCurrent=false;continue;}if(g.team==t)break;if(++streak>=2){mask|=1u<<Respuesta2;break;}}
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
  uint16_t read=0;
  for(uint8_t p=0;p<PoolCount;p++){uint16_t old=c.counts[p],kept=0;
    for(uint16_t i=0;i<old;i++){uint32_t code=c.codes[read++];if(validAudio(store,code)){c.codes[write++]=code;kept++;}if((i&15u)==15u)delay(1);}
    c.counts[p]=kept;c.previous[p]=65535;}
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
  char buf[160];
  if(!strcmp(line,"COMENTARIOS?")){snprintf(buf,sizeof buf,"COMENTARIOS total=%u limite=%u grupos=%u,%u,%u,%u,%u,%u",catalog.n,MAX_FILES,catalog.counts[0],catalog.counts[1],catalog.counts[2],catalog.counts[3],catalog.counts[4],catalog.counts[5]);reply(buf);return true;}
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
