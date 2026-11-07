// Bajar de la nube por páginas sin saltarse filas aunque otra mesa cambie algo entre página y página
// (auditoría MD-05 / NC01). Servidor PostgREST simulado en memoria: no consulta Supabase ni toca una placa.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "Arduino.h"
#include "mfv3_sound.h"
#include "s3_nube.h"

namespace fake {
struct Row {
  std::string tipo, id, act;
};
static std::vector<Row> rows;
static int micro = 1, gets = 0;
static bool sawOffset = false;
static std::function<void()> afterFirstGet;
static std::string stamp() {
  char b[48];
  snprintf(b, sizeof(b), "2026-11-07T10:00:00.%06d+00:00", micro++);
  return b;
}
// Una subida (un upsert): todas sus filas con la misma hora del servidor.
static void put(const std::vector<std::string>& ids, const char* tipo = "player") {
  std::string t = stamp();
  for (auto& id : ids) {
    auto it = std::find_if(rows.begin(), rows.end(), [&](const Row& r) { return r.tipo == tipo && r.id == id; });
    if (it == rows.end()) rows.push_back({tipo, id, t});
    else it->act = t;
  }
}
static std::string decode(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '%' && i + 2 < s.size()) o += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16), i += 2;
    else o += s[i];
  }
  return o;
}
static std::string param(const std::string& url, const char* name) {
  std::string k = std::string("&") + name + "=";
  size_t p = url.find(k);
  if (p == std::string::npos) return "";
  size_t e = url.find('&', p + 1);
  return decode(url.substr(p + k.size(), e == std::string::npos ? std::string::npos : e - p - k.size()));
}
// Filtros lógicos de PostgREST: or=(a.gt.x,and(b.eq."y",c.gt.z)), con comillas y «\».
struct Node {
  std::string op, col, cmp, value;  // op: "and"/"or" (con hijos) o vacío (comparación)
  std::vector<Node> items;
};
static std::vector<Node> parseList(const std::string& s, size_t& i) {
  std::vector<Node> out;
  assert(s[i] == '(');
  i++;
  for (;;) {
    Node n;
    if (!s.compare(i, 4, "and(") || !s.compare(i, 3, "or(")) {
      n.op = s[i] == 'a' ? "and" : "or";
      i += n.op.size();
      n.items = parseList(s, i);
    } else {
      size_t d1 = s.find('.', i), d2 = s.find('.', d1 + 1);
      assert(d1 != std::string::npos && d2 != std::string::npos);
      n.col = s.substr(i, d1 - i);
      n.cmp = s.substr(d1 + 1, d2 - d1 - 1);
      i = d2 + 1;
      if (s[i] == '"') {
        for (i++; s[i] != '"'; i++) {
          assert(i < s.size());
          if (s[i] == '\\') i++;
          n.value += s[i];
        }
        i++;
      } else {
        while (s[i] != ',' && s[i] != ')') n.value += s[i++];
      }
    }
    out.push_back(n);
    if (s[i] == ',') { i++; continue; }
    assert(s[i] == ')');
    i++;
    return out;
  }
}
static bool eval(const Node& n, const Row& r) {
  if (n.op == "and") { for (auto& x : n.items) if (!eval(x, r)) return false; return true; }
  if (n.op == "or") { for (auto& x : n.items) if (eval(x, r)) return true; return false; }
  const std::string& v = n.col == "actualizado" ? r.act : n.col == "tipo" ? r.tipo : r.id;
  assert(n.col == "actualizado" || n.col == "tipo" || n.col == "id");
  if (n.cmp == "eq") return v == n.value;
  assert(n.cmp == "gt");
  return v > n.value;
}
static std::string json(const std::string& s) {
  std::string o;
  for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
  return o;
}
static nube::Resp reply(int code, const std::string& body) {
  nube::Resp r;
  r.code = code;
  r.len = body.size();
  r.body = (char*)malloc(r.len + 1);
  assert(r.body);
  memcpy(r.body, body.c_str(), r.len + 1);
  return r;
}
static nube::Resp http(const char* method, const char* url, const char*, const char*, const char*) {
  assert(!strcmp(method, "GET"));
  std::string u = url;
  assert(param(u, "order") == "actualizado.asc,tipo.asc,id.asc");
  std::string tipos = param(u, "tipo"), since = param(u, "actualizado"), off = param(u, "offset");
  if (!off.empty()) sawOffset = true;
  std::vector<const Row*> list;
  std::vector<Node> tree;
  std::string orq = param(u, "or");
  if (!orq.empty()) {
    size_t i = 0;
    tree = parseList(orq, i);
    assert(i == orq.size());
  }
  for (auto& r : rows) {
    if (tipos.find(r.tipo) == std::string::npos) continue;
    if (!since.empty() && !(r.act > since.substr(3))) continue;
    bool any = tree.empty();
    for (auto& n : tree) any = any || eval(n, r);
    if (any) list.push_back(&r);
  }
  std::sort(list.begin(), list.end(), [](const Row* a, const Row* b) {
    return a->act != b->act ? a->act < b->act : a->tipo != b->tipo ? a->tipo < b->tipo : a->id < b->id;
  });
  int offset = off.empty() ? 0 : atoi(off.c_str()), limit = atoi(param(u, "limit").c_str());
  std::string out = "[";
  for (int i = offset; i < (int)list.size() && i < offset + limit; i++) {
    const Row& r = *list[i];
    if (out.size() > 1) out += ",";
    out += "{\"tipo\":\"" + r.tipo + "\",\"id\":\"" + json(r.id) + "\",\"datos\":{\"name\":\"N" + json(r.id) +
           "\",\"active\":true},\"borrado\":false,\"actualizado\":\"" + r.act + "\"}";
  }
  if (++gets == 1 && afterFirstGet) afterFirstGet();
  return reply(200, out + "]");
}
}  // namespace fake

static std::set<std::string> received() {
  std::set<std::string> s;
  for (int i = 0; i < nube::nChanges; i++) s.insert(nube::changes[i].id);
  return s;
}
static void reset() {
  fake::rows.clear();
  fake::gets = 0;
  fake::sawOffset = false;
  fake::afterFirstGet = nullptr;
  free(nube::changes);
  nube::changes = nullptr;
  nube::nChanges = nube::capChanges = 0;
  nube::newLastPull[0] = 0;
}
static std::vector<std::string> ids(const char* pre, int n) {
  std::vector<std::string> v;
  char b[16];
  for (int i = 0; i < n; i++) snprintf(b, sizeof(b), "%s%03d", pre, i), v.push_back(b);
  return v;
}

int main() {
  std::filesystem::create_directories("../build/equipo-b-paginacion-test");
  partidos::setDir("../build/equipo-b-paginacion-test");
  nube::http = fake::http;

  // 1) Escenario de la auditoría: 60 jugadores en una subida, páginas de 20; tras la primera página cambia p000
  //    (ya leído) y pasa al final. Con «offset» faltaba p020 y la marca lo dejaba fuera de la siguiente bajada.
  reset();
  auto all = ids("p", 60);
  for (int i = 0; i < 60; i += 20) fake::put(std::vector<std::string>(all.begin() + i, all.begin() + i + 20));
  fake::afterFirstGet = [] { fake::put({"p000"}); };
  assert(nube::pull("", "player", 20));
  auto got = received();
  printf("paginacion: %d filas, %zu ids distintos (de 60)\n", nube::nChanges, got.size());
  assert(got.size() == 60);
  assert(!fake::sawOffset);
  assert(!strcmp(nube::changes[nube::nChanges - 1].id, "p000"));  // y su versión nueva llega en esta bajada
  // La siguiente bajada desde la marca no deja nada atrás: solo lo nuevo.
  std::string mark = nube::newLastPull;
  free(nube::changes); nube::changes = nullptr; nube::nChanges = nube::capChanges = 0;
  fake::put({"p059"});
  assert(nube::pull(mark.c_str(), "player", 20));
  assert(nube::nChanges == 1 && !strcmp(nube::changes[0].id, "p059"));

  // 2) Fila sin leer y la de la frontera cambian a la vez; todo en una misma hora (empates por tipo e id).
  reset();
  all = ids("q", 100);
  fake::put(all);
  fake::afterFirstGet = [] { fake::put({"q001", "q070"}); fake::put({"q019"}); };
  assert(nube::pull("2026-01-01T00:00:00+00:00", "player", 20));
  assert(received().size() == 100);

  // 3) Ids raros en la frontera de una página: comas, paréntesis, comillas, barras y espacios.
  reset();
  std::vector<std::string> odd = ids("x", 17);
  for (const char* s : {"za,b", "zc(d)", "ze\"f", "zg\\h", "zi.j:k", "zl m"}) odd.push_back(s);
  fake::put(odd);  // 23 filas: la página de 20 se corta en 'ze"f'
  assert(nube::pull("", "player", 20));
  got = received();
  assert(got.size() == 23 && got.count("ze\"f") && got.count("zg\\h") && got.count("zl m"));

  // 4) Una página que no avanza (servidor raro) no se queda dando vueltas: falla y se reintentará.
  reset();
  fake::put(ids("r", 20));
  static int calls = 0;
  calls = 0;
  nube::http = [](const char* m, const char* url, const char* a, const char* b, const char* c) {
    ++calls;
    std::string u = url;
    size_t p = u.find("&or=");
    return fake::http(m, p == std::string::npos ? url : u.substr(0, p).c_str(), a, b, c);  // ignora el cursor
  };
  assert(!nube::pull("", "player", 20));
  assert(calls == 2);
  nube::http = fake::http;

  puts("nube_paginacion_test OK");
  return 0;
}
