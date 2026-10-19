// MARCADOR FUTBOLÍN V3 · recursos de la pantalla de 7": capas de la app y letras Inter, desde la memoria FAT.
// -----------------------------------------------------------------------------------------------------------
// - Capas actuales: RGB565 o RGB565 + A8 comprimido con LZ4; se descomprimen y conservan en caché.
// - JPEG sigue disponible como alternativa. Las imágenes de pantallas anteriores se pueden expulsar.
// - Letras: Inter convertida con lv_font_conv (fuentes.txt), cargada al pedirla por primera vez.
// En la placa la FAT está en /ffat (FFat). En el PC, en build/placa/fat (lo prepara scripts/placa_recursos.py).

#ifndef MFV3_UI_ASSETS_H
#define MFV3_UI_ASSETS_H

#include <lvgl.h>
#include <math.h>
#include <new>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "libs/tjpgd/tjpgd.h"
#include "misc/cache/instance/lv_image_cache.h"
#include "ui_capas.h"

namespace assets {

#ifdef ESP_PLATFORM
static const char* ROOT = "/ffat";
#else
static const char* ROOT = "build/placa/fat";  // el PC fija la ruta con assets::setRoot()
#endif
static char rootBuf[256];
static void setRoot(const char* r) {
  snprintf(rootBuf, sizeof(rootBuf), "%s", r);
  ROOT = rootBuf;
}

// ---------------------------------------------------------------- JPEG -> RGB565 (tjpgd de LVGL)
struct JpegCtx {
  FILE* f;
  uint16_t* dst;
  int w;
};

static size_t jpegIn(JDEC* jd, uint8_t* buf, size_t n) {
  JpegCtx* c = (JpegCtx*)jd->device;
  if (buf) return fread(buf, 1, n, c->f);
  fseek(c->f, (long)n, SEEK_CUR);
  return n;
}

static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

static int jpegOut(JDEC* jd, void* bmp, JRECT* r) {
  JpegCtx* c = (JpegCtx*)jd->device;
  const uint8_t* s = (const uint8_t*)bmp;
  for (int y = r->top; y <= r->bottom; y++) {
    uint16_t* d = c->dst + (size_t)y * c->w + r->left;
    for (int x = r->left; x <= r->right; x++) {
      uint8_t t = BAYER[((y & 3) << 2) | (x & 3)];
      // El tjpgd de LVGL entrega cada píxel como lv_color_t: azul, verde, rojo.
      int B = s[0] + (t >> 1), G = s[1] + (t >> 2), R = s[2] + (t >> 1);
      if (R > 255) R = 255;
      if (G > 255) G = 255;
      if (B > 255) B = 255;
      *d++ = (uint16_t)(((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3));
      s += 3;
    }
  }
  return 1;
}

// Imágenes ya descomprimidas (por archivo). Se quedan en la PSRAM mientras quepan.
struct Cached {
  char path[48];
  lv_image_dsc_t dsc;
  uint32_t usedAt;
  bool sticky;  // precargada al encender: se borra la última
};
static const int MAX_CACHED = 96;  // capas, fotos de los jugadores en varios tamaños…
static Cached* cache[MAX_CACHED];  // cada una en su sitio fijo: LVGL guarda la dirección de su descriptor
static int cachedCount = 0;
static uint32_t useCounter = 0;
static size_t cachedBytes = 0;
// De 16 MB de PSRAM: 1,5 MB son las dos imágenes de pantalla, ~3 MB las letras y lo demás objetos, fichas y cifras
// del reloj. Con 10 MB la memoria se quedaba sin un hueco grande para los carteles y la placa podía reiniciarse.
static const size_t MAX_CACHED_BYTES = 8u * 1024u * 1024u;

// Las imágenes pedidas desde el último cambio de pantalla (`screenEpoch`) las está usando la pantalla actual:
// esas no se borran nunca (LVGL las está pintando); se borra la más antigua de las demás.
static uint32_t screenEpoch = 0;
static void newScreen() { screenEpoch = useCounter + 1; }

static bool evictOne() {
  int victim = -1;
  for (int pass = 0; pass < 2 && victim < 0; pass++)  // primero las no precargadas
    for (int i = 0; i < cachedCount; i++)
      if (cache[i]->usedAt < screenEpoch && (pass || !cache[i]->sticky) &&
          (victim < 0 || cache[i]->usedAt < cache[victim]->usedAt))
        victim = i;
  if (victim < 0) return false;
  lv_image_cache_drop(&cache[victim]->dsc);
  cachedBytes -= cache[victim]->dsc.data_size;
  free((void*)cache[victim]->dsc.data);
  delete cache[victim];
  cache[victim] = cache[--cachedCount];
  return true;
}

// Si falta un bloque contiguo, soltar únicamente imágenes que ya no usa la pantalla.
// El límite de caché sigue siendo blando: nunca expulsar una imagen que LVGL esté dibujando.
static void* allocImage(size_t bytes) {
  void* p = malloc(bytes);
  while (!p && evictOne()) p = malloc(bytes);
  return p;
}

static Cached* addCached() {
  if (cachedCount >= MAX_CACHED) return nullptr;
  Cached* c = new (std::nothrow) Cached();
  if (c) cache[cachedCount++] = c;
  return c;
}

static const lv_image_dsc_t* loadJpeg(const char* rel) {
  for (int i = 0; i < cachedCount; i++)
    if (!strcmp(cache[i]->path, rel)) {
      cache[i]->usedAt = ++useCounter;
      return &cache[i]->dsc;
    }
  char full[320];
  snprintf(full, sizeof(full), "%s/%s", ROOT, rel);
  FILE* f = fopen(full, "rb");
  if (!f) {
    LV_LOG_WARN("Falta %s", full);
    return nullptr;
  }
  uint32_t t0 = lv_tick_get();
  const size_t POOL = 16 * 1024;
  void* pool = malloc(POOL);
  JpegCtx ctx = {f, nullptr, 0};
  JDEC jd;
  const lv_image_dsc_t* result = nullptr;
  if (pool && jd_prepare(&jd, jpegIn, pool, POOL, &ctx) == JDR_OK) {
    size_t bytes = (size_t)jd.width * jd.height * 2;
    while (cachedCount && (cachedCount >= MAX_CACHED || cachedBytes + bytes > MAX_CACHED_BYTES))
      if (!evictOne()) break;  // todo lo guardado es de esta pantalla: se pasa del límite antes que romperla
    if (cachedCount >= MAX_CACHED) {
      free(pool);
      fclose(f);
      return nullptr;
    }
    ctx.dst = (uint16_t*)allocImage(bytes);
    ctx.w = jd.width;
    if (ctx.dst && jd_decomp(&jd, jpegOut, 0) == JDR_OK) {
      Cached* entry = addCached();
      if (!entry) { free(ctx.dst); free(pool); fclose(f); return nullptr; }
      Cached& c = *entry;
      snprintf(c.path, sizeof(c.path), "%s", rel);
      memset(&c.dsc, 0, sizeof(c.dsc));
      c.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
      c.dsc.header.cf = LV_COLOR_FORMAT_RGB565;
      c.dsc.header.w = jd.width;
      c.dsc.header.h = jd.height;
      c.dsc.header.stride = jd.width * 2;
      c.dsc.data = (const uint8_t*)ctx.dst;
      c.dsc.data_size = bytes;
      c.usedAt = ++useCounter;
      cachedBytes += bytes;
      result = &c.dsc;
    } else {
      free(ctx.dst);
    }
  }
  free(pool);
  fclose(f);
  // Se apunta lo que tarda (por el USB, con LV_LOG_WARN) para ver qué capas conviene precargar.
  if (lv_tick_elaps(t0) > 40) LV_LOG_WARN("JPEG %s: %lu ms", rel, (unsigned long)lv_tick_elaps(t0));
  return result;
}

// Descompresor de bloques LZ4 propio, compilado para ir rápido (el de LVGL va optimizado para ocupar poco): copia
// de 4 en 4 bytes cuando puede. Devuelve los bytes escritos o -1 si los datos no cuadran.
__attribute__((optimize("O3"))) static int lz4Unpack(const uint8_t* src, size_t n, uint8_t* dst, size_t cap) {
  const uint8_t *ip = src, *iend = src + n;
  uint8_t *op = dst, *oend = dst + cap;
  while (ip < iend) {
    uint8_t token = *ip++;
    size_t lit = token >> 4;
    if (lit == 15) {
      uint8_t b;
      do {
        if (ip >= iend) return -1;
        b = *ip++;
        lit += b;
      } while (b == 255);
    }
    if (lit) {
      if ((size_t)(iend - ip) < lit || (size_t)(oend - op) < lit) return -1;
      memcpy(op, ip, lit);
      ip += lit;
      op += lit;
    }
    if (ip >= iend) break;  // el último trozo solo trae literales
    if (iend - ip < 2) return -1;
    size_t off = ip[0] | (ip[1] << 8);
    ip += 2;
    if (off == 0 || off > (size_t)(op - dst)) return -1;
    size_t len = (token & 15) + 4;
    if ((token & 15) == 15) {
      uint8_t b;
      do {
        if (ip >= iend) return -1;
        b = *ip++;
        len += b;
      } while (b == 255);
    }
    if ((size_t)(oend - op) < len) return -1;
    const uint8_t* m = op - off;
    if (off >= len) {
      memcpy(op, m, len);
      op += len;
    } else if (off == 2 || off == 4) {  // repeticiones de un color (2 bytes) o de dos: el caso más común aquí
      uint32_t pat = off == 2 ? (uint32_t)(m[0] | (m[1] << 8)) * 0x10001u : (uint32_t)(m[0] | (m[1] << 8) | (m[2] << 16) | (m[3] << 24));
      while (len >= 4) {
        memcpy(op, &pat, 4);
        op += 4;
        len -= 4;
      }
      for (size_t i = 0; i < len; i++, op++) *op = (uint8_t)(pat >> (8 * i));
    } else {
      for (size_t i = 0; i < len; i++) op[i] = m[i];
      op += len;
    }
  }
  return (int)(op - dst);
}

// Capa en LZ4 (scripts/placa_recursos.py): cabecera de imagen de LVGL, tamaño sin comprimir y el bloque LZ4.
// Se lee y se descomprime en unos milisegundos a la PSRAM, donde se queda para las siguientes veces.
static const lv_image_dsc_t* loadLz4(const char* rel) {
  for (int i = 0; i < cachedCount; i++)
    if (!strcmp(cache[i]->path, rel)) {
      cache[i]->usedAt = ++useCounter;
      return &cache[i]->dsc;
    }
  uint32_t t0 = lv_tick_get();
  char full[320];
  snprintf(full, sizeof(full), "%s/%s", ROOT, rel);
  FILE* f = fopen(full, "rb");
  if (!f) {
    LV_LOG_WARN("Falta %s", full);
    return nullptr;
  }
  uint8_t head[16];
  fseek(f, 0, SEEK_END);
  long fileSize = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (fileSize < 16 || fread(head, 1, 16, f) != 16 || head[0] != LV_IMAGE_HEADER_MAGIC) {
    fclose(f);
    return nullptr;
  }
  uint32_t raw = (uint32_t)head[12] | ((uint32_t)head[13] << 8) | ((uint32_t)head[14] << 16) | ((uint32_t)head[15] << 24);
  uint16_t w = head[4] | (head[5] << 8), h = head[6] | (head[7] << 8), stride = head[8] | (head[9] << 8);
  bool alpha = head[1] == LV_COLOR_FORMAT_RGB565A8;
  size_t expected = (size_t)stride * h + (alpha ? (size_t)w * h : 0);
  if (!w || !h || (!alpha && head[1] != LV_COLOR_FORMAT_RGB565) || stride != (size_t)w * 2 ||
      raw != expected || raw > MAX_CACHED_BYTES) { fclose(f); return nullptr; }
  size_t packed = (size_t)fileSize - 16;
  if (!packed || packed > raw + raw / 255 + 16) { fclose(f); return nullptr; }
  while (cachedCount && (cachedCount >= MAX_CACHED || cachedBytes + raw > MAX_CACHED_BYTES))
    if (!evictOne()) break;
  if (cachedCount >= MAX_CACHED) {
    fclose(f);
    return nullptr;
  }
  char* in = (char*)malloc(packed);
  uint8_t* out = (uint8_t*)allocImage(raw);
  bool ok = in && out && fread(in, 1, packed, f) == packed;
  uint32_t tRead = lv_tick_elaps(t0);
  ok = ok && lz4Unpack((const uint8_t*)in, packed, out, raw) == (int)raw;
  fclose(f);
  free(in);
  if (!ok) {
    free(out);
    LV_LOG_WARN("No se pudo abrir %s", full);
    return nullptr;
  }
  Cached* entry = addCached();
  if (!entry) { free(out); return nullptr; }
  Cached& c = *entry;
  snprintf(c.path, sizeof(c.path), "%s", rel);
  memset(&c.dsc, 0, sizeof(c.dsc));
  c.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  c.dsc.header.cf = head[1];
  c.dsc.header.w = (uint16_t)(head[4] | (head[5] << 8));
  c.dsc.header.h = (uint16_t)(head[6] | (head[7] << 8));
  c.dsc.header.stride = (uint16_t)(head[8] | (head[9] << 8));
  c.dsc.data = out;
  c.dsc.data_size = raw;
  c.usedAt = ++useCounter;
  cachedBytes += raw;
  if (lv_tick_elaps(t0) > 40)
    LV_LOG_WARN("LZ4 %s: %lu ms (leer %lu)", rel, (unsigned long)lv_tick_elaps(t0), (unsigned long)tRead);
  return &c.dsc;
}

// Foto de jugador (120 × 120 RGB565 de la app, «jugadores/<código>.bin») ya reducida a `size` px y con las esquinas
// redondeadas en su transparencia: así LVGL solo la copia, sin escalarla ni recortarla en cada imagen (escalar y
// recortar esquinas en cada repintado era lo que más frenaba las pantallas de jugadores y del partido).
// `bottom`: también las esquinas de abajo (en la lista de libres la foto va arriba de la ficha y abajo es recta).
static const lv_image_dsc_t* photo(const char* rel, int size, int radius, bool bottom = true) {
  char key[48];
  snprintf(key, sizeof(key), "%s@%d,%d,%d", rel, size, radius, bottom ? 1 : 0);
  for (int i = 0; i < cachedCount; i++)
    if (!strcmp(cache[i]->path, key)) {
      cache[i]->usedAt = ++useCounter;
      return &cache[i]->dsc;
    }
  const int S = 120;
  if (size < 1 || size > 240) return nullptr;
  char full[320];
  snprintf(full, sizeof(full), "%s/%s", ROOT, rel);
  FILE* f = fopen(full, "rb");
  if (!f) return nullptr;
  uint16_t* in = (uint16_t*)malloc(S * S * 2);
  bool ok = in && fseek(f, 12, SEEK_SET) == 0 && fread(in, 2, S * S, f) == (size_t)(S * S);
  fclose(f);
  size_t raw = (size_t)size * size * 3;
  while (cachedCount && (cachedCount >= MAX_CACHED || cachedBytes + raw > MAX_CACHED_BYTES))
    if (!evictOne()) break;
  uint8_t* out = ok && cachedCount < MAX_CACHED ? (uint8_t*)allocImage(raw) : nullptr;
  if (!out) {
    free(in);
    return nullptr;
  }
  uint16_t* px = (uint16_t*)out;
  uint8_t* alpha = out + (size_t)size * size * 2;
  // Media de los píxeles que caen en cada uno (de 120 a menos se ve mejor que coger uno suelto).
  for (int y = 0; y < size; y++) {
    int y0 = y * S / size, y1 = ((y + 1) * S + size - 1) / size;
    for (int x = 0; x < size; x++) {
      int x0 = x * S / size, x1 = ((x + 1) * S + size - 1) / size;
      uint32_t r = 0, g = 0, b = 0, n = 0;
      for (int sy = y0; sy < y1; sy++)
        for (int sx = x0; sx < x1; sx++) {
          uint16_t v = in[sy * S + sx];
          r += v >> 11;
          g += (v >> 5) & 63;
          b += v & 31;
          n++;
        }
      px[y * size + x] = (uint16_t)(((r / n) << 11) | ((g / n) << 5) | (b / n));
      // Esquinas: cuánto del píxel cae dentro del círculo de radio `radius` (suavizado de medio píxel).
      float a = 1;
      if (radius > 0) {
        float cx = x < radius ? radius : x >= size - radius ? size - radius : -1;
        float cy = y < radius ? radius : (bottom && y >= size - radius) ? size - radius : -1;
        if (cx >= 0 && cy >= 0) {
          float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
          float d = sqrtf(dx * dx + dy * dy);
          a = radius + 0.5f - d;
          a = a < 0 ? 0 : a > 1 ? 1 : a;
        }
      }
      alpha[y * size + x] = (uint8_t)(a * 255 + 0.5f);
    }
  }
  free(in);
  Cached* entry = addCached();
  if (!entry) { free(out); return nullptr; }
  Cached& c = *entry;
  snprintf(c.path, sizeof(c.path), "%s", key);
  memset(&c.dsc, 0, sizeof(c.dsc));
  c.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  c.dsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
  c.dsc.header.w = (uint16_t)size;
  c.dsc.header.h = (uint16_t)size;
  c.dsc.header.stride = (uint16_t)(size * 2);
  c.dsc.data = out;
  c.dsc.data_size = raw;
  c.usedAt = ++useCounter;
  cachedBytes += raw;
  return &c.dsc;
}

// Capa con transparencia «pegada» sobre un color liso (el fondo opaco de la pausa): queda una imagen sin
// transparencias que LVGL solo copia, y tapa lo de debajo. Se guarda aparte; la original se suelta.
static const lv_image_dsc_t* flat(const capas::Capa& c, uint32_t bg, bool sticky = false) {
  char key[48];
  snprintf(key, sizeof(key), "%s#%06lx", c.archivo, (unsigned long)(bg & 0xFFFFFF));
  for (int i = 0; i < cachedCount; i++)
    if (!strcmp(cache[i]->path, key)) {
      cache[i]->usedAt = ++useCounter;
      return &cache[i]->dsc;
    }
  const lv_image_dsc_t* srcDsc = loadLz4(c.archivo);
  if (!srcDsc) return nullptr;
  if (srcDsc->header.cf != LV_COLOR_FORMAT_RGB565A8) return srcDsc;
  int w = srcDsc->header.w, h = srcDsc->header.h;
  size_t raw = (size_t)w * h * 2;
  while (cachedCount && (cachedCount >= MAX_CACHED || cachedBytes + raw > MAX_CACHED_BYTES))
    if (!evictOne()) break;
  if (cachedCount >= MAX_CACHED) return srcDsc;
  uint16_t* out = (uint16_t*)allocImage(raw);
  if (!out) return srcDsc;
  const uint16_t* px = (const uint16_t*)srcDsc->data;
  const uint8_t* alpha = srcDsc->data + (size_t)srcDsc->header.stride * h;
  uint32_t br = (bg >> 16) & 0xFF, bgc = (bg >> 8) & 0xFF, bb = bg & 0xFF;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      uint16_t v = px[(size_t)y * (srcDsc->header.stride / 2) + x];
      uint32_t a = alpha[(size_t)y * w + x];
      uint32_t r = ((v >> 11) & 31) * 255 / 31, g = ((v >> 5) & 63) * 255 / 63, b = (v & 31) * 255 / 31;
      r = (r * a + br * (255 - a)) / 255;
      g = (g * a + bgc * (255 - a)) / 255;
      b = (b * a + bb * (255 - a)) / 255;
      out[(size_t)y * w + x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
  Cached* entry = addCached();
  if (!entry) { free(out); return srcDsc; }
  Cached& e = *entry;
  snprintf(e.path, sizeof(e.path), "%s", key);
  memset(&e.dsc, 0, sizeof(e.dsc));
  e.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  e.dsc.header.cf = LV_COLOR_FORMAT_RGB565;
  e.dsc.header.w = (uint16_t)w;
  e.dsc.header.h = (uint16_t)h;
  e.dsc.header.stride = (uint16_t)(w * 2);
  e.dsc.data = (const uint8_t*)out;
  e.dsc.data_size = raw;
  e.usedAt = ++useCounter;
  e.sticky = sticky;
  cachedBytes += raw;
  // La original con transparencia ya no hace falta.
  for (int i = 0; i < cachedCount; i++)
    if (&cache[i]->dsc == srcDsc) {
      lv_image_cache_drop(&cache[i]->dsc);
      cachedBytes -= cache[i]->dsc.data_size;
      free((void*)cache[i]->dsc.data);
      delete cache[i];
      cache[i] = cache[--cachedCount];
      break;
    }
  return &e.dsc;
}

// Suelta imágenes guardadas (las más antiguas que no use la pantalla) hasta dejar `bytes` libres de la caché: para
// hacer sitio a algo grande (un cartel) antes de pedirlo.
// Olvida las imágenes guardadas cuya ruta empieza por `prefix` (una foto de jugador que acaba de cambiar). Solo se
// llama justo antes de rehacer la pantalla: la que las enseñaba se borra antes de volver a pintar.
static void forget(const char* prefix) {
  size_t n = strlen(prefix);
  for (int i = 0; i < cachedCount;)
    if (!strncmp(cache[i]->path, prefix, n)) {
      lv_image_cache_drop(&cache[i]->dsc);
      cachedBytes -= cache[i]->dsc.data_size;
      free((void*)cache[i]->dsc.data);
      delete cache[i];
      cache[i] = cache[--cachedCount];
    } else {
      i++;
    }
}

static void trim(size_t bytes) {
  while (cachedCount && cachedBytes + bytes > MAX_CACHED_BYTES)
    if (!evictOne()) break;
}

// Fuente para lv_image_set_src: capa ya descomprimida (LZ4 o JPEG) o ruta «A:» de una imagen de LVGL (.bin).
static const void* src(const capas::Capa& c) {
  static char path[320];
  size_t n = strlen(c.archivo);
  if (n > 4 && !strcmp(c.archivo + n - 4, ".lz4")) return loadLz4(c.archivo);
  if (n > 4 && !strcmp(c.archivo + n - 4, ".jpg")) return loadJpeg(c.archivo);
  snprintf(path, sizeof(path), "A:%s/%s", ROOT, c.archivo);
  return path;
}

// Precarga (al arrancar o antes de enseñar una pantalla) para que no haya espera al tocar. `sticky`: de las que se
// quedan siempre (las pantallas de cada partido).
static void preload(const capas::Capa& c, bool sticky = false) {
  src(c);
  if (!sticky) return;
  for (int i = 0; i < cachedCount; i++)
    if (!strcmp(cache[i]->path, c.archivo)) cache[i]->sticky = true;
}

// ---------------------------------------------------------------- letras
struct Font {
  uint16_t weight, size;
  bool italic;
  lv_font_t* font;
};
static Font fonts[48];
static int fontCount = 0;

static const lv_font_t* font(uint16_t weight, uint16_t size, bool italic = false) {
  for (int i = 0; i < fontCount; i++)
    if (fonts[i].weight == weight && fonts[i].size == size && fonts[i].italic == italic) return fonts[i].font;
  char path[320];
  snprintf(path, sizeof(path), "A:%s/fuentes/inter-%u%s-%u.bin", ROOT, weight, italic ? "i" : "", size);
  uint32_t t0 = lv_tick_get();
  // El fichero entero a la memoria de una vez y la letra se monta desde ahí: leyéndolo trozo a trozo (como hace
  // LVGL) una de las cifras grandes tardaba medio segundo.
  lv_font_t* f = nullptr;
  char full[320];
  snprintf(full, sizeof(full), "%s/fuentes/inter-%u%s-%u.bin", ROOT, weight, italic ? "i" : "", size);
  if (FILE* fp = fopen(full, "rb")) {
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    void* buf = n > 0 ? malloc((size_t)n) : nullptr;
    if (buf && fread(buf, 1, (size_t)n, fp) == (size_t)n) f = lv_binfont_create_from_buffer(buf, (uint32_t)n);
    fclose(fp);
    free(buf);  // la letra ya tiene su copia
  }
  if (!f) f = lv_binfont_create(path);
  if (lv_tick_elaps(t0) > 40) LV_LOG_WARN("Letra %u%s-%u: %lu ms", weight, italic ? "i" : "", size, (unsigned long)lv_tick_elaps(t0));
  if (!f) {
    LV_LOG_WARN("Falta la letra %s", path);
    return LV_FONT_DEFAULT;
  }
  if (fontCount < (int)(sizeof(fonts) / sizeof(fonts[0]))) fonts[fontCount++] = {weight, size, italic, f};
  return f;
}

// Todas las letras de la carpeta, cargadas al encender: la primera vez que se usa una grande (las cifras del
// marcador, ~180 KB) tardaba en abrirse y el partido se quedaba parado antes de empezar.
static void preloadFonts(void (*tick)() = nullptr) {
  char dirPath[300];
  snprintf(dirPath, sizeof(dirPath), "%s/fuentes", ROOT);
  DIR* d = opendir(dirPath);
  if (!d) return;
  while (struct dirent* e = readdir(d)) {
    unsigned w = 0, sz = 0;
    char it = 0;
    if (sscanf(e->d_name, "inter-%u-%u.bin", &w, &sz) == 2) font((uint16_t)w, (uint16_t)sz);
    else if (sscanf(e->d_name, "inter-%u%c-%u.bin", &w, &it, &sz) == 3 && it == 'i') font((uint16_t)w, (uint16_t)sz, true);
    else continue;
    if (tick) tick();
  }
  closedir(d);
}

// Tamaño (en px) con el que se pidió una letra; 0 si no es de las nuestras. Hace falta para colocar el texto en
// la línea base del navegador: las letras de solo cifras no traen ascendente y descendente de la letra entera.
static uint16_t fontSize(const lv_font_t* f) {
  for (int i = 0; i < fontCount; i++)
    if (fonts[i].font == f) return fonts[i].size;
  return 0;
}

}  // namespace assets

#endif  // MFV3_UI_ASSETS_H
