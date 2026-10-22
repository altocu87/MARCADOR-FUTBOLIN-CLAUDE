#pragma once
#include "SD_MMC.h"
#include <cstring>
#include <string>
namespace sim {
inline uint64_t sdFatTotal=8ull<<30,sdFatFree=(8ull<<30)-(1ull<<20);
inline bool sdFatInfoOk=true;
inline std::string sdFatPath;
}
inline int esp_vfs_fat_info(const char* path,uint64_t* total,uint64_t* free){
  sim::sdFatPath=path;
  if(!sim::sdCard || !sim::sdFatInfoOk || strcmp(path,"/sdcard"))return -1;
  *total=sim::sdFatTotal;*free=sim::sdFatFree;return 0;
}
