// Sistema de archivos simulado (tarjeta SD de la pantalla de 7"): archivos en memoria en sim::sdFiles.
#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include "Arduino.h"
namespace sim {
inline bool sdPresent = true;
inline std::map<std::string, std::vector<uint8_t>> sdFiles;  // ruta completa → contenido
}  // namespace sim
class File {
 public:
  File() {}
  File(const std::string& path, bool dir) : path_(path), dir_(dir), ok_(true) {}
  explicit operator bool() const { return ok_; }
  bool isDirectory() const { return dir_; }
  const char* name() const {
    size_t p = path_.rfind('/');
    return path_.c_str() + (p == std::string::npos ? 0 : p + 1);
  }
  size_t size() const { return dir_ ? 0 : sim::sdFiles[path_].size(); }
  int read(uint8_t* buf, size_t n) {
    auto& d = sim::sdFiles[path_];
    size_t k = pos_ >= d.size() ? 0 : (d.size() - pos_ < n ? d.size() - pos_ : n);
    memcpy(buf, d.data() + pos_, k);
    pos_ += k;
    return (int)k;
  }
  bool seek(uint32_t p) { pos_ = p; return true; }
  void close() {}
  // Siguiente entrada de una carpeta (archivos y subcarpetas de primer nivel).
  File openNextFile() {
    std::string pre = path_ + "/";
    std::vector<std::string> names;
    for (auto& kv : sim::sdFiles) {
      if (kv.first.rfind(pre, 0) != 0) continue;
      std::string rest = kv.first.substr(pre.size());
      size_t slash = rest.find('/');
      std::string n = slash == std::string::npos ? rest : rest.substr(0, slash);
      if (names.empty() || names.back() != n) names.push_back(n);
    }
    if (next_ >= names.size()) return File();
    std::string full = pre + names[next_++];
    return File(full, !sim::sdFiles.count(full));
  }

 private:
  std::string path_;
  bool dir_ = false, ok_ = false;
  size_t pos_ = 0, next_ = 0;
};
