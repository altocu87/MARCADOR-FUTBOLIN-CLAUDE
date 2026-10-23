// MARCADOR FUTBOLÍN V3 · el QR del perfil de un jugador, como en la app (src/services/profiles).
// -------------------------------------------------------------------------------------------------
// Cada jugador tiene un código (va en el QR) y una clave que solo conoce esta placa («/jugadores/perfiles.txt»).
// Al enseñar el QR, la placa calcula su perfil con las mismas cuentas que la app (snapshot.ts: partidos, rachas,
// rivales, compañeros, hábitos, nivel, ELO y logros) y lo publica en internet (RPC publicar_perfil_jugador, con
// la clave pública: no hace falta cuenta). El móvil, al leer el QR, abre «…/perfil/#código» y lo ve.
// Lo calcula loop() (son los datos de la placa); lo envía la tarea de la nube (s3_nube.h, trabajo PROFILE).
// Palmarés (playerTournaments de report.ts): los torneos no descartados donde juega, del más reciente al más viejo,
// con el puesto en que quedó si ha terminado. Cada partido de torneo lleva el nombre y el tipo de su torneo.

#ifndef MFV3_S3_PERFIL_H
#define MFV3_S3_PERFIL_H

namespace perfil {

static const char* const WEB = "https://altocu87.github.io/MARCADOR-FUTBOLIN-CLAUDE/perfil/#";
static const char* const CREDS = "/jugadores/perfiles.txt";
static const char* const ALPHABET = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
static fs::FS* fs_ = nullptr;

struct Creds {
  uint32_t code;
  char token[24];
  char clave[34];
};

static void randomCode(char* out, int n) {
  for (int k = 0; k < n; k++) {
#ifdef ARDUINO
    uint32_t r = esp_random();
#else
    uint32_t r = (uint32_t)rand();
#endif
    out[k] = ALPHABET[r % 56];
  }
  out[n] = 0;
}

// Código y clave del jugador; se crean (y se guardan) la primera vez.
static bool credsFor(uint32_t code, Creds& c) {
  if (!fs_) return false;
  if (fs::File f = fs_->open(CREDS, FILE_READ)) {
    char line[80];
    size_t n = 0;
    bool found = false;
    for (int ch = f.read();; ch = f.read()) {
      if (ch < 0 || ch == '\n') {
        line[n] = 0;
        n = 0;
        unsigned long k;
        char t[24], cl[34];
        if (sscanf(line, "%lx %23s %33s", &k, t, cl) == 3 && (uint32_t)k == code) {
          c.code = code;
          snprintf(c.token, sizeof(c.token), "%s", t);
          snprintf(c.clave, sizeof(c.clave), "%s", cl);
          found = true;
        }
        if (ch < 0 || found) break;
      } else if (n < sizeof(line) - 1) line[n++] = (char)ch;
    }
    f.close();
    if (found) return true;
  }
  c.code = code;
  randomCode(c.token, 22);
  randomCode(c.clave, 32);
  // Se añade al final (los anteriores no se tocan).
  std::string all;
  if (fs::File f = fs_->open(CREDS, FILE_READ)) {
    for (int ch = f.read(); ch >= 0; ch = f.read()) all += (char)ch;
    f.close();
  }
  char line[80];
  snprintf(line, sizeof(line), "%08lx %s %s\n", (unsigned long)code, c.token, c.clave);
  all += line;
  fs::File w = fs_->open(CREDS, FILE_WRITE);
  if (!w) return false;
  bool ok = w.write((const uint8_t*)all.data(), all.size()) == all.size();
  w.close();
  return ok;
}

static void url(const Creds& c, char* out, size_t n) { snprintf(out, n, "%s%s", WEB, c.token); }

// ---------------------------------------------------------------- texto JSON que crece
struct Out {
  char* s = nullptr;
  size_t n = 0, cap = 0;
  bool ok = true;
  ~Out() { free(s); }
  void add(const char* fmt, ...) {
    if (!ok) return;
    for (;;) {
      va_list a;
      va_start(a, fmt);
      int k = vsnprintf(s ? s + n : nullptr, s ? cap - n : 0, fmt, a);
      va_end(a);
      if (k < 0) { ok = false; return; }
      if (s && n + (size_t)k < cap) { n += (size_t)k; return; }
      size_t nc = (cap ? cap * 2 : 8192) + (size_t)k;
      char* ns = (char*)realloc(s, nc);
      if (!ns) { ok = false; return; }
      s = ns;
      cap = nc;
    }
  }
  void str(const char* v) {
    add("\"");
    for (const char* p = v; *p; p++) {
      if (*p == '"' || *p == '\\') add("\\%c", *p);
      else if ((uint8_t)*p < 0x20) add("\\u%04x", (uint8_t)*p);
      else add("%c", *p);
    }
    add("\"");
  }
  char* take() {
    char* t = s;
    s = nullptr;
    return t;
  }
};

// ---------------------------------------------------------------- cuentas (statistics.ts / extras.ts)
struct Block {
  int played, wins, losses, draws, gf, ga, pgf, pga;
  void add(int res, int mine, int theirs, int pm, int pt) {  // res: 1 G, 0 E, -1 P
    played++;
    if (res > 0) wins++;
    else if (res < 0) losses++;
    else draws++;
    gf += mine;
    ga += theirs;
    if (pm >= 0) pgf += pm, pga += pt;
  }
};
struct Rec {
  int played, wins, draws, losses;
  void add(int res) {
    played++;
    if (res > 0) wins++;
    else if (res < 0) losses++;
    else draws++;
  }
};
struct Other {
  uint32_t code;
  char name[20];
  Rec r;
};

static void block(Out& o, const Block& b) {
  o.add("{\"played\":%d,\"wins\":%d,\"losses\":%d,\"draws\":%d,\"winPct\":", b.played, b.wins, b.losses, b.draws);
  if (b.played) o.add("%.17g", (double)b.wins / b.played * 100);  // como la app: (wins / played) * 100
  else o.add("null");
  o.add(",\"goalsFor\":%d,\"goalsAgainst\":%d,\"goalDiff\":%d,\"penaltyGoalsFor\":%d,\"penaltyGoalsAgainst\":%d}", b.gf, b.ga,
        b.gf - b.ga, b.pgf, b.pga);
}
static void rec(Out& o, const Rec& r) {
  o.add("{\"played\":%d,\"wins\":%d,\"draws\":%d,\"losses\":%d,\"winPct\":", r.played, r.wins, r.draws, r.losses);
  if (r.played) o.add("%.17g", (double)r.wins / r.played * 100);
  else o.add("null");
  o.add("}");
}
static void idOf(uint32_t code, char* out, size_t n) {
  if (jugadores.find(code)) jugadores.idOf(code, out, n);
  else snprintf(out, n, "placa-%08lx", (unsigned long)code);
}
static Other* findOther(Other* list, int& n, int cap, uint32_t code, const char* name) {
  for (int i = 0; i < n; i++)
    if (list[i].code == code) {
      snprintf(list[i].name, sizeof(list[i].name), "%s", name);  // el último nombre (nameSnapshot)
      return &list[i];
    }
  if (n >= cap) return nullptr;
  Other& x = list[n++];
  memset(&x, 0, sizeof(x));
  x.code = code;
  snprintf(x.name, sizeof(x.name), "%s", name);
  return &x;
}
static int byPlayed(const void* a, const void* b) { return ((const Other*)b)->r.played - ((const Other*)a)->r.played; }
static double pct(const Rec& r, int v) { return r.played ? (double)v / r.played * 100 : 0; }

static const char* const ICONS[] = {"⚑","★","♛","⬆","⏫","✶","◎","∞","✓","✪","✹","💯","⚔","⏱","⚭","➹","⚡","♜","🜂","⛨","✺","⑩","🎉","☄","⛟","⚽","🥅","◉","⚔","◆","◈","♔","⚒","🏆","♕","◎","❄","🎯","☠","⧗","↺","🥊","✧","✦","✺","☺","🌐","🦉","🌅","🏃","⌛"};
static_assert(sizeof(ICONS) / sizeof(ICONS[0]) == progresion::ACH_COUNT, "Iconos de logros: los de la app");
static const char* const RARITY[3] = {"common", "rare", "epic"};
static const char* const WEEKDAY[7] = {"Dom", "Lun", "Mar", "Mié", "Jue", "Vie", "Sáb"};
static const char* const DAYPART[4] = {"mañana", "mediodía", "tarde", "noche"};
static int dayPart(int h) { return h >= 6 && h < 12 ? 0 : h >= 12 && h < 16 ? 1 : h >= 16 && h < 21 ? 2 : 3; }
static const int RIVAL_MIN = 5, TOP = 12, LAST = 60;

// Palmarés del jugador `id` (playerTournaments): abre cada torneo no descartado (pocos) para ver si juega y su puesto.
struct Title { uint16_t s; uint64_t at; bool champion; uint8_t place, of; };
static void palmares(Out& o, const char* id) {
  using namespace torneos;
  static Tournament t;
  Title* list = nSummaries ? (Title*)calloc(nSummaries, sizeof(Title)) : nullptr;
  Place* ranking = (Place*)calloc(MAX_TEAMS_T, sizeof(Place));
  int n = 0;
  for (uint16_t i = 0; list && ranking && i < nSummaries; i++) {
    const Summary& s = summaries[i];
    if (s.status == Status::Cancelled || !load(s.id, t)) continue;
    bool in = false;
    for (uint16_t k = 0; k < t.nTeams && !in; k++)
      for (uint8_t j = 0; j < t.teams[k].n; j++) in |= !strcmp(t.teams[k].player[j], id);
    for (uint8_t k = 0; k < t.nEntrants && !in; k++) in = !strcmp(t.entrants[k], id);
    if (!in) continue;
    Title& x = list[n++];
    x.s = i;
    x.at = s.finishedAt ? s.finishedAt : s.createdAt;
    if (s.status != Status::Finished) continue;
    x.champion = t.winner >= 0 && t.winner < (int16_t)t.nTeams;
    if (x.champion) {
      const torneos::Team& w = t.teams[t.winner];
      x.champion = false;
      for (uint8_t j = 0; j < w.n; j++) x.champion |= !strcmp(w.player[j], id);
    }
    uint8_t nr = finalRanking(t, ranking, (uint8_t)(MAX_TEAMS_T > 255 ? 255 : MAX_TEAMS_T));
    for (uint8_t k = 0; k < nr; k++) {
      if (ranking[k].place > x.of) x.of = ranking[k].place;
      const torneos::Team& tm = t.teams[ranking[k].team];
      bool mine = false;
      for (uint8_t j = 0; j < tm.n; j++) mine |= !strcmp(tm.player[j], id);
      if (mine && !x.place) x.place = ranking[k].place;
    }
  }
  // Lo más reciente arriba (con el mismo momento, en el orden de la lista).
  for (int a = 1; a < n; a++)
    for (int b = a; b > 0 && list[b].at > list[b - 1].at; b--) {
      Title k = list[b];
      list[b] = list[b - 1];
      list[b - 1] = k;
    }
  static const char* const FMT[3] = {"league", "bracket", "pool"};
  static const char* const ST[3] = {"active", "finished", "cancelled"};
  for (int i = 0; i < n; i++) {
    const Summary& s = summaries[list[i].s];
    o.add("%s{\"name\":", i ? "," : "");
    o.str(s.name);
    o.add(",\"format\":\"%s\",\"status\":\"%s\",\"champion\":%s,\"at\":%llu", FMT[(int)s.format], ST[(int)s.status],
          list[i].champion ? "true" : "false", (unsigned long long)list[i].at);
    if (s.cup[0]) o.add(",\"cup\":"), o.str(s.cup);
    if (s.logo[0]) o.add(",\"logo\":"), o.str(s.logo);
    if (s.banner[0]) o.add(",\"banner\":"), o.str(s.banner);
    if (list[i].place) o.add(",\"place\":%u,\"of\":%u", list[i].place, list[i].of);
    o.add("}");
  }
  free(list);
  free(ranking);
  release(t);
}

// El perfil (snapshot.ts) del jugador `code`, en JSON. malloc; nullptr si no hay memoria.
static char* snapshot(uint32_t code, uint32_t nowUtc) {
  const jug::Player* me = jugadores.find(code);
  if (!me) return nullptr;
  int nm = 0;
  int* idx = (int*)malloc(sizeof(int) * (size_t)(partidos::count ? partidos::count : 1));
  Other* rivals = (Other*)calloc(128, sizeof(Other));
  Other* mates = (Other*)calloc(128, sizeof(Other));
  int nr = 0, nt = 0;
  struct Free {
    Other *a, *b;
    ~Free() { free(a); free(b); }
  } freeLater{rivals, mates};
  if (!idx || !rivals || !mates) {
    free(idx);
    return nullptr;
  }
  for (int i = 0; i < partidos::count; i++) idx[nm++] = i;
  qsort(idx, nm, sizeof(int), progresion::byFinish);
  Block general = {}, ranked = {}, byMode[3] = {};
  Rec sides[2] = {}, days[7] = {}, parts[4] = {};
  int run = 0, best = 0, goals = 0, sType = 0, sCount = 0;
  uint64_t playMs = 0;
  char letters[5];
  int nl = 0;
  uint32_t since = 0;
  int* mine = (int*)malloc(sizeof(int) * (size_t)(nm ? nm : 1));  // filas donde juega, en orden
  int nmine = 0;
  if (!mine) {
    free(idx);
    return nullptr;
  }
  for (int k = 0; k < nm; k++) {
    const partidos::Row& m = partidos::rows[idx[k]];
    int team = -1, slot = -1;
    for (int t = 0; t < 2 && team < 0; t++)
      for (int i = 0; i < m.n[t]; i++)
        if (m.code[t][i] == code) team = t, slot = i;
    if (team < 0) continue;
    mine[nmine++] = idx[k];
    int other = 1 - team;
    int res = m.winner == '-' ? 0 : (m.winner == 'w') == (team == 0) ? 1 : -1;
    int my = team ? m.b : m.w, th = team ? m.w : m.b;
    int pm = m.pw < 0 ? -1 : team ? m.pb : m.pw, pt = team ? m.pw : m.pb;
    general.add(res, my, th, pm, pt);
    int mode = m.mode == 'c' ? 1 : m.mode == 'r' ? 2 : 0;
    byMode[mode].add(res, my, th, pm, pt);
    if (mode == 2) ranked.add(res, my, th, pm, pt);
    char l = res > 0 ? 'G' : res < 0 ? 'P' : 'E';
    if (nl == 5) memmove(letters, letters + 1, 4), nl = 4;
    letters[nl++] = l;
    run = res > 0 ? run + 1 : 0;
    if (run > best) best = run;
    // Racha actual: un empate la corta; si cambia el resultado, empieza otra.
    if (l == 'E') sType = 0, sCount = 0;
    else if (sType == l) sCount++;
    else sType = l, sCount = 1;
    playMs += m.totalMs;
    goals += m.pg[team][slot];
    sides[team].add(res);
    if (m.fin) {
      reloj::Clock c = partidos::localOf(m.fin);
      days[reloj::weekday(c)].add(res);
      parts[dayPart(c.hour)].add(res);
      if (!since || m.fin < since) since = m.fin;
    } else {
      parts[dayPart(0)].add(res);  // sin hora: Date(0) es de madrugada, como en la app
      days[4].add(res);
    }
    for (int t = 0; t < 2; t++)
      for (int i = 0; i < m.n[t]; i++) {
        if (t == team && i == slot) continue;
        Other* x = t == team ? findOther(mates, nt, 128, m.code[t][i], m.name[t][i]) : findOther(rivals, nr, 128, m.code[t][i], m.name[t][i]);
        if (x) x->r.add(res);
      }
    (void)other;
  }
  free(idx);
  qsort(rivals, nr, sizeof(Other), byPlayed);
  qsort(mates, nt, sizeof(Other), byPlayed);
  int fav = -1, nem = -1;
  for (int i = 0; i < nr; i++) {
    const Rec& r = rivals[i].r;
    if (r.played < RIVAL_MIN) continue;
    double w = pct(r, r.wins), lo = pct(r, r.losses);
    if (w > 50 && (fav < 0 || w > pct(rivals[fav].r, rivals[fav].r.wins))) fav = i;
    if (lo > 50 && (nem < 0 || lo > pct(rivals[nem].r, rivals[nem].r.losses))) nem = i;
  }

  Out o;
  char id[48];
  const progresion::Player* pr = progresion::of(code);
  uint64_t now = (uint64_t)(nowUtc ? nowUtc : (since ? since : 0)) * 1000u;
  o.add("{\"v\":1,\"generatedAt\":%llu,\"player\":{\"name\":", (unsigned long long)now);
  o.str(me->name);
  // Su foto (la de la placa, en BMP como la sube a la cuenta).
  char rel[40], path[96];
  if (jugadores.photoRel(code, rel, sizeof(rel))) {
    snprintf(path, sizeof(path), "%s/%s", nube::ffatRoot, rel);
    FILE* pf = fopen(path, "rb");
    uint16_t* px = (uint16_t*)malloc(fotos::PIXELS * 2);
    if (pf && px && fseek(pf, 12, SEEK_SET) == 0 && fread(px, 2, fotos::PIXELS, pf) == fotos::PIXELS) {
      char* photo = nube::photoDataUrl(px);
      if (photo) {
        o.add(",\"photo\":");
        o.str(photo);
        free(photo);
      }
    }
    free(px);
    if (pf) fclose(pf);
  }
  if (me->title[0]) {
    o.add(",\"title\":");
    o.str(me->title);
  }
  if (pr) {
    o.add(",\"level\":%d,\"xp\":%ld,\"elo\":%d,\"maxElo\":%d,\"category\":{\"name\":\"%s\",\"color\":\"#%06lx\"}", pr->level, pr->xp,
          pr->elo, pr->maxElo, progresion::CATEGORY[pr->category], (unsigned long)progresion::CATEGORY_COLOR[pr->category]);
  }
  o.add(",\"since\":%llu},\"stats\":{\"general\":", (unsigned long long)(since ? (uint64_t)since * 1000u : now));
  block(o, general);
  o.add(",\"ranked\":");
  block(o, ranked);
  o.add(",\"byMode\":{\"quick\":");
  block(o, byMode[0]);
  o.add(",\"chaos\":");
  block(o, byMode[1]);
  o.add(",\"ranked\":");
  block(o, byMode[2]);
  o.add("},\"streak\":{\"type\":");
  if (sType) o.add("\"%c\"", sType);
  else o.add("null");
  o.add(",\"count\":%d},\"bestWinStreak\":%d,\"recent\":[", sCount, best);
  for (int i = 0; i < nl; i++) o.add("%s\"%c\"", i ? "," : "", letters[i]);
  o.add("],\"playTimeMs\":%llu,\"personalGoals\":%d},\"sides\":{\"white\":", (unsigned long long)playMs, goals);
  rec(o, sides[0]);
  o.add(",\"blue\":");
  rec(o, sides[1]);
  // Hábitos: lunes primero; el día favorito, el de más partidos; la mejor hora, con 3 o más.
  o.add("},\"habits\":{\"byWeekday\":[");
  int favDay = -1;
  for (int j = 0; j < 7; j++) {
    int d = (j + 1) % 7;
    o.add("%s{\"label\":\"%s\",\"rec\":", j ? "," : "", WEEKDAY[d]);
    rec(o, days[d]);
    o.add("}");
    if (favDay < 0 || days[d].played > days[favDay].played) favDay = d;
  }
  o.add("],\"byDayPart\":[");
  int bestPart = -1;
  for (int j = 0; j < 4; j++) {
    o.add("%s{\"label\":\"%s\",\"rec\":", j ? "," : "", DAYPART[j]);
    rec(o, parts[j]);
    o.add("}");
    if (parts[j].played >= 3 && (bestPart < 0 || pct(parts[j], parts[j].wins) > pct(parts[bestPart], parts[bestPart].wins))) bestPart = j;
  }
  o.add("],\"favoriteDay\":");
  if (favDay >= 0 && days[favDay].played) o.add("\"%s\"", WEEKDAY[favDay]);
  else o.add("null");
  o.add(",\"bestDayPart\":");
  if (bestPart >= 0) o.add("\"%s\"", DAYPART[bestPart]);
  else o.add("null");
  // Gente: rivales y compañeros (los 12 con más partidos) y los de sus últimos partidos.
  o.add("},\"people\":{");
  bool firstPerson = true;
  auto person = [&](uint32_t c, const char* name) {
    idOf(c, id, sizeof(id));
    char key[60];
    snprintf(key, sizeof(key), "\"%s\":", id);
    if (o.s && strstr(o.s + (size_t)(strstr(o.s, "\"people\":{") - o.s), key)) return;
    o.add("%s", firstPerson ? "" : ",");
    firstPerson = false;
    o.str(id);
    o.add(":{\"name\":");
    o.str(name);
    o.add("}");
  };
  for (int i = 0; i < nr && i < TOP; i++) person(rivals[i].code, rivals[i].name);
  for (int i = 0; i < nt && i < TOP; i++) person(mates[i].code, mates[i].name);
  if (fav >= TOP) person(rivals[fav].code, rivals[fav].name);
  if (nem >= TOP) person(rivals[nem].code, rivals[nem].name);
  for (int k = nmine - 1, n = 0; k >= 0 && n < LAST; k--, n++) {
    const partidos::Row& m = partidos::rows[mine[k]];
    for (int t = 0; t < 2; t++)
      for (int i = 0; i < m.n[t]; i++)
        if (m.code[t][i] != code) person(m.code[t][i], m.name[t][i]);
  }
  auto list = [&](const char* key, Other* l, int n) {
    o.add("},\"%s\":[", key);
    for (int i = 0; i < n && i < TOP; i++) {
      idOf(l[i].code, id, sizeof(id));
      o.add("%s{\"id\":", i ? "," : "");
      o.str(id);
      o.add(",\"played\":%d,\"wins\":%d,\"draws\":%d,\"losses\":%d,\"winPct\":%d}", l[i].r.played, l[i].r.wins, l[i].r.draws,
            l[i].r.losses, (int)lround(pct(l[i].r, l[i].r.wins)));
    }
    o.add("]");
  };
  list("rivals", rivals, nr);
  // (list() abre con «},»: el de la gente; el siguiente, sin llave)
  o.add(",\"teammates\":[");
  for (int i = 0; i < nt && i < TOP; i++) {
    idOf(mates[i].code, id, sizeof(id));
    o.add("%s{\"id\":", i ? "," : "");
    o.str(id);
    o.add(",\"played\":%d,\"wins\":%d,\"draws\":%d,\"losses\":%d,\"winPct\":%d}", mates[i].r.played, mates[i].r.wins, mates[i].r.draws,
          mates[i].r.losses, (int)lround(pct(mates[i].r, mates[i].r.wins)));
  }
  o.add("],\"favoriteRival\":");
  if (fav >= 0) idOf(rivals[fav].code, id, sizeof(id)), o.str(id);
  else o.add("null");
  o.add(",\"nemesis\":");
  if (nem >= 0) idOf(rivals[nem].code, id, sizeof(id)), o.str(id);
  else o.add("null");
  // Últimos partidos, del más nuevo al más viejo.
  o.add(",\"palmares\":[");
  idOf(code, id, sizeof(id));
  palmares(o, id);
  o.add("],\"matches\":[");
  for (int k = nmine - 1, n = 0; k >= 0 && n < LAST; k--, n++) {
    const partidos::Row& m = partidos::rows[mine[k]];
    int team = 0, slot = 0;
    for (int t = 0; t < 2; t++)
      for (int i = 0; i < m.n[t]; i++)
        if (m.code[t][i] == code) team = t, slot = i;
    int res = m.winner == '-' ? 0 : (m.winner == 'w') == (team == 0) ? 1 : -1;
    o.add("%s{\"at\":%llu,\"mode\":\"%s\",\"result\":\"%c\",\"mine\":%d,\"theirs\":%d", n ? "," : "", (unsigned long long)m.fin * 1000u,
          m.mode == 'c' ? "chaos" : m.mode == 'r' ? "ranked" : "quick", res > 0 ? 'G' : res < 0 ? 'P' : 'E', team ? m.b : m.w,
          team ? m.w : m.b);
    if (const torneos::Summary* ts = m.tournament[0] ? torneos::summaryOf(m.tournament) : nullptr) {
      static const char* const FMT[3] = {"league", "bracket", "pool"};
      o.add(",\"tournament\":{\"name\":");
      o.str(ts->name);
      o.add(",\"format\":\"%s\"}", FMT[(int)ts->format]);
    }
    if (m.pw >= 0) o.add(",\"note\":\"Penaltis %d–%d\"", m.pw, m.pb);
    else if (m.reason == 'g') o.add(",\"note\":\"Gol de oro\"");
    for (int side = 0; side < 2; side++) {
      o.add(side ? "],\"rivals\":[" : ",\"mates\":[");
      int t = side ? 1 - team : team, w = 0;
      for (int i = 0; i < m.n[t]; i++) {
        if (m.code[t][i] == code) continue;
        idOf(m.code[t][i], id, sizeof(id));
        o.add(w++ ? "," : "");
        o.str(id);
      }
    }
    o.add("],\"goals\":%d}", m.pg[team][slot]);
  }
  free(mine);
  // Logros: del más nuevo al más viejo.
  o.add("],\"achievements\":[");
  if (pr) {
    int order[progresion::ACH_COUNT], na = 0;
    for (int a = 0; a < progresion::ACH_COUNT; a++)
      if (pr->ach[a >> 3] & (1 << (a & 7))) order[na++] = a;
    for (int i = 0; i < na; i++)
      for (int j = i + 1; j < na; j++)
        if (pr->achAt[order[j]] > pr->achAt[order[i]]) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    for (int i = 0; i < na; i++) {
      const progresion::Ach& a = progresion::ACH[order[i]];
      o.add("%s{\"id\":\"%s\",\"name\":", i ? "," : "", a.id);
      o.str(a.name);
      o.add(",\"icon\":");
      o.str(ICONS[order[i]]);
      o.add(",\"rarity\":\"%s\",\"at\":%llu}", RARITY[a.rarity], (unsigned long long)pr->achAt[order[i]] * 1000u);
    }
  }
  o.add("],\"rewards\":[]}");
  return o.ok ? o.take() : nullptr;
}

// Lo que se envía: {"p_token","p_clave","p_datos"}. malloc.
static char* body(const Creds& c, uint32_t nowUtc) {
  char* snap = snapshot(c.code, nowUtc);
  if (!snap) return nullptr;
  size_t n = strlen(snap) + 160;
  char* b = (char*)malloc(n);
  if (b) snprintf(b, n, "{\"p_token\":\"%s\",\"p_clave\":\"%s\",\"p_datos\":%s}", c.token, c.clave, snap);
  free(snap);
  return b;
}

}  // namespace perfil

#endif  // MFV3_S3_PERFIL_H
