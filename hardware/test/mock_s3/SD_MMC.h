#pragma once
#include "FS.h"
enum sdcard_type_t { CARD_NONE, CARD_MMC, CARD_SD, CARD_SDHC };
namespace sim { inline bool sdCard = false; }
struct SDMMCFS : fs::FS {
  bool setPins(int, int, int) { return true; }
  bool begin(const char*, bool) { return sim::sdCard; }
  sdcard_type_t cardType() { return sim::sdCard ? CARD_SDHC : CARD_NONE; }
  uint64_t totalBytes() { return 8ull << 30; }
  uint64_t usedBytes() { return 1ull << 20; }
};
inline SDMMCFS SD_MMC;
