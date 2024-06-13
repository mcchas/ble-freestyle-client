#include "AsyncElegantOTA.h"
#include "config.h"
#include "freestyle_ble.h"
#include "proto/cmd.pb.h"
#include "web.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ETH.h>
#include <PubSubClient.h>
#include <WebSerialStream.h>
#include <WiFiClient.h>
#include <base64.h>

static char key[32] = {0};
static bool eth_state = false;
static uint8_t setState = cmd_State_STATE_UNKNOWN;
static uint8_t webLockAction = 0;
static int8_t lockStatus = -1;
static uint8_t desiredState = cmd_State_STATE_UNKNOWN;

void lockCallback(uint8_t _desiredState, int8_t _reportedState) {
  desiredState = _desiredState;
  lockStatus = _reportedState;
};

void webActionCallback(uint8_t data) {
  if (data > 0) {
    webLockAction = data;
  }
};

FreestyleClient lock(lockCallback);
EspConfig cfg;
WiFiClient ethernetClient;
PubSubClient mqttClient;
AsyncWebServer http(80);
Web webFrontEnd;

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
  case ARDUINO_EVENT_ETH_START:
    eth_state = false;
    Log.println("ETH Started");
    // set eth hostname here
    //  ETH.setHostname("esp32-ethernet");
    break;
  case ARDUINO_EVENT_ETH_CONNECTED:
    eth_state = false;
    Log.println("ETH Connected");
    break;
  case ARDUINO_EVENT_ETH_GOT_IP:
    eth_state = true;
    Log.print("ETH addr: ");
    Log.print(ETH.macAddress());
    Log.print(", IPv4: ");
    Log.print(ETH.localIP());
    if (ETH.fullDuplex()) {
      Log.print(", FULL_DUPLEX");
    }
    Log.print(", ");
    Log.print(ETH.linkSpeed());
    Log.println("Mbps");
    break;
  case ARDUINO_EVENT_ETH_DISCONNECTED:
    eth_state = false;
    Log.println("ETH Disconnected");
    break;
  case ARDUINO_EVENT_ETH_STOP:
    eth_state = false;
    Log.println("ETH Stopped");
    break;
  default:
    break;
  }
}

void mqttReconnect() {
  while (!mqttClient.connected()) {
    Log.println("Attempting MQTT connection...");
    String clientId = "esp-fsclient";
    if (mqttClient.connect(clientId.c_str())) {
      Log.println("MQTT connected");
      mqttClient.publish(cfg.mqtt_topic, "ONLINE");
      char str[32];
      sprintf(str, "%s/unlock", cfg.mqtt_topic);
      mqttClient.subscribe(str);
      sprintf(str, "%s/lock", cfg.mqtt_topic);
      mqttClient.subscribe(str);
      sprintf(str, "%s/deadlock", cfg.mqtt_topic);
      mqttClient.subscribe(str);
    } else {
      Log.print("failed, rc=");
      Log.print(mqttClient.state());
      Log.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

void mqttHandler(char *topic, byte *payload, unsigned int length) {
  Log.print("Message arrived [");
  Log.print(topic);
  Log.print("] ");
  for (int i = 0; i < length; i++) {
    Log.print((char)payload[i]);
  }
  Log.println();

  uint8_t topicLen = strlen(cfg.mqtt_topic);
  if (strncmp(topic, cfg.mqtt_topic, topicLen) != 0) {
    return;
  }

  const char *matchTopic1 = "/unlock";
  uint8_t match = strcmp(topic + topicLen, matchTopic1);
  if (match == 0) {
    setState = cmd_State_UNLOCKED;
  }
  const char *matchTopic2 = "/lock";
  match = strcmp(topic + topicLen, matchTopic2);
  if (match == 0) {
    setState = cmd_State_LOCKED_PRIVACY;
  }
  const char *matchTopic3 = "/deadlock";
  match = strcmp(topic + topicLen, matchTopic3);
  if (match == 0) {
    setState = cmd_State_LOCKED_DEADLOCK;
  }
  const char *matchTopic4 = "/connect";
  match = strcmp(topic + topicLen, matchTopic4);
  if (match == 0) {
    setState = 50;
  }
}

void setup() {
  WiFi.onEvent(WiFiEvent);
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);
  delay(2000);

  Serial.begin(115200);

  WebSerialStream Logger = WebSerialStream();
  Log.addPrintStream(std::make_shared<WebSerialStream>(Logger));

  Log.println("Starting " PROJECT_NAME " v" PROJECT_VERSION);

  cfg.initEspConfig();

  char buf[100];
  Base64.decode(buf, cfg.aes_key, 100);
  memcpy(key, buf, 32);

  delay(1000);

  webFrontEnd.actionCallback(webActionCallback);
  webFrontEnd.begin(&http, &cfg);
  AsyncElegantOTA.begin(&http, cfg.http_user, cfg.http_pass);
  Log.begin(&http);
  http.begin();

  delay(1000);

  Log.println("ETH addr: " + ETH.macAddress());

  mqttClient.setClient(ethernetClient);
  mqttClient.setServer(cfg.mqtt_server, MQTT_PORT);
  mqttClient.setBufferSize(512);
  mqttClient.setCallback(mqttHandler);

  lock.init(key, cfg.ble_mac);
}

void loop() {
  static uint8_t retries = 0;

  lock.handler();

  // handle mqtt
  if (setState > 0) {
    retries = 0;
    if (setState == CB_CONNECT) {
      lock.connect();
    } else {
      lock.setLockState(setState);
    }
    setState = 0;
  }

  // handle web
  if (webLockAction > 0) {
    retries = 0;
    if (webLockAction == CB_CONNECT) {
      lock.connect();
    } else {
      lock.setLockState(webLockAction);
    }
    webLockAction = 0;
  }

  if (!mqttClient.connected()) {
    mqttReconnect();
  } else {
    if (lockStatus >= 0) {
      // failed to connect to lock
      if (lockStatus == 0) {
        char str[32];
        sprintf(str, "%s/retrying", cfg.mqtt_topic);
        char attempt[10];
        sprintf(attempt, "attempt=%d", retries);
        mqttClient.publish(str, attempt);
        // try again
        if (retries < RETRY_ATTEMPTS) {
          lock.setLockState(desiredState);
          retries++;
        }
        // give up
        if (retries >= RETRY_ATTEMPTS) {
          mqttClient.publish(str, "failed");
        }
      }
      if (lockStatus > 0) {
        String stateString = lock.getLockState(lockStatus);
        char str[32];
        sprintf(str, "%s/success", cfg.mqtt_topic);
        mqttClient.publish(str, stateString.c_str());
      }
      lockStatus = -1;
    }
  }

  mqttClient.loop();
}