/* Copyright 2008, 2012-2022 Dirk-Willem van Gulik <dirkx(at)webweaving(dot)org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Library that provides a fanout, or T-flow; so that output or logs do 
 * not just got to the serial port; but also to a configurable mix of a 
 * telnetserver, a webserver, syslog or MQTT.
 */

#include "TLog.h"
#include "WebSerialStream.h"

size_t WebSerialStream::write(uint8_t c) {
  if(c=='\0')
    return 0;
  _buff[_at % sizeof(_buff)] = c;
  _at++;
  return 1;
}

size_t WebSerialStream::write(uint8_t *buffer, size_t size) {
  size_t n = 0;
  while(size--) {
    n++;
    _buff[_at % sizeof(_buff)] = *buffer++;
    _at++;
  }
  return n;
}

size_t WebSerialStream::write(const uint8_t *buffer, size_t size) {
  if (size==0)
    return 0;
  size_t n = 0;
  while(size--) {
      n++;
    _buff[_at % sizeof(_buff)] = *buffer++;
    _at++;
  }
  return n;
}

WebSerialStream::~WebSerialStream() {
}

void WebSerialStream::stop() {
}

void WebSerialStream::loop() {
}

void WebSerialStream::begin(AsyncWebServer *server) {
  
  _server = server;

  // _server->on("/debug",[this]() {
  _server->on("/debug", HTTP_GET, [&](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print("<html><head><title>log</title></head>");
    response->print("<style>");
    response->print(  "#log { font-family: 'Courier New', monospace; white-space: pre; }");
    response->print(  "textarea { resize: vertical; width: 98%; height: 90%; padding: 5px; overflow: auto; background: #1f1f1f; color: #65c115;}");
    response->print(  "body { text-align: center; font-family: verdana,sans-serif; background: #252525;}");
    response->print("button { border: 0; border-radius: 0.3rem; background: #1fa4ec4b; color: #faffff;");
    response->print("line-height: 2.4rem; font-size: 1.2rem; width: 100%; -webkit-transition-duration: 0.4s; transition-duration: 0.4s; cursor: pointer;}");
    response->print("button:hover {background: #0e70a4;}");
    response->print("</style>");
    response->print("<script language=javascript>");
    response->print(      "var at = 0;");
    response->print("function f() { fetch('log?at='+at).");
    response->print(              "then(");
    response->print(      "r => { return r.json(); }");
    response->print(              ").then( ");
    response->print(                  "j => { " );
    response->print(      " var isAtEnd = (window.innerHeight + window.pageYOffset) >= document.body.offsetHeight - 4; " );
    response->print(                        " document.getElementById('log').innerHTML += j.buff; ");
    response->print(                        " at= j.at; ");
    response->print(                        " if (isAtEnd) window.scrollTo(0,document.body.scrollHeight); ");
    response->print(                  "}");
    response->print(  ").catch( e => { console.log(e); } ");
    response->print(              ");");
    response->print(      "};");
    response->print(      "window.onLoad = setInterval(f, 500);");
    response->print("</script>");
    response->print("<body>");
    response->print("<textarea readonly id='log' cols='340' wrap='off'></textarea>");
    response->print("<div style='text-align:left;display:inline-block;color:#eaeaea;min-width:340px;'>");
    response->print("<form action=\"/\"><button>Home</button></form></div>");
    response->print("</body></html>");
    request->send(response);
  });


  _server->on("/log", HTTP_GET, [&](AsyncWebServerRequest *request) {

    if (!request->hasArg("at")) {
       request->send(400, "text/plain", "Missing at argument.");
       return;
    };
    unsigned long prevAt= request->arg("at").toInt();
    String out = "{\"at\":" + String(_at) + ",\"buff\":\"";

    // reset browsers from the future (e.g. after a reset)
    if (prevAt > _at) {
        out += "<font color=red><hr><i>.. log reset..</i></font><hr>";
    	prevAt = _at;
    };
    if (_at > sizeof(_buff) && prevAt < _at - sizeof(_buff)) {
        out += "<font color=red><hr><i>.. skipping " + 
                String(_at - sizeof(_buff) - prevAt) +
                " bytes of log - no longer in buffer ..</i><hr></font>";
        prevAt = _at - sizeof(_buff);
    };
    for(;prevAt != _at; prevAt++) {
       char c = _buff[prevAt % sizeof(_buff)];
       switch(c) {
       case '<': out += "&lt;"; break;
       case '>': out += "&gt;"; break;
       case '\b': out += "\\b"; break;
       case '\n': out += "\\n"; break;
       case '\r': out += "\\r"; break;
       case '\f': out += "\\f"; break;
       case '\t': out += "\\t"; break;
       case '"' : out += "\\\""; break;
       case '\\': out += "\\\\"; break;
       default  : out += c; break;
       };
    };
    out += "\"}";
    request->send(200, "application/json", out);
  });
};
