// MARCADOR FUTBOLÍN V3 · equipos fijos en la pantalla de 7" (como src/services/clubs de la app).
// -------------------------------------------------------------------------------------------------
// Un equipo fijo es una pareja de jugadores con nombre (y el logo que le pusieran en la app). Cuando esos dos
// juegan juntos en el mismo lado, el partido y la victoria ponen el nombre del equipo en vez de BLANCO o AZUL.
//
// Se guardan en la carpeta de los jugadores, «/jugadores/equipos.txt» (así van con el modo prueba y con las copias),
// una línea por equipo: id, nombre, logo, los dos jugadores (ids de la app, ordenados) y cuándo se creó, separados
// por tabuladores. «/jugadores/equipos.pend» dice que hay cambios hechos aquí sin subir a la cuenta.
//
// En la cuenta los equipos van dentro de las preferencias de la mesa («prefs», id «mesa», campo «clubs»), igual
// que en la app: s3_nube.h baja ese campo y sube el de la placa (sin tocar lo demás de las preferencias).

#ifndef MFV3_S3_EQUIPOS_H
#define MFV3_S3_EQUIPOS_H

#include "s3_jugadores.h"

namespace equipos {

static const uint8_t MAX = 32;
static const char* const FILE_PATH = "/jugadores/equipos.txt";
static const char* const PENDING = "/jugadores/equipos.pend";
static const char* const GONE = "/jugadores/equipos.del";  // ids borrados aquí sin subir, uno por línea

struct Club {
  char id[48];
  char name[28];
  char logo[40];
  char player[2][40];  // ids de la app, ordenados
  uint64_t createdAt;  // ms (como la app)
};
// En la PSRAM (las reservas grandes van allí): la memoria interna de la placa va justa.
static Club* list = nullptr;
static uint8_t count = 0;
static bool alloc() {
  if (!list) list = (Club*)calloc(MAX, sizeof(Club));
  return list != nullptr;
}
static bool dirty = false;  // cambios de aquí aún sin subir
static fs::FS* fs_ = nullptr;
static uint32_t revision = 0;  // cambia con cada cambio (las pantallas se rehacen)

// Colores de los logos de la app (Crest.tsx, LOGO_COLORS): el del equipo sale de su logo.
static const uint32_t COLORS[8] = {0x62d6ff, 0xff5cd6, 0xffcc33, 0x3dff9a, 0xb46cff, 0xff8a2a, 0xff4d6d, 0x8fd0ff};
static uint32_t colorOf(const Club& c) {
  uint32_t h = 2166136261u;
  for (const char* p = c.logo[0] ? c.logo : c.name; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
  return COLORS[h % 8];
}

static void sortPair(char a[40], char b[40]) {
  if (strcmp(a, b) > 0) {
    char t[40];
    memcpy(t, a, 40);
    memcpy(a, b, 40);
    memcpy(b, t, 40);
  }
}

static bool load() {
  count = 0;
  dirty = false;
  revision++;
  if (!fs_ || !alloc()) return false;
  dirty = fs_->exists(PENDING);
  fs::File f = fs_->open(FILE_PATH, FILE_READ);
  if (!f) return true;
  char line[240];
  size_t n = 0;
  auto take = [&]() {
    line[n] = 0;
    n = 0;
    if (!line[0] || count >= MAX) return;
    Club c;
    memset(&c, 0, sizeof(c));
    char* field[6] = {nullptr};
    int k = 0;
    field[k++] = line;
    for (char* p = line; *p && k < 6; p++)
      if (*p == '\t') { *p = 0; field[k++] = p + 1; }
    if (k < 5) return;
    snprintf(c.id, sizeof(c.id), "%s", field[0]);
    snprintf(c.name, sizeof(c.name), "%s", field[1]);
    snprintf(c.logo, sizeof(c.logo), "%s", field[2]);
    snprintf(c.player[0], 40, "%s", field[3]);
    snprintf(c.player[1], 40, "%s", field[4]);
    c.createdAt = k > 5 ? strtoull(field[5], nullptr, 10) : 0;
    if (c.id[0] && c.name[0] && c.player[0][0] && c.player[1][0]) list[count++] = c;
  };
  for (int ch = f.read(); ch >= 0; ch = f.read()) {
    if (ch == '\n') take();
    else if (ch != '\r' && n < sizeof(line) - 1) line[n++] = (char)ch;
  }
  if (n) take();
  f.close();
  return true;
}

static bool save(bool fromHere) {
  if (!fs_ || !list) return false;
  fs::File f = fs_->open("/jugadores/equipos.tmp", FILE_WRITE);
  if (!f) return false;
  bool ok = true;
  for (uint8_t i = 0; i < count; i++) {
    char line[240];
    const Club& c = list[i];
    int k = snprintf(line, sizeof(line), "%s\t%s\t%s\t%s\t%s\t%llu\n", c.id, c.name, c.logo, c.player[0], c.player[1],
                     (unsigned long long)c.createdAt);
    ok = f.write((const uint8_t*)line, (size_t)k) == (size_t)k && ok;
  }
  f.close();
  if (!ok) return false;
  fs_->remove(FILE_PATH);
  if (!fs_->rename("/jugadores/equipos.tmp", FILE_PATH)) return false;
  if (fromHere && !dirty) {
    fs::File p = fs_->open(PENDING, FILE_WRITE);
    if (p) p.close();
    dirty = true;
  }
  revision++;
  s3flash::wrote();
  return true;
}

static void begin(fs::FS* fs) {
  fs_ = fs;
  load();
}

// Equipo de estos dos jugadores (por su id de la app), o nullptr.
static const Club* find(const char* a, const char* b) {
  if (!list || !a || !b || !a[0] || !b[0]) return nullptr;
  char x[40], y[40];
  snprintf(x, 40, "%s", a);
  snprintf(y, 40, "%s", b);
  sortPair(x, y);
  for (uint8_t i = 0; i < count; i++)
    if (!strcmp(list[i].player[0], x) && !strcmp(list[i].player[1], y)) return &list[i];
  return nullptr;
}
// Por los códigos de la placa (los del partido).
static const Club* findCodes(const jug::Store& s, uint32_t a, uint32_t b) {
  char x[48], y[48];
  if (!s.find(a) || !s.find(b)) return nullptr;
  s.idOf(a, x, sizeof(x));
  s.idOf(b, y, sizeof(y));
  return find(x, y);
}
static int indexOf(const char* id) {
  for (uint8_t i = 0; i < count; i++)
    if (!strcmp(list[i].id, id)) return i;
  return -1;
}

// El nombre ya lo usa otro equipo (sin distinguir mayúsculas).
static bool nameTaken(const char* name, const char* exceptId) {
  for (uint8_t i = 0; i < count; i++)
    if ((!exceptId || strcmp(list[i].id, exceptId)) && !strcasecmp(list[i].name, name)) return true;
  return false;
}

static void newId(char* out, size_t n) {
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
  snprintf(out, n, "club_%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", b[0], b[1], b[2], b[3], b[4],
           b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

// Crea o cambia el equipo `id` (vacío: nuevo). Como saveClub de la app: si la pareja ya tenía equipo, es ese el
// que cambia. Devuelve el índice o -1.
static int put(const char* id, const char* name, const char* a, const char* b, uint64_t nowMs) {
  if (!alloc() || !name[0] || !a[0] || !b[0] || !strcmp(a, b)) return -1;
  int i = id && id[0] ? indexOf(id) : -1;
  const Club* same = find(a, b);
  if (same && (i < 0 || same != &list[i])) {
    if (i >= 0) {  // la pareja nueva ya tenía equipo: el editado se quita y queda el de la pareja
      for (uint8_t k = (uint8_t)i; k + 1 < count; k++) list[k] = list[k + 1];
      count--;
    }
    i = (int)(find(a, b) - list);
  }
  if (i < 0) {
    if (count >= MAX) return -1;
    i = count++;
    memset(&list[i], 0, sizeof(Club));
    newId(list[i].id, sizeof(list[i].id));
    snprintf(list[i].logo, sizeof(list[i].logo), "escudo-generico-%02u", (unsigned)(1 + count % 10));
    list[i].createdAt = nowMs;
  }
  Club& c = list[i];
  snprintf(c.name, sizeof(c.name), "%s", name);
  snprintf(c.player[0], 40, "%s", a);
  snprintf(c.player[1], 40, "%s", b);
  sortPair(c.player[0], c.player[1]);
  return save(true) ? i : -1;
}

static bool removeAt(int i) {
  if (i < 0 || i >= count) return false;
  char* had = fs_ ? (char*)malloc(2048) : nullptr;
  if (had) {  // que la cuenta no lo devuelva al unir las listas
    size_t n = 0;
    if (fs::File g = fs_->open(GONE, FILE_READ)) {
      n = g.read((uint8_t*)had, 2048 - 64);
      g.close();
    }
    n += (size_t)snprintf(had + n, 2048 - n, "%s\n", list[i].id);
    if (fs::File g = fs_->open(GONE, FILE_WRITE)) {
      g.write((const uint8_t*)had, n);
      g.close();
    }
    free(had);
  }
  for (uint8_t k = (uint8_t)i; k + 1 < count; k++) list[k] = list[k + 1];
  count--;
  return save(true);
}

// ---------------------------------------------------------------- la cuenta (s3_nube.h)
static size_t jsonEsc(char* out, size_t n, const char* s) {
  size_t k = 0;
  for (; *s && k + 7 < n; s++) {
    if (*s == '"' || *s == '\\') out[k++] = '\\', out[k++] = *s;
    else if ((uint8_t)*s < 0x20) k += (size_t)snprintf(out + k, n - k, "\\u%04x", (uint8_t)*s);
    else out[k++] = *s;
  }
  out[k] = 0;
  return k;
}

// El campo «clubs» de la app: [{"id","name","logo","playerIds":[a,b],"createdAt"}]. malloc: lo libera quien llama.
static char* toJson() {
  size_t cap = 16 + (size_t)count * 320;
  char* out = (char*)malloc(cap);
  if (!out) return nullptr;
  size_t k = 0;
  out[k++] = '[';
  for (uint8_t i = 0; i < count; i++) {
    const Club& c = list[i];
    char id[100], name[60], logo[80], a[80], b[80];
    jsonEsc(id, sizeof(id), c.id);
    jsonEsc(name, sizeof(name), c.name);
    jsonEsc(logo, sizeof(logo), c.logo);
    jsonEsc(a, sizeof(a), c.player[0]);
    jsonEsc(b, sizeof(b), c.player[1]);
    k += (size_t)snprintf(out + k, cap - k, "%s{\"id\":\"%s\",\"name\":\"%s\",\"logo\":\"%s\",\"playerIds\":[\"%s\",\"%s\"],\"createdAt\":%llu}",
                          i ? "," : "", id, name, logo, a, b, (unsigned long long)c.createdAt);
  }
  snprintf(out + k, cap - k, "]");
  return out;
}

// Lo que hay en la cuenta sustituye a lo de aquí (salvo si aquí hay cambios sin subir: esos ganan y se suben).
static bool fromJson(const char* arr) {
  if (!arr || *arr != '[' || !alloc()) return false;
  Club* got = (Club*)calloc(MAX, sizeof(Club));
  if (!got) return false;
  uint8_t n = 0;
  for (const char* o = partidos::nextIn(arr, nullptr); o && n < MAX; o = partidos::nextIn(arr, o)) {
    Club c;
    memset(&c, 0, sizeof(c));
    partidos::strOf(partidos::keyIn(o, "id"), c.id, sizeof(c.id));
    partidos::strOf(partidos::keyIn(o, "name"), c.name, sizeof(c.name));
    partidos::strOf(partidos::keyIn(o, "logo"), c.logo, sizeof(c.logo));
    c.createdAt = (uint64_t)partidos::numOf(partidos::keyIn(o, "createdAt"));
    const char* ids = partidos::keyIn(o, "playerIds");
    int k = 0;
    if (ids && *ids == '[')
      for (const char* p = ids + 1; *p && *p != ']' && k < 3; p++)
        if (*p == '"') {
          if (k < 2) partidos::strOf(p, c.player[k], 40);
          k++;
          p = partidos::skipStr(p) - 1;
        }
    if (k != 2 || !c.id[0] || !c.name[0]) continue;  // solo parejas
    sortPair(c.player[0], c.player[1]);
    got[n++] = c;
  }
  memcpy(list, got, sizeof(Club) * n);
  free(got);
  count = n;
  return save(false);
}

// Los borrados aquí sin subir («id\n…»), para la nube. malloc.
static char* goneIds() {
  if (!fs_) return nullptr;
  fs::File g = fs_->open(GONE, FILE_READ);
  if (!g) return nullptr;
  size_t n = g.size();
  char* out = (char*)malloc(n + 1);
  if (out) out[g.read((uint8_t*)out, n)] = 0;
  g.close();
  return out;
}

static void pushed() {
  if (fs_) fs_->remove(PENDING);
  if (fs_) fs_->remove(GONE);
  dirty = false;
}

}  // namespace equipos

#endif  // MFV3_S3_EQUIPOS_H
