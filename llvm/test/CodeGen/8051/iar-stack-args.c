// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -mabi=iar -S %s -o - | FileCheck %s --check-prefix=IAR
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -S %s -o - | FileCheck %s --check-prefix=CLANG
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -mabi=iar %s -o %t.elf
// RUN: llvm-readobj --sections %t.elf | FileCheck %s --check-prefix=LINK
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s --check-prefix=IAR-SYMS

typedef unsigned char u8;
volatile u8 sink;

__attribute__((noinline)) u8 sixth(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f) {
  sink = a + b + c + d + e + f;
  return f;
}

u8 call_sixth(u8 a, u8 b, u8 c, u8 d, u8 e) {
  return sixth(a, b, c, d, e, 0x5a);
}

int main(void) { return call_sixth(1, 2, 3, 4, 5); }

// The IAR ABI sends the sixth byte through the downward-growing XDATA stack.
// The callee reads it at XSP; the caller restores XSP after the call.
// IAR-LABEL: sixth:
// IAR: movx a, @dptr
// IAR-LABEL: call_sixth:
// IAR: "?XSP"
// IAR: movx @dptr, a
// IAR: lcall sixth
// IAR: "?XSP"

// The default ABI stays on its own hardware stack and never references ?XSP.
// CLANG-NOT: ?XSP
// CLANG: call_sixth:
// CLANG: lcall sixth

// The IAR runtime contributes a reserved XDATA stack to the final image.
// LINK: Name: .mcs51_iar_xstack

// The IAR runtime also exports its direct-RAM virtual argument registers.
// IAR-SYMS: Name: ?V0
// IAR-SYMS: Value: 0x30
// IAR-SYMS: Name: ?V7
// IAR-SYMS: Value: 0x37
