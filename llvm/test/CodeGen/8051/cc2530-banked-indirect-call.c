// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=LINK
// RUN: llvm-objcopy -O ihex %t.elf %t.hex
// RUN: FileCheck %s --check-prefix=HEX < %t.hex

typedef unsigned char (*callback_t)(unsigned char);

__attribute__((noinline, section(".bank2.text")))
unsigned char banked_add(unsigned char value) {
  return value + 1;
}

callback_t volatile banked_function_pointer = banked_add;

__attribute__((noinline))
unsigned char invoke_function_pointer(callback_t function,
                                      unsigned char value) {
  return function(value);
}

unsigned char banked_pointer_caller(unsigned char value) {
  return invoke_function_pointer(banked_add, value);
}

unsigned char banked_global_pointer_caller(unsigned char value) {
  return banked_function_pointer(value);
}

int main(void) { return banked_global_pointer_caller(0); }

// CHECK-LABEL: banked_add:
// CHECK: .section .text.bankthunks,"ax"
// CHECK-LABEL: __mcs51_bankcall_banked_add:
// CHECK: mov 159, #2
// CHECK: lcall banked_add
// CHECK-LABEL: invoke_function_pointer:
// CHECK: lcall .Linvoke_function_pointer.mcs51.icall
// CHECK: jmp @a+dptr
// CHECK-LABEL: banked_pointer_caller:
// CHECK: mov dptr, #__mcs51_bankcall_banked_add
// CHECK: lcall invoke_function_pointer
// CHECK-LABEL: banked_global_pointer_caller:
// CHECK: lcall .Lbanked_global_pointer_caller.mcs51.icall
// CHECK: jmp @a+dptr
// CHECK: .short __mcs51_bankcall_banked_add

// LINK-LABEL: <__mcs51_bankcall_banked_add>:
// LINK: mov 159, #2
// LINK: lcall 32768
// LINK-LABEL: <invoke_function_pointer>:
// LINK: lcall
// LINK: jmp @a+dptr
// LINK-LABEL: <banked_pointer_caller>:
// LINK: mov dptr, #
// LINK: lcall
// LINK-LABEL: <banked_global_pointer_caller>:
// LINK: lcall
// LINK: jmp @a+dptr

// HEX: :00000001FF
