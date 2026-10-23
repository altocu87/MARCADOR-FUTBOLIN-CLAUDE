// MARCADOR FUTBOLÍN V3 · pantalla de 7": AJUSTES → DATOS (modo prueba y copias de seguridad), como la app.
// -------------------------------------------------------------------------------------------------------
// Los datos de la mesa están en tres carpetas: los jugadores (con sus fotos, los equipos fijos y los tipos de
// torneo) en la memoria del chip, «/jugadores» (FFat), y los partidos y los torneos en la tarjeta SD (partidos::dir y
// torneos::guardado::dir, una al lado de la otra).
//
// MODO PRUEBA: las carpetas reales se apartan con otro nombre («….real») y se crean otras con jugadores, partidos y
// torneos ficticios, jugados de verdad con el motor. Todo lo que se juegue o se añada va ahí. Mientras dura, la
// nube no sincroniza (no sube nada de prueba a la cuenta). Al quitarlo, se borra lo de prueba y vuelven las
// carpetas reales tal cual estaban. Si se va la luz a medias, al encender se deja todo como estaba antes (los
// torneos se apartan los primeros y vuelven los últimos: si solo ellos están apartados, vuelven).
//
// COPIA DE SEGURIDAD: una carpeta en la SD («copias/<fecha>») con los jugadores, los partidos, los torneos y los
// ajustes de la mesa (sin la clave del Wi-Fi). Restaurar copia en carpetas «….new» y solo al final las cambia por las
// de ahora, así que un corte nunca deja la mesa a medias. Una copia de antes de los torneos deja la mesa sin torneos
// (como estaba entonces).
//
// Son tareas de unos segundos y se hacen seguidas, con la barra de progreso al día (como al actualizar).
// Se incluye A MITAD del sketch (usa jugadores, partidos, prefs, el motor y la nube).

#ifndef MFV3_S3_DATOS_H
#define MFV3_S3_DATOS_H

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>  // rmdir
#include "s3_torneos_datos.h"

static void nubeLoggedIn();

namespace datos {

static fs::FS* chip = nullptr;  // FFat: la carpeta de los jugadores
static const char* const PLAYERS = "/jugadores";
static bool demo = false;       // modo prueba activo
typedef void (*Progress)(const char* what, int done, int total);
static Progress progress = nullptr;

// ---------------------------------------------------------------- carpetas (chip: fs::FS; SD: stdio)
static bool stdioExists(const char* p) {
  struct stat st;
  return stat(p, &st) == 0;
}
static bool stdioMkdir(const char* p) {
#ifdef _WIN32
  return mkdir(p) == 0 || stdioExists(p);
#else
  return mkdir(p, 0777) == 0 || stdioExists(p);
#endif
}

// Crea la carpeta y las que le falten por encima.
static bool stdioMkdirs(const char* p) {
  char b[160];
  snprintf(b, sizeof(b), "%s", p);
  for (char* q = b + 1; *q; q++)
    if (*q == '/') {
      *q = 0;
      stdioMkdir(b);
      *q = '/';
    }
  return stdioMkdir(b);
}

// Nombres de lo que hay en una carpeta (en la PSRAM). Devuelve cuántos; isDir[i] dice si es carpeta.
struct Names {
  char (*name)[64] = nullptr;
  bool* isDir = nullptr;
  int n = 0, cap = 0;
  ~Names() { free(name); free(isDir); }
  bool add(const char* s, bool d) {
    if (n == cap) {
      int nc = cap ? cap * 2 : 64;
      auto* nn = (char(*)[64])realloc(name, (size_t)nc * 64);
      if (!nn) return false;
      name = nn;
      bool* nd = (bool*)realloc(isDir, (size_t)nc);
      if (!nd) return false;
      isDir = nd;
      cap = nc;
    }
    snprintf(name[n], 64, "%s", s);
    isDir[n++] = d;
    return true;
  }
};
static bool listFs(fs::FS& fs, const char* dir, Names& out) {
  fs::File d = fs.open(dir);
  if (!d || !d.isDirectory()) return false;
  for (fs::File f = d.openNextFile(); f; f = d.openNextFile()) {
    const char* n = f.name();
    const char* slash = strrchr(n, '/');
    if (!out.add(slash ? slash + 1 : n, f.isDirectory())) return false;
  }
  return true;
}
static bool listStdio(const char* dir, Names& out) {
  DIR* d = opendir(dir);
  if (!d) return false;
  bool ok = true;
  while (struct dirent* e = readdir(d)) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
    char p[200];
    snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
    struct stat st;
    bool isDir = stat(p, &st) == 0 && S_ISDIR(st.st_mode);
    if (!out.add(e->d_name, isDir)) { ok = false; break; }
  }
  closedir(d);
  return ok;
}

static bool removeFsTree(fs::FS& fs, const char* dir) {
  if (!fs.exists(dir)) return true;
  Names n;
  listFs(fs, dir, n);
  for (int i = 0; i < n.n; i++) {
    char p[160];
    snprintf(p, sizeof(p), "%s/%s", dir, n.name[i]);
    if (n.isDir[i]) removeFsTree(fs, p);
    else fs.remove(p);
  }
  return fs.rmdir(dir) || !fs.exists(dir);
}
static bool removeStdioTree(const char* dir) {
  if (!stdioExists(dir)) return true;
  Names n;
  listStdio(dir, n);
  for (int i = 0; i < n.n; i++) {
    char p[200];
    snprintf(p, sizeof(p), "%s/%s", dir, n.name[i]);
    if (n.isDir[i]) removeStdioTree(p);
    else ::remove(p);
  }
  return rmdir(dir) == 0 || !stdioExists(dir);
}

static uint8_t* buf = nullptr;  // 4 KB para copiar (en la PSRAM, se reserva al hacer falta)
static const size_t BUF = 4096;
static bool haveBuf() { return buf || (buf = (uint8_t*)malloc(BUF)) != nullptr; }
static int copied = 0, toCopy = 0;
static const char* copyWhat = "";
static void step() {
  copied++;
  if (progress) progress(copyWhat, copied, toCopy);
  delay(1);  // que respiren las demás tareas
}

// Solo archivos sueltos (las carpetas de datos no tienen subcarpetas).
static int countFs(fs::FS& fs, const char* dir) {
  Names n;
  listFs(fs, dir, n);
  int k = 0;
  for (int i = 0; i < n.n; i++) k += !n.isDir[i];
  return k;
}
static int countStdio(const char* dir) {
  Names n;
  listStdio(dir, n);
  int k = 0;
  for (int i = 0; i < n.n; i++) k += !n.isDir[i];
  return k;
}
static bool copyFsToStdio(fs::FS& fs, const char* from, const char* to) {
  if (!haveBuf() || !stdioMkdir(to)) return false;
  Names n;
  if (!listFs(fs, from, n)) return true;  // sin carpeta: nada que copiar
  for (int i = 0; i < n.n; i++) {
    if (n.isDir[i]) continue;
    char a[160], b[200];
    snprintf(a, sizeof(a), "%s/%s", from, n.name[i]);
    snprintf(b, sizeof(b), "%s/%s", to, n.name[i]);
    fs::File in = fs.open(a, FILE_READ);
    FILE* out = fopen(b, "wb");
    bool ok = in && out;
    while (ok) {
      size_t k = in.read(buf, BUF);
      if (!k) break;
      ok = fwrite(buf, 1, k, out) == k;
    }
    if (in) in.close();
    if (out) ok = fclose(out) == 0 && ok;
    if (!ok) return false;
    step();
  }
  return true;
}
static bool copyStdioToFs(const char* from, fs::FS& fs, const char* to) {
  if (!haveBuf()) return false;
  fs.mkdir(to);
  Names n;
  if (!listStdio(from, n)) return true;
  for (int i = 0; i < n.n; i++) {
    if (n.isDir[i]) continue;
    char a[200], b[160];
    snprintf(a, sizeof(a), "%s/%s", from, n.name[i]);
    snprintf(b, sizeof(b), "%s/%s", to, n.name[i]);
    FILE* in = fopen(a, "rb");
    fs::File out = fs.open(b, FILE_WRITE);
    bool ok = in && out;
    while (ok) {
      size_t k = fread(buf, 1, BUF, in);
      if (!k) break;
      ok = out.write(buf, k) == k;
    }
    if (in) fclose(in);
    if (out) out.close();
    if (!ok) return false;
    step();
  }
  return true;
}
static bool copyStdio(const char* from, const char* to) {
  if (!haveBuf() || !stdioMkdir(to)) return false;
  Names n;
  if (!listStdio(from, n)) return true;
  for (int i = 0; i < n.n; i++) {
    if (n.isDir[i]) continue;
    char a[200], b[200];
    snprintf(a, sizeof(a), "%s/%s", from, n.name[i]);
    snprintf(b, sizeof(b), "%s/%s", to, n.name[i]);
    FILE* in = fopen(a, "rb");
    FILE* out = fopen(b, "wb");
    bool ok = in && out;
    while (ok) {
      size_t k = fread(buf, 1, BUF, in);
      if (!k) break;
      ok = fwrite(buf, 1, k, out) == k;
    }
    if (in) fclose(in);
    if (out) ok = fclose(out) == 0 && ok;
    if (!ok) return false;
    step();
  }
  return true;
}

// Carpeta de los partidos con un final («.real», «.new», «.old»).
static void matchesWith(const char* suffix, char* out, size_t n) { snprintf(out, n, "%s%s", partidos::dir, suffix); }
static void tournamentsWith(const char* suffix, char* out, size_t n) { snprintf(out, n, "%s%s", torneos::guardado::dir, suffix); }
static void playersWith(const char* suffix, char* out, size_t n) { snprintf(out, n, "%s%s", PLAYERS, suffix); }

// Las copias, junto a la carpeta de los partidos («/sdcard/copias»). Sin tarjeta, no hay copias.
static bool backupRoot(char* out, size_t n) {
  if (!strncmp(partidos::dir, "/ffat", 5)) return false;
  const char* slash = strrchr(partidos::dir, '/');
  if (!slash) return false;
  snprintf(out, n, "%.*s/copias", (int)(slash - partidos::dir), partidos::dir);
  return true;
}

// ---------------------------------------------------------------- volver a leer los datos
static void reload() {
  if (chip) jugadores.begin(chip);
  equipos::load();
  bool sd = !strncmp(partidos::dir, "/sdcard", 7);
  partidos::begin(sd);
  torneos::begin(sd);  // después de los partidos (sus resúmenes y la progresión los usan)
  jugadores.changed = true;
  nube::paused = demo;
}

// ---------------------------------------------------------------- al encender: nada a medias
// Restaurar sin terminar: «.new» se borra (o se pone si faltaba la de verdad) y «.old» vuelve si hace falta.
// Modo prueba a medio poner o quitar: si solo una de las dos carpetas reales está apartada, se devuelve.
static void recover() {
  if (!chip) return;
  if (!torneos::guardado::dir[0]) torneos::guardado::pickDir(!strncmp(partidos::dir, "/sdcard", 7));
  char a[96], b[96];
  for (const char* suf : {".new", ".old"}) {
    playersWith(suf, a, sizeof(a));
    if (chip->exists(a)) {
      if (chip->exists(PLAYERS)) removeFsTree(*chip, a);
      else chip->rename(a, PLAYERS);
    }
    matchesWith(suf, b, sizeof(b));
    if (stdioExists(b)) {
      if (stdioExists(partidos::dir)) removeStdioTree(b);
      else rename(b, partidos::dir);
    }
    tournamentsWith(suf, b, sizeof(b));
    if (stdioExists(b)) {
      if (stdioExists(torneos::guardado::dir)) removeStdioTree(b);
      else rename(b, torneos::guardado::dir);
    }
  }
  playersWith(".real", a, sizeof(a));
  matchesWith(".real", b, sizeof(b));
  bool realPlayers = chip->exists(a), realMatches = stdioExists(b);
  if (realPlayers != realMatches) {
    if (realPlayers) {
      removeFsTree(*chip, PLAYERS);
      chip->rename(a, PLAYERS);
    } else {
      removeStdioTree(partidos::dir);
      rename(b, partidos::dir);
    }
    realPlayers = realMatches = false;
  }
  // Torneos apartados sin el modo prueba puesto del todo: vuelven.
  if (!(realPlayers && realMatches)) {
    tournamentsWith(".real", b, sizeof(b));
    if (stdioExists(b)) {
      removeStdioTree(torneos::guardado::dir);
      rename(b, torneos::guardado::dir);
    }
  }
  demo = realPlayers && realMatches;
  reload();
}

// ---------------------------------------------------------------- MODO PRUEBA
// Los mismos nombres que la app (src/services/demo/demoData.ts), con su nivel de juego (0–100).
struct DemoPlayer { const char* name; uint8_t skill; };
static const DemoPlayer DEMO[] = {{"Alex", 74},   {"Vicky", 70},  {"José", 72},  {"María", 68},
                                  {"Lucía", 85},  {"Marcos", 80}, {"Sara", 72},  {"Javi", 68},
                                  {"Elena", 64},  {"Rubén", 60},  {"Paula", 55}, {"Diego", 50}};
static const int DEMO_PLAYERS = sizeof(DEMO) / sizeof(DEMO[0]);
static const int DEMO_MATCHES = 40;

static uint32_t rnd(uint32_t& s) {  // mulberry32, como la app: mismos datos cada vez
  s += 0x6D2B79F5u;
  uint32_t t = s;
  t = (t ^ (t >> 15)) * (t | 1u);
  t ^= t + (t ^ (t >> 7)) * (t | 61u);
  return t ^ (t >> 14);
}

// Un partido ficticio jugado con el motor: goles al ritmo del nivel de cada equipo.
static bool demoMatch(uint32_t& seed, uint32_t endEpoch, const uint32_t* codes, int i) {
  static mfv3::Engine e;
  static partidos::Match m;
  m = partidos::Match();
  uint32_t r = rnd(seed);
  m.mode = r % 4 == 0 ? 'r' : 'q';
  mfv3::Config c;
  c.endCondition = mfv3::EndCondition::Goals;
  c.goalsPerPeriod = (uint8_t)(5 + rnd(seed) % 3);
  m.cfg = c;
  int per = rnd(seed) % 3 == 0 ? 1 : 2;  // 1 contra 1 o 2 contra 2
  int picked[4], n = 0;
  while (n < per * 2) {
    int k = (int)(rnd(seed) % DEMO_PLAYERS), dup = 0;
    for (int j = 0; j < n; j++) dup |= picked[j] == k;
    if (!dup) picked[n++] = k;
  }
  int skill[2] = {0, 0};
  for (int t = 0; t < 2; t++)
    for (int j = 0; j < per; j++) {
      const jug::Player* p = jugadores.find(codes[picked[t * per + j]]);
      if (!p) return false;
      partidos::Who& w = m.who[t][m.n[t]++];
      w.code = p->code;
      jugadores.idOf(p->code, w.id, sizeof(w.id));
      snprintf(w.name, sizeof(w.name), "%s", p->name);
      skill[t] += DEMO[picked[t * per + j]].skill;
    }
  uint32_t now = 1000;
  if (!e.start(c, now)) return false;
  e.skipCountdown(now);
  for (int guard = 0; guard < 60 && e.phase() != mfv3::Phase::Finished; guard++) {
    now += 6000 + rnd(seed) % 20000;
    Team t = (int)(rnd(seed) % (uint32_t)(skill[0] + skill[1])) < skill[0] ? Team::White : Team::Blue;
    e.goal(t, now);
  }
  if (e.phase() != mfv3::Phase::Finished) return false;
  m.endEpoch = endEpoch;
  m.startEpoch = endEpoch - (now - 1000) / 1000;
  m.buildId = (uint32_t)MFV3_BUILD;
  (void)i;
  return partidos::save(e, m);
}

// Torneos de demostración (como los del modo prueba de la app): una liga ya terminada y otra a medias. Se juegan con
// las reglas de los torneos (s3_torneos_reglas.h); sin ellas, el modo prueba se queda sin torneos.
#ifdef MFV3_TORNEOS_REGLAS
static int demoElo(const char*) { return 1200; }
static const char* demoName(const char* id) {
  const jug::Player* p = jugadores.findById(id);
  return p ? p->name : "?";
}
// Juega el cruce `fi` del torneo con el motor (a goles) y apunta el resultado. false si no se pudo.
static bool demoFixture(uint32_t& seed, torneos::Tournament& t, int16_t fi, uint32_t endEpoch) {
  static mfv3::Engine e;
  static partidos::Match m;
  const torneos::Fixture& f = t.fix[fi];
  if (f.white < 0 || f.blue < 0) return false;
  m = partidos::Match();
  m.mode = t.ranked ? 'r' : 'q';
  torneos::Rules r = torneos::fixtureRules(t, f);
  mfv3::Config c;
  c.endCondition = mfv3::EndCondition::Goals;  // a goles: el tiempo de la demostración es de mentira
  c.goalsPerPeriod = r.goals;
  m.cfg = c;
  snprintf(m.tournament, sizeof(m.tournament), "%s", t.id);
  snprintf(m.fixture, sizeof(m.fixture), "%s", f.id);
  int skill[2] = {0, 0};
  for (int s = 0; s < 2; s++) {
    const torneos::Team& tm = t.teams[s ? f.blue : f.white];
    for (uint8_t k = 0; k < tm.n && k < 2; k++) {
      const jug::Player* p = jugadores.findById(tm.player[k]);
      if (!p) return false;
      partidos::Who& w = m.who[s][m.n[s]++];
      w.code = p->code;
      snprintf(w.id, sizeof(w.id), "%s", tm.player[k]);
      snprintf(w.name, sizeof(w.name), "%s", p->name);
      for (int d = 0; d < DEMO_PLAYERS; d++)
        if (!strcmp(DEMO[d].name, p->name)) skill[s] += DEMO[d].skill;
    }
  }
  uint32_t now = 1000;
  if (!e.start(c, now)) return false;
  e.skipCountdown(now);
  for (int guard = 0; guard < 60 && e.phase() != mfv3::Phase::Finished; guard++) {
    now += 6000 + rnd(seed) % 20000;
    Team tm = (int)(rnd(seed) % (uint32_t)(skill[0] + skill[1] + 2)) <= skill[0] ? Team::White : Team::Blue;
    e.goal(tm, now);
  }
  if (e.phase() != mfv3::Phase::Finished) return false;
  m.endEpoch = endEpoch;
  m.startEpoch = endEpoch - (now - 1000) / 1000;
  m.buildId = (uint32_t)MFV3_BUILD;
  if (!partidos::save(e, m)) return false;
  const partidos::Row& row = partidos::rows[partidos::count - 1];
  torneos::Result res;
  memset(&res, 0, sizeof(res));
  snprintf(res.matchId, sizeof(res.matchId), "%s", row.id);
  res.winner = row.winner;
  res.w = row.w;
  res.b = row.b;
  res.startedAt = row.startedAt;
  res.finishedAt = row.finishedAt;
  torneos::recordFixtureResult(t, fi, res);
  return true;
}
static void demoTournaments(uint32_t& seed, uint32_t base, const uint32_t* codes) {
  static const char* const NAMES[2] = {"Liguilla de prueba", "Liguilla de prueba"};
  static const uint8_t PLAYERS_IN[2] = {4, 5};
  static const int PLAY[2] = {99, 4};  // la primera entera; la segunda, cuatro partidos (en juego)
  torneos::Template tpl;
  torneos::newTemplate(tpl, torneos::Format::League, nullptr);
  snprintf(tpl.name, sizeof(tpl.name), "Liguilla de prueba");
  snprintf(tpl.id, sizeof(tpl.id), "tpl_demo-liguilla");
  tpl.ranked = true;
  torneos::Tournament t;
  torneos::Draft* d = (torneos::Draft*)calloc(1, sizeof(torneos::Draft));
  char (*sel)[40] = (char(*)[40])calloc(DEMO_PLAYERS, 40);
  char (*left)[40] = (char(*)[40])calloc(torneos::MAX_ENTRANTS, 40);
  if (!d || !sel || !left || !torneos::alloc(t)) {
    free(d), free(sel), free(left);
    return;
  }
  for (int k = 0; k < 2; k++) {
    uint8_t n = PLAYERS_IN[k];
    for (uint8_t i = 0; i < n; i++) jugadores.idOf(codes[(k * 4 + i) % DEMO_PLAYERS], sel[i], 40);
    char seedText[24], err[64];
    snprintf(seedText, sizeof(seedText), "demo-%d", k);
    torneos::draftFromTemplate(tpl, NAMES[k], sel, n, demoElo, seedText, nullptr, 0, *d, left);
    d->edition = (uint16_t)(k + 1);
    // Las dos ediciones, días antes de hoy; la segunda sigue en juego.
    uint32_t start = base - (uint32_t)(k ? 2 : 12) * 86400u - base % 86400u + 17u * 3600u;
    if (!torneos::createTournament(*d, demoElo, demoName, (uint64_t)start * 1000u, seedText, t, err, sizeof(err))) continue;
    uint32_t at = start;
    for (int played = 0; played < PLAY[k]; played++) {
      int16_t fi = torneos::nextFixture(t);
      if (fi < 0) break;
      at += 600 + rnd(seed) % 600;
      if (!demoFixture(seed, t, fi, at)) break;
    }
    torneos::save(t);
  }
  torneos::release(t);
  free(d), free(sel), free(left);
}
#else
static void demoTournaments(uint32_t&, uint32_t, const uint32_t*) {}  // sin las reglas de los torneos todavía
#endif

static const char* lastError = "";

static bool setDemo(bool on, uint32_t nowUtc) {
  lastError = "";
  if (!chip || on == demo) return on == demo;
  char rp[96], rm[96];
  playersWith(".real", rp, sizeof(rp));
  matchesWith(".real", rm, sizeof(rm));
  if (on) {
    copyWhat = "Preparando el modo prueba";
    copied = 0;
    toCopy = DEMO_PLAYERS + DEMO_MATCHES;
    // Lo de verdad, aparte (y la última partida de Inicio, que vuelve al quitarlo).
    char rt[96];
    tournamentsWith(".real", rt, sizeof(rt));
    if (chip->exists(rp) || stdioExists(rm) || stdioExists(rt)) { lastError = "Quedan datos de un modo prueba anterior"; return false; }
    stdioMkdirs(partidos::dir);  // sin ningún partido aún, la carpeta podría no estar
    stdioMkdirs(torneos::guardado::dir);
    chip->mkdir(PLAYERS);
    // Los torneos, los primeros (si se va la luz antes de apartar lo demás, recover() los devuelve).
    if (rename(torneos::guardado::dir, rt) != 0) { lastError = "No se pudieron apartar los torneos"; return false; }
    if (!chip->rename(PLAYERS, rp)) {
      rename(rt, torneos::guardado::dir);
      lastError = "No se pudieron apartar los jugadores";
      return false;
    }
    if (rename(partidos::dir, rm) != 0) {
      chip->rename(rp, PLAYERS);
      rename(rt, torneos::guardado::dir);
      lastError = "No se pudieron apartar los partidos";
      return false;
    }
    uint8_t hist[sizeof(history)];
    size_t hn = prefs.getBytes("hist2", hist, sizeof(hist));
    prefs.putBytes("hist2r", hist, hn);
    prefs.putULong("playr", matchesPlayed);
    demo = true;
    reload();
    uint32_t codes[DEMO_PLAYERS];
    for (int i = 0; i < DEMO_PLAYERS; i++) {
      codes[i] = jugadores.add(DEMO[i].name, nullptr);
      step();
    }
    // Repartidos por los últimos 40 días, a la hora de jugar (de 17 a 23 h).
    uint32_t base = nowUtc ? nowUtc : 1792000000u, seed = 20261022u;
    for (int i = 0; i < DEMO_MATCHES; i++) {
      uint32_t day = (uint32_t)(DEMO_MATCHES - i) * 86400u;
      uint32_t at = base - day - (base % 86400u) + 15u * 3600u + rnd(seed) % (6u * 3600u);
      if (!demoMatch(seed, at, codes, i)) { lastError = "No se pudieron crear todos los partidos"; break; }
      step();
    }
    demoTournaments(seed, base, codes);
    jugadores.changed = true;
    return true;
  }
  // Quitar: fuera lo de prueba y vuelve lo de verdad (los torneos, los últimos).
  removeFsTree(*chip, PLAYERS);
  removeStdioTree(partidos::dir);
  bool ok = chip->rename(rp, PLAYERS);
  ok = rename(rm, partidos::dir) == 0 && ok;
  char rt[96];
  tournamentsWith(".real", rt, sizeof(rt));
  removeStdioTree(torneos::guardado::dir);
  if (stdioExists(rt)) ok = rename(rt, torneos::guardado::dir) == 0 && ok;
  else stdioMkdirs(torneos::guardado::dir);  // apartado por una versión sin torneos: no había ninguno
  uint8_t hist[sizeof(history)];
  size_t hn = prefs.getBytes("hist2r", hist, sizeof(hist));
  prefs.putBytes("hist2", hist, hn);
  memcpy(history, hist, hn);
  historyCount = (uint8_t)(hn / sizeof(LastResult));
  matchesPlayed = prefs.getULong("playr", matchesPlayed);
  prefs.putULong("played", matchesPlayed);
  demo = false;
  reload();
  if (!ok) lastError = "Revisa los datos: no se pudo devolver todo";
  return ok;
}

// ---------------------------------------------------------------- COPIAS DE SEGURIDAD
struct Copy {
  char name[24];  // la carpeta: «AAAAMMDD-HHMMSS»
  uint16_t players, matches;
};
static Copy copies[8];
static int copyCount = 0;

static void readInfo(const char* dir, Copy& c) {
  char p[200];
  snprintf(p, sizeof(p), "%s/info.txt", dir);
  c.players = c.matches = 0;
  FILE* f = fopen(p, "rb");
  if (!f) return;
  char line[64];
  while (fgets(line, sizeof(line), f)) {
    unsigned v;
    if (sscanf(line, "jugadores=%u", &v) == 1) c.players = (uint16_t)v;
    if (sscanf(line, "partidos=%u", &v) == 1) c.matches = (uint16_t)v;
  }
  fclose(f);
}

// Las 8 más nuevas (el nombre es la fecha: ordenar por nombre es ordenar por fecha).
static void scanCopies() {
  copyCount = 0;
  char root[96];
  if (!backupRoot(root, sizeof(root))) return;
  Names n;
  listStdio(root, n);
  for (int i = 0; i < n.n; i++) {
    if (!n.isDir[i] || strlen(n.name[i]) >= sizeof(copies[0].name)) continue;
    Copy c;
    snprintf(c.name, sizeof(c.name), "%s", n.name[i]);
    int at = copyCount < 8 ? copyCount++ : -1;
    if (at < 0) {  // ya hay 8: entra si es más nueva que la más vieja
      int old = 0;
      for (int j = 1; j < 8; j++) if (strcmp(copies[j].name, copies[old].name) < 0) old = j;
      if (strcmp(c.name, copies[old].name) <= 0) continue;
      at = old;
    }
    copies[at] = c;
  }
  for (int i = 0; i < copyCount; i++)
    for (int j = i + 1; j < copyCount; j++)
      if (strcmp(copies[j].name, copies[i].name) > 0) { Copy t = copies[i]; copies[i] = copies[j]; copies[j] = t; }
  for (int i = 0; i < copyCount; i++) {
    char d[160];
    snprintf(d, sizeof(d), "%s/%s", root, copies[i].name);
    readInfo(d, copies[i]);
  }
}

// «20261022-213005» → «22/10/2026 21:30».
static void copyLabel(const Copy& c, char* out, size_t n) {
  unsigned y, mo, d, h, mi, s, k;
  if (sscanf(c.name, "00000000-%6u", &k) == 1) snprintf(out, n, "Copia %u (sin hora)", k);
  else if (sscanf(c.name, "%4u%2u%2u-%2u%2u%2u", &y, &mo, &d, &h, &mi, &s) == 6) snprintf(out, n, "%02u/%02u/%04u %02u:%02u", d, mo, y, h, mi);
  else snprintf(out, n, "%s", c.name);
}

// Ajustes de la mesa que viajan con la copia (no la red ni la cuenta).
static const char* const PREF_KEYS[] = {"cond", "goals", "mins", "pfirst", "vol", "bri", "slp", "mmod"};

static bool makeCopy(const reloj::Clock* clock) {
  lastError = "";
  char root[96], dir[160], sub[200];
  if (demo) { lastError = "Quita antes el modo prueba"; return false; }
  if (!chip || !backupRoot(root, sizeof(root))) { lastError = "Necesita la tarjeta SD"; return false; }
  stdioMkdirs(root);
  if (clock) snprintf(dir, sizeof(dir), "%s/%04u%02u%02u-%02u%02u%02u", root, clock->year, clock->month, clock->day, clock->hour, clock->minute, clock->second);
  else {
    scanCopies();  // sin hora: «00000000-000001», «…02»… (siguen ordenadas)
    unsigned k = 1;
    for (int i = 0; i < copyCount; i++) {
      unsigned v;
      if (sscanf(copies[i].name, "00000000-%6u", &v) == 1 && v >= k) k = v + 1;
    }
    snprintf(dir, sizeof(dir), "%s/00000000-%06u", root, k);
  }
  if (stdioExists(dir)) { lastError = "Ya hay una copia de este momento"; return false; }
  copyWhat = "Haciendo la copia";
  copied = 0;
  toCopy = countFs(*chip, PLAYERS) + countStdio(partidos::dir) + countStdio(torneos::guardado::dir) + 1;
  bool ok = stdioMkdir(dir);
  snprintf(sub, sizeof(sub), "%s/jugadores", dir);
  ok = ok && copyFsToStdio(*chip, PLAYERS, sub);
  snprintf(sub, sizeof(sub), "%s/partidos", dir);
  ok = ok && copyStdio(partidos::dir, sub);
  snprintf(sub, sizeof(sub), "%s/torneos", dir);
  ok = ok && copyStdio(torneos::guardado::dir, sub);
  snprintf(sub, sizeof(sub), "%s/ajustes.txt", dir);
  if (ok) {
    FILE* f = fopen(sub, "wb");
    ok = f != nullptr;
    for (const char* k : PREF_KEYS)
      if (f && prefs.isKey(k)) fprintf(f, "%s=%u\n", k, (unsigned)prefs.getUChar(k, 0));
    char snd[mfv3snd::EVENT_COUNT * 64 + 1] = {0};
    size_t n = prefs.getBytes("snd", snd, sizeof(snd) - 1);
    snd[n] = 0;
    if (f) fprintf(f, "snd=%s\n", snd);
    if (f) ok = fclose(f) == 0 && ok;
  }
  // info.txt al final: una copia sin él es una copia a medias.
  snprintf(sub, sizeof(sub), "%s/info.txt", dir);
  if (ok) {
    FILE* f = fopen(sub, "wb");
    ok = f && fprintf(f, "jugadores=%u\npartidos=%d\ntorneos=%u\nversion=%s\n", (unsigned)jugadores.count, partidos::count,
                      (unsigned)torneos::nSummaries, FW_VERSION) > 0;
    if (f) ok = fclose(f) == 0 && ok;
  }
  step();
  if (!ok) {
    removeStdioTree(dir);
    lastError = "No se pudo escribir en la tarjeta";
  }
  scanCopies();
  return ok;
}

static bool copyDir(int i, char* out, size_t n) {
  char root[96];
  if (i < 0 || i >= copyCount || !backupRoot(root, sizeof(root))) return false;
  snprintf(out, n, "%s/%s", root, copies[i].name);
  return true;
}

static bool deleteCopy(int i) {
  char dir[160];
  if (!copyDir(i, dir, sizeof(dir))) return false;
  bool ok = removeStdioTree(dir);
  scanCopies();
  return ok;
}

static void applyPrefs(const char* file) {
  FILE* f = fopen(file, "rb");
  if (!f) return;
  static char line[mfv3snd::EVENT_COUNT * 64 + 16];
  while (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\r\n")] = 0;
    char* eq = strchr(line, '=');
    if (!eq) continue;
    *eq = 0;
    if (!strcmp(line, "snd")) {
      prefs.putBytes("snd", eq + 1, strlen(eq + 1));
      continue;
    }
    for (const char* k : PREF_KEYS)
      if (!strcmp(line, k)) prefs.putUChar(k, (uint8_t)atoi(eq + 1));
  }
  fclose(f);
}

static bool restoreCopy(int i) {
  lastError = "";
  char dir[160], sub[200], pn[96], po[96], mn[96], mo[96], tn[96], to[96];
  if (demo) { lastError = "Quita antes el modo prueba"; return false; }
  if (!chip || !copyDir(i, dir, sizeof(dir))) { lastError = "No se encuentra la copia"; return false; }
  snprintf(sub, sizeof(sub), "%s/info.txt", dir);
  if (!stdioExists(sub)) { lastError = "Esa copia está incompleta"; return false; }
  playersWith(".new", pn, sizeof(pn));
  playersWith(".old", po, sizeof(po));
  matchesWith(".new", mn, sizeof(mn));
  matchesWith(".old", mo, sizeof(mo));
  tournamentsWith(".new", tn, sizeof(tn));
  tournamentsWith(".old", to, sizeof(to));
  removeFsTree(*chip, pn);
  removeStdioTree(mn);
  removeStdioTree(tn);
  char sp[200], sm[200], st[200];
  snprintf(sp, sizeof(sp), "%s/jugadores", dir);
  snprintf(sm, sizeof(sm), "%s/partidos", dir);
  snprintf(st, sizeof(st), "%s/torneos", dir);  // una copia de antes de los torneos no la tiene: sin torneos
  copyWhat = "Restaurando la copia";
  copied = 0;
  toCopy = countStdio(sp) + countStdio(sm) + countStdio(st) + 1;
  bool ok = copyStdioToFs(sp, *chip, pn) && copyStdio(sm, mn) && copyStdio(st, tn);
  if (!ok) {
    removeFsTree(*chip, pn);
    removeStdioTree(mn);
    removeStdioTree(tn);
    lastError = "No se pudo copiar: todo sigue como estaba";
    return false;
  }
  // El cambio: las de ahora se apartan, entran las de la copia y se borran las apartadas.
  removeFsTree(*chip, po);
  removeStdioTree(mo);
  removeStdioTree(to);
  stdioMkdirs(torneos::guardado::dir);
  ok = chip->rename(PLAYERS, po) && chip->rename(pn, PLAYERS);
  ok = rename(partidos::dir, mo) == 0 && rename(mn, partidos::dir) == 0 && ok;
  ok = rename(torneos::guardado::dir, to) == 0 && rename(tn, torneos::guardado::dir) == 0 && ok;
  if (ok) {
    removeFsTree(*chip, po);
    removeStdioTree(mo);
    removeStdioTree(to);
  }
  snprintf(sub, sizeof(sub), "%s/ajustes.txt", dir);
  applyPrefs(sub);
  step();
  recover();  // por si algo no se pudo cambiar: se deja coherente
  if (nube::loggedIn()) nubeLoggedIn(), torneos::queueAll();  // lo restaurado, también a la cuenta
  if (!ok) lastError = "Restaurada a medias: revisa los datos";
  return ok;
}

}  // namespace datos

#endif  // MFV3_S3_DATOS_H
