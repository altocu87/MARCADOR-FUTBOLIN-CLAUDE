// Salida I2S simulada: cuenta lo que se escribe.
#pragma once
#include <stdint.h>
#include <stddef.h>
#ifndef ESP_OK
#define ESP_OK 0
typedef int esp_err_t;
#endif
#define I2S_GPIO_UNUSED GPIO_NUM_NC
#define I2S_NUM_1 1
#define I2S_ROLE_MASTER 0
#define I2S_DATA_BIT_WIDTH_16BIT 16
#define I2S_SLOT_MODE_STEREO 2
typedef struct i2s_chan* i2s_chan_handle_t;
struct i2s_chan_config_t { int id, role; bool auto_clear; };
struct i2s_std_clk_config_t { uint32_t sample_rate_hz; };
struct i2s_std_slot_config_t { int bits, mode; };
struct i2s_invert_t { bool mclk_inv, bclk_inv, ws_inv; };
struct i2s_std_gpio_config_t { gpio_num_t mclk, bclk, ws, dout, din; i2s_invert_t invert_flags; };
struct i2s_std_config_t { i2s_std_clk_config_t clk_cfg; i2s_std_slot_config_t slot_cfg; i2s_std_gpio_config_t gpio_cfg; };
#define I2S_CHANNEL_DEFAULT_CONFIG(i, r) {i, r, false}
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) {(uint32_t)(rate)}
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(b, m) {b, m}
namespace sim {
inline uint32_t i2sRate = 0;
inline size_t i2sBytes = 0;
}  // namespace sim
inline esp_err_t i2s_new_channel(const i2s_chan_config_t*, i2s_chan_handle_t* tx, i2s_chan_handle_t*) {
  *tx = (i2s_chan_handle_t)1;
  return ESP_OK;
}
inline esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t, const i2s_std_config_t* c) { sim::i2sRate = c->clk_cfg.sample_rate_hz; return ESP_OK; }
inline esp_err_t i2s_channel_enable(i2s_chan_handle_t) { return ESP_OK; }
inline esp_err_t i2s_channel_disable(i2s_chan_handle_t) { return ESP_OK; }
inline esp_err_t i2s_channel_reconfig_std_clock(i2s_chan_handle_t, const i2s_std_clk_config_t* c) { sim::i2sRate = c->sample_rate_hz; return ESP_OK; }
inline esp_err_t i2s_channel_write(i2s_chan_handle_t, const void*, size_t n, size_t* done, uint32_t) {
  sim::i2sBytes += n;
  *done = n;
  return ESP_OK;
}
