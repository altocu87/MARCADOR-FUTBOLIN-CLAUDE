// Reloj y resumen reales: I2C intermitente, falta de RTC, guardado y filtros sin inventar fechas.
#include <cassert>
#include <cstring>
#include <string>
#include <filesystem>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"
static void collect(lv_obj_t* obj,std::string& s) {
  if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)) return;
  if(lv_obj_check_type(obj,&lv_label_class)){s+=lv_label_get_text(obj);s+='|';}
  for(uint32_t i=0;i<lv_obj_get_child_count(obj);++i) collect(lv_obj_get_child(obj,i),s);
}
static std::string text(){std::string s;collect(lv_screen_active(),s);return s;}
static bool shows(const char* s){return text().find(s)!=std::string::npos;}
static uint32_t nowUtc=0;
static void summary(historial::DigestSpan span) {
  historial::tab=historial::DIGEST;historial::digestSpan=span;historial::digestSpanChosen=true;
  historial::show(&jugadores,true);
}
int main(){
  sim::now=1000;setup();test.active=false;nube::phase=nube::OFF;
  assert(!reloj::validClock({2026,2,29,12,0,0}));
  assert(reloj::validClock({2028,2,29,12,0,0}));
  assert(rtc.set({2026,12,31,23,59,58}));rtc.poll(millis());assert(rtc.rtcOk);
  sim::rtcPresent=false;sim::now+=3000;rtc.poll(millis());
  assert(rtc.clockOk && !rtc.rtcOk && rtc.now.year==2027 && rtc.now.month==1 && rtc.now.day==1 && rtc.now.second==1);
  sim::now+=61000;rtc.poll(millis());assert(rtc.now.minute==1 && rtc.now.second==2);
  reloj::State noClock;noClock.poll(millis());assert(!noClock.clockOk);
  assert(noClock.set({2026,10,22,18,0,0}) && noClock.clockOk && !noClock.rtcOk);
  sim::now+=2100;noClock.poll(millis());assert(noClock.now.second==2);
  noClock.advance(millis()-10000);assert(noClock.now.second==2); // timestamp anterior: nunca salta 49 días
  // Si el RTC reaparece con una fecha antigua, no deshace una puesta en hora que no logró guardar.
  sim::rtcPresent=true;sim::now+=1100;noClock.poll(millis());
  assert(noClock.now.year==2026 && noClock.now.month==10 && noClock.now.day==22 && noClock.now.second==3);
  sim::rtcPresent=false;
  reloj::State wrapping;sim::now=0xfffff800u;assert(wrapping.set({2028,2,29,23,59,58}));
  sim::now+=4000;wrapping.poll(millis());assert(wrapping.clockOk && wrapping.now.month==3 && wrapping.now.day==1 && wrapping.now.second==2);
  // Un arranque sin RTC ni hora conocida no inventa la fecha del día.
  reloj::State cold;cold.poll(millis());assert(!cold.clockOk);
  // La hora recuperada de la red se aprovecha aunque se haya cancelado la búsqueda de versiones.
  versionsRunning=true;versionsDone=true;checkState=CheckState::Never;
  fetchedClockOk=true;fetchedClock={2026,10,22,18,0,0};finishVersions();
  assert(rtc.clockOk && rtc.now.year==2026 && rtc.now.month==10 && rtc.now.day==22);
  fetchedClockOk=false;
  // Partidos originales sin fecha: el resumen ALL los muestra y no modifica sus filas.
  partidos::count=0;++partidos::revision;
  for(int i=0;i<2;++i){partidos::Row r{};snprintf(r.id,sizeof(r.id),"historia-sin-fecha-%d",i);r.mode='q';r.winner='w';r.w=5;r.b=2;assert(partidos::push(r));}
  partidos::Row original[2]={partidos::rows[0],partidos::rows[1]};
  historial::hooks.nowUtc=[](){return nowUtc;};
  historial::tab=historial::DIGEST;historial::digestSpanChosen=false;historial::show(&jugadores,true);
  assert(historial::digestSpan==historial::DIGEST_ALL && shows("2 · 14") && shows("Incluye 2 sin fecha"));
  assert(shows("Sin jugadores elegidos") && !shows("Sin partidos en este periodo"));
  summary(historial::DIGEST_WEEK);assert(shows("Falta la fecha y hora"));
  nowUtc=partidos::epoch({2026,10,22,18,0,0});
  summary(historial::DIGEST_WEEK);assert(shows("Hay partidos sin fecha"));
  summary(historial::DIGEST_ALL);assert(shows("2 · 14"));
  assert(!memcmp(original,partidos::rows,sizeof(original)));
  // Mezcla de fechas: solo el partido reciente entra en 7 días, los dos fechados en 30.
  for(int i=0;i<2;++i){partidos::Row r{};snprintf(r.id,sizeof(r.id),"historia-fechada-%d",i);r.mode='q';r.winner='w';r.w=3;r.b=1;r.fin=nowUtc-(i?20u:1u)*86400u;assert(partidos::push(r));}
  summary(historial::DIGEST_WEEK);assert(shows("1 · 4"));
  summary(historial::DIGEST_MONTH);assert(shows("2 · 8"));
  summary(historial::DIGEST_ALL);assert(shows("4 · 22"));
  // Las consultas son de lectura y no cambian los registros ni ponen el reloj.
  sim::out.clear();handleBoardLine("HISTORIAL?");assert(sim::out.find("partidos=4 sin_fecha=2")!=std::string::npos);
  sim::out.clear();handleBoardLine("RELOJ?");assert(sim::out.find("RELOJ valida=")!=std::string::npos);
  assert(!memcmp(original,partidos::rows,sizeof(original)));
  // Guardado real JSON + índice con fecha válida, sin depender del modo rápido o jugadores.
  auto dir=std::filesystem::temp_directory_path()/"mfv3-fechas-resumen-test";std::filesystem::create_directories(dir);
  partidos::setDir(dir.string().c_str());
  mfv3::Engine game;mfv3::Config singleCfg;singleCfg.goalsPerPeriod=1;assert(game.start(singleCfg,0));game.skipCountdown(1);game.goal(mfv3::Team::White,1000);
  assert(game.phase()==mfv3::Phase::Finished);
  partidos::Match match{};match.mode='q';match.cfg=singleCfg;match.startEpoch=nowUtc-100;match.endEpoch=nowUtc;
  assert(partidos::save(game,match));assert(partidos::rows[partidos::count-1].fin==nowUtc);
  partidos::Row reread{};char path[256];snprintf(path,sizeof(path),"%s/%s.json",dir.string().c_str(),partidos::rows[partidos::count-1].id);
  assert(partidos::rowFromJson(path,reread) && reread.fin==nowUtc && reread.w==1);
  char date[32];partidos::dateText(reread.fin,date,sizeof(date));assert(!strcmp(date,"22/10/26, 18:00"));
  std::filesystem::remove(path);
  // Inicio sin hora, sincronización posterior y pausa: recuperar startedAt con tiempo de pared.
  rtc=reloj::State();sim::rtcPresent=false;cfg=mfv3::Config();cfg.goalsPerPeriod=1;
  chosenMode=inicio::QUICK;haveTeams=false;startMatch();
  assert(matchStartEpoch==0);
  sim::now+=4000;loop();inputPause();assert(eng.phase()==Phase::Paused);
  sim::now+=10000;loop();inputPause();
  assert(rtc.set({2026,10,22,18,0,15}));
  sim::now+=2000;loop();inputButtonGoal(Team::White);
  assert(eng.phase()==Phase::Finished);
  snprintf(path,sizeof(path),"%s/%s.json",dir.string().c_str(),partidos::rows[partidos::count-1].id);
  char* json=partidos::readAll(path);assert(json);
  uint32_t started=(uint32_t)(partidos::numOf(partidos::keyIn(json,"startedAt"))/1000.0);
  uint32_t finished=(uint32_t)(partidos::numOf(partidos::keyIn(json,"finishedAt"))/1000.0);
  assert(started && finished>started && finished-started>=16 && finished-started<=17);
  assert(finished-started>eng.totalTimeMs()/1000u);free(json);
  std::filesystem::remove(path);std::filesystem::remove(dir/"indice.txt");std::filesystem::remove(dir);
  puts("Fechas/resumen: reloj intermitente/temporal/wrap, JSON fechado, TODOS y filtros, datos originales intactos OK");
}
