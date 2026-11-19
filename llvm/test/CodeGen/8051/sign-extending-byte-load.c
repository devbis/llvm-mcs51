// A signed byte loaded from memory and widened to 16 bits must be sign
// extended. The DAG combiner folds (sext (load i8)) into an i16 SEXTLOAD;
// byte loads are selected by memory type and reach the 16-bit pair through a
// zero-extending byte-to-word copy, so a SEXTLOAD must be lowered to a byte
// load followed by an explicit sign extension (high byte = 0 - ACC.7).
// Before the fix every address space produced high byte 0, e.g. -127 / 7
// evaluated as 129 / 7.
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -mllvm -verify-machineinstrs \
// RUN:   -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -mllvm -verify-machineinstrs \
// RUN:   -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -Os -mllvm -verify-machineinstrs \
// RUN:   -S %s -o - | FileCheck %s

typedef signed char s8;
typedef int s16;

volatile __xdata s8 xa;
volatile __idata s8 ia;
volatile __pdata s8 pa;
volatile const __code s8 ca[2] = {-128, 5};
volatile s8 ga;
volatile __xdata s16 out;
volatile __xdata unsigned char k;

__attribute__((noinline)) void from_xdata(void) { out = xa; }
__attribute__((noinline)) void from_idata(void) { out = ia; }
__attribute__((noinline)) void from_pdata(void) { out = pa; }
__attribute__((noinline)) void from_code(void) { out = ca[k]; }
__attribute__((noinline)) void from_default(void) { out = ga; }
__attribute__((noinline)) void divide(void) { out = xa / (s8)k; }
// Non-volatile loads are folded into a SEXTLOAD before legalization.
__attribute__((noinline)) s16 divide_ptr(const __xdata s8 *p) {
  return p[0] / p[1];
}

// CHECK-LABEL: from_xdata:
// CHECK: movx a, @dptr
// CHECK: mov c, 231
// CHECK-NEXT: clr a
// CHECK-NEXT: subb a, #0
// CHECK-LABEL: from_idata:
// CHECK: mov a, @r0
// CHECK: mov c, 231
// CHECK-NEXT: clr a
// CHECK-NEXT: subb a, #0
// CHECK-LABEL: from_pdata:
// CHECK: movx a, @r0
// CHECK: mov c, 231
// CHECK-NEXT: clr a
// CHECK-NEXT: subb a, #0
// CHECK-LABEL: from_code:
// CHECK: movc a, @a+dptr
// CHECK: mov c, 231
// CHECK-NEXT: clr a
// CHECK-NEXT: subb a, #0
// CHECK-LABEL: from_default:
// CHECK: mov c, 231
// CHECK-NEXT: clr a
// CHECK-NEXT: subb a, #0
// CHECK-LABEL: divide:
// CHECK: mov c, 231
// CHECK-NEXT: clr a
// CHECK-NEXT: subb a, #0
// CHECK: mov c, 231
// CHECK-NEXT: clr a
// CHECK-NEXT: subb a, #0
// CHECK: lcall {{.*}}__divhi3
// CHECK-LABEL: divide_ptr:
// CHECK: movx a, @dptr
// CHECK: mov c, 231
// CHECK-NEXT: clr a
// CHECK-NEXT: subb a, #0
// CHECK: movx a, @dptr
// CHECK: mov c, 231
// CHECK-NEXT: clr a
// CHECK-NEXT: subb a, #0
// CHECK: lcall {{.*}}__divhi3
