// MARCADOR FUTBOLÍN V3 · motor gráfico de la pantalla de 7": LVGL 9 sobre el panel RGB 800×480.
// -----------------------------------------------------------------------------------------------
// En la placa (Waveshare 7C): el controlador RGB del ESP32-S3 (esp_lcd) con DOS imágenes de pantalla en la PSRAM.
// LVGL dibuja en la que no se ve y, al terminar, se cambia de una a otra en el retorno vertical: sin parpadeo ni
// cortes a media imagen. Los «bounce buffers» en la RAM interna evitan que la PSRAM se quede corta a 16 MHz.
// El táctil GT911 se lee por el mismo bus I2C que el expansor (lgfx::i2c, como el resto de chips de la placa).
//
// En el PC (pruebas y capturas sin placa): la misma LVGL dibujando en una imagen en memoria; el táctil sale de
// sim::touch* (mock_s3).
//
// Órdenes por USB que usan esto: «CAPTURA» (la imagen que se ve, en RGB565) y «TOQUE x y» (un toque simulado).

#ifndef MFV3_UI_PORT_H
#define MFV3_UI_PORT_H

#include <lvgl.h>

namespace uiport {

const int W = 800, H = 480;

// Toque simulado (orden TOQUE o pruebas): se mantiene pulsado unos instantes para que LVGL lo vea.
static volatile int16_t fakeX = 0, fakeY = 0;
static volatile uint32_t fakeUntil = 0;
static uint32_t lastTouchAt = 0;  // último toque real o simulado (para el reposo)
static uint16_t* shown = nullptr;  // la imagen de pantalla que se está viendo ahora
// Al empezar cada toque: si devuelve true, el toque entero se ignora (p. ej. el que despierta la pantalla).
static bool (*touchDownHook)() = nullptr;
static bool swallowing = false, wasDown = false;

static volatile int16_t fakeX2 = 0, fakeY2 = 0;
static volatile uint32_t fakeStart = 0, fakeMove = 0;  // arrastre simulado: de (fakeX, fakeY) a (fakeX2, fakeY2)

static void injectTouch(int16_t x, int16_t y, uint32_t ms = 160) {  // como un dedo (con 90 ms se perdía si la placa estaba pintando)
  fakeX = fakeX2 = x;
  fakeY = fakeY2 = y;
  fakeStart = millis();
  fakeMove = 0;
  fakeUntil = fakeStart + ms;
}

// Dedo que se arrastra de (x, y) a (x2, y2) en `ms` y se levanta (orden DESLIZA, para medir el desplazamiento).
static void injectDrag(int16_t x, int16_t y, int16_t x2, int16_t y2, uint32_t ms) {
  fakeX = x;
  fakeY = y;
  fakeX2 = x2;
  fakeY2 = y2;
  fakeStart = millis();
  fakeMove = ms ? ms : 1;
  fakeUntil = fakeStart + fakeMove;
}

static uint32_t tickCb() { return millis(); }

// Los avisos son prescindibles: nunca deben esperar al lector USB. Solo loop() escribe en Serial;
// las tareas de red/radio entregan sus resultados mediante colas. El protocolo y CAPTURA conservan
// su escritura normal. No añadir Serial.print desde otra tarea sin separar antes ambos canales.
static const uint32_t USB_PROTOCOL_TIMEOUT_MS = 100;  // valor de HWCDC del núcleo ESP32
static void diagnostic(const char* text, bool newline = false) {
  if (!text) return;
  char line[256];
  size_t n = strnlen(text, sizeof(line) - 2);
  memcpy(line, text, n);
  if (newline) line[n++] = '\n';
  // En USB nativo, también la toma del mutex y la escritura deben ser sin espera. En UART
  // se comprueba el espacio; no hay otros escritores en esta tarea.
#if !defined(ESP_PLATFORM) || (defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT)
  Serial.setTxTimeoutMs(0);
#endif
  if (Serial.availableForWrite() >= (int)n) Serial.write((const uint8_t*)line, n);
#if !defined(ESP_PLATFORM) || (defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT)
  Serial.setTxTimeoutMs(USB_PROTOCOL_TIMEOUT_MS);
#endif
}
static void logCb(lv_log_level_t, const char* buf) { diagnostic(buf); }

// Contadores de «RENDIMIENTO» (ver step()).
static uint32_t perfWaitFrame = 0, perfT0 = 0, perfFrames = 0, perfDrawUs = 0, perfDrawMax = 0, perfWaitUs = 0, perfLvUs = 0, perfLvMax = 0,
                perfGapMax = 0, perfLast = 0, perfRenderStart = 0;

#ifdef ESP_PLATFORM
}  // namespace uiport
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <freertos/semphr.h>
namespace uiport {

static esp_lcd_panel_handle_t panel = nullptr;
static const uint32_t PCLK_HZ = 12500000;
static SemaphoreHandle_t vsyncSem = nullptr;
static uint16_t *fb0 = nullptr, *fb1 = nullptr;

static bool IRAM_ATTR onVsync(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t*, void*) {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(vsyncSem, &woken);
  return woken == pdTRUE;
}


// Al acabar de dibujar: se enseña la imagen nueva y se espera al retorno vertical antes de dejar a LVGL dibujar
// en la otra (que ya no se está viendo).
static void flushCb(lv_display_t* disp, const lv_area_t*, uint8_t* px) {
  if (lv_display_flush_is_last(disp)) {
    xSemaphoreTake(vsyncSem, 0);
    esp_lcd_panel_draw_bitmap(panel, 0, 0, W, H, px);
    uint32_t w0 = micros();
    xSemaphoreTake(vsyncSem, pdMS_TO_TICKS(60));
    perfWaitFrame = micros() - w0;
    perfWaitUs += perfWaitFrame;
    shown = (uint16_t*)px;
  }
  lv_display_flush_ready(disp);
}

// Táctil GT911 por el bus I2C de la placa (el mismo que el expansor en la 7C): 0x814E = estado (bit 7: hay
// datos; bits 0–3: dedos), 0x8150 = primer punto (x, y en 16 bits).
static int touchPort = 0, touchAddr = 0x5D;

static bool gt911Read(int16_t& x, int16_t& y) {
  static uint32_t lastPoll = 0;
  static bool down = false;
  static int16_t lx = 0, ly = 0;
  uint32_t now = millis();
  if (now - lastPoll < 10) {
    x = lx;
    y = ly;
    return down;
  }
  lastPoll = now;
  uint8_t reg[2] = {0x81, 0x4E}, st = 0;
  if (lgfx::i2c::transactionWriteRead(touchPort, touchAddr, reg, 2, &st, 1).has_error()) return down = false;
  if (st & 0x80) {
    if ((st & 0x0F) > 0) {
      uint8_t r2[2] = {0x81, 0x50}, p[4];
      if (!lgfx::i2c::transactionWriteRead(touchPort, touchAddr, r2, 2, p, 4).has_error()) {
        lx = (int16_t)(p[0] | (p[1] << 8));
        ly = (int16_t)(p[2] | (p[3] << 8));
        down = true;
      }
    } else {
      down = false;
    }
    uint8_t clr[3] = {0x81, 0x4E, 0};
    lgfx::i2c::transactionWrite(touchPort, touchAddr, clr, 3);
  }
  x = lx;
  y = ly;
  return down;
}

static bool readTouch(int16_t& x, int16_t& y) { return gt911Read(x, y); }

// Tras escribir en la memoria flash (ajustes, resultados, fotos) la imagen puede quedar descolocada: mientras se
// escribe, el ESP32-S3 no puede rellenar los búferes de la pantalla. Se vuelve a alinear en el siguiente retorno.
static void resyncPanel() {
  if (panel) esp_lcd_rgb_panel_restart(panel);
}

// Pines y tiempos del panel: los de board_config.h (perfil de cada placa), leídos de la configuración LovyanGFX.
static bool beginDisplay(LGFX& lcd, lv_display_t*& disp) {
  vsyncSem = xSemaphoreCreateBinary();
  auto b = lcd.bus.config();
  esp_lcd_rgb_panel_config_t c = {};
  c.clk_src = LCD_CLK_SRC_DEFAULT;
  // 12,5 MHz (≈ 30 imágenes por segundo): lo que usa el ejemplo de Waveshare (12 MHz) con un poco más. A 16 MHz la
  // PSRAM no da abasto cuando además se dibuja mucho y la imagen se descoloca («interferencias»).
  c.timings.pclk_hz = PCLK_HZ;
  c.timings.h_res = W;
  c.timings.v_res = H;
  c.timings.hsync_pulse_width = b.hsync_pulse_width;
  c.timings.hsync_back_porch = b.hsync_back_porch;
  c.timings.hsync_front_porch = b.hsync_front_porch;
  c.timings.vsync_pulse_width = b.vsync_pulse_width;
  c.timings.vsync_back_porch = b.vsync_back_porch;
  c.timings.vsync_front_porch = b.vsync_front_porch;
  c.timings.flags.pclk_active_neg = b.pclk_active_neg;
  c.timings.flags.de_idle_high = b.de_idle_high;
  c.timings.flags.pclk_idle_high = b.pclk_idle_high;
  c.data_width = 16;
  c.bits_per_pixel = 16;
  c.num_fbs = 2;
  c.bounce_buffer_size_px = W * 20;  // 20 líneas de reserva en la RAM interna
  c.dma_burst_size = 64;
  c.hsync_gpio_num = (int)b.pin_hsync;
  c.vsync_gpio_num = (int)b.pin_vsync;
  c.de_gpio_num = (int)b.pin_henable;
  c.pclk_gpio_num = (int)b.pin_pclk;
  c.disp_gpio_num = -1;
  const int pins[16] = {(int)b.pin_d0, (int)b.pin_d1, (int)b.pin_d2,  (int)b.pin_d3,  (int)b.pin_d4,  (int)b.pin_d5,
                        (int)b.pin_d6, (int)b.pin_d7, (int)b.pin_d8,  (int)b.pin_d9,  (int)b.pin_d10, (int)b.pin_d11,
                        (int)b.pin_d12, (int)b.pin_d13, (int)b.pin_d14, (int)b.pin_d15};
  for (int i = 0; i < 16; i++) c.data_gpio_nums[i] = pins[i];
  c.flags.fb_in_psram = 1;
  // Se crea desde este núcleo (el que dibuja): su interrupción copia la imagen de la PSRAM a los búferes de reserva
  // y, en el mismo núcleo, mientras copia LVGL no usa la PSRAM. Probado en el otro núcleo (18/10/2026): dibujaba algo
  // más rápido, pero los dos a la vez se pisaban la PSRAM, la pantalla no recibía a tiempo y la imagen se corría
  // (con CONFIG_LCD_RGB_RESTART_IN_VSYNC se recoloca en la imagen siguiente, pero se veía el salto).
  if (esp_lcd_new_rgb_panel(&c, &panel) != ESP_OK) return false;
  esp_lcd_rgb_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = onVsync;
  esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, nullptr);
  if (esp_lcd_panel_reset(panel) != ESP_OK || esp_lcd_panel_init(panel) != ESP_OK) return false;
  void *f0 = nullptr, *f1 = nullptr;
  if (esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &f0, &f1) != ESP_OK) return false;
  fb0 = (uint16_t*)f0;
  fb1 = (uint16_t*)f1;
  shown = fb0;
  disp = lv_display_create(W, H);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, fb0, fb1, W * H * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_flush_cb(disp, flushCb);
  // Táctil: mismo puerto, pines y dirección que la configuración LovyanGFX de la placa.
  auto t = lcd.touch.config();
  touchPort = t.i2c_port;
  touchAddr = t.i2c_addr;
  if (!t.bus_shared && t.pin_sda >= 0) lgfx::i2c::init(t.i2c_port, t.pin_sda, t.pin_scl);
  return true;
}

#else  // ---------------------------------------------------------------- PC

static uint16_t* pcFb = nullptr;
static void flushCb(lv_display_t* disp, const lv_area_t*, uint8_t*) { lv_display_flush_ready(disp); }
static void resyncPanel() {}
static bool readTouch(int16_t& x, int16_t& y) {
  x = (int16_t)sim::touchX;
  y = (int16_t)sim::touchY;
  return sim::touchDown;
}
static bool beginDisplay(LGFX&, lv_display_t*& disp) {
  pcFb = (uint16_t*)calloc(W * H, 2);
  shown = pcFb;
  disp = lv_display_create(W, H);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, pcFb, nullptr, W * H * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_flush_cb(disp, flushCb);
  return true;
}
#endif

static void indevCb(lv_indev_t*, lv_indev_data_t* d) {
  int16_t x = 0, y = 0;
  bool down;
  if ((int32_t)(fakeUntil - millis()) > 0) {
    x = fakeX;
    y = fakeY;
    if (fakeMove) {
      uint32_t t = millis() - fakeStart;
      x = (int16_t)(fakeX + (int32_t)(fakeX2 - fakeX) * (int32_t)t / (int32_t)fakeMove);
      y = (int16_t)(fakeY + (int32_t)(fakeY2 - fakeY) * (int32_t)t / (int32_t)fakeMove);
    }
    down = true;
  } else {
    down = readTouch(x, y);
  }
  if (x < 0) x = 0;
  if (x >= W) x = W - 1;
  if (y < 0) y = 0;
  if (y >= H) y = H - 1;
  if (down && !wasDown) swallowing = touchDownHook && touchDownHook();
  wasDown = down;
  if (swallowing) down = false;
  d->point.x = x;
  d->point.y = y;
  d->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (down) lastTouchAt = millis();
}

static lv_display_t* display = nullptr;
static lv_indev_t* touch = nullptr;

static bool begin(LGFX& lcd) {
  lv_init();
  lv_tick_set_cb(tickCb);
  lv_log_register_print_cb(logCb);
  if (!beginDisplay(lcd, display)) return false;
  touch = lv_indev_create();
  lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touch, indevCb);
  // Mantener pulsado (flechas de la configuración): empieza a repetir a los 0,5 s y luego cada 0,15 s.
  lv_indev_set_long_press_time(touch, 500);
  lv_indev_set_long_press_repeat_time(touch, 150);
  return true;
}

// Algo escribió en la memoria flash: hay que realinear la imagen (ver resyncPanel).
static volatile bool needResync = false;

// Un paso de LVGL (animaciones, toques, dibujo). Se llama en cada vuelta de loop().
// «RENDIMIENTO 1»: cada segundo, por el USB, cuántas imágenes se han pintado y cuánto se ha tardado (para ver
// dónde se atasca la pantalla). «RENDIMIENTO 0» lo apaga.
static bool perfOn = false;
// Zona repintada en cada imagen (unión de lo invalidado), para decir qué se está repintando cuando tarda.
static lv_area_t perfInv = {0, 0, -1, -1};
static bool perfZones = false;  // «RENDIMIENTO 2»: además, cada zona que se marca para repintar
static void perfInvalidate(lv_event_t* e) {
  if (!perfOn) return;
  const lv_area_t* a = (const lv_area_t*)lv_event_get_param(e);
  if (!a) return;
  if (perfZones) {
    char b[64];
    snprintf(b, sizeof(b), "ZONA %ld,%ld %ldx%ld", (long)a->x1, (long)a->y1, (long)lv_area_get_width(a), (long)lv_area_get_height(a));
    diagnostic(b, true);
  }
  if (perfInv.x2 < perfInv.x1) perfInv = *a;
  else {
    if (a->x1 < perfInv.x1) perfInv.x1 = a->x1;
    if (a->y1 < perfInv.y1) perfInv.y1 = a->y1;
    if (a->x2 > perfInv.x2) perfInv.x2 = a->x2;
    if (a->y2 > perfInv.y2) perfInv.y2 = a->y2;
  }
}
static void perfRender(lv_event_t* e) {
  if (!perfOn) return;
  if (lv_event_get_code(e) == LV_EVENT_RENDER_START) perfRenderStart = micros();
  else {
    uint32_t d = micros() - perfRenderStart - perfWaitFrame;  // sin la espera al retorno vertical
    perfWaitFrame = 0;
    if (d > 60000) {
      char b[96];
      snprintf(b, sizeof(b), "RENDIMIENTO lenta %lu ms: zona %ld,%ld %ldx%ld", (unsigned long)(d / 1000), (long)perfInv.x1,
               (long)perfInv.y1, (long)lv_area_get_width(&perfInv), (long)lv_area_get_height(&perfInv));
      diagnostic(b, true);
    }
    perfInv = {0, 0, -1, -1};
    perfFrames++;
    perfDrawUs += d;
    if (d > perfDrawMax) perfDrawMax = d;
  }
}
static void perfEnable(int mode) {
  bool on = mode != 0;
  perfZones = mode == 2;
  perfOn = on;
  perfT0 = millis();
  perfFrames = perfDrawUs = perfDrawMax = perfWaitUs = perfLvUs = perfLvMax = perfGapMax = 0;
  perfLast = perfRenderStart = perfWaitFrame = 0;
  perfInv = {0, 0, -1, -1};
  static bool hooked = false;
  if (on && !hooked && display) {
    hooked = true;
    lv_display_add_event_cb(display, perfRender, LV_EVENT_RENDER_START, nullptr);
    lv_display_add_event_cb(display, perfRender, LV_EVENT_RENDER_READY, nullptr);
    lv_display_add_event_cb(display, perfInvalidate, LV_EVENT_INVALIDATE_AREA, nullptr);
  }
}

static void step() {
  if (needResync) {
    needResync = false;
    resyncPanel();
  }
  if (!perfOn) {
    lv_timer_handler();
    return;
  }
  uint32_t t = micros();
  if (perfLast && t - perfLast > perfGapMax) perfGapMax = t - perfLast;  // lo que ha tardado el resto del bucle
  lv_timer_handler();
  perfLast = micros();
  uint32_t d = perfLast - t;
  perfLvUs += d;
  if (d > perfLvMax) perfLvMax = d;
  if (millis() - perfT0 >= 1000) {
    char b[160];
    snprintf(b, sizeof(b), "RENDIMIENTO %lu fps, pintar media %lu ms max %lu, espera %lu ms, lvgl %lu ms/s max %lu, resto max %lu ms",
             (unsigned long)perfFrames, (unsigned long)(perfFrames ? perfDrawUs / perfFrames / 1000 : 0), (unsigned long)(perfDrawMax / 1000),
             (unsigned long)(perfWaitUs / 1000), (unsigned long)(perfLvUs / 1000), (unsigned long)(perfLvMax / 1000),
             (unsigned long)(perfGapMax / 1000));
    diagnostic(b, true);
    perfEnable(perfZones ? 2 : 1);
  }
}

// CRC32 (el mismo que zlib) para que el PC compruebe la captura.
static uint32_t crc32Update(uint32_t crc, const uint8_t* d, size_t n) {
  crc = ~crc;
  while (n--) {
    crc ^= *d++;
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

// «CAPTURA»: la imagen que se ve ahora, en RGB565 (byte bajo primero), precedida de una línea con su tamaño y CRC.
static void sendCapture() {
  const uint8_t* p = (const uint8_t*)shown;
  const size_t n = (size_t)W * H * 2;
  char head[64];
  snprintf(head, sizeof(head), "CAPTURA %d %d %u %08lx", W, H, (unsigned)n, (unsigned long)crc32Update(0, p, n));
  Serial.println(head);
  for (size_t off = 0; off < n; off += 4096) Serial.write(p + off, n - off < 4096 ? n - off : 4096);
  Serial.flush();
}

}  // namespace uiport

#endif  // MFV3_UI_PORT_H
