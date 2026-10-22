// Simulación mínima del sistema de archivos de Arduino-ESP32 (FFat, SD_MMC) para las pruebas en PC.
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#define FILE_READ "r"
#define FILE_WRITE "w"
namespace fs {
struct Disk {
  std::map<std::string, std::vector<uint8_t>> files;
  std::map<std::string, bool> dirs;
  std::string failRenameTo;
};
class File {
 public:
  File() {}
  File(Disk* d, const std::string& p, bool w, bool dir) : st_(std::make_shared<St>()) {
    st_->disk = d; st_->path = p; st_->write = w; st_->dir = dir;
    if (dir) {
      for (auto& f : d->files)
        if (f.first.rfind(p + "/", 0) == 0 && f.first.find('/',p.size()+1)==std::string::npos) st_->entries.push_back(f.first);
      for(auto& entry:d->dirs)if(entry.first.rfind(p+"/",0)==0 && entry.first.find('/',p.size()+1)==std::string::npos)st_->entries.push_back(entry.first);
    } else if (w) {
      d->files[p].clear();
    }
  }
  explicit operator bool() const { return st_ && st_->open; }
  size_t write(const uint8_t* b, size_t n) {
    if (!*this || !st_->write) return 0;
    auto& v = st_->disk->files[st_->path];
    v.insert(v.end(), b, b + n);
    return n;
  }
  size_t read(uint8_t* b, size_t n) {
    if (!*this) return 0;
    auto& v = st_->disk->files[st_->path];
    size_t k = st_->pos >= v.size() ? 0 : std::min(n, v.size() - st_->pos);
    if (k) memcpy(b, v.data() + st_->pos, k);
    st_->pos += k;
    return k;
  }
  int available() { return *this ? (int)(st_->disk->files[st_->path].size() - std::min(st_->pos, st_->disk->files[st_->path].size())) : 0; }
  int read() {
    uint8_t c;
    return read(&c, 1) == 1 ? c : -1;
  }
  size_t print(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  bool seek(uint32_t p) { if (!*this) return false; st_->pos = p; return true; }
  size_t size() { return *this ? st_->disk->files[st_->path].size() : 0; }
  void close() { if (st_) st_->open = false; }
  bool isDirectory() { return st_ && st_->dir; }
  const char* name() { return st_->path.c_str() + st_->path.rfind('/') + 1; }
  File openNextFile() {
    if (!st_ || st_->next >= st_->entries.size()) return File();
    auto name=st_->entries[st_->next++];return File(st_->disk,name,false,st_->disk->dirs.count(name)>0);
  }
 private:
  struct St {
    Disk* disk = nullptr;
    std::string path;
    bool write = false, dir = false, open = true;
    size_t pos = 0, next = 0;
    std::vector<std::string> entries;
  };
  std::shared_ptr<St> st_;
};
class FS {
 public:
  Disk disk;
  bool exists(const char* p) { return disk.files.count(p) || disk.dirs.count(p); }
  bool remove(const char* p) { return disk.files.erase(p) > 0; }
  bool mkdir(const char* p) { disk.dirs[p] = true; return true; }
  bool rename(const char* a, const char* b) {
    if(disk.failRenameTo==b)return false;
    auto it = disk.files.find(a);
    if (it == disk.files.end()) return false;
    disk.files[b] = it->second;
    disk.files.erase(a);
    return true;
  }
  File open(const char* p, const char* mode = FILE_READ) {
    if (disk.dirs.count(p)) return File(&disk, p, false, true);
    bool w = mode[0] == 'w';
    if (!w && !disk.files.count(p)) return File();
    return File(&disk, p, w, false);
  }
};
}  // namespace fs
