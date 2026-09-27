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

u8 call_indirect_with_stack_arg(u8 (*fn)(u8, u8, u8, u8, u8), u8 a, u8 b,
                                u8 c, u8 d, u8 e) {
  return fn(a, b, c, d, e);
}

// The 8051 has no indirect CALL opcode. The backend calls a local thunk,
// which restores an ordinary call frame before jumping through DPTR.
// CHECK-LABEL: call_indirect:
// CHECK: push 130
// CHECK: push 131
// CHECK: lcall .Lcall_indirect.mcs51.icall
// CHECK-LABEL: .Lcall_indirect.mcs51.icall:
// CHECK: jmp @a+dptr
// CHECK-LABEL: call_indirect_with_arg:
// CHECK: push 130
// CHECK: push 131
// CHECK: lcall .Lcall_indirect_with_arg.mcs51.icall
// CHECK-LABEL: .Lcall_indirect_with_arg.mcs51.icall:
// CHECK: jmp @a+dptr
// CHECK-LABEL: call_indirect_with_stack_arg:
// CHECK: lcall .Lcall_indirect_with_stack_arg.mcs51.icall
// CHECK-LABEL: .Lcall_indirect_with_stack_arg.mcs51.icall:
// CHECK: jmp @a+dptr

// DIS-LABEL: <call_indirect>:
// DIS: lcall {{[0-9]+}}
// DIS: ret
// DIS: mov a, 129
// DIS: jmp @a+dptr
// DIS-LABEL: <call_indirect_with_arg>:
// DIS: lcall {{[0-9]+}}
// DIS: ret
// DIS: mov a, 129
// DIS: jmp @a+dptr
