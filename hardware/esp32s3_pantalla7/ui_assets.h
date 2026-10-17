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
};
static const int MAX_CACHED = 48;
static Cached* cache[MAX_CACHED];  // cada una en su sitio fijo: LVGL guarda la dirección de su descriptor
static int cachedCount = 0;
static uint32_t useCounter = 0;
static size_t cachedBytes = 0;
static const size_t MAX_CACHED_BYTES = 8u * 1024u * 1024u;  // de 16 MB de PSRAM: 1,5 MB son las dos imágenes de pantalla

// Las imágenes pedidas desde el último cambio de pantalla (`screenEpoch`) las está usando la pantalla actual:
// esas no se borran nunca (LVGL las está pintando); se borra la más antigua de las demás.
static uint32_t screenEpoch = 0;
static void newScreen() { screenEpoch = useCounter + 1; }

static bool evictOne() {
  int victim = -1;
  for (int i = 0; i < cachedCount; i++)
    if (cache[i]->usedAt < screenEpoch && (victim < 0 || cache[i]->usedAt < cache[victim]->usedAt)) victim = i;
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
  return result;
}

// Fuente para lv_image_set_src: JPEG ya descomprimido o ruta «A:» de una imagen de LVGL (.bin).
static const void* src(const capas::Capa& c) {
  static char path[320];
  size_t n = strlen(c.archivo);
  if (n > 4 && !strcmp(c.archivo + n - 4, ".jpg")) return loadJpeg(c.archivo);
  snprintf(path, sizeof(path), "A:%s/%s", ROOT, c.archivo);
  return path;
}

// Precarga (al arrancar o antes de enseñar una pantalla) para que no haya espera al tocar.
static void preload(const capas::Capa& c) { src(c); }

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
