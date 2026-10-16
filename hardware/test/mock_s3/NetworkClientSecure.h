// Cliente HTTPS simulado: el contenido de cada dirección está en sim::web.
#pragma once
#include <map>
#include <string>
#include <cstring>
#include "Arduino.h"
namespace sim {
inline std::map<std::string, std::string> web;  // dirección → contenido
inline bool webDown = false;                     // sin internet
}  // namespace sim
class NetworkClient {
 public:
  std::string body;
  size_t pos = 0;
  size_t available() { return body.size() - pos; }
  size_t readBytes(uint8_t* b, size_t n) {
    size_t k = available() < n ? available() : n;
    memcpy(b, body.data() + pos, k);
    pos += k;
    return k;
  }
};
class NetworkClientSecure : public NetworkClient {
 public:
  bool bundle = false;
  void useBuiltinCACertBundle() { bundle = true; }
};
