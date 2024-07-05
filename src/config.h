#pragma once
#include <Arduino.h>
#include <FS.h>
#include <TLog.h>

#define PROJECT_NAME "wt32-freestyle-client"
#define PROJECT_VERSION "1.0"
#define PROJECT_LOCATION "https://github.com/mcchas/wt32-freestyle-client.git"
#define HOSTNAME "fsclient-\0"
#define MQTT_SERVER "10.1.1.1"
#define SYSLOG_SERVER "10.1.1.1"
#define AES_KEY "bm9rZXlzZXQ="
#define BLE_MAC "a1:b2:c3:d4:e5:f6"
#define HTTP_PORT "80"

enum { MQTT_PORT = 1883 };
enum { CB_CONNECT = 50 };
enum { RETRY_ATTEMPTS = 3 };

class EspConfig {
public:
  explicit EspConfig();

  char host_name[20] = {"\0"};
  char mqtt_server[40] = {"\0"};
  char mqtt_topic[40] = {"\0"};
  char syslog_server[40] = {"\0"};
  char aes_key[100] = {"\0"};
  char ble_mac[18] = {"\0"};
  char http_user[20] = {"\0"};
  char http_pass[20] = {"\0"};

  bool initEspConfig();
  bool resetConfig();
  bool saveConfig();
};