// Modo compatible de las pruebas anteriores y modo con cola DMA/reloj explícitos.
#pragma once
#include <stdint.h>
#include <vector>
#include <algorithm>
#include "LovyanGFX.hpp"
typedef int esp_err_t;
#define ESP_OK 0
#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif
#ifndef ESP_ERR_TIMEOUT
#define ESP_ERR_TIMEOUT 0x107
#endif
typedef void* i2s_chan_handle_t;
enum i2s_port_t { I2S_NUM_0,I2S_NUM_1 };
enum i2s_role_t { I2S_ROLE_MASTER };
struct i2s_chan_config_t {int id;int role;uint32_t dma_desc_num,dma_frame_num;bool auto_clear;};
#define I2S_CHANNEL_DEFAULT_CONFIG(p,r) i2s_chan_config_t{p,r,6,240,false}
struct i2s_std_clk_config_t {uint32_t sample_rate_hz;};
struct i2s_std_slot_config_t {int bits,mode;};
struct i2s_std_gpio_config_t {int mclk,bclk,ws,dout,din;struct {bool mclk_inv,bclk_inv,ws_inv;} invert_flags;};
struct i2s_std_config_t {i2s_std_clk_config_t clk_cfg;i2s_std_slot_config_t slot_cfg;i2s_std_gpio_config_t gpio_cfg;};
#define I2S_STD_CLK_DEFAULT_CONFIG(r) i2s_std_clk_config_t{r}
#define I2S_DATA_BIT_WIDTH_16BIT 16
#define I2S_SLOT_MODE_STEREO 2
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(b,m) i2s_std_slot_config_t{b,m}
#define I2S_GPIO_UNUSED -1
namespace sim {
inline std::vector<int16_t> heard,preload,i2sDma;
inline size_t i2sRoom=4000,i2sDmaCapacityFrames=2000,i2sPreloadLimit=~size_t(0);
inline uint32_t i2sRate=0;
inline unsigned i2sStops=0,i2sUnderrunFrames=0;
inline bool failI2sStop=false,i2sRealtime=false,i2sEnabled=false;
inline esp_err_t i2sWriteResult=ESP_OK,i2sPreloadResult=ESP_OK,i2sEnableResult=ESP_OK;
inline void advanceI2sFrames(size_t frames){
  if(!i2sEnabled)return;
  size_t n=std::min(frames,i2sDma.size());heard.insert(heard.end(),i2sDma.begin(),i2sDma.begin()+n);
  i2sDma.erase(i2sDma.begin(),i2sDma.begin()+n);
  if(n<frames){heard.insert(heard.end(),frames-n,0);i2sUnderrunFrames+=(unsigned)(frames-n);}
}
}
inline esp_err_t i2s_new_channel(const i2s_chan_config_t* c,i2s_chan_handle_t* tx,i2s_chan_handle_t*){
  *tx=(void*)1;sim::i2sDmaCapacityFrames=c->dma_desc_num*c->dma_frame_num;sim::preload.clear();sim::i2sDma.clear();sim::i2sEnabled=false;return ESP_OK;
}
inline esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t,const i2s_std_config_t* c){sim::i2sRate=c->clk_cfg.sample_rate_hz;return ESP_OK;}
inline esp_err_t i2s_channel_enable(i2s_chan_handle_t){
  if(sim::i2sEnableResult!=ESP_OK)return sim::i2sEnableResult;
  sim::i2sEnabled=true;
  auto& target=sim::i2sRealtime?sim::i2sDma:sim::heard;
  target.insert(target.end(),sim::preload.begin(),sim::preload.end());sim::preload.clear();return ESP_OK;
}
inline esp_err_t i2s_channel_disable(i2s_chan_handle_t){
  if(sim::failI2sStop)return ESP_FAIL;
  sim::i2sEnabled=false;sim::preload.clear();sim::i2sDma.clear();sim::i2sStops++;return ESP_OK;
}
inline esp_err_t i2s_del_channel(i2s_chan_handle_t){if(sim::i2sEnabled)return ESP_FAIL;sim::preload.clear();sim::i2sDma.clear();return ESP_OK;}
inline esp_err_t i2s_channel_preload_data(i2s_chan_handle_t,const void* src,size_t bytes,size_t* loaded){
  *loaded=0;if(sim::i2sPreloadResult!=ESP_OK)return sim::i2sPreloadResult;
  if(sim::i2sEnabled)return ESP_FAIL;
  size_t frames=bytes/4,room=sim::i2sDmaCapacityFrames-sim::preload.size();
  frames=std::min(frames,std::min(room,sim::i2sPreloadLimit/4));
  const int16_t* s=(const int16_t*)src;for(size_t i=0;i<frames;i++)sim::preload.push_back(s[2*i]);
  *loaded=frames*4;return ESP_OK;
}
inline esp_err_t i2s_channel_write(i2s_chan_handle_t,const void* src,size_t bytes,size_t* written,uint32_t){
  *written=0;if(sim::i2sWriteResult!=ESP_OK)return sim::i2sWriteResult;
  if(!sim::i2sEnabled)return ESP_FAIL;
  size_t frames=bytes/4;
  size_t room=sim::i2sRealtime?sim::i2sDmaCapacityFrames-sim::i2sDma.size():sim::i2sRoom;
  frames=std::min(frames,room);const int16_t* s=(const int16_t*)src;
  auto& target=sim::i2sRealtime?sim::i2sDma:sim::heard;
  for(size_t i=0;i<frames;i++)target.push_back(s[2*i]);
  if(!sim::i2sRealtime)sim::i2sRoom-=frames;
  *written=frames*4;return sim::i2sRealtime && frames*4<bytes?ESP_ERR_TIMEOUT:ESP_OK;
}
