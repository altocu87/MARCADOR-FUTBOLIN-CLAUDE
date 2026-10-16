#pragma once
#include "NetworkClientSecure.h"
namespace sim { inline std::vector<std::string> fetched; inline bool insecure = false; }
class HTTPClient {
 public:
  void setTimeout(uint16_t) {}
  bool begin(NetworkClient& c, const String& u) {
    client_ = &c;
    url_ = u.c_str();
    if (!static_cast<NetworkClientSecure&>(c).bundle) sim::insecure = true;  // siempre con certificados
    return true;
  }
  int GET() {
    sim::fetched.push_back(url_);
    if (sim::webDown) return -1;
    auto it = sim::web.find(url_);
    if (it == sim::web.end()) return 404;
    client_->body = it->second;
    client_->pos = 0;
    return 200;
  }
  String getString() { return String(client_->body); }
  NetworkClient* getStreamPtr() { return client_; }
  void end() {}

 private:
  NetworkClient* client_ = nullptr;
  std::string url_;
};
