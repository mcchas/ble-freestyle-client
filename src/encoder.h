#include <Arduino.h>
#include <cstdint>
#include <TLog.h>

int encodeMessage(char *key, unsigned char *input, uint8_t inputLen,
                  unsigned char *outArr, char *iv1, unsigned short msgId);

void decodeMessage(char *key, unsigned char *input, uint8_t inputLen,
                   unsigned char *output, unsigned char *iv1,
                   unsigned char *iv2, unsigned short msgId);
