// MARCADOR FUTBOLÍN V3 · recursos de la pantalla de 7": capas de la app y letras Inter, desde la memoria FAT.
// -----------------------------------------------------------------------------------------------------------
// - Capas opacas (fondos y variantes): JPEG. Se descomprimen una vez a RGB565 (con tramado, para que los
//   degradados no hagan escalones) y se guardan en la PSRAM; la segunda vez salen al instante.
// - Capas con transparencia: imagen de LVGL (RGB565 + A8), las lee LVGL directamente.
// - Letras: Inter convertida con lv_font_conv (fuentes.txt), cargada al pedirla por primera vez.
// En la placa la FAT está en /ffat (FFat). En el PC, en build/placa/fat (lo prepara scripts/placa_recursos.py).

#ifndef MFV3_UI_ASSETS_H
#define MFV3_UI_ASSETS_H

#include <lvgl.h>
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
static const int MAX_CACHED = 48;
static Cached* cache[MAX_CACHED];  // cada una en su sitio fijo: LVGL guarda la dirección de su descriptor
static int cachedCount = 0;
static uint32_t useCounter = 0;
static size_t cachedBytes = 0;
static const size_t MAX_CACHED_BYTES = 10u * 1024u * 1024u;  // de 16 MB de PSRAM: 1,5 MB son las dos imágenes de pantalla

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
    ctx.dst = (uint16_t*)malloc(bytes);
    ctx.w = jd.width;
    if (ctx.dst && jd_decomp(&jd, jpegOut, 0) == JDR_OK) {
      cache[cachedCount] = new Cached();
      Cached& c = *cache[cachedCount++];
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
  size_t packed = (size_t)fileSize - 16;
  while (cachedCount && (cachedCount >= MAX_CACHED || cachedBytes + raw > MAX_CACHED_BYTES))
    if (!evictOne()) break;
  if (cachedCount >= MAX_CACHED) {
    fclose(f);
    return nullptr;
  }
  char* in = (char*)malloc(packed);
  uint8_t* out = (uint8_t*)malloc(raw);
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
  cache[cachedCount] = new Cached();
  Cached& c = *cache[cachedCount++];
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
static Font fonts[40];
static int fontCount = 0;

static const lv_font_t* font(uint16_t weight, uint16_t size, bool italic = false) {
  for (int i = 0; i < fontCount; i++)
    if (fonts[i].weight == weight && fonts[i].size == size && fonts[i].italic == italic) return fonts[i].font;
  char path[320];
  snprintf(path, sizeof(path), "A:%s/fuentes/inter-%u%s-%u.bin", ROOT, weight, italic ? "i" : "", size);
  lv_font_t* f = lv_binfont_create(path);
  if (!f) {
    LV_LOG_WARN("Falta la letra %s", path);
    return LV_FONT_DEFAULT;
  }
  if (fontCount < (int)(sizeof(fonts) / sizeof(fonts[0]))) fonts[fontCount++] = {weight, size, italic, f};
  return f;
}

}  // namespace assets

#endif  // MFV3_UI_ASSETS_H
