// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -nostdlib \
// RUN:   -Wl,-T,%S/../../../lib/Target/MCS51/cc2530.ld \
// RUN:   -Wl,--no-check-sections \
// RUN:   %S/../../../lib/Target/MCS51/cc2530_startup.s %s -o %t.elf
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=LINK
// RUN: llvm-readobj --sections %t.elf | FileCheck %s --check-prefix=MAP
// RUN: llvm-objcopy -O ihex %t.elf %t.hex
// RUN: FileCheck %s --check-prefix=HEX < %t.hex

__attribute__((section(".bank2.text")))
unsigned char banked_add(unsigned char value);

__attribute__((noinline, section(".bank2.text")))
unsigned char banked_add(unsigned char value) {
  return value + 1;
}

__attribute__((noinline, section(".bank2.text")))
unsigned char bank2_caller(unsigned char value) { return banked_add(value); }

__attribute__((noinline, section(".bank3.text")))
unsigned char banked_caller(unsigned char value) { return banked_add(value); }

unsigned char caller(unsigned char value) { return banked_add(value); }
int main(void) { return 0; }

// CHECK: .section .bank2.text,"ax"
// CHECK-LABEL: banked_add:
// CHECK: .section .text.bankthunks,"ax"
// CHECK-LABEL: __mcs51_bankcall_banked_add:
// CHECK: push 159
// CHECK: mov 159, #2
// CHECK: lcall banked_add
// CHECK: pop 159
// CHECK: ret
// CHECK: .section .bank2.text,"ax"
// CHECK-LABEL: bank2_caller:
// CHECK: lcall banked_add
// CHECK: .section .bank3.text,"ax"
// CHECK-LABEL: banked_caller:
// CHECK: lcall __mcs51_bankcall_banked_add
// CHECK: .text
// CHECK-LABEL: caller:
// CHECK: lcall __mcs51_bankcall_banked_add

// MAP: Name: .bank2
// MAP: Address: 0x8000
// LINK-LABEL: <__mcs51_bankcall_banked_add>:
// LINK: push 159
// LINK: mov 159, #2
// LINK: lcall 32768
// LINK: pop 159
// LINK: ret
// LINK-LABEL: <caller>:
// LINK: lcall
// LINK-LABEL: <banked_caller>:
// LINK: lcall

// Bank 2's physical flash image starts at 0x10000, although it executes at
// the shared CODE window starting at 0x8000.
// HEX: :020000021000EC
