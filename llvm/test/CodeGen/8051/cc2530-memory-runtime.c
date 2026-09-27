// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -fno-builtin %s -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s

typedef unsigned short size_t;

extern __xdata void *memcpy(__xdata void *destination,
                            __xdata const void *source, size_t count);
extern __xdata void *memmove(__xdata void *destination,
                             __xdata const void *source, size_t count);
extern __xdata void *memset(__xdata void *destination, int value,
                            size_t count);
extern int memcmp(__xdata const void *lhs, __xdata const void *rhs,
                  size_t count);
extern size_t strlen(__xdata const char *string);

int main(void) {
  __xdata unsigned char *source = (__xdata unsigned char *)0x2000;
  __xdata unsigned char *destination = (__xdata unsigned char *)0x2010;
  memcpy(destination, source, 8);
  memmove(destination + 1, destination, 7);
  memset(destination, 0, 1);
  return memcmp(destination, source, 8) + strlen((__xdata const char *)0x2020);
}

// CHECK-DAG: Name: memcpy
// CHECK-DAG: Name: memmove
// CHECK-DAG: Name: memset
// CHECK-DAG: Name: memcmp
// CHECK-DAG: Name: strlen
