// Transiciones reales del sketch: radio, deduplicación y separación de sensores.
#include <cassert>
#include <cstring>
#include <vector>
#include "Arduino.h"
#include "../esp32s3_pantalla7/esp32s3_pantalla7.ino"

static uint16_t sequence = 100;
static uint8_t remoteMac[6] = {0xCE, 1, 2, 3, 4, 5};
static void packet(mfv3::RadioCmd command, uint16_t seq) {
  std::vector<uint8_t> bytes(mfv3::RADIO_PACKET_SIZE);
  mfv3::radioEncode(bytes.data(), GRUPO_MESA, seq, command);
  for (int i=0;i<3;++i) sim::radioDeliver(remoteMac, bytes);
  loop();
}
static void advance(uint32_t ms) {sim::now += ms;loop();}
static void beginTimed() {
  goHome();cfg = mfv3::Config();cfg.endCondition=mfv3::EndCondition::Time;cfg.minutesPerPeriod=1;
  chosenMode=inicio::QUICK;haveTeams=false;startMatch();advance(4000);
  assert(eng.phase()==Phase::Playing && eng.period()==Period::First);
}
int main() {
  sim::now=1000;setup();test.active=false;nube::phase=nube::OFF;
  for (auto command : {mfv3::RadioCmd::GoalWhite,mfv3::RadioCmd::GoalBlue}) {
    beginTimed();
    for (int part=0;part<3;++part) {
      advance(61000);
      assert(eng.phase()==Phase::PeriodEnd);
      partido::View view={};matchView(view,millis());
      assert(strstr(view.endNote,"pulsador")!=nullptr);
      // Un sensor o una anulación larga no deben hacer avanzar de pantalla.
      inputGoal(Team::White);inputGoal(Team::Blue);
      assert(eng.phase()==Phase::PeriodEnd);
      packet(mfv3::RadioCmd::AnnulWhite,++sequence);
      packet(mfv3::RadioCmd::AnnulBlue,++sequence);
      assert(eng.phase()==Phase::PeriodEnd);
      const uint16_t seq=++sequence;
      packet(command,seq);
      assert(eng.score().white==0 && eng.score().blue==0 && eng.kickCount()==0);
      if (part<2) {
        assert(eng.phase()==Phase::Countdown);
        assert(eng.period()==(part==0?Period::Second:Period::Overtime));
        packet(command,seq); // repetición tardía: no salta la cuenta atrás
        assert(eng.phase()==Phase::Countdown);
        advance(4000);assert(eng.phase()==Phase::Playing);
      } else {
        assert(eng.phase()==Phase::Penalties && eng.period()==Period::Shootout);
        packet(command,seq);assert(eng.kickCount()==0);
        const auto next=eng.nextKicker();
        packet(next==Team::White?mfv3::RadioCmd::GoalWhite:mfv3::RadioCmd::GoalBlue,++sequence);
        assert(eng.kickCount()==1); // solo una pulsación NUEVA tira el penalti
      }
    }
  }
  // La entrada física justo en el vencimiento del reloj también sirve para continuar.
  beginTimed();sim::now+=61000;
  packet(mfv3::RadioCmd::GoalBlue,++sequence);
  assert(eng.phase()==Phase::Countdown && eng.period()==Period::Second && eng.score().blue==0);
  // Se conserva el botón táctil de continuar.
  advance(4000);advance(61000);assert(eng.phase()==Phase::PeriodEnd);
  onMatchNext();assert(eng.phase()==Phase::Countdown && eng.period()==Period::Overtime);
  // Un final con ganador no se convierte en otra parte al pulsar el mando.
  beginTimed();inputGoal(Team::White);advance(61000);
  onMatchNext();advance(4000);advance(61000);
  assert(eng.phase()==Phase::Finished);
  packet(mfv3::RadioCmd::GoalWhite,++sequence);packet(mfv3::RadioCmd::GoalBlue,++sequence);
  assert(eng.phase()==Phase::Finished && eng.score().white==1 && eng.score().blue==0);
  puts("Pulsadores: ambos continúan 1a/2a/prórroga sin goles, sin penalti ni duplicados; sensores y táctil OK");
}
