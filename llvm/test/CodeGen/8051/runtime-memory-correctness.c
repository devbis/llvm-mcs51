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
  return strlen(Text) != 4;
}
