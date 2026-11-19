// RUN: not clang -target mcs51 -O0 -DBAD_CALL -c %s -o %t.o 2>&1 | FileCheck %s
// RUN: not clang -target mcs51 -O2 -DBAD_CALL -c %s -o %t.o 2>&1 | FileCheck %s
// RUN: not clang -target mcs51 -O1 -DBAD_SELECT -c %s -o %t.o 2>&1 | FileCheck %s
// RUN: not clang -target mcs51 -O1 -DBAD_XDATA -c %s -o %t.o 2>&1 | FileCheck %s
// RUN: not clang -target mcs51 -O1 -DBAD_STORE -c %s -o %t.o 2>&1 | FileCheck %s
// RUN: not clang -target mcs51 -O1 -DBAD_RETURN -Wno-return-stack-address -c %s -o %t.o 2>&1 | FileCheck %s
// RUN: clang -target mcs51 -O1 -flto -DBAD_CALL -c %s -o %t.lto.o
// RUN: not ld.lld -m elf32-mcs51 -r %t.lto.o -o %t.link.o 2>&1 | FileCheck %s
// RUN: clang -target mcs51 -O0 -DGOOD -c %s -o %t.o
// RUN: clang -target mcs51 -O2 -DGOOD -c %s -o %t.o
// RUN: clang -target mcs51 -mcpu=cc2530 -O1 -DBAD_CALL -c %s -o %t.o
// CHECK: IDATA stack pointer escapes through an XDATA or incompatible address space
// CHECK-SAME: use an __idata or __generic pointer on classic MCS-51

typedef unsigned char u8;
extern void consume(u8 *);
extern void consume_idata(__idata u8 *);
extern void consume_xdata(__xdata u8 *);
extern void consume_generic(__generic u8 *);
u8 *volatile saved;

#if defined(BAD_CALL)
void example(void) {
  u8 local[2] = {1, 2};
  consume(local + 1);
}
#elif defined(BAD_SELECT)
void example(int choose, u8 *other) {
  u8 local = 1;
  consume(choose ? &local : other);
}
#elif defined(BAD_XDATA)
void example(void) {
  u8 local[2] = {1, 2};
  consume_xdata((__xdata u8 *)local);
}
#elif defined(BAD_STORE)
void example(void) {
  u8 local = 1;
  saved = &local;
}
#elif defined(BAD_RETURN)
u8 *example(void) {
  u8 local;
  return &local;
}
#elif defined(GOOD)
void example(void) {
  u8 local[2] = {1, 2};
  consume_idata((__idata u8 *)local);
  consume_generic((__generic u8 *)local);
}
#endif
