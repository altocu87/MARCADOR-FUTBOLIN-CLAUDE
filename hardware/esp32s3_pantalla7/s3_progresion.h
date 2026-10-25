// MARCADOR FUTBOLÍN V3 · ELO, experiencia, niveles, rangos, logros y retos en la pantalla de 7".
// -------------------------------------------------------------------------------------------------
// Copia de computeProgression (src/services/progression/progression.ts) con sus reglas (rules.ts, gameRules.ts,
// achievements.ts, challenges.ts y calendar.ts): se recalcula todo desde el historial de partidos (s3_partidos.h),
// en orden de cuándo terminaron, igual que la app. Los jugadores se conocen por su código (FNV-1a del id de la
// app). tests/fixtures/progresion (partidos del modo prueba de la app y lo que calcula la app con ellos) comprueba
// en el PC que la placa da lo mismo (hardware/test/progresion_test.cpp); si la app cambia una regla, esa prueba
// falla hasta que se cambie también aquí.
// Torneos (los resúmenes de s3_torneos_datos.h, con setTournaments): los partidos de un torneo que sigue en juego
// solo mueven el ELO; su XP, sus logros y sus retos esperan a que termine (`deferred`). El partido que cierra un
// torneo da «Ganar torneo» a los campeones y cuenta para los logros Campeón y Tricampeón.

#ifndef MFV3_S3_PROGRESION_H
#define MFV3_S3_PROGRESION_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "s3_partidos.h"
#include "s3_memory.h"

namespace progresion {

// ---------------------------------------------------------------- reglas (DEFAULT_RULES de gameRules.ts)
struct Rules {
  int eloInitial = 1200, kProvisional = 40, provisionalMatches = 10, kEstablished = 20;
  bool goalDiffMultiplier = false;
  bool challenges = true;
  int rank[6] = {1225, 1275, 1350, 1425, 1500, 1600};  // Madera, Bronce, Plata, Oro, Platino, Diamante
  int xpComplete = 50, xpWin = 100, xpDraw = 60, xpLoss = 25, xpRankedWinBonus = 50, xpOvertimeWin = 25, xpPenaltiesWin = 25;
  int xpTournamentWin = 300;
  int xpAchievement[3] = {25, 50, 100};  // común, rara, épica
  int xpChallengeDaily = 40, xpChallengeWeekly = 100;
  double levelBase = 100, levelExponent = 1.35;
  int maxLevel = 100;
  bool seasonMonth = true;  // temporadas por meses (false: trimestres)
};
static Rules rules;

static const char* const CATEGORY[7] = {"Chatarra", "Madera", "Bronce", "Plata", "Oro", "Platino", "Diamante"};
static const uint32_t CATEGORY_COLOR[7] = {0x9C8F86, 0xB5814A, 0xC98A54, 0xC9D3E0, 0xF2C94C, 0x7FE3D6, 0x8AB8FF};

static uint8_t categoryFor(int elo) {
  uint8_t c = 0;
  for (int i = 0; i < 6; i++)
    if (elo >= rules.rank[i]) c = (uint8_t)(i + 1);
  return c;
}
static long xpForLevel(int level) { return level <= 0 ? 0 : lround(rules.levelBase * pow((double)level, rules.levelExponent)); }
static int levelForXp(long xp) {
  int l = 0;
  while (l < rules.maxLevel && xp >= xpForLevel(l + 1)) l++;
  return l;
}
static double levelProgress(long xp) {
  int l = levelForXp(xp);
  if (l >= rules.maxLevel) return 1;
  long a = xpForLevel(l), b = xpForLevel(l + 1);
  return (double)(xp - a) / (double)(b - a);
}
static double goalDiffMult(int diff) {
  int d = abs(diff);
  return d <= 1 ? 1 : d == 2 ? 1.05 : d == 3 ? 1.1 : d == 4 ? 1.15 : 1.2;
}
static int jsRound(double x) { return (int)floor(x + 0.5); }  // Math.round de JS (las mitades, hacia arriba)

// ---------------------------------------------------------------- logros (achievements.ts, en el mismo orden)
enum Rarity : uint8_t { COMMON, RARE, EPIC };
struct Ach {
  const char* id;
  const char* name;
  const char* title;  // nullptr si no da título
  Rarity rarity;
};
static const Ach ACH[] = {
    {"debut", "Debut", nullptr, COMMON},
    {"veteran", "Veterano", "Veterano", RARE},
    {"legend", "Leyenda de la mesa", "Leyenda", EPIC},
    {"level10", "Nivel 10", nullptr, RARE},
    {"level25", "Nivel 25", nullptr, EPIC},
    {"level50", "Medio centenar", "Gran Maestro", EPIC},
    {"challenger", "Retador", nullptr, COMMON},
    {"tireless", "Incansable", "Incansable", RARE},
    {"first_win", "Primera victoria", nullptr, COMMON},
    {"wins10", "Diez victorias", nullptr, RARE},
    {"wins50", "Cincuenta victorias", nullptr, EPIC},
    {"wins100", "Centenario", "Centenario", EPIC},
    {"ranked_wins10", "Competitivo", nullptr, RARE},
    {"express", "Exprés", "Relámpago", RARE},
    {"dynamic_duo", "Dúo dinámico", "Dúo Dinámico", RARE},
    {"streak3", "En racha", nullptr, COMMON},
    {"streak5", "Imparable", "Imparable", RARE},
    {"streak10", "Invencible", "Invencible", EPIC},
    {"phoenix", "Fénix", "Fénix", RARE},
    {"shutout", "Portería a cero", "El Muro", RARE},
    {"rout", "Goleada", nullptr, RARE},
    {"double_digits", "Doble dígito", nullptr, COMMON},
    {"goal_fest", "Festival", nullptr, COMMON},
    {"early_goal", "Madrugador", nullptr, RARE},
    {"run5", "Apisonadora", "Apisonadora", RARE},
    {"hat_trick", "Hat-trick", nullptr, RARE},
    {"scorer50", "Goleador", "Pichichi", EPIC},
    {"first_blood", "Primera sangre", nullptr, COMMON},
    {"ranked_debut", "Competidor", nullptr, COMMON},
    {"platinum", "Platino", nullptr, RARE},
    {"diamond", "Diamante", nullptr, EPIC},
    {"elite", "Élite", "Élite", EPIC},
    {"giant_killer", "Matagigantes", "Matagigantes", RARE},
    {"champion", "Campeón", "Campeón", EPIC},
    {"tri_champion", "Tricampeón", "Tricampeón", EPIC},
    {"golden_goal", "Gol de oro", "Rey de la Prórroga", RARE},
    {"ice_cold", "Sangre fría", "Sangre Fría", RARE},
    {"perfect_pens", "Penaltis perfectos", nullptr, RARE},
    {"sudden_death", "Al límite", nullptr, RARE},
    {"eternal", "Tanda eterna", nullptr, RARE},
    {"comeback", "Remontada", "El Remontador", EPIC},
    {"against_ropes", "Contra las cuerdas", nullptr, EPIC},
    {"impossible", "Misión imposible", nullptr, EPIC},
    {"chaos_win", "Locura controlada", nullptr, COMMON},
    {"chaos_lord", "Señor de la Locura", "Señor de la Locura", RARE},
    {"social", "Sociable", nullptr, COMMON},
    {"globetrotter", "Trotamundos", nullptr, RARE},
    {"night_owl", "Búho", "Búho Nocturno", COMMON},
    {"early_bird", "Al alba", nullptr, COMMON},
    {"marathon_day", "Día de maratón", nullptr, RARE},
    {"secret_marathon", "Maratón", nullptr, RARE},
};
const int ACH_COUNT = sizeof(ACH) / sizeof(ACH[0]);

// Lo que enseña la ficha de un jugador (pestaña LOGROS): descripción, apartado y si es secreto, en el orden de ACH.
// Copia de ACHIEVEMENTS de la app (tests/logrosPlaca.test.ts comprueba que coinciden).
static const char* const ACH_GROUP[6] = {"Progresión", "Victorias", "Rachas", "Goles", "Competición", "Especiales"};
static const char* const RARITY_NAME[3] = {"Común", "Rara", "Épica"};
struct AchInfo {
  const char* description;
  uint8_t group;
  bool secret;
};
static const AchInfo ACH_INFO[] = {
    {"Completa tu primer partido.", 0, false},  // debut
    {"Completa 25 partidos.", 0, false},  // veteran
    {"Completa 100 partidos.", 0, false},  // legend
    {"Alcanza el nivel 10.", 0, false},  // level10
    {"Alcanza el nivel 25.", 0, false},  // level25
    {"Alcanza el nivel 50.", 0, true},  // level50
    {"Completa tu primer reto.", 0, false},  // challenger
    {"Completa 10 retos.", 0, false},  // tireless
    {"Gana tu primer partido.", 1, false},  // first_win
    {"Gana 10 partidos.", 1, false},  // wins10
    {"Gana 50 partidos.", 1, false},  // wins50
    {"Gana 100 partidos.", 1, false},  // wins100
    {"Gana 10 clasificatorios.", 1, false},  // ranked_wins10
    {"Gana un partido en menos de 3 minutos.", 1, false},  // express
    {"Gana 10 partidos con el mismo compañero.", 1, false},  // dynamic_duo
    {"Gana 3 partidos seguidos.", 2, false},  // streak3
    {"Gana 5 partidos seguidos.", 2, false},  // streak5
    {"Gana 10 partidos seguidos.", 2, false},  // streak10
    {"Gana tras perder 5 seguidos.", 2, true},  // phoenix
    {"Gana sin encajar ningún gol.", 3, false},  // shutout
    {"Gana por 5 o más goles de diferencia.", 3, false},  // rout
    {"Tu equipo marca 10 o más goles en un partido.", 3, false},  // double_digits
    {"Juega un partido con 15 o más goles.", 3, false},  // goal_fest
    {"Tu equipo marca en los primeros 10 segundos de una parte.", 3, false},  // early_goal
    {"Tu equipo marca 5 goles seguidos sin respuesta.", 3, false},  // run5
    {"Marca 3 goles apuntados a ti en un partido.", 3, false},  // hat_trick
    {"Llega a 50 goles apuntados a ti.", 3, false},  // scorer50
    {"Marca (apuntado a ti) el primer gol de un partido.", 3, true},  // first_blood
    {"Juega tu primer partido clasificatorio.", 4, false},  // ranked_debut
    {"Alcanza 1500 de ELO.", 4, false},  // platinum
    {"Alcanza 1600 de ELO.", 4, false},  // diamond
    {"Alcanza 1800 de ELO.", 4, false},  // elite
    {"Gana un clasificatorio contra rivales con 150+ ELO más.", 4, false},  // giant_killer
    {"Gana un torneo.", 4, false},  // champion
    {"Gana 3 torneos.", 4, true},  // tri_champion
    {"Gana un partido en la prórroga.", 5, false},  // golden_goal
    {"Gana una tanda de penaltis.", 5, false},  // ice_cold
    {"Tu equipo no falla ningún penalti en una tanda.", 5, false},  // perfect_pens
    {"Gana una tanda en muerte súbita.", 5, false},  // sudden_death
    {"Juega una muerte súbita de 3 o más rondas.", 5, true},  // eternal
    {"Gana tras ir perdiendo por 2 o más goles.", 5, false},  // comeback
    {"Gana tras ir perdiendo por 3 o más goles.", 5, false},  // against_ropes
    {"Gana tras ir perdiendo por 4 o más goles.", 5, true},  // impossible
    {"Gana un Partido Loco.", 5, false},  // chaos_win
    {"Gana 10 Partidos Locos.", 5, false},  // chaos_lord
    {"Juega con 5 compañeros distintos.", 5, false},  // social
    {"Enfréntate a 10 rivales distintos.", 5, false},  // globetrotter
    {"Juega un partido entre las 23:00 y las 5:00.", 5, true},  // night_owl
    {"Juega un partido antes de las 9:00.", 5, true},  // early_bird
    {"Juega 10 partidos en un mismo día.", 5, true},  // marathon_day
    {"Juega un partido de más de 20 minutos.", 5, true},  // secret_marathon
};
static_assert(sizeof(ACH_INFO) / sizeof(ACH_INFO[0]) == sizeof(ACH) / sizeof(ACH[0]), "Logros: falta su descripción");

struct Ctx {
  const partidos::Row* m;
  bool won;
  int goalsFor, goalsAgainst, played, wins, winStreak, lossStreakBefore, rankedPlayed, rankedWins, chaosWins, elo, eloGapBefore;
  double eloGapBeforeD;
  int level, comeback, ownGoalRun, totalGoals, personalGoalsInMatch, personalGoalsTotal;
  long earliestOwnGoalMs;  // -1 = ninguno
  bool firstGoalOfMatch, penaltiesPerfect;
  int suddenDeathRounds, hour, matchesToday, distinctTeammates, distinctRivals, winsWithSameTeammate, tournamentsWon, challengesDone;
};

static bool check(int i, const Ctx& c) {
  const partidos::Row& m = *c.m;
  switch (i) {
    case 0: return c.played >= 1;
    case 1: return c.played >= 25;
    case 2: return c.played >= 100;
    case 3: return c.level >= 10;
    case 4: return c.level >= 25;
    case 5: return c.level >= 50;
    case 6: return c.challengesDone >= 1;
    case 7: return c.challengesDone >= 10;
    case 8: return c.won && c.wins >= 1;
    case 9: return c.wins >= 10;
    case 10: return c.wins >= 50;
    case 11: return c.wins >= 100;
    case 12: return c.rankedWins >= 10;
    case 13: return c.won && m.totalMs < 180000;
    case 14: return c.winsWithSameTeammate >= 10;
    case 15: return c.winStreak >= 3;
    case 16: return c.winStreak >= 5;
    case 17: return c.winStreak >= 10;
    case 18: return c.won && c.lossStreakBefore >= 5;
    case 19: return c.won && c.goalsAgainst == 0 && c.goalsFor > 0;
    case 20: return c.won && c.goalsFor - c.goalsAgainst >= 5;
    case 21: return c.goalsFor >= 10;
    case 22: return c.totalGoals >= 15;
    case 23: return c.earliestOwnGoalMs >= 0 && c.earliestOwnGoalMs <= 10000;
    case 24: return c.ownGoalRun >= 5;
    case 25: return c.personalGoalsInMatch >= 3;
    case 26: return c.personalGoalsTotal >= 50;
    case 27: return c.firstGoalOfMatch;
    case 28: return c.rankedPlayed >= 1;
    case 29: return c.elo >= 1500;
    case 30: return c.elo >= 1600;
    case 31: return c.elo >= 1800;
    case 32: return c.won && m.mode == 'r' && c.eloGapBeforeD >= 150;
    case 33: return c.tournamentsWon >= 1;
    case 34: return c.tournamentsWon >= 3;
    case 35: return c.won && m.reason == 'g';
    case 36: return c.won && m.reason == 'p';
    case 37: return c.penaltiesPerfect;
    case 38: return c.won && c.suddenDeathRounds >= 1;
    case 39: return c.suddenDeathRounds >= 3;
    case 40: return c.won && c.comeback >= 2;
    case 41: return c.won && c.comeback >= 3;
    case 42: return c.won && c.comeback >= 4;
    case 43: return c.won && m.mode == 'c';
    case 44: return c.chaosWins >= 10;
    case 45: return c.distinctTeammates >= 5;
    case 46: return c.distinctRivals >= 10;
    case 47: return c.hour >= 23 || c.hour < 5;
    case 48: return c.hour >= 5 && c.hour < 9;
    case 49: return c.matchesToday >= 10;
    case 50: return m.totalMs >= 20u * 60000u;
  }
  return false;
}

// ---------------------------------------------------------------- calendario (calendar.ts, hora de España)
static void dayKey(uint32_t fin, char* out) {
  reloj::Clock c = partidos::localOf(fin);
  snprintf(out, 11, "%04u-%02u-%02u", c.year, c.month, c.day);
}
static void weekKey(uint32_t fin, char* out) {
  reloj::Clock c = partidos::localOf(fin);
  // Semana ISO 8601 de la fecha local.
  reloj::Clock d = c;
  uint8_t wd = reloj::weekday(d);
  int day = wd ? wd : 7;
  // Días desde 2000-01-01 hasta la fecha, y el jueves de su semana.
  auto days = [](const reloj::Clock& k) { return (int32_t)((reloj::stamp(reloj::Clock{k.year, k.month, k.day, 0, 0, 0}) - 1) / 1440); };
  int32_t thu = days(d) + 4 - day;
  // Año del jueves
  uint16_t y = 2000;
  int32_t left = thu;
  for (;;) {
    int len = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    if (left < len) break;
    left -= len;
    y++;
  }
  int week = (int)ceil(((double)left + 1) / 7.0);
  snprintf(out, 9, "%04u-W%02d", y, week);
}
static void seasonKey(uint32_t fin, char* out) {
  reloj::Clock c = partidos::localOf(fin);
  if (rules.seasonMonth) snprintf(out, 8, "%04u-%02u", c.year, c.month);
  else snprintf(out, 8, "%04u-T%u", c.year, (c.month - 1) / 3 + 1);
}

// seededRandom de calendar.ts (FNV-1a + xorshift de 32 bits, igual que en JS).
struct Seeded {
  uint32_t x;
  explicit Seeded(const char* seed) {
    uint32_t h = 2166136261u;
    for (const char* p = seed; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    x = h;
  }
  double next() {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (double)(x % 1000000u) / 1000000.0;
  }
};

// ---------------------------------------------------------------- retos (challenges.ts)
enum Tpl : uint8_t { PLAY, WIN, WIN_RANKED, CLEAN_WIN, TEAM_GOALS, SHUTOUT, COMEBACK, BIG_WIN, CHAOS, DUO, TPL_COUNT };
static const uint8_t TPL_DAILY[TPL_COUNT] = {3, 2, 1, 1, 10, 1, 1, 1, 2, 1};
static const uint8_t TPL_WEEKLY[TPL_COUNT] = {10, 6, 3, 3, 40, 2, 1, 3, 5, 4};
static const char* const TPL_ID[TPL_COUNT] = {"play", "win", "win_ranked", "clean_win", "team_goals", "shutout", "comeback", "big_win", "chaos", "duo"};
static const char* const TPL_TEXT[TPL_COUNT] = {"Juega %u partidos", "Gana %u partidos", "Gana %u clasificatorios",
                                                "Gana %u partidos encajando 3 goles o menos", "Tu equipo marca %u goles",
                                                "Gana %u partido(s) sin encajar", "Remonta %u partido(s) tras ir 2 abajo",
                                                "Gana %u partido(s) por 3 o más", "Juega %u Partidos Locos", "Gana %u partidos de 2 vs 2"};

static int tplProgress(uint8_t tpl, const partidos::Row& m, int team) {
  bool won = (m.winner == 'w' && team == 0) || (m.winner == 'b' && team == 1);
  int f = team ? m.b : m.w, a = team ? m.w : m.b;
  switch (tpl) {
    case PLAY: return 1;
    case WIN: return won;
    case WIN_RANKED: return won && m.mode == 'r';
    case CLEAN_WIN: return won && a <= 3;
    case TEAM_GOALS: return f;
    case SHUTOUT: return won && a == 0;
    case COMEBACK: return won && m.comeback >= 2;
    case BIG_WIN: return won && f - a >= 3;
    case CHAOS: return m.mode == 'c';
    case DUO: return won && m.n[0] == 2 && m.n[1] == 2;
  }
  return 0;
}

struct Challenge {
  char key[40];  // «daily:2026-10-02:win»
  bool daily;
  uint8_t tpl, target;
  int xp;
};
// Retos activos cuando terminó un partido: 1 diario y 3 semanales.
static int activeChallenges(uint32_t fin, Challenge out[4]) {
  char dk[12], wk[10], seed[16];
  dayKey(fin, dk);
  weekKey(fin, wk);
  int n = 0;
  for (int scope = 0; scope < 2; scope++) {
    snprintf(seed, sizeof(seed), "%c%s", scope ? 'w' : 'd', scope ? wk : dk);
    Seeded rnd(seed);
    uint8_t pool[TPL_COUNT];
    int left = TPL_COUNT;
    for (int i = 0; i < TPL_COUNT; i++) pool[i] = (uint8_t)i;
    for (int k = 0; k < (scope ? 3 : 1) && left; k++) {
      int j = (int)floor(rnd.next() * left);
      uint8_t t = pool[j];
      memmove(pool + j, pool + j + 1, (size_t)(left - j - 1));
      left--;
      Challenge& c = out[n++];
      c.daily = !scope;
      c.tpl = t;
      c.target = scope ? TPL_WEEKLY[t] : TPL_DAILY[t];
      c.xp = scope ? rules.xpChallengeWeekly : rules.xpChallengeDaily;
      snprintf(c.key, sizeof(c.key), "%s:%s:%s", scope ? "weekly" : "daily", scope ? wk : dk, TPL_ID[t]);
    }
  }
  return n;
}

// Progreso de un jugador en los retos activos (challengeProgress de la app): suma lo de cada partido de su día o
// su semana, sin pasar de la meta. `value[c]` y `done[c]` por reto.
static void challengeProgress(uint32_t code, const Challenge* ch, int nch, uint32_t now, int value[4], bool done[4]) {
  char nowDk[12], nowWk[10], dk[12], wk[10];
  dayKey(now, nowDk);
  weekKey(now, nowWk);
  int sum[4] = {0, 0, 0, 0};
  for (int i = 0; i < partidos::count; i++) {
    const partidos::Row& m = partidos::rows[i];
    if (!m.fin) continue;
    int team = -1;
    for (int t = 0; t < 2 && team < 0; t++)
      for (int j = 0; j < m.n[t]; j++)
        if (m.code[t][j] == code) team = t;
    if (team < 0) continue;
    dayKey(m.fin, dk);
    weekKey(m.fin, wk);
    for (int c = 0; c < nch; c++)
      if (!strcmp(ch[c].daily ? dk : wk, ch[c].daily ? nowDk : nowWk)) sum[c] += tplProgress(ch[c].tpl, m, team);
  }
  for (int c = 0; c < nch; c++) {
    done[c] = sum[c] >= ch[c].target;
    value[c] = done[c] ? ch[c].target : sum[c];
  }
}

// ---------------------------------------------------------------- resultado
const int MAX_P = 160;  // jugadores distintos en el historial

struct Player {
  uint32_t code;
  int elo, maxElo, rankedPlayed;
  long xp;
  int level;
  uint8_t category;
  uint8_t ach[(ACH_COUNT + 7) / 8];  // logros conseguidos
  uint32_t achAt[ACH_COUNT];          // cuándo (segundos UTC del partido; 0 sin hora): el perfil del QR
  int achCount;
  int tournamentsWon;
  int challengesDone;
  // contadores
  int played, wins, losses, draws, winStreak, lossStreak, rankedWins, chaosWins, personalGoals;
  int teammates, rivals;
  char day[12];
  int dayCount;
  struct Slot {
    uint32_t keyHash;
    int value;
    bool done;
  } slot[8];
  uint8_t mates[MAX_P / 8], foes[MAX_P / 8];
  // forma: últimos resultados (bit 1 = victoria), del más nuevo al más viejo
  uint32_t formBits;
  uint8_t formLen;
  char rform[6];   // forma en clasificatorios: los 5 últimos («G», «P», «E»), del más viejo al más nuevo
  int bestStreak;  // mejor racha de victorias (computePlayerStats: cuenta también los partidos de torneos en juego)
  int streakNow;   // racha de victorias para bestStreak (winStreak es la de los logros)
  int achPlayed, achWins;  // partidos y victorias para los logros (sin los de torneos en juego, como la app)
};

// Lo que pasó en cada partido a cada jugador (para el detalle).
struct Entry {
  int16_t eloBefore, eloAfter, eloDelta;  // eloDelta = 0x7FFF si no movió ELO
  uint16_t xpGained;
  uint8_t levelBefore, levelAfter;
  bool deferred;  // partido de un torneo en juego: la XP y los logros esperan a que termine
};

// ---------------------------------------------------------------- torneos (los pone s3_torneos_datos.h)
// Ids como FNV-1a de 64 bits (idHash): 24 bytes por torneo (con cientos de torneos, poca memoria).
static uint64_t idHash(const char* s) {
  uint64_t h = 1469598103934665603ull;
  for (; *s; s++) h = (h ^ (uint8_t)*s) * 1099511628211ull;
  return h;
}
struct TournamentInfo {
  uint64_t id;            // idHash del torneo
  uint64_t finalMatch;    // terminado: idHash del partido que lo cerró («Ganar torneo»); 0 si no
  bool active;            // en juego: sus partidos solo mueven el ELO
  uint8_t nChampion;
  uint32_t champion[2];   // códigos de los campeones
};
static TournamentInfo* tournaments = nullptr;  // PSRAM
static int nTournaments = 0, capTournaments = 0;
static uint32_t tournamentsRevision = 0;
static bool setTournaments(const TournamentInfo* list, int n) {
  if (n > 0 && !s3mem::reserve(tournaments, capTournaments, n, 16)) return false;
  if (n > 0) memcpy(tournaments, list, (size_t)n * sizeof(TournamentInfo));
  nTournaments = n > 0 ? n : 0;
  ++tournamentsRevision;
  return true;
}
static bool tournamentActive(const char* id) {
  if (!id[0] || !nTournaments) return false;
  uint64_t h = idHash(id);
  for (int i = 0; i < nTournaments; i++)
    if (tournaments[i].active && tournaments[i].id == h) return true;
  return false;
}
// ¿Este partido cierra un torneo terminado y `code` es de los campeones?
static bool wonTournament(const char* matchId, uint32_t code) {
  if (!nTournaments) return false;
  uint64_t h = idHash(matchId);
  for (int i = 0; i < nTournaments; i++) {
    const TournamentInfo& t = tournaments[i];
    if (t.active || !t.finalMatch || t.finalMatch != h) continue;
    for (int k = 0; k < t.nChampion; k++)
      if (t.champion[k] == code) return true;
  }
  return false;
}

struct Snapshot {
  Player* p = nullptr;
  int n = 0;
  Entry* entries = nullptr;  // [fila][equipo*4+plaza]
  int rowsCap = 0;
  uint16_t* pairWins = nullptr;  // victorias con cada compañero [MAX_P × MAX_P]

  int find(uint32_t code) const {
    for (int i = 0; i < n; i++)
      if (p[i].code == code) return i;
    return -1;
  }
  void clear() {
    free(p);
    free(entries);
    free(pairWins);
    *this = Snapshot();
  }
};

static uint32_t fnvStr(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
  return h;
}

static bool hasBit(const uint8_t* b, int i) { return b[i >> 3] & (1 << (i & 7)); }
static void setBit(uint8_t* b, int i) { b[i >> 3] |= (uint8_t)(1 << (i & 7)); }

// Orden de la app: por fin y, si coincide, por id.
static int* order = nullptr;
static int orderCapacity = 0;
static int byFinish(const void* a, const void* b) {
  const partidos::Row &x = partidos::rows[*(const int*)a], &y = partidos::rows[*(const int*)b];
  if (x.fin != y.fin) return x.fin < y.fin ? -1 : 1;
  return strcmp(x.id, y.id);
}

// Recalcula todo. `keep(row)` elige los partidos (todos, o los clasificatorios de una temporada); `codes` son los
// jugadores que salen aunque no hayan jugado (los de la lista). `challenges`: con retos y torneos (no en las
// temporadas: la app las calcula sin los dos).
typedef bool (*Keep)(const partidos::Row&, const void* arg);
static bool compute(Snapshot& s, const uint32_t* codes, int nCodes, Keep keep, const void* arg, bool withChallenges) {
  s.clear();
  s.p = (Player*)calloc(MAX_P, sizeof(Player));
  s.rowsCap = partidos::count;
  s.entries = (Entry*)calloc((size_t)(s.rowsCap ? s.rowsCap : 1) * 8, sizeof(Entry));
  s.pairWins = (uint16_t*)calloc((size_t)MAX_P * MAX_P, sizeof(uint16_t));
  if (!s.p || !s.entries || !s.pairWins || !s3mem::reserve(order, orderCapacity, partidos::count ? partidos::count : 1)) {
    s.clear();
    return false;
  }
  auto ensure = [&](uint32_t code) -> int {
    int i = s.find(code);
    if (i >= 0) return i;
    if (s.n >= MAX_P) return -1;
    Player& p = s.p[s.n];
    memset(&p, 0, sizeof(p));
    p.code = code;
    p.elo = p.maxElo = rules.eloInitial;
    p.category = categoryFor(rules.eloInitial);
    return s.n++;
  };
  for (int i = 0; i < nCodes; i++) ensure(codes[i]);
  int nm = 0;
  for (int i = 0; i < partidos::count; i++)
    if (!keep || keep(partidos::rows[i], arg)) order[nm++] = i;
  qsort(order, nm, sizeof(int), byFinish);
  bool useCh = withChallenges && rules.challenges;
  bool useTournaments = withChallenges;

  for (int oi = 0; oi < nm; oi++) {
    int ri = order[oi];
    const partidos::Row& m = partidos::rows[ri];
    Entry* ent = &s.entries[(size_t)ri * 8];
    for (int k = 0; k < 8; k++) ent[k].eloDelta = 0x7FFF;
    bool ranked = m.mode == 'r';
    int winner = m.winner == 'w' ? 0 : m.winner == 'b' ? 1 : -1;
    int idx[2][4];
    for (int t = 0; t < 2; t++)
      for (int i = 0; i < m.n[t]; i++) idx[t][i] = ensure(m.code[t][i]);
    // ELO: media de cada equipo antes del partido.
    double avg[2] = {0, 0};
    for (int t = 0; t < 2; t++) {
      double sum = 0;
      int k = 0;
      for (int i = 0; i < m.n[t]; i++)
        if (idx[t][i] >= 0) sum += s.p[idx[t][i]].elo, k++;
      avg[t] = k ? sum / k : NAN;
    }
    double mult = rules.goalDiffMultiplier ? goalDiffMult((int)m.w - (int)m.b) : 1;
    int change[2][4];
    bool hasChange = ranked && m.n[0] && m.n[1];
    if (hasChange)
      for (int t = 0; t < 2; t++)
        for (int i = 0; i < m.n[t]; i++) {
          if (idx[t][i] < 0) continue;
          Player& p = s.p[idx[t][i]];
          double expected = 1.0 / (1.0 + pow(10.0, (avg[1 - t] - avg[t]) / 400.0));
          double sc = winner < 0 ? 0.5 : (t == winner ? 1 : 0);
          int k = p.rankedPlayed < rules.provisionalMatches ? rules.kProvisional : rules.kEstablished;
          change[t][i] = jsRound(k * mult * (sc - expected));
        }
    // Torneo en juego: solo el ELO (y lo que enseñan las estadísticas, que cuentan todos los partidos).
    if (useTournaments && tournamentActive(m.tournament)) {
      for (int t = 0; t < 2; t++)
        for (int i = 0; i < m.n[t]; i++) {
          int pi = idx[t][i];
          if (pi < 0) continue;
          Player& p = s.p[pi];
          Entry& e = ent[t * 4 + i];
          bool won = t == winner;
          p.played++;
          if (won) p.wins++;
          else if (winner < 0) p.draws++;
          else p.losses++;
          p.streakNow = won ? p.streakNow + 1 : 0;
          if (p.streakNow > p.bestStreak) p.bestStreak = p.streakNow;
          p.formBits = (p.formBits << 1) | (won ? 1u : 0u);
          if (p.formLen < 32) p.formLen++;
          if (m.mode == 'r') {
            size_t fl = strlen(p.rform);
            if (fl == 5) memmove(p.rform, p.rform + 1, 5), fl = 4;
            p.rform[fl] = won ? 'G' : winner >= 0 ? 'P' : 'E';
            p.rform[fl + 1] = 0;
          }
          e.eloBefore = (int16_t)p.elo;
          if (hasChange) {
            p.elo += change[t][i];
            if (p.elo > p.maxElo) p.maxElo = p.elo;
            p.rankedPlayed++;
            p.category = categoryFor(p.elo);
            e.eloDelta = (int16_t)change[t][i];
          }
          e.eloAfter = (int16_t)p.elo;
          e.xpGained = 0;
          e.levelBefore = e.levelAfter = (uint8_t)p.level;
          e.deferred = true;
        }
      continue;
    }
    Challenge ch[4];
    int nch = useCh && m.fin ? activeChallenges(m.fin, ch) : 0;
    char dk[12] = "";
    if (m.fin) dayKey(m.fin, dk);
    reloj::Clock lc = partidos::localOf(m.fin);
    int totalGoals = m.w + m.b;
    for (int t = 0; t < 2; t++)
      for (int i = 0; i < m.n[t]; i++) {
        int pi = idx[t][i];
        if (pi < 0) continue;
        Player& p = s.p[pi];
        Entry& e = ent[t * 4 + i];
        bool won = t == winner;
        int levelBefore = p.level;
        int lossStreakBefore = p.lossStreak;
        p.played++;
        p.achPlayed++;
        if (won) p.achWins++;
        p.streakNow = won ? p.streakNow + 1 : 0;
        if (p.streakNow > p.bestStreak) p.bestStreak = p.streakNow;
        if (won) {
          p.wins++;
          p.winStreak++;
          p.lossStreak = 0;
          if (m.mode == 'r') p.rankedWins++;
          if (m.mode == 'c') p.chaosWins++;
        } else if (winner < 0) {
          p.draws++;
          p.winStreak = 0;
          p.lossStreak = 0;
        } else {
          p.losses++;
          p.winStreak = 0;
          p.lossStreak++;
        }
        p.formBits = (p.formBits << 1) | (won ? 1u : 0u);
        if (m.mode == 'r') {
          size_t fl = strlen(p.rform);
          if (fl == 5) memmove(p.rform, p.rform + 1, 5), fl = 4;
          p.rform[fl] = won ? 'G' : winner >= 0 ? 'P' : 'E';
          p.rform[fl + 1] = 0;
        }
        if (p.formLen < 32) p.formLen++;
        if (strcmp(p.day, dk)) {
          snprintf(p.day, sizeof(p.day), "%s", dk);
          p.dayCount = 0;
        }
        p.dayCount++;
        int winsWithSame = 0;
        for (int t2 = 0; t2 < 2; t2++)
          for (int j = 0; j < m.n[t2]; j++) {
            int oj = idx[t2][j];
            if (oj < 0 || (t2 == t && j == i) || oj == pi) continue;
            if (t2 == t) {
              if (!hasBit(p.mates, oj)) setBit(p.mates, oj), p.teammates++;
              uint16_t& w = s.pairWins[(size_t)pi * MAX_P + oj];
              if (won) w++;
              if (w > winsWithSame) winsWithSame = w;
            } else if (!hasBit(p.foes, oj)) {
              setBit(p.foes, oj);
              p.rivals++;
            }
          }
        int pg = m.pg[t][i];
        p.personalGoals += pg;
        e.eloBefore = (int16_t)p.elo;
        if (hasChange) {
          int before = p.elo;
          p.elo += change[t][i];
          if (p.elo > p.maxElo) p.maxElo = p.elo;
          p.rankedPlayed++;
          p.category = categoryFor(p.elo);
          e.eloDelta = (int16_t)change[t][i];
          e.eloBefore = (int16_t)before;
        }
        e.eloAfter = (int16_t)p.elo;
        long xp = rules.xpComplete + (won ? rules.xpWin : winner < 0 ? rules.xpDraw : rules.xpLoss);
        if (won && ranked) xp += rules.xpRankedWinBonus;
        if (won && m.reason == 'g') xp += rules.xpOvertimeWin;
        if (won && m.reason == 'p') xp += rules.xpPenaltiesWin;
        if (useTournaments && wonTournament(m.id, p.code)) {  // «Ganar torneo»
          p.tournamentsWon++;
          xp += rules.xpTournamentWin;
        }
        // Retos del momento
        for (int c = 0; c < nch; c++) {
          uint32_t kh = fnvStr(ch[c].key);
          Player::Slot* sl = nullptr;
          for (auto& x : p.slot)
            if (x.keyHash == kh) sl = &x;
          if (sl && sl->done) continue;
          int inc = tplProgress(ch[c].tpl, m, t);
          if (!inc) continue;
          if (!sl) {  // hueco: el de un reto que ya no está activo
            for (auto& x : p.slot) {
              bool active = false;
              for (int c2 = 0; c2 < nch; c2++) active |= x.keyHash == fnvStr(ch[c2].key);
              if (!active) {
                sl = &x;
                break;
              }
            }
            if (!sl) continue;
            *sl = Player::Slot{kh, 0, false};
          }
          sl->value += inc;
          if (sl->value >= ch[c].target) {
            sl->done = true;
            p.challengesDone++;
            xp += ch[c].xp;
          }
        }
        // Logros
        int ownKicks = m.kicks[t], ownIn = m.kicksIn[t];
        Ctx cx;
        cx.m = &m;
        cx.won = won;
        cx.goalsFor = t ? m.b : m.w;
        cx.goalsAgainst = t ? m.w : m.b;
        cx.played = p.achPlayed;
        cx.wins = p.achWins;
        cx.winStreak = p.winStreak;
        cx.lossStreakBefore = lossStreakBefore;
        cx.rankedPlayed = p.rankedPlayed;
        cx.rankedWins = p.rankedWins;
        cx.chaosWins = p.chaosWins;
        cx.elo = p.elo;
        cx.eloGapBefore = 0;
        cx.eloGapBeforeD = ranked ? avg[1 - t] - avg[t] : 0;
        cx.level = levelForXp(p.xp + xp);
        cx.comeback = m.comeback;
        cx.earliestOwnGoalMs = m.earliest[t];
        cx.ownGoalRun = m.run[t];
        cx.totalGoals = totalGoals;
        cx.personalGoalsInMatch = pg;
        cx.personalGoalsTotal = p.personalGoals;
        cx.firstGoalOfMatch = m.firstScorer == t * 4 + i;
        cx.penaltiesPerfect = ownKicks >= m.penRounds && ownIn == ownKicks;
        cx.suddenDeathRounds = m.sdKicks / 2;
        cx.hour = lc.hour;
        cx.matchesToday = p.dayCount;
        cx.distinctTeammates = p.teammates;
        cx.distinctRivals = p.rivals;
        cx.winsWithSameTeammate = winsWithSame;
        cx.tournamentsWon = p.tournamentsWon;
        cx.challengesDone = p.challengesDone;
        for (int a = 0; a < ACH_COUNT; a++) {
          if (hasBit(p.ach, a)) continue;
          if (check(a, cx)) {
            setBit(p.ach, a);
            p.achAt[a] = m.fin;
            p.achCount++;
            xp += rules.xpAchievement[ACH[a].rarity];
          }
        }
        p.xp += xp;
        p.level = levelForXp(p.xp);
        e.xpGained = (uint16_t)xp;
        e.levelBefore = (uint8_t)levelBefore;
        e.levelAfter = (uint8_t)p.level;
      }
  }
  return true;
}

// ---------------------------------------------------------------- lo que usa la placa
static Snapshot all;  // historial completo (ELO, nivel, logros…)

static bool keepRankedSeason(const partidos::Row& r, const void* arg) {
  if (r.mode != 'r') return false;
  char k[10];
  seasonKey(r.fin, k);
  return !strcmp(k, (const char*)arg);
}

static Snapshot season;  // clasificación de la temporada (solo sus clasificatorios, todos desde 1200)
static char seasonNow[10] = "";

// «octubre 2026», «4º trimestre 2026» (seasonLabel de la app).
static void seasonLabel(const char* key, char* out, size_t n) {
  static const char* MONTHS[12] = {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre",
                                   "noviembre", "diciembre"};
  int y = atoi(key);
  if (key[5] == 'T') snprintf(out, n, "%cº trimestre %d", key[6], y);
  else snprintf(out, n, "%s %d", MONTHS[(atoi(key + 5) + 11) % 12], y);
}

// Clave exacta: partidos, lista de jugadores y reglas. Se publica solo tras un cálculo correcto.
struct RefreshKey {
  bool valid = false;
  uint32_t revision = 0;
  int count = 0, n = 0;
  uint32_t tRevision = 0;
  uint32_t codes[MAX_P] = {};
  Rules rulesCopy;
  bool matches(const uint32_t* list, int size) const {
    return valid && size >= 0 && size <= MAX_P && n == size && count == partidos::count &&
           revision == partidos::revision && tRevision == tournamentsRevision && !memcmp(&rulesCopy, &rules, sizeof(rules)) &&
           (!size || !memcmp(codes, list, (size_t)size * sizeof(uint32_t)));
  }
  void remember(const uint32_t* list, int size) {
    valid = size >= 0 && size <= MAX_P;
    if (!valid) return;
    revision = partidos::revision;
    tRevision = tournamentsRevision;
    count = partidos::count;
    n = size;
    if (size) memcpy(codes, list, (size_t)size * sizeof(uint32_t));
    memcpy(&rulesCopy, &rules, sizeof(rules));
  }
};
static RefreshKey allKey, seasonCacheKey;

static void refreshSeason(const uint32_t* codes, int n, uint32_t nowUtc) {
  char key[10];
  seasonKey(nowUtc, key);
  if (seasonCacheKey.matches(codes, n) && !strcmp(key, seasonNow)) return;
  seasonCacheKey.valid = false;
  snprintf(seasonNow, sizeof(seasonNow), "%s", key);
  if (compute(season, codes, n, keepRankedSeason, seasonNow, false)) seasonCacheKey.remember(codes, n);
}

static void refresh(const uint32_t* codes, int n) {
  if (allKey.matches(codes, n)) return;
  allKey.valid = false;
  if (compute(all, codes, n, nullptr, nullptr, true)) allKey.remember(codes, n);
}

static const Player* of(uint32_t code) {
  int i = all.find(code);
  return i >= 0 ? &all.p[i] : nullptr;
}

// ---------------------------------------------------------------- títulos (availableTitles / displayTitle de la app)
// Los logros con título que tiene, en el orden en que los consiguió (a la misma hora, en el del catálogo).
static int availableTitles(const Player* pr, int out[ACH_COUNT]) {
  int n = 0;
  if (!pr) return 0;
  for (int a = 0; a < ACH_COUNT; a++)
    if (ACH[a].title && hasBit(pr->ach, a)) {
      int k = n++;
      while (k > 0 && pr->achAt[out[k - 1]] > pr->achAt[a]) out[k] = out[k - 1], k--;
      out[k] = a;
    }
  return n;
}

// El título que se ve: el elegido si lo tiene; «none», ninguno; si no, el de mayor rareza y, entre ellos, el último.
static const char* displayTitle(const Player* pr, const char* chosenId) {
  int t[ACH_COUNT];
  int n = availableTitles(pr, t);
  if (!n || (chosenId && !strcmp(chosenId, "none"))) return nullptr;
  if (chosenId && *chosenId)
    for (int i = 0; i < n; i++)
      if (!strcmp(ACH[t[i]].id, chosenId)) return ACH[t[i]].title;
  int best = n - 1;
  for (int i = n - 2; i >= 0; i--)
    if (ACH[t[i]].rarity > ACH[t[best]].rarity) best = i;
  return ACH[t[best]].title;
}

}  // namespace progresion

#endif  // MFV3_S3_PROGRESION_H
