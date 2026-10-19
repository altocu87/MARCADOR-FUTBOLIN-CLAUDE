// MARCADOR FUTBOLÍN V3 · la pantalla de 7" con la cuenta del dueño de la mesa en la nube (Supabase).
// -------------------------------------------------------------------------------------------------
// Las mismas peticiones que la app (docs/NUBE.md): entrar con correo y contraseña (se guarda solo el «refresh
// token», nunca la contraseña), subir los partidos que se juegan aquí y los jugadores creados o cambiados aquí, y
// bajar los jugadores (con su foto) y los partidos de la cuenta. Todo va a la tabla `mesa_datos`.
//
// Cuándo: con la placa parada (nunca en un partido: mientras está en el Wi-Fi la radio del mando no oye), al
// entrar, al encender, al acabar cada partido, cada 15 minutos y con «Sincronizar ahora». En la placa el trabajo
// de red va en una tarea aparte (núcleo 0) para que la pantalla siga respondiendo; lo que llega se aplica después,
// poco a poco, desde loop() (apply()). En el PC (pruebas) se hace todo seguido.
//
// Lo pendiente de subir se apunta en la tarjeta: <partidos>/subir.txt (partidos) y /jugadores en la memoria del
// chip (jugadores, en subir.txt dentro de la carpeta de partidos: «j <código>» o «b <id>» si se borró).

#ifndef MFV3_S3_NUBE_H
#define MFV3_S3_NUBE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "s3_fotos.h"
#include "s3_jugadores.h"
#include "s3_partidos.h"

namespace nube {

// Proyecto de Supabase (la misma dirección y la misma clave PÚBLICA que lleva la app web).
static const char* URL = "https://qhoaetqjpyxgqfpimbbi.supabase.co";
static const char* ANON = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InFob2FldHFqcHl4Z3FmcGltYmJpIiwicm9sZSI6ImFub24iLCJpYXQiOjE3OTE3MTQ1ODMsImV4cCI6MjEwNzI5MDU4M30.JdKTfWCjSTgmYf7gFhLzWYmbO28qxpb9Z5IpfLqyjj8";

// ---------------------------------------------------------------- transporte (la placa: HTTPClient; el PC: simulado)
struct Resp {
  int code = 0;      // 0 = sin conexión
  char* body = nullptr;  // malloc (lo libera quien llama)
  size_t len = 0;
};
typedef Resp (*HttpFn)(const char* method, const char* url, const char* bearer, const char* prefer, const char* body);
static HttpFn http = nullptr;
typedef bool (*NetFn)();
static NetFn wifiUp = nullptr;   // conectar al Wi-Fi de casa (true si hay conexión)
static NetFn wifiDown = nullptr; // desconectar (la radio del mando vuelve a su canal)
typedef uint32_t (*NowFn)();
static NowFn nowUtc = nullptr;   // hora (segundos UTC; 0 sin hora)

// ---------------------------------------------------------------- estado guardado
static char stateFile[96] = "";
struct Saved {
  char email[96];
  char userId[48];
  char refresh[160];
  char lastPull[40];
};
static Saved saved;

static void saveState() {
  FILE* f = fopen(stateFile, "wb");
  if (!f) return;
  fprintf(f, "%s\n%s\n%s\n%s\n", saved.email, saved.userId, saved.refresh, saved.lastPull);
  fclose(f);
}
static void loadState() {
  memset(&saved, 0, sizeof(saved));
  FILE* f = fopen(stateFile, "rb");
  if (!f) return;
  char* fields[4] = {saved.email, saved.userId, saved.refresh, saved.lastPull};
  size_t sizes[4] = {sizeof(saved.email), sizeof(saved.userId), sizeof(saved.refresh), sizeof(saved.lastPull)};
  for (int i = 0; i < 4; i++) {
    if (!fgets(fields[i], (int)sizes[i], f)) break;
    fields[i][strcspn(fields[i], "\r\n")] = 0;
  }
  fclose(f);
}
static bool loggedIn() { return saved.refresh[0] != 0; }

// ---------------------------------------------------------------- lo que ve la pantalla
enum Phase : uint8_t { OFF, IDLE, BUSY, OK, FAILED };
static volatile uint8_t phase = OFF;
static char message[96] = "";
static uint32_t lastOkAt = 0;  // millis() de la última sincronización buena
static int pendingCount = 0;   // cosas por subir

// ---------------------------------------------------------------- pendientes (los lleva loop(), no la tarea)
static jug::Store* store = nullptr;
static void pendingPath(char* out, size_t n) { snprintf(out, n, "%s/subir.txt", partidos::dir); }

static void countPending() {
  char p[96];
  pendingPath(p, sizeof(p));
  pendingCount = 0;
  FILE* f = fopen(p, "rb");
  if (!f) return;
  char line[80];
  while (fgets(line, sizeof(line), f))
    if (line[0] > ' ') pendingCount++;
  fclose(f);
}
static void addPending(const char* line) {
  char p[96];
  pendingPath(p, sizeof(p));
  FILE* f = fopen(p, "ab");
  if (!f) return;
  fprintf(f, "%s\n", line);
  fclose(f);
  pendingCount++;
}
// Partido recién guardado (s3_partidos.h) / jugador cambiado o borrado aquí (ui_fichas.h).
static void matchSaved(const char* id) {
  char l[64];
  snprintf(l, sizeof(l), "m %s", id);
  addPending(l);
}
static void playerChanged(uint32_t code) {
  char l[24];
  snprintf(l, sizeof(l), "j %08lx", (unsigned long)code);
  addPending(l);
}
static void playerRemoved(const char* id) {
  char l[64];
  snprintf(l, sizeof(l), "b %s", id);
  addPending(l);
}

// ---------------------------------------------------------------- utilidades
static char* jsonEscape(const char* s) {  // malloc
  size_t n = 0;
  for (const char* p = s; *p; p++) n += (*p == '"' || *p == '\\') ? 2 : ((uint8_t)*p < 0x20 ? 0 : 1);
  char* o = (char*)malloc(n + 1);
  if (!o) return nullptr;
  size_t k = 0;
  for (const char* p = s; *p; p++) {
    if ((uint8_t)*p < 0x20) continue;
    if (*p == '"' || *p == '\\') o[k++] = '\\';
    o[k++] = *p;
  }
  o[k] = 0;
  return o;
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static size_t base64Encode(const uint8_t* in, size_t n, char* out) {
  size_t k = 0;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < n ? (uint32_t)in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
    out[k++] = B64[(v >> 18) & 63];
    out[k++] = B64[(v >> 12) & 63];
    out[k++] = i + 1 < n ? B64[(v >> 6) & 63] : '=';
    out[k++] = i + 2 < n ? B64[v & 63] : '=';
  }
  out[k] = 0;
  return k;
}

// Foto de un jugador (120 × 120 RGB565 de la placa) → «data:image/bmp;base64,…» (el navegador la enseña igual).
static char* photoDataUrl(const uint16_t* px) {
  const int S = 120, row = S * 3;
  size_t bmpLen = 54 + (size_t)row * S;
  uint8_t* bmp = (uint8_t*)calloc(1, bmpLen);
  char* out = (char*)malloc(24 + (bmpLen + 2) / 3 * 4 + 1);
  if (!bmp || !out) {
    free(bmp);
    free(out);
    return nullptr;
  }
  auto le32 = [&](int at, uint32_t v) {
    for (int i = 0; i < 4; i++) bmp[at + i] = (uint8_t)(v >> (8 * i));
  };
  bmp[0] = 'B';
  bmp[1] = 'M';
  le32(2, (uint32_t)bmpLen);
  le32(10, 54);
  le32(14, 40);
  le32(18, S);
  le32(22, S);
  bmp[26] = 1;
  bmp[28] = 24;
  le32(34, (uint32_t)(row * S));
  for (int y = 0; y < S; y++) {
    uint8_t* d = bmp + 54 + (size_t)(S - 1 - y) * row;  // de abajo arriba
    for (int x = 0; x < S; x++) {
      uint16_t c = px[y * S + x];
      uint8_t r = (uint8_t)((c >> 11) << 3), g = (uint8_t)(((c >> 5) & 63) << 2), b = (uint8_t)((c & 31) << 3);
      d[x * 3] = b;
      d[x * 3 + 1] = g;
      d[x * 3 + 2] = r;
    }
  }
  strcpy(out, "data:image/bmp;base64,");
  base64Encode(bmp, bmpLen, out + strlen(out));
  free(bmp);
  return out;
}

// «data:image/…;base64,…» → foto de 120 × 120 (JPEG de la app o BMP de otra placa). false si no se puede.
static bool photoFromDataUrl(const char* url, size_t len, uint16_t* out) {
  const char* comma = (const char*)memchr(url, ',', len);
  if (!comma || len < 16 || memcmp(url, "data:image/", 11)) return false;
  bool isBmp = !memcmp(url + 11, "bmp", 3);
  size_t b64 = len - (size_t)(comma + 1 - url);
  uint8_t* bin = (uint8_t*)malloc(b64 / 4 * 3 + 3);
  if (!bin) return false;
  int n = mfv3snd::base64Decode(comma + 1, b64 - b64 % 4, bin, b64 / 4 * 3 + 3);
  bool ok = false;
  if (n > 54 && isBmp && bin[0] == 'B' && bin[1] == 'M') {
    int w = bin[18] | bin[19] << 8, h = bin[22] | bin[23] << 8, off = bin[10] | bin[11] << 8;
    int row = (w * 3 + 3) & ~3;
    if (w > 0 && h > 0 && bin[28] == 24 && off + row * h <= n) {
      for (int y = 0; y < fotos::SIZE; y++)
        for (int x = 0; x < fotos::SIZE; x++) {
          int sx = x * w / fotos::SIZE, sy = y * h / fotos::SIZE;
          const uint8_t* p = bin + off + (size_t)(h - 1 - sy) * row + sx * 3;
          out[y * fotos::SIZE + x] = (uint16_t)(((p[2] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[0] >> 3));
        }
      ok = true;
    }
  } else if (n > 0) {
    // JPEG: a un archivo y se abre como las fotos de la tarjeta (recorte, tramado y giro incluidos).
    char tmp[96];
    snprintf(tmp, sizeof(tmp), "%s/foto.tmp", partidos::dir);
    FILE* f = fopen(tmp, "wb");
    if (f) {
      fwrite(bin, 1, (size_t)n, f);
      fclose(f);
      ok = fotos::decode(tmp, out);
      ::remove(tmp);
    }
  }
  free(bin);
  return ok;
}

// ---------------------------------------------------------------- cambios que llegan (los aplica loop())
struct Change {
  uint8_t kind;  // 'm' partido nuevo (ya escrito en la tarjeta), 'x' partido borrado, 'p' jugador, 'q' jugador borrado
  char id[48];
  char name[28];
  uint16_t* px;  // foto (malloc) o nullptr
};
static Change* changes = nullptr;
static volatile int nChanges = 0;
static int capChanges = 0, applied = 0;
static bool pushChange(const Change& c) {
  if (nChanges == capChanges) {
    int nc = capChanges ? capChanges * 2 : 64;
    Change* p = (Change*)realloc(changes, (size_t)nc * sizeof(Change));
    if (!p) return false;
    changes = p;
    capChanges = nc;
  }
  changes[nChanges] = c;
  nChanges = nChanges + 1;
  return true;
}

// ---------------------------------------------------------------- el trabajo (en la tarea)
struct Up {
  char line[64];  // la línea de subir.txt
  bool done;
};
static Up* ups = nullptr;  // lo pendiente al empezar (lo prepara loop())
static int nUps = 0;
// Jugadores para subir: lo que hace falta de cada uno, copiado por loop() antes de empezar.
struct UpPlayer {
  char id[48];
  char name[28];
  char* photoUrl;  // «data:image/bmp;base64,…» ya hecha por loop() (la tarea no lee la memoria del chip) o nullptr
};
static UpPlayer* upPlayers = nullptr;
static int nUpPlayers = 0;

enum Job : uint8_t { NONE, LOGIN, SYNC, LINK };
// Entrar con QR: código aleatorio que enseña la placa (y su dirección de la app) y si se ha dejado de esperar.
static const char* APP_URL = "https://altocu87.github.io/MARCADOR-FUTBOLIN-CLAUDE/app/";
static char linkCode[40] = "";
static volatile bool linkCancel = false;
static volatile uint8_t job = NONE;
static char loginEmail[96], loginPass[96];
static char access[1600];
static char newLastPull[40];
static char jobError[96];
static volatile bool jobDone = false;
static bool pendingTrimmed = false;
static bool jobOk = false;
static const char* ffatRoot = "/ffat";

static bool auth(const char* grant, const char* body) {
  char url[200];
  snprintf(url, sizeof(url), "%s/auth/v1/token?grant_type=%s", URL, grant);
  Resp r = http("POST", url, ANON, nullptr, body);
  bool ok = false;
  if (r.code == 200 && r.body) {
    char tok[1600], ref[160], uid[48], mail[96];
    partidos::strOf(partidos::keyIn(r.body, "access_token"), tok, sizeof(tok));
    partidos::strOf(partidos::keyIn(r.body, "refresh_token"), ref, sizeof(ref));
    const char* user = partidos::keyIn(r.body, "user");
    partidos::strOf(partidos::keyIn(user, "id"), uid, sizeof(uid));
    partidos::strOf(partidos::keyIn(user, "email"), mail, sizeof(mail));
    if (tok[0] && ref[0]) {
      snprintf(access, sizeof(access), "%s", tok);
      snprintf(saved.refresh, sizeof(saved.refresh), "%s", ref);
      if (uid[0]) snprintf(saved.userId, sizeof(saved.userId), "%s", uid);
      if (mail[0]) snprintf(saved.email, sizeof(saved.email), "%s", mail);
      ok = true;
    }
  }
  if (!ok) {
    const char* b = r.body ? r.body : "";
    if (!r.code) snprintf(jobError, sizeof(jobError), "Sin conexión con la nube");
    else if (strstr(b, "invalid_credentials") || strstr(b, "Invalid login")) snprintf(jobError, sizeof(jobError), "Correo o contraseña incorrectos");
    else if (strstr(b, "email_not_confirmed") || strstr(b, "not confirmed")) snprintf(jobError, sizeof(jobError), "Falta confirmar el correo");
    else if (strstr(b, "refresh_token") || r.code == 400 || r.code == 401) snprintf(jobError, sizeof(jobError), "La sesión ha caducado: vuelve a entrar");
    else snprintf(jobError, sizeof(jobError), "La nube no responde (%d)", r.code);
  }
  free(r.body);
  return ok;
}

// Enlace mágico de un solo uso (lo preparó la función vincular-placa) → sesión propia de la placa.
static bool verifyHash(const char* tokenHash) {
  char url[200], body[300];
  snprintf(url, sizeof(url), "%s/auth/v1/verify", URL);
  snprintf(body, sizeof(body), "{\"type\":\"magiclink\",\"token_hash\":\"%s\"}", tokenHash);
  Resp r = http("POST", url, ANON, nullptr, body);
  bool ok = false;
  if (r.code == 200 && r.body) {
    char tok[1600], ref[160], uid[48], mail[96];
    partidos::strOf(partidos::keyIn(r.body, "access_token"), tok, sizeof(tok));
    partidos::strOf(partidos::keyIn(r.body, "refresh_token"), ref, sizeof(ref));
    const char* user = partidos::keyIn(r.body, "user");
    partidos::strOf(partidos::keyIn(user, "id"), uid, sizeof(uid));
    partidos::strOf(partidos::keyIn(user, "email"), mail, sizeof(mail));
    if (tok[0] && ref[0]) {
      snprintf(access, sizeof(access), "%s", tok);
      snprintf(saved.refresh, sizeof(saved.refresh), "%s", ref);
      snprintf(saved.userId, sizeof(saved.userId), "%s", uid);
      snprintf(saved.email, sizeof(saved.email), "%s", mail);
      ok = true;
    }
  }
  if (!ok) snprintf(jobError, sizeof(jobError), r.code ? "El QR ha caducado: vuelve a probar" : "Sin conexión con la nube");
  free(r.body);
  return ok;
}

// Espera (cada 2 s, hasta 5 min o hasta que se deja de esperar) a que el móvil confirme el QR.
static bool waitLink() {
  char url[200], body[100];
  snprintf(url, sizeof(url), "%s/rest/v1/rpc/vinculo_placa", URL);
  snprintf(body, sizeof(body), "{\"p_codigo\":\"%s\"}", linkCode);
  for (int i = 0; i < 150 && !linkCancel; i++) {
    Resp r = http("POST", url, ANON, nullptr, body);
    char hash[200] = "";
    if (r.code == 200 && r.body) partidos::strOf(partidos::keyIn(r.body, "token_hash"), hash, sizeof(hash));
    free(r.body);
    if (hash[0]) {
      snprintf(message, sizeof(message), "Entrando…");
      return verifyHash(hash);
    }
#ifdef ARDUINO
    for (int k = 0; k < 20 && !linkCancel; k++) vTaskDelay(pdMS_TO_TICKS(100));
#else
    break;  // en el PC, una sola vuelta
#endif
  }
  snprintf(jobError, sizeof(jobError), "%s", linkCancel ? "" : "El QR ha caducado: vuelve a probar");
  return false;
}

static bool post(const char* body) {
  char url[200];
  snprintf(url, sizeof(url), "%s/rest/v1/mesa_datos?on_conflict=owner_id,tipo,id", URL);
  Resp r = http("POST", url, access, "resolution=merge-duplicates,return=minimal", body);
  free(r.body);
  if (r.code >= 200 && r.code < 300) return true;
  snprintf(jobError, sizeof(jobError), r.code ? "No se pudo subir (%d)" : "Sin conexión con la nube", r.code);
  return false;
}

// Un partido de la tarjeta → la nube.
static bool pushMatch(const char* id, bool* done) {
  char path[112];
  partidos::jsonPath(id, path, sizeof(path));
  char* datos = partidos::readAll(path);
  if (!datos) {
    *done = true;  // ya no está (se borró): nada que subir
    return true;
  }
  size_t n = strlen(datos);
  while (n && (datos[n - 1] == '\n' || datos[n - 1] == '\r')) datos[--n] = 0;
  char* body = (char*)malloc(n + 160);
  bool ok = false;
  if (body) {
    snprintf(body, n + 160, "[{\"tipo\":\"match\",\"id\":\"%s\",\"borrado\":false,\"datos\":%s}]", id, datos);
    ok = post(body);
  }
  free(body);
  free(datos);
  *done = ok;
  return ok;
}

static bool pushPlayer(const UpPlayer& p, bool* done) {
  char* name = jsonEscape(p.name);
  const char* photo = p.photoUrl;
  uint64_t ms = (uint64_t)(nowUtc ? nowUtc() : 0) * 1000u;
  size_t cap = 400 + strlen(name ? name : "") + (photo ? strlen(photo) : 0);
  char* body = (char*)malloc(cap);
  bool ok = false;
  if (body && name) {
    int k = snprintf(body, cap, "[{\"tipo\":\"player\",\"id\":\"%s\",\"borrado\":false,\"datos\":{\"id\":\"%s\",\"name\":\"%s\",\"active\":true,"
                                "\"createdAt\":%llu,\"updatedAt\":%llu",
                     p.id, p.id, name, (unsigned long long)ms, (unsigned long long)ms);
    if (photo) k += snprintf(body + k, cap - k, ",\"photo\":\"%s\"", photo);
    snprintf(body + k, cap - k, "}}]");
    ok = post(body);
  }
  free(body);
  free(name);
  *done = ok;
  return ok;
}

static bool pushDeleted(const char* id, bool* done) {
  char body[200];
  snprintf(body, sizeof(body), "[{\"tipo\":\"player\",\"id\":\"%s\",\"borrado\":true,\"datos\":{}}]", id);
  *done = post(body);
  return *done;
}

// Una fila de la nube → cambio para loop() (los partidos se escriben ya en la tarjeta).
static void takeRow(const char* o) {
  char tipo[16], id[48];
  partidos::strOf(partidos::keyIn(o, "tipo"), tipo, sizeof(tipo));
  partidos::strOf(partidos::keyIn(o, "id"), id, sizeof(id));
  bool del = partidos::isTrue(partidos::keyIn(o, "borrado"));
  const char* datos = partidos::keyIn(o, "datos");
  Change c;
  memset(&c, 0, sizeof(c));
  snprintf(c.id, sizeof(c.id), "%s", id);
  if (!strcmp(tipo, "match")) {
    char path[112];
    partidos::jsonPath(id, path, sizeof(path));
    if (del) {
      c.kind = 'x';
      pushChange(c);
      return;
    }
    if (!datos || *datos != '{') return;
    FILE* test = fopen(path, "rb");
    if (test) {  // ya lo tenemos (los partidos no cambian)
      fclose(test);
      return;
    }
    const char* end = partidos::closeOf(datos);
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fwrite(datos, 1, (size_t)(end - datos + 1), f);
    fputc('\n', f);
    fclose(f);
    c.kind = 'm';
    pushChange(c);
  } else if (!strcmp(tipo, "player")) {
    if (del) {
      c.kind = 'q';
      pushChange(c);
      return;
    }
    if (!datos || *datos != '{') return;
    const char* act = partidos::keyIn(datos, "active");
    if (act && !partidos::isTrue(act)) {
      c.kind = 'q';  // inactivo en la app: fuera de la lista de la placa
      pushChange(c);
      return;
    }
    partidos::strOf(partidos::keyIn(datos, "name"), c.name, sizeof(c.name));
    const char* ph = partidos::keyIn(datos, "photo");
    if (ph && *ph == '"') {
      const char* e = partidos::skipStr(ph);
      uint16_t* px = (uint16_t*)malloc(fotos::PIXELS * 2);
      if (px && photoFromDataUrl(ph + 1, (size_t)(e - ph - 2), px)) c.px = px;
      else free(px);
    }
    c.kind = 'p';
    pushChange(c);
  }
}

static bool pull(const char* since) {
  for (int offset = 0;; offset += 20) {
    char url[400];
    snprintf(url, sizeof(url),
             "%s/rest/v1/mesa_datos?select=tipo,id,datos,borrado,actualizado&tipo=in.(player,match)&order=actualizado.asc,tipo.asc,id.asc&limit=20&offset=%d%s%s",
             URL, offset, since[0] ? "&actualizado=gt." : "", since);
    // «+» de la hora (zona) en la dirección: %2B
    char enc[480];
    size_t k = 0;
    for (const char* p = url; *p && k < sizeof(enc) - 4; p++) {
      if (*p == '+') enc[k++] = '%', enc[k++] = '2', enc[k++] = 'B';
      else enc[k++] = *p;
    }
    enc[k] = 0;
    Resp r = http("GET", enc, access, nullptr, nullptr);
    if (r.code != 200 || !r.body) {
      snprintf(jobError, sizeof(jobError), r.code ? "No se pudo bajar (%d)" : "Sin conexión con la nube", r.code);
      free(r.body);
      return false;
    }
    int rows = 0;
    const char* arr = r.body;
    while (*arr && *arr != '[') arr++;
    for (const char* o = partidos::nextIn(arr, nullptr); o; o = partidos::nextIn(arr, o)) {
      rows++;
      takeRow(o);
      char a[40];
      partidos::strOf(partidos::keyIn(o, "actualizado"), a, sizeof(a));
      if (strcmp(a, newLastPull) > 0) snprintf(newLastPull, sizeof(newLastPull), "%s", a);
    }
    free(r.body);
    if (rows < 20) return true;
  }
}

static void runJob() {
  jobOk = false;
  jobError[0] = 0;
  if (!http) {
    snprintf(jobError, sizeof(jobError), "Sin conexión con la nube");
    jobDone = true;
    return;
  }
  snprintf(message, sizeof(message), "Conectando al Wi-Fi…");
  if (wifiUp && !wifiUp()) {
    snprintf(jobError, sizeof(jobError), "No se pudo conectar al Wi-Fi");
    jobDone = true;
    return;
  }
  snprintf(message, sizeof(message), job == LOGIN ? "Entrando…" : "Sincronizando…");
  if (job == LINK) {
    snprintf(message, sizeof(message), "Esperando al móvil…");
    jobOk = waitLink();
  } else if (job == LOGIN) {
    char* e = jsonEscape(loginEmail);
    char* p = jsonEscape(loginPass);
    char body[400];
    snprintf(body, sizeof(body), "{\"email\":\"%s\",\"password\":\"%s\"}", e ? e : "", p ? p : "");
    free(e);
    free(p);
    memset(loginPass, 0, sizeof(loginPass));  // la contraseña no se queda en la memoria
    jobOk = auth("password", body);
    memset(body, 0, sizeof(body));
  } else {
    char body[300];
    snprintf(body, sizeof(body), "{\"refresh_token\":\"%s\"}", saved.refresh);
    jobOk = auth("refresh_token", body);
    // Primero bajar (para unir los jugadores que ya estén en la cuenta) y luego subir lo pendiente.
    snprintf(newLastPull, sizeof(newLastPull), "%s", saved.lastPull);
    if (jobOk) jobOk = pull(saved.lastPull);
    for (int i = 0; jobOk && i < nUpPlayers; i++) {
      // Si en la cuenta ya hay uno con su nombre, la placa lo va a unir con ese (no se sube otro igual).
      bool joins = false;
      for (int k = 0; k < nChanges && !joins; k++)
        joins = changes[k].kind == 'p' && strcmp(changes[k].id, upPlayers[i].id) && !strcasecmp(changes[k].name, upPlayers[i].name);
      if (joins) continue;
      bool d = false;
      jobOk = pushPlayer(upPlayers[i], &d);
    }
    for (int i = 0; jobOk && i < nUps; i++) {
      const char* l = ups[i].line;
      if (l[0] == 'm') jobOk = pushMatch(l + 2, &ups[i].done);
      else if (l[0] == 'b') jobOk = pushDeleted(l + 2, &ups[i].done);
      else ups[i].done = true;  // «j»: los jugadores ya van arriba
    }
    if (jobOk) for (int i = 0; i < nUps; i++) if (ups[i].line[0] == 'j') ups[i].done = true;
  }
  if (wifiDown) wifiDown();
  jobDone = true;
}

#ifdef ARDUINO
static TaskHandle_t worker = nullptr;
static void workerTask(void*) {
  for (;;) {
    if (job != NONE && !jobDone) runJob();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
#endif

// La tarea se crea al encender (su pila necesita un bloque seguido de memoria interna, que más tarde, con la
// memoria ya repartida, puede no haber).
// `external`: la pila en la PSRAM (deja libre la memoria interna, que la necesita el Wi-Fi). Solo si la tarea no
// toca la memoria del chip (con la memoria del chip escribiéndose, la PSRAM no se puede usar): los partidos van a la
// tarjeta SD y las fotos se las prepara loop().
static void startWorker(bool external = true) {
#ifdef ARDUINO
  if (worker) return;
  if (external) xTaskCreatePinnedToCoreWithCaps(workerTask, "nube", 12288, nullptr, 1, &worker, 0, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!worker) xTaskCreatePinnedToCore(workerTask, "nube", 10240, nullptr, 1, &worker, 0);
#else
  (void)external;
#endif
}

static void startJob(Job j) {
  jobDone = false;
  pendingTrimmed = false;
  job = j;
  phase = BUSY;
  snprintf(message, sizeof(message), j == LOGIN ? "Entrando…" : "Sincronizando…");
#ifdef ARDUINO
  startWorker();
  if (!worker) {  // sin tarea no se puede: se dice en vez de quedarse esperando
    snprintf(jobError, sizeof(jobError), "La placa no tiene memoria libre: reiníciala");
    jobOk = false;
    jobDone = true;
  }
#else
  runJob();
#endif
}

// ---------------------------------------------------------------- desde loop()
static bool busy() { return phase == BUSY || (nChanges > applied); }

static void begin(jug::Store* s, const char* stateDir) {
  store = s;
  snprintf(stateFile, sizeof(stateFile), "%s/nube.txt", stateDir);
  loadState();
  countPending();
  phase = loggedIn() ? IDLE : OFF;
}

// Entrar con QR: nuevo código (32 letras y números al azar) y a esperar al móvil.
static void loginWithQr() {
  if (phase == BUSY) return;
  static const char ABC[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  for (int i = 0; i < 32; i++) {
#ifdef ARDUINO
    uint32_t r = esp_random();
#else
    uint32_t r = (uint32_t)rand();
#endif
    linkCode[i] = ABC[r % (sizeof(ABC) - 1)];
  }
  linkCode[32] = 0;
  linkCancel = false;
  startJob(LINK);
}
static void cancelQr() { linkCancel = true; }
// Dirección que lleva el QR.
static void qrUrl(char* out, size_t n) { snprintf(out, n, "%s#vincular=%s", APP_URL, linkCode); }

static void login(const char* email, const char* pass) {
  if (phase == BUSY) return;
  snprintf(loginEmail, sizeof(loginEmail), "%s", email);
  snprintf(loginPass, sizeof(loginPass), "%s", pass);
  startJob(LOGIN);
}

// Prepara lo pendiente (partidos y jugadores) y lanza la sincronización.
static void sync() {
  if (phase == BUSY || !loggedIn()) return;
  free(ups);
  for (int i = 0; i < nUpPlayers; i++) free(upPlayers[i].photoUrl);
  free(upPlayers);
  ups = nullptr;
  upPlayers = nullptr;
  nUps = nUpPlayers = 0;
  char p[96];
  pendingPath(p, sizeof(p));
  if (FILE* f = fopen(p, "rb")) {
    char line[80];
    int cap = 0;
    while (fgets(line, sizeof(line), f)) {
      line[strcspn(line, "\r\n")] = 0;
      if (line[0] <= ' ') continue;
      bool dup = false;
      for (int i = 0; i < nUps; i++) dup |= !strcmp(ups[i].line, line);
      if (dup) continue;
      if (nUps == cap) {
        cap = cap ? cap * 2 : 32;
        ups = (Up*)realloc(ups, (size_t)cap * sizeof(Up));
      }
      snprintf(ups[nUps].line, sizeof(ups[0].line), "%s", line);
      ups[nUps].done = false;
      nUps++;
      if (line[0] == 'j' && store) {
        uint32_t code = (uint32_t)strtoul(line + 2, nullptr, 16);
        const jug::Player* pl = store->find(code);
        if (!pl) continue;
        upPlayers = (UpPlayer*)realloc(upPlayers, (size_t)(nUpPlayers + 1) * sizeof(UpPlayer));
        UpPlayer& u = upPlayers[nUpPlayers++];
        store->idOf(code, u.id, sizeof(u.id));
        snprintf(u.name, sizeof(u.name), "%s", pl->name);
        u.photoUrl = nullptr;
        char rel[40], path[96];
        if (store->photoRel(code, rel, sizeof(rel))) {
          snprintf(path, sizeof(path), "%s/%s", ffatRoot, rel);
          if (FILE* pf = fopen(path, "rb")) {
            uint16_t* px = (uint16_t*)malloc(fotos::PIXELS * 2);
            if (px && fseek(pf, 12, SEEK_SET) == 0 && fread(px, 2, fotos::PIXELS, pf) == fotos::PIXELS) u.photoUrl = photoDataUrl(px);
            free(px);
            fclose(pf);
          }
        }
      }
    }
    fclose(f);
  }
  nChanges = 0;
  applied = 0;
  startJob(SYNC);
}

static void logout() {
  if (phase == BUSY) return;
  memset(&saved, 0, sizeof(saved));
  saveState();
  phase = OFF;
  message[0] = 0;
}

// Ganchos para lo que hay que hacer en la placa con lo que llega (los pone el sketch).
struct Hooks {
  void (*loggedIn)() = nullptr;      // recién entrado: apuntar todo para subir
  void (*matchArrived)(const char* id) = nullptr;
  void (*matchGone)(const char* id) = nullptr;
  void (*player)(const Change& c) = nullptr;
  void (*playerGone)(const char* id) = nullptr;
  void (*finished)(bool ok) = nullptr;
};
static Hooks hooks;

static void trimPending() {
  // Lo subido sale de la lista de pendientes (lo apuntado mientras tanto se queda).
  char p[96], tmp[100];
  pendingPath(p, sizeof(p));
  snprintf(tmp, sizeof(tmp), "%s.tmp", p);
  FILE* in = fopen(p, "rb");
  FILE* out = fopen(tmp, "wb");
  if (in && out) {
    char line[80];
    while (fgets(line, sizeof(line), in)) {
      line[strcspn(line, "\r\n")] = 0;
      if (line[0] <= ' ') continue;
      bool done = false;
      for (int i = 0; i < nUps && !done; i++) done = ups[i].done && !strcmp(ups[i].line, line);
      if (!done) fprintf(out, "%s\n", line);
    }
  }
  if (in) fclose(in);
  if (out) fclose(out);
  if (in && out) {
    ::remove(p);
    rename(tmp, p);
  }
  countPending();
}

// En cada vuelta de loop(): aplica unos cuantos cambios que ya han llegado y, al acabar, guarda el estado.
static void apply() {
  // En cuanto acaba el trabajo, lo subido sale de la lista (antes de aplicar lo que llega, que puede apuntar más).
  if (phase == BUSY && jobDone && job == SYNC && !pendingTrimmed) {
    pendingTrimmed = true;
    trimPending();
  }
  int budget = 4;
  while (applied < nChanges && budget-- > 0) {
    Change& c = changes[applied++];
    if (c.kind == 'm' && hooks.matchArrived) hooks.matchArrived(c.id);
    else if (c.kind == 'x' && hooks.matchGone) hooks.matchGone(c.id);
    else if (c.kind == 'p' && hooks.player) hooks.player(c);
    else if (c.kind == 'q' && hooks.playerGone) hooks.playerGone(c.id);
    free(c.px);
    c.px = nullptr;
  }
  if (phase != BUSY || !jobDone || applied < nChanges) return;
  // Trabajo terminado y todo aplicado.
  if (job == LOGIN || job == LINK) {
    job = NONE;
    linkCode[0] = 0;
    if (!jobOk) {
      memset(&saved, 0, sizeof(saved));
      phase = OFF;
      snprintf(message, sizeof(message), "%s", jobError);
      return;
    }
    saved.lastPull[0] = 0;
    saveState();
    if (hooks.loggedIn) hooks.loggedIn();
    phase = IDLE;
    message[0] = 0;
    sync();  // y la primera sincronización, ya
    return;
  }
  if (jobOk) {
    snprintf(saved.lastPull, sizeof(saved.lastPull), "%s", newLastPull);
    lastOkAt = millis();
    message[0] = 0;
  } else {
    snprintf(message, sizeof(message), "%s", jobError);
    if (strstr(jobError, "caducado")) memset(saved.refresh, 0, sizeof(saved.refresh));
  }
  saveState();  // el refresh token cambia en cada renovación: hay que guardarlo siempre
  countPending();
  phase = !loggedIn() ? OFF : jobOk ? OK : FAILED;
  job = NONE;
  if (hooks.finished) hooks.finished(jobOk);
}

}  // namespace nube

#endif  // MFV3_S3_NUBE_H
