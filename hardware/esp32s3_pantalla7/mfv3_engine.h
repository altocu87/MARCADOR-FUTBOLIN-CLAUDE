// MARCADOR FUTBOLÍN V3 · motor del partido en C++ portable (para la placa ESP32-S3 de 7")
// -----------------------------------------------------------------------------------------
// Mismas reglas que el MatchEngine de la app (src/match-engine/engine.ts), sin memoria dinámica:
//   - POR GOLES / POR TIEMPO / AMBAS: final de cada parte por goles totales del periodo o tiempo.
//   - Cuenta atrás 3 s antes de cada parte y de la prórroga (se puede saltar).
//   - Empate tras la 2ª parte → prórroga de 60 s con gol de oro → penaltis (5 + muerte súbita).
//   - Bloqueo global de 3.000 ms tras cada gol; deshacer, −1, pausa o cambio de parte no lo eluden.
//   - −1 anula el último gol del equipo en la parte actual; DESHACER revierte la última acción.
//   - El reloj usa tiempo real (millis()), la pausa lo detiene y las correcciones no lo retroceden.
// Se prueba en el PC con g++ (hardware/test/engine_test.cpp).

#ifndef MFV3_ENGINE_H
#define MFV3_ENGINE_H

#include <stdint.h>

namespace mfv3 {

enum class Team : uint8_t { White = 0, Blue = 1 };
enum class EndCondition : uint8_t { Goals, Time, Both };
enum class Phase : uint8_t { Countdown, Playing, Paused, PeriodEnd, Penalties, Finished };
enum class Period : uint8_t { First, Second, Overtime, Shootout };
enum class Reason : uint8_t { Regulation, GoldenGoal, Penalties };
enum class Result : uint8_t { Accepted, InvalidState, GoalLock, NoGoalToRemove, NothingToUndo, WrongTurn, Decided };

inline Team other(Team t) { return t == Team::White ? Team::Blue : Team::White; }

struct Config {
  EndCondition endCondition = EndCondition::Goals;
  uint8_t goalsPerPeriod = 5;     // 1–20
  uint8_t minutesPerPeriod = 5;   // 1–30
  uint16_t overtimeSeconds = 60;
  uint8_t penaltyRounds = 5;
  Team penaltyFirst = Team::White;

  bool valid() const {
    return goalsPerPeriod >= 1 && goalsPerPeriod <= 20 && minutesPerPeriod >= 1 && minutesPerPeriod <= 30 &&
           overtimeSeconds >= 10 && penaltyRounds >= 1 && penaltyRounds <= 10;
  }
};

struct Goal {
  Team team;
  Period period;
  uint32_t periodTimeMs;
  bool annulled;
};

struct Kick {
  Team team;
  bool scored;
};

struct Score {
  uint8_t white = 0;
  uint8_t blue = 0;
  uint8_t of(Team t) const { return t == Team::White ? white : blue; }
};

class Engine {
 public:
  static const uint32_t LOCK_MS = 3000;
  static const uint32_t COUNTDOWN_MS = 3000;
  static const uint8_t MAX_GOALS = 120;
  static const uint8_t MAX_KICKS = 60;
  static const uint8_t MAX_UNDO = 64;

  // Empieza un partido: cuenta atrás de la 1ª parte.
  bool start(const Config& cfg, uint32_t now) {
    if (!cfg.valid()) return false;
    *this = Engine();
    cfg_ = cfg;
    phase_ = Phase::Countdown;
    period_ = Period::First;
    countdownEnds_ = now + COUNTDOWN_MS;
    active_ = true;
    return true;
  }

  bool active() const { return active_; }

  // ---------------------------------------------------------------- lectura
  Phase phase() const { return phase_; }
  Period period() const { return period_; }
  const Config& config() const { return cfg_; }

  Score score() const {
    Score s;
    for (uint8_t i = 0; i < goalCount_; i++)
      if (!goals_[i].annulled) (goals_[i].team == Team::White ? s.white : s.blue)++;
    return s;
  }

  Score periodScore(Period p) const {
    Score s;
    for (uint8_t i = 0; i < goalCount_; i++)
      if (!goals_[i].annulled && goals_[i].period == p) (goals_[i].team == Team::White ? s.white : s.blue)++;
    return s;
  }

  Score penaltyScore() const {
    Score s;
    for (uint8_t i = 0; i < kickCount_; i++)
      if (kicks_[i].scored) (kicks_[i].team == Team::White ? s.white : s.blue)++;
    return s;
  }

  Score penaltyAttempts() const {
    Score s;
    for (uint8_t i = 0; i < kickCount_; i++) (kicks_[i].team == Team::White ? s.white : s.blue)++;
    return s;
  }

  uint8_t kickCount() const { return kickCount_; }
  const Kick& kick(uint8_t i) const { return kicks_[i]; }
  // Goles apuntados en orden (también los anulados, marcados con `annulled`), para los avisos y sonidos.
  uint8_t goalCount() const { return goalCount_; }
  const Goal& goalAt(uint8_t i) const { return goals_[i]; }

  // 0 si el tiempo no termina el periodo (POR GOLES).
  uint32_t periodLimitMs() const {
    if (period_ == Period::Overtime) return (uint32_t)cfg_.overtimeSeconds * 1000UL;
    if (period_ == Period::Shootout || cfg_.endCondition == EndCondition::Goals) return 0;
    return (uint32_t)cfg_.minutesPerPeriod * 60000UL;
  }

  uint32_t periodElapsed(uint32_t now) const {
    uint32_t e = elapsedMs_ + (running_ ? now - runningSince_ : 0);
    uint32_t lim = periodLimitMs();
    return (lim && e > lim) ? lim : e;
  }

  // Tiempo a mostrar: restante si hay límite, transcurrido si no.
  uint32_t clockMs(uint32_t now) const {
    uint32_t lim = periodLimitMs();
    uint32_t e = periodElapsed(now);
    return lim ? lim - e : e;
  }
  bool clockCountsDown() const { return periodLimitMs() != 0; }

  uint32_t countdownRemaining(uint32_t now) const {
    if (phase_ != Phase::Countdown) return 0;
    return now >= countdownEnds_ ? 0 : countdownEnds_ - now;
  }

  uint32_t lockRemaining(uint32_t now) const {
    if (!hasLastGoal_) return 0;
    uint32_t since = now - lastGoalAt_;
    return since >= LOCK_MS ? 0 : LOCK_MS - since;
  }

  bool canUndo() const { return undoCount_ > 0 && (phase_ == Phase::Playing || phase_ == Phase::Paused); }

  Team nextKicker() const { return (kickCount_ % 2 == 0) ? cfg_.penaltyFirst : other(cfg_.penaltyFirst); }

  bool suddenDeath() const {
    Score a = penaltyAttempts();
    return a.white >= cfg_.penaltyRounds && a.blue >= cfg_.penaltyRounds;
  }

  Team winner() const { return winner_; }
  Reason reason() const { return reason_; }
  uint32_t totalTimeMs() const { return closedMs_; }

  // Equipos que ganarían con el siguiente gol (bit 0 Blanco, bit 1 Azul).
  uint8_t matchPoint() const {
    if (phase_ != Phase::Playing && phase_ != Phase::Paused) return 0;
    if (period_ == Period::Overtime) return 3;
    if (cfg_.endCondition == EndCondition::Time) return 0;
    // A un gol del objetivo de goles para ganar.
    Score s = score();
    uint8_t m = 0;
    if (s.white + 1 >= cfg_.goalsPerPeriod) m |= 1;
    if (s.blue + 1 >= cfg_.goalsPerPeriod) m |= 2;
    return m;
  }

  // ---------------------------------------------------------------- avance del reloj
  // Devuelve true si cambió el estado (fin de cuenta atrás o de periodo por tiempo).
  bool tick(uint32_t now) {
    bool changed = false;
    for (int guard = 0; guard < 4; guard++) {
      if (phase_ == Phase::Countdown && now >= countdownEnds_) {
        startClock(countdownEnds_);
        changed = true;
        continue;
      }
      uint32_t lim = periodLimitMs();
      if (phase_ == Phase::Playing && running_ && lim) {
        uint32_t raw = elapsedMs_ + (now - runningSince_);
        if (raw >= lim) {
          endPeriod(runningSince_ + (lim - elapsedMs_), PeriodEndBy::Time);
          changed = true;
          continue;
        }
      }
      break;
    }
    return changed;
  }

  // ---------------------------------------------------------------- comandos
  Result skipCountdown(uint32_t now) {
    tick(now);
    if (phase_ != Phase::Countdown) return Result::InvalidState;
    startClock(now);
    return Result::Accepted;
  }

  Result goal(Team t, uint32_t now) {
    tick(now);
    if (phase_ != Phase::Playing) return Result::InvalidState;
    if (lockRemaining(now) > 0) return Result::GoalLock;
    if (goalCount_ >= MAX_GOALS) return Result::InvalidState;
    goals_[goalCount_] = Goal{t, period_, periodElapsed(now), false};
    pushUndo(Action{ActionKind::Goal, goalCount_});
    goalCount_++;
    lastGoalAt_ = now;
    hasLastGoal_ = true;
    checkGoalEnd(now);
    return Result::Accepted;
  }

  Result minusOne(Team t, uint32_t now) {
    tick(now);
    if (phase_ != Phase::Playing && phase_ != Phase::Paused) return Result::InvalidState;
    for (int i = (int)goalCount_ - 1; i >= 0; i--) {
      Goal& g = goals_[i];
      if (!g.annulled && g.team == t && g.period == period_) {
        g.annulled = true;
        pushUndo(Action{ActionKind::Correction, (uint8_t)i});
        return Result::Accepted;
      }
    }
    return Result::NoGoalToRemove;
  }

  Result undo(uint32_t now) {
    tick(now);
    if (phase_ != Phase::Playing && phase_ != Phase::Paused) return Result::InvalidState;
    if (undoCount_ == 0) return Result::NothingToUndo;
    Action a = undo_[--undoCount_];
    if (a.kind == ActionKind::Goal) {
      goals_[a.goal].annulled = true;
    } else {
      goals_[a.goal].annulled = false;  // restaurar el gol corregido
      if (phase_ == Phase::Playing) checkGoalEnd(now);
    }
    // El reloj y el bloqueo de gol no se modifican.
    return Result::Accepted;
  }

  Result pause(uint32_t now) {
    tick(now);
    if (phase_ != Phase::Playing) return Result::InvalidState;
    elapsedMs_ = periodElapsed(now);
    running_ = false;
    phase_ = Phase::Paused;
    return Result::Accepted;
  }

  Result resume(uint32_t now) {
    tick(now);
    if (phase_ != Phase::Paused) return Result::InvalidState;
    runningSince_ = now;
    running_ = true;
    phase_ = Phase::Playing;
    return Result::Accepted;
  }

  // Desde final de periodo: 2ª parte → prórroga → penaltis.
  Result next(uint32_t now) {
    tick(now);
    if (phase_ != Phase::PeriodEnd) return Result::InvalidState;
    if (period_ == Period::First) beginCountdown(Period::Second, now);
    else if (period_ == Period::Second) beginCountdown(Period::Overtime, now);
    else if (period_ == Period::Overtime) {
      phase_ = Phase::Penalties;
      period_ = Period::Shootout;
      elapsedMs_ = 0;
      running_ = false;
      undoCount_ = 0;
    } else return Result::InvalidState;
    return Result::Accepted;
  }

  Result penalty(Team t, bool scored, uint32_t now) {
    tick(now);
    if (phase_ != Phase::Penalties) return Result::InvalidState;
    if (t != nextKicker()) return Result::WrongTurn;
    if (kickCount_ >= MAX_KICKS) return Result::InvalidState;
    kicks_[kickCount_++] = Kick{t, scored};
    Team w;
    if (shootoutWinner(w)) finish(w, Reason::Penalties, now);
    return Result::Accepted;
  }

  Result undoPenalty() {
    if (phase_ != Phase::Penalties || kickCount_ == 0) return Result::NothingToUndo;
    kickCount_--;
    return Result::Accepted;
  }

  bool shootoutWinner(Team& out) const {
    Score g = penaltyScore();
    Score n = penaltyAttempts();
    uint8_t r = cfg_.penaltyRounds;
    if (n.white <= r && n.blue <= r) {
      int remW = r - n.white, remB = r - n.blue;
      if (g.white > g.blue + remB) { out = Team::White; return true; }
      if (g.blue > g.white + remW) { out = Team::Blue; return true; }
      return false;
    }
    if (n.white == n.blue && g.white != g.blue) {
      out = g.white > g.blue ? Team::White : Team::Blue;
      return true;
    }
    return false;
  }

 private:
  enum class ActionKind : uint8_t { Goal, Correction };
  enum class PeriodEndBy : uint8_t { Goals, Time, GoldenGoal };
  struct Action {
    ActionKind kind;
    uint8_t goal;
  };

  void pushUndo(Action a) {
    if (undoCount_ >= MAX_UNDO) {
      for (uint8_t i = 1; i < MAX_UNDO; i++) undo_[i - 1] = undo_[i];
      undoCount_--;
    }
    undo_[undoCount_++] = a;
  }

  void startClock(uint32_t at) {
    phase_ = Phase::Playing;
    elapsedMs_ = 0;
    runningSince_ = at;
    running_ = true;
  }

  void beginCountdown(Period p, uint32_t now) {
    phase_ = Phase::Countdown;
    period_ = p;
    countdownEnds_ = now + COUNTDOWN_MS;
    elapsedMs_ = 0;
    running_ = false;
    undoCount_ = 0;
  }

  void checkGoalEnd(uint32_t now) {
    if (phase_ != Phase::Playing) return;
    Score ps = periodScore(period_);
    if (period_ == Period::Overtime) {
      if (ps.white + ps.blue > 0) endPeriod(now, PeriodEndBy::GoldenGoal);
      return;
    }
    if (cfg_.endCondition == EndCondition::Time) return;
    // Por goles (y en «ambas»): el primer equipo que llega al objetivo gana en ese momento.
    Score s = score();
    if (s.white >= cfg_.goalsPerPeriod || s.blue >= cfg_.goalsPerPeriod) endPeriod(now, PeriodEndBy::Goals);
  }

  void endPeriod(uint32_t at, PeriodEndBy by) {
    uint32_t e = periodElapsed(at);
    elapsedMs_ = e;
    running_ = false;
    phase_ = Phase::PeriodEnd;
    undoCount_ = 0;
    closedMs_ += e;
    Score s = score();
    if (by == PeriodEndBy::Goals) {
      finish(s.white > s.blue ? Team::White : Team::Blue, Reason::Regulation, at);
    } else if (period_ == Period::Second && s.white != s.blue) {
      finish(s.white > s.blue ? Team::White : Team::Blue, Reason::Regulation, at);
    } else if (period_ == Period::Overtime && by == PeriodEndBy::GoldenGoal) {
      finish(s.white > s.blue ? Team::White : Team::Blue, Reason::GoldenGoal, at);
    }
  }

  void finish(Team w, Reason r, uint32_t) {
    winner_ = w;
    reason_ = r;
    phase_ = Phase::Finished;
    undoCount_ = 0;
  }

  Config cfg_;
  bool active_ = false;
  Phase phase_ = Phase::Countdown;
  Period period_ = Period::First;
  uint32_t countdownEnds_ = 0;
  uint32_t elapsedMs_ = 0;
  uint32_t runningSince_ = 0;
  bool running_ = false;
  uint32_t closedMs_ = 0;
  uint32_t lastGoalAt_ = 0;
  bool hasLastGoal_ = false;
  Goal goals_[MAX_GOALS];
  uint8_t goalCount_ = 0;
  Action undo_[MAX_UNDO];
  uint8_t undoCount_ = 0;
  Kick kicks_[MAX_KICKS];
  uint8_t kickCount_ = 0;
  Team winner_ = Team::White;
  Reason reason_ = Reason::Regulation;
};

}  // namespace mfv3

#endif  // MFV3_ENGINE_H
