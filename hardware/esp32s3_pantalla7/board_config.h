// MARCADOR FUTBOLÍN V3 · configuración de la placa ESP32-S3 con pantalla de 7" (800×480, táctil GT911)
// -----------------------------------------------------------------------------------------------------
// Elige UNA placa. Los pines de la pantalla RGB cambian según el fabricante; si tu placa no está en la lista,
// copia el perfil CUSTOM y rellénalo con los datos del esquema/ejemplo LovyanGFX de tu fabricante.
//
//   BOARD_WAVESHARE_7C Waveshare ESP32-S3-Touch-LCD-7C / 7C-BOX («7inch AI Voice Touch Display», 800×480,
//                      GT911, audio ES8389, 4 entradas aisladas DI0–DI3). Pines del repositorio oficial
//                      github.com/waveshareteam/ESP32-S3-Touch-LCD-7C (ejemplos Arduino 03_LCD, 06_TOUCH, 04_ISOLATION_IO).
//   BOARD_ELECROW_7    Elecrow CrowPanel 7.0" ESP32-S3 (HMI 800×480, GT911)
//   BOARD_WAVESHARE_7  Waveshare ESP32-S3-Touch-LCD-7 (800×480, GT911, expansor CH422G)
//   BOARD_CUSTOM       Otra placa: edita los valores marcados con «RELLENAR»
//
// Biblioteca: LovyanGFX (Gestor de bibliotecas). Placa: «ESP32S3 Dev Module» con PSRAM «OPI PSRAM»,
// en la 7C Flash 32 MB y Partition Scheme «Custom» (usa partitions.csv: 2 × 6 MB de programa y 19,9 MB para
// imágenes; los sonidos, en la SD). En las demás, borra partitions.csv y elige las que indique el fabricante.

#ifndef MFV3_BOARD_CONFIG_H
#define MFV3_BOARD_CONFIG_H

#define BOARD_ELECROW_7 1
#define BOARD_WAVESHARE_7 2
#define BOARD_CUSTOM 3
#define BOARD_WAVESHARE_7C 4

#ifndef MFV3_BOARD
#define MFV3_BOARD BOARD_WAVESHARE_7C   // <── CAMBIA AQUÍ TU PLACA
#endif

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <Wire.h>

// Pines de entradas auxiliares (UART desde un Arduino/ESP32-C3 con el sketch arduino_usb). -1 = sin usar.
#if MFV3_BOARD == BOARD_WAVESHARE_7C
// GPIO43/44 los usa el audio y GPIO19/20 el USB: sin UART libre. Los sensores van a las entradas aisladas.
#define AUX_UART_RX -1
#define AUX_UART_TX -1
#define BUZZER_PIN -1
#define HAS_ISOLATED_IO 1
#elif MFV3_BOARD == BOARD_ELECROW_7
#define AUX_UART_RX 18   // conector UART1 de la CrowPanel (comprueba la serigrafía)
#define AUX_UART_TX 17
#define BUZZER_PIN -1
#elif MFV3_BOARD == BOARD_WAVESHARE_7
#define AUX_UART_RX 44   // UART del conector (compartido con USB-UART: no uses ambos a la vez)
#define AUX_UART_TX 43
#define BUZZER_PIN -1
#else
#define AUX_UART_RX -1   // RELLENAR
#define AUX_UART_TX -1   // RELLENAR
#define BUZZER_PIN -1
#endif

#ifndef HAS_ISOLATED_IO
#define HAS_ISOLATED_IO 0
#endif

#if MFV3_BOARD == BOARD_WAVESHARE_7C
// Expansor de E/S de la 7C (dirección 0x24, mismo bus I2C que el táctil). Registros y patas EXIO según
// io_extension.h del fabricante: 0x02 modo (1 = salida), 0x03 salidas, 0x04 entradas, 0x05 brillo (PWM).
namespace exio {
const uint8_t ADDR = 0x24, REG_MODE = 0x02, REG_OUT = 0x03, REG_IN = 0x04, REG_PWM = 0x05;
const uint8_t TOUCH_RST = 1, BACKLIGHT = 2, SPEAKER_AMP = 3, SD_CS = 4;
const uint8_t DI0 = 7, DI1 = 8, DI2 = 9, DI3 = 10;      // entradas aisladas
const uint8_t DO0 = 11, DO1 = 12, DO2 = 13, DO3 = 14;   // salidas aisladas
const uint16_t OUTPUT_MASK = (1u << TOUCH_RST) | (1u << BACKLIGHT) | (1u << SPEAKER_AMP) | (1u << SD_CS) | (1u << 6) |
                             (1u << DO0) | (1u << DO1) | (1u << DO2) | (1u << DO3);
const int I2C_PORT = 0, SDA = 47, SCL = 48, TOUCH_INT = 4;
}  // namespace exio
#endif

class LGFX : public lgfx::LGFX_Device {
 public:
  lgfx::Bus_RGB bus;
  lgfx::Panel_RGB panel;
  lgfx::Light_PWM light;
  lgfx::Touch_GT911 touch;
  int touchSda = -1;
  int touchScl = -1;

  LGFX() {
    {
      auto cfg = panel.config();
      cfg.memory_width = 800;
      cfg.memory_height = 480;
      cfg.panel_width = 800;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      panel.config(cfg);
    }
    {
      auto cfg = panel.config_detail();
      cfg.use_psram = 1;  // el framebuffer de 800×480 va en PSRAM
      panel.config_detail(cfg);
    }
    auto b = bus.config();
    b.panel = &panel;
#if MFV3_BOARD == BOARD_WAVESHARE_7C
    // Datos: B3..B7, G2..G7, R3..R7 (bus de 16 bits, RGB565)
    b.pin_d0 = GPIO_NUM_14; b.pin_d1 = GPIO_NUM_38; b.pin_d2 = GPIO_NUM_18; b.pin_d3 = GPIO_NUM_17; b.pin_d4 = GPIO_NUM_10;
    b.pin_d5 = GPIO_NUM_39; b.pin_d6 = GPIO_NUM_0;  b.pin_d7 = GPIO_NUM_45; b.pin_d8 = GPIO_NUM_9;  b.pin_d9 = GPIO_NUM_8;
    b.pin_d10 = GPIO_NUM_21; b.pin_d11 = GPIO_NUM_1; b.pin_d12 = GPIO_NUM_2; b.pin_d13 = GPIO_NUM_42; b.pin_d14 = GPIO_NUM_41;
    b.pin_d15 = GPIO_NUM_40;
    b.pin_henable = GPIO_NUM_5; b.pin_vsync = GPIO_NUM_3; b.pin_hsync = GPIO_NUM_46; b.pin_pclk = GPIO_NUM_7;
    b.freq_write = 16000000;
    b.hsync_polarity = 0; b.hsync_front_porch = 8; b.hsync_pulse_width = 4; b.hsync_back_porch = 8;
    b.vsync_polarity = 0; b.vsync_front_porch = 8; b.vsync_pulse_width = 4; b.vsync_back_porch = 8;
    b.pclk_active_neg = 1; b.de_idle_high = 0; b.pclk_idle_high = 0;
    touchSda = exio::SDA; touchScl = exio::SCL;
    // Retroiluminación, brillo y reset del táctil por el expansor (ver expanderInit()).
#elif MFV3_BOARD == BOARD_ELECROW_7
    // Datos: B0..B4, G0..G5, R0..R4
    b.pin_d0 = GPIO_NUM_15; b.pin_d1 = GPIO_NUM_7;  b.pin_d2 = GPIO_NUM_6;  b.pin_d3 = GPIO_NUM_5;  b.pin_d4 = GPIO_NUM_4;
    b.pin_d5 = GPIO_NUM_9;  b.pin_d6 = GPIO_NUM_46; b.pin_d7 = GPIO_NUM_3;  b.pin_d8 = GPIO_NUM_8;  b.pin_d9 = GPIO_NUM_16;
    b.pin_d10 = GPIO_NUM_1; b.pin_d11 = GPIO_NUM_14; b.pin_d12 = GPIO_NUM_21; b.pin_d13 = GPIO_NUM_47; b.pin_d14 = GPIO_NUM_48;
    b.pin_d15 = GPIO_NUM_45;
    b.pin_henable = GPIO_NUM_41; b.pin_vsync = GPIO_NUM_40; b.pin_hsync = GPIO_NUM_39; b.pin_pclk = GPIO_NUM_0;
    b.freq_write = 15000000;
    b.hsync_polarity = 0; b.hsync_front_porch = 40; b.hsync_pulse_width = 48; b.hsync_back_porch = 40;
    b.vsync_polarity = 0; b.vsync_front_porch = 1;  b.vsync_pulse_width = 31; b.vsync_back_porch = 13;
    b.pclk_active_neg = 1; b.de_idle_high = 0; b.pclk_idle_high = 0;
    touchSda = 19; touchScl = 20;
    { auto l = light.config(); l.pin_bl = GPIO_NUM_2; light.config(l); }
    panel.setLight(&light);
#elif MFV3_BOARD == BOARD_WAVESHARE_7
    b.pin_d0 = GPIO_NUM_14; b.pin_d1 = GPIO_NUM_38; b.pin_d2 = GPIO_NUM_18; b.pin_d3 = GPIO_NUM_17; b.pin_d4 = GPIO_NUM_10;
    b.pin_d5 = GPIO_NUM_39; b.pin_d6 = GPIO_NUM_0;  b.pin_d7 = GPIO_NUM_45; b.pin_d8 = GPIO_NUM_48; b.pin_d9 = GPIO_NUM_47;
    b.pin_d10 = GPIO_NUM_21; b.pin_d11 = GPIO_NUM_1; b.pin_d12 = GPIO_NUM_2; b.pin_d13 = GPIO_NUM_42; b.pin_d14 = GPIO_NUM_41;
    b.pin_d15 = GPIO_NUM_40;
    b.pin_henable = GPIO_NUM_5; b.pin_vsync = GPIO_NUM_3; b.pin_hsync = GPIO_NUM_46; b.pin_pclk = GPIO_NUM_7;
    b.freq_write = 16000000;
    b.hsync_polarity = 0; b.hsync_front_porch = 8; b.hsync_pulse_width = 4; b.hsync_back_porch = 8;
    b.vsync_polarity = 0; b.vsync_front_porch = 8; b.vsync_pulse_width = 4; b.vsync_back_porch = 8;
    b.pclk_active_neg = 1; b.de_idle_high = 0; b.pclk_idle_high = 0;
    touchSda = 8; touchScl = 9;
    // Retroiluminación y reset por el expansor CH422G (ver waveshareExpanderInit()).
#else
    // RELLENAR con los pines de tu placa (orden B0..B4, G0..G5, R0..R4).
    b.pin_d0 = GPIO_NUM_NC; b.pin_d1 = GPIO_NUM_NC; b.pin_d2 = GPIO_NUM_NC; b.pin_d3 = GPIO_NUM_NC; b.pin_d4 = GPIO_NUM_NC;
    b.pin_d5 = GPIO_NUM_NC; b.pin_d6 = GPIO_NUM_NC; b.pin_d7 = GPIO_NUM_NC; b.pin_d8 = GPIO_NUM_NC; b.pin_d9 = GPIO_NUM_NC;
    b.pin_d10 = GPIO_NUM_NC; b.pin_d11 = GPIO_NUM_NC; b.pin_d12 = GPIO_NUM_NC; b.pin_d13 = GPIO_NUM_NC; b.pin_d14 = GPIO_NUM_NC;
    b.pin_d15 = GPIO_NUM_NC;
    b.pin_henable = GPIO_NUM_NC; b.pin_vsync = GPIO_NUM_NC; b.pin_hsync = GPIO_NUM_NC; b.pin_pclk = GPIO_NUM_NC;
    b.freq_write = 14000000;
    b.hsync_polarity = 0; b.hsync_front_porch = 8; b.hsync_pulse_width = 4; b.hsync_back_porch = 8;
    b.vsync_polarity = 0; b.vsync_front_porch = 8; b.vsync_pulse_width = 4; b.vsync_back_porch = 8;
    b.pclk_active_neg = 1; b.de_idle_high = 0; b.pclk_idle_high = 0;
    touchSda = -1; touchScl = -1;  // RELLENAR
#endif
    bus.config(b);
    panel.setBus(&bus);

    {
      auto t = touch.config();
      t.x_min = 0; t.x_max = 799;
      t.y_min = 0; t.y_max = 479;
      t.pin_int = -1;
      t.pin_rst = -1;
#if MFV3_BOARD == BOARD_WAVESHARE_7C
      t.bus_shared = true;   // el expansor comparte el bus
      t.i2c_port = exio::I2C_PORT;
#else
      t.bus_shared = false;
      t.i2c_port = 1;
#endif
      t.offset_rotation = 0;
      t.pin_sda = touchSda;
      t.pin_scl = touchScl;
      t.freq = 400000;
      t.i2c_addr = 0x5D;  // se ajusta en detectTouchAddress()
      touch.config(t);
    }
    panel.setTouch(&touch);
    setPanel(&panel);
  }

  // Brillo 0–255 (0 = apagada), igual en todas las placas.
  void backlight(uint8_t level) {
#if MFV3_BOARD == BOARD_WAVESHARE_7C
    exioWrite(exio::BACKLIGHT, level > 0);
    uint8_t pct = (uint8_t)((level * 100u + 254u) / 255u);
    if (pct < 5) pct = 5;  // el fabricante no baja de 5 %
    uint8_t d[2] = {exio::REG_PWM, (uint8_t)(pct * 255u / 100u)};
    lgfx::i2c::transactionWrite(exio::I2C_PORT, exio::ADDR, d, 2);
#else
    setBrightness(level);
#endif
  }

#if MFV3_BOARD == BOARD_WAVESHARE_7C
  uint16_t exioOut = (1u << exio::TOUCH_RST) | (1u << exio::BACKLIGHT) | (1u << exio::SD_CS);  // altavoz y DO apagados

  void exioWrite(uint8_t pin, bool on) {
    if (on) exioOut |= (uint16_t)(1u << pin);
    else exioOut &= (uint16_t)~(1u << pin);
    uint8_t d[3] = {exio::REG_OUT, (uint8_t)(exioOut & 0xFF), (uint8_t)(exioOut >> 8)};
    lgfx::i2c::transactionWrite(exio::I2C_PORT, exio::ADDR, d, 3);
  }

  // Otros chips del mismo bus I2C (códec de audio ES8389).
  bool i2cWrite(uint8_t addr, const uint8_t* data, uint8_t len) {
    return !lgfx::i2c::transactionWrite(exio::I2C_PORT, addr, data, len).has_error();
  }
  bool i2cReadReg(uint8_t addr, uint8_t reg, uint8_t& value) {
    return !lgfx::i2c::transactionWriteRead(exio::I2C_PORT, addr, &reg, 1, &value, 1).has_error();
  }

  // Lee las 16 patas del expansor (bit n = EXIOn). Devuelve false si el expansor no responde.
  bool exioRead(uint16_t& value) {
    uint8_t reg = exio::REG_IN, buf[2] = {0, 0};
    if (lgfx::i2c::transactionWriteRead(exio::I2C_PORT, exio::ADDR, &reg, 1, buf, 2).has_error()) return false;
    value = (uint16_t)(buf[0] | (buf[1] << 8));
    return true;
  }

  // Antes de lcd.init(): bus I2C, modo de las patas y reset del táctil con INT a nivel bajo (dirección 0x5D),
  // la misma secuencia que el ejemplo 06_TOUCH del fabricante.
  void expanderInit() {
    lgfx::i2c::init(exio::I2C_PORT, exio::SDA, exio::SCL);
    uint8_t m[3] = {exio::REG_MODE, (uint8_t)(exio::OUTPUT_MASK & 0xFF), (uint8_t)(exio::OUTPUT_MASK >> 8)};
    lgfx::i2c::transactionWrite(exio::I2C_PORT, exio::ADDR, m, 3);
    pinMode(exio::TOUCH_INT, OUTPUT);
    exioWrite(exio::TOUCH_RST, false);
    delay(100);
    digitalWrite(exio::TOUCH_INT, LOW);
    delay(100);
    exioWrite(exio::TOUCH_RST, true);
    delay(200);
    pinMode(exio::TOUCH_INT, INPUT);
  }
#endif

  // El GT911 responde en 0x5D o 0x14 según cómo arrancó: se detecta antes de init().
  void detectTouchAddress() {
#if MFV3_BOARD == BOARD_WAVESHARE_7C
    return;  // fijada a 0x5D por el reset de expanderInit()
#endif
    if (touchSda < 0) return;
    Wire.begin(touchSda, touchScl);
    uint8_t addr = 0x5D;
    Wire.beginTransmission(0x5D);
    if (Wire.endTransmission() != 0) {
      Wire.beginTransmission(0x14);
      if (Wire.endTransmission() == 0) addr = 0x14;
    }
    Wire.end();
    auto t = touch.config();
    t.i2c_addr = addr;
    touch.config(t);
  }

#if MFV3_BOARD == BOARD_WAVESHARE_7
  // CH422G: 0x24 = modo (salidas), 0x38 = valor de las salidas EXIO (reset táctil, retroiluminación, reset LCD).
  void waveshareExpanderInit() {
    Wire.begin(touchSda, touchScl);
    Wire.beginTransmission(0x24); Wire.write(0x01); Wire.endTransmission();
    Wire.beginTransmission(0x38); Wire.write(0xFF); Wire.endTransmission();
    Wire.end();
  }
#endif
};

#endif  // MFV3_BOARD_CONFIG_H
