#include "config.h"
#include <ESPAsyncWebServer.h>

class Web {
public:
  void begin(AsyncWebServer *server, EspConfig *cfg),
       actionCallback(std::function<void(uint8_t)> a_callback);

private:
  AsyncWebServer *_http;
  EspConfig *_cfg;
  std::function<void(uint8_t)> callback;
  bool _authRequired = false;
};