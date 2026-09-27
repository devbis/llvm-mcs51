// RUN: cc -O2 -fno-builtin %s -o %t
// RUN: %t

#include <stdint.h>

#include "../../../lib/Target/MCS51/mcs51-runtime.c"

int main(void) {
  uint8_t Source[] = {0x11, 0x22, 0x33, 0x44, 0x55};
  uint8_t Buffer[8] = {0};
  uint8_t Expected[] = {0x11, 0x11, 0x22, 0x33, 0x44, 0x55, 0, 0};

  if (memcpy(Buffer, Source, sizeof(Source)) != Buffer ||
      memcmp(Buffer, Source, sizeof(Source)))
    return 1;
  if (memset(Buffer, 0xaa, 2) != Buffer || Buffer[0] != 0xaa ||
      Buffer[1] != 0xaa)
    return 2;
  memcpy(Buffer, Source, sizeof(Source));
  if (memmove(Buffer + 1, Buffer, sizeof(Source)) != Buffer + 1 ||
      memcmp(Buffer, Expected, sizeof(Buffer)))
    return 3;
  if (memmove(Buffer, Buffer + 1, sizeof(Source)) != Buffer ||
      memcmp(Buffer, Source, sizeof(Source)))
    return 4;

  char Text[] = {'8', '0', '5', '1', 0};
  if (strlen(Text) != 4)
    return 5;

  char StringBuffer[16] = "llvm";
  char StringCopy[8];
  if (strcpy(StringCopy, Text) != StringCopy || strcmp(StringCopy, Text))
    return 6;
  if (strcmp("8051", "8050") <= 0 || strcmp("8051", "8052") >= 0 ||
      strcmp("llvm", "llvm") != 0)
    return 7;
  if (strncmp("8051", "8052", 3) != 0 || strncmp("8051", "8052", 4) >= 0 ||
      strncmp("8051", "8051x", 8) >= 0 || strncmp("8051", "8051x", 4) != 0 ||
      strncmp("8051", "8051", 0) != 0)
    return 8;
  if (strncpy(StringCopy, "mcu", 5) != StringCopy || StringCopy[0] != 'm' ||
      StringCopy[1] != 'c' || StringCopy[2] != 'u' || StringCopy[3] != '\0' ||
      StringCopy[4] != '\0')
    return 9;
  if (strncpy(StringCopy, "8051", 2) != StringCopy || StringCopy[0] != '8' ||
      StringCopy[1] != '0' || StringCopy[2] != 'u')
    return 10;
  if (strcat(StringBuffer, "-8051") != StringBuffer ||
      strcmp(StringBuffer, "llvm-8051"))
    return 11;
  if (strncat(StringBuffer, "-mcu", 3) != StringBuffer ||
      strcmp(StringBuffer, "llvm-8051-mc"))
    return 12;
  return 0;
}
