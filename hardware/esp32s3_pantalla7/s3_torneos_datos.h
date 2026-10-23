// MARCADOR FUTBOLÍN V3 · torneos de la pantalla de 7": guardado, índice, tipos de torneo y nube.
// -------------------------------------------------------------------------------------------------
// La parte «guardado y nube» del contrato (s3_torneos.h). Cada torneo se guarda como lo guarda la app (`Tournament`
// de src/services/persistence/types.ts, en JSON): /sdcard/torneos/<id>.json (sin tarjeta, en el chip:
// /ffat/torneos, como los partidos). Así pasa tal cual a la app y a la nube (filas «tournament»). Lo que la placa no
// usa de un torneo de la app se conserva al guardarlo: los demás campos de «config» (prórroga, penaltis…) y las
// claves de primer nivel que no conoce.
//
// /sdcard/torneos/indice.txt: una línea por torneo con su resumen (`Summary`: listas, «en juego», palmarés y
// perfil), para no abrir todos los archivos al encender. Si falta o no cuadra con la carpeta, se rehace.
//
// Tipos de torneo (`TournamentTemplate`): en la carpeta de los jugadores, «/jugadores/tipos.txt» (el mismo JSON que
// el campo «tournamentTemplates» de las preferencias de la app), así van con el modo prueba y con las copias, como
// equipos.txt. En la cuenta, dentro de «prefs»/«mesa»: s3_nube.h sube solo ese campo, unido con el de la cuenta.
//
// Escrituras seguras ante cortes de luz: siempre a un archivo nuevo que luego sustituye al viejo (el viejo queda en
// «.bak» hasta el final; al encender se recupera). Todo lo grande va a la PSRAM con calloc.

#ifndef MFV3_S3_TORNEOS_DATOS_H
#define MFV3_S3_TORNEOS_DATOS_H

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <FS.h>
#include <FFat.h>
#include "s3_torneos.h"
#include "s3_partidos.h"
#include "s3_progresion.h"
#include "s3_nube.h"

// Las reglas, la memoria (alloc/release/clear), la ficha y el resumen (summarize) son del agente 1.
#include "s3_torneos_reglas.h"
#include "s3_torneos_ficha.h"
#define MFV3_TORNEOS_REGLAS 1

namespace torneos {

Summary* summaries = nullptr;
uint16_t nSummaries = 0;
uint32_t revision = 0;
Template* templates = nullptr;
uint8_t nTemplates = 0;


namespace guardado {

const char* const SD_DIR = "/sdcard/torneos";
const char* const CHIP_DIR = "/ffat/torneos";
static char dir[64] = "";
// Tipos de torneo, en la carpeta de los jugadores (FFat).
static fs::FS* chip = &FFat;
static const char* const TPL_FILE = "/jugadores/tipos.txt";
static const char* const TPL_TMP = "/jugadores/tipos.tmp";
static const char* const TPL_PEND = "/jugadores/tipos.pend";  // cambios de aquí sin subir a la cuenta
static const char* const TPL_GONE = "/jugadores/tipos.del";   // ids borrados aquí sin subir, uno por línea
static bool templatesDirty = false;
// Lo que pone la app en «config» y la placa no elige al crear un torneo (DEFAULT_CONFIG y las preferencias).
static uint16_t defaultOvertime = 60;
static uint8_t defaultPenaltyRounds = 5;
static bool defaultPenaltyBlue = false;

#ifndef ARDUINO
static void setDir(const char* d) { snprintf(dir, sizeof(dir), "%s", d); }  // PC: carpeta de pruebas
#endif

// La carpeta, la misma que los partidos: en la tarjeta o, sin ella, en el chip (la tarea de la nube la usa igual).
static void pickDir(bool sd) {
#ifdef ARDUINO
  snprintf(dir, sizeof(dir), "%s", sd ? SD_DIR : CHIP_DIR);
#else
  (void)sd;
  if (!dir[0]) snprintf(dir, sizeof(dir), "../build/placa/torneos-sd");
#endif
}

static void jsonPath(const char* id, char* out, size_t n) { snprintf(out, n, "%s/%s.json", dir, id); }
static void indexPath(char* out, size_t n) { snprintf(out, n, "%s/indice.txt", dir); }
static void cloudPath(const char* id, char* out, size_t n) { snprintf(out, n, "%s/%s.nube", dir, id); }

static void mkdirs(const char* p) {
#ifdef _WIN32
  mkdir(p);
#else
  mkdir(p, 0777);
#endif
}

// Copia un texto sin partir una letra de varios bytes (UTF-8) al recortarlo.
static void copyText(char* out, size_t n, const char* s) {
  size_t k = 0;
  if (s)
    while (s[k] && k + 1 < n) k++;
  while (k && s[k] && ((uint8_t)s[k] & 0xC0) == 0x80) k--;  // no cortar en medio de una letra
  if (k) memcpy(out, s, k);
  out[k] = 0;
}
static void strField(const char* obj, const char* key, char* out, size_t n) {
  static char tmp[160];  // solo desde loop()
  partidos::strOf(partidos::keyIn(obj, key), tmp, sizeof(tmp));
  copyText(out, n, tmp);
}
static bool hasKey(const char* obj, const char* key) {
  const char* v = partidos::keyIn(obj, key);
  return v && strncmp(v, "null", 4);
}
// Un array de textos («["a","b"]») en `out` (filas de N letras). Devuelve cuántos.
template <size_t N>
static int strArray(const char* arr, char (*out)[N], int max) {
  int k = 0;
  if (!arr || *arr != '[') return 0;
  const char* end = partidos::closeOf(arr);
  for (const char* p = arr + 1; p < end && *p; p++) {
    if (*p != '"') continue;
    if (k < max) partidos::strOf(p, out[k], N);
    k++;
    p = partidos::skipStr(p) - 1;
  }
  return k;
}
// Final de un valor JSON que empieza en v.
static const char* skipValue(const char* v) {
  if (*v == '"') return partidos::skipStr(v);
  if (*v == '{' || *v == '[') return partidos::closeOf(v) + 1;
  while (*v && *v != ',' && *v != '}' && *v != ']') v++;
  return v;
}
// Escribe ",\"clave\":valor" de cada clave del objeto `obj` que no esté en `known` (tal cual venía).
static bool writeUnknown(FILE* f, const char* obj, const char* const* known, int nKnown) {
  if (!obj || *obj != '{') return true;
  const char* end = partidos::closeOf(obj);
  for (const char* p = obj + 1; p < end && *p;) {
    while (p < end && *p != '"') p++;
    if (p >= end) break;
    const char* ke = partidos::skipStr(p);
    size_t kl = (size_t)(ke - p - 2);
    const char* v = ke;
    while (*v == ' ' || *v == ':' || *v == '\n' || *v == '\t' || *v == '\r') v++;
    const char* ve = skipValue(v);
    bool isKnown = false;
    for (int i = 0; i < nKnown && !isKnown; i++) isKnown = strlen(known[i]) == kl && !memcmp(known[i], p + 1, kl);
    if (!isKnown && fprintf(f, ",%.*s:%.*s", (int)(ke - p), p, (int)(ve - v), v) < 0) return false;
    p = ve;
    while (p < end && (*p == ',' || *p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
  }
  return true;
}

static const char* formatName(Format f) { return f == Format::Bracket ? "bracket" : f == Format::Pool ? "pool" : "league"; }
static Format formatOf(const char* s) { return !strcmp(s, "bracket") ? Format::Bracket : !strcmp(s, "pool") ? Format::Pool : Format::League; }
static const char* endName(End e) { return e == End::Time ? "time" : e == End::Both ? "both" : "goals"; }
static End endOf(const char* s) { return !strcmp(s, "time") ? End::Time : !strcmp(s, "both") ? End::Both : End::Goals; }
static const char* statusName(Status s) { return s == Status::Finished ? "finished" : s == Status::Cancelled ? "cancelled" : "active"; }
static Status statusOf(const char* s) { return !strcmp(s, "finished") ? Status::Finished : !strcmp(s, "cancelled") ? Status::Cancelled : Status::Active; }

static void rulesOf(const char* cfg, Rules& r) {
  char s[16];
  partidos::strOf(partidos::keyIn(cfg, "endCondition"), s, sizeof(s));
  r.end = endOf(s);
  r.goals = (uint8_t)partidos::numOf(partidos::keyIn(cfg, "goalsPerPeriod"), 5);
  r.minutes = (uint8_t)partidos::numOf(partidos::keyIn(cfg, "minutesPerPeriod"), 5);
  r.allowDraw = partidos::isTrue(partidos::keyIn(cfg, "allowDraw"));
}

// «config» (MatchConfig) completo: lo de la placa y, de lo demás, lo que ya tuviera (`old`) o lo de por defecto.
static bool writeConfig(FILE* f, const Rules& r, bool ranked, const char* old) {
  static const char* const MINE[] = {"mode", "endCondition", "goalsPerPeriod", "minutesPerPeriod", "testMode", "allowDraw"};
  bool ok = fprintf(f, "{\"mode\":\"%s\",\"endCondition\":\"%s\",\"goalsPerPeriod\":%u,\"minutesPerPeriod\":%u", ranked ? "ranked" : "quick",
                    endName(r.end), r.goals, r.minutes) >= 0;
  if (old && *old == '{') ok = writeUnknown(f, old, MINE, 6) && ok;
  else
    ok = fprintf(f, ",\"overtimeSeconds\":%u,\"penaltyRounds\":%u,\"penaltyFirstTeam\":\"%s\"", defaultOvertime, defaultPenaltyRounds,
                 defaultPenaltyBlue ? "blue" : "white") >= 0 && ok;
  ok = fprintf(f, ",\"testMode\":false%s}", r.allowDraw ? ",\"allowDraw\":true" : "") >= 0 && ok;
  return ok;
}

// ---------------------------------------------------------------- Tournament ⇄ JSON
static int teamIndex(const Tournament& t, const char* id) {
  if (!id || !id[0]) return -1;
  for (uint16_t i = 0; i < t.nTeams; i++)
    if (!strcmp(t.teams[i].id, id)) return i;
  return -1;
}

// La lista de partidos apuntados (Tournament::played) sale del historial: los partidos de este torneo que están en
// algún cruce (matchId o matchIds), del más antiguo al último.
static void addPlayedFromRows(Tournament& t) {
  t.nPlayed = 0;
  if (!t.played) return;
  int* idx = partidos::count ? (int*)malloc(sizeof(int) * (size_t)partidos::count) : nullptr;
  int n = 0;
  for (int i = 0; idx && i < partidos::count; i++)
    if (!strcmp(partidos::rows[i].tournament, t.id)) idx[n++] = i;
  for (int a = 1; a < n; a++)  // por fin (y por id si coinciden)
    for (int b = a; b > 0; b--) {
      const partidos::Row &x = partidos::rows[idx[b]], &y = partidos::rows[idx[b - 1]];
      if (x.finishedAt > y.finishedAt || (x.finishedAt == y.finishedAt && strcmp(x.id, y.id) >= 0)) break;
      int k = idx[b]; idx[b] = idx[b - 1]; idx[b - 1] = k;
    }
  for (int k = 0; k < n; k++) {
    const partidos::Row& r = partidos::rows[idx[k]];
    int16_t fi = fixtureIndex(t, r.fixture);
    if (fi < 0) continue;
    const Fixture& f = t.fix[fi];
    bool inFixture = !strcmp(f.matchId, r.id);
    for (uint8_t m = 0; m < f.nMatches && !inFixture; m++) inFixture = !strcmp(f.matchIds[m], r.id);
    if (!inFixture) continue;
    Result res;
    memset(&res, 0, sizeof(res));
    copyText(res.matchId, sizeof(res.matchId), r.id);
    res.winner = r.winner;
    res.w = r.w;
    res.b = r.b;
    res.startedAt = r.startedAt;
    res.finishedAt = r.finishedAt;
    addPlayed(t, f.id, res);
  }
  free(idx);
}

// Los partidos de un torneo para la ficha (matchSource de las reglas), desde partidos::rows.
static uint16_t matchesOf(const char* tournamentId, MatchLine* out, uint16_t max) {
  uint16_t n = 0;
  for (int i = 0; i < partidos::count && n < max; i++) {
    const partidos::Row& r = partidos::rows[i];
    if (strcmp(r.tournament, tournamentId)) continue;
    MatchLine& m = out[n++];
    memset(&m, 0, sizeof(m));
    copyText(m.id, sizeof(m.id), r.id);
    m.winner = r.winner;
    m.w = r.w;
    m.b = r.b;
    m.reason = r.reason;
    m.totalMs = r.totalMs;
    m.startedAt = r.startedAt;
    m.finishedAt = r.finishedAt;
    for (int t = 0; t < 2; t++) {
      m.n[t] = r.n[t] > 2 ? 2 : r.n[t];
      for (int k = 0; k < m.n[t]; k++) {
        m.code[t][k] = r.code[t][k];
        memcpy(m.name[t][k], r.name[t][k], sizeof(m.name[t][k]));
        m.scored[t][k] = r.pg[t][k];
      }
    }
    m.goalsValid = r.goalsValid;
    m.goalsAssigned = r.goalsAssigned;
  }
  return n;
}

// Lee un torneo de la app (o de la placa). false si no es un torneo o no cabe en la placa.
static bool fromJson(const char* txt, Tournament& t) {
  while (*txt == ' ' || *txt == '\n' || *txt == '\r' || *txt == '\t') txt++;
  if (*txt != '{' || !alloc(t)) return false;
  clear(t);
  strField(txt, "id", t.id, sizeof(t.id));
  if (!t.id[0]) return false;
  strField(txt, "name", t.name, sizeof(t.name));
  char s[24];
  partidos::strOf(partidos::keyIn(txt, "format"), s, sizeof(s));
  t.format = formatOf(s);
  t.teamSize = (uint8_t)partidos::numOf(partidos::keyIn(txt, "teamSize"), 1);
  t.ranked = partidos::isTrue(partidos::keyIn(txt, "ranked"));
  const char* cfg = partidos::keyIn(txt, "config");
  rulesOf(cfg, t.rules);
  const char* fcfg = partidos::keyIn(txt, "finalConfig");
  t.hasFinalRules = fcfg && *fcfg == '{';
  if (t.hasFinalRules) rulesOf(fcfg, t.finalRules);
  partidos::strOf(partidos::keyIn(txt, "status"), s, sizeof(s));
  t.status = statusOf(s);
  t.createdAt = (uint64_t)partidos::numOf(partidos::keyIn(txt, "createdAt"));
  t.finishedAt = (uint64_t)partidos::numOf(partidos::keyIn(txt, "finishedAt"));
  strField(txt, "finalMatchId", t.finalMatchId, sizeof(t.finalMatchId));
  const char* pts = partidos::keyIn(txt, "points");
  t.hasPoints = pts && *pts == '{';
  if (t.hasPoints) {
    t.points.win = (uint8_t)partidos::numOf(partidos::keyIn(pts, "win"), 3);
    t.points.draw = (uint8_t)partidos::numOf(partidos::keyIn(pts, "draw"), 1);
    t.points.loss = (uint8_t)partidos::numOf(partidos::keyIn(pts, "loss"), 0);
  }
  strField(txt, "lastTitle", t.lastTitle, sizeof(t.lastTitle));
  partidos::strOf(partidos::keyIn(txt, "final"), s, sizeof(s));
  t.finalKind = !strcmp(s, "top2") ? FinalKind::Top2 : FinalKind::None;
  strField(txt, "templateName", t.templateName, sizeof(t.templateName));
  strField(txt, "templateId", t.templateId, sizeof(t.templateId));
  t.edition = (uint16_t)partidos::numOf(partidos::keyIn(txt, "edition"), 0);
  strField(txt, "competition", t.competition, sizeof(t.competition));
  strField(txt, "logo", t.logo, sizeof(t.logo));
  strField(txt, "banner", t.banner, sizeof(t.banner));
  strField(txt, "cup", t.cup, sizeof(t.cup));
  const char* lad = partidos::keyIn(txt, "ladder");
  t.hasLadder = lad && *lad == '{';
  if (t.hasLadder) {
    const char* sw = partidos::keyIn(lad, "swap");
    t.ladder.swap = !sw || partidos::isTrue(sw);
    partidos::strOf(partidos::keyIn(lad, "limit"), s, sizeof(s));
    t.ladder.byRounds = !strcmp(s, "rounds");
    t.ladder.minutes = (uint8_t)partidos::numOf(partidos::keyIn(lad, "minutes"), 30);
    t.ladder.rounds = (uint8_t)partidos::numOf(partidos::keyIn(lad, "rounds"), 5);
    t.ladder.finalBestOf = (uint8_t)partidos::numOf(partidos::keyIn(lad, "finalBestOf"), 1);
    const char* st = partidos::keyIn(lad, "start");
    t.ladder.nStart = 0;
    for (const char* p = st && *st == '[' ? st + 1 : nullptr; p && *p && *p != ']'; p++) {
      if (*p != '[') continue;
      if (t.ladder.nStart >= POOL_MAX_PAIRS) return false;
      strArray(p, t.ladder.start[t.ladder.nStart++], 2);
      p = partidos::closeOf(p);
    }
  }
  int ne = strArray(partidos::keyIn(txt, "entrants"), t.entrants, MAX_ENTRANTS);
  if (ne > MAX_ENTRANTS) return false;
  t.nEntrants = (uint8_t)ne;
  // Equipos
  const char* teams = partidos::keyIn(txt, "teams");
  for (const char* o = partidos::nextIn(teams, nullptr); o; o = partidos::nextIn(teams, o)) {
    if (t.nTeams >= MAX_TEAMS_T) return false;
    Team& x = t.teams[t.nTeams++];
    x = Team();
    strField(o, "id", x.id, sizeof(x.id));
    strField(o, "name", x.name, sizeof(x.name));
    int n = strArray(partidos::keyIn(o, "playerIds"), x.player, 2);
    x.n = (uint8_t)(n > 2 ? 2 : n);
    strField(o, "logo", x.logo, sizeof(x.logo));
    strField(o, "clubId", x.clubId, sizeof(x.clubId));
  }
  char win[48];
  strField(txt, "winnerTeamId", win, sizeof(win));
  t.winner = (int16_t)teamIndex(t, win);
  // Cruces (el siguiente de cada uno se busca al final: puede venir después)
  const char* fx = partidos::keyIn(txt, "fixtures");
  char (*next)[64] = (char(*)[64])calloc(MAX_FIXTURES, 64);
  if (!next) return false;
  bool ok = true;
  for (const char* o = partidos::nextIn(fx, nullptr); o && ok; o = partidos::nextIn(fx, o)) {
    if (t.nFix >= MAX_FIXTURES) {
      ok = false;
      break;
    }
    uint16_t fi = t.nFix++;
    Fixture& x = t.fix[fi];
    x = Fixture();
    strField(o, "id", x.id, sizeof(x.id));
    x.round = (uint16_t)partidos::numOf(partidos::keyIn(o, "round"));
    char id[64];
    strField(o, "whiteTeamId", id, sizeof(id));
    x.white = (int16_t)teamIndex(t, id);
    strField(o, "blueTeamId", id, sizeof(id));
    x.blue = (int16_t)teamIndex(t, id);
    strField(o, "winnerTeamId", id, sizeof(id));
    x.winner = (int16_t)teamIndex(t, id);
    x.draw = partidos::isTrue(partidos::keyIn(o, "draw"));
    x.bye = partidos::isTrue(partidos::keyIn(o, "bye"));
    x.skipped = partidos::isTrue(partidos::keyIn(o, "skipped"));
    partidos::strOf(partidos::keyIn(o, "stage"), s, sizeof(s));
    x.isFinal = !strcmp(s, "final");
    strField(o, "nextFixtureId", next[fi], 64);
    partidos::strOf(partidos::keyIn(o, "nextSlot"), s, sizeof(s));
    x.nextSlot = !strcmp(s, "blue") ? 1 : 0;
    x.court = (uint8_t)partidos::numOf(partidos::keyIn(o, "court"), 0);
    x.bestOf = (uint8_t)partidos::numOf(partidos::keyIn(o, "bestOf"), 1);
    const char* ser = partidos::keyIn(o, "series");
    x.hasSeries = ser && *ser == '{';
    if (x.hasSeries) {
      x.serW = (uint8_t)partidos::numOf(partidos::keyIn(ser, "white"));
      x.serB = (uint8_t)partidos::numOf(partidos::keyIn(ser, "blue"));
    }
    strField(o, "matchId", x.matchId, sizeof(x.matchId));
    int nm = strArray(partidos::keyIn(o, "matchIds"), x.matchIds, MAX_SERIES);
    x.nMatches = (uint8_t)(nm > MAX_SERIES ? MAX_SERIES : nm);
    // Un partido suelto: también en la lista (el último de la lista es siempre `matchId`).
    if (!x.nMatches && x.matchId[0]) {
      x.nMatches = 1;
      memcpy(x.matchIds[0], x.matchId, sizeof(x.matchId));
    }
    x.abandonedAt = (uint64_t)partidos::numOf(partidos::keyIn(o, "abandonedAt"));
    int nr = strArray(partidos::keyIn(o, "resting"), x.resting, 2);
    x.nResting = (uint8_t)(nr > 2 ? 2 : nr);
  }
  for (uint16_t i = 0; i < t.nFix && ok; i++) {
    t.fix[i].next = -1;
    if (!next[i][0]) continue;
    for (uint16_t k = 0; k < t.nFix; k++)
      if (!strcmp(t.fix[k].id, next[i])) t.fix[i].next = (int16_t)k;
  }
  free(next);
  if (ok) addPlayedFromRows(t);
  return ok;
}

static const char* teamId(const Tournament& t, int16_t i) { return i >= 0 && i < (int16_t)t.nTeams ? t.teams[i].id : nullptr; }

static bool writeStrings(FILE* f, const char (*list)[40], int n) {
  bool ok = fputc('[', f) != EOF;
  for (int i = 0; i < n; i++) {
    if (i) fputc(',', f);
    partidos::jstr(f, list[i]);
  }
  return fputc(']', f) != EOF && ok;
}

// Escribe el torneo como la app (campos opcionales solo si están). `old`: el JSON que había (lo que no se conoce se
// conserva) o nullptr.
static bool toJson(FILE* f, const Tournament& t, const char* old) {
  using partidos::jstr;
  bool ok = fprintf(f, "{\"formatVersion\":1,\"id\":") >= 0;
  jstr(f, t.id);
  fputs(",\"name\":", f);
  jstr(f, t.name);
  fprintf(f, ",\"format\":\"%s\",\"teamSize\":%u,\"ranked\":%s,\"config\":", formatName(t.format), t.teamSize ? t.teamSize : 1,
          t.ranked ? "true" : "false");
  const char* oldCfg = old ? partidos::keyIn(old, "config") : nullptr;
  ok = writeConfig(f, t.rules, t.ranked, oldCfg) && ok;
  fputs(",\"teams\":[", f);
  for (uint16_t i = 0; i < t.nTeams; i++) {
    const Team& x = t.teams[i];
    fprintf(f, "%s{\"id\":", i ? "," : "");
    jstr(f, x.id);
    fputs(",\"name\":", f);
    jstr(f, x.name);
    fputs(",\"playerIds\":", f);
    writeStrings(f, x.player, x.n > 2 ? 2 : x.n);
    if (x.logo[0]) fputs(",\"logo\":", f), jstr(f, x.logo);
    if (x.clubId[0]) fputs(",\"clubId\":", f), jstr(f, x.clubId);
    fputc('}', f);
  }
  fputs("],\"fixtures\":[", f);
  for (uint16_t i = 0; i < t.nFix; i++) {
    const Fixture& x = t.fix[i];
    fprintf(f, "%s{\"id\":", i ? "," : "");
    jstr(f, x.id);
    fprintf(f, ",\"round\":%u", x.round);
    if (x.court) fprintf(f, ",\"court\":%u", x.court);
    const char* w = teamId(t, x.white);
    const char* b = teamId(t, x.blue);
    fputs(",\"whiteTeamId\":", f);
    if (w) jstr(f, w);
    else fputs("null", f);
    fputs(",\"blueTeamId\":", f);
    if (b) jstr(f, b);
    else fputs("null", f);
    if (x.nResting) fputs(",\"resting\":", f), writeStrings(f, x.resting, x.nResting);
    if (x.next >= 0 && x.next < (int16_t)t.nFix) {
      fputs(",\"nextFixtureId\":", f);
      jstr(f, t.fix[x.next].id);
      fprintf(f, ",\"nextSlot\":\"%s\"", x.nextSlot ? "blue" : "white");
    }
    if (x.matchId[0]) fputs(",\"matchId\":", f), jstr(f, x.matchId);
    const char* win = teamId(t, x.winner);
    if (win) fputs(",\"winnerTeamId\":", f), jstr(f, win);
    if (x.draw) fputs(",\"draw\":true", f);
    if (x.bye) fputs(",\"bye\":true", f);
    if (x.isFinal) fputs(",\"stage\":\"final\"", f);
    if (x.skipped) fputs(",\"skipped\":true", f);
    if (x.bestOf > 1) fprintf(f, ",\"bestOf\":%u", x.bestOf);
    // La lista de partidos solo en las series (en un partido suelto la app solo pone `matchId`).
    if (x.nMatches && (x.bestOf > 1 || x.hasSeries)) {
      fputs(",\"matchIds\":[", f);
      for (uint8_t k = 0; k < x.nMatches && k < MAX_SERIES; k++) {
        if (k) fputc(',', f);
        jstr(f, x.matchIds[k]);
      }
      fputc(']', f);
    }
    if (x.hasSeries) fprintf(f, ",\"series\":{\"white\":%u,\"blue\":%u}", x.serW, x.serB);
    if (x.abandonedAt) fprintf(f, ",\"abandonedAt\":%llu", (unsigned long long)x.abandonedAt);
    fputc('}', f);
  }
  fprintf(f, "],\"status\":\"%s\",\"createdAt\":%llu", statusName(t.status), (unsigned long long)t.createdAt);
  if (t.finishedAt) fprintf(f, ",\"finishedAt\":%llu", (unsigned long long)t.finishedAt);
  const char* win = teamId(t, t.winner);
  if (win) fputs(",\"winnerTeamId\":", f), jstr(f, win);
  if (t.finalMatchId[0]) fputs(",\"finalMatchId\":", f), jstr(f, t.finalMatchId);
  if (t.nEntrants) fputs(",\"entrants\":", f), writeStrings(f, t.entrants, t.nEntrants);
  if (t.hasLadder) {
    const Ladder& l = t.ladder;
    fprintf(f, ",\"ladder\":{\"swap\":%s,\"limit\":\"%s\",\"minutes\":%u,\"rounds\":%u,\"finalBestOf\":%u,\"start\":[", l.swap ? "true" : "false",
            l.byRounds ? "rounds" : "time", l.minutes, l.rounds, l.finalBestOf ? l.finalBestOf : 1);
    for (uint8_t i = 0; i < l.nStart && i < POOL_MAX_PAIRS; i++) {
      if (i) fputc(',', f);
      int n = l.start[i][1][0] ? 2 : l.start[i][0][0] ? 1 : 0;
      writeStrings(f, l.start[i], n);
    }
    fputs("]}", f);
  }
  if (t.hasPoints) fprintf(f, ",\"points\":{\"win\":%u,\"draw\":%u,\"loss\":%u}", t.points.win, t.points.draw, t.points.loss);
  if (t.lastTitle[0]) fputs(",\"lastTitle\":", f), jstr(f, t.lastTitle);
  if (t.finalKind == FinalKind::Top2) fputs(",\"final\":\"top2\"", f);
  if (t.hasFinalRules) {
    fputs(",\"finalConfig\":", f);
    const char* oldFinal = old ? partidos::keyIn(old, "finalConfig") : nullptr;
    ok = writeConfig(f, t.finalRules, t.ranked, oldFinal && *oldFinal == '{' ? oldFinal : oldCfg) && ok;
  }
  if (t.templateName[0]) fputs(",\"templateName\":", f), jstr(f, t.templateName);
  if (t.templateId[0]) fputs(",\"templateId\":", f), jstr(f, t.templateId);
  if (t.edition) fprintf(f, ",\"edition\":%u", t.edition);
  if (t.competition[0]) fputs(",\"competition\":", f), jstr(f, t.competition);
  if (t.logo[0]) fputs(",\"logo\":", f), jstr(f, t.logo);
  if (t.banner[0]) fputs(",\"banner\":", f), jstr(f, t.banner);
  if (t.cup[0]) fputs(",\"cup\":", f), jstr(f, t.cup);
  // Lo que traiga la app y la placa no conoce (campos nuevos de versiones futuras), tal cual.
  static const char* const KNOWN[] = {"formatVersion", "id",        "name",        "format",       "teamSize",   "ranked",     "config",
                                      "teams",         "fixtures",  "status",      "createdAt",    "finishedAt", "winnerTeamId", "finalMatchId",
                                      "entrants",      "ladder",    "points",      "lastTitle",    "final",      "finalConfig", "templateName",
                                      "templateId",    "edition",   "competition", "logo",         "banner",     "cup"};
  if (old) ok = writeUnknown(f, old, KNOWN, (int)(sizeof(KNOWN) / sizeof(KNOWN[0]))) && ok;
  return fputs("}\n", f) >= 0 && ok;
}

// ---------------------------------------------------------------- resúmenes e índice
// El índice de la SD es un registro: cada vez que un torneo cambia se AÑADE su línea al final («S2 …»; descartarlo
// añade «X <id>»), así guardar no reescribe todo el historial. Vale la última línea de cada torneo. Al encender se
// compacta (una línea por torneo, sin los descartados, con los que falten y sin los que ya no estén) y una línea a
// medio escribir por un corte de luz se descarta. Las líneas añadidas en esta sesión se recuerdan (`touched`) para
// saber cuál es la buena hasta la próxima compactación.
//
// En memoria (`summaries`, PSRAM, sin tope fijo): los torneos en juego y las RECENT_EDITIONS ediciones más nuevas de
// cada competición. El resto se lee del índice cuando hace falta (editionsPage, cuadro de honor, perfil).

// El resumen del índice: el de las reglas (summarize) y el partido que cerró el torneo.
static void summarizeAll(const Tournament& t, Summary& s) {
  torneos::summarize(t, s);
  memcpy(s.finalMatchId, t.finalMatchId, sizeof(s.finalMatchId));
}

static int capSummaries = 0;
static int findSummary(const char* id) {
  for (uint16_t i = 0; i < nSummaries; i++)
    if (!strcmp(summaries[i].id, id)) return i;
  return -1;
}
static void removeAt(int at) {
  memmove(&summaries[at], &summaries[at + 1], (size_t)(nSummaries - at - 1) * sizeof(Summary));
  nSummaries--;
}
static void dropSummary(const char* id) {
  int at = findSummary(id);
  if (at >= 0) removeAt(at);
}
static bool newer(const Summary& a, const Summary& b) {  // a es más nueva que b
  return a.createdAt != b.createdAt ? a.createdAt > b.createdAt : strcmp(a.id, b.id) > 0;
}
// Deja en memoria, de la competición `key`, las que están en juego y las RECENT_EDITIONS más nuevas.
static void trimKey(const char* key) {
  for (int i = (int)nSummaries - 1; i >= 0; i--) {
    const Summary& s = summaries[i];
    if (strcmp(s.competitionKey, key) || s.status == Status::Active) continue;
    bool out = s.status == Status::Cancelled;
    int rank = 0;
    for (uint16_t k = 0; k < nSummaries && !out; k++) {
      const Summary& o = summaries[k];
      if ((int)k != i && o.status != Status::Cancelled && !strcmp(o.competitionKey, key) && newer(o, s)) rank++;
      out = rank >= RECENT_EDITIONS;
    }
    if (out) removeAt(i);
  }
}
// Pone (o cambia) un resumen en memoria, en orden de creación, y recorta su competición.
static bool keepSummary(const Summary& s) {
  dropSummary(s.id);
  int cap = capSummaries;
  if (!s3mem::reserve(summaries, cap, nSummaries + 1, 32)) return false;
  capSummaries = cap;
  uint16_t k = nSummaries;
  while (k > 0 && newer(summaries[k - 1], s)) k--;
  memmove(&summaries[k + 1], &summaries[k], (size_t)(nSummaries - k) * sizeof(Summary));
  summaries[k] = s;
  nSummaries++;
  trimKey(s.competitionKey);
  return true;
}

// Línea del índice: «S2» y los campos separados por tabuladores (en un texto no puede ir ni tabulador ni salto); los
// jugadores, separados por comas.
static void cleanField(FILE* f, const char* s) {
  for (; *s; s++) fputc(*s == '\t' || *s == '\n' || *s == '\r' ? ' ' : *s, f);
}
static bool writeLine(FILE* f, const Summary& s) {
  const char* text[] = {s.id, s.name, s.competitionKey, s.competition};
  fputs("S2", f);
  for (const char* x : text) fputc('\t', f), cleanField(f, x);
  fprintf(f, "\t%c\t%c\t%u\t%llu\t%llu\t%u\t%u\t%d\t%u", "lbp"[(int)s.format], "afc"[(int)s.status], s.edition,
          (unsigned long long)s.createdAt, (unsigned long long)s.finishedAt, s.done, s.total, s.ranked ? 1 : 0, s.nChampion);
  const char* more[] = {s.champion, s.championIds[0], s.championIds[1], s.mvpId, s.logo, s.cup, s.banner, s.finalMatchId};
  for (const char* x : more) fputc('\t', f), cleanField(f, x);
  fputc('\t', f);
  for (uint8_t i = 0; i < s.nPlayers && i < MAX_ENTRANTS; i++) {
    if (i) fputc(',', f);
    cleanField(f, s.players[i]);
  }
  return fputc('\n', f) != EOF;
}
static bool parseLine(char* l, Summary& s) {
  memset(&s, 0, sizeof(s));
  char* f[24];
  int k = 0;
  f[k++] = l;
  for (char* p = l; *p && k < 24; p++)
    if (*p == '\t') *p = 0, f[k++] = p + 1;
  if (k < 23 || strcmp(f[0], "S2")) return false;
  copyText(s.id, sizeof(s.id), f[1]);
  copyText(s.name, sizeof(s.name), f[2]);
  copyText(s.competitionKey, sizeof(s.competitionKey), f[3]);
  copyText(s.competition, sizeof(s.competition), f[4]);
  s.format = f[5][0] == 'b' ? Format::Bracket : f[5][0] == 'p' ? Format::Pool : Format::League;
  s.status = f[6][0] == 'f' ? Status::Finished : f[6][0] == 'c' ? Status::Cancelled : Status::Active;
  s.edition = (uint16_t)atoi(f[7]);
  s.createdAt = strtoull(f[8], nullptr, 10);
  s.finishedAt = strtoull(f[9], nullptr, 10);
  s.done = (uint16_t)atoi(f[10]);
  s.total = (uint16_t)atoi(f[11]);
  s.ranked = f[12][0] == '1';
  s.nChampion = (uint8_t)atoi(f[13]);
  if (s.nChampion > 2) s.nChampion = 2;
  copyText(s.champion, sizeof(s.champion), f[14]);
  copyText(s.championIds[0], sizeof(s.championIds[0]), f[15]);
  copyText(s.championIds[1], sizeof(s.championIds[1]), f[16]);
  copyText(s.mvpId, sizeof(s.mvpId), f[17]);
  copyText(s.logo, sizeof(s.logo), f[18]);
  copyText(s.cup, sizeof(s.cup), f[19]);
  copyText(s.banner, sizeof(s.banner), f[20]);
  copyText(s.finalMatchId, sizeof(s.finalMatchId), f[21]);
  for (char* p = f[22]; *p && s.nPlayers < MAX_ENTRANTS;) {
    char* c = strchr(p, ',');
    if (c) *c = 0;
    if (*p) copyText(s.players[s.nPlayers++], 40, p);
    if (!c) break;
    p = c + 1;
  }
  return s.id[0] != 0;
}
// «X <id>»: descartado.
static bool goneLine(const char* l, char* id, size_t n) {
  if (l[0] != 'X' || l[1] != '\t') return false;
  copyText(id, n, l + 2);
  return id[0] != 0;
}
static uint64_t idHash(const char* s) {  // FNV-1a de 64 bits
  uint64_t h = 1469598103934665603ull;
  for (; *s; s++) h = (h ^ (uint8_t)*s) * 1099511628211ull;
  return h;
}

// Líneas añadidas en esta sesión: la buena de cada torneo es la última (las demás, viejas).
struct Touch {
  char id[48];
  long at;
};
static Touch* touched = nullptr;
static int nTouched = 0, capTouched = 0;
static bool live(const char* id, long at) {
  for (int i = nTouched - 1; i >= 0; i--)
    if (!strcmp(touched[i].id, id)) return touched[i].at == at;
  return true;  // tras compactar, cada torneo tiene una sola línea
}
static void touch(const char* id, long at) {
  for (int i = 0; i < nTouched; i++)
    if (!strcmp(touched[i].id, id)) {
      touched[i].at = at;
      return;
    }
  if (!s3mem::reserve(touched, capTouched, nTouched + 1, 16)) return;
  copyText(touched[nTouched].id, 48, id);
  touched[nTouched++].at = at;
}

static char* lineBuf = nullptr;  // 2 KB, PSRAM (una línea del índice)
static const size_t INDEX_LINE = 2048;
static bool haveLineBuf() { return lineBuf || (lineBuf = (char*)malloc(INDEX_LINE)) != nullptr; }

// Cada línea buena del índice (la última de cada torneo y no descartado): fn(resumen, posición). fn devuelve false
// para parar. Las líneas sin salto al final (a medio escribir) no cuentan.
template <typename F>
static void eachLine(F fn) {
  char ip[96];
  indexPath(ip, sizeof(ip));
  FILE* f = haveLineBuf() ? fopen(ip, "rb") : nullptr;
  if (!f) return;
  for (;;) {
    long at = ftell(f);
    if (!fgets(lineBuf, (int)INDEX_LINE, f)) break;
    size_t len = strlen(lineBuf);
    if (!len || lineBuf[len - 1] != '\n') continue;  // a medio escribir (o demasiado larga)
    lineBuf[strcspn(lineBuf, "\r\n")] = 0;
    Summary s;
    if (!parseLine(lineBuf, s) || !live(s.id, at)) continue;
    if (!fn(s, at)) break;
  }
  fclose(f);
}
// Para competitions()/nextEdition() de la ficha (eachSummary del contrato).
static void eachSummaryFromIndex(SummaryFn fn, void* arg) {
  eachLine([&](const Summary& s, long) { return fn(s, arg); });
}

static bool compact(bool scanDir);
// Añade la línea de un torneo (o «X <id>» si `s` es nullptr) al final del índice.
static bool append(const Summary* s, const char* goneId) {
  char ip[96];
  indexPath(ip, sizeof(ip));
  FILE* f = fopen(ip, "ab");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  long at = ftell(f);
  bool ok = s ? writeLine(f, *s) : fprintf(f, "X\t%s\n", goneId) > 0;
  ok = fclose(f) == 0 && ok;
  if (ok) touch(s ? s->id : goneId, at);
  if (nTouched > 64) compact(false);  // muchos cambios en una sesión larga: se compacta
  return ok;
}

// Un torneo de trabajo (resúmenes de los que llegan o se rehacen), en la PSRAM.
static Tournament work;
static bool readFileInto(const char* path, Tournament& t) {
  char* txt = partidos::readAll(path);
  if (!txt) return false;
  bool ok = fromJson(txt, t);
  free(txt);
  return ok;
}

// Compacta el índice: una línea por torneo (la última), sin los descartados. Con `scanDir` (al encender), además
// quita los que ya no tienen archivo y añade los archivos sin línea (y recoge los restos de un corte de luz). La
// memoria de trabajo (12 bytes por línea) se libera al acabar.
static bool compact(bool scanDir) {
  char ip[96], tmp[104];
  indexPath(ip, sizeof(ip));
  snprintf(tmp, sizeof(tmp), "%s.tmp", ip);
  partidos::recoverFile(ip);
  if (!haveLineBuf()) return false;
  struct L { uint64_t h; uint32_t k; bool gone, keep; };
  L* ls = nullptr;
  int nl = 0, capL = 0;
  bool dirty = false, ok = true;
  // Pase 1: qué hay en cada línea.
  if (FILE* f = fopen(ip, "rb")) {
    while (ok && fgets(lineBuf, (int)INDEX_LINE, f)) {
      size_t len = strlen(lineBuf);
      if (!len || lineBuf[len - 1] != '\n') { dirty = true; continue; }
      lineBuf[len - 1] = 0;
      Summary s;
      char id[48];
      bool gone = goneLine(lineBuf, id, sizeof(id));
      if (!gone && !parseLine(lineBuf, s)) { dirty = true; continue; }
      if (!s3mem::reserve(ls, capL, nl + 1, 64)) { ok = false; break; }
      ls[nl] = L{idHash(gone ? id : s.id), (uint32_t)nl, gone, false};
      dirty |= gone;
      nl++;
    }
    fclose(f);
  } else {
    dirty = true;
  }
  if (!ok) { free(ls); return false; }
  // La última de cada torneo es la que vale.
  L** by = nl ? (L**)malloc((size_t)nl * sizeof(L*)) : nullptr;
  for (int i = 0; by && i < nl; i++) by[i] = &ls[i];
  if (by)
    qsort(by, (size_t)nl, sizeof(L*), [](const void* a, const void* b) {
      const L &x = **(L* const*)a, &y = **(L* const*)b;
      if (x.h != y.h) return x.h < y.h ? -1 : 1;
      return x.k < y.k ? -1 : x.k > y.k ? 1 : 0;
    });
  for (int i = 0; by && i < nl; i++) {
    bool last = i + 1 == nl || by[i + 1]->h != by[i]->h;
    by[i]->keep = last && !by[i]->gone;
    if (!last) dirty = true;
  }
  // Los archivos de la carpeta: los que faltan en el índice se añaden; las líneas sin archivo se quitan.
  char (*add)[48] = nullptr;
  int nAdd = 0, capAdd = 0;
  if (scanDir && (by || !nl)) {
    uint64_t* present = nullptr;
    int np = 0, capP = 0;
    bool listed = false;
    if (DIR* d = opendir(dir)) {
      listed = true;
      while (struct dirent* e = readdir(d)) {
        const char* n = e->d_name;
        size_t len = strlen(n);
        char path[160];
        snprintf(path, sizeof(path), "%s/%s", dir, n);
        if (len > 5 && !strcmp(n + len - 5, ".nube")) { ::remove(path); continue; }
        if (len > 4 && !strcmp(n + len - 4, ".tmp")) { ::remove(path); continue; }
        if (len > 9 && !strcmp(n + len - 9, ".json.bak")) {
          path[strlen(path) - 4] = 0;
          partidos::recoverFile(path);
          len -= 4;
        } else if (len <= 5 || strcmp(n + len - 5, ".json")) {
          continue;
        }
        if (len - 5 >= 48) continue;
        char id[48];
        snprintf(id, sizeof(id), "%.*s", (int)(len - 5), n);
        if (!s3mem::reserve(present, capP, np + 1, 64)) { listed = false; break; }
        present[np++] = idHash(id);
        bool inIndex = false;
        uint64_t h = idHash(id);
        for (int i = 0; i < nl && !inIndex; i++) inIndex = ls[i].h == h && ls[i].keep;
        bool goneHere = false;  // descartado (con «X») pero con archivo: se vuelve a borrar
        for (int i = 0; i < nl && !goneHere && !inIndex; i++) goneHere = ls[i].h == h;
        if (goneHere) {
          ::remove(path);
          continue;
        }
        if (inIndex) continue;
        if (!s3mem::reserve(add, capAdd, nAdd + 1, 8)) continue;
        memcpy(add[nAdd++], id, 48);
        dirty = true;
      }
      closedir(d);
    }
    if (listed) {
      qsort(present, (size_t)np, sizeof(uint64_t), [](const void* a, const void* b) {
        uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
        return x < y ? -1 : x > y ? 1 : 0;
      });
      for (int i = 0; i < nl; i++)
        if (ls[i].keep && !bsearch(&ls[i].h, present, (size_t)np, sizeof(uint64_t), [](const void* a, const void* b) {
              uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
              return x < y ? -1 : x > y ? 1 : 0;
            }))
          ls[i].keep = false, dirty = true;
    }
    free(present);
  }
  free(by);
  // Pase 2: el índice nuevo (si hay algo que cambiar).
  if (dirty) {
    FILE* in = fopen(ip, "rb");
    FILE* out = fopen(tmp, "wb");
    ok = out != nullptr;
    int k = 0;
    while (ok && in && fgets(lineBuf, (int)INDEX_LINE, in)) {
      size_t len = strlen(lineBuf);
      if (!len || lineBuf[len - 1] != '\n') continue;
      Summary s;
      char id[48];
      lineBuf[len - 1] = 0;
      char* again = (char*)malloc(len + 1);
      if (!again) { ok = false; break; }
      memcpy(again, lineBuf, len);
      bool valid = goneLine(lineBuf, id, sizeof(id)) || parseLine(lineBuf, s);
      if (valid) {
        if (k < nl && ls[k].keep) ok = fprintf(out, "%s\n", again) > 0;
        k++;
      }
      free(again);
    }
    if (in) fclose(in);
    for (int i = 0; ok && i < nAdd; i++) {
      char jp[112];
      jsonPath(add[i], jp, sizeof(jp));
      if (!alloc(work) || !readFileInto(jp, work)) continue;
      Summary s;
      summarizeAll(work, s);
      ok = writeLine(out, s);
    }
    if (out) ok = fclose(out) == 0 && ok;
    if (!ok || !partidos::replaceFile(tmp, ip)) ::remove(tmp), ok = false;
  }
  free(ls);
  free(add);
  if (ok) nTouched = 0;
  return ok;
}

// La memoria, otra vez desde el índice: los en juego y las ediciones recientes de cada competición.
static void select() {
  nSummaries = 0;
  eachLine([](const Summary& s, long) {
    keepSummary(s);
    return true;
  });
}

// Lo que necesita la progresión (s3_progresion.h): qué torneos siguen en juego y quién ganó cada uno (de todos).
static void syncProgresion() {
  progresion::TournamentInfo* list = nullptr;
  int n = 0, cap = 0;
  eachLine([&](const Summary& s, long) {
    if (s.status == Status::Cancelled || (s.status == Status::Finished && !s.finalMatchId[0])) return true;
    if (!s3mem::reserve(list, cap, n + 1, 64)) return false;
    progresion::TournamentInfo& x = list[n++];
    memset(&x, 0, sizeof(x));
    x.id = progresion::idHash(s.id);
    x.active = s.status == Status::Active;
    if (s.status == Status::Finished) x.finalMatch = progresion::idHash(s.finalMatchId);
    x.nChampion = s.nChampion;
    for (uint8_t k = 0; k < s.nChampion && k < 2; k++) x.champion[k] = partidos::codeOf(s.championIds[k]);
    return true;
  });
  progresion::setTournaments(list, n);
  free(list);
}

// Resumen de un torneo por su id: de memoria o del índice. false si no está.
static bool findAny(const char* id, Summary& out) {
  int i = findSummary(id);
  if (i >= 0) {
    out = summaries[i];
    return true;
  }
  bool found = false;
  eachLine([&](const Summary& s, long) {
    if (strcmp(s.id, id)) return true;
    out = s;
    found = true;
    return false;
  });
  return found;
}

// Tras guardar o descartar: memoria, progresión y pantallas al día.
static void changed(const Summary* s, const char* goneId) {
  append(s, goneId);
  if (s) keepSummary(*s);
  else {
    dropSummary(goneId);
    select();  // puede volver a memoria una edición antigua de esa competición
  }
  syncProgresion();
  ++revision;
}

// ---------------------------------------------------------------- tipos de torneo (FFat)
static bool allocTemplates() {
  if (!templates) templates = (Template*)calloc(MAX_TEMPLATES, sizeof(Template));
  return templates != nullptr;
}
static const char* pairingName(Pairing p) { return p == Pairing::Random ? "random" : p == Pairing::Manual ? "manual" : "elo"; }

static bool templateFromJson(const char* o, Template& t) {
  t = Template();
  strField(o, "id", t.id, sizeof(t.id));
  if (!t.id[0] || partidos::isTrue(partidos::keyIn(o, "builtIn"))) return false;  // los de fábrica se retiraron
  strField(o, "name", t.name, sizeof(t.name));
  char s[16];
  partidos::strOf(partidos::keyIn(o, "format"), s, sizeof(s));
  t.format = formatOf(s);
  t.teamSize = (uint8_t)partidos::numOf(partidos::keyIn(o, "teamSize"), t.format == Format::Pool ? 2 : 1);
  partidos::strOf(partidos::keyIn(o, "pairing"), s, sizeof(s));
  t.pairing = !strcmp(s, "random") ? Pairing::Random : !strcmp(s, "manual") ? Pairing::Manual : Pairing::Elo;
  partidos::strOf(partidos::keyIn(o, "seeding"), s, sizeof(s));
  t.seeding = !strcmp(s, "random") ? Seeding::Random : Seeding::Elo;
  const char* sw = partidos::keyIn(o, "poolSwap");
  t.poolSwap = !sw || partidos::isTrue(sw);
  partidos::strOf(partidos::keyIn(o, "poolLimit"), s, sizeof(s));
  t.poolByRounds = !strcmp(s, "rounds");
  t.poolMinutes = (uint8_t)partidos::numOf(partidos::keyIn(o, "poolMinutes"), 30);
  t.poolRounds = (uint8_t)partidos::numOf(partidos::keyIn(o, "poolRounds"), 5);
  partidos::strOf(partidos::keyIn(o, "final"), s, sizeof(s));
  t.finalKind = !strcmp(s, "top2") ? FinalKind::Top2 : FinalKind::None;
  t.finalBestOf = (uint8_t)partidos::numOf(partidos::keyIn(o, "finalBestOf"), 1);
  t.ranked = partidos::isTrue(partidos::keyIn(o, "ranked"));
  partidos::strOf(partidos::keyIn(o, "endCondition"), s, sizeof(s));
  t.rules.end = endOf(s);
  t.rules.goals = (uint8_t)partidos::numOf(partidos::keyIn(o, "goalsPerPeriod"), 5);
  t.rules.minutes = (uint8_t)partidos::numOf(partidos::keyIn(o, "minutesPerPeriod"), 5);
  // Final con reglas propias: alguno de sus tres campos puesto (finalGoals: null = los mismos goles).
  bool fe = hasKey(o, "finalEndCondition"), fg = hasKey(o, "finalGoals"), fm = hasKey(o, "finalMinutes");
  t.ownFinalRules = fe || fg || fm;
  t.finalRules = t.rules;
  t.finalRules.allowDraw = false;
  if (fe) partidos::strOf(partidos::keyIn(o, "finalEndCondition"), s, sizeof(s)), t.finalRules.end = endOf(s);
  if (fg) t.finalRules.goals = (uint8_t)partidos::numOf(partidos::keyIn(o, "finalGoals"), t.rules.goals);
  if (fm) t.finalRules.minutes = (uint8_t)partidos::numOf(partidos::keyIn(o, "finalMinutes"), t.rules.minutes);
  const char* ad = partidos::keyIn(o, "allowDraw");
  t.allowDraw = !ad || partidos::isTrue(ad);
  const char* pts = partidos::keyIn(o, "points");
  if (pts && *pts == '{') {
    t.points.win = (uint8_t)partidos::numOf(partidos::keyIn(pts, "win"), 3);
    t.points.draw = (uint8_t)partidos::numOf(partidos::keyIn(pts, "draw"), 1);
    t.points.loss = (uint8_t)partidos::numOf(partidos::keyIn(pts, "loss"), 0);
  }
  t.legs = (uint8_t)partidos::numOf(partidos::keyIn(o, "legs"), 1) == 2 ? 2 : 1;
  strField(o, "logo", t.logo, sizeof(t.logo));
  strField(o, "banner", t.banner, sizeof(t.banner));
  strField(o, "cup", t.cup, sizeof(t.cup));
  strField(o, "lastTitle", t.lastTitle, sizeof(t.lastTitle));
  return true;
}

// Escritura en memoria (para los tipos de torneo: van al chip con fs::FS y a la nube como texto).
struct Buf {
  char* p = nullptr;
  size_t n = 0, cap = 0;
  bool ok = true;
  void add(const char* fmt, ...) {
    if (!ok) return;
    va_list a;
    va_start(a, fmt);
    char tmp[256];
    int k = vsnprintf(tmp, sizeof(tmp), fmt, a);
    va_end(a);
    if (k < 0) { ok = false; return; }
    put(tmp, (size_t)k < sizeof(tmp) ? (size_t)k : sizeof(tmp) - 1);
  }
  void put(const char* s, size_t k) {
    if (!ok) return;
    if (n + k + 1 > cap) {
      size_t nc = cap ? cap * 2 : 1024;
      while (nc < n + k + 1) nc *= 2;
      char* q = (char*)realloc(p, nc);
      if (!q) { ok = false; return; }
      p = q;
      cap = nc;
    }
    memcpy(p + n, s, k);
    n += k;
    p[n] = 0;
  }
  void str(const char* s) {
    put("\"", 1);
    for (; *s; s++) {
      if ((uint8_t)*s < 0x20) continue;
      if (*s == '"' || *s == '\\') put("\\", 1);
      put(s, 1);
    }
    put("\"", 1);
  }
};

static void templateJson(Buf& b, const Template& t) {
  b.add("{\"id\":");
  b.str(t.id);
  b.add(",\"name\":");
  b.str(t.name);
  b.add(",\"format\":\"%s\",\"teamSize\":%u,\"pairing\":\"%s\",\"seeding\":\"%s\"", formatName(t.format), t.teamSize, pairingName(t.pairing),
        t.seeding == Seeding::Random ? "random" : "elo");
  if (t.format == Format::Pool)
    b.add(",\"poolSwap\":%s,\"poolLimit\":\"%s\",\"poolMinutes\":%u,\"poolRounds\":%u", t.poolSwap ? "true" : "false",
          t.poolByRounds ? "rounds" : "time", t.poolMinutes, t.poolRounds);
  b.add(",\"final\":\"%s\",\"finalBestOf\":%u,\"ranked\":%s,\"endCondition\":\"%s\",\"goalsPerPeriod\":%u,\"minutesPerPeriod\":%u",
        t.finalKind == FinalKind::Top2 ? "top2" : "none", t.finalBestOf > 1 ? 3 : 1, t.ranked ? "true" : "false", endName(t.rules.end),
        t.rules.goals, t.rules.minutes);
  if (t.ownFinalRules)
    b.add(",\"finalGoals\":%u,\"finalEndCondition\":\"%s\",\"finalMinutes\":%u", t.finalRules.goals, endName(t.finalRules.end), t.finalRules.minutes);
  else b.add(",\"finalGoals\":null");
  if (!t.allowDraw) b.add(",\"allowDraw\":false");
  if (t.format == Format::League && (t.points.win != 3 || t.points.draw != 1 || t.points.loss != 0))
    b.add(",\"points\":{\"win\":%u,\"draw\":%u,\"loss\":%u}", t.points.win, t.points.draw, t.points.loss);
  if (t.format == Format::League && t.legs == 2) b.add(",\"legs\":2");
  if (t.logo[0]) b.add(",\"logo\":"), b.str(t.logo);
  if (t.banner[0]) b.add(",\"banner\":"), b.str(t.banner);
  if (t.cup[0]) b.add(",\"cup\":"), b.str(t.cup);
  if (t.lastTitle[0]) b.add(",\"lastTitle\":"), b.str(t.lastTitle);
  b.add("}");
}
// El campo «tournamentTemplates» de la app. malloc: lo libera quien llama.
static char* templatesJson() {
  Buf b;
  b.add("[");
  for (uint8_t i = 0; i < nTemplates; i++) {
    if (i) b.add(",");
    templateJson(b, templates[i]);
  }
  b.add("]");
  if (!b.ok) {
    free(b.p);
    return nullptr;
  }
  return b.p;
}
static bool readTemplates(const char* arr) {
  if (!arr || *arr != '[' || !allocTemplates()) return false;
  Template* got = (Template*)calloc(MAX_TEMPLATES, sizeof(Template));
  if (!got) return false;
  uint8_t n = 0;
  for (const char* o = partidos::nextIn(arr, nullptr); o && n < MAX_TEMPLATES; o = partidos::nextIn(arr, o))
    if (templateFromJson(o, got[n])) n++;
  memcpy(templates, got, sizeof(Template) * n);
  free(got);
  nTemplates = n;
  return true;
}
static bool writeTemplatesFile(bool fromHere) {
  if (!chip || !allocTemplates()) return false;
  char* json = templatesJson();
  if (!json) return false;
  fs::File f = chip->open(TPL_TMP, FILE_WRITE);
  size_t len = strlen(json);
  bool ok = f && f.write((const uint8_t*)json, len) == len;
  if (f) f.close();
  free(json);
  if (!ok) return false;
  chip->remove(TPL_FILE);
  if (!chip->rename(TPL_TMP, TPL_FILE)) return false;
  if (fromHere && !templatesDirty) {
    fs::File p = chip->open(TPL_PEND, FILE_WRITE);
    if (p) p.close();
    templatesDirty = true;
  }
  return true;
}
static void loadTemplates() {
  nTemplates = 0;
  templatesDirty = false;
  if (!chip || !allocTemplates()) return;
  // Si se fue la luz al sustituirlo, el nuevo está completo en tipos.tmp.
  if (!chip->exists(TPL_FILE) && chip->exists(TPL_TMP)) chip->rename(TPL_TMP, TPL_FILE);
  templatesDirty = chip->exists(TPL_PEND);
  fs::File f = chip->open(TPL_FILE, FILE_READ);
  if (!f) return;
  size_t n = f.size();
  char* txt = (char*)malloc(n + 1);
  if (txt) {
    txt[f.read((uint8_t*)txt, n)] = 0;
    const char* p = txt;
    while (*p && *p != '[') p++;
    readTemplates(p);
    free(txt);
  }
  f.close();
}
static char* templatesGoneIds() {
  if (!chip) return nullptr;
  fs::File g = chip->open(TPL_GONE, FILE_READ);
  if (!g) return nullptr;
  size_t n = g.size();
  char* out = (char*)malloc(n + 1);
  if (out) out[g.read((uint8_t*)out, n)] = 0;
  g.close();
  return out;
}
static void templatesPushed() {
  if (chip) chip->remove(TPL_PEND);
  if (chip) chip->remove(TPL_GONE);
  templatesDirty = false;
}

// ---------------------------------------------------------------- nube (lo llama s3_nube.h desde loop())
static bool loggedIn() { return nube::loggedIn(); }
static void queue(char kind, const char* id) {
  if (!loggedIn()) return;
  char l[64];
  snprintf(l, sizeof(l), "%c %s", kind, id);
  nube::addPending(l);
}
static void matchGone(const char* id) { queue('x', id); }

// Llegó un torneo de la cuenta (ya escrito como <id>.nube): pasa a ser el de verdad, salvo que aquí se haya cambiado
// o descartado mientras tanto (lo de aquí gana y se sube, como en la app).
static void cloudArrived(const char* id) {
  char from[112], to[112], l[64];
  cloudPath(id, from, sizeof(from));
  jsonPath(id, to, sizeof(to));
  snprintf(l, sizeof(l), "t %s", id);
  bool mine = nube::isPending(l);
  snprintf(l, sizeof(l), "T %s", id);
  mine = mine || nube::isPending(l);
  if (mine || !alloc(work) || !readFileInto(from, work) || !partidos::replaceFile(from, to)) {
    ::remove(from);
    return;
  }
  Summary s;
  summarizeAll(work, s);
  changed(&s, nullptr);
}
// Lo borraron en la cuenta (sus partidos llegan borrados aparte).
static void cloudGone(const char* id) {
  char l[64];
  snprintf(l, sizeof(l), "t %s", id);
  if (nube::isPending(l)) return;  // aquí se ha cambiado: se queda y se sube
  char path[112];
  jsonPath(id, path, sizeof(path));
  ::remove(path);
  changed(nullptr, id);
}

// Un jugador ha cambiado de id (se ha unido con su ficha de la cuenta): también en los torneos donde juega.
static void playerIdChanged(const char* oldId, const char* newId) {
  char from[64], to[64];
  snprintf(from, sizeof(from), "\"%s\"", oldId);
  snprintf(to, sizeof(to), "\"%s\"", newId);
  size_t fl = strlen(from), tl = strlen(to);
  // Todos los torneos (del índice): primero la lista de ids, luego cada archivo.
  char (*ids)[48] = nullptr;
  int n = 0, cap = 0;
  eachLine([&](const Summary& s, long) {
    if (!s3mem::reserve(ids, cap, n + 1, 32)) return false;
    memcpy(ids[n++], s.id, 48);
    return true;
  });
  for (int i = 0; i < n; i++) {
    char path[112], tmp[120];
    jsonPath(ids[i], path, sizeof(path));
    char* txt = partidos::readAll(path);
    if (!txt || !strstr(txt, from)) {
      free(txt);
      continue;
    }
    snprintf(tmp, sizeof(tmp), "%s/%s.tmp", dir, ids[i]);
    FILE* f = fopen(tmp, "wb");
    bool ok = f != nullptr;
    for (const char* q = txt; ok && *q;) {
      if (!strncmp(q, from, fl)) ok = fwrite(to, 1, tl, f) == tl, q += fl;
      else ok = fputc(*q++, f) != EOF;
    }
    if (f) ok = fclose(f) == 0 && ok;
    free(txt);
    if (!ok || !partidos::replaceFile(tmp, path)) {
      ::remove(tmp);
      continue;
    }
    if (alloc(work) && readFileInto(path, work)) {
      Summary s;
      summarizeAll(work, s);
      changed(&s, nullptr);
    }
    queue('t', ids[i]);
  }
  free(ids);
}

}  // namespace guardado

// ---------------------------------------------------------------- lo del contrato
bool loadIndex() {
  using namespace guardado;
  ++revision;
  compact(true);  // una línea por torneo y que cuadre con la carpeta
  select();
  syncProgresion();
  return true;
}

void begin(bool sd) {
  using namespace guardado;
  pickDir(sd);
  mkdirs(dir);
  loadIndex();
  loadTemplates();
  // La nube: filas «tournament» y el campo «tournamentTemplates» de las preferencias.
  snprintf(nube::tournamentsDir, sizeof(nube::tournamentsDir), "%s", dir);
  nube::tournamentArrived = cloudArrived;
  nube::tournamentGone = cloudGone;
  nube::tournamentsLoggedIn = []() {
    eachLine([](const Summary& s, long) {  // todos, también los antiguos
      queue('t', s.id);
      return true;
    });
    if (nTemplates) writeTemplatesFile(true);
  };
  nube::templatesOut = []() -> char* { return templatesDirty ? templatesJson() : nullptr; };
  nube::templatesGone = templatesGoneIds;
  nube::templatesIn = [](const char* arr) {
    if (readTemplates(arr)) writeTemplatesFile(false);
    ++revision;
  };
  nube::templatesDone = templatesPushed;
  partidos::playerIdChanged = playerIdChanged;
  matchSource = matchesOf;
  eachSummary = eachSummaryFromIndex;
}

bool load(const char* id, Tournament& t) {
  char path[112];
  guardado::jsonPath(id, path, sizeof(path));
  return guardado::readFileInto(path, t);
}

bool save(Tournament& t) {
  using namespace guardado;
  if (!t.id[0] || !t.teams || !t.fix) return false;
  mkdirs(dir);
  char path[112], tmp[120];
  jsonPath(t.id, path, sizeof(path));
  snprintf(tmp, sizeof(tmp), "%s/%s.tmp", dir, t.id);
  char* old = partidos::readAll(path);  // lo que no conoce la placa se conserva
  FILE* f = fopen(tmp, "wb");
  bool ok = f != nullptr;
  if (f) {
    setvbuf(f, nullptr, _IOFBF, 4096);
    ok = toJson(f, t, old);
    ok = fclose(f) == 0 && ok;
  }
  free(old);
  if (!ok || !partidos::replaceFile(tmp, path)) {
    ::remove(tmp);
    return false;
  }
  Summary s;
  guardado::summarizeAll(t, s);
  changed(&s, nullptr);
  queue('t', t.id);
  return true;
}

bool discard(const char* id) {
  using namespace guardado;
  if (!id || !id[0]) return false;
  // Sus partidos primero (como discardTournament de la app), también de la cuenta.
  partidos::removeTournament(id, matchGone);
  char path[112];
  jsonPath(id, path, sizeof(path));
  bool had = findSummary(id) >= 0;
  FILE* f = fopen(path, "rb");
  if (f) fclose(f), had = true;
  ::remove(path);
  changed(nullptr, id);
  queue('T', id);
  return had;
}

bool saveTemplate(const Template& t) {
  using namespace guardado;
  if (!t.id[0] || !allocTemplates()) return false;
  int at = -1;
  for (uint8_t i = 0; i < nTemplates; i++)
    if (!strcmp(templates[i].id, t.id)) at = i;
  if (at < 0) {
    if (nTemplates >= MAX_TEMPLATES) return false;
    at = nTemplates++;
  }
  templates[at] = t;
  ++revision;
  return writeTemplatesFile(true);
}

bool removeTemplate(const char* id) {
  using namespace guardado;
  int at = -1;
  for (uint8_t i = 0; i < nTemplates; i++)
    if (!strcmp(templates[i].id, id)) at = i;
  if (at < 0) return false;
  // Que la cuenta no lo devuelva al unir las listas.
  if (chip) {
    size_t n = 0;
    char* had = nullptr;
    if (fs::File g = chip->open(TPL_GONE, FILE_READ)) {
      n = g.size();
      had = (char*)malloc(n + 64);
      if (had) n = g.read((uint8_t*)had, n);
      g.close();
    } else {
      had = (char*)malloc(64);
    }
    if (had) {
      n += (size_t)snprintf(had + n, 64, "%s\n", id);
      if (fs::File g = chip->open(TPL_GONE, FILE_WRITE)) {
        g.write((const uint8_t*)had, n);
        g.close();
      }
      free(had);
    }
  }
  memmove(&templates[at], &templates[at + 1], (size_t)(nTemplates - at - 1) * sizeof(Template));
  nTemplates--;
  ++revision;
  return writeTemplatesFile(true);
}

// newTemplate de templates.ts: copia de otro («… (copia)») o los valores de por defecto del formato.
void newTemplate(Template& out, Format f, const Template* copyOf) {
  char id[48];
  partidos::newId(id + 4);
  memcpy(id, "tpl_", 4);
  if (copyOf) {
    out = *copyOf;
    memcpy(out.id, id, sizeof(out.id));
    char name[64];
    snprintf(name, sizeof(name), "%s (copia)", copyOf->name);
    guardado::copyText(out.name, sizeof(out.name), name);
    return;
  }
  out = Template();  // con los textos a cero
  memcpy(out.id, id, sizeof(out.id));
  out.format = f;
  out.teamSize = f == Format::Pool ? 2 : 1;
  out.pairing = f == Format::Pool ? Pairing::Random : Pairing::Elo;
}

// Ediciones de una competición en la SD (no descartadas), de la más nueva a la más antigua: primero dónde está cada
// una (16 bytes por edición, solo mientras dura) y luego se leen las de la página.
namespace guardado {
struct At {
  uint64_t createdAt;
  long at;
  char id[48];
};
static At* editionsOf(const char* key, int& n) {
  At* list = nullptr;
  int cap = 0;
  n = 0;
  eachLine([&](const Summary& s, long at) {
    if (s.status == Status::Cancelled || strcmp(s.competitionKey, key)) return true;
    if (!s3mem::reserve(list, cap, n + 1, 32)) return false;
    list[n].createdAt = s.createdAt;
    list[n].at = at;
    memcpy(list[n].id, s.id, 48);
    n++;
    return true;
  });
  qsort(list, (size_t)n, sizeof(At), [](const void* a, const void* b) {
    const At &x = *(const At*)a, &y = *(const At*)b;
    if (x.createdAt != y.createdAt) return x.createdAt > y.createdAt ? -1 : 1;
    return -strcmp(x.id, y.id);
  });
  return list;
}
}  // namespace guardado

uint16_t editionCount(const char* key) {
  uint16_t n = 0;
  guardado::eachLine([&](const Summary& s, long) {
    if (s.status != Status::Cancelled && !strcmp(s.competitionKey, key)) n++;
    return true;
  });
  return n;
}

uint16_t editionsPage(const char* key, uint16_t from, Summary* out, uint16_t max) {
  using namespace guardado;
  int n = 0;
  At* list = editionsOf(key, n);
  uint16_t k = 0;
  char ip[96];
  indexPath(ip, sizeof(ip));
  FILE* f = list && haveLineBuf() ? fopen(ip, "rb") : nullptr;
  for (int i = from; f && i < n && k < max; i++) {
    if (fseek(f, list[i].at, SEEK_SET) != 0 || !fgets(lineBuf, (int)INDEX_LINE, f)) continue;
    lineBuf[strcspn(lineBuf, "\r\n")] = 0;
    if (parseLine(lineBuf, out[k])) k++;
  }
  if (f) fclose(f);
  free(list);
  return k;
}

// ---------------------------------------------------------------- extras (fuera del contrato)
// Todo lo de aquí, a la cuenta (al entrar o tras restaurar una copia).
static void queueAll() {
  if (nube::tournamentsLoggedIn) nube::tournamentsLoggedIn();
}
// Resumen de un torneo por su id, si está en memoria (en juego o reciente); si no, nullptr.
static const Summary* summaryOf(const char* id) {
  int i = guardado::findSummary(id);
  return i >= 0 ? &summaries[i] : nullptr;
}
// Resumen de cualquier torneo (de memoria o del índice de la SD). false si no está.
static bool findSummary(const char* id, Summary& out) { return guardado::findAny(id, out); }

}  // namespace torneos

#endif  // MFV3_S3_TORNEOS_DATOS_H
