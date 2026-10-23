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

#include <atomic>
#include <errno.h>
#include "s3_memory.h"
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
static Saved jobSaved;  // copia privada de la tarea; se publica al terminar

// Preferencia local, independiente de la copia de trabajo de la tarea. Las cuatro líneas antiguas
// siguen siendo legibles; la quinta es opcional. Sin recordar, el permiso solo vive en RAM.
static bool rememberSession = true;
static void recoverState() {
  char backup[104]; snprintf(backup, sizeof(backup), "%s.bak", stateFile);
  FILE* f = fopen(stateFile, "rb");
  if (f) { fclose(f); ::remove(backup); }
  else if (errno == ENOENT) rename(backup, stateFile);
}
static bool saveState() {
  recoverState();
  char tmp[104], backup[104];
  snprintf(tmp, sizeof(tmp), "%s.tmp", stateFile);
  snprintf(backup, sizeof(backup), "%s.bak", stateFile);
  FILE* f = fopen(tmp, "wb");
  if (!f) return false;
  bool ok = fprintf(f, "%s\n%s\n%s\n%s\n%d\n", saved.email, saved.userId,
                    rememberSession ? saved.refresh : "", saved.lastPull, rememberSession ? 1 : 0) >= 0;
  ok = fclose(f) == 0 && ok;
  bool hadOriginal = false;
  if (ok) {
    hadOriginal = rename(stateFile, backup) == 0;
    if (!hadOriginal && errno != ENOENT) ok = false;
  }
  if (ok) {
    if (rename(tmp, stateFile) != 0) {
      if (hadOriginal) rename(backup, stateFile);
      ok = false;
    } else ::remove(backup);
  }
  if (!ok) ::remove(tmp);
  return ok;
}
static void loadState() {
  rememberSession = true;
  recoverState();
  memset(&saved, 0, sizeof(saved));
  FILE* f = fopen(stateFile, "rb");
  if (!f) return;
  char* fields[4] = {saved.email, saved.userId, saved.refresh, saved.lastPull};
  size_t sizes[4] = {sizeof(saved.email), sizeof(saved.userId), sizeof(saved.refresh), sizeof(saved.lastPull)};
  for (int i = 0; i < 4; i++) {
    if (!fgets(fields[i], (int)sizes[i], f)) break;
    fields[i][strcspn(fields[i], "\r\n")] = 0;
  }
  char remember[8];
  if (fgets(remember, sizeof(remember), f)) rememberSession = remember[0] != '0';
  if (!rememberSession) saved.refresh[0] = 0;
  fclose(f);
}
static bool loggedIn() { return saved.refresh[0] != 0; }

// ---------------------------------------------------------------- lo que ve la pantalla
enum Phase : uint8_t { OFF, IDLE, BUSY, OK, FAILED };
static uint8_t phase = OFF;
static char message[96] = "";
static uint32_t lastOkAt = 0;  // millis() de la última sincronización buena
static int pendingCount = 0;   // cosas por subir

static bool setRememberSession(bool remember) {
  bool old = rememberSession;
  rememberSession = remember;
  // Durante QR/sincronización la tarea de red usa PSRAM y puede leer la tarjeta.
  // Solo cambiar la intención en RAM; apply() la guarda cuando esa tarea ha terminado.
  if (phase == BUSY) return true;
  if (saveState()) return true;
  rememberSession = old;
  return false;
}

// ---------------------------------------------------------------- pendientes (los lleva loop(), no la tarea)
static jug::Store* store = nullptr;
static void pendingPath(char* out, size_t n) { snprintf(out, n, "%s/subir.txt", partidos::dir); }

// Recupera el original si se interrumpió la sustitución de la cola. Se llama desde loop().
static void recoverPending() {
  char p[96], backup[104];
  pendingPath(p, sizeof(p));
  snprintf(backup, sizeof(backup), "%s.bak", p);
  FILE* f = fopen(p, "rb");
  if (f) { fclose(f); ::remove(backup); }
  else if (errno == ENOENT) rename(backup, p);
}

static void countPending() {
  recoverPending();
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
  recoverPending();
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
  if (!px) return nullptr;
  const int S = 120, row = S * 3;
  const size_t bmpLen = 54 + (size_t)row * S;
  static const char prefix[] = "data:image/bmp;base64,";
  char* out = (char*)malloc(sizeof(prefix) + (bmpLen + 2) / 3 * 4);
  if (!out) return nullptr;
  // Cabecera y fila son múltiplos de tres: se concatenan sus bloques base64 sin relleno intermedio.
  uint8_t header[54] = {}, pixels[row];
  auto le32 = [&](int at, uint32_t v) {
    for (int i = 0; i < 4; i++) header[at + i] = (uint8_t)(v >> (8 * i));
  };
  header[0] = 'B'; header[1] = 'M';
  le32(2, (uint32_t)bmpLen);
  le32(10, 54); le32(14, 40);
  le32(18, S); le32(22, S);
  header[26] = 1; header[28] = 24;
  le32(34, (uint32_t)(row * S));
  memcpy(out, prefix, sizeof(prefix) - 1);
  size_t k = sizeof(prefix) - 1;
  k += base64Encode(header, sizeof(header), out + k);
  for (int y = S - 1; y >= 0; --y) {  // BMP conserva el orden de abajo arriba
    for (int x = 0; x < S; ++x) {
      uint16_t c = px[y * S + x];
      pixels[x * 3] = (uint8_t)((c & 31) << 3);
      pixels[x * 3 + 1] = (uint8_t)(((c >> 5) & 63) << 2);
      pixels[x * 3 + 2] = (uint8_t)((c >> 11) << 3);
    }
    k += base64Encode(pixels, sizeof(pixels), out + k);
  }
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
static int nChanges = 0;
static int capChanges = 0, applied = 0;
static bool pushChange(const Change& c) {
  if (!s3mem::reserve(changes, capChanges, nChanges + 1, 64)) return false;
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
static long pendingBytes = 0;  // solo se retiran las líneas incluidas en esta copia
// Jugadores para subir: lo que hace falta de cada uno, copiado por loop() antes de empezar.
struct UpPlayer {
  char id[48];
  char name[28];
  char* photoUrl;  // «data:image/bmp;base64,…» ya hecha por loop() (la tarea no lee la memoria del chip) o nullptr
};
static UpPlayer* upPlayers = nullptr;
static int nUpPlayers = 0;

enum Job : uint8_t { NONE, LOGIN, SYNC, LINK, PROFILE };
// Perfil público de un jugador (su QR; s3_perfil.h): no hace falta cuenta, va con la clave pública.
static char* profileBody = nullptr;
static int profileResult = 0;  // 0 sin terminar, 1 publicado, -1 no se pudo
static uint8_t profileReturn = 0;  // fase y aviso de la cuenta de antes (se dejan igual)
static char profileMessage[96] = "";
// Entrar con QR: código aleatorio que enseña la placa (y su dirección de la app) y si se ha dejado de esperar.
static const char* APP_URL = "https://altocu87.github.io/MARCADOR-FUTBOLIN-CLAUDE/app/";
static char linkCode[40] = "";
static std::atomic<bool> linkCancel{false};
static std::atomic<uint8_t> job{NONE};
static char loginEmail[96], loginPass[96];
static char access[1600];
static char newLastPull[40];
static char jobError[96];
static std::atomic<bool> jobDone{false};
static std::atomic<bool> jobReady{false};
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
      snprintf(jobSaved.refresh, sizeof(jobSaved.refresh), "%s", ref);
      if (uid[0]) snprintf(jobSaved.userId, sizeof(jobSaved.userId), "%s", uid);
      if (mail[0]) snprintf(jobSaved.email, sizeof(jobSaved.email), "%s", mail);
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
      snprintf(jobSaved.refresh, sizeof(jobSaved.refresh), "%s", ref);
      snprintf(jobSaved.userId, sizeof(jobSaved.userId), "%s", uid);
      snprintf(jobSaved.email, sizeof(jobSaved.email), "%s", mail);
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

// ---------------------------------------------------------------- equipos fijos (en las preferencias de la mesa)
// La app los guarda en la fila «prefs»/«mesa», campo «clubs» (src/services/cloud/sync.ts). La tarea solo mueve
// texto: lo baja a clubsDown y sube clubsUp (los prepara y los aplica loop(), que es quien toca la memoria del chip).
static char* clubsUp = nullptr;    // equipos de la placa para subir (nullptr: nada que subir)
static char* clubsDown = nullptr;  // equipos que llegan de la cuenta
static char* clubsGoneIds = nullptr;  // ids borrados en la placa («id\n…»), para no recuperarlos de la cuenta
static bool clubsPushedOk = false;
static char* (*clubsOut)() = nullptr;          // loop(): los equipos en JSON si hay cambios sin subir
static char* (*clubsGone)() = nullptr;         // loop(): los borrados aquí sin subir
static void (*clubsIn)(const char*) = nullptr;  // loop(): llegaron de la cuenta
static void (*clubsDone)() = nullptr;           // loop(): ya están en la cuenta

static bool post(const char* body);

// Los de la placa más los de la cuenta que la placa no tiene (ni ha borrado, ni son de una pareja que ya tiene).
// Así un equipo creado en la app mientras tanto no se pierde. malloc.
static bool hasText(const char* hay, const char* needle) { return hay && needle[0] && strstr(hay, needle); }
static char* mergeClubs(const char* mine, const char* cloud) {
  size_t cap = strlen(mine) + (cloud ? strlen(cloud) : 0) + 8;
  char* out = (char*)malloc(cap);
  if (!out) return nullptr;
  size_t k = strlen(mine);
  memcpy(out, mine, k);
  while (k && out[k - 1] != ']') k--;
  if (k) k--;  // sin el «]»
  bool empty = k <= 1;
  if (cloud && *cloud == '[')
    for (const char* o = partidos::nextIn(cloud, nullptr); o; o = partidos::nextIn(cloud, o)) {
      char id[60], key[80], a[48] = "", b[48] = "";
      partidos::strOf(partidos::keyIn(o, "id"), id, sizeof(id));
      const char* ids = partidos::keyIn(o, "playerIds");
      if (ids && *ids == '[') {
        const char* q = strchr(ids, '"');
        if (q) {
          partidos::strOf(q, a, sizeof(a));
          q = strchr(partidos::skipStr(q), '"');
          if (q && q < partidos::closeOf(ids)) partidos::strOf(q, b, sizeof(b));
        }
      }
      if (strcmp(a, b) > 0) { char t[48]; memcpy(t, a, 48); memcpy(a, b, 48); memcpy(b, t, 48); }
      snprintf(key, sizeof(key), "\"id\":\"%s\"", id);
      if (!id[0] || hasText(mine, key)) continue;
      snprintf(key, sizeof(key), "%s\n", id);
      if (clubsGoneIds && !strncmp(clubsGoneIds, key, strlen(key))) continue;
      snprintf(key, sizeof(key), "\n%s\n", id);
      if (hasText(clubsGoneIds, key)) continue;
      char pair[110];
      snprintf(pair, sizeof(pair), "[\"%s\",\"%s\"]", a, b);
      if (hasText(mine, pair)) continue;
      const char* end = partidos::closeOf(o);
      size_t n = (size_t)(end - o + 1);
      if (k + n + 4 > cap) break;
      if (!empty) out[k++] = ',';
      memcpy(out + k, o, n);
      k += n;
      empty = false;
    }
  out[k++] = ']';
  out[k] = 0;
  return out;
}

// Sube los equipos sin tocar lo demás de las preferencias: se lee la fila, se cambia solo «clubs» y se guarda.
static bool pushClubs() {
  char url[200];
  snprintf(url, sizeof(url), "%s/rest/v1/mesa_datos?select=datos&tipo=eq.prefs&id=eq.mesa", URL);
  Resp r = http("GET", url, access, nullptr, nullptr);
  if (r.code != 200 || !r.body) {
    snprintf(jobError, sizeof(jobError), r.code ? "No se pudieron leer los equipos (%d)" : "Sin conexión con la nube", r.code);
    free(r.body);
    return false;
  }
  const char* arr = r.body;
  while (*arr && *arr != '[') arr++;
  const char* row = partidos::nextIn(arr, nullptr);
  const char* datos = row ? partidos::keyIn(row, "datos") : nullptr;
  const char* cloudClubs = datos && *datos == '{' ? partidos::keyIn(datos, "clubs") : nullptr;
  char* merged = mergeClubs(clubsUp, cloudClubs);
  if (!merged) {
    free(r.body);
    snprintf(jobError, sizeof(jobError), "Sin memoria para subir los equipos");
    return false;
  }
  size_t up = strlen(merged), cap = up + 120 + (datos ? strlen(datos) : 0);
  char* body = (char*)malloc(cap);
  bool ok = false;
  if (body) {
    int k = snprintf(body, cap, "[{\"tipo\":\"prefs\",\"id\":\"mesa\",\"borrado\":false,\"datos\":");
    if (datos && *datos == '{') {
      const char* end = partidos::closeOf(datos);
      const char* old = partidos::keyIn(datos, "clubs");
      if (old && (*old == '[' || *old == 'n')) {  // se cambia su valor
        const char* oldEnd = *old == '[' ? partidos::closeOf(old) + 1 : old + 4;
        k += snprintf(body + k, cap - k, "%.*s%s%.*s", (int)(old - datos), datos, merged, (int)(end - oldEnd + 1), oldEnd);
      } else {  // no estaba: delante de lo demás
        const char* rest = datos + 1;
        while (*rest == ' ' || *rest == '\n') rest++;
        k += snprintf(body + k, cap - k, "{\"clubs\":%s%s%.*s", merged, *rest == '}' ? "" : ",", (int)(end - rest + 1), rest);
      }
    } else {
      k += snprintf(body + k, cap - k, "{\"clubs\":%s}", merged);
    }
    snprintf(body + k, cap - k, "}]");
    ok = post(body);
  }
  free(body);
  free(r.body);
  // loop() se queda con la lista unida (la de la cuenta ya es esta).
  free(clubsDown);
  clubsDown = ok ? merged : nullptr;
  if (!ok) free(merged);
  return ok;
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
    // readAll también falla por memoria o lectura: solo descartar un archivo realmente ausente.
    FILE* exists = fopen(path, "rb");
    bool missing = !exists && errno == ENOENT;
    if (exists) fclose(exists);
    *done = missing;
    if (!missing) snprintf(jobError, sizeof(jobError), "No se pudo leer el partido; se conserva pendiente");
    return missing;
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
static bool takeRow(const char* o) {
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
      return pushChange(c);
    }
    if (!datos || *datos != '{') return false;
    FILE* test = fopen(path, "rb");
    if (test) {  // ya lo tenemos (los partidos no cambian)
      fclose(test);
      return true;
    }
    const char* end = partidos::closeOf(datos);
    if (!end || *end != '}') return false;
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t len = (size_t)(end - datos + 1);
    bool written = fwrite(datos, 1, len, f) == len;
    written = fputc('\n', f) != EOF && written;
    written = fclose(f) == 0 && written;
    c.kind = 'm';
    if (!written || !pushChange(c)) {
      ::remove(path);  // creado por esta descarga: permitir el reintento completo
      return false;
    }
    return true;
  } else if (!strcmp(tipo, "prefs")) {
    const char* clubs = !del && !strcmp(id, "mesa") && datos && *datos == '{' ? partidos::keyIn(datos, "clubs") : nullptr;
    if (clubs && *clubs == '[') {
      const char* end = partidos::closeOf(clubs);
      size_t n = (size_t)(end - clubs + 1);
      char* copy = (char*)malloc(n + 1);
      if (!copy) return false;
      memcpy(copy, clubs, n);
      copy[n] = 0;
      free(clubsDown);
      clubsDown = copy;
    }
    return true;
  } else if (!strcmp(tipo, "player")) {
    if (del) {
      c.kind = 'q';
      return pushChange(c);
    }
    if (!datos || *datos != '{') return false;
    const char* act = partidos::keyIn(datos, "active");
    if (act && !partidos::isTrue(act)) {
      c.kind = 'q';  // inactivo en la app: fuera de la lista de la placa
      return pushChange(c);
    }
    partidos::strOf(partidos::keyIn(datos, "name"), c.name, sizeof(c.name));
    const char* ph = partidos::keyIn(datos, "photo");
    if (ph && *ph == '"' && ph[1] != '"') {
      const char* e = partidos::skipStr(ph);
      if (e - ph < 2 || e[-1] != '"') return false;
      uint16_t* px = (uint16_t*)malloc(fotos::PIXELS * 2);
      if (px && photoFromDataUrl(ph + 1, (size_t)(e - ph - 2), px)) c.px = px;
      else { free(px); return false; }
    }
    c.kind = 'p';
    if (!pushChange(c)) { free(c.px); return false; }
  }
  return true;
}

static bool pull(const char* since) {
  for (int offset = 0;; offset += 20) {
    char url[400];
    snprintf(url, sizeof(url),
             "%s/rest/v1/mesa_datos?select=tipo,id,datos,borrado,actualizado&tipo=in.(player,match,prefs)&order=actualizado.asc,tipo.asc,id.asc&limit=20&offset=%d%s%s",
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
      if (!takeRow(o)) {
        snprintf(jobError, sizeof(jobError), "No se pudo guardar lo recibido; se reintentará");
        free(r.body);
        return false;
      }
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

  if (wifiUp && !wifiUp()) {
    snprintf(jobError, sizeof(jobError), "No se pudo conectar al Wi-Fi");
    if (wifiDown) wifiDown();
    jobDone = true;
    return;
  }

  if (job == PROFILE) {
    char url[160];
    snprintf(url, sizeof(url), "%s/rest/v1/rpc/publicar_perfil_jugador", URL);
    Resp r = http("POST", url, ANON, nullptr, profileBody);
    free(r.body);
    jobOk = r.code >= 200 && r.code < 300;
    if (!jobOk) snprintf(jobError, sizeof(jobError), r.code ? "No se pudo publicar (%d)" : "Sin conexión", r.code);
  } else if (job == LINK) {

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
    snprintf(body, sizeof(body), "{\"refresh_token\":\"%s\"}", jobSaved.refresh);
    jobOk = auth("refresh_token", body);
    // Primero bajar (para unir los jugadores que ya estén en la cuenta) y luego subir lo pendiente.
    snprintf(newLastPull, sizeof(newLastPull), "%s", jobSaved.lastPull);
    if (jobOk) jobOk = pull(jobSaved.lastPull);
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
    if (jobOk && clubsUp) jobOk = clubsPushedOk = pushClubs();
  }
  if (wifiDown) wifiDown();
  jobDone = true;
}

#ifdef ARDUINO
static TaskHandle_t worker = nullptr;
static void workerTask(void*) {
  for (;;) {
    if (jobReady.exchange(false)) runJob();
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
  jobSaved = saved;
  phase = BUSY;
  snprintf(message, sizeof(message), j == LINK ? "Esperando al móvil…" : j == LOGIN ? "Entrando…" : "Sincronizando…");
  job = j;
#ifdef ARDUINO
  startWorker();
  if (worker) jobReady.store(true);
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
static bool busy() { return phase == BUSY; }

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

// Memoria temporal de una sincronización: también se libera al terminar, no en la siguiente.
static void freeUploads() {
  for (int i = 0; i < nUpPlayers; i++) free(upPlayers[i].photoUrl);
  free(upPlayers);
  free(ups);
  upPlayers = nullptr;
  ups = nullptr;
  nUps = nUpPlayers = 0;
}

// Prepara lo pendiente sin perder la cola persistente cuando no cabe en memoria.
// Publicar el perfil de un jugador (el texto es de la tarea hasta que acaba). false si la red está ocupada.
static bool publishProfile(char* body) {
  if (phase == BUSY || !body) {
    free(body);
    return false;
  }
  free(profileBody);
  profileBody = body;
  profileResult = 0;
  profileReturn = phase;
  snprintf(profileMessage, sizeof(profileMessage), "%s", message);
  nChanges = applied = 0;
  Saved keep = saved;
  startJob(PROFILE);
  saved = keep;
  return true;
}

// Modo prueba (s3_datos.h): no se sube ni se baja nada; lo de prueba no llega nunca a la cuenta.
static bool paused = false;
static void sync() {
  if (paused || phase == BUSY || !loggedIn()) return;
  freeUploads();
  recoverPending();
  pendingBytes = 0;
  char p[96];
  pendingPath(p, sizeof(p));
  bool ok = true;
  if (FILE* f = fopen(p, "rb")) {
    char line[80];
    int cap = 0, playerCap = 0;
    while (ok && fgets(line, sizeof(line), f)) {
      line[strcspn(line, "\r\n")] = 0;
      if (line[0] <= ' ') continue;
      bool dup = false;
      for (int i = 0; i < nUps; i++) dup |= !strcmp(ups[i].line, line);
      if (dup) continue;
      if (!s3mem::reserve(ups, cap, nUps + 1, 32)) { ok = false; break; }
      snprintf(ups[nUps].line, sizeof(ups[0].line), "%s", line);
      ups[nUps++].done = false;
      if (line[0] != 'j' || !store) continue;
      uint32_t code = (uint32_t)strtoul(line + 2, nullptr, 16);
      const jug::Player* pl = store->find(code);
      if (!pl) continue;
      if (!s3mem::reserve(upPlayers, playerCap, nUpPlayers + 1)) { ok = false; break; }
      UpPlayer& u = upPlayers[nUpPlayers++];
      u.photoUrl = nullptr;
      store->idOf(code, u.id, sizeof(u.id));
      snprintf(u.name, sizeof(u.name), "%s", pl->name);
      char rel[40], path[96];
      if (store->photoRel(code, rel, sizeof(rel))) {
        snprintf(path, sizeof(path), "%s/%s", ffatRoot, rel);
        FILE* pf = fopen(path, "rb");
        uint16_t* px = (uint16_t*)malloc(fotos::PIXELS * 2);
        if (pf && px && fseek(pf, 12, SEEK_SET) == 0 && fread(px, 2, fotos::PIXELS, pf) == fotos::PIXELS)
          u.photoUrl = photoDataUrl(px);
        free(px);
        if (pf) fclose(pf);
        // No subir una ficha sin foto por un fallo temporal: podría borrar la foto de la nube.
        if (!u.photoUrl) ok = false;
      }
    }
    pendingBytes = ftell(f);
    if (ferror(f) || pendingBytes < 0) ok = false;
    fclose(f);
  }
  if (!ok) {
    freeUploads();
    phase = FAILED;
    snprintf(message, sizeof(message), "No se pudo preparar la sincronización; se conserva lo pendiente");
    return;
  }
  nChanges = applied = 0;
  free(clubsUp);
  free(clubsDown);
  clubsDown = nullptr;
  clubsPushedOk = false;
  clubsUp = clubsOut ? clubsOut() : nullptr;
  free(clubsGoneIds);
  clubsGoneIds = clubsUp && clubsGone ? clubsGone() : nullptr;
  startJob(SYNC);
}

static bool logout() {
  if (phase == BUSY) return false;
  Saved before = saved;
  memset(&saved, 0, sizeof(saved));
  if (!saveState()) { saved = before; return false; }
  memset(&jobSaved, 0, sizeof(jobSaved));
  memset(access, 0, sizeof(access));
  phase = OFF;
  message[0] = 0;
  return true;
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
  // Retirar solo lo confirmado; ante errores conservar el original (un reenvío es idempotente).
  char p[96], tmp[104], backup[104];
  pendingPath(p, sizeof(p));
  snprintf(tmp, sizeof(tmp), "%s.tmp", p);
  snprintf(backup, sizeof(backup), "%s.bak", p);
  FILE* in = fopen(p, "rb");
  if (!in) { countPending(); return; }
  FILE* out = fopen(tmp, "wb");
  if (!out) { fclose(in); countPending(); return; }
  bool ok = true;
  char line[80];
  while (fgets(line, sizeof(line), in)) {
    line[strcspn(line, "\r\n")] = 0;
    if (line[0] <= ' ') continue;
    bool done = false;
    long position = ftell(in);
    if (position < 0) { ok = false; break; }
    if (position <= pendingBytes)
      for (int i = 0; i < nUps && !done; i++) done = ups[i].done && !strcmp(ups[i].line, line);
    if (!done && fprintf(out, "%s\n", line) < 0) { ok = false; break; }
  }
  ok = !ferror(in) && ok;
  ok = fclose(in) == 0 && ok;
  ok = fclose(out) == 0 && ok;
  if (ok) {
    // FAT no reemplaza un destino existente: conservar copia hasta confirmar ambos renombrados.
    if (rename(p, backup) == 0) {
      if (rename(tmp, p) == 0) ::remove(backup);
      else rename(backup, p);
    }
  }
  ::remove(tmp);
  countPending();
}

// La tarea conserva la propiedad de changes y sus fotos hasta publicar jobDone.
// Después loop() aplica como máximo cuatro cambios por vuelta, sin realloc concurrente.
static void apply() {
  if (phase != BUSY || !jobDone.load()) return;
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
  // Equipos: lo subido manda; si no había nada que subir, lo de la cuenta sustituye a lo de aquí.
  if (job == SYNC) {
    if (clubsUp && clubsPushedOk) {
      if (clubsDown && clubsIn) clubsIn(clubsDown);
      if (clubsDone) clubsDone();
    } else if (!clubsUp && clubsDown && clubsIn) clubsIn(clubsDown);
  }
  free(clubsGoneIds);
  clubsGoneIds = nullptr;
  free(clubsUp);
  free(clubsDown);
  clubsUp = clubsDown = nullptr;
  clubsPushedOk = false;
  // Trabajo terminado y todo aplicado.
  saved = jobSaved;
  freeUploads();
  free(changes);
  changes = nullptr;
  nChanges = capChanges = applied = 0;
  if (job == PROFILE) {  // no toca la cuenta ni lo pendiente
    free(profileBody);
    profileBody = nullptr;
    profileResult = jobOk ? 1 : -1;
    job = NONE;
    phase = profileReturn;
    snprintf(message, sizeof(message), "%s", profileMessage);
    return;
  }
  if (job == LOGIN || job == LINK) {
    job = NONE;
    linkCode[0] = 0;
    if (!jobOk) {
      memset(&saved, 0, sizeof(saved));
      saveState();  // también conservar la opción elegida si se canceló/falló el QR
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
