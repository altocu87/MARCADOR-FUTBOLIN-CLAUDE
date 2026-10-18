// Pruebas del motor C++ en el PC (mismos criterios A01–A13 que la app):
//   g++ -std=c++11 -Wall -I../common engine_test.cpp && ./a.out
#include <cassert>
#include <cstdio>

#include "mfv3_engine.h"

using namespace mfv3;
typedef Result R;

static Config cfg(EndCondition c = EndCondition::Goals, uint8_t goals = 5, uint8_t minutes = 5) {
  Config k;
  k.endCondition = c;
  k.goalsPerPeriod = goals;
  k.minutesPerPeriod = minutes;
  return k;
}

struct Sim {
  Engine e;
  uint32_t t = 1000;
  explicit Sim(const Config& c) { assert(e.start(c, t)); }
  void wait(uint32_t ms) { t += ms; e.tick(t); }
  R goal(Team x) { return e.goal(x, t); }
  void goalLater(Team x) { wait(3000); assert(goal(x) == R::Accepted); }
  void startPeriod() { assert(e.skipCountdown(t) == R::Accepted); }
};

static void a01_full_match() {
  // Por goles: una sola parte; gana el primero que llega al objetivo.
  Sim s(cfg(EndCondition::Goals, 3));
  assert(s.e.phase() == Phase::Countdown);
  s.wait(2999); assert(s.e.phase() == Phase::Countdown);
  s.wait(1); assert(s.e.phase() == Phase::Playing);
  s.goalLater(Team::White); s.goalLater(Team::Blue); s.goalLater(Team::White); s.goalLater(Team::Blue);
  assert(s.e.phase() == Phase::Playing);
  s.goalLater(Team::White);
  assert(s.e.phase() == Phase::Finished && s.e.winner() == Team::White && s.e.reason() == Reason::Regulation);
  assert(s.e.score().white == 3 && s.e.score().blue == 2);
  // Por tiempo: dos partes y gana quien suma más.
  Sim t(cfg(EndCondition::Time, 5, 1));
  t.startPeriod(); t.goalLater(Team::White); t.goalLater(Team::White);
  t.wait(60000); assert(t.e.phase() == Phase::PeriodEnd);
  assert(t.e.next(t.t) == R::Accepted && t.e.period() == Period::Second);
  t.startPeriod(); t.goalLater(Team::Blue);
  t.wait(60000);
  assert(t.e.phase() == Phase::Finished && t.e.winner() == Team::White && t.e.reason() == Reason::Regulation);
  // Ambas: llegar a los goles gana aunque quede tiempo.
  Sim b(cfg(EndCondition::Both, 2, 5));
  b.startPeriod(); b.goalLater(Team::Blue); b.goalLater(Team::Blue);
  assert(b.e.phase() == Phase::Finished && b.e.winner() == Team::Blue);
}

static void a02_invalid_config() {
  Engine e;
  assert(!e.start(cfg(EndCondition::Goals, 0), 0));
  assert(!e.start(cfg(EndCondition::Goals, 21), 0));
  assert(!e.start(cfg(EndCondition::Time, 5, 31), 0));
}

static void a03_a04_goals_and_time() {
  Sim s(cfg(EndCondition::Goals, 5, 1));
  s.startPeriod();
  s.wait(3600000);  // una hora: POR GOLES no termina por tiempo
  assert(s.e.phase() == Phase::Playing);
  Sim t(cfg(EndCondition::Time, 1, 1));
  t.startPeriod();
  t.goalLater(Team::White); t.goalLater(Team::White);
  assert(t.e.phase() == Phase::Playing);
  t.wait(60000);
  assert(t.e.phase() == Phase::PeriodEnd);
}

static void a05_both_boundary() {
  Sim s(cfg(EndCondition::Both, 10, 1));
  s.startPeriod();
  s.t += 60000;
  assert(s.goal(Team::White) == R::InvalidState);  // el tiempo se procesa primero
  assert(s.e.score().white == 0 && s.e.phase() == Phase::PeriodEnd);
}

static void a06_a07_lock() {
  Sim s(cfg(EndCondition::Goals, 20));
  assert(s.goal(Team::White) == R::InvalidState);  // antes de empezar
  s.startPeriod();
  s.wait(10000);
  uint32_t t0 = s.t;
  assert(s.goal(Team::White) == R::Accepted);
  s.t = t0 + 500;  assert(s.goal(Team::Blue) == R::GoalLock);
  s.t = t0 + 2900; assert(s.goal(Team::White) == R::GoalLock);
  s.t = t0 + 3000; assert(s.goal(Team::Blue) == R::Accepted);
  assert(s.e.pause(s.t) == R::Accepted);
  assert(s.goal(Team::Blue) == R::InvalidState);  // en pausa
}

static void a08_nothing_bypasses_lock() {
  Sim s(cfg(EndCondition::Goals, 20));
  s.startPeriod(); s.wait(5000);
  uint32_t t0 = s.t;
  s.goal(Team::White);
  s.t = t0 + 100; assert(s.e.undo(s.t) == R::Accepted);
  assert(s.goal(Team::Blue) == R::GoalLock);
  s.e.pause(s.t); s.t += 100; s.e.resume(s.t);
  assert(s.goal(Team::Blue) == R::GoalLock);
  // Cambio de parte (por tiempo) justo después de un gol.
  Sim p(cfg(EndCondition::Time, 5, 1));
  p.startPeriod(); p.wait(58000);
  t0 = p.t;
  p.goal(Team::White);
  p.wait(2000); assert(p.e.phase() == Phase::PeriodEnd);
  p.t = t0 + 2200; p.e.next(p.t); p.startPeriod();
  p.t = t0 + 2500; assert(p.goal(Team::Blue) == R::GoalLock);
}

static void a09_corrections() {
  Sim s(cfg(EndCondition::Time, 5, 5));
  s.startPeriod();
  assert(s.e.minusOne(Team::White, s.t) == R::NoGoalToRemove);
  s.goalLater(Team::White);
  uint32_t before = s.e.periodElapsed(s.t);
  assert(s.e.minusOne(Team::White, s.t) == R::Accepted);
  assert(s.e.score().white == 0 && s.e.periodElapsed(s.t) == before);
  assert(s.e.undo(s.t) == R::Accepted && s.e.score().white == 1);  // restaura el gol
}

static Sim toOvertime() {
  // Por tiempo, 2–2 al final de la 2ª parte.
  Sim s(cfg(EndCondition::Time, 5, 1));
  s.startPeriod(); s.goalLater(Team::White); s.goalLater(Team::Blue);
  while (s.e.phase() == Phase::Playing) s.wait(1000);
  s.e.next(s.t); s.startPeriod(); s.goalLater(Team::White); s.goalLater(Team::Blue);
  while (s.e.phase() == Phase::Playing) s.wait(1000);
  assert(s.e.phase() == Phase::PeriodEnd && s.e.period() == Period::Second);
  s.e.next(s.t);
  assert(s.e.period() == Period::Overtime);
  s.startPeriod();
  return s;
}

static void a10_golden_goal() {
  Sim s = toOvertime();
  assert(s.e.clockMs(s.t) == 60000);
  s.goalLater(Team::Blue);
  assert(s.e.phase() == Phase::Finished && s.e.winner() == Team::Blue && s.e.reason() == Reason::GoldenGoal);
}

static void kicks(Sim& s, const char* seq) {
  for (const char* c = seq; *c; c++) assert(s.e.penalty(s.e.nextKicker(), *c == 'G', s.t) == R::Accepted);
}

static Sim toShootout() {
  Sim s = toOvertime();
  s.wait(60000);
  assert(s.e.phase() == Phase::PeriodEnd);
  s.e.next(s.t);
  assert(s.e.phase() == Phase::Penalties);
  return s;
}

static void a11_a12_a13_penalties() {
  Sim s = toShootout();
  assert(s.e.penalty(Team::Blue, true, s.t) == R::WrongTurn);
  assert(s.goal(Team::White) == R::InvalidState);
  kicks(s, "GFGFGF");  // Blanco 3, Azul 0 con 2 restantes → decidida
  assert(s.e.phase() == Phase::Finished && s.e.winner() == Team::White && s.e.reason() == Reason::Penalties);
  assert(s.e.score().white == 2 && s.e.score().blue == 2);  // el ordinario sigue empatado

  Sim d = toShootout();
  kicks(d, "GGGGGGGGGG");
  assert(d.e.phase() == Phase::Penalties && d.e.suddenDeath());
  kicks(d, "GG");
  assert(d.e.phase() == Phase::Penalties);
  kicks(d, "FG");
  assert(d.e.phase() == Phase::Finished && d.e.winner() == Team::Blue);

  Sim u = toShootout();
  kicks(u, "G");
  assert(u.e.undoPenalty() == R::Accepted && u.e.kickCount() == 0);
}

static void pause_keeps_time() {
  Sim s(cfg(EndCondition::Time, 5, 1));
  s.startPeriod();
  s.wait(10000);
  s.e.pause(s.t);
  s.wait(120000);
  assert(s.e.clockMs(s.t) == 50000);
  s.e.resume(s.t);
  s.wait(49999); assert(s.e.phase() == Phase::Playing);
  s.wait(1); assert(s.e.phase() == Phase::PeriodEnd);
}

static void match_point() {
  Sim s(cfg(EndCondition::Goals, 3));
  s.startPeriod(); s.goalLater(Team::White);
  assert(s.e.matchPoint() == 0);
  s.goalLater(Team::White);
  assert(s.e.matchPoint() == 1);  // 2–0 a 3 goles: solo Blanco gana con el siguiente
  s.goalLater(Team::Blue); s.goalLater(Team::Blue);
  assert(s.e.matchPoint() == 3);
}

// Partido Loco: un hándicap entre los 30 y 45 s de juego, con el reloj parado mientras se anuncia.
static void chaos_handicaps() {
  Config c = cfg(EndCondition::Goals, 20);
  c.chaos = true;
  c.players[0] = 2;
  c.players[1] = 2;
  for (uint32_t seed = 1; seed <= 300; seed++) {
    Sim s(c);
    s.e.seed(seed);
    s.startPeriod();
    // 1 s cada vez hasta que salga.
    int secs = 0;
    while (s.e.phase() == Phase::Playing && secs < 60) { s.wait(1000); secs++; }
    assert(s.e.phase() == Phase::Handicap && s.e.handicapStage() == HStage::Announce);
    assert(secs >= 30 && secs <= 46);
    uint32_t clock = s.e.clockMs(s.t);
    s.wait(5000);
    assert(s.e.clockMs(s.t) == clock);  // reloj parado
    const Handicap h = s.e.handicap();
    if (h.kind == HKind::Penalty) {
      Score before = s.e.score();
      assert(s.e.handicapPenalty(true, s.t) == R::Accepted);
      assert(s.e.phase() == Phase::Playing && s.e.score().of(h.team ? Team::Blue : Team::White) == before.of(h.team ? Team::Blue : Team::White) + 1);
      continue;
    }
    assert(s.e.handicapGo(s.t) == R::Accepted && s.e.handicapStage() == HStage::Active && s.e.phase() == Phase::Playing);
    s.wait(3000);
    Score b = s.e.score();
    Team who = h.team == 1 ? Team::Blue : Team::White;
    assert(s.goal(who) == R::Accepted);
    Score a = s.e.score();
    int got = a.of(who) - b.of(who);
    switch (h.kind) {
      case HKind::DoubleAll: assert(got == 2); break;
      case HKind::DoubleTeam: assert(got == 2); break;
      case HKind::TripleNext: assert(got == 3 && s.e.handicapStage() == HStage::None); break;
      case HKind::FreezeScore: assert(got == 0); break;
      case HKind::Steal: assert(got == 1 && a.of(other(who)) + 1 == b.of(other(who)) && s.e.handicapStage() == HStage::None); break;
      default: assert(got == 1); break;
    }
    // Los de duración terminan solos: «vuelta a la normalidad» 3 s y se sigue.
    if (h.durationMs && s.e.handicapStage() == HStage::Active) {
      int steps = 0;
      while (s.e.phase() == Phase::Playing && steps++ < 100) s.wait(500);
      if (!(s.e.phase() == Phase::Handicap && s.e.handicapStage() == HStage::Ending)) std::printf("seed %u kind %d dur %u fase %d etapa %d fin %d\n", seed, (int)h.kind, h.durationMs, (int)s.e.phase(), (int)s.e.handicapStage(), (int)s.e.lastEnd());
      assert(s.e.phase() == Phase::Handicap && s.e.handicapStage() == HStage::Ending && s.e.lastEnd() == HEnd::Time);
      s.wait(3000);
      assert(s.e.phase() == Phase::Playing && s.e.handicapStage() == HStage::None);
    }
  }
  // Sin modo loco no sale ninguno.
  Sim q(cfg(EndCondition::Goals, 20));
  q.startPeriod();
  for (int i = 0; i < 200; i++) q.wait(1000);
  assert(q.e.phase() == Phase::Playing && q.e.handicapStage() == HStage::None);
}

// Visitas: el animal para el reloj, hace su travesura y se sigue; nunca decide el partido.
static void chaos_visits() {
  Config c = cfg(EndCondition::Goals, 10);
  c.chaos = true;
  int visits = 0;
  for (uint32_t seed = 1; seed <= 200; seed++) {
    Sim s(c);
    s.e.seed(seed);
    s.startPeriod();
    s.goalLater(Team::White);
    for (int i = 0; i < 400 && s.e.phase() != Phase::Finished; i++) {
      s.wait(500);
      if (s.e.phase() == Phase::Handicap) {
        if (s.e.handicapStage() == HStage::Announce && s.e.handicap().kind == HKind::Penalty) s.e.handicapPenalty(false, s.t);
        else s.e.handicapGo(s.t);
      }
      if (s.e.phase() == Phase::Visit) {
        visits++;
        Score b = s.e.score();
        uint8_t tgt = s.e.goalTarget();
        while (s.e.phase() == Phase::Visit) s.wait(500);
        Score a = s.e.score();
        const Visitor& v = s.e.visitor();
        assert(s.e.visitApplied());
        if (v.animal == Animal::Squirrel) assert(a.white + a.blue == b.white + b.blue && a.of(v.from ? Team::Blue : Team::White) + 1 == b.of(v.from ? Team::Blue : Team::White));
        if (v.animal == Animal::Cat) assert(a.white + a.blue == b.white + b.blue + 1);
        if (v.animal == Animal::Snail) assert(s.e.goalTarget() == tgt + 1);
        assert(s.e.phase() == Phase::Playing);
      }
    }
  }
  assert(visits > 20);
  // Por tiempo: se juega hasta el final sin atascarse (el caracol añade un minuto a su parte).
  Config t = cfg(EndCondition::Time, 5, 3);
  t.chaos = true;
  for (uint32_t seed = 1; seed <= 60; seed++) {
    Sim s(t);
    s.e.seed(seed);
    s.startPeriod();
    int guard = 0;
    while (s.e.phase() != Phase::Finished && guard++ < 4000) {
      s.wait(500);
      if (s.e.phase() == Phase::Handicap && s.e.handicapStage() == HStage::Announce) {
        if (s.e.handicap().kind == HKind::Penalty) s.e.handicapPenalty(seed & 1, s.t);
        else s.e.handicapGo(s.t);
      }
      if (s.e.phase() == Phase::PeriodEnd) s.e.next(s.t);
      if (s.e.phase() == Phase::Countdown) s.startPeriod();
      if (s.e.phase() == Phase::Penalties) s.e.penalty(s.e.nextKicker(), s.e.nextKicker() == Team::White, s.t);
      if (guard % 20 == 0 && s.e.phase() == Phase::Playing) s.e.goal(guard % 40 ? Team::White : Team::Blue, s.t);
    }
    assert(s.e.phase() == Phase::Finished);
  }
}

int main() {
  a01_full_match();
  a02_invalid_config();
  a03_a04_goals_and_time();
  a05_both_boundary();
  a06_a07_lock();
  a08_nothing_bypasses_lock();
  a09_corrections();
  a10_golden_goal();
  a11_a12_a13_penalties();
  pause_keeps_time();
  match_point();
  chaos_handicaps();
  chaos_visits();
  std::puts("mfv3_engine: criterios A01-A13 y Partido Loco OK");
  return 0;
}
