// «MD5» simulado: la misma suma sencilla que sim::fakeMd5.
#pragma once
#include "Update.h"
#include "Arduino.h"
class MD5Builder {
 public:
  void begin() { data_.clear(); }
  void add(const uint8_t* d, size_t n) { data_.insert(data_.end(), d, d + n); }
  void calculate() {}
  String toString() { return String(sim::fakeMd5(data_)); }

 private:
  std::vector<uint8_t> data_;
};
