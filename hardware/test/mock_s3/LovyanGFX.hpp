// Simulación mínima de LovyanGFX: registra los textos dibujados y simula el táctil.
#pragma once
#include <stdint.h>
#include <string>
enum gpio_num_t {
  GPIO_NUM_NC = -1,
  GPIO_NUM_0 = 0,
  GPIO_NUM_1 = 1,
  GPIO_NUM_2 = 2,
  GPIO_NUM_3 = 3,
  GPIO_NUM_4 = 4,
  GPIO_NUM_5 = 5,
  GPIO_NUM_6 = 6,
  GPIO_NUM_7 = 7,
  GPIO_NUM_8 = 8,
  GPIO_NUM_9 = 9,
  GPIO_NUM_10 = 10,
  GPIO_NUM_11 = 11,
  GPIO_NUM_12 = 12,
  GPIO_NUM_13 = 13,
  GPIO_NUM_14 = 14,
  GPIO_NUM_15 = 15,
  GPIO_NUM_16 = 16,
  GPIO_NUM_17 = 17,
  GPIO_NUM_18 = 18,
  GPIO_NUM_19 = 19,
  GPIO_NUM_20 = 20,
  GPIO_NUM_21 = 21,
  GPIO_NUM_22 = 22,
  GPIO_NUM_23 = 23,
  GPIO_NUM_24 = 24,
  GPIO_NUM_25 = 25,
  GPIO_NUM_26 = 26,
  GPIO_NUM_27 = 27,
  GPIO_NUM_28 = 28,
  GPIO_NUM_29 = 29,
  GPIO_NUM_30 = 30,
  GPIO_NUM_31 = 31,
  GPIO_NUM_32 = 32,
  GPIO_NUM_33 = 33,
  GPIO_NUM_34 = 34,
  GPIO_NUM_35 = 35,
  GPIO_NUM_36 = 36,
  GPIO_NUM_37 = 37,
  GPIO_NUM_38 = 38,
  GPIO_NUM_39 = 39,
  GPIO_NUM_40 = 40,
  GPIO_NUM_41 = 41,
  GPIO_NUM_42 = 42,
  GPIO_NUM_43 = 43,
  GPIO_NUM_44 = 44,
  GPIO_NUM_45 = 45,
  GPIO_NUM_46 = 46,
  GPIO_NUM_47 = 47,
  GPIO_NUM_48 = 48,
};
namespace sim {
inline std::string screen;  // textos dibujados desde el último fillScreen
inline bool touchDown = false;
inline int32_t touchX = 0, touchY = 0;
inline uint8_t brightness = 0;
inline int fullRedraws = 0;
// Expansor de E/S de la Waveshare 7C (dirección 0x24)
inline uint16_t exioMode = 0, exioOut = 0, exioIn = 0;
inline uint8_t exioPwm = 0;
// Códec de audio ES8389 (dirección 0x10): registros escritos.
inline bool codecPresent = true;
inline uint8_t codecRegs[256] = {0};
}  // namespace sim
namespace lgfx {
struct IFont {};
struct I2cResult {
  bool err;
  bool has_error() const { return err; }
};
namespace i2c {
inline I2cResult init(int, int, int) { return {false}; }
inline I2cResult transactionWrite(int, int addr, const uint8_t* d, uint8_t len, uint32_t = 400000) {
  if (addr == 0x10) {
    if (!sim::codecPresent) return {true};
    for (uint8_t i = 1; i < len; i++) sim::codecRegs[(uint8_t)(d[0] + i - 1)] = d[i];
    return {false};
  }
  if (addr != 0x24 || len < 2) return {false};
  if (d[0] == 0x02) sim::exioMode = (uint16_t)(d[1] | (d[2] << 8));
  if (d[0] == 0x03) sim::exioOut = (uint16_t)(d[1] | (d[2] << 8));
  if (d[0] == 0x05) sim::exioPwm = d[1];
  return {false};
}
inline I2cResult transactionWriteRead(int, int addr, const uint8_t* w, uint8_t, uint8_t* r, size_t, uint32_t = 400000) {
  if (addr == 0x10) {
    if (!sim::codecPresent) return {true};
    r[0] = sim::codecRegs[w[0]];
    return {false};
  }
  if (addr != 0x24 || w[0] != 0x04) return {true};
  r[0] = (uint8_t)(sim::exioIn & 0xFF);
  r[1] = (uint8_t)(sim::exioIn >> 8);
  return {false};
}
}  // namespace i2c
namespace fonts {
inline IFont FreeSans9pt7b, FreeSans12pt7b, FreeSansBold9pt7b, FreeSansBold12pt7b, FreeSansBold18pt7b, FreeSansBold24pt7b, Font7, Font8;
}
enum class textdatum_t { middle_center, middle_left, middle_right };
struct Bus_RGB {
  struct Cfg {
    void* panel;
    int pin_d0, pin_d1, pin_d2, pin_d3, pin_d4, pin_d5, pin_d6, pin_d7, pin_d8, pin_d9, pin_d10, pin_d11, pin_d12, pin_d13, pin_d14, pin_d15;
    int pin_henable, pin_vsync, pin_hsync, pin_pclk;
    uint32_t freq_write;
    int hsync_polarity, hsync_front_porch, hsync_pulse_width, hsync_back_porch;
    int vsync_polarity, vsync_front_porch, vsync_pulse_width, vsync_back_porch;
    int pclk_active_neg, de_idle_high, pclk_idle_high;
  } c{};
  Cfg config() const { return c; }
  void config(const Cfg& x) { c = x; }
};
struct Light_PWM {
  struct Cfg { int pin_bl; } c{};
  Cfg config() const { return c; }
  void config(const Cfg& x) { c = x; }
};
struct Touch_GT911 {
  struct Cfg { int x_min, x_max, y_min, y_max, pin_int, pin_rst; bool bus_shared; int offset_rotation, i2c_port, pin_sda, pin_scl; uint32_t freq; uint8_t i2c_addr; } c{};
  Cfg config() const { return c; }
  void config(const Cfg& x) { c = x; }
};
struct Panel_RGB {
  struct Cfg { int memory_width, memory_height, panel_width, panel_height, offset_x, offset_y; } c{};
  struct Detail { int use_psram; } d{};
  Cfg config() const { return c; }
  void config(const Cfg& x) { c = x; }
  Detail config_detail() const { return d; }
  void config_detail(const Detail& x) { d = x; }
  void setLight(Light_PWM*) {}
  void setBus(Bus_RGB*) {}
  void setTouch(Touch_GT911*) {}
};
struct LGFX_Device {
  void setPanel(Panel_RGB*) {}
  bool init() { return true; }
  void setRotation(int) {}
  void setBrightness(uint8_t b) { sim::brightness = b; }
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }
  void fillScreen(uint16_t) { sim::screen.clear(); sim::fullRedraws++; }
  void fillRect(int, int, int, int, uint16_t) {}
  void fillRoundRect(int, int, int, int, int, uint16_t) {}
  void drawRoundRect(int, int, int, int, int, uint16_t) {}
  void drawCircle(int, int, int, uint16_t) {}
  void fillCircle(int, int, int, uint16_t) {}
  void drawFastHLine(int, int, int, uint16_t) {}
  void drawLine(int, int, int, int, uint16_t) {}
  void setFont(const IFont*) {}
  void setTextSize(float) {}
  void setTextColor(uint16_t) {}
  void setTextDatum(textdatum_t) {}
  void drawString(const char* s, int, int) { sim::screen += s; sim::screen += '|'; }
  int getTouch(int32_t* x, int32_t* y) {
    if (!sim::touchDown) return 0;
    *x = sim::touchX; *y = sim::touchY; return 1;
  }
};
}  // namespace lgfx
