// RUN: clang -target mcs51 -O1 -S %s -o - | FileCheck %s

unsigned short add_one16(unsigned short value) { return value + 1; }
unsigned short add_two16(unsigned short value) { return value + 2; }
unsigned short subtract_one16(unsigned short value) { return value - 1; }
unsigned short add_negative_one16(unsigned short value) { return value + 65535; }
short subtract_one_signed16(short value) { return value - 1; }
unsigned short add_25616(unsigned short value) { return value + 256; }
unsigned short add_large16(unsigned short value) { return value + 60000; }

// CHECK-LABEL: add_one16:
// CHECK: inc dptr
// CHECK-NEXT: ret
// CHECK-LABEL: add_two16:
// CHECK: inc dptr
// CHECK-NEXT: inc dptr
// CHECK-NEXT: ret
// CHECK-LABEL: subtract_one16:
// CHECK: mov a, 130
// CHECK-NEXT: add a, #255
// CHECK-NEXT: mov 130, a
// CHECK-NEXT: mov a, 131
// CHECK-NEXT: addc a, #255
// CHECK-NEXT: mov 131, a
// CHECK-NEXT: ret
// CHECK-LABEL: add_negative_one16:
// CHECK: mov a, 130
// CHECK-NEXT: add a, #255
// CHECK-NEXT: mov 130, a
// CHECK-NEXT: mov a, 131
// CHECK-NEXT: addc a, #255
// CHECK-NEXT: mov 131, a
// CHECK-NEXT: ret
// CHECK-LABEL: subtract_one_signed16:
// CHECK: mov a, 130
// CHECK-NEXT: add a, #255
// CHECK-NEXT: mov 130, a
// CHECK-NEXT: mov a, 131
// CHECK-NEXT: addc a, #255
// CHECK-NEXT: mov 131, a
// CHECK-NEXT: ret
// CHECK-LABEL: add_25616:
// CHECK: mov a, 130
// CHECK-NEXT: add a, #0
// CHECK-NEXT: mov 130, a
// CHECK-NEXT: mov a, 131
// CHECK-NEXT: addc a, #1
// CHECK-NEXT: mov 131, a
// CHECK-NEXT: ret
// CHECK-LABEL: add_large16:
// CHECK: mov a, 130
// CHECK-NEXT: add a, #96
// CHECK-NEXT: mov 130, a
// CHECK-NEXT: mov a, 131
// CHECK-NEXT: addc a, #234
// CHECK-NEXT: mov 131, a
// CHECK-NEXT: ret
