#include "encoder.h"
#include "mbedtls/gcm.h"
#include <Arduino.h>

auto encodeMessage(char *key, unsigned char *input, uint8_t inputLen,
                   unsigned char *outArr, char *iv1, unsigned short msgId) -> int {

  uint8_t keySize = 32;

  mbedtls_gcm_context aes;
  mbedtls_gcm_init(&aes);
  unsigned short ivLen = 24;

  unsigned char output[128] = {0};

  Log.printf("Message length: %d\n", inputLen);

  Log.print("AES Key: ");
  for (int i = 0; i < keySize; i++) {
    if (i > 6 && i< 26)
      Log.print("XX");
    else {
      char str[3];
      sprintf(str, "%02x", key[i]);
      Log.print(str);
    }
  }
  Log.println();

  Log.print("Invoked nonce: ");
  for (int i = 0; i < 12; i++) {
    char str[3];
    sprintf(str, "%02x", iv1[i]);
    Log.print(str);
  }
  Log.println();

  Log.print("Generating nonce: ");
  char iv2[12];
  esp_fill_random(iv2, 12);
  for (char &i : iv2) {
    char str[3];
    sprintf(str, "%02x", i);
    Log.print(str);
  }
  Log.println();
  char *finalIV = new char[24];

  memcpy(finalIV, iv1, 12);
  memcpy(finalIV + 12, iv2, 12);
  Log.print("Final nonce: ");
  for (int i = 0; i < 24; i++) {
    char str[3];
    sprintf(str, "%02x", finalIV[i]);
    Log.print(str);
  }
  Log.println();

  unsigned short padLen = 16 - (inputLen & 15);

  char randomData[padLen] = {0};
  esp_fill_random(randomData, padLen);
  Log.printf("Padding length: %d\n", padLen);
  Log.print("Padding random data: ");
  for (int i = 0; i < padLen; i++) {
    char str[3];
    sprintf(str, "%02x", randomData[i]);
    Log.print(str);
  }
  Log.println();

  unsigned char paddedInput[inputLen + padLen] = {0};
  memcpy(paddedInput, input, inputLen);
  memcpy(paddedInput + inputLen, randomData, padLen);

  Log.print("Padded message: ");
  for (int i = 0; i < sizeof(paddedInput); i++) {
    char str[3];
    sprintf(str, "%02x", paddedInput[i]);
    Log.print(str);
  }
  Log.println();

  unsigned char header[6] = {
    0x00,       // 0
    0x00,       // 1
    (msgId>>8), // 2
    msgId,      // 3
    inputLen,   // 4
    0x00        // 5
  };

  mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, (const unsigned char *)key,
                     keySize * 8);
  mbedtls_gcm_starts(&aes, MBEDTLS_GCM_ENCRYPT, (const unsigned char *)finalIV,
                     ivLen, header, sizeof(header));
  mbedtls_gcm_update(&aes, sizeof(paddedInput), paddedInput, output);

  unsigned char tag[16] = {0};
  mbedtls_gcm_finish(&aes, tag, sizeof(tag));
  mbedtls_gcm_free(&aes);

  auto *message = new unsigned char[inputLen + 6 + 12 + padLen + 16];

  memcpy(message, &header, 6);
  memcpy(message + 6, iv2, 12);
  memcpy(message + 6 + 12, output, inputLen + padLen);
  memcpy(message + 6 + 12 + inputLen + padLen, tag, 16);
  memcpy(outArr, message, inputLen + 6 + 12 + padLen + 16);

  return inputLen + 6 + 12 + padLen + 16;
}

void decodeMessage(char *key, unsigned char *input, uint8_t inputLen,
                   unsigned char *outArr, unsigned char *iv1,
                   unsigned char *iv2, unsigned short msgId) {

  uint8_t keySize = 32;

  mbedtls_gcm_context aes;
  mbedtls_gcm_init(&aes);
  unsigned short ivLen = 24;

  Log.print("Encrypted data: ");
  for (int i = 0; i < inputLen; i++) {
    char str[3];
    sprintf(str, "%02x", input[i]);
    Log.print(str);
  }
  Log.println();

  Log.print("First nonce: ");
  for (int i = 0; i < 12; i++) {
    char str[3];
    sprintf(str, "%02x", iv1[i]);
    Log.print(str);
  }
  Log.println();

  Log.print("Second nonce: ");
  for (int i = 0; i < 12; i++) {
    char str[3];
    sprintf(str, "%02x", iv2[i]);
    Log.print(str);
  }
  Log.println();
  char *finalIV = new char[24];

  memcpy(finalIV, iv1, 12);
  memcpy(finalIV + 12, iv2, 12);
  Log.print("Final nonce: ");
  for (int i = 0; i < 24; i++) {
    char str[3];
    sprintf(str, "%02x", finalIV[i]);
    Log.print(str);
  }
  Log.println();

  unsigned int padLen = 16 - (sizeof(input) & 15);

  mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, (const unsigned char *)key,
                     keySize * 8);
  mbedtls_gcm_starts(&aes, MBEDTLS_GCM_DECRYPT, (const unsigned char *)finalIV,
                     ivLen, NULL, 0);
  mbedtls_gcm_update(&aes, inputLen, (const unsigned char *)input, outArr);
  mbedtls_gcm_free(&aes);
}
