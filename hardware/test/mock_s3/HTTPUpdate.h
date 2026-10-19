// Autoactualización simulada: comprueba que la dirección existe y que el MD5 (sim::fakeMd5) cuadra.
#pragma once
#include <functional>
#include "HTTPClient.h"
#include "Update.h"
enum HTTPUpdateResult { HTTP_UPDATE_FAILED, HTTP_UPDATE_NO_UPDATES, HTTP_UPDATE_OK };
namespace sim { inline std::string selfUpdatedFrom; }
class HTTPUpdate {
 public:
  void rebootOnUpdate(bool r) { reboot_ = r; }
  void setMD5sum(const String& m) { md5_ = m.c_str(); }
  void onProgress(std::function<void(int, int)> cb) { cb_ = cb; }
  HTTPUpdateResult update(NetworkClient& c, const String& u) {
    if (!static_cast<NetworkClientSecure&>(c).bundle) sim::insecure = true;
    if (reboot_ || sim::webDown) return HTTP_UPDATE_FAILED;
    auto it = sim::web.find(u.c_str());
    if (it == sim::web.end()) return HTTP_UPDATE_FAILED;
    std::vector<uint8_t> v(it->second.begin(), it->second.end());
    if (sim::fakeMd5(v) != md5_) return HTTP_UPDATE_FAILED;
    sim::now += sim::webLatencyMs;
    if (cb_) { cb_(0, (int)v.size()); cb_((int)v.size(), (int)v.size()); }
    sim::selfUpdatedFrom = u.c_str();
    return HTTP_UPDATE_OK;
  }

 private:
  bool reboot_ = true;
  std::string md5_;
  std::function<void(int, int)> cb_;
};
inline HTTPUpdate httpUpdate;
