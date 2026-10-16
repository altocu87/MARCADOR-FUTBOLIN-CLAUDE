#pragma once
#include "FS.h"
struct SDMMCSim {
  bool setPins(int, int, int) { return true; }
  bool begin(const char*, bool) { return sim::sdPresent; }
  uint64_t cardSize() { return 32ULL * 1024 * 1024 * 1024; }
  File open(const char* p) {
    std::string path(p);
    if (sim::sdFiles.count(path)) return File(path, false);
    for (auto& kv : sim::sdFiles)
      if (kv.first.rfind(path + "/", 0) == 0) return File(path, true);
    return File();
  }
};
inline SDMMCSim SD_MMC;
