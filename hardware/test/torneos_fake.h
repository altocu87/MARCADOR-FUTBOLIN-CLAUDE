// MARCADOR FUTBOLÍN V3 · torneos de EJEMPLO para la prueba de las pantallas (s3_torneos_test.cpp).
// -------------------------------------------------------------------------------------------------
// Todo es lo de verdad (reglas, ficha, guardado): aquí solo se crean plantillas y torneos fijos y se juegan con
// partidos falsos del historial (filas de partidos::rows con su torneo y cruce), para recorrer las pantallas:
// una liga de 5 en juego, una copa de 6 con pases, un pool de 5 parejas con banquillo, una liga terminada y una
// competición con 35 ediciones (el palmarés por páginas). Se incluye después del sketch.

#ifndef MFV3_TORNEOS_FAKE_H
#define MFV3_TORNEOS_FAKE_H

namespace torneos {
namespace fake {

static uint64_t now = 1792000000000ull;  // ms (oct 2026)
static uint32_t matchSeq = 1;

// Apunta un partido falso (fila del historial con su marcador) en el cruce `fi`.
static void play(Tournament& t, int16_t fi, uint8_t w, uint8_t b, uint64_t at) {
  if (fi < 0) return;
  partidos::Row r = {};
  snprintf(r.id, sizeof(r.id), "m-fake-%lu", (unsigned long)matchSeq++);
  r.fin = (uint32_t)(at / 1000);
  r.mode = t.ranked ? 'r' : 'q';
  r.winner = w > b ? 'w' : w < b ? 'b' : '-';
  r.reason = 'r';
  r.w = w;
  r.b = b;
  r.pw = r.pb = -1;
  r.totalMs = 240000;
  snprintf(r.tournament, sizeof(r.tournament), "%s", t.id);
  snprintf(r.fixture, sizeof(r.fixture), "%s", t.fix[fi].id);
  const Fixture& f = t.fix[fi];
  for (int s = 0; s < 2; s++) {
    int16_t tm = s ? f.blue : f.white;
    if (tm < 0) continue;
    for (uint8_t k = 0; k < t.teams[tm].n && k < 4; k++) {
      r.code[s][r.n[s]] = partidos::codeOf(t.teams[tm].player[k]);
      snprintf(r.name[s][r.n[s]], 20, "%s", playerName && playerName(t.teams[tm].player[k]) ? playerName(t.teams[tm].player[k]) : "?");
      r.n[s]++;
    }
  }
  partidos::push(r);
  Result res = {};
  snprintf(res.matchId, sizeof(res.matchId), "%s", r.id);
  res.winner = r.winner;
  res.w = w;
  res.b = b;
  res.finishedAt = at;
  res.startedAt = at - r.totalMs;
  recordFixtureResult(t, fi, res);
}

static void tpl(Template& t, const char* name, Format f) {
  newTemplate(t, f, nullptr);
  snprintf(t.name, sizeof(t.name), "%s", name);
}

static int eloSeed(const char*) { return 1200; }
static const char* nameSeed(const char* id) { return playerName && playerName(id) ? playerName(id) : id; }

static Tournament* make(const Template& tp, const char (*sel)[40], uint8_t n, const char* seed, uint64_t at) {
  static Draft d;
  static char left[MAX_ENTRANTS][40];
  static Tournament t;
  uint16_t ed = nextEdition(tp.id);
  char name[64];
  snprintf(name, sizeof(name), "%s · %uª edición", tp.name, ed);
  draftFromTemplate(tp, name, sel, n, eloSeed, seed, nullptr, 0, d, left);
  d.edition = ed;
  alloc(t);
  char err[64];
  if (!createTournament(d, eloSeed, nameSeed, at, seed, t, err, sizeof(err))) {
    printf("fake: no se creó %s: %s\n", name, err);
    return nullptr;
  }
  return &t;
}
// Juega `k` cruces seguidos (o todos con k < 0) con marcadores que van cambiando, y lo guarda.
static void playSome(Tournament* t, int k, uint64_t& at) {
  if (!t) return;
  for (int i = 0; k < 0 || i < k; i++) {
    int16_t f = nextFixture(*t);
    if (f < 0) break;
    at += 600000;
    bool draw = i % 3 == 1 && t->format == Format::League && t->rules.allowDraw;  // empates, solo en la liga
    play(*t, f, (uint8_t)(draw ? 3 : i % 2 ? 2 : 5), (uint8_t)(draw ? 3 : i % 2 ? 5 : 2), at);
  }
  save(*t);
}

// Plantillas y torneos de ejemplo con los jugadores `sel` (12 o más).
static void seed(const char (*sel)[40], uint8_t n) {
  static Template a, c, p, l2, h;
  tpl(a, "Liga del Jueves", Format::League);
  a.rules.end = End::Both;
  a.rules.minutes = 5;
  a.finalKind = FinalKind::Top2;
  snprintf(a.logo, 40, "emoji:\xf0\x9f\xa6\x85");  // 🦅
  snprintf(a.lastTitle, 24, "Farolillo rojo");
  saveTemplate(a);
  tpl(c, "Copa del Viernes", Format::Bracket);
  c.finalBestOf = 3;
  c.ranked = true;
  snprintf(c.cup, 40, "trofeo:plata");
  saveTemplate(c);
  tpl(p, "Pool de los Martes", Format::Pool);
  p.pairing = Pairing::Random;
  p.poolByRounds = true;
  p.poolRounds = 4;
  saveTemplate(p);
  tpl(l2, "Liga de Parejas", Format::League);
  l2.teamSize = 2;
  l2.pairing = Pairing::Elo;
  l2.legs = 2;
  saveTemplate(l2);
  tpl(h, "Copa Histórica", Format::League);
  snprintf(h.cup, 40, "trofeo:bronce");
  saveTemplate(h);
  if (n < 12) return;
  // 35 ediciones terminadas de «Copa Histórica» (3 jugadores, todos contra todos): el palmarés por páginas.
  uint64_t at = now - 400ull * 86400000ull;
  for (int e = 0; e < 35; e++) {
    char s[8];
    snprintf(s, sizeof(s), "h%d", e);
    char tri[3][40];
    for (int k = 0; k < 3; k++) snprintf(tri[k], 40, "%s", sel[(e + k * 4) % n]);
    Tournament* t = make(h, (const char(*)[40])tri, 3, s, at);
    playSome(t, -1, at);
    at += 7ull * 86400000ull;
  }
  // 1ª edición de la liga (sin final), terminada.
  Template noFinal = a;
  noFinal.finalKind = FinalKind::None;
  at = now - 20ull * 86400000ull;
  playSome(make(noFinal, sel, 4, "a", at), -1, at);
  // 2ª edición: liga de 5 en juego (cuatro partidos jugados, uno empatado).
  at = now - 2ull * 86400000ull;
  playSome(make(a, sel, 5, "b", at), 4, at);
  // Copa de 6 con dos pases directos (un partido jugado).
  at = now - 86400000ull;
  playSome(make(c, sel + 2, 6, "c", at), 1, at);
  // Pool de 5 parejas (2 pistas y banquillo): la ronda 1 jugada.
  at = now - 3600000ull;
  playSome(make(p, sel, 10, "d", at), 2, at);
  loadIndex();
}

}  // namespace fake
}  // namespace torneos

#endif
