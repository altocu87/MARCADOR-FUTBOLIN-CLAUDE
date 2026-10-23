// MARCADOR FUTBOLÍN V3 · motor del partido en C++ portable (para la placa ESP32-S3 de 7")
// -----------------------------------------------------------------------------------------
// Mismas reglas que el MatchEngine de la app (src/match-engine/engine.ts), sin memoria dinámica:
//   - POR GOLES / POR TIEMPO / AMBAS: final de cada parte por goles totales del periodo o tiempo.
//   - Cuenta atrás 3 s antes de cada parte y de la prórroga (se puede saltar).
//   - Empate tras la 2ª parte → prórroga de 60 s con gol de oro → penaltis (5 + muerte súbita).
//     Con Config::allowDraw (liga de un torneo a tiempo o «ambas»), igualados al acabar la 2ª parte es empate: se
//     acaba ahí, sin prórroga ni penaltis (draw() = true; winner() no vale nada).
//   - Bloqueo global de 3.000 ms tras cada gol; deshacer, −1, pausa o cambio de parte no lo eluden.
//   - −1 anula el último gol del equipo en la parte actual; DESHACER revierte la última acción.
//   - El reloj usa tiempo real (millis()), la pausa lo detiene y las correcciones no lo retroceden.
//   - Partido Loco (Config::chaos): cada 30–60 s de juego un hándicap (src/match-engine/handicaps.ts): se para el
//     reloj y se anuncia, luego dura su tiempo (o hasta usarse) y cambia el valor de los goles; a veces, entre
//     dos hándicaps, un animal (ardilla, caracol, gato) hace una travesura en el marcador.
// Se prueba en el PC con g++ (hardware/test/engine_test.cpp).

#ifndef MFV3_ENGINE_H
#define MFV3_ENGINE_H

#include <stdint.h>

namespace mfv3 {

enum class Team : uint8_t { White = 0, Blue = 1 };
enum class EndCondition : uint8_t { Goals, Time, Both };
enum class Phase : uint8_t { Countdown, Playing, Paused, PeriodEnd, Penalties, Finished, Handicap, Visit };
enum class Period : uint8_t { First, Second, Overtime, Shootout };
enum class Reason : uint8_t { Regulation, GoldenGoal, Penalties };
enum class Result : uint8_t { Accepted, InvalidState, GoalLock, NoGoalToRemove, NothingToUndo, WrongTurn, Decided };

inline Team other(Team t) { return t == Team::White ? Team::Blue : Team::White; }

// ---------------------------------------------------------------- Partido Loco
// Hándicaps, en el mismo orden que HANDICAP_DURATION de la app.
enum class HKind : uint8_t {
  DoubleAll, DoubleTeam, TripleNext, Steal, FreezeScore, SwapPositions, FrozenPlayer, BarLock, Penalty, Transfer,
  WeakHand, NoSpin, LongShots, OneHand, Count
};
enum class HStage : uint8_t { None, Announce, Active, Ending };
enum class HEnd : uint8_t { None, Time, Used, Replaced, PeriodEnd };
enum class Bar : uint8_t { Portero, Defensa, Medio, Delantero };
enum class Animal : uint8_t { Squirrel, Snail, Cat };
enum class Bonus : uint8_t { None, Double, Triple, Steal, Frozen, Penalty };

// Duración en tiempo de juego (0 = sin duración: hasta usarse, hasta el siguiente o un solo lanzamiento).
inline uint32_t handicapDuration(HKind k) {
  static const uint32_t D[(int)HKind::Count] = {30000, 30000, 0, 0, 15000, 0, 15000, 15000, 0, 30000, 20000, 30000, 30000, 20000};
  return D[(int)k];
}

struct Handicap {
  HKind kind = HKind::DoubleAll;
  uint8_t team = 2;          // 0 Blanco, 1 Azul, 2 los dos
  uint32_t durationMs = 0;
  int8_t playerW = -1;       // jugador del Blanco al que afecta (índice en su equipo), −1 ninguno
  int8_t playerB = -1;       // ídem del Azul
  Bar bar = Bar::Portero;
  uint16_t seq = 0;          // número de hándicap del partido (1, 2…)
};

struct Visitor {
  Animal animal = Animal::Squirrel;
  int8_t from = -1;          // ardilla: equipo al que roba
  int8_t to = -1;            // gato: equipo al que regala el gol
  bool time = false;         // caracol: un minuto más (true) o un gol más de meta (false)
};

struct Config {
  EndCondition endCondition = EndCondition::Goals;
  uint8_t goalsPerPeriod = 5;     // 1–20
  uint8_t minutesPerPeriod = 5;   // 1–30
  uint16_t overtimeSeconds = 60;
  uint8_t penaltyRounds = 5;
  Team penaltyFirst = Team::White;
  bool chaos = false;             // Partido Loco: hándicaps y visitas
  uint8_t players[2] = {1, 1};    // jugadores de cada equipo (para algunos hándicaps; 1 si no se eligieron)
  bool allowDraw = false;         // se puede empatar (liga de torneo): solo cuenta a tiempo o «ambas»

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
  uint8_t value;    // lo que vale (1; 2 o 3 con hándicap; 0 con el marcador congelado)
  bool steal;       // además quita un gol al rival (hándicap ROBO)
  Bonus bonus;
};

// Travesura de un animal que ya ha cambiado el marcador (ardilla: −1 y +1; gato: +1).
struct VisitMark {
  Visitor v;
  Period period;
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
  static const uint32_t HANDICAP_MIN_GAP_MS = 30000;
  static const uint32_t HANDICAP_MAX_GAP_MS = 60000;
  static const uint32_t HANDICAP_END_PAUSE_MS = 3000;
  static const uint32_t AFTER_GOAL_MS = 4500;  // sin hándicap en plena celebración de un gol
  static const uint8_t MAX_VISITS = 2;
  static const uint8_t MAX_VISIT_MARKS = 4;

  // Empieza un partido: cuenta atrás de la 1ª parte.
  bool start(const Config& cfg, uint32_t now) {
    if (!cfg.valid()) return false;
    *this = Engine();
    cfg_ = cfg;
    phase_ = Phase::Countdown;
    period_ = Period::First;
    countdownEnds_ = now + COUNTDOWN_MS;
    active_ = true;
    rng_ = (now * 2654435761u) ^ 0x9E3779B9u;
    if (!rng_) rng_ = 1;
    // Primer hándicap: entre 30 y 45 s de juego.
    nextHandicapAt_ = HANDICAP_MIN_GAP_MS + rnd(15000);
    return true;
  }

  bool active() const { return active_; }

  // ---------------------------------------------------------------- lectura
  Phase phase() const { return phase_; }
  Period period() const { return period_; }
  const Config& config() const { return cfg_; }

  // Como la app (sumGoals + applyVisits): cada gol suma lo que vale, el ROBO quita uno al rival, la ardilla pasa
  // un gol de un equipo al otro y el gato regala uno; nunca por debajo de 0.
  Score score() const { return scoreOf(false, period_); }
  Score periodScore(Period p) const { return scoreOf(true, p); }

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

  // 0 si el tiempo no termina el periodo (POR GOLES). Con el minuto que añada el caracol.
  uint32_t periodLimitMs() const {
    uint32_t base;
    if (period_ == Period::Overtime) base = (uint32_t)cfg_.overtimeSeconds * 1000UL;
    else if (period_ == Period::Shootout || cfg_.endCondition == EndCondition::Goals) return 0;
    else base = (uint32_t)cfg_.minutesPerPeriod * 60000UL;
    return base + timeAdjustMs_;
  }

  // Goles para ganar (con el que añada el caracol).
  uint8_t goalTarget() const { return (uint8_t)(cfg_.goalsPerPeriod + targetAdjust_); }

  uint32_t periodElapsed(uint32_t now) const {
    uint32_t since = now - runningSince_;
    uint32_t e = elapsedMs_ + (running_ && (int32_t)since >= 0 ? since : 0);
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
    return (int32_t)(now - countdownEnds_) >= 0 ? 0 : countdownEnds_ - now;
  }

  uint32_t lockRemaining(uint32_t now) const {
    if (!hasLastGoal_) return 0;
    uint32_t since = now - lastGoalAt_;
    return (int32_t)since < 0 ? LOCK_MS : since >= LOCK_MS ? 0 : LOCK_MS - since;
  }

  bool canUndo() const { return undoCount_ > 0 && (phase_ == Phase::Playing || phase_ == Phase::Paused); }

  Team nextKicker() const { return (kickCount_ % 2 == 0) ? cfg_.penaltyFirst : other(cfg_.penaltyFirst); }

  bool suddenDeath() const {
    Score a = penaltyAttempts();
    return a.white >= cfg_.penaltyRounds && a.blue >= cfg_.penaltyRounds;
  }

  Team winner() const { return winner_; }  // con draw() no hay ganador: mirar draw() antes
  bool draw() const { return draw_; }
  Reason reason() const { return reason_; }
  uint32_t totalTimeMs() const { return closedMs_; }
  // Tiempo de juego que duró cada parte ya cerrada (0 si no se jugó), para guardar el partido como la app.
  uint32_t periodDuration(Period p) const { return periodMs_[(uint8_t)p & 3]; }
  uint8_t visitMarkCount() const { return visitMarkCount_; }
  const VisitMark& visitMark(uint8_t i) const { return visitMarks_[i]; }

  // Equipos que ganarían con el siguiente gol (bit 0 Blanco, bit 1 Azul).
  uint8_t matchPoint() const {
    if (phase_ != Phase::Playing && phase_ != Phase::Paused) return 0;
    if (period_ == Period::Overtime) return 3;
    if (cfg_.endCondition == EndCondition::Time) return 0;
    // A un gol del objetivo de goles para ganar.
    Score s = score();
    uint8_t m = 0;
    if (s.white + 1 >= goalTarget()) m |= 1;
    if (s.blue + 1 >= goalTarget()) m |= 2;
    return m;
  }

  // ---------------------------------------------------------------- Partido Loco (lectura)
  HStage handicapStage() const { return hStage_; }
  const Handicap& handicap() const { return hSpec_; }
  bool hasReplaced() const { return hasReplaced_; }
  const Handicap& replaced() const { return hReplaced_; }
  // Hándicap activo (corriendo) o null.
  const Handicap* activeHandicap() const { return hStage_ == HStage::Active ? &hSpec_ : nullptr; }
  // Tiempo de juego que le queda al activo; −1 si no tiene duración.
  int32_t handicapRemaining(uint32_t now) const {
    if (hStage_ != HStage::Active || !hSpec_.durationMs) return -1;
    uint32_t total = closedMs_ + periodElapsed(now);
    uint32_t end = hStartTotal_ + hSpec_.durationMs;
    return total >= end ? 0 : (int32_t)(end - total);
  }
  // «Vuelta a la normalidad»: lo que falta para seguir solo.
  uint32_t endingRemaining(uint32_t now) const {
    if (hStage_ != HStage::Ending) return 0;
    return (int32_t)(now - hResumeAt_) >= 0 ? 0 : hResumeAt_ - now;
  }
  // Último hándicap terminado y por qué (para los avisos).
  HEnd lastEnd() const { return lastEnd_; }
  const Handicap& lastEnded() const { return lastEnded_; }
  uint16_t endSeq() const { return endSeq_; }
  // Visita en curso (fase Visit) y la última aplicada.
  const Visitor& visitor() const { return visit_; }
  bool visitApplied() const { return visitApplied_; }
  uint16_t visitSeq() const { return visitSeq_; }
  uint32_t visitStartedAt() const { return visitAt_; }
  // Último penalti del hándicap (para el cartel): 0 ninguno, 1 gol, 2 fallo; y de qué equipo.
  uint8_t lastHandicapPenalty() const { return lastPenalty_; }
  Team lastHandicapPenaltyTeam() const { return lastPenaltyTeam_; }
  uint16_t penaltySeq() const { return penaltySeq_; }
  // Semilla del azar (las pruebas la fijan para que el partido sea siempre igual).
  void seed(uint32_t s) { rng_ = s ? s : 1; }

  // ---------------------------------------------------------------- avance del reloj
  // Devuelve true si cambió el estado (fin de cuenta atrás o de periodo por tiempo).
  bool tick(uint32_t now) {
    // Una hora capturada antes de reanudar no equivale a días transcurridos.
    // Diferencias firmadas conservan el rollover de millis dentro de 2^31 ms.
    if (running_ && (int32_t)(now - runningSince_) < 0) return false;
    bool changed = false;
    for (int guard = 0; guard < 6; guard++) {
      if (phase_ == Phase::Countdown && (int32_t)(now - countdownEnds_) >= 0) {
        startClock(countdownEnds_);
        changed = true;
        continue;
      }
      // «Vuelta a la normalidad»: se sigue jugando solo tras la pausa breve.
      if (phase_ == Phase::Handicap && hStage_ == HStage::Ending && (int32_t)(now - hResumeAt_) >= 0) {
        resumeAt(hResumeAt_);
        hStage_ = HStage::None;
        changed = true;
        continue;
      }
      // Visita: la travesura a su tiempo y luego se sigue jugando.
      if (phase_ == Phase::Visit) {
        if (!visitApplied_ && (int32_t)(now - (visitAt_ + visitApplyMs())) >= 0) {
          applyVisit();
          changed = true;
        }
        if ((int32_t)(now - (visitAt_ + visitEndMs())) >= 0) {
          uint32_t at = visitAt_ + visitEndMs();
          resumeAt(at);
          checkGoalEnd(at);
          changed = true;
          continue;
        }
        break;
      }
      uint32_t lim = periodLimitMs();
      if (phase_ == Phase::Playing && running_) {
        uint32_t periodEndWall = lim ? runningSince_ + (lim - (elapsedMs_ < lim ? elapsedMs_ : lim)) : 0;
        // Fin de un hándicap con duración (si llega antes que el final de la parte).
        if (hStage_ == HStage::Active && hSpec_.durationMs) {
          uint32_t endTotal = hStartTotal_ + hSpec_.durationMs;
          uint32_t base = closedMs_ + elapsedMs_;
          uint32_t endWall = runningSince_ + (endTotal > base ? endTotal - base : 0);
          if ((int32_t)(now - endWall) >= 0 && (!lim || (int32_t)(endWall - periodEndWall) < 0)) {
            handicapTimeUp(endWall);
            changed = true;
            continue;
          }
        }
        if (lim) {
          uint32_t raw = elapsedMs_ + (now - runningSince_);
          if (raw >= lim) {
            endPeriod(runningSince_ + (lim - elapsedMs_), PeriodEndBy::Time);
            changed = true;
            continue;
          }
        }
        // Partido Loco: ¿toca hándicap o visita? (nunca en la prórroga ni justo después de un gol)
        if (cfg_.chaos && period_ != Period::Overtime && lockRemaining(now) == 0 &&
            (!hasLastGoal_ || now - lastGoalAt_ > AFTER_GOAL_MS)) {
          uint32_t total = closedMs_ + periodElapsed(now);
          if (total >= nextHandicapAt_) {
            startHandicap(now, total);
            changed = true;
            break;
          }
          if (visitPending_ && total >= nextVisitAt_) {
            visitPending_ = false;
            if (pickVisitor(visit_)) {
              elapsedMs_ = periodElapsed(now);
              running_ = false;
              phase_ = Phase::Visit;
              visitAt_ = now;
              visitApplied_ = false;
              visitSeq_++;
              changed = true;
            }
            break;
          }
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
    // Partido Loco: el hándicap activo cambia el valor del gol.
    uint8_t value = 1;
    bool steal = false;
    Bonus bonus = Bonus::None;
    const Handicap* h = activeHandicap();
    if (h && (h->kind == HKind::DoubleAll || (h->kind == HKind::DoubleTeam && h->team == (uint8_t)t))) {
      value = 2;
      bonus = Bonus::Double;
    } else if (h && h->kind == HKind::TripleNext) {
      value = 3;
      bonus = Bonus::Triple;
    } else if (h && h->kind == HKind::Steal && h->team == (uint8_t)t && score().of(other(t)) > 0) {
      steal = true;
      bonus = Bonus::Steal;
    } else if (h && h->kind == HKind::FreezeScore) {
      value = 0;
      bonus = Bonus::Frozen;
    }
    goals_[goalCount_] = Goal{t, period_, periodElapsed(now), false, value, steal, bonus};
    pushUndo(Action{ActionKind::Goal, goalCount_});
    goalCount_++;
    lastGoalAt_ = now;
    hasLastGoal_ = true;
    // El gol triple y el robo se gastan con el gol.
    if (h && (h->kind == HKind::TripleNext || (h->kind == HKind::Steal && h->team == (uint8_t)t))) endHandicap(HEnd::Used);
    checkGoalEnd(now);
    return Result::Accepted;
  }

  // ---------------------------------------------------------------- Partido Loco (órdenes)
  // Seguir tras el anuncio (empieza a correr) o tras «vuelta a la normalidad».
  Result handicapGo(uint32_t now) {
    tick(now);
    if (phase_ != Phase::Handicap) return Result::InvalidState;
    if (hStage_ == HStage::Announce) {
      if (hSpec_.kind == HKind::Penalty) return Result::InvalidState;
      hStartTotal_ = closedMs_ + elapsedMs_;
      hStage_ = HStage::Active;
      resumeAt(now);
      return Result::Accepted;
    }
    hStage_ = HStage::None;
    resumeAt(now);
    return Result::Accepted;
  }

  // Penalti del hándicap: GOL (vale 1) o FALLO, y se sigue jugando.
  Result handicapPenalty(bool scored, uint32_t now) {
    tick(now);
    if (phase_ != Phase::Handicap || hStage_ != HStage::Announce || hSpec_.kind != HKind::Penalty) return Result::InvalidState;
    Team t = hSpec_.team == 1 ? Team::Blue : Team::White;
    hStage_ = HStage::None;
    resumeAt(now);
    lastPenalty_ = scored ? 1 : 2;
    lastPenaltyTeam_ = t;
    penaltySeq_++;
    if (!scored) return Result::Accepted;
    if (goalCount_ >= MAX_GOALS) return Result::Accepted;
    goals_[goalCount_] = Goal{t, period_, periodElapsed(now), false, 1, false, Bonus::Penalty};
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
    timeAdjustMs_ = 0;  // el minuto del caracol solo vale para su parte
  }

  // Sigue el reloj (tras un anuncio, una visita o la vuelta a la normalidad).
  void resumeAt(uint32_t at) {
    phase_ = Phase::Playing;
    runningSince_ = at;
    running_ = true;
  }

  Score scoreOf(bool onlyPeriod, Period p) const {
    int w = 0, b = 0;
    for (uint8_t i = 0; i < goalCount_; i++) {
      const Goal& g = goals_[i];
      if (g.annulled || (onlyPeriod && g.period != p)) continue;
      (g.team == Team::White ? w : b) += g.value;
      if (g.steal) (g.team == Team::White ? b : w) -= 1;
    }
    if (w < 0) w = 0;
    if (b < 0) b = 0;
    for (uint8_t i = 0; i < visitMarkCount_; i++) {
      const VisitMark& m = visitMarks_[i];
      if (onlyPeriod && m.period != p) continue;
      if (m.v.animal == Animal::Squirrel && m.v.from >= 0) {
        (m.v.from == 0 ? w : b) -= 1;
        (m.v.from == 0 ? b : w) += 1;
      } else if (m.v.animal == Animal::Cat && m.v.to >= 0) {
        (m.v.to == 0 ? w : b) += 1;
      }
    }
    Score s;
    s.white = (uint8_t)(w < 0 ? 0 : w > 255 ? 255 : w);
    s.blue = (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b);
    return s;
  }

  // Azar propio (xorshift): reproducible con seed().
  uint32_t rnd(uint32_t n) {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return n ? rng_ % n : 0;
  }
  bool chance(uint32_t pct) { return rnd(100) < pct; }

  // Equipo que va perdiendo; con empate, uno al azar.
  uint8_t trailing() {
    Score s = score();
    if (s.white == s.blue) return (uint8_t)rnd(2);
    return s.white < s.blue ? 0 : 1;
  }

  // Sorteo como pickHandicap de la app (marcador, jugadores y el anterior).
  Handicap pickHandicap() {
    uint8_t sizeW = cfg_.players[0] ? cfg_.players[0] : 1, sizeB = cfg_.players[1] ? cfg_.players[1] : 1;
    Score sc = score();
    bool tied = sc.white == sc.blue;
    HKind allowed[(int)HKind::Count];
    uint8_t n = 0;
    for (uint8_t k = 0; k < (uint8_t)HKind::Count; k++) {
      HKind kind = (HKind)k;
      if (hadHandicap_ && kind == lastKind_) continue;
      if (kind == HKind::SwapPositions && !(sizeW >= 2 || sizeB >= 2)) continue;
      if (kind == HKind::Transfer && !(sizeW >= 2 && sizeB >= 2)) continue;
      if (kind == HKind::Steal && tied) continue;
      allowed[n++] = kind;
    }
    Handicap h;
    h.kind = allowed[rnd(n)];
    h.durationMs = handicapDuration(h.kind);
    h.team = 2;
    switch (h.kind) {
      case HKind::DoubleTeam:
      case HKind::Steal:
        h.team = trailing();
        break;
      case HKind::Penalty:
        h.team = (uint8_t)rnd(2);
        break;
      case HKind::SwapPositions: {
        bool w = sizeW >= 2, b = sizeB >= 2;
        h.team = (w && b) ? (chance(40) ? 2 : (uint8_t)rnd(2)) : (w ? 0 : 1);
        break;
      }
      case HKind::FrozenPlayer:
      case HKind::BarLock: {
        if (chance(50)) h.team = 2;
        else h.team = (uint8_t)rnd(2);
        if (h.team != 1) h.playerW = (int8_t)rnd(sizeW);
        if (h.team != 0) h.playerB = (int8_t)rnd(sizeB);
        if (h.kind == HKind::BarLock) h.bar = (Bar)rnd(4);
        break;
      }
      case HKind::Transfer:
        h.playerW = (int8_t)rnd(sizeW);
        h.playerB = (int8_t)rnd(sizeB);
        break;
      default:
        break;
    }
    return h;
  }

  // Se para el reloj y se anuncia; el anterior (si lo había) termina aquí.
  void startHandicap(uint32_t now, uint32_t total) {
    hasReplaced_ = hStage_ == HStage::Active;
    if (hasReplaced_) {
      hReplaced_ = hSpec_;
      endHandicap(HEnd::Replaced);
    }
    Handicap h = pickHandicap();
    h.seq = ++handicapSeq_;
    elapsedMs_ = periodElapsed(now);
    running_ = false;
    phase_ = Phase::Handicap;
    hSpec_ = h;
    hStage_ = HStage::Announce;
    lastKind_ = h.kind;
    hadHandicap_ = true;
    nextHandicapAt_ = total + HANDICAP_MIN_GAP_MS + rnd(HANDICAP_MAX_GAP_MS - HANDICAP_MIN_GAP_MS);
    // A veces (35 %), un animal entre este hándicap y el siguiente, con 8 s de margen a cada lado; dos como mucho.
    visitPending_ = false;
    if (visitCount_ < MAX_VISITS && chance(35)) {
      uint32_t from = total + h.durationMs + 8000, to = nextHandicapAt_ - 8000;
      if (to > from) {
        nextVisitAt_ = from + rnd(to - from);
        visitPending_ = true;
      }
    }
  }

  // Termina el hándicap sin parar el juego (usado, sustituido o fin de la parte).
  void endHandicap(HEnd why) {
    if (hStage_ == HStage::None) return;
    lastEnded_ = hSpec_;
    lastEnd_ = why;
    endSeq_++;
    hStage_ = HStage::None;
  }

  // Se acabó el tiempo de un hándicap: reloj parado y «vuelta a la normalidad» 3 s.
  void handicapTimeUp(uint32_t at) {
    elapsedMs_ = periodElapsed(at);
    running_ = false;
    lastEnded_ = hSpec_;
    lastEnd_ = HEnd::Time;
    endSeq_++;
    phase_ = Phase::Handicap;
    hStage_ = HStage::Ending;
    hResumeAt_ = at + HANDICAP_END_PAUSE_MS;
  }

  // Animal y travesura sin decidir el partido (pickVisitor de la app); false si ninguno puede salir.
  bool pickVisitor(Visitor& out) {
    Score sc = score();
    uint8_t target = goalTarget();
    bool timed = periodLimitMs() != 0;
    bool goalsDecide = !(timed && cfg_.endCondition == EndCondition::Time);
    Visitor opts[3];
    uint8_t n = 0;
    // Ardilla: roba a un equipo con goles (mejor al que va ganando), sin hacer ganar al otro.
    int8_t victims[2];
    uint8_t nv = 0;
    for (uint8_t t = 0; t < 2; t++) {
      uint8_t mine = t ? sc.blue : sc.white, thief = t ? sc.white : sc.blue;
      if (!mine) continue;
      if (!goalsDecide || thief + 1 < target) victims[nv++] = (int8_t)t;
    }
    if (nv) {
      int8_t leader = sc.white == sc.blue ? -1 : (sc.white > sc.blue ? 0 : 1);
      bool leaderIn = leader >= 0 && (victims[0] == leader || (nv > 1 && victims[1] == leader));
      Visitor v;
      v.animal = Animal::Squirrel;
      v.from = (leaderIn && chance(75)) ? leader : victims[rnd(nv)];
      opts[n++] = v;
    }
    // Caracol: alarga (tiempo o meta), como mucho dos veces.
    if (snailCount_ < 2) {
      Visitor v;
      v.animal = Animal::Snail;
      v.time = timed;
      opts[n++] = v;
    }
    // Gato vomitón: un gol (mejor al que va perdiendo), sin que decida el partido.
    int8_t lucky[2];
    uint8_t nl = 0;
    for (uint8_t t = 0; t < 2; t++)
      if (!goalsDecide || (t ? sc.blue : sc.white) + 1 < target) lucky[nl++] = (int8_t)t;
    if (nl) {
      int8_t loser = sc.white == sc.blue ? -1 : (sc.white < sc.blue ? 0 : 1);
      bool loserIn = loser >= 0 && (lucky[0] == loser || (nl > 1 && lucky[1] == loser));
      Visitor v;
      v.animal = Animal::Cat;
      v.to = (loserIn && chance(65)) ? loser : lucky[rnd(nl)];
      opts[n++] = v;
    }
    if (!n) return false;
    out = opts[rnd(n)];
    return true;
  }

  uint32_t visitApplyMs() const {
    return visit_.animal == Animal::Squirrel ? 4700 : visit_.animal == Animal::Snail ? 3800 : 5600;
  }
  uint32_t visitEndMs() const {
    return visit_.animal == Animal::Squirrel ? 7800 : visit_.animal == Animal::Snail ? 6600 : 8600;
  }

  void applyVisit() {
    visitApplied_ = true;
    visitCount_++;
    if (visit_.animal == Animal::Snail) {
      snailCount_++;
      if (visit_.time) timeAdjustMs_ += 60000;
      else targetAdjust_++;
    } else if (visitMarkCount_ < MAX_VISIT_MARKS) {
      visitMarks_[visitMarkCount_++] = VisitMark{visit_, period_};
    }
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
    if (s.white >= goalTarget() || s.blue >= goalTarget()) endPeriod(now, PeriodEndBy::Goals);
  }

  void endPeriod(uint32_t at, PeriodEndBy by) {
    endHandicap(HEnd::PeriodEnd);  // un hándicap no pasa a la parte siguiente
    uint32_t e = periodElapsed(at);
    elapsedMs_ = e;
    running_ = false;
    phase_ = Phase::PeriodEnd;
    undoCount_ = 0;
    closedMs_ += e;
    periodMs_[(uint8_t)period_ & 3] = e;
    Score s = score();
    if (by == PeriodEndBy::Goals) {
      finish(s.white > s.blue ? Team::White : Team::Blue, Reason::Regulation, at);
    } else if (period_ == Period::Second && s.white != s.blue) {
      finish(s.white > s.blue ? Team::White : Team::Blue, Reason::Regulation, at);
    } else if (period_ == Period::Second && cfg_.allowDraw) {
      // Liga: igualados al final del tiempo, empate (sin prórroga ni penaltis).
      finish(Team::White, Reason::Regulation, at);
      draw_ = true;
    } else if (period_ == Period::Overtime && by == PeriodEndBy::GoldenGoal) {
      finish(s.white > s.blue ? Team::White : Team::Blue, Reason::GoldenGoal, at);
    }
  }

  void finish(Team w, Reason r, uint32_t) {
    winner_ = w;
    draw_ = false;
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
  uint32_t periodMs_[4] = {0, 0, 0, 0};
  uint32_t lastGoalAt_ = 0;
  bool hasLastGoal_ = false;
  Goal goals_[MAX_GOALS];
  uint8_t goalCount_ = 0;
  Action undo_[MAX_UNDO];
  uint8_t undoCount_ = 0;
  Kick kicks_[MAX_KICKS];
  uint8_t kickCount_ = 0;
  Team winner_ = Team::White;
  bool draw_ = false;
  Reason reason_ = Reason::Regulation;
  // Partido Loco
  uint32_t rng_ = 1;
  uint32_t nextHandicapAt_ = 0xFFFFFFFFu;
  HStage hStage_ = HStage::None;
  Handicap hSpec_;
  Handicap hReplaced_;
  bool hasReplaced_ = false;
  uint32_t hStartTotal_ = 0;
  uint32_t hResumeAt_ = 0;
  HKind lastKind_ = HKind::DoubleAll;
  bool hadHandicap_ = false;
  uint16_t handicapSeq_ = 0;
  HEnd lastEnd_ = HEnd::None;
  Handicap lastEnded_;
  uint16_t endSeq_ = 0;
  uint32_t timeAdjustMs_ = 0;
  uint8_t targetAdjust_ = 0;
  bool visitPending_ = false;
  uint32_t nextVisitAt_ = 0;
  Visitor visit_;
  bool visitApplied_ = false;
  uint32_t visitAt_ = 0;
  uint16_t visitSeq_ = 0;
  uint8_t visitCount_ = 0;
  uint8_t snailCount_ = 0;
  VisitMark visitMarks_[MAX_VISIT_MARKS];
  uint8_t visitMarkCount_ = 0;
  uint8_t lastPenalty_ = 0;
  Team lastPenaltyTeam_ = Team::White;
  uint16_t penaltySeq_ = 0;
};

}  // namespace mfv3

#endif  // MFV3_ENGINE_H
