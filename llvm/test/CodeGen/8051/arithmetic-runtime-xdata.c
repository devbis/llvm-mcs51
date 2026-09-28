// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld -Wl,--no-check-sections %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=LINK
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s --check-prefix=LINKSYM

__xdata volatile unsigned char a;
__xdata volatile unsigned char b;
__xdata volatile unsigned char result;

void main(void) {
  a = 17;
  b = 9;
  result = (unsigned char)((a * 3u + b) ^ (a - b));
}

// A right operand returned in A can be exchanged with the left register.
// This avoids saving the load to a temporary, then reloading the left operand.
// CHECK-LABEL: main:
// CHECK: mov dptr, #a
// CHECK: mov a, #17
// CHECK: movx @dptr, a
// CHECK: mov dptr, #b
// CHECK: mov a, #9
// CHECK: movx @dptr, a
// CHECK: mov dptr, #a
// CHECK: movx a, @dptr
// CHECK: mov 240, #3
// CHECK-NEXT: mul ab
// CHECK: mov dptr, #b
// CHECK-NEXT: movx a, @dptr
// CHECK-NEXT: add a, r0
// CHECK-NEXT: mov r0, a
// CHECK: mov dptr, #a
// CHECK-NEXT: movx a, @dptr
// CHECK-NEXT: mov r2, a
// CHECK: mov dptr, #b
// CHECK-NEXT: movx a, @dptr
// CHECK-NEXT: xch a, r2
// CHECK-NEXT: clr c
// CHECK-NEXT: subb a, r2
// CHECK-NEXT: xrl a, r0
// CHECK: mov dptr, #result
// CHECK-NEXT: movx @dptr, a
// CHECK-NEXT: ret

// Linking knows the final XDATA layout. Adjacent global addresses reuse DPTR
// with INC DPTR, including across volatile accesses.
// LINK-LABEL: <main>:
// LINK-COUNT-3: inc dptr
// LINK-NOT: mov dptr, #1
// LINKSYM: Name: main
// LINKSYM: Size: 39
