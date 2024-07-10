#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <base64.h>
#include <config.h>

auto EspConfig::resetConfig() -> bool { return SPIFFS.remove("/config.json"); }

auto EspConfig::saveConfig() -> bool {

  Log.println("Saving config..");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();
  json["hostname"] = host_name;
  json["mqtt_server"] = mqtt_server;
  json["mqtt_topic"] = mqtt_topic;
  json["aes_key"] = aes_key;
  json["ble_mac"] = ble_mac;
  json["http_user"] = http_user;
  json["http_pass"] = http_pass;
  json["syslog_server"] = syslog_server;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Log.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(Serial);
  Log.println("");
  json.printTo(configFile);
  configFile.close();
  jsonBuffer.clear();
  Log.println("Saved");
  return true;
}

EspConfig::EspConfig() = default;

auto EspConfig::initEspConfig() -> bool {

  if (SPIFFS.begin(true)) {
    Log.println("Mounted SPIFFS file system");
    if (SPIFFS.exists("/config.json")) {
      // file exists, reading and loading
      Log.println("Reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        // Log.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(buf.get());
        if (json.success()) {
          Log.print("Config: ");
          json.printTo(Log);
          Log.println();

          if (json.containsKey("hostname")) {
            strcpy(host_name, json["hostname"]);
          } else {
            strncpy(host_name, HOSTNAME, 20);
          }

          if (json.containsKey("mqtt_server")) {
            strcpy(mqtt_server, json["mqtt_server"]);
          } else {
            strncpy(mqtt_server, MQTT_SERVER, 40);
          }

          if (json.containsKey("mqtt_topic")) {
            strcpy(mqtt_topic, json["mqtt_topic"]);
          } else {
            strncpy(mqtt_topic, "freestyle", 9);
          }

          if (json.containsKey("aes_key")) {
            strcpy(aes_key, json["aes_key"]);
          } else {
            strncpy(aes_key, AES_KEY, 89);
          }

          if (json.containsKey("ble_mac")) {
            strcpy(ble_mac, json["ble_mac"]);
          } else {
            strncpy(aes_key, BLE_MAC, 17);
          }

          if (json.containsKey("http_user")) {
            strcpy(http_user, json["http_user"]);
          } else {
            strncpy(http_user, "user", 17);
          }

          if (json.containsKey("http_pass")) {
            strcpy(http_pass, json["http_pass"]);
          } else {
            strncpy(http_pass, "pass", 17);
          }

          if (json.containsKey("syslog_server")) {
            strcpy(syslog_server, json["syslog_server"]);
          } else {
            strncpy(syslog_server, SYSLOG_SERVER, 40);
          }

        } else {
          Log.println("Failed to load json config");
          return false;
        }
      }
    }
  } else {
    Log.println("Failed to mount SPIFFS");
    return false;
  }
  return true;
}