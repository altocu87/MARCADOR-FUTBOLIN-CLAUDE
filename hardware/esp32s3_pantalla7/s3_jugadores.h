// MARCADOR FUTBOLÍN V3 · jugadores en la pantalla de 7": los que pasa la app por el USB, con su foto.
// -------------------------------------------------------------------------------------------------
// La placa guarda en su memoria FAT la lista de jugadores (nombre, «Nv 23 · ELO 1299», título) y la foto de cada uno
// (120 × 120, imagen RGB565 de LVGL que ya prepara la app). Así puede elegir jugadores y enseñar sus fichas jugando
// sola. Cada jugador se conoce por un código de 8 cifras (FNV-1a de su id en la app, como los sonidos).
//
// Órdenes (las manda la app al conectar; ver src/inputs/hardware/boardPlayers.ts):
//   JUGADORES?                              → «JUGADORES <n> <huella>» (la app compara la huella con la suya)
//   JUGADORES_INICIO <n>                    → «JUGADORES_LISTO»
//   JUGADOR <código> <crc foto|-> <nombre>|<detalle>|<título>   (sin respuesta: van seguidas)
//   JUGADORES_FIN <huella>                  → «JUGADORES_GUARDADOS <n>» o «JUGADORES_ERROR <motivo>»
//   FOTOS?                                  → «FOTOS <código>:<crc>,…» (las que tiene)
//   FOTO_SUBIR <código> <bytes>             → «FOTO_LISTO <código>»
//   FOTO_DATOS <base64>                     → «FOTO_OK <bytes recibidos>»
//   FOTO_FIN <crc>                          → «FOTO_GUARDADA <código>» o «FOTO_ERROR <código> <motivo>»
// La huella es FNV-1a de las líneas «<código> <crc> <nombre>|<detalle>|<título>\n» en orden (la app hace lo mismo).

#ifndef MFV3_S3_JUGADORES_H
#define MFV3_S3_JUGADORES_H

#include <FS.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "mfv3_sound.h"

namespace jug {

const uint8_t MAX = 64;
const char* const DIR = "/jugadores";
const char* const LIST = "/jugadores/lista.txt";
const uint32_t PHOTO_BYTES = 12 + 120 * 120 * 2;  // cabecera de imagen de LVGL + RGB565

struct Player {
  uint32_t code;
  uint32_t photoCrc;  // 0 = sin foto
  char name[28];
  char meta[40];
  char title[32];
};

// Iniciales como la app (initials() de services/players): primera letra de las dos primeras palabras, o las dos
// primeras letras si es una sola palabra. En mayúsculas (también Á, É, Ñ…).
inline void initials(const char* name, char* out, size_t n) {
  char tmp[8] = {0};
  size_t k = 0;
  const char* p = name;
  while (*p == ' ') p++;
  const char* second = strchr(p, ' ');
  while (second && *second == ' ') second++;
  auto copyChar = [&](const char*& s) {  // un carácter UTF-8 entero
    int len = ((uint8_t)*s & 0x80) == 0 ? 1 : ((uint8_t)*s & 0xE0) == 0xC0 ? 2 : ((uint8_t)*s & 0xF0) == 0xE0 ? 3 : 4;
    for (int i = 0; i < len && *s && k < sizeof(tmp) - 1; i++) tmp[k++] = *s++;
  };
  const char* a = p;
  if (*a) copyChar(a);
  if (second && *second) copyChar(second);
  else if (*a) copyChar(a);
  // A mayúsculas: ASCII y las vocales con tilde y la ñ (UTF-8 de dos bytes 0xC3 0xA0–0xBF → 0x80–0x9F).
  for (size_t i = 0; i < k; i++) {
    if (tmp[i] >= 'a' && tmp[i] <= 'z') tmp[i] = (char)(tmp[i] - 32);
    else if ((uint8_t)tmp[i] == 0xC3 && i + 1 < k && (uint8_t)tmp[i + 1] >= 0xA0 && (uint8_t)tmp[i + 1] <= 0xBE) tmp[i + 1] -= 0x20;
  }
  snprintf(out, n, "%s", tmp);
}

class Store {
 public:
  Player list[MAX];
  uint8_t count = 0;
  uint32_t hash = 2166136261u;
  bool ok = false;

  void begin(fs::FS* fs) {
    fs_ = fs;
    ok = fs_ != nullptr;
    if (!ok) return;
    if (!fs_->exists(DIR)) fs_->mkdir(DIR);
    load();
  }

  const Player* find(uint32_t code) const {
    for (uint8_t i = 0; i < count; i++)
      if (list[i].code == code) return &list[i];
    return nullptr;
  }

  // ¿Tiene foto guardada y entera? Se comprueba en el chip una vez por jugador y se recuerda (las pantallas lo
  // preguntan en cada repaso y leer los ficheros cada vez frenaba el partido).
  bool photoOk(uint32_t code) {
    for (uint8_t i = 0; i < count; i++)
      if (list[i].code == code) {
        if (!list[i].photoCrc) return false;
        if (known[i] != list[i].photoCrc) known[i] = hasPhoto(code, list[i].photoCrc) ? list[i].photoCrc : 0;
        return known[i] != 0;
      }
    return false;
  }

  // Ruta de la foto para LVGL («A:/ffat/jugadores/1a2b3c4d.bin»), o false si no la tiene.
  bool photoPath(uint32_t code, char* out, size_t n, const char* root) {
    if (!photoOk(code)) return false;
    snprintf(out, n, "A:%s%s/%08lx.bin", root, DIR, (unsigned long)code);
    return true;
  }

  // Ruta relativa a la carpeta de recursos («jugadores/1a2b3c4d.bin»), para assets::photo().
  bool photoRel(uint32_t code, char* out, size_t n) {
    if (!photoOk(code)) return false;
    snprintf(out, n, "%s/%08lx.bin", DIR + 1, (unsigned long)code);
    return true;
  }

  bool hasPhoto(uint32_t code, uint32_t crc) {
    char path[48];
    snprintf(path, sizeof(path), "%s/%08lx.bin", DIR, (unsigned long)code);
    if (!fs_ || !fs_->exists(path)) return false;
    fs::File f = fs_->open(path, "r");
    bool ok2 = f && f.size() == PHOTO_BYTES;
    f.close();
    return ok2 && photoCrc(code) == crc;
  }

  // ---------------------------------------------------------------- órdenes
  typedef void (*Send)(const char*);

  // true si la línea era una orden de jugadores (se haya podido cumplir o no).
  bool handle(const char* line, Send send) {
    char out[96];
    if (!strcmp(line, "JUGADORES?")) {
      snprintf(out, sizeof(out), "JUGADORES %u %08lx", count, (unsigned long)hash);
      send(out);
      return true;
    }
    if (!strncmp(line, "JUGADORES_INICIO ", 17)) {
      pendingCount = 0;
      expected = (uint8_t)atoi(line + 17);
      receiving = true;
      send("JUGADORES_LISTO");
      return true;
    }
    if (!strncmp(line, "JUGADOR ", 8)) {
      if (receiving && pendingCount < MAX && parsePlayer(line + 8, pending[pendingCount])) pendingCount++;
      return true;
    }
    if (!strncmp(line, "JUGADORES_FIN ", 14)) {
      uint32_t want = 0;
      if (!receiving || !mfv3snd::parseCode(line + 14, strlen(line + 14), want)) {
        send("JUGADORES_ERROR orden");
        return true;
      }
      receiving = false;
      uint32_t h = hashOf(pending, pendingCount);
      if (pendingCount != expected || h != want) {
        send("JUGADORES_ERROR huella");
        return true;
      }
      memcpy(list, pending, sizeof(Player) * pendingCount);
      count = pendingCount;
      hash = h;
      if (!save()) {
        send("JUGADORES_ERROR memoria");
        return true;
      }
      snprintf(out, sizeof(out), "JUGADORES_GUARDADOS %u", count);
      send(out);
      changed = true;
      memset(known, 0, sizeof(known));
      return true;
    }
    if (!strcmp(line, "FOTOS?")) {
      static char all[8 + MAX * 18];
      size_t n = (size_t)snprintf(all, sizeof(all), "FOTOS ");
      bool first = true;
      for (uint8_t i = 0; i < count; i++) {
        uint32_t crc = list[i].photoCrc ? photoCrc(list[i].code) : 0;
        if (!crc) continue;
        n += (size_t)snprintf(all + n, sizeof(all) - n, "%s%08lx:%08lx", first ? "" : ",", (unsigned long)list[i].code,
                              (unsigned long)crc);
        first = false;
      }
      send(all);
      return true;
    }
    if (!strncmp(line, "FOTO_SUBIR ", 11)) {
      const char* p = line + 11;
      uint32_t code = 0;
      if (!mfv3snd::parseCode(p, 8, code) || atol(p + 9) != (long)PHOTO_BYTES || !fs_) {
        snprintf(out, sizeof(out), "FOTO_ERROR %.8s formato", p);
        send(out);
        return true;
      }
      upCode = code;
      upBytes = 0;
      upCrc = 0;
      upFile = fs_->open("/jugadores/subida.tmp", "w");
      if (!upFile) {
        snprintf(out, sizeof(out), "FOTO_ERROR %08lx memoria", (unsigned long)code);
        send(out);
        return true;
      }
      uploading = true;
      snprintf(out, sizeof(out), "FOTO_LISTO %08lx", (unsigned long)code);
      send(out);
      return true;
    }
    if (!strncmp(line, "FOTO_DATOS ", 11)) {
      uint8_t buf[300];
      int n = uploading ? mfv3snd::base64Decode(line + 11, strlen(line + 11), buf, sizeof(buf)) : -1;
      if (n < 0 || upBytes + (uint32_t)n > PHOTO_BYTES || upFile.write(buf, (size_t)n) != (size_t)n) {
        abortUpload();
        snprintf(out, sizeof(out), "FOTO_ERROR %08lx datos", (unsigned long)upCode);
        send(out);
        return true;
      }
      upBytes += (uint32_t)n;
      upCrc = mfv3snd::crc32Update(upCrc, buf, (size_t)n);
      snprintf(out, sizeof(out), "FOTO_OK %lu", (unsigned long)upBytes);
      send(out);
      return true;
    }
    if (!strncmp(line, "FOTO_FIN ", 9)) {
      uint32_t crc = 0;
      bool good = uploading && mfv3snd::parseCode(line + 9, strlen(line + 9), crc) && crc == upCrc && upBytes == PHOTO_BYTES;
      if (!good) {
        abortUpload();
        snprintf(out, sizeof(out), "FOTO_ERROR %08lx crc", (unsigned long)upCode);
        send(out);
        return true;
      }
      upFile.close();
      uploading = false;
      char path[48];
      snprintf(path, sizeof(path), "%s/%08lx.bin", DIR, (unsigned long)upCode);
      fs_->remove(path);
      fs_->rename("/jugadores/subida.tmp", path);
      setPhotoCrc(upCode, crc);
      snprintf(out, sizeof(out), "FOTO_GUARDADA %08lx", (unsigned long)upCode);
      send(out);
      changed = true;
      memset(known, 0, sizeof(known));
      return true;
    }
    return false;
  }

  // ---------------------------------------------------------------- jugadores hechos en la placa
  // (Ajustes → JUGADORES, ui_fichas.h). La foto llega ya a 120 × 120 RGB565 (s3_fotos.h); nullptr = sin foto.

  // ¿Ya hay otro jugador con ese nombre? (sin mirar mayúsculas)
  bool nameTaken(const char* name, uint32_t except = 0) const {
    for (uint8_t i = 0; i < count; i++)
      if (list[i].code != except && !strcasecmp(list[i].name, name)) return true;
    return false;
  }

  // Jugador nuevo al final de la lista. Devuelve su código (0 si no se pudo).
  uint32_t add(const char* name, const uint16_t* px) {
    if (!fs_ || count >= MAX || !name[0]) return 0;
    uint32_t code = fnv(2166136261u, name) ^ (uint32_t)millis() * 2654435761u;
    while (!code || find(code)) code = code * 16777619u + 1;
    Player& p = list[count];
    memset(&p, 0, sizeof(p));
    p.code = code;
    snprintf(p.name, sizeof(p.name), "%s", name);
    count++;
    known[count - 1] = 0;
    if (px) p.photoCrc = writePhoto(code, px);
    return commit() ? code : 0;
  }

  bool rename(uint32_t code, const char* name) {
    Player* p = (Player*)find(code);
    if (!p || !name[0]) return false;
    snprintf(p->name, sizeof(p->name), "%s", name);
    return commit();
  }

  bool setPhoto(uint32_t code, const uint16_t* px) {
    Player* p = (Player*)find(code);
    if (!p) return false;
    removePhoto(code);
    p->photoCrc = px ? writePhoto(code, px) : 0;
    memset(known, 0, sizeof(known));
    return commit();
  }

  bool remove(uint32_t code) {
    uint8_t k = 0;
    for (uint8_t i = 0; i < count; i++)
      if (list[i].code != code) list[k++] = list[i];
    if (k == count) return false;
    count = k;
    removePhoto(code);
    memset(known, 0, sizeof(known));
    return commit();
  }

  bool changed = false;  // la lista o alguna foto cambió (las pantallas se rehacen)
  uint32_t known[MAX] = {};  // CRC de la foto ya comprobada de cada jugador (0 = sin comprobar o sin foto)

 private:
  fs::FS* fs_ = nullptr;
  Player pending[MAX];
  uint8_t pendingCount = 0, expected = 0;
  bool receiving = false;
  bool uploading = false;
  uint32_t upCode = 0, upBytes = 0, upCrc = 0;
  fs::File upFile;

  static uint32_t fnv(uint32_t h, const char* s) {
    for (; *s; s++) {
      h ^= (uint8_t)*s;
      h *= 16777619u;
    }
    return h;
  }

  static void canonical(const Player& p, char* out, size_t n) {
    char crc[9] = "-";
    if (p.photoCrc) snprintf(crc, sizeof(crc), "%08lx", (unsigned long)p.photoCrc);
    snprintf(out, n, "%08lx %s %s|%s|%s\n", (unsigned long)p.code, crc, p.name, p.meta, p.title);
  }

  static uint32_t hashOf(const Player* ps, uint8_t n) {
    uint32_t h = 2166136261u;
    char line[160];
    for (uint8_t i = 0; i < n; i++) {
      canonical(ps[i], line, sizeof(line));
      h = fnv(h, line);
    }
    return h;
  }

  // «<código> <crc|-> <nombre>|<detalle>|<título>»
  static bool parsePlayer(const char* s, Player& p) {
    memset(&p, 0, sizeof(p));
    if (strlen(s) < 11 || s[8] != ' ' || !mfv3snd::parseCode(s, 8, p.code)) return false;
    const char* c = s + 9;
    const char* sp = strchr(c, ' ');
    if (!sp) return false;
    if (!(sp - c == 1 && *c == '-') && !mfv3snd::parseCode(c, (size_t)(sp - c), p.photoCrc)) return false;
    const char* f = sp + 1;
    const char* b1 = strchr(f, '|');
    const char* b2 = b1 ? strchr(b1 + 1, '|') : nullptr;
    if (!b1 || !b2) return false;
    snprintf(p.name, sizeof(p.name), "%.*s", (int)(b1 - f), f);
    snprintf(p.meta, sizeof(p.meta), "%.*s", (int)(b2 - b1 - 1), b1 + 1);
    snprintf(p.title, sizeof(p.title), "%s", b2 + 1);
    return p.name[0] != 0;
  }

  bool commit() {
    hash = hashOf(list, count);
    changed = true;
    return save();
  }

  // Guarda la foto como la manda la app (cabecera de LVGL + RGB565) y devuelve su CRC (0 si no se pudo).
  uint32_t writePhoto(uint32_t code, const uint16_t* px) {
    char path[48];
    snprintf(path, sizeof(path), "%s/%08lx.bin", DIR, (unsigned long)code);
    fs::File f = fs_->open(path, "w");
    if (!f) return 0;
    uint8_t h[12] = {0x19, 0x12, 0, 0, 120, 0, 120, 0, 240, 0, 0, 0};
    uint32_t crc = mfv3snd::crc32Update(0, h, sizeof(h));
    crc = mfv3snd::crc32Update(crc, (const uint8_t*)px, PHOTO_BYTES - 12);
    bool ok = f.write(h, sizeof(h)) == sizeof(h) && f.write((const uint8_t*)px, PHOTO_BYTES - 12) == PHOTO_BYTES - 12;
    f.close();
    if (!ok || !crc) {
      fs_->remove(path);
      return 0;
    }
    setPhotoCrc(code, crc);
    return crc;
  }

  void removePhoto(uint32_t code) {
    char path[48];
    snprintf(path, sizeof(path), "%s/%08lx.bin", DIR, (unsigned long)code);
    fs_->remove(path);
    snprintf(path, sizeof(path), "%s/%08lx.crc", DIR, (unsigned long)code);
    fs_->remove(path);
  }

  bool save() {
    if (!fs_) return false;
    fs::File f = fs_->open(LIST, "w");
    if (!f) return false;
    char line[160];
    for (uint8_t i = 0; i < count; i++) {
      canonical(list[i], line, sizeof(line));
      f.print(line);
    }
    f.close();
    return true;
  }

  void load() {
    count = 0;
    memset(known, 0, sizeof(known));
    fs::File f = fs_->open(LIST, "r");
    if (f) {
      char line[160];
      size_t n = 0;
      while (f.available() && count < MAX) {
        int c = f.read();
        if (c < 0) break;
        if (c == '\n') {
          line[n] = 0;
          if (parsePlayer(line, list[count])) count++;
          n = 0;
        } else if (n < sizeof(line) - 1) {
          line[n++] = (char)c;
        }
      }
      f.close();
    }
    hash = hashOf(list, count);
  }

  // CRC de la foto guardada, apuntado al subirla en «<código>.crc» (la lista solo dice cuál debería tener).
  uint32_t photoCrc(uint32_t code) {
    char path[48];
    snprintf(path, sizeof(path), "%s/%08lx.crc", DIR, (unsigned long)code);
    fs::File f = fs_->open(path, "r");
    if (!f) return 0;
    char b[9] = {0};
    size_t n = f.read((uint8_t*)b, 8);
    f.close();
    uint32_t v = 0;
    return n == 8 && mfv3snd::parseCode(b, 8, v) ? v : 0;
  }

  void setPhotoCrc(uint32_t code, uint32_t crc) {
    char path[48], b[9];
    snprintf(path, sizeof(path), "%s/%08lx.crc", DIR, (unsigned long)code);
    snprintf(b, sizeof(b), "%08lx", (unsigned long)crc);
    fs::File f = fs_->open(path, "w");
    if (f) {
      f.write((const uint8_t*)b, 8);
      f.close();
    }
  }

  void abortUpload() {
    if (uploading) upFile.close();
    uploading = false;
    if (fs_) fs_->remove("/jugadores/subida.tmp");
  }
};

}  // namespace jug

#endif  // MFV3_S3_JUGADORES_H
