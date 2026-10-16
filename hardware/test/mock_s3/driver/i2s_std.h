// Simulación de la salida de audio I2S: guarda lo que se «oye» y deja llenar un colchón de 8 × 500 tramas.
#pragma once
#include <stdint.h>
#include <vector>
#include "LovyanGFX.hpp"
typedef int esp_err_t;
#define ESP_OK 0
typedef void* i2s_chan_handle_t;
enum i2s_port_t { I2S_NUM_0, I2S_NUM_1 };
enum i2s_role_t { I2S_ROLE_MASTER };
struct i2s_chan_config_t { int id; int role; uint32_t dma_desc_num, dma_frame_num; bool auto_clear; };
#define I2S_CHANNEL_DEFAULT_CONFIG(p, r) i2s_chan_config_t{p, r, 6, 240, false}
struct i2s_std_clk_config_t { uint32_t sample_rate_hz; };
struct i2s_std_slot_config_t { int bits, mode; };
struct i2s_std_gpio_config_t { int mclk, bclk, ws, dout, din; struct { bool mclk_inv, bclk_inv, ws_inv; } invert_flags; };
struct i2s_std_config_t { i2s_std_clk_config_t clk_cfg; i2s_std_slot_config_t slot_cfg; i2s_std_gpio_config_t gpio_cfg; };
#define I2S_STD_CLK_DEFAULT_CONFIG(r) i2s_std_clk_config_t{r}
#define I2S_DATA_BIT_WIDTH_16BIT 16
#define I2S_SLOT_MODE_STEREO 2
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(b, m) i2s_std_slot_config_t{b, m}
#define I2S_GPIO_UNUSED -1
namespace sim {
inline std::vector<int16_t> heard;   // muestras del canal izquierdo enviadas a los altavoces
inline size_t i2sRoom = 4000;        // tramas que caben ahora en el colchón
inline uint32_t i2sRate = 0;
}
inline esp_err_t i2s_new_channel(const i2s_chan_config_t*, i2s_chan_handle_t* tx, i2s_chan_handle_t*) { *tx = (void*)1; return ESP_OK; }
inline esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t, const i2s_std_config_t* c) { sim::i2sRate = c->clk_cfg.sample_rate_hz; return ESP_OK; }
inline esp_err_t i2s_channel_enable(i2s_chan_handle_t) { return ESP_OK; }
inline esp_err_t i2s_channel_write(i2s_chan_handle_t, const void* src, size_t bytes, size_t* written, uint32_t) {
  size_t frames = bytes / 4;
  if (frames > sim::i2sRoom) frames = sim::i2sRoom;
  const int16_t* s = (const int16_t*)src;
  for (size_t i = 0; i < frames; i++) sim::heard.push_back(s[2 * i]);
  sim::i2sRoom -= frames;
  *written = frames * 4;
  return ESP_OK;
}
