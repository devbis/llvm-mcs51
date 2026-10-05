// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -S %s -o %t.s
// RUN: FileCheck %s < %t.s
// RUN: llvm-mc -triple=mcs51 -mcpu=cc2530 -filetype=obj %t.s -o %t.o

volatile __xdata unsigned char result;

__attribute__((noinline)) static void case0(void) { result = 0x21; }
__attribute__((noinline)) static void case1(void) { result = 0x53; }
__attribute__((noinline)) static void case2(void) { result = 0x17; }
__attribute__((noinline)) static void case3(void) { result = 0x9c; }
__attribute__((noinline)) static void case4(void) { result = 0x62; }
__attribute__((noinline)) static void case5(void) { result = 0xb7; }
__attribute__((noinline)) static void case6(void) { result = 0x34; }
__attribute__((noinline)) static void case7(void) { result = 0x88; }
__attribute__((noinline)) static void case8(void) { result = 0x42; }
__attribute__((noinline)) static void case9(void) { result = 0x71; }
__attribute__((noinline)) static void case10(void) { result = 0x0d; }
__attribute__((noinline)) static void case11(void) { result = 0xe3; }
__attribute__((noinline)) static void case12(void) { result = 0x29; }
__attribute__((noinline)) static void case13(void) { result = 0x64; }
__attribute__((noinline)) static void case14(void) { result = 0x96; }
__attribute__((noinline)) static void case15(void) { result = 0x05; }

__attribute__((noinline)) void dispatch(unsigned char value) {
  switch (value) {
  case 0:
    case0();
    break;
  case 1:
    case1();
    break;
  case 2:
    case2();
    break;
  case 3:
    case3();
    break;
  case 4:
    case4();
    break;
  case 5:
    case5();
    break;
  case 6:
    case6();
    break;
  case 7:
    case7();
    break;
  case 8:
    case8();
    break;
  case 9:
    case9();
    break;
  case 10:
    case10();
    break;
  case 11:
    case11();
    break;
  case 12:
    case12();
    break;
  case 13:
    case13();
    break;
  case 14:
    case14();
    break;
  case 15:
    case15();
    break;
  default:
    result = 0xff;
    break;
  }
}

int main(void) {
  dispatch(3);
  return result;
}

// CHECK: dispatch:
// CHECK: mov dptr, #JTI{{[0-9]+}}_0
// CHECK: mov 240, #2
// CHECK: mul ab
// CHECK: mov a, 131
// CHECK: add a, 240
// CHECK: mov 131, a
// CHECK: movc a, @a+dptr
// CHECK: inc a
// CHECK: movc a, @a+dptr
// CHECK: mov 131, a
// CHECK: mov 130, r6
// CHECK: clr a
// CHECK: jmp @a+dptr
// CHECK: JTI{{[0-9]+}}_0:
// CHECK-COUNT-16: .short .LBB{{[0-9]+}}_{{[0-9]+}}
