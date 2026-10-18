// MARCADOR FUTBOLÍN V3 · historial de partidos de la pantalla de 7": todos, en la tarjeta SD.
// -------------------------------------------------------------------------------------------------
// Cada partido terminado se guarda como lo guarda la app (StoredMatch de src/services/persistence/types.ts, en
// JSON): /sdcard/partidos/<id>.json. Así se puede pasar tal cual a la app o a la nube. Además, una línea por
// partido en /sdcard/partidos/indice.txt con lo que enseña la lista del historial (se lee entera al encender):
//   <id>|<fin, segundos desde 1970>|<modo q/c/r>|<ganador w/b/->|<cómo r/g/p>|<B>|<A>|<penaltis B|->|<penaltis A|->|
//   <código>,<nombre>;…  (Blanco)|<código>,<nombre>;…  (Azul)
// Sin tarjeta, en la memoria del chip (/ffat/partidos), con sitio para menos.
// La hora de la placa es la de España: aquí se pasa a la hora universal que usa la app (cambio de hora de la UE).

#ifndef MFV3_S3_PARTIDOS_H
#define MFV3_S3_PARTIDOS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "mfv3_engine.h"
#include "s3_reloj.h"

namespace partidos {

const char* const SD_DIR = "/sdcard/partidos";
const char* const CHIP_DIR = "/ffat/partidos";
static char dir[64] = "";

#ifndef ARDUINO
static void setDir(const char* d) { snprintf(dir, sizeof(dir), "%s", d); }  // PC: carpeta de pruebas
#endif

// ---------------------------------------------------------------- lo que enseña la lista y lo que usan los cálculos
// (ELO, XP, logros y retos de la app: s3_progresion.h). Todo sale del JSON del partido (rowFromJson), sea de la placa
// o de la app.
struct Row {
  char id[48];  // la placa: UUID (36); la app: «m_» + UUID
  uint32_t fin;  // segundos desde 1970 (UTC); 0 = sin hora
  char mode;     // 'q' rápido, 'c' loco, 'r' clasificatorio
  char winner;   // 'w', 'b' o '-' (empate)
  char reason;   // 'r' normal, 'g' gol de oro, 'p' penaltis
  uint8_t w, b;
  int16_t pw, pb;  // -1 sin penaltis
  uint8_t n[2];
  uint32_t code[2][4];  // FNV-1a del id del jugador en la app (como los sonidos)
  char name[2][4][20];
  // Para los cálculos
  uint32_t totalMs;
  uint8_t comeback;       // mayor desventaja que remontó el ganador
  int32_t earliest[2];    // gol válido más temprano de cada equipo (ms desde el inicio de su parte; -1 sin goles)
  uint8_t run[2];         // mayor serie de goles seguidos de cada equipo
  int8_t firstScorer;     // quién marcó el primer gol válido (equipo × 4 + plaza; -1 nadie apuntado)
  uint8_t pg[2][4];       // goles apuntados a cada jugador
  uint8_t kicks[2], kicksIn[2], sdKicks, penRounds;
};
static Row* rows = nullptr;  // en orden de guardado (el último, el más nuevo)
static int count = 0, cap = 0;

static bool push(const Row& r) {
  if (count == cap) {
    int nc = cap ? cap * 2 : 256;
    Row* nr = (Row*)realloc(rows, (size_t)nc * sizeof(Row));
    if (!nr) return false;
    rows = nr;
    cap = nc;
  }
  rows[count++] = r;
  return true;
}

// Código de un jugador a partir de su id: FNV-1a (como la app); «placa-<código>» lleva el código dentro.
static uint32_t codeOf(const char* id) {
  if (!strncmp(id, "placa-", 6) && strlen(id) == 14) return (uint32_t)strtoul(id + 6, nullptr, 16);
  uint32_t h = 2166136261u;
  for (const char* s = id; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
  return h;
}

// ---------------------------------------------------------------- JSON (lo justo para leer un StoredMatch)
static const char* skipStr(const char* p) {  // p en la comilla de apertura → tras la de cierre
  for (p++; *p && *p != '"'; p++)
    if (*p == '\\' && p[1]) p++;
  return *p ? p + 1 : p;
}
static const char* closeOf(const char* p) {  // p en { o [ → su cierre
  int depth = 0;
  for (; *p; p++) {
    if (*p == '"') {
      p = skipStr(p) - 1;
      continue;
    }
    if (*p == '{' || *p == '[') depth++;
    else if ((*p == '}' || *p == ']') && --depth == 0) return p;
  }
  return p;
}
// Valor de `key` en el objeto que empieza en `o` (solo en su primer nivel).
static const char* keyIn(const char* o, const char* key) {
  if (!o || *o != '{') return nullptr;
  size_t kl = strlen(key);
  int depth = 0;
  for (const char* p = o; *p; p++) {
    if (*p == '"') {
      const char* e = skipStr(p);
      if (depth == 1 && (size_t)(e - p - 2) == kl && !memcmp(p + 1, key, kl)) {
        const char* v = e;
        while (*v == ' ' || *v == ':') v++;
        if (e[0] == ':') return v;
      }
      p = e - 1;
      continue;
    }
    if (*p == '{' || *p == '[') depth++;
    else if (*p == '}' || *p == ']') {
      if (--depth == 0) return nullptr;
    }
  }
  return nullptr;
}
static void strOf(const char* v, char* out, size_t n) {
  size_t i = 0;
  if (v && *v == '"')
    for (v++; *v && *v != '"' && i < n - 1; v++) {
      if (*v == '\\' && v[1]) v++;
      out[i++] = *v;
    }
  out[i] = 0;
}
static double numOf(const char* v, double def = 0) {
  return v && (*v == '-' || (*v >= '0' && *v <= '9')) ? strtod(v, nullptr) : def;
}
static bool isTrue(const char* v) { return v && !strncmp(v, "true", 4); }
// Cada objeto de un array: el siguiente tras `prev` (nullptr = el primero).
static const char* nextIn(const char* arr, const char* prev) {
  if (!arr || *arr != '[') return nullptr;
  const char* p = prev ? closeOf(prev) + 1 : arr + 1;
  while (*p && *p != '{' && *p != ']') p++;
  return *p == '{' ? p : nullptr;
}
static int teamOf(const char* v) {
  char t[8];
  strOf(v, t, sizeof(t));
  return !strcmp(t, "blue") ? 1 : !strcmp(t, "white") ? 0 : -1;
}

static char* readAll(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return nullptr;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* t = n > 0 && n < 4000000 ? (char*)malloc((size_t)n + 1) : nullptr;
  bool ok = t && fread(t, 1, (size_t)n, f) == (size_t)n;
  fclose(f);
  if (!ok) {
    free(t);
    return nullptr;
  }
  t[n] = 0;
  return t;
}

static void clean(char* s) {  // los separadores del índice no pueden ir en un nombre
  for (; *s; s++)
    if (*s == '|' || *s == ';' || *s == ',') *s = ' ';
}

// La fila de un partido a partir de su JSON (formato StoredMatch de la app).
static bool rowFromText(const char* t, Row& r) {
  memset(&r, 0, sizeof(r));
  while (*t == ' ' || *t == '\n' || *t == '\r' || *t == '\t') t++;
  if (*t != '{') return false;
  strOf(keyIn(t, "id"), r.id, sizeof(r.id));
  if (!r.id[0]) return false;
  r.fin = (uint32_t)(numOf(keyIn(t, "finishedAt")) / 1000.0);
  const char* cfg = keyIn(t, "config");
  char s[24];
  strOf(keyIn(cfg, "mode"), s, sizeof(s));
  r.mode = !strcmp(s, "chaos") ? 'c' : !strcmp(s, "ranked") ? 'r' : 'q';
  r.penRounds = (uint8_t)numOf(keyIn(cfg, "penaltyRounds"), 5);
  const char* res = keyIn(t, "result");
  int win = teamOf(keyIn(res, "winner"));
  r.winner = win == 1 ? 'b' : win == 0 ? 'w' : '-';
  strOf(keyIn(res, "reason"), s, sizeof(s));
  r.reason = !strcmp(s, "golden_goal") ? 'g' : !strcmp(s, "penalties") ? 'p' : 'r';
  const char* sc = keyIn(res, "score");
  r.w = (uint8_t)numOf(keyIn(sc, "white"));
  r.b = (uint8_t)numOf(keyIn(sc, "blue"));
  const char* ps = keyIn(res, "penaltyScore");
  r.pw = ps && *ps == '{' ? (int16_t)numOf(keyIn(ps, "white")) : -1;
  r.pb = ps && *ps == '{' ? (int16_t)numOf(keyIn(ps, "blue")) : -1;
  r.totalMs = (uint32_t)numOf(keyIn(res, "totalTimeMs"));
  // Jugadores (por equipo, en el orden del partido)
  static char ids[2][4][48];
  memset(ids, 0, sizeof(ids));
  const char* parts = keyIn(t, "participants");
  for (const char* o = nextIn(parts, nullptr); o; o = nextIn(parts, o)) {
    int tm = teamOf(keyIn(o, "team"));
    if (tm < 0 || r.n[tm] >= 4) continue;
    int i = r.n[tm]++;
    strOf(keyIn(o, "playerId"), ids[tm][i], sizeof(ids[0][0]));
    r.code[tm][i] = codeOf(ids[tm][i]);
    strOf(keyIn(o, "nameSnapshot"), r.name[tm][i], sizeof(r.name[0][0]));
    clean(r.name[tm][i]);
  }
  // Goles válidos (sin los anulados), como validGoalsFromEvents.
  const char* evs = keyIn(t, "events");
  static char annulled[160][12];
  int na = 0;
  for (const char* o = nextIn(evs, nullptr); o; o = nextIn(evs, o)) {
    strOf(keyIn(o, "type"), s, sizeof(s));
    char ref[12];
    strOf(keyIn(o, "refEventId"), ref, sizeof(ref));
    if (!strcmp(s, "CORRECTION") && ref[0] && na < 160) snprintf(annulled[na++], 12, "%s", ref);
  }
  // UNDO: deshace un gol (anulado) o una anulación (vuelve a valer).
  for (const char* o = nextIn(evs, nullptr); o; o = nextIn(evs, o)) {
    strOf(keyIn(o, "type"), s, sizeof(s));
    if (strcmp(s, "UNDO")) continue;
    char ref[12];
    strOf(keyIn(o, "refEventId"), ref, sizeof(ref));
    for (const char* q = nextIn(evs, nullptr); q && ref[0]; q = nextIn(evs, q)) {
      char qid[12], qt[16];
      strOf(keyIn(q, "id"), qid, sizeof(qid));
      if (strcmp(qid, ref)) continue;
      strOf(keyIn(q, "type"), qt, sizeof(qt));
      if (!strcmp(qt, "GOAL") && na < 160) snprintf(annulled[na++], 12, "%s", qid);
      if (!strcmp(qt, "CORRECTION")) {
        char back[12];
        strOf(keyIn(q, "refEventId"), back, sizeof(back));
        for (int i = 0; i < na; i++)
          if (!strcmp(annulled[i], back)) annulled[i][0] = 0;
      }
      break;
    }
  }
  const char* scorers = keyIn(t, "scorers");
  r.earliest[0] = r.earliest[1] = -1;
  r.firstScorer = -1;
  int score[2] = {0, 0}, runNow[2] = {0, 0}, maxDef = 0, loser = win == 0 ? 1 : 0;
  bool first = true;
  for (const char* o = nextIn(evs, nullptr); o; o = nextIn(evs, o)) {
    strOf(keyIn(o, "type"), s, sizeof(s));
    if (strcmp(s, "GOAL")) continue;
    char gid[12];
    strOf(keyIn(o, "id"), gid, sizeof(gid));
    bool off = false;
    for (int i = 0; i < na && !off; i++) off = annulled[i][0] && !strcmp(annulled[i], gid);
    int tm = teamOf(keyIn(o, "team"));
    if (off || tm < 0) continue;
    int32_t ms = (int32_t)numOf(keyIn(o, "periodTimeMs"));
    if (r.earliest[tm] < 0 || ms < r.earliest[tm]) r.earliest[tm] = ms;
    runNow[tm]++;
    runNow[1 - tm] = 0;
    if (runNow[tm] > r.run[tm]) r.run[tm] = (uint8_t)runNow[tm];
    score[tm] += (int)numOf(keyIn(o, "value"), 1);
    if (win >= 0 && score[loser] - score[win] > maxDef) maxDef = score[loser] - score[win];
    char who[48] = "";
    if (scorers && *scorers == '{') strOf(keyIn(scorers, gid), who, sizeof(who));
    for (int t2 = 0; t2 < 2 && who[0]; t2++)
      for (int i = 0; i < r.n[t2]; i++)
        if (!strcmp(ids[t2][i], who)) {
          if (r.pg[t2][i] < 255) r.pg[t2][i]++;
          if (first) r.firstScorer = (int8_t)(t2 * 4 + i);
        }
    first = false;
  }
  r.comeback = (uint8_t)maxDef;
  const char* pens = keyIn(t, "penalties");
  for (const char* o = nextIn(pens, nullptr); o; o = nextIn(pens, o)) {
    int tm = teamOf(keyIn(o, "team"));
    if (tm < 0) continue;
    r.kicks[tm]++;
    if (isTrue(keyIn(o, "scored"))) r.kicksIn[tm]++;
    if (isTrue(keyIn(o, "suddenDeath"))) r.sdKicks++;
  }
  return true;
}

static bool rowFromJson(const char* path, Row& r) {
  char* t = readAll(path);
  if (!t) return false;
  bool ok = rowFromText(t, r);
  free(t);
  return ok;
}

// ---------------------------------------------------------------- índice
// Una línea por partido: id|fin|modo|ganador|cómo|B|A|penB|penA|<código,nombre;…>|<…>|x<cálculos, con comas>
static void parseSide(const char* s, const char* end, Row& r, int t) {
  r.n[t] = 0;
  while (s < end && r.n[t] < 4) {
    const char* semi = (const char*)memchr(s, ';', (size_t)(end - s));
    if (!semi) semi = end;
    const char* comma = (const char*)memchr(s, ',', (size_t)(semi - s));
    if (comma) {
      r.code[t][r.n[t]] = (uint32_t)strtoul(s, nullptr, 16);
      snprintf(r.name[t][r.n[t]], sizeof(r.name[0][0]), "%.*s", (int)(semi - comma - 1), comma + 1);
      r.n[t]++;
    }
    s = semi + 1;
  }
}

static bool parseLine(char* l, Row& r) {
  memset(&r, 0, sizeof(r));
  char* f[12];
  int k = 0;
  f[k++] = l;
  for (char* p = l; *p && k < 12; p++)
    if (*p == '|') {
      *p = 0;
      f[k++] = p + 1;
    }
  if (k < 12 || strlen(f[0]) > 47 || f[11][0] != 'x') return false;  // sin los cálculos: se lee el JSON otra vez
  snprintf(r.id, sizeof(r.id), "%s", f[0]);
  r.fin = (uint32_t)strtoul(f[1], nullptr, 10);
  r.mode = f[2][0];
  r.winner = f[3][0];
  r.reason = f[4][0];
  r.w = (uint8_t)atoi(f[5]);
  r.b = (uint8_t)atoi(f[6]);
  r.pw = f[7][0] == '-' ? -1 : (int16_t)atoi(f[7]);
  r.pb = f[8][0] == '-' ? -1 : (int16_t)atoi(f[8]);
  parseSide(f[9], f[9] + strlen(f[9]), r, 0);
  parseSide(f[10], f[10] + strlen(f[10]), r, 1);
  long v[26] = {0};
  int nv = 0;
  for (char* p = f[11] + 1; *p && nv < 26;) {
    v[nv++] = strtol(p, &p, 10);
    if (*p == ',') p++;
  }
  if (nv < 21) return false;
  r.totalMs = (uint32_t)v[0];
  r.comeback = (uint8_t)v[1];
  r.earliest[0] = (int32_t)v[2];
  r.earliest[1] = (int32_t)v[3];
  r.run[0] = (uint8_t)v[4];
  r.run[1] = (uint8_t)v[5];
  r.firstScorer = (int8_t)v[6];
  r.kicks[0] = (uint8_t)v[7];
  r.kicks[1] = (uint8_t)v[8];
  r.kicksIn[0] = (uint8_t)v[9];
  r.kicksIn[1] = (uint8_t)v[10];
  r.sdKicks = (uint8_t)v[11];
  r.penRounds = (uint8_t)v[12];
  for (int i = 0; i < 8; i++) r.pg[i / 4][i % 4] = (uint8_t)v[13 + i];
  return true;
}

static int lineOf(const Row& r, char* line, size_t size) {
  int n = snprintf(line, size, "%s|%lu|%c|%c|%c|%u|%u|", r.id, (unsigned long)r.fin, r.mode, r.winner, r.reason, r.w, r.b);
  n += r.pw >= 0 ? snprintf(line + n, size - n, "%d|%d|", r.pw, r.pb) : snprintf(line + n, size - n, "-|-|");
  for (int t = 0; t < 2; t++) {
    for (int i = 0; i < r.n[t]; i++)
      n += snprintf(line + n, size - n, "%s%08lx,%s", i ? ";" : "", (unsigned long)r.code[t][i], r.name[t][i]);
    n += snprintf(line + n, size - n, "|");
  }
  n += snprintf(line + n, size - n, "x%lu,%u,%ld,%ld,%u,%u,%d,%u,%u,%u,%u,%u,%u", (unsigned long)r.totalMs, r.comeback,
                (long)r.earliest[0], (long)r.earliest[1], r.run[0], r.run[1], r.firstScorer, r.kicks[0], r.kicks[1], r.kicksIn[0],
                r.kicksIn[1], r.sdKicks, r.penRounds);
  for (int i = 0; i < 8; i++) n += snprintf(line + n, size - n, ",%u", r.pg[i / 4][i % 4]);
  return n;
}

static void indexPath(char* out, size_t n) { snprintf(out, n, "%s/indice.txt", dir); }
static void jsonPath(const char* id, char* out, size_t n) { snprintf(out, n, "%s/%s.json", dir, id); }

static bool appendIndex(const Row& r) {
  char ip[96], line[700];
  indexPath(ip, sizeof(ip));
  FILE* x = fopen(ip, "ab");
  if (!x) return false;
  lineOf(r, line, sizeof(line));
  fprintf(x, "%s\n", line);
  fclose(x);
  return true;
}

static void rewriteIndex() {
  char ip[96], tmp[100], line[700];
  indexPath(ip, sizeof(ip));
  snprintf(tmp, sizeof(tmp), "%s.tmp", ip);
  FILE* out = fopen(tmp, "wb");
  if (!out) return;
  for (int i = 0; i < count; i++) {
    lineOf(rows[i], line, sizeof(line));
    fprintf(out, "%s\n", line);
  }
  fclose(out);
  ::remove(ip);
  rename(tmp, ip);
}

// Al encender: dónde se guardan y la lista de los que ya hay.
static void begin(bool sd) {
#ifdef ARDUINO
  snprintf(dir, sizeof(dir), "%s", sd ? SD_DIR : CHIP_DIR);
  mkdir(dir, 0777);
#else
  (void)sd;
  if (!dir[0]) snprintf(dir, sizeof(dir), "../build/placa/partidos-sd");
#ifdef _WIN32
  mkdir(dir);
#else
  mkdir(dir, 0777);
#endif
#endif
  count = 0;
  char path[96];
  indexPath(path, sizeof(path));
  FILE* f = fopen(path, "rb");
  if (!f) return;
  char line[700];
  bool dirty = false;
  while (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\r\n")] = 0;
    Row r;
    char id[48];
    snprintf(id, sizeof(id), "%.*s", (int)strcspn(line, "|") < 47 ? (int)strcspn(line, "|") : 47, line);
    if (parseLine(line, r)) {
      push(r);
    } else if (strlen(id) >= 36) {  // índice de una versión anterior: se vuelve a leer el partido
      char jp[112];
      jsonPath(id, jp, sizeof(jp));
      if (rowFromJson(jp, r)) push(r);
      dirty = true;
    }
  }
  fclose(f);
  if (dirty) rewriteIndex();
}

// ---------------------------------------------------------------- hora
static bool summer(const reloj::Clock& c) {  // horario de verano de la UE (en hora local)
  auto lastSunday = [](uint16_t y, uint8_t m) {
    reloj::Clock d{y, m, 31, 0, 0, 0};
    return (uint8_t)(31 - reloj::weekday(d));
  };
  if (c.month < 3 || c.month > 10) return false;
  if (c.month > 3 && c.month < 10) return true;
  uint8_t ls = lastSunday(c.year, c.month);
  if (c.month == 3) return c.day > ls || (c.day == ls && c.hour >= 2);
  return c.day < ls || (c.day == ls && c.hour < 3);
}
// Hora local de la placa → segundos desde 1970 (UTC).
static uint32_t epoch(const reloj::Clock& c) {
  uint32_t m = reloj::stamp(c) - 1;  // minutos desde 2000
  uint32_t s = 946684800u + m * 60u + c.second;
  return s - (summer(c) ? 7200u : 3600u);
}

// ---------------------------------------------------------------- partido para guardar
struct Who {
  char id[48];
  char name[28];
  uint32_t code;
};
struct Match {
  char mode;  // 'q', 'c', 'r'
  mfv3::Config cfg;
  uint8_t n[2] = {0, 0};
  Who who[2][4];
  uint32_t startEpoch = 0, endEpoch = 0;  // segundos (0 = sin hora)
  uint32_t buildId = 0;
};

static void newId(char* out) {  // UUID v4
  uint8_t b[16];
  for (int i = 0; i < 16; i += 4) {
#ifdef ARDUINO
    uint32_t r = esp_random();
#else
    uint32_t r = (uint32_t)rand() * 2654435761u ^ (uint32_t)rand();
#endif
    memcpy(b + i, &r, 4);
  }
  b[6] = (uint8_t)((b[6] & 0x0F) | 0x40);
  b[8] = (uint8_t)((b[8] & 0x3F) | 0x80);
  snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", b[0], b[1], b[2], b[3], b[4], b[5],
           b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

// Texto JSON con comillas y barras escapadas.
static void jstr(FILE* f, const char* s) {
  fputc('"', f);
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') fputc('\\', f);
    if ((uint8_t)*s < 0x20) continue;
    fputc(*s, f);
  }
  fputc('"', f);
}

static const char* TEAM[2] = {"white", "blue"};
static const char* PERIOD[4] = {"first", "second", "overtime", "shootout"};


// Guarda el partido que acaba de terminar. Devuelve false si no se pudo escribir.
static bool save(const mfv3::Engine& e, const Match& m) {
  using mfv3::Period;
  using mfv3::Team;
  char id[37];
  newId(id);
  char path[112];
  snprintf(path, sizeof(path), "%s/%s.json", dir, id);
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  setvbuf(f, nullptr, _IOFBF, 4096);
  const mfv3::Config& c = m.cfg;
  uint64_t t0 = (uint64_t)m.startEpoch * 1000u, t1 = (uint64_t)m.endEpoch * 1000u;
  const char* mode = m.mode == 'c' ? "chaos" : m.mode == 'r' ? "ranked" : "quick";
  const char* cond = c.endCondition == mfv3::EndCondition::Time ? "time" : c.endCondition == mfv3::EndCondition::Both ? "both" : "goals";
  fprintf(f, "{\"formatVersion\":1,\"id\":\"%s\",\"engineVersion\":\"placa-%lu\",\"rulesVersion\":\"reglas-partido-2\",", id,
          (unsigned long)m.buildId);
  fprintf(f,
          "\"config\":{\"mode\":\"%s\",\"endCondition\":\"%s\",\"goalsPerPeriod\":%u,\"minutesPerPeriod\":%u,"
          "\"overtimeSeconds\":%u,\"penaltyRounds\":%u,\"penaltyFirstTeam\":\"%s\",\"testMode\":false},",
          mode, cond, c.goalsPerPeriod, c.minutesPerPeriod, c.overtimeSeconds, c.penaltyRounds, TEAM[c.penaltyFirst == Team::Blue]);
  fputs("\"participants\":[", f);
  bool first = true;
  for (int t = 0; t < 2; t++)
    for (int i = 0; i < m.n[t]; i++) {
      fprintf(f, "%s{\"playerId\":", first ? "" : ",");
      jstr(f, m.who[t][i].id);
      fprintf(f, ",\"team\":\"%s\",\"slot\":%d,\"nameSnapshot\":", TEAM[t], i + 1);
      jstr(f, m.who[t][i].name);
      fputc('}', f);
      first = false;
    }
  fprintf(f, "],\"createdAt\":%llu,\"startedAt\":%llu,\"finishedAt\":%llu,", (unsigned long long)t0, (unsigned long long)t0,
          (unsigned long long)t1);
  // Resultado
  mfv3::Score s = e.score(), ps = e.penaltyScore();
  const char* reason = e.reason() == mfv3::Reason::GoldenGoal ? "golden_goal" : e.reason() == mfv3::Reason::Penalties ? "penalties" : "regulation";
  bool pens = e.kickCount() > 0;
  fprintf(f, "\"result\":{\"winner\":\"%s\",\"reason\":\"%s\",\"score\":{\"white\":%u,\"blue\":%u},", TEAM[e.winner() == Team::Blue],
          reason, s.white, s.blue);
  if (pens) fprintf(f, "\"penaltyScore\":{\"white\":%u,\"blue\":%u},", ps.white, ps.blue);
  fprintf(f, "\"totalTimeMs\":%lu},", (unsigned long)e.totalTimeMs());
  // Partes jugadas
  bool played[3] = {true, false, false};
  played[1] = e.periodDuration(Period::Second) > 0 || c.endCondition != mfv3::EndCondition::Goals;
  played[2] = e.periodDuration(Period::Overtime) > 0 || e.reason() == mfv3::Reason::GoldenGoal || pens;
  if (c.endCondition == mfv3::EndCondition::Goals) played[1] = false;
  fputs("\"periods\":[", f);
  uint32_t before[3] = {0, 0, 0};
  uint32_t acc = 0;
  for (int p = 0; p < 3; p++) {
    before[p] = acc;
    if (!played[p]) continue;
    mfv3::Score sc = e.periodScore((Period)p);
    uint32_t d = e.periodDuration((Period)p);
    acc += d;
    const char* end = p == 2 ? (e.reason() == mfv3::Reason::GoldenGoal ? "golden_goal" : "time")
                             : (c.endCondition != mfv3::EndCondition::Time && (sc.white >= c.goalsPerPeriod || sc.blue >= c.goalsPerPeriod) ? "goals" : "time");
    fprintf(f, "%s{\"period\":\"%s\",\"score\":{\"white\":%u,\"blue\":%u},\"durationMs\":%lu,\"endReason\":\"%s\"}", p ? "," : "",
            PERIOD[p], sc.white, sc.blue, (unsigned long)d, end);
  }
  fputs("],\"events\":[", f);
  // Eventos: inicio, partes, goles (y la anulación justo después), visitas, penaltis y final.
  int seq = 0;
  int sw = 0, sb = 0;
  auto ev = [&](const char* type, int team, int period, uint32_t ptime, const char* extra) {
    seq++;
    uint32_t total = before[period < 3 ? period : 2] + ptime;
    fprintf(f, "%s{\"id\":\"e%d\",\"seq\":%d,\"type\":\"%s\",", seq > 1 ? "," : "", seq, seq, type);
    if (team >= 0) fprintf(f, "\"team\":\"%s\",", TEAM[team]);
    fprintf(f, "\"period\":\"%s\",\"periodTimeMs\":%lu,\"totalTimeMs\":%lu,\"wallTime\":%llu,\"scoreAfter\":{\"white\":%d,\"blue\":%d}%s}",
            PERIOD[period & 3], (unsigned long)ptime, (unsigned long)total, (unsigned long long)(t0 + total), sw, sb, extra ? extra : "");
  };
  ev("MATCH_START", -1, 0, 0, nullptr);
  ev("PERIOD_START", -1, 0, 0, nullptr);
  int lastPeriod = 0;
  uint8_t vm = 0;
  auto visitsUpTo = [&](int period) {  // las travesuras de los animales, al final de su parte
    while (vm < e.visitMarkCount() && (int)e.visitMark(vm).period <= period) {
      const mfv3::VisitMark& v = e.visitMark(vm++);
      char x[160];
      const char* an = v.v.animal == mfv3::Animal::Squirrel ? "squirrel" : v.v.animal == mfv3::Animal::Snail ? "snail" : "cat";
      int n = snprintf(x, sizeof(x), ",\"visitor\":{\"id\":\"v%u\",\"animal\":\"%s\"", vm, an);
      if (v.v.animal == mfv3::Animal::Squirrel && v.v.from >= 0) {
        n += snprintf(x + n, sizeof(x) - n, ",\"from\":\"%s\"", TEAM[v.v.from & 1]);
        int& a = v.v.from ? sb : sw;
        int& b = v.v.from ? sw : sb;
        if (a > 0) a--, b++;
      } else if (v.v.animal == mfv3::Animal::Cat && v.v.to >= 0) {
        n += snprintf(x + n, sizeof(x) - n, ",\"to\":\"%s\"", TEAM[v.v.to & 1]);
        (v.v.to ? sb : sw)++;
      } else if (v.v.animal == mfv3::Animal::Snail) {
        n += snprintf(x + n, sizeof(x) - n, v.v.time ? ",\"timeMs\":60000" : ",\"goals\":1");
      }
      snprintf(x + n, sizeof(x) - n, "}");
      ev("VISIT", -1, (int)v.period, e.periodDuration(v.period), x);
    }
  };
  auto closeTo = [&](int p) {  // cambia de parte: fin de la anterior e inicio de la siguiente
    while (lastPeriod < p) {
      visitsUpTo(lastPeriod);
      ev("PERIOD_END", -1, lastPeriod, e.periodDuration((Period)lastPeriod), nullptr);
      lastPeriod++;
      if (lastPeriod == 2) ev("OVERTIME_START", -1, 2, 0, nullptr);
      else ev("PERIOD_START", -1, lastPeriod, 0, nullptr);
    }
  };
  static int goalSeq[mfv3::Engine::MAX_GOALS];
  for (uint8_t i = 0; i < e.goalCount(); i++) {
    const mfv3::Goal& g = e.goalAt(i);
    goalSeq[i] = 0;
    int p = (int)g.period;
    if (p > 2) continue;
    closeTo(p);
    int t = g.team == Team::Blue;
    int v = g.value;
    (t ? sb : sw) += v;
    if (g.steal) {
      int& o = t ? sw : sb;
      if (o > 0) o--;
    }
    char x[120];
    int n = snprintf(x, sizeof(x), ",\"value\":%d", v);
    const char* bonus = g.bonus == mfv3::Bonus::Double ? "double" : g.bonus == mfv3::Bonus::Triple ? "triple" : g.bonus == mfv3::Bonus::Frozen ? "frozen"
                        : g.bonus == mfv3::Bonus::Penalty ? "penalty" : g.bonus == mfv3::Bonus::Steal ? "steal" : nullptr;
    if (bonus) n += snprintf(x + n, sizeof(x) - n, ",\"bonus\":[\"%s\"]", bonus);
    if (g.steal) snprintf(x + n, sizeof(x) - n, ",\"steal\":true");
    ev("GOAL", t, p, g.periodTimeMs, x);
    goalSeq[i] = seq;
    if (g.annulled) {
      (t ? sb : sw) -= v;
      if (g.steal) (t ? sw : sb)++;
      char r[32];
      snprintf(r, sizeof(r), ",\"refEventId\":\"e%d\"", seq);
      ev("CORRECTION", t, p, g.periodTimeMs, r);
    }
  }
  int lastPlayed = played[2] ? 2 : played[1] ? 1 : 0;
  closeTo(lastPlayed);
  visitsUpTo(lastPlayed);
  ev("PERIOD_END", -1, lastPlayed, e.periodDuration((Period)lastPlayed), nullptr);
  // Penaltis
  int firstKick = seq + 2;  // tras SHOOTOUT_START
  if (pens) {
    ev("SHOOTOUT_START", -1, 3, 0, nullptr);
    for (uint8_t i = 0; i < e.kickCount(); i++) {
      const mfv3::Kick& k = e.kick(i);
      char x[24];
      snprintf(x, sizeof(x), ",\"scored\":%s", k.scored ? "true" : "false");
      ev("PENALTY", k.team == Team::Blue, 3, 0, x);
    }
  }
  ev("MATCH_END", -1, pens ? 3 : lastPlayed, pens ? 0 : e.periodDuration((Period)lastPlayed), nullptr);
  fputs("],\"penalties\":[", f);
  int att[2] = {0, 0};
  for (uint8_t i = 0; pens && i < e.kickCount(); i++) {
    const mfv3::Kick& k = e.kick(i);
    int t = k.team == Team::Blue;
    att[t]++;
    fprintf(f, "%s{\"order\":%u,\"team\":\"%s\",\"scored\":%s,\"attempt\":%d,\"suddenDeath\":%s,\"eventId\":\"e%d\"}", i ? "," : "", i + 1,
            TEAM[t], k.scored ? "true" : "false", att[t], att[t] > c.penaltyRounds ? "true" : "false", firstKick + i);
  }
  fputs("]", f);
  // Goleadores: si un equipo juega con un solo jugador, sus goles son suyos (autoScorers de la app).
  if (m.n[0] == 1 || m.n[1] == 1) {
    fputs(",\"scorers\":{", f);
    bool firstS = true;
    for (uint8_t i = 0; i < e.goalCount(); i++) {
      int t = e.goalAt(i).team == Team::Blue;
      if (!goalSeq[i] || m.n[t] != 1) continue;
      fprintf(f, "%s\"e%d\":", firstS ? "" : ",", goalSeq[i]);
      jstr(f, m.who[t][0].id);
      firstS = false;
    }
    fputc('}', f);
  }
  fputs("}\n", f);
  bool ok = fclose(f) == 0;
  if (!ok) return false;
  // Índice: la fila sale del JSON recién escrito (igual que con los partidos que lleguen de la app).
  Row r;
  if (!rowFromJson(path, r)) return false;
  appendIndex(r);
  push(r);
  return true;
}


// ---------------------------------------------------------------- hora local (para enseñar las fechas)
static reloj::Clock localOf(uint32_t utc) {
  // Primero con +1 (invierno); si esa hora cae en verano, +2.
  uint32_t m = (utc + 3600u - 946684800u) / 60u + 1;
  reloj::Clock c = reloj::fromStamp(m);
  c.second = (uint8_t)((utc) % 60);
  if (summer(c)) {
    c = reloj::fromStamp(m + 60);
    c.second = (uint8_t)(utc % 60);
  }
  return c;
}
// «18/10/26, 17:31» como la app (formatDate).
static void dateText(uint32_t utc, char* out, size_t n) {
  if (!utc) {
    snprintf(out, n, "Sin fecha");
    return;
  }
  reloj::Clock c = localOf(utc);
  snprintf(out, n, "%02u/%02u/%02u, %02u:%02u", c.day, c.month, c.year % 100, c.hour, c.minute);
}

// ---------------------------------------------------------------- detalle (lee el JSON del partido)
struct GoalLine {
  uint8_t team;    // 0 Blanco, 1 Azul
  uint8_t period;  // 0 1ª, 1 2ª, 2 prórroga
  uint32_t ms;     // tiempo dentro de la parte
  uint8_t value;
  bool off;        // anulado
  uint8_t w, b;    // marcador después
  char id[12];
};
struct Detail {
  uint32_t totalMs = 0;
  bool singlePeriod = true;
  uint8_t goals = 0;
  GoalLine goal[120];
};

// Valor de una clave dentro de [s, e): número o texto entre comillas. false si no está.
static bool field(const char* s, const char* e, const char* key, char* out, size_t n) {
  char k[40];
  snprintf(k, sizeof(k), "\"%s\":", key);
  size_t kl = strlen(k);
  for (const char* p = s; p + kl < e; p++)
    if (!memcmp(p, k, kl)) {
      p += kl;
      size_t i = 0;
      if (*p == '"') {
        p++;
        while (p < e && *p != '"' && i < n - 1) out[i++] = *p++;
      } else {
        while (p < e && *p != ',' && *p != '}' && i < n - 1) out[i++] = *p++;
      }
      out[i] = 0;
      return true;
    }
  return false;
}

static bool loadDetail(const char* id, Detail& d) {
  d = Detail();
  char path[112];
  jsonPath(id, path, sizeof(path));
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* t = n > 0 && n < 4000000 ? (char*)malloc((size_t)n + 1) : nullptr;
  bool ok = t && fread(t, 1, (size_t)n, f) == (size_t)n;
  fclose(f);
  if (!ok) {
    free(t);
    return false;
  }
  t[n] = 0;
  char v[48];
  const char* res = strstr(t, "\"result\":");
  if (res && field(res, t + n, "totalTimeMs", v, sizeof(v))) d.totalMs = (uint32_t)strtoul(v, nullptr, 10);
  d.singlePeriod = !strstr(t, "\"period\":\"second\"") && strstr(t, "\"endCondition\":\"goals\"");
  const char* ev = strstr(t, "\"events\":[");
  const char* p = ev ? ev + 10 : nullptr;
  while (p && *p && *p != ']') {
    const char* o = strchr(p, '{');
    if (!o) break;
    int depth = 0;
    const char* q = o;
    for (; *q; q++) {
      if (*q == '{') depth++;
      else if (*q == '}' && --depth == 0) break;
    }
    if (!*q) break;
    char type[20] = "";
    field(o, q, "type", type, sizeof(type));
    if (!strcmp(type, "GOAL") && d.goals < 120) {
      GoalLine& g = d.goal[d.goals++];
      memset(&g, 0, sizeof(g));
      field(o, q, "id", g.id, sizeof(g.id));
      if (field(o, q, "team", v, sizeof(v))) g.team = !strcmp(v, "blue");
      if (field(o, q, "period", v, sizeof(v))) g.period = !strcmp(v, "second") ? 1 : !strcmp(v, "overtime") ? 2 : 0;
      if (field(o, q, "periodTimeMs", v, sizeof(v))) g.ms = (uint32_t)strtoul(v, nullptr, 10);
      g.value = field(o, q, "value", v, sizeof(v)) ? (uint8_t)atoi(v) : 1;
      const char* sa = strstr(o, "\"scoreAfter\":");
      if (sa && sa < q) {
        if (field(sa, q, "white", v, sizeof(v))) g.w = (uint8_t)atoi(v);
        if (field(sa, q, "blue", v, sizeof(v))) g.b = (uint8_t)atoi(v);
      }
    } else if (!strcmp(type, "CORRECTION") && field(o, q, "refEventId", v, sizeof(v))) {
      for (uint8_t i = 0; i < d.goals; i++)
        if (!strcmp(d.goal[i].id, v)) d.goal[i].off = true;
    }
    p = q + 1;
    while (*p == ',' || *p == ' ' || *p == '\n') p++;
  }
  free(t);
  return true;
}

// Borra un partido (su JSON y su línea del índice).
static bool remove(const char* id) {
  int k = -1;
  for (int i = 0; i < count; i++)
    if (!strcmp(rows[i].id, id)) k = i;
  if (k < 0) return false;
  char path[112];
  jsonPath(id, path, sizeof(path));
  ::remove(path);
  char ip[96], tmp[100];
  indexPath(ip, sizeof(ip));
  snprintf(tmp, sizeof(tmp), "%s.tmp", ip);
  FILE* in = fopen(ip, "rb");
  FILE* out = fopen(tmp, "wb");
  if (!in || !out) {
    if (in) fclose(in);
    if (out) fclose(out);
    return false;
  }
  char line[640];
  size_t idl = strlen(id);
  while (fgets(line, sizeof(line), in))
    if (strncmp(line, id, idl) || line[idl] != '|') fputs(line, out);
  fclose(in);
  fclose(out);
  ::remove(ip);
  rename(tmp, ip);
  memmove(&rows[k], &rows[k + 1], (size_t)(count - k - 1) * sizeof(Row));
  count--;
  return true;
}

}  // namespace partidos

#endif  // MFV3_S3_PARTIDOS_H
