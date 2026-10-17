#pragma once
#include "FS.h"
namespace sim { inline bool ffatOk = true; }
struct FFatFS : fs::FS {
  bool begin(bool = false) { return sim::ffatOk; }
  size_t freeBytes() { return 9u * 1024 * 1024; }
};
inline FFatFS FFat;
