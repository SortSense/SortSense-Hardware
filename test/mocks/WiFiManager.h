#pragma once
#include <stdint.h>
#include <string>

class WiFiManager {
private:
  bool _blocking = true;
  uint32_t _timeout = 0;

public:
  void setConfigPortalBlocking(bool blocking) { _blocking = blocking; }
  void setConfigPortalTimeout(uint32_t seconds) { _timeout = seconds; }
  bool autoConnect(const char * /*apName*/) { return true; }
  void process() {}
  bool isBlocking() const { return _blocking; }
  uint32_t getTimeout() const { return _timeout; }
};
