// MARCADOR FUTBOLÍN V3 · torneos en la pantalla de 7" (como src/services/tournaments de la app).
// -------------------------------------------------------------------------------------------------
// CONTRATO común del equipo (docs/PLAN_TORNEOS_PLACA.md): estructuras y funciones que usan las pantallas
// (ui_torneo*.h) y que implementan las reglas (s3_torneos_reglas.h), la ficha y el palmarés
// (s3_torneos_ficha.h) y el guardado y la nube (s3_torneos_datos.h). No cambiar una firma sin avisar al
// coordinador: las tres partes se escriben a la vez.
//
// - Liga: todos contra todos a una vuelta o ida y vuelta (3–8 equipos), puntos configurables, desempate puntos →
//   diferencia → goles a favor → enfrentamiento directo → nombre. Final opcional 1º contra 2º.
// - Copa: eliminatoria 3–8 equipos, huecos con pase directo, siembra por ELO (1 contra el último) o sorteo.
// - Pool: escalera de pistas de 2–8 parejas (ladder.ts). Sorteos con seededRandom (misma semilla y mismo
//   resultado que la app: las rondas salen iguales en los dos lados).
// Los JSON que se leen y escriben son los mismos `Tournament` y `TournamentTemplate` de la app
// (src/services/persistence/types.ts): un torneo creado aquí se abre en la app y al revés.
//
// Memoria: un torneo abierto vive en la PSRAM (`teams` y `fix`, reservados con calloc). De los demás solo se
// guarda el resumen (`Summary`), que basta para listas, «en juego» y palmarés.

#ifndef MFV3_S3_TORNEOS_H
#define MFV3_S3_TORNEOS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace torneos {

// ---------------------------------------------------------------- límites (los de la app)
static const uint8_t MIN_TEAMS = 3, MAX_TEAMS = 8;       // liga y copa
static const uint8_t POOL_MIN_PAIRS = 2, POOL_MAX_PAIRS = 8;
static const uint8_t MAX_ENTRANTS = 16;                  // jugadores de un torneo
static const uint16_t MAX_TEAMS_T = 160;                 // equipos de un torneo (el Pool crea parejas al vuelo)
static const uint16_t MAX_FIXTURES = 320;                // cruces de un torneo
static const uint8_t MAX_SERIES = 3;                     // partidos de una serie (al mejor de 3)
static const uint8_t MAX_TEMPLATES = 24;                 // tipos de torneo guardados
static const uint16_t MAX_SUMMARIES = 200;               // tamaño de las listas de trabajo (no es un tope del historial)
// CONTRATO (agente 2, palmarés casi ilimitado): en la SD está todo el historial (un <id>.json por torneo y una línea
// por torneo en el índice). En memoria (`summaries`) solo los torneos en juego y las RECENT_EDITIONS últimas ediciones
// de cada competición; las demás se leen de la SD bajo demanda (editionsPage) y el cuadro de honor recorre el índice
// entero línea a línea.
static const uint8_t RECENT_EDITIONS = 10;
static const uint8_t POOL_MINUTES[4] = {20, 30, 45, 60};
static const uint8_t POOL_ROUNDS[6] = {3, 4, 5, 6, 8, 10};
static const uint16_t MAX_PLAYED = MAX_FIXTURES + 16;   // CONTRATO (agente 1): partidos apuntados (con las series)

enum class Format : uint8_t { League, Bracket, Pool };       // 'league' | 'bracket' | 'pool'
enum class FinalKind : uint8_t { None, Top2 };               // 'none' | 'top2'
enum class Status : uint8_t { Active, Finished, Cancelled };
enum class End : uint8_t { Goals, Time, Both };              // endCondition
enum class Pairing : uint8_t { Elo, Random, Manual };
enum class Seeding : uint8_t { Elo, Random };

struct Rules {  // MatchRules de la app + allowDraw
  End end = End::Goals;
  uint8_t goals = 5;
  uint8_t minutes = 5;
  bool allowDraw = false;  // solo liga a tiempo (o ambas); la final nunca
};

struct Points {
  uint8_t win = 3, draw = 1, loss = 0;
};

struct Team {  // TournamentTeam
  char id[48];
  char name[48];
  uint8_t n;             // 1 o 2 jugadores
  char player[2][40];    // ids de la app (s3_jugadores Player::id)
  char logo[40];         // vacío si no tiene
  char clubId[48];
};

struct Fixture {
  char id[64];
  uint16_t round;
  int16_t white = -1, blue = -1;   // índice en Tournament::teams; -1 = por decidir
  int16_t winner = -1;
  bool draw = false, bye = false, isFinal = false, skipped = false;
  int16_t next = -1;               // índice del cruce siguiente (copa); -1 ninguno
  uint8_t nextSlot = 0;            // 0 blanco, 1 azul
  uint8_t court = 0;               // Pool: pista (1 = la mejor); 0 sin pista
  uint8_t bestOf = 1;
  uint8_t serW = 0, serB = 0;      // victorias en la serie
  bool hasSeries = false;
  uint8_t nMatches = 0;            // partidos apuntados (series); el último es `matchId`
  char matchId[48];
  char matchIds[MAX_SERIES][48];
  uint64_t abandonedAt = 0;        // ms; 0 = no
  uint8_t nResting = 0;
  char resting[2][40];             // Pool: pareja en el banquillo
};

// CONTRATO (agente 1): el resultado se define aquí arriba (antes iba junto a recordFixtureResult) porque el torneo
// guarda la lista de los partidos apuntados (`played`).
// Resultado de un partido ya guardado (s3_partidos Row con su id).
struct Result { char matchId[48]; char winner; /* 'w','b','-' */ uint8_t w, b; uint64_t startedAt, finishedAt; };
// Partido apuntado en un cruce: lo que la app saca de sus StoredMatch para la clasificación (marcador), el reloj del
// Pool (inicio y fin), el premio del campeón (su último partido) y deshacer (se vuelven a apuntar todos en orden).
struct Played { char fixture[64]; Result r; };

struct Ladder {
  bool swap = true;
  bool byRounds = false;  // false = por tiempo
  uint8_t minutes = 30;
  uint8_t rounds = 5;
  uint8_t finalBestOf = 1;
  uint8_t nStart = 0;
  char start[POOL_MAX_PAIRS][2][40];  // parejas de salida en orden de escalera
};

struct Tournament {
  char id[48];
  char name[64];
  Format format = Format::League;
  uint8_t teamSize = 1;
  bool ranked = false;
  Rules rules;                 // config (endCondition, goals, minutes, allowDraw)
  bool hasFinalRules = false;
  Rules finalRules;            // finalConfig
  Status status = Status::Active;
  uint64_t createdAt = 0, finishedAt = 0;  // ms, como la app
  int16_t winner = -1;         // índice del equipo campeón
  char finalMatchId[48];
  bool hasPoints = false;
  Points points;
  char lastTitle[24];
  FinalKind finalKind = FinalKind::None;
  char templateName[28];
  char templateId[48];
  uint16_t edition = 0;
  char competition[28];
  char logo[40], banner[40], cup[40];
  bool hasLadder = false;
  Ladder ladder;
  uint8_t nEntrants = 0;
  char entrants[MAX_ENTRANTS][40];
  uint16_t nTeams = 0;
  Team* teams = nullptr;       // PSRAM, MAX_TEAMS_T
  uint16_t nFix = 0;
  Fixture* fix = nullptr;      // PSRAM, MAX_FIXTURES
  // CONTRATO (agente 1): partidos apuntados, del más antiguo al último (por `finishedAt`), PSRAM, MAX_PLAYED. Al
  // cargar un torneo (s3_torneos_datos.h) se rellena con addPlayed() con los partidos de partidos::rows que sean de
  // este torneo y estén en algún cruce (matchId o matchIds); recordFixtureResult y undoLast lo mantienen solos.
  uint16_t nPlayed = 0;
  Played* played = nullptr;
};

struct Template {  // TournamentTemplate
  char id[48];
  char name[28];
  Format format = Format::League;
  uint8_t teamSize = 1;
  Pairing pairing = Pairing::Elo;
  Seeding seeding = Seeding::Elo;
  bool poolSwap = true, poolByRounds = false;
  uint8_t poolMinutes = 30, poolRounds = 5;
  FinalKind finalKind = FinalKind::None;
  uint8_t finalBestOf = 1;
  bool ranked = false;
  Rules rules;
  bool ownFinalRules = false;
  Rules finalRules;
  bool allowDraw = true;  // liga a tiempo: se puede empatar (sin poner en la app: sí)
  Points points;
  uint8_t legs = 1;
  char logo[40], banner[40], cup[40];
  char lastTitle[24];
};

// Lo que se recuerda de cada torneo sin abrirlo (listas, «en juego», palmarés y perfil).
struct Summary {
  char id[48];
  char name[64];
  char competitionKey[56];   // competitionKey() de report.ts
  char competition[28];      // competitionName()
  Format format;
  Status status;
  uint16_t edition;
  uint64_t createdAt, finishedAt;
  uint16_t done, total;      // fixtureProgress
  uint8_t nChampion;
  char champion[48];         // nombre del equipo campeón
  char championIds[2][40];
  char mvpId[40];            // solo si terminado
  char logo[40], cup[40], banner[40];
  bool ranked;
  // CONTRATO: añadido por el agente 2. Partido que cerró el torneo (vacío si no ha terminado): la progresión da
  // «Ganar torneo» en ese partido sin abrir el torneo.
  char finalMatchId[48];
  // CONTRATO (agente 1): para el palmarés (ediciones jugadas de cada uno). Lo rellena summarize().
  uint8_t nPlayers;
  char players[MAX_ENTRANTS][40];  // jugadores con algún partido jugado
};

// ---------------------------------------------------------------- memoria
bool alloc(Tournament& t);   // reserva teams/fix en PSRAM (idempotente); false sin memoria
void release(Tournament& t);
void clear(Tournament& t);   // vacía sin liberar

// ---------------------------------------------------------------- reglas (s3_torneos_reglas.h · agente 1)
struct Draft {  // TournamentDraft (lo que sale de draftFromTemplate + nombre/edición/escudos)
  char name[64];
  const Template* tpl;
  uint8_t nTeams;
  struct DTeam { uint8_t n; char player[2][40]; char name[48]; char logo[40]; char clubId[48]; } teams[MAX_ENTRANTS];
  uint16_t edition;
  char logo[40], banner[40], cup[40];
  char competition[28];  // CONTRATO (agente 1): nombre propio de la competición (vacío: la del tipo de torneo)
};
typedef int (*EloFn)(const char* playerId);       // ELO actual (s3_progresion)
typedef const char* (*NameFn)(const char* playerId);

struct Fit { bool ok; char reason[48]; char summary[96]; };
Fit templateFit(const Template& t, uint8_t players);
void describeTemplate(const Template& t, char* out, size_t n);
// Forma el borrador como draftFromTemplate (parejas fijas guardadas, equilibradas, al azar con semilla o a mano).
// Devuelve cuántos jugadores sobran (en `leftover`).
uint8_t draftFromTemplate(const Template& t, const char* name, const char (*selected)[40], uint8_t n, EloFn elo,
                          const char* seed, const char (*fixedPairs)[2][40], uint8_t nFixed, Draft& out,
                          char (*leftover)[40]);
// CONTRATO (agente 1): validateDraft suelto (el creador lo enseña antes de crear). false si hay error (en `err`).
bool validateDraft(const Draft& d, char* err, size_t errN);
// Valida (validateDraft): 0 errores → crea. `err` recibe el primer error en español.
bool createTournament(const Draft& d, EloFn elo, NameFn name, uint64_t nowMs, const char* seed, Tournament& out,
                      char* err, size_t errN);
int16_t nextFixture(const Tournament& t);                        // índice o -1
void fixtureProgress(const Tournament& t, uint16_t& done, uint16_t& total);
bool isFinalFixture(const Tournament& t, const Fixture& f);
Rules fixtureRules(const Tournament& t, const Fixture& f);       // fixtureConfig (la final nunca empata)
// Apunta el resultado de un partido ya guardado (s3_partidos Row con su id). false si no cambia nada.
bool recordFixtureResult(Tournament& t, int16_t fixture, const Result& r);
bool undoLast(Tournament& t, char* removedMatchId, size_t n);    // undoMatch del último; devuelve qué partido borrar
void updateRules(Tournament& t, const Rules& r, const Rules* finalR, uint8_t finalBestOf);  // updateTournamentRules
bool finalSeriesEditable(const Tournament& t);
void markAbandoned(Tournament& t, int16_t fixture, uint64_t nowMs);

struct StandingRow { int16_t team; uint8_t played, wins, draws, losses; int16_t points, gf, ga, diff; int8_t move; /* 1 sube, -1 baja, 0 */ };
uint8_t standings(const Tournament& t, StandingRow* out, uint8_t max);  // con flechas de movimiento
struct Place { int16_t team; uint8_t place; bool champion; };
uint8_t finalRanking(const Tournament& t, Place* out, uint8_t max);
void roundLabel(const Tournament& t, uint16_t round, char* out, size_t n);  // «Jornada 3», «Semifinales»…
struct Phase { uint8_t tier; /* 0 final 1 semi 2 cuartos 3 octavos 4 ronda */ char title[40]; char sub[40]; };
Phase phaseInfo(const Tournament& t, const Fixture& f);          // cartel del partido
// Pool
struct Clock { bool started; int32_t remainingMs; int32_t avgMs; };
Clock ladderClock(const Tournament& t, uint64_t nowMs);
void ladderMoves(const Tournament& t, uint16_t round, const char* playerId, int8_t& move);  // ▲ ▼

// CONTRATO (agente 1): añadidos para cargar, para las pantallas y para la prueba cruzada con la app.
bool fixtureDone(const Fixture& f);                                // con ganador o empate
int16_t fixtureIndex(const Tournament& t, const char* fixtureId);  // -1 si no está
// Al cargar: apunta en `played` un partido que ya figura en sus cruces (no cambia los cruces).
void addPlayed(Tournament& t, const char* fixtureId, const Result& r);
void rulesShort(const Rules& r, char* out, size_t n);              // «a 5 goles», «5 min», «a 5 goles o 5 min»
Points pointsOf(const Tournament& t);                              // los del torneo o 3-1-0
bool timeForFinal(const Tournament& t);                            // Pool por tiempo: ya solo da para la final
// Pistas de una ronda del Pool (pista 1 primero) y cómo llegan las parejas a la siguiente (LadderRoundShow).
struct Lineup { uint8_t nCourts; char court[POOL_MAX_PAIRS / 2][2][2][40]; bool bench; char benchIds[2][40]; };
void lineupOfRound(const Tournament& t, uint16_t round, Lineup& out);
void arrivalLineup(const Tournament& t, uint16_t round, Lineup& out);
// Nombre de un jugador (s3_jugadores) para la ficha, el palmarés y los desempates por nombre; sin él, el de su
// equipo de 1 en el torneo o el que guardó el partido.
static NameFn playerName = nullptr;

// ---------------------------------------------------------------- ficha y palmarés (s3_torneos_ficha.h · agente 1)
struct PlayerLine { char id[40]; uint8_t played, wins, draws, losses; int16_t gf, ga, scored; bool champion; float mvp; };
struct Report {
  uint16_t matches, totalGoals; uint32_t playTimeMs; uint64_t startedAt, finishedAt;
  uint8_t nPlayers; PlayerLine players[MAX_ENTRANTS];  // por MVP
  int8_t mvp, topScorer;        // índice en players; -1
  bool scorersReliable;
  char biggestWin[48], highestScoring[48];  // ids de partido
  char finalScore[32];
  // CONTRATO (agente 1): como la ficha de la app, el partido más largo y cuántos se decidieron en prórroga o penaltis.
  char longest[48];
  uint16_t extraTime;
};
void report(const Tournament& t, Report& out);
// CONTRATO (agente 2): títulos, MVP y ediciones con 16 bits (cientos de ediciones).
struct HonourRow { char id[40]; uint16_t titles, mvps, editions; };
struct Competition {
  char key[56]; char name[28]; Format format; char logo[40], cup[40], banner[40];
  // CONTRATO (agente 2): `editions`, las RECENT_EDITIONS más recientes que hay en memoria (índices en summaries, la
  // más reciente primero); `totalEditions`, todas las de la SD (las antiguas, con editionsPage). `honours`, el cuadro
  // de honor entero (todas las ediciones), en PSRAM: es del módulo y vale hasta la siguiente llamada a competitions().
  uint8_t nEditions; uint16_t editions[RECENT_EDITIONS];
  uint16_t totalEditions;
  uint16_t nHonours; HonourRow* honours;
  int16_t lastChampion;  // índice en summaries
};
uint8_t competitions(Competition* out, uint8_t max);     // PSRAM: cada Competition es grande
uint16_t nextEdition(const char* key);
int16_t activeEdition(const char* key);                   // índice en summaries o -1

// CONTRATO (agente 1): de dónde salen los partidos de la ficha (todos los guardados con ese torneo, como
// tournamentReport de la app). Lo pone el guardado (agente 2) con las filas de partidos::rows; sin él, la ficha usa
// `played` (sin goleadores ni prórrogas). `code` = partidos::codeOf del id del jugador; `scored` = Row::pg.
struct MatchLine {
  char id[48];
  char winner;              // 'w', 'b', '-'
  uint8_t w, b;
  char reason;              // 'r' normal, 'g' gol de oro, 'p' penaltis
  uint32_t totalMs;
  uint64_t startedAt, finishedAt;  // ms
  uint8_t n[2];
  uint32_t code[2][2];
  char name[2][2][20];      // nameSnapshot
  uint8_t scored[2][2];     // goles apuntados a cada uno
  uint8_t goalsValid, goalsAssigned;
};
typedef uint16_t (*MatchesFn)(const char* tournamentId, MatchLine* out, uint16_t max);
static MatchesFn matchSource = nullptr;
// Resumen de un torneo (lo llama save() para el índice): clave y nombre de competición, progreso, campeón, MVP…
void summarize(const Tournament& t, Summary& out);
void competitionKey(const Tournament& t, char* out, size_t n);
const char* competitionName(const Tournament& t);
uint32_t playerCode(const char* id);                      // = partidos::codeOf

// CONTRATO (agente 2): todos los resúmenes, de la SD (s3_torneos_datos.h lo pone al empezar) o, sin él, los de
// memoria. `fn` devuelve false para parar. Lo usan competitions() y nextEdition() para contar todas las ediciones.
typedef bool (*SummaryFn)(const Summary& s, void* arg);
static void (*eachSummary)(SummaryFn fn, void* arg) = nullptr;

// ---------------------------------------------------------------- guardado y nube (s3_torneos_datos.h · agente 2)
// /sdcard/torneos/<id>.json (o el chip sin SD) + /sdcard/torneos/indice.txt; plantillas en /jugadores/tipos.txt
// (FFat, como equipos.txt) y en la nube dentro de prefs/mesa campo «tournamentTemplates».
void begin(bool sd);
bool loadIndex();                          // rellena summaries
extern Summary* summaries; extern uint16_t nSummaries;
extern uint32_t revision;                  // cambia con cada cambio (las pantallas se rehacen)
bool load(const char* id, Tournament& t);
bool save(Tournament& t);                  // guarda, actualiza el resumen y lo marca para subir
bool discard(const char* id);              // borra el torneo y sus partidos (como la app)
extern Template* templates; extern uint8_t nTemplates;
bool saveTemplate(const Template& t);
bool removeTemplate(const char* id);
void newTemplate(Template& out, Format f, const Template* copyOf);
// CONTRATO (agente 2): ediciones de una competición en la SD (no descartadas), de la más nueva a la más antigua.
uint16_t editionCount(const char* key);
// Copia en `out` hasta `max` ediciones saltándose las `from` más nuevas (páginas del palmarés). Devuelve cuántas.
uint16_t editionsPage(const char* key, uint16_t from, Summary* out, uint16_t max);

}  // namespace torneos

#endif
