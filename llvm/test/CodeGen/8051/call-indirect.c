// RUN: clang -target mcs51 -S -emit-llvm -O2 %s -o - | llc -mtriple=mcs51 -o - | FileCheck %s
// RUN: clang -target mcs51 -S -emit-llvm -O2 %s -o - | llc -mtriple=mcs51 -filetype=obj -o %t.o
// RUN: ld.lld -e call_indirect -T %S/../../../lib/Target/MCS51/cc2530.ld --no-check-sections -o %t.elf %t.o
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DIS

typedef unsigned char u8;

u8 call_indirect(u8 (*fn)(void)) {
  return fn();
}

u8 call_indirect_with_arg(u8 (*fn)(u8), u8 value) {
  return fn(value);
}

// The 8051 has no indirect CALL opcode. The backend calls its local
// dispatcher, which jumps through DPTR; the return address skips the
// dispatcher and continues after it.
// CHECK-LABEL: call_indirect:
// CHECK: lcall .LBB0_
// CHECK: jmp @a+dptr
// CHECK-LABEL: call_indirect_with_arg:
// CHECK: lcall .LBB1_
// CHECK: jmp @a+dptr

// DIS-LABEL: <call_indirect>:
// DIS: lcall 4
// DIS: ret
// DIS: clr a
// DIS: jmp @a+dptr
// DIS-LABEL: <call_indirect_with_arg>:
// DIS: lcall 10
// DIS: ret
// DIS: clr a
// DIS: jmp @a+dptr
