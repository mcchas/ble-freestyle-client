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
#include "SyslogStream.h"


size_t SyslogStream::write(uint8_t *buffer, size_t size)
{
    size_t n = 0;
    while (size--)
    {
        n++;
        SyslogStream::write(*buffer++);
    }
    return n;
}

size_t SyslogStream::write(const uint8_t *buffer, size_t size)
{
    if (size == 0)
        return 0;
    size_t n = 0;
    while (size--)
    {
        n++;
        SyslogStream::write(*buffer++);
    }
    return n;
}

size_t SyslogStream::write(uint8_t c) {

  if (at >= sizeof(logbuff) - 1) {
    Log.println("Purged logbuffer (should never happen)");
    at = 0;
  };

  if (c >= 32 && c < 128)
    logbuff[ at++ ] = c;

  if (c == '\n' || at >= sizeof(logbuff) - 1) {

    logbuff[at++] = 0;
    at = 0;

    if (_logging) {
      WiFiUDP syslog;

      if (syslog.begin(_syslogPort)) {
        if (_dest)
          syslog.beginPacket(_dest, _syslogPort);
        else
          syslog.beginPacket(WiFi.gatewayIP(), _syslogPort);

        if (_raw)
          syslog.printf("%s\n", logbuff);
        else {
#ifdef ESP32
          syslog.printf("<134> Jan 01 00:00:00 1: %s", logbuff);
#else
          syslog.printf("<134> %s %s %s", p, identifier().c_str(), logbuff);
#endif
        };
        syslog.endPacket();
      };
    };
  };
  return 1;
}