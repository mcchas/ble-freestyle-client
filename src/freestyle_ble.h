#include "NimBLEDevice.h"
#include <Arduino.h>
#include <TLog.h>
#include <functional>
#include <string>

#define SERVICE_UUID "00000001-4757-4100-6c78-67726f757000"
#define CMD_CHANNEL_UUID "00000301-4757-4100-6c78-67726f757000"
enum { TIMEOUT = 3000 };

class FreestyleClient {
public:
  FreestyleClient(
      std::function<void(uint8_t /* desiredState */, int8_t /* reportedState*/)>
          a_callback) {
    callback = a_callback;
  }

  uint8_t desiredLockState = 0;
  uint8_t reportedLockState = 0;

  bool connect();
  void init(char *a_key, std::string a_mac);
  bool begin();
  void handler();
  void disconnect();
  void scan();
  void setLockState(uint8_t state);
  void setLockState(uint8_t state, bool skipConnect);
  String getLockState(uint8_t state);

private:
  uint8_t state = 0;
  bool connected = false;

  std::string mac;
  char *key;
  std::function<void(uint8_t, int8_t)> callback;

  NimBLEClient *pClient;
  NimBLERemoteCharacteristic *pRemoteCharacteristic301 = nullptr;
  inline static uint8_t *notify_pData;
  inline static size_t notify_length;
  inline static unsigned long notify_time = 0;
  unsigned char encodedMessageData[128] = {0};
  uint8_t encodedMessageDataLen = 0;
  unsigned char encodedMessageId[2] = {0, 0};
  unsigned char senderNonce[12] = {0};

  static void notifyCallback(NimBLERemoteCharacteristic *Characteristic,
                             uint8_t *pData, size_t length, bool isNotify);
  void generatePayload();
  void sendEncodedMessage();
  void requestStatusMessage();
  void deleteMessage();
  void decodeIncomingMessage();
};