// MARCADOR FUTBOLÍN V3 · fotos de la tarjeta SD para las fichas de los jugadores (pantalla de 7").
// -------------------------------------------------------------------------------------------------
// Las fotos .jpg de la carpeta FOTOS de la tarjeta se abren reducidas (1/2, 1/4 o 1/8 al descomprimir: una foto
// de móvil de 12 Mpx no cabe ni se abre a tiempo entera), se recortan al centro, cuadradas, y se dejan a 120 × 120
// RGB565 con el mismo tramado que la app (src/inputs/hardware/boardPlayers.ts). Si la foto dice que va girada
// (EXIF de los móviles), se endereza.
// Abrir una foto grande tarda 1–3 s: en la placa se hace en una tarea aparte (núcleo 0) para que la pantalla siga
// respondiendo; en el PC, al momento.

#ifndef MFV3_S3_FOTOS_H
#define MFV3_S3_FOTOS_H

#include "s3_memory.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "fotojpg.h"

namespace fotos {

const int SIZE = 120;
const int MAX_FILES = 60;
const size_t PIXELS = (size_t)SIZE * SIZE;

#ifdef ARDUINO
static const char* dirPath = "/sdcard/FOTOS";
#else
static const char* dirPath = "../build/placa/fotos-sd";  // el PC pone aquí unas fotos de prueba
#endif

static MFV3_UI_DATA char names[MAX_FILES][64];
static int count = 0;

static bool isJpeg(const char* n) {
  size_t l = strlen(n);
  if (n[0] == '.') return false;  // «._foto.jpg» de los Mac y ocultos
  return (l > 4 && !strcasecmp(n + l - 4, ".jpg")) || (l > 5 && !strcasecmp(n + l - 5, ".jpeg"));
}

// Lista de fotos de la carpeta, en orden alfabético. false si no hay carpeta (o tarjeta).
static bool scan() {
  count = 0;
  DIR* d = opendir(dirPath);
  if (!d) return false;
  while (struct dirent* e = readdir(d)) {
    if (!isJpeg(e->d_name) || strlen(e->d_name) >= sizeof(names[0])) continue;
    if (count < MAX_FILES) snprintf(names[count++], sizeof(names[0]), "%s", e->d_name);
  }
  closedir(d);
  qsort(names, count, sizeof(names[0]), [](const void* a, const void* b) { return strcasecmp((const char*)a, (const char*)b); });
  return true;
}

// ---------------------------------------------------------------- EXIF: cómo va girada la foto (1 = derecha)
static uint16_t rd16(const uint8_t* p, bool le) { return le ? p[0] | p[1] << 8 : p[0] << 8 | p[1]; }
static uint32_t rd32(const uint8_t* p, bool le) {
  return le ? p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24 : (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}
static int orientation(FILE* f) {
  uint8_t h[4];
  if (fread(h, 1, 2, f) != 2 || h[0] != 0xFF || h[1] != 0xD8) return 1;
  for (int seg = 0; seg < 16; seg++) {
    if (fread(h, 1, 4, f) != 4 || h[0] != 0xFF) return 1;
    uint16_t len = (uint16_t)(h[2] << 8 | h[3]);
    if (len < 2) return 1;
    if (h[1] == 0xDA) return 1;  // empiezan los datos de la imagen
    if (h[1] != 0xE1 || len < 16) {
      fseek(f, len - 2, SEEK_CUR);
      continue;
    }
    size_t n = len - 2 > 4096 ? 4096 : len - 2;
    uint8_t* b = (uint8_t*)malloc(n);
    if (!b) return 1;
    int o = 1;
    if (fread(b, 1, n, f) == n && !memcmp(b, "Exif\0\0", 6)) {
      const uint8_t* t = b + 6;
      size_t tn = n - 6;
      bool le = t[0] == 'I';
      uint32_t ifd = rd32(t + 4, le);
      if (ifd + 2 <= tn) {
        uint16_t entries = rd16(t + ifd, le);
        for (uint16_t i = 0; i < entries && ifd + 2 + (i + 1) * 12u <= tn; i++) {
          const uint8_t* e = t + ifd + 2 + i * 12;
          if (rd16(e, le) == 0x0112) {
            o = rd16(e + 8, le);
            break;
          }
        }
      }
    }
    free(b);
    return o >= 1 && o <= 8 ? o : 1;
  }
  return 1;
}

// ---------------------------------------------------------------- descomprimir y reducir
struct Dec {
  FILE* f;
  int side, x0, y0;     // recorte cuadrado (en píxeles ya reducidos)
  uint16_t* sum;        // R, G, B sumados por cada píxel de destino
  uint16_t* n;          // cuántos píxeles han caído en cada uno
};

static size_t decIn(FJDEC* jd, uint8_t* buf, size_t n) {
  Dec* d = (Dec*)jd->device;
  if (buf) return fread(buf, 1, n, d->f);
  return fseek(d->f, (long)n, SEEK_CUR) == 0 ? n : 0;
}

static int decOut(FJDEC* jd, void* bmp, FJRECT* r) {
  Dec* d = (Dec*)jd->device;
  const uint8_t* s = (const uint8_t*)bmp;
  for (int y = r->top; y <= r->bottom; y++) {
    int ty = (y - d->y0) * SIZE / d->side;
    for (int x = r->left; x <= r->right; x++, s += 3) {
      if (ty < 0 || ty >= SIZE) continue;
      int tx = (x - d->x0) * SIZE / d->side;
      if (tx < 0 || tx >= SIZE) continue;
      size_t i = (size_t)ty * SIZE + tx;
      if (d->n[i] >= 255) continue;  // con la reducción elegida no pasa (como mucho ~16 × 16 por píxel)
      d->sum[i * 3] += s[0];  // rojo, verde, azul
      d->sum[i * 3 + 1] += s[1];
      d->sum[i * 3 + 2] += s[2];
      d->n[i]++;
    }
  }
  return 1;
}

static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

// Gira la foto ya hecha: 1 cuarto de vuelta a la derecha cada vez.
static void rotate(uint16_t* px, uint16_t* tmp) {
  for (int y = 0; y < SIZE; y++)
    for (int x = 0; x < SIZE; x++) tmp[x * SIZE + (SIZE - 1 - y)] = px[y * SIZE + x];
  memcpy(px, tmp, PIXELS * 2);
}

// Abre `path` y deja en `out` (120 × 120 RGB565) la foto recortada al centro. false si no se puede (no es JPEG,
// es «progresivo», está rota…).
static bool decode(const char* path, uint16_t* out) {
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  setvbuf(f, nullptr, _IOFBF, 8192);
  int orient = orientation(f);
  fseek(f, 0, SEEK_SET);
  const size_t POOL = 12 * 1024;
  void* pool = malloc(POOL);
  Dec d = {f, 0, 0, 0, nullptr, nullptr};
  FJDEC jd;
  bool ok = false;
  if (pool && fj_prepare(&jd, decIn, pool, POOL, &d) == FJR_OK) {
    // La reducción más fuerte que aún deja el lado corto en 120 px o más.
    int sc = 0;
    while (sc < 3 && (int)(jd.width < jd.height ? jd.width : jd.height) >> (sc + 1) >= SIZE) sc++;
    int w = (jd.width + (1 << sc) - 1) >> sc, h = (jd.height + (1 << sc) - 1) >> sc;
    d.side = w < h ? w : h;
    d.x0 = (w - d.side) / 2;
    d.y0 = (h - d.side) / 2;
    d.sum = (uint16_t*)calloc(PIXELS * 3, 2);
    d.n = (uint16_t*)calloc(PIXELS, 2);
    if (d.sum && d.n && d.side > 0 && fj_decomp(&jd, decOut, (uint8_t)sc) == FJR_OK) {
      for (size_t i = 0; i < PIXELS; i++) {
        int c = d.n[i] ? d.n[i] : 1;
        int x = (int)(i % SIZE), y = (int)(i / SIZE);
        uint8_t t = BAYER[((y & 3) << 2) | (x & 3)];
        int R = d.sum[i * 3] / c + (t >> 1), G = d.sum[i * 3 + 1] / c + (t >> 2), B = d.sum[i * 3 + 2] / c + (t >> 1);
        if (R > 255) R = 255;
        if (G > 255) G = 255;
        if (B > 255) B = 255;
        out[i] = (uint16_t)(((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3));
      }
      ok = true;
    }
  }
  free(d.sum);
  free(d.n);
  free(pool);
  fclose(f);
  if (ok && orient != 1) {
    uint16_t* tmp = (uint16_t*)malloc(PIXELS * 2);
    if (tmp) {
      int turns = orient == 6 ? 1 : orient == 3 ? 2 : orient == 8 ? 3 : 0;  // 6 = girar a la derecha, 8 = a la izquierda
      for (int i = 0; i < turns; i++) rotate(out, tmp);
      free(tmp);
    }
  }
  return ok;
}

// ---------------------------------------------------------------- trabajo en segundo plano
// Una foto cada vez: la pantalla pide (`request`) y luego mira (`poll`) si ya está.
enum : uint8_t { IDLE, QUEUED, BUSY, DONE, FAILED };
static volatile uint8_t jobState = IDLE;
static char jobPath[128];
static uint16_t* jobOut = nullptr;

static void runJob() {
  jobState = BUSY;
  bool ok = decode(jobPath, jobOut);
  jobState = ok ? DONE : FAILED;
}

#ifdef ARDUINO
static TaskHandle_t worker = nullptr;
static void workerTask(void*) {
  for (;;) {
    if (jobState == QUEUED) runJob();
    else vTaskDelay(pdMS_TO_TICKS(8));
  }
}
#endif

// Pila en la PSRAM (solo lee la tarjeta SD): la memoria interna se queda para el Wi-Fi.
static void startWorker() {
#ifdef ARDUINO
  if (!worker) xTaskCreatePinnedToCoreWithCaps(workerTask, "fotos", 6144, nullptr, 1, &worker, 0, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!worker) xTaskCreatePinnedToCore(workerTask, "fotos", 5120, nullptr, 1, &worker, 0);
#endif
}

// Pide la foto número `i` de la lista en `out` (120 × 120). false si ya hay otra a medias.
static bool request(int i, uint16_t* out) {
  if (jobState == QUEUED || jobState == BUSY || i < 0 || i >= count) return false;
  snprintf(jobPath, sizeof(jobPath), "%s/%s", dirPath, names[i]);
  jobOut = out;
#ifdef ARDUINO
  startWorker();
  jobState = QUEUED;
#else
  jobState = QUEUED;
  runJob();
#endif
  return true;
}

// Descomprimiendo una foto ahora mismo (la pantalla se protege: ui_port.h, setNetwork).
static bool busy() {
  uint8_t s = jobState;
  return s == QUEUED || s == BUSY;
}

// IDLE, BUSY, DONE o FAILED (al leer DONE o FAILED, vuelve a IDLE).
static uint8_t poll() {
  uint8_t s = jobState;
  if (s == QUEUED) return BUSY;
  if (s == DONE || s == FAILED) jobState = IDLE;
  return s;
}

// Imagen de LVGL tal cual se guarda (cabecera de 12 bytes + RGB565), como la que manda la app.
static void header(uint8_t* h) {
  memset(h, 0, 12);
  h[0] = 0x19;
  h[1] = 0x12;
  h[4] = SIZE;
  h[6] = SIZE;
  h[8] = (uint8_t)((SIZE * 2) & 0xFF);
  h[9] = (uint8_t)((SIZE * 2) >> 8);
}

}  // namespace fotos

#endif  // MFV3_S3_FOTOS_H
