// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -fno-builtin %s -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s

typedef unsigned short size_t;

extern void *memcpy(void *destination, const void *source, size_t count);
extern void *memmove(void *destination, const void *source, size_t count);
extern void *memset(void *destination, int value, size_t count);
extern int memcmp(const void *lhs, const void *rhs, size_t count);
extern size_t strlen(const char *string);

unsigned char source[8] = {1, 2, 3, 4, 5, 6, 7, 8};
unsigned char destination[8];
char text[5] = {'8', '0', '5', '1', 0};

int main(void) {
  memcpy(destination, source, 8);
  memmove(destination + 1, destination, 7);
  memset(destination, 0, 1);
  return memcmp(destination, source, 8) + (int)strlen(text);
}

// CHECK-DAG: Name: memcpy
// CHECK-DAG: Name: memmove
// CHECK-DAG: Name: memset
// CHECK-DAG: Name: memcmp
// CHECK-DAG: Name: strlen
