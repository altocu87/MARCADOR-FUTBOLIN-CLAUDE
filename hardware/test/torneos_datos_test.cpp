// Torneos de la placa (s3_torneos_datos.h): JSON igual que la app, índice, tipos de torneo, partidos con torneo,
// descartar y la nube simulada (sin red: el transporte es falso). Deja en tests/fixtures/torneos-placa los torneos
// de la app tal como los vuelve a guardar la placa y en tests/fixtures/partidos-placa un partido de torneo guardado
// por la placa: tests/torneosDatosPlaca.test.ts y tests/boardMatches.test.ts comprueban que la app los acepta.
// Uso (desde hardware/): ver check_copies.sh
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "Arduino.h"
#include "mfv3_sound.h"
#include "s3_nube.h"
#include "s3_torneos_datos.h"

namespace fsx = std::filesystem;
static const char* ROOT = "../build/ag2-torneos-test";

static std::string readFile(const std::string& p) {
  FILE* f = fopen(p.c_str(), "rb");
  if (!f) return "";
  std::string s;
  char b[65536];
  size_t n;
  while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
  fclose(f);
  return s;
}
static void writeFile(const std::string& p, const std::string& s) {
  FILE* f = fopen(p.c_str(), "wb");
  assert(f);
  fwrite(s.data(), 1, s.size(), f);
  fclose(f);
}
static bool has(const std::string& s, const char* x) { return s.find(x) != std::string::npos; }

// ---------------------------------------------------------------- nube falsa
static std::vector<std::string> posts;
static std::string cloudRows = "[]", cloudTournaments = "[]", cloudPrefs = "[]";
static nube::Resp reply(int code, const std::string& body) {
  nube::Resp r;
  r.code = code;
  r.len = body.size();
  r.body = (char*)malloc(r.len + 1);
  memcpy(r.body, body.c_str(), r.len + 1);
  return r;
}
static nube::Resp http(const char* method, const char* url, const char*, const char*, const char* body) {
  if (strstr(url, "/auth/")) return reply(200, R"({"access_token":"a","refresh_token":"r","user":{"id":"u","email":"e@x.test"}})");
  if (!strcmp(method, "GET")) {
    if (strstr(url, "tipo=eq.prefs")) return reply(200, cloudPrefs);
    if (!strstr(url, "offset=0")) return reply(200, "[]");
    if (strstr(url, "tipo=in.(tournament)")) return reply(200, cloudTournaments);
    if (strstr(url, "tipo=in.(player,match,prefs)")) return reply(200, cloudRows);
    return reply(200, "[]");
  }
  posts.push_back(body ? body : "");
  return reply(201, "");
}
static void drain() {
  while (nube::busy()) nube::apply();
}
static std::string pending() { return readFile(std::string(ROOT) + "/partidos/subir.txt"); }

// Un partido jugado con el motor (Blanco gana 5–2), guardado por la placa.
static bool playMatch(const char* tournament, const char* fixture, const char* a, const char* b) {
  static mfv3::Engine e;
  static partidos::Match m;
  m = partidos::Match();
  m.mode = 'q';
  mfv3::Config c;
  c.goalsPerPeriod = 5;
  m.cfg = c;
  m.n[0] = m.n[1] = 1;
  snprintf(m.who[0][0].id, 48, "%s", a);
  snprintf(m.who[0][0].name, 28, "Ana");
  snprintf(m.who[1][0].id, 48, "%s", b);
  snprintf(m.who[1][0].name, 28, "Bea");
  m.who[0][0].code = partidos::codeOf(a);
  m.who[1][0].code = partidos::codeOf(b);
  if (tournament) {
    snprintf(m.tournament, sizeof(m.tournament), "%s", tournament);
    snprintf(m.fixture, sizeof(m.fixture), "%s", fixture);
  }
  uint32_t now = 1000;
  if (!e.start(c, now)) return false;
  e.skipCountdown(now);
  const int seq[] = {0, 1, 0, 0, 1, 0, 0};
  for (int t : seq) e.goal(t ? mfv3::Team::Blue : mfv3::Team::White, now += 7000);
  m.startEpoch = 1792000000u;
  m.endEpoch = m.startEpoch + 60;
  return partidos::save(e, m);
}

int main() {
  fsx::remove_all(ROOT);
  fsx::create_directories(std::string(ROOT) + "/partidos");
  fsx::create_directories(std::string(ROOT) + "/torneos");
  partidos::setDir((std::string(ROOT) + "/partidos").c_str());
  torneos::guardado::setDir((std::string(ROOT) + "/torneos").c_str());
  FFat.mkdir("/jugadores");
  partidos::begin(true);
  fs::FS disk;
  jug::Store players;
  players.begin(&disk);
  nube::begin(&players, ROOT);
  nube::http = http;

  // ------------------------------------------------ torneos de la app, como llegan (archivo tal cual)
  std::string app = readFile("../tests/fixtures/progresion/torneos-app.json");
  assert(!app.empty());
  std::vector<std::string> ids, texts;
  const char* arr = app.c_str();
  for (const char* o = partidos::nextIn(arr, nullptr); o; o = partidos::nextIn(arr, o)) {
    std::string txt(o, partidos::closeOf(o) + 1);
    char id[48];
    partidos::strOf(partidos::keyIn(o, "id"), id, sizeof(id));
    ids.push_back(id);
    texts.push_back(txt);
  }
  assert(ids.size() >= 6);
  // Uno con cosas que la placa no conoce (en la config y arriba): se conservan al guardar.
  std::string& odd = texts[0];
  odd.insert(odd.find("\"testMode\""), "\"chaos\":{\"doubleLastMinute\":false,\"jokers\":true},");
  odd.insert(odd.size() - 1, ",\"futuro\":{\"nivel\":[1,2,{\"a\":\"b}\"}]}");
  for (size_t i = 0; i < ids.size(); i++) writeFile(std::string(ROOT) + "/torneos/" + ids[i] + ".json", texts[i]);
  torneos::begin(true);
  assert(torneos::nSummaries == ids.size());
  assert(fsx::exists(std::string(ROOT) + "/torneos/indice.txt"));
  int active = 0, finished = 0;
  for (uint16_t i = 0; i < torneos::nSummaries; i++) {
    const torneos::Summary& s = torneos::summaries[i];
    if (i) assert(torneos::summaries[i - 1].createdAt <= s.createdAt);
    if (s.status == torneos::Status::Active) active++, assert(!s.champion[0] && s.done < s.total);
    if (s.status == torneos::Status::Finished) {
      finished++;
      assert(s.champion[0] && s.nChampion >= 1 && s.finalMatchId[0] && s.done == s.total);
    }
    assert(s.competitionKey[0] && s.competition[0] && s.edition >= 1);
  }
  assert(active == 2 && finished == 4);
  // La progresión sabe qué torneos siguen en juego.
  assert(progresion::nTournaments == (int)ids.size());
  int liveP = 0;
  for (int i = 0; i < progresion::nTournaments; i++) liveP += progresion::tournaments[i].active;
  assert(liveP == 2);

  // Abrir y volver a guardar cada uno: el JSON queda como el de la app (lo compara tests/torneosDatosPlaca.test.ts).
  fsx::create_directories("../tests/fixtures/torneos-placa");
  for (auto& p : fsx::directory_iterator("../tests/fixtures/torneos-placa")) fsx::remove(p.path());
  torneos::Tournament t;
  for (size_t i = 0; i < ids.size(); i++) {
    assert(torneos::load(ids[i].c_str(), t));
    assert(t.nTeams > 0 && t.nFix > 0);
    assert(torneos::save(t));
    std::string out = readFile(std::string(ROOT) + "/torneos/" + ids[i] + ".json");
    if (i == 0) {
      assert(has(out, "\"chaos\":{\"doubleLastMinute\":false,\"jokers\":true}"));
      assert(has(out, "\"futuro\":{\"nivel\":[1,2,{\"a\":\"b}\"}]}"));
      assert(has(out, "\"overtimeSeconds\":60,\"penaltyRounds\":5,\"penaltyFirstTeam\":\"white\""));
    } else {
      writeFile("../tests/fixtures/torneos-placa/" + ids[i] + ".json", out);
    }
  }
  // Copa: siguiente cruce y serie de la final; Pool: escalera, pistas y final.
  bool bracket = false, pool = false;
  for (size_t i = 0; i < ids.size(); i++) {
    assert(torneos::load(ids[i].c_str(), t));
    for (uint16_t k = 0; k < t.nFix; k++) {
      const torneos::Fixture& f = t.fix[k];
      if (t.format == torneos::Format::Bracket && f.next >= 0) bracket = true, assert(t.fix[f.next].round > f.round);
      if (t.format == torneos::Format::Pool && f.court) pool = true;
      if (f.hasSeries) assert(f.bestOf == 3 && f.nMatches >= 2 && !strcmp(f.matchIds[f.nMatches - 1], f.matchId));
    }
    if (t.format == torneos::Format::Pool) assert(t.hasLadder && t.ladder.nStart >= 4 && t.nEntrants >= 8);
  }
  assert(bracket && pool);
  printf("torneos de la app: se leen, se resumen y se vuelven a guardar igual (%zu)\n", ids.size());

  // Con sus partidos en el historial: al abrir cada torneo, `played` tiene sus partidos (en orden) y la ficha también.
  {
    std::string all = readFile("../tests/fixtures/progresion/partidos-torneos.jsonl");
    size_t at = 0;
    while (at < all.size()) {
      size_t e = all.find('\n', at);
      std::string line = all.substr(at, e - at);
      at = e == std::string::npos ? all.size() : e + 1;
      partidos::Row row;
      if (!line.empty() && partidos::rowFromText(line.c_str(), row)) partidos::push(row);
    }
    for (size_t i = 0; i < ids.size(); i++) {
      assert(torneos::load(ids[i].c_str(), t));
      int inFix = 0;
      for (uint16_t k = 0; k < t.nFix; k++) inFix += t.fix[k].nMatches;
      assert(t.nPlayed == inFix && inFix > 0);
      for (uint16_t k = 1; k < t.nPlayed; k++) assert(t.played[k - 1].r.finishedAt <= t.played[k].r.finishedAt);
      torneos::Summary s;
      torneos::guardado::summarizeAll(t, s);
      assert(s.nPlayers > 0);
      if (t.status == torneos::Status::Finished) assert(s.mvpId[0] && s.finalMatchId[0]);
    }
    partidos::count = 0;
    ++partidos::revision;
  }
  printf("torneos abiertos con sus partidos: lista de jugados, MVP y jugadores del palmarés\n");

  // ------------------------------------------------ índice: se rehace si falta y recoge restos de un corte
  std::string ip = std::string(ROOT) + "/torneos/indice.txt";
  std::string before = readFile(ip);
  assert(torneos::loadIndex() && torneos::nSummaries == ids.size() && readFile(ip) == before);
  fsx::remove(ip);
  writeFile(std::string(ROOT) + "/torneos/basura.nube", "{");
  fsx::rename(std::string(ROOT) + "/torneos/" + ids[1] + ".json", std::string(ROOT) + "/torneos/" + ids[1] + ".json.bak");
  assert(torneos::loadIndex() && torneos::nSummaries == ids.size());
  assert(readFile(ip) == before);
  assert(!fsx::exists(std::string(ROOT) + "/torneos/basura.nube"));
  assert(fsx::exists(std::string(ROOT) + "/torneos/" + ids[1] + ".json"));
  // Una línea cuyo torneo ya no está se quita.
  fsx::remove(std::string(ROOT) + "/torneos/" + ids[2] + ".json");
  assert(torneos::loadIndex() && torneos::nSummaries == ids.size() - 1);
  writeFile(std::string(ROOT) + "/torneos/" + ids[2] + ".json", texts[2]);
  assert(torneos::loadIndex() && torneos::nSummaries == ids.size());
  printf("índice de torneos: se rehace, recupera un .bak y quita lo que falta\n");

  // ------------------------------------------------ partidos con torneo
  assert(torneos::load(ids[5].c_str(), t));  // la liga en juego
  assert(t.status == torneos::Status::Active);
  const char* fx = t.fix[t.nFix - 1].id;
  assert(playMatch(t.id, fx, t.teams[0].player[0], t.teams[1].player[0]));
  const partidos::Row& r = partidos::rows[partidos::count - 1];
  assert(!strcmp(r.tournament, t.id) && !strcmp(r.fixture, fx) && r.startedAt == 1792000000000ull);
  char jp[160];
  partidos::jsonPath(r.id, jp, sizeof(jp));
  std::string mj = readFile(jp);
  assert(has(mj, (std::string(",\"tournament\":{\"id\":\"") + t.id + "\",\"fixtureId\":\"" + fx + "\"}}").c_str()));
  fsx::create_directories("../tests/fixtures/partidos-placa");
  writeFile("../tests/fixtures/partidos-placa/torneo.json", mj);
  // El índice lo recuerda y un índice de antes de los torneos se vuelve a leer del JSON.
  std::string mid = r.id;
  partidos::begin(true);
  assert(partidos::count == 1 && !strcmp(partidos::rows[0].tournament, t.id) && !strcmp(partidos::rows[0].fixture, fx));
  std::string idx = readFile(std::string(ROOT) + "/partidos/indice.txt");
  std::string oldLine = idx.substr(0, idx.find("|t"));
  writeFile(std::string(ROOT) + "/partidos/indice.txt", oldLine + "\n");
  partidos::begin(true);
  assert(partidos::count == 1 && !strcmp(partidos::rows[0].tournament, t.id) && partidos::rows[0].finishedAt == 1792000060000ull);
  assert(has(readFile(std::string(ROOT) + "/partidos/indice.txt"), (std::string("|t") + t.id).c_str()));
  // Un partido sin torneo (y un empate de la app con «winner»: null) se siguen leyendo igual.
  assert(playMatch(nullptr, nullptr, "p_a", "p_b"));
  assert(!partidos::rows[partidos::count - 1].tournament[0]);
  partidos::Row d;
  assert(partidos::rowFromText(R"({"id":"m_e","finishedAt":1000,"config":{"mode":"quick","allowDraw":true},"result":{"winner":null,"score":{"white":2,"blue":2}},"participants":[],"events":[],"penalties":[],"tournament":{"id":"t_x","fixtureId":"fx_y"}})", d));
  assert(d.winner == '-' && !strcmp(d.tournament, "t_x") && !strcmp(d.fixture, "fx_y"));
  printf("partidos de torneo: se guardan con «tournament» y el índice antiguo se actualiza\n");

  // ------------------------------------------------ descartar: el torneo y sus partidos (también en la cuenta)
  strcpy(nube::saved.refresh, "r");
  nube::phase = nube::IDLE;
  int n0 = partidos::count;
  assert(torneos::discard(ids[5].c_str()));
  assert(partidos::count == n0 - 1 && torneos::nSummaries == ids.size() - 1);
  assert(!fsx::exists(std::string(ROOT) + "/torneos/" + ids[5] + ".json") && !fsx::exists(jp));
  std::string pend = pending();
  assert(has(pend, ("x " + mid).c_str()) && has(pend, ("T " + ids[5]).c_str()));
  printf("descartar: fuera el torneo y sus partidos, y apuntado para la cuenta\n");

  // ------------------------------------------------ tipos de torneo (FFat)
  torneos::Template a, b;
  torneos::newTemplate(a, torneos::Format::Pool, nullptr);
  assert(!strncmp(a.id, "tpl_", 4) && a.teamSize == 2 && a.pairing == torneos::Pairing::Random && a.poolSwap && a.poolMinutes == 30);
  strcpy(a.name, "Pool de los viernes");
  a.poolByRounds = true;
  a.poolRounds = 6;
  assert(torneos::saveTemplate(a));
  torneos::newTemplate(b, torneos::Format::League, nullptr);
  strcpy(b.name, "Liga a tiempo");
  b.rules.end = torneos::End::Time;
  b.rules.minutes = 4;
  b.ownFinalRules = true;
  b.finalRules = b.rules;
  b.finalRules.end = torneos::End::Goals;
  b.finalRules.goals = 7;
  b.finalKind = torneos::FinalKind::Top2;
  b.points.draw = 2;
  b.legs = 2;
  assert(torneos::saveTemplate(b));
  torneos::Template c;
  torneos::newTemplate(c, torneos::Format::League, &b);
  assert(strcmp(c.id, b.id) && !strcmp(c.name, "Liga a tiempo (copia)") && c.legs == 2);
  assert(torneos::saveTemplate(c) && torneos::nTemplates == 3);
  assert(torneos::removeTemplate(c.id) && torneos::nTemplates == 2);
  torneos::guardado::loadTemplates();
  assert(torneos::nTemplates == 2 && torneos::guardado::templatesDirty);
  const torneos::Template& lb = torneos::templates[1];
  assert(!strcmp(lb.name, "Liga a tiempo") && lb.ownFinalRules && lb.finalRules.goals == 7 && lb.finalRules.end == torneos::End::Goals);
  assert(lb.points.draw == 2 && lb.legs == 2 && lb.finalKind == torneos::FinalKind::Top2 && lb.allowDraw);
  assert(torneos::templates[0].poolByRounds && torneos::templates[0].poolRounds == 6);
  char* tj = torneos::guardado::templatesJson();
  std::string tjs = tj;
  free(tj);
  assert(has(tjs, "\"finalGoals\":7,\"finalEndCondition\":\"goals\",\"finalMinutes\":4") && has(tjs, "\"poolLimit\":\"rounds\""));
  assert(has(tjs, "\"finalGoals\":null") && has(tjs, "\"points\":{\"win\":3,\"draw\":2,\"loss\":0}") && has(tjs, "\"legs\":2"));
  writeFile("../tests/fixtures/torneos-placa/tipos.json", tjs + "\n");
  char* gone = torneos::guardado::templatesGoneIds();
  assert(gone && has(gone, c.id));
  free(gone);
  printf("tipos de torneo: se crean, copian, borran y se leen como en la app\n");

  // ------------------------------------------------ nube: bajar y subir
  // Llega de la cuenta: un torneo nuevo, el borrado de otro y las preferencias con un tipo de torneo de la app.
  std::string arrivedId = "t_nube-1";
  std::string arrived = texts[3];
  arrived.replace(arrived.find(ids[3]), ids[3].size(), arrivedId);
  cloudTournaments = "[{\"tipo\":\"tournament\",\"id\":\"" + arrivedId + "\",\"borrado\":false,\"datos\":" + arrived +
                     ",\"actualizado\":\"2026-10-20T00:00:00Z\"},{\"tipo\":\"tournament\",\"id\":\"" + ids[4] +
                     "\",\"borrado\":true,\"datos\":{},\"actualizado\":\"2026-10-20T00:00:01Z\"}]";
  std::string appTpl = R"({"id":"tpl_app","name":"Copa de la app","format":"bracket","teamSize":1,"pairing":"elo","seeding":"random","final":"none","finalBestOf":3,"ranked":true,"endCondition":"goals","goalsPerPeriod":6,"minutesPerPeriod":5,"finalGoals":null})";
  std::string prefsDatos = "{\"volume\":70,\"clubs\":[],\"tournamentTemplates\":[" + appTpl + "," +
                           R"({"id":")" + std::string(c.id) + R"(","name":"Borrado aquí","format":"league","teamSize":1,"pairing":"elo","seeding":"elo","final":"none","finalBestOf":1,"ranked":false,"endCondition":"goals","goalsPerPeriod":5,"minutesPerPeriod":5,"finalGoals":null})" +
                           "],\"soundEvents\":{\"goal\":\"x\"}}";
  cloudRows = "[{\"tipo\":\"prefs\",\"id\":\"mesa\",\"borrado\":false,\"datos\":" + prefsDatos + ",\"actualizado\":\"2026-10-19T00:00:00Z\"}]";
  cloudPrefs = "[{\"datos\":" + prefsDatos + "}]";
  // Uno de aquí cambiado sin subir: no se pisa con lo que baje.
  assert(torneos::load(ids[0].c_str(), t));
  snprintf(t.name, sizeof(t.name), "Cambiado aquí");
  assert(torneos::save(t));
  cloudTournaments.insert(cloudTournaments.size() - 1, ",{\"tipo\":\"tournament\",\"id\":\"" + ids[0] +
                                                         "\",\"borrado\":true,\"datos\":{},\"actualizado\":\"2026-10-20T00:00:02Z\"}");
  strcpy(nube::saved.lastPull, "2026-10-01T00:00:00Z");
  nube::saved.tournaments = false;
  posts.clear();
  nube::sync();
  drain();
  assert(nube::phase == nube::OK && nube::saved.tournaments);
  // Lo bajado
  assert(torneos::summaryOf(arrivedId.c_str()) && fsx::exists(std::string(ROOT) + "/torneos/" + arrivedId + ".json"));
  assert(!fsx::exists(std::string(ROOT) + "/torneos/" + arrivedId + ".nube"));
  assert(!torneos::summaryOf(ids[4].c_str()) && !fsx::exists(std::string(ROOT) + "/torneos/" + ids[4] + ".json"));
  assert(torneos::summaryOf(ids[0].c_str()) && !strcmp(torneos::summaryOf(ids[0].c_str())->name, "Cambiado aquí"));
  // Lo subido: el torneo cambiado, los borrados (torneo y partido) y las preferencias con solo sus tipos cambiados.
  bool upT = false, delT = false, delM = false, upPrefs = false;
  for (auto& p : posts) {
    if (has(p, "\"tipo\":\"tournament\"") && has(p, ids[0].c_str()) && has(p, "Cambiado aquí") && has(p, "\"borrado\":false")) upT = true;
    if (has(p, "\"tipo\":\"tournament\"") && has(p, ids[5].c_str()) && has(p, "\"borrado\":true")) delT = true;
    if (has(p, "\"tipo\":\"match\"") && has(p, mid.c_str()) && has(p, "\"borrado\":true")) delM = true;
    if (has(p, "\"tipo\":\"prefs\"")) {
      upPrefs = true;
      assert(has(p, "\"volume\":70") && has(p, "\"soundEvents\":{\"goal\":\"x\"}") && has(p, "\"clubs\":[]"));
      assert(has(p, "Pool de los viernes") && has(p, "Liga a tiempo") && has(p, "tpl_app"));
      assert(!has(p, "Borrado aquí"));  // lo borrado aquí no vuelve
    }
  }
  assert(upT && delT && delM && upPrefs);
  // La placa se queda con la lista unida y ya no queda nada pendiente.
  assert(torneos::nTemplates == 3 && !torneos::guardado::templatesDirty && !FFat.exists("/jugadores/tipos.del"));
  assert(pending().empty());
  printf("nube: torneos bajados y subidos, borrados y tipos de torneo unidos con la cuenta\n");

  // Sin cambios aquí: los tipos de la cuenta sustituyen a los de la placa.
  cloudTournaments = "[]";
  prefsDatos = "{\"volume\":70,\"tournamentTemplates\":[" + appTpl + "]}";
  cloudRows = "[{\"tipo\":\"prefs\",\"id\":\"mesa\",\"borrado\":false,\"datos\":" + prefsDatos + ",\"actualizado\":\"2026-10-21T00:00:00Z\"}]";
  posts.clear();
  nube::sync();
  drain();
  assert(posts.empty() && torneos::nTemplates == 1 && !strcmp(torneos::templates[0].id, "tpl_app") && torneos::templates[0].finalBestOf == 3);

  // Al entrar con la cuenta, todos los torneos de aquí se apuntan para subir.
  nube::tournamentsLoggedIn();
  int lines = 0;
  for (char ch : pending()) lines += ch == '\n';
  assert(lines == torneos::nSummaries);
  printf("Agente 2: torneos, índice, tipos, partidos de torneo, descartar y nube OK\n");
  torneos::release(t);
  return 0;
}
