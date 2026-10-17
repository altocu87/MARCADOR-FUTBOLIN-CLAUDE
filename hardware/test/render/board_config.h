// MARCADOR FUTBOLÍN V3 · «placa» de PC para sacar capturas de la pantalla de 7" sin tenerla.
// Sustituye a esp32s3_pantalla7/board_config.h: la pantalla es un lienzo de 800×480 en memoria dibujado con la
// LovyanGFX de verdad (la misma que usa la placa), así que las capturas salen igual que en la pantalla real.
// El táctil, el expansor y la retroiluminación se simulan. Lo usa test/capturas.sh.

#ifndef MFV3_BOARD_CONFIG_H
#define MFV3_BOARD_CONFIG_H

#define BOARD_ELECROW_7 1
#define BOARD_WAVESHARE_7 2
#define BOARD_CUSTOM 3
#define BOARD_WAVESHARE_7C 4
#ifndef MFV3_BOARD
#define MFV3_BOARD BOARD_WAVESHARE_7C
#endif

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <Wire.h>

// Pines que nombra mfv3_audio.h (en el PC no hay ESP-IDF).
enum gpio_num_t { GPIO_NUM_NC = -1, GPIO_NUM_6 = 6, GPIO_NUM_15 = 15, GPIO_NUM_16 = 16, GPIO_NUM_44 = 44 };

#define AUX_UART_RX -1
#define AUX_UART_TX -1
#define BUZZER_PIN -1
#define HAS_ISOLATED_IO 1

namespace exio {
const uint8_t TOUCH_RST = 1, BACKLIGHT = 2, SPEAKER_AMP = 3, SD_CS = 4;
const int I2C_PORT = 0;
const uint8_t DI0 = 7, DI1 = 8, DI2 = 9, DI3 = 10;
const uint8_t DO0 = 11, DO1 = 12, DO2 = 13, DO3 = 14;
}  // namespace exio

namespace sim {
inline bool touchDown = false;
inline int32_t touchX = 0, touchY = 0;
inline uint16_t exioIn = 0, exioOut = 0;
inline uint8_t backlight = 0;
}  // namespace sim

class LGFX : public lgfx::LGFX_Sprite {
 public:
  bool init() {
    setColorDepth(16);
    return createSprite(800, 480) != nullptr;
  }
  void setRotation(int) {}
  void backlight(uint8_t level) { sim::backlight = level; }
  int getTouch(int32_t* x, int32_t* y) {
    if (!sim::touchDown) return 0;
    *x = sim::touchX;
    *y = sim::touchY;
    return 1;
  }
  void expanderInit() {}
  void detectTouchAddress() {}
  void exioWrite(uint8_t pin, bool on) {
    if (on) sim::exioOut |= (uint16_t)(1u << pin);
    else sim::exioOut &= (uint16_t)~(1u << pin);
  }
  bool exioRead(uint16_t& v) {
    v = sim::exioIn;
    return true;
  }
  bool i2cWrite(uint8_t, const uint8_t*, uint8_t) { return true; }
  bool i2cReadReg(uint8_t, uint8_t, uint8_t& v) {
    v = 0;
    return true;
  }
};

#endif  // MFV3_BOARD_CONFIG_H
