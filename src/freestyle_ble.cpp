#include "CRC.h"
#include "NimBLEDevice.h"
#include "esp_random.h"
#include "mbedtls/gcm.h"
#include <Arduino.h>
#include <iomanip>
#include <iostream>

#include "freestyle_ble.h"
#include "proto/cmd.pb.h"
#include <encoder.h>
#include <pb_common.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <string>

static NimBLEUUID serviceUUID(SERVICE_UUID);
static NimBLEUUID charUUID301(CMD_CHANNEL_UUID);

String FreestyleClient::getLockState(uint8_t lockState) {
  String state;
  switch (lockState) {
  case 0:
    state = "STATE_UNKNOWN";
    break;
  case 1:
    state = "UNLOCKED";
    break;
  case 2:
    state = "LOCKED_PRIVACY";
    break;
  case 3:
    state = "LOCKED_DEADLOCK";
    break;
  case 4:
    state = "ERROR_FORCED";
    break;
  default:
    state = "ERROR_JAMMED";
    break;
  }
  return state;
}

void printHex(unsigned char *data, uint8_t len) {
  Log.print("Writing data: ");
  for (int i = 0; i < len; i++) {
    char str[3];
    sprintf(str, "%02x", data[i]);
    Log.print(str);
  }
  Log.println();
}

void parse0x01(unsigned char *data, int len) {
  Log.println("###### DoAi status query response ######");
  Log.println("Transfer Status field");
  Log.printf(" -> doAiFlags: 0x%02X\n", data[1]);
  Log.printf(" -> doAiMsgIdTrf: 0x%02X%02X\n", data[2], data[3]);
  Log.printf(" -> doAiMsgLen: 0x%02X%02X\n", data[4], data[5]);
  Log.printf(" -> doAiMsgCrc: 0x%02X%02X%02X%02X\n", data[6], data[7], data[8], data[9]);
  Log.printf(" -> doAiMsgCount: %d", data[10]);
  Log.printf(" -> doAiMsgId: 0x%02X%02X\n", data[11], data[12]);
  Log.println();
}

void parse0x02(unsigned char *data, int len) {
  Log.print(" -> DiAoMsgId: ");
  Log.println(data[18], 10);
  Log.print(" -> DiAoNonce (hex): ");
  for (int i = 6; i < len - 2; i++) {
    char str[3];
    sprintf(str, "%02x", data[i]);
    Log.print(str);
  }
  Log.println();
  Log.print("DiAoFlags: 0x");
  Log.println(data[1], HEX);
  if (((data[1] >> 0) & 1) == 1) {
    Log.println(" - DIAO Message transfer active");
  }
  if (((data[1] >> 1) & 1) == 1) {
    Log.println(" - Lock ready to receive first fragment");
  }
  if (((data[1] >> 2) & 1) == 1) {
    Log.println(" - DIAO Message Valid (Accepted)");
  }
  if (((data[1] >> 4) & 1) == 1) {
    Log.println(" - Error: Message ID Invalid");
  }
  if (((data[1] >> 5) & 1) == 1) {
    Log.println(" - Error: Message Offset Invalid");
  }
  if (((data[1] >> 6) & 1) == 1) {
    Log.println(" - Error: Message Length Invalid");
  }
  if (((data[1] >> 7) & 1) == 1) {
    Log.println(" - Error: Message CRC Invalid");
  }
  Log.printf(" -> DiAoMsgIdTrf (hex): 0x%02X%02X\n", data[2], data[3]);
  Log.printf(" -> DiAoMsgIdTrf: %d\n", ((uint16_t)(data[3] << 8) | (uint16_t)data[2]));
  Log.printf(" -> DiAoMsgOffset (hex): 0x%02X%02X\n", data[4], data[5]);
  Log.printf(" -> DiAoMsgOffset: %d\n", ((uint16_t)(data[5] << 8) | (uint16_t)data[4]));
  Log.printf(" -> DiAoMsgId (hex): 0x%02X%02X\n", data[18], data[19]);
  Log.printf(" -> DiAoMsgId: %d\n", ((uint16_t)(data[19] << 8) | (uint16_t)data[18]));
  Log.println();
}

void FreestyleClient::scan() { NimBLEDevice::getScan()->start(3, true); }

auto FreestyleClient::connect() -> bool {

  this->pRemoteCharacteristic301 = nullptr;

  Log.println("Connecting");
  Log.println(" - Created client");
  this->pClient->connect();
  Log.println(" - Connected to server");
  Log.print(" - RSSI:");
  Log.println(this->pClient->getRssi());

  NimBLERemoteService *pRemoteService = this->pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Log.print("Failed to find our service UUID: ");
    Log.println(serviceUUID.toString().c_str());
    this->disconnect();
    return false;
  }
  Log.println(" - Found our service");

  this->pRemoteCharacteristic301 = pRemoteService->getCharacteristic(charUUID301);
  if (this->pRemoteCharacteristic301 == nullptr) {
    Log.print("Failed to find characteristic: ");
    Log.println(charUUID301.toString().c_str());
    this->disconnect();
    return false;
  }
  Log.println(" - Found characteristic 301");
  this->pRemoteCharacteristic301->subscribe(false, FreestyleClient::notifyCallback);

  Log.println(" - Connected to lock");
  connected = true;
  notify_time = millis();
  return true;
}

void FreestyleClient::init(char *a_key, std::string a_mac) {
  key = a_key;
  mac = a_mac;

  NimBLEDevice::init("Freestyle");
  NimBLEDevice::setMTU(255);
  esp_power_level_t min, max;
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
  esp_bredr_tx_power_set(ESP_PWR_LVL_P9, ESP_PWR_LVL_P9);
  delay(5);
  esp_bredr_tx_power_get(&min, &max);
  Log.printf("BLE power level: min %d max %d\n", min, max);
  this->pClient = NimBLEDevice::createClient(NimBLEAddress(this->mac, 1));
}

void FreestyleClient::setLockState(uint8_t state, bool skipConnect) {
  if (!skipConnect || !connected) {
    return setLockState(state);
  }
  desiredLockState = state;
  Log.print("Setting lock state to ");
  Log.println(getLockState(desiredLockState));
  Log.println("Writing 02 to command channel");
  this->pRemoteCharacteristic301->writeValue(0x02, true);
}

void FreestyleClient::setLockState(uint8_t state) {

  if (millis() - notify_time < TIMEOUT) {
    Log.println("Error: setLockState already called");
    return;
  }

  desiredLockState = state;
  Log.print("Setting lock state to ");
  Log.println(getLockState(desiredLockState));
  if (state < 1 && state > 3) {
    Serial.println("setLockState failed, invalid state");
    this->callback(desiredLockState, -1);
    return;
  }
  bool result = begin();
  if (!result) {
    this->callback(desiredLockState, 0);
  }
}

auto FreestyleClient::begin() -> bool {
  if (!this->connect()) {
    Log.println("Failed to connect to the lock.");
    this->disconnect();
    return false;
  }
  Log.println("Writing 02 to command channel");
  this->pRemoteCharacteristic301->writeValue(0x02, true);
  return true;
}

void FreestyleClient::notifyCallback(
  NimBLERemoteCharacteristic *Characteristic,
  uint8_t *pData,
  size_t length,
  bool isNotify)
{
  notify_time = millis();
  notify_pData = pData;
  notify_length = length;
}

void FreestyleClient::handler() {
  if (connected) {
    if (millis() - notify_time > TIMEOUT) {
      Log.print("Timed out waiting for response\n");
      this->disconnect();
      callback(desiredLockState, 0);
    }
  }
  if (this->notify_pData == nullptr) {
    return;
  }
  Log.printf("[Command channel characteristic updated, commandType: 0x%02x]\n", this->notify_pData[0]);
  if (this->notify_pData[0] == 0x02) {
    parse0x02(notify_pData, notify_length);
  }
  if (this->notify_pData[0] == 0x01) {
    parse0x01(notify_pData, notify_length);
  }
  // step 1 - lock responds with status
  if (this->notify_pData[0] == 0x02 && this->notify_pData[1] == 0x00) {
    this->generatePayload();
  }
  // step 2 - lock responds with ready to receive
  if (this->notify_pData[0] == 0x02 && this->notify_pData[1] == 0x03) {
    this->sendEncodedMessage();
  }
  // step 3 - Message was accepted
  if (this->notify_pData[0] == 0x02 && this->notify_pData[1] == 0x04) {
    Log.print("Writing 01 to command channel\n");
    this->pRemoteCharacteristic301->writeValue(0x01, true);
  }
  // step 4 - Get the status response
  if (this->notify_pData[0] == 0x01 && this->notify_pData[1] == 0x00) {
    this->requestStatusMessage();
  }
  // step 5 - Get the end of status response message checksum
  if (this->notify_pData[0] == 0x01 && this->notify_pData[1] == 0x02) {
    Log.println("DoAi end of status response received, requesting message delete");
    this->deleteMessage();
  }
  // step 6 - receive status message
  if (this->notify_pData[0] == 0x30) {
    Log.println("Received an encoded message");
    this->decodeIncomingMessage();
  }
  // step 7 - get end of status response message
  if (this->notify_pData[0] == 0x01 && this->notify_pData[1] == 0x06) {
    Log.println("No more messages, disconnecting");
    this->disconnect();
  }

  this->notify_pData = nullptr;
  FreestyleClient::notify_length = 0;
}

void FreestyleClient::generatePayload() {
  unsigned short msgId = this->notify_pData[18];
  char nonce[12];
  memcpy(nonce, this->notify_pData + 6, 12);

  // generate random token
  unsigned char rand[4];
  esp_fill_random(rand, 4);
  unsigned long token = *((unsigned long *)rand);

  // build the protobuf message
  uint8_t buffer[128];
  size_t message_length = 0;
  bool status = false;
  cmd_Request message = cmd_Request_init_zero;
  message.has_lockStateUpdate = true;
  message.lockStateUpdate.has_request = true;
  message.lockStateUpdate.request.has_desiredState = true;
  message.lockStateUpdate.request.has_desiredStateToken = true;
  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
  message.lockStateUpdate.request.desiredState = (cmd_State)desiredLockState; // fixme
  message.lockStateUpdate.request.desiredStateToken = token;
  status = pb_encode(&stream, cmd_Request_fields, &message);
  message_length = stream.bytes_written;

  if (!status) {
    Log.printf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
    return;
  }

  Log.println("Request: ");
  Log.print("    desiredState: ");
  String lockState = getLockState(message.lockStateUpdate.request.desiredState);
  Log.print(lockState);
  Log.println();
  Log.print("    desiredStateToken: ");
  Log.println(message.lockStateUpdate.request.desiredStateToken);
  Log.println();

  // encrypt the message
  uint8_t msgLen = encodeMessage(this->key, buffer, message_length,
                                 encodedMessageData, nonce, msgId + 1);
  Log.print("Encoded Message as: ");
  for (int i = 0; i < msgLen; i++) {
    char str[3];
    sprintf(str, "%02x", encodedMessageData[i]);
    Log.print(str);
  }
  Log.println();
  Log.printf("Encoded Message length: %d (0x%02X%02X)\n", msgLen, msgLen, 0x00);

  // send request to accept encoded message
  unsigned char thisMsgId = this->notify_pData[18] + 1;
  unsigned char writeBack[] = {
      0x20,                   // 0
      thisMsgId,              // 1
      this->notify_pData[19], // 2
      msgLen,                 // 3
      0x00                    // 4
  };

  encodedMessageDataLen = msgLen;
  encodedMessageId[0] = this->notify_pData[18] + 1;
  encodedMessageId[1] = this->notify_pData[19];

  Log.print("Sending request to accept new message\n");
  printHex(writeBack, sizeof(writeBack));
  this->pRemoteCharacteristic301->writeValue(writeBack, sizeof(writeBack), true);
}

void FreestyleClient::sendEncodedMessage() {
  unsigned char header[] = {
      0x30,                // 0
      encodedMessageId[0], // 1
      encodedMessageId[1], // 2
      0x00,                // 3
      0x00,                // 4
  };

  unsigned char sendMsg[encodedMessageDataLen + sizeof(header)] = {0};
  memcpy(sendMsg, header, sizeof(header));
  memcpy(sendMsg + sizeof(header), encodedMessageData, encodedMessageDataLen);

  Log.println("Sending message payload");
  printHex(sendMsg, sizeof(sendMsg));

  this->pRemoteCharacteristic301->writeValue(sendMsg, sizeof(sendMsg), 1);

  // send the end of transmission with crc
  uint32_t crcI = calcCRC32(encodedMessageData + 2, encodedMessageDataLen - 2,
                            0x04C11DB7, 0x00000000, 0xFFFFFFFF, true, true);
  unsigned char endMsg[] = {
      0x21,                  // 0
      encodedMessageId[0],   // 1
      encodedMessageId[1],   // 2
      encodedMessageDataLen, // 3
      0x00,                  // 4
      crcI,                  // 5
      crcI >> 8,             // 6
      crcI >> 16,            // 7
      crcI >> 24,            // 8
  };

  Log.println("Sending CRC");
  printHex(endMsg, sizeof(endMsg));

  this->pRemoteCharacteristic301->writeValue(endMsg, sizeof(endMsg), 1);
}

void FreestyleClient::requestStatusMessage() {
  Log.println("DoAi status response received");
  unsigned short msgId = this->notify_pData[11];
  unsigned char msgCount = this->notify_pData[10];

  if (msgCount == 0) {
    Log.println("No DoAi messages, cancelling DevicePA.\n\n\n");
    this->disconnect();
    return;
  }

  Log.print("Generating Sender nonce: ");
  esp_fill_random(senderNonce, 12);
  for (unsigned char i : senderNonce) {
    char str[3];
    sprintf(str, "%02x", i);
    Log.print(str);
  }
  Log.println();

  unsigned char senderRequestHeader[5] = {
      0x10,                   // 0
      this->notify_pData[11], // 1 msgId
      this->notify_pData[12], // 2 msgId
      0x00,                   // 3
      0x00                    // 4
  };

  unsigned char senderRequestData[17] = {0};
  memcpy(senderRequestData, senderRequestHeader, sizeof(senderRequestHeader));
  memcpy(senderRequestData + sizeof(senderRequestHeader), senderNonce, sizeof(senderNonce));

  Log.println("Sending request to return messages");
  printHex(senderRequestData, sizeof(senderRequestData));

  pRemoteCharacteristic301->writeValue(senderRequestData, sizeof(senderRequestData), 1);
}

void FreestyleClient::deleteMessage() {
  unsigned char senderRequestAckData[] = {
      0x12,
      this->notify_pData[2],
      this->notify_pData[3],
  };
  printHex(senderRequestAckData, sizeof(senderRequestAckData));
  pRemoteCharacteristic301->writeValue(senderRequestAckData, sizeof(senderRequestAckData), 1);
}

void FreestyleClient::decodeIncomingMessage() {
  unsigned char rxNonce[12] = {};
  memcpy(rxNonce, this->notify_pData + 11, 12);
  unsigned short rxLen = ((uint16_t)(this->notify_pData[10] << 8) | (uint16_t)this->notify_pData[9]);
  unsigned short rxMsgId = ((uint16_t)(this->notify_pData[2] << 8) | (uint16_t)this->notify_pData[1]);
  Log.printf("MsgId: %d\n", rxMsgId);
  Log.printf("MsgLength: %d\n", rxLen);
  uint8_t encLength = FreestyleClient::notify_length - 23;
  unsigned char rxData[encLength] = {0};
  decodeMessage(this->key, this->notify_pData + 23, encLength, rxData, senderNonce, rxNonce, rxMsgId);

  Log.print("Decrypted response: ");
  for (int i = 0; i < rxLen; i++) {
    char str[3];
    sprintf(str, "%02x", rxData[i]);
    Log.print(str);
  }
  Log.println();

  uint8_t buffer[128];
  size_t message_length = 0;
  bool status = false;
  cmd_Confirm message = cmd_Confirm_init_zero;
  message.has_lockStateConfirm = true;
  message.lockStateConfirm.has_confirm = true;
  message.lockStateConfirm.confirm.has_desiredState = true;
  message.lockStateConfirm.confirm.has_desiredStateToken = true;
  message.lockStateConfirm.confirm.has_reportedState = true;
  pb_istream_t stream = pb_istream_from_buffer(rxData, rxLen);
  status = pb_decode(&stream, cmd_Confirm_fields, &message);

  if (!status) {
    printf("Nanopb decode failed: %s\n", PB_GET_ERROR(&stream));
    return;
  }

  Log.println("\nResult: ");
  Log.print("    desiredState: ");
  String desiredState = getLockState(message.lockStateConfirm.confirm.desiredState);
  Log.println(desiredState);
  Log.print("    desiredStateToken: ");
  Log.println(message.lockStateConfirm.confirm.desiredStateToken, DEC);
  Log.print("    reportedState: ");
  String reportedState = getLockState(message.lockStateConfirm.confirm.reportedState);
  Log.println(reportedState);
  Log.println();
  this->callback(desiredLockState, message.lockStateConfirm.confirm.reportedState);
}

void FreestyleClient::disconnect() {
  connected = false;
  this->pClient->disconnect();
}