#include "web.h"
#include "config.h"
#include "proto/cmd.pb.h"
#include <Arduino.h>

void Web::actionCallback(std::function<void(uint8_t)> a_callback) { callback = a_callback; }

void Web::begin(AsyncWebServer *server, EspConfig *cfg) {
  _http = server;
  _cfg = cfg;

  _http->on("/unlock", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (!request->authenticate(_cfg->http_user, _cfg->http_pass))
      return request->requestAuthentication();
    request->send(200, "text/html", "<html><head><meta http-equiv=\"refresh\" content=\"3;URL='debug'\"/></head><body><h1>Unlocking. Redirecting in 1 second...</h1></body></html>\n");
    callback(cmd_State_UNLOCKED);
  });

  _http->on("/lock", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (!request->authenticate(_cfg->http_user, _cfg->http_pass))
      return request->requestAuthentication();
    request->send(200, "text/html", "<html><head><meta http-equiv=\"refresh\" content=\"3;URL='debug'\"/></head><body><h1>Locking. Redirecting in 1 second...</h1></body></html>\n");
    callback(cmd_State_LOCKED_PRIVACY);
  });

  _http->on("/deadlock", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (!request->authenticate(_cfg->http_user, _cfg->http_pass))
      return request->requestAuthentication();
    request->send(200, "text/html", "<html><head><meta http-equiv=\"refresh\" content=\"3;URL='debug'\"/></head><body><h1>Deadlocking. Redirecting in 1 second...</h1></body></html>\n");
    callback(cmd_State_LOCKED_DEADLOCK);
  });

  _http->on("/reboot", HTTP_POST, [&](AsyncWebServerRequest *request) {
    if (!request->authenticate(_cfg->http_user, _cfg->http_pass))
      return request->requestAuthentication();
    request->send(200, "text/html", "<html><head><meta http-equiv=\"refresh\" content=\"10;URL='/'\"/></head><body><h1>Reboot. Refreshing in 10 seconds...</h1></body></html>\n");
    ESP.restart();
  });

  _http->on("/config", HTTP_POST, [&](AsyncWebServerRequest *request) {
    if (!request->authenticate(_cfg->http_user, _cfg->http_pass))
      return request->requestAuthentication();
    String message;
    bool save = false;
    if (request->hasParam("hostname", true)) {
      message = request->getParam("hostname", true)->value();
      strncpy(_cfg->host_name, message.c_str(), message.length());
      save = true;
    }
    if (request->hasParam("aes_key", true)) {
      message = request->getParam("aes_key", true)->value();
      strncpy(_cfg->aes_key, message.c_str(), message.length());
      save = true;
    }
    if (request->hasParam("ble_mac", true)) {
      message = request->getParam("ble_mac", true)->value();
      strncpy(_cfg->ble_mac, message.c_str(), message.length());
      save = true;
    }
    if (request->hasParam("mqtt_server", true)) {
      message = request->getParam("mqtt_server", true)->value();
      strncpy(_cfg->mqtt_server, message.c_str(), message.length());
      save = true;
    }
    if (request->hasParam("mqtt_topic", true)) {
      message = request->getParam("mqtt_topic", true)->value();
      strncpy(_cfg->mqtt_topic, message.c_str(), message.length());
      save = true;
    }
    if (request->hasParam("http_user", true)) {
      message = request->getParam("http_user", true)->value();
      strncpy(_cfg->http_user, message.c_str(), message.length());
      save = true;
    }
    if (request->hasParam("http_pass", true)) {
      message = request->getParam("http_pass", true)->value();
      strncpy(_cfg->http_pass, message.c_str(), message.length());
      save = true;
    }
    if (request->hasParam("syslog_server", true)) {
      message = request->getParam("syslog_server", true)->value();
      strncpy(_cfg->syslog_server, message.c_str(), message.length());
      save = true;
    }
    if (!save) {
      request->send(404, "text/plain", "not found\n");
      return;
    }
    request->send(200, "text/html", "<html><head><meta http-equiv=\"refresh\" content=\"3;URL='setup'\"/></head><body><h1>Saved. Redirecting in 3 seconds...</h1></body></html>\n");
    _cfg->saveConfig();
  });

  _http->on("/config", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (!request->authenticate(_cfg->http_user, _cfg->http_pass))
      return request->requestAuthentication();
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->printf("{\"hostname\": \"%s\",", _cfg->host_name);
    response->printf("\"aes_key\": \"%s\",", _cfg->aes_key);
    response->printf("\"ble_mac\": \"%s\",", _cfg->ble_mac);
    response->printf("\"mqtt_server\": \"%s\",", _cfg->mqtt_server);
    response->printf("\"mqtt_topic\": \"%s\"", _cfg->mqtt_topic);
    response->printf("\"http_user\": \"%s\"", _cfg->http_user);
    response->printf("\"http_pass\": \"%s\"}", _cfg->http_pass);
    response->printf("\"syslog_server\": \"%s\"}", _cfg->syslog_server);
    request->send(response);
  });

  _http->on("/config", HTTP_DELETE, [&](AsyncWebServerRequest *request) {
    if (!request->authenticate(_cfg->http_user, _cfg->http_pass))
      return request->requestAuthentication();
    _cfg->resetConfig();
    request->send(200, "text/plain", "config deleted");
  });

  _http->on("/setup", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (!request->authenticate(_cfg->http_user, _cfg->http_pass))
      return request->requestAuthentication();
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print("<!DOCTYPE html><html><head><title>Freestyle Trilock Setup</title><style>");
    response->print("div,fieldset,input,select { padding: 7px; font-size: 1em;}");
    response->print("p {margin: 0.5em 0;}");
    response->print("input {width: 100%; box-sizing: border-box; -webkit-box-sizing: border-box; -moz-box-sizing:");
    response->print("border-box; background: #f9f7f7fc; color: #000000;}");
    response->print("body {text-align: left;font-family: verdana,sans-serif;background: #252525;}");
    response->print("button { border: 0; border-radius: 0.3rem; background: #1fa4ec4b; color: #faffff;");
    response->print("line-height: 2.4rem; font-size: 1.2rem; width: 100%; -webkit-transition-duration: 0.4s; transition-duration: 0.4s; cursor: pointer;}");
    response->print("button:hover {background: #0e70a4;}");
    response->print("</style></head><body>");
    response->print("<div style='text-align:left;display:inline-block;color:#eaeaea;min-width:340px;'>");
    response->print("<fieldset>");
    response->print("<form action=\"/config\" method=\"post\">");
    response->print("<label for=\"hostname\">Hostname:</label><br>");
    response->printf("<input type=\"text\" id=\"hostname\" name=\"hostname\" value=\"%s\"><br>", _cfg->host_name);
    response->print("<label for=\"aes_key\">AES Key (base64):</label><br>");
    response->printf("<input type=\"text\" id=\"aes_key\" name=\"aes_key\" value=\"%s\"><br><br>", _cfg->aes_key);
    response->print("<label for=\"ble_mac\">Lock MAC Address:</label><br>");
    response->printf("<input type=\"text\" id=\"ble_mac\" name=\"ble_mac\" value=\"%s\"><br><br>", _cfg->ble_mac);
    response->print("<label for=\"mqtt_server\">MQTT Server:</label><br>");
    response->printf("  <input type=\"text\" id=\"mqtt_server\" name=\"mqtt_server\" value=\"%s\"><br><br>", _cfg->mqtt_server);
    response->print("  <label for=\"mqtt_topic\">MQTT Topic:</label><br>");
    response->printf("  <input type=\"text\" id=\"mqtt_topic\" name=\"mqtt_topic\" value=\"%s\"><br><br>", _cfg->mqtt_topic);
    response->print("  <label for=\"http_user\">HTTP Username:</label><br>");
    response->printf("  <input type=\"text\" id=\"http_user\" name=\"http_user\" value=\"%s\"><br><br>", _cfg->http_user);
    response->print("  <label for=\"http_pass\">HTTP Password:</label><br>");
    response->printf("  <input type=\"text\" id=\"http_pass\" name=\"http_pass\" value=\"%s\"><br><br>", _cfg->http_pass);
    response->print("  <label for=\"syslog_server\">Syslog Server:</label><br>");
    response->printf("  <input type=\"text\" id=\"syslog_server\" name=\"syslog_server\" value=\"%s\"><br><br>", _cfg->syslog_server);
    response->print("<button name='save' type='submit'>Save</button>");
    response->print("</form><br>");
    response->print("<form action=\"/reboot\" method=\"post\">");
    response->print("<button>Reboot</button>");
    response->print("</form><br>");
    response->print("<form action=\"/\">");
    response->print("<button>Home</button>");
    response->print("</form>");
    response->print("</fieldset></div></body></html>");
    request->send(response);
  });

  _http->on("/", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (!request->authenticate(_cfg->http_user, _cfg->http_pass))
      return request->requestAuthentication();
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print("<!DOCTYPE html><html><head><title>Freestyle Trilock</title>");
    response->print("<style>");
    response->print("  div { padding: 5px; font-size: 1em;}");
    response->print("  p { margin: 0.5em 0;}");
    response->print("  body {text-align: left;font-family: verdana, sans-serif; background: #252525;}");
    response->print("  button { border: 0; border-radius: 0.3rem; background: #1fa4ec4b; color: #faffff;");
    response->print("           line-height: 2.4rem; font-size: 1.2rem; width: 100%; -webkit-transition-duration: 0.4s; transition-duration: 0.4s; cursor: pointer;}");
    response->print("  button:hover {background: #0e70a4;}");
    response->print("</style>");
    response->print("</head><body><div style='text-align:left;display:inline-block;color:#eaeaea;min-width:340px;'>");
    response->print("  <div style='text-align:center;color:#eaeaea;'><h4>Freestyle Trilock</h4></div>");
    response->print("  <form action='lock' method='get'><button>Privacy Lock</button></form>");
    response->print("  <p><form action='deadlock' method='get'><button>Deadlock</button></form></p>");
    response->print("  <p><form action='unlock' method='get'><button>Unlock</button></form></p>");
    response->print("  <div style=\"display: block;\"></div>");
    response->print("  <p><form action='setup'><button>Setup</button></form></p>");
    response->print("  <p><form action='debug' method='get'><button>Logs</button></form></p>");
    response->print("</div></body></html>");
    request->send(response);
  });

  _http->on("/connect", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (!request->authenticate(_cfg->http_user, _cfg->http_pass))
      return request->requestAuthentication();
    request->send(200, "text/plain", "connecting");
    callback(CB_CONNECT);
  });
};