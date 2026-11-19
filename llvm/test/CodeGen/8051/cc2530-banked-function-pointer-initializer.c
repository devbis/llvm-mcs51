// The address of a CC2530 banked function stored in a static initializer must
// refer to the start of its common-flash bank-call trampoline, whatever the
// element's position inside the initialized object. AsmPrinter::lowerConstant
// receives that position as Offset; it is not an addend. Before the fix an
// element at byte position N pointed N bytes into the trampoline, skipping
// `push FMAP`, so the trampoline's `pop FMAP; ret` consumed the caller's
// return address.
// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -c %s -o %t.o
// RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC

typedef unsigned short u16;
typedef unsigned char u8;
typedef u16 (*fn_t)(u16);

__attribute__((noinline, section(".bank1.text.fa"))) u16 fa(u16 v) { return v + 1; }
__attribute__((noinline, section(".bank2.text.fb"))) u16 fb(u16 v) { return v * 2; }
// Placed in a bank by the linker (CC2530 auto banking).
__attribute__((noinline)) u16 fc(u16 v) { return v ^ 0x5A5A; }

const __code fn_t ctab[3] = {fa, fb, fc};
__xdata fn_t xtab[3] = {fc, fb, fa};
struct ent { u8 tag; fn_t f; };
const __code struct ent stab[2] = {{0x11, fb}, {0x22, fa}};

// CHECK-LABEL: ctab:
// CHECK-NEXT: .short __mcs51_bankcall_fa{{$}}
// CHECK-NEXT: .short __mcs51_bankcall_fb{{$}}
// CHECK-NEXT: .short __mcs51_bankcall_fc{{$}}
// CHECK-LABEL: xtab:
// CHECK-NEXT: .short __mcs51_bankcall_fc{{$}}
// CHECK-NEXT: .short __mcs51_bankcall_fb{{$}}
// CHECK-NEXT: .short __mcs51_bankcall_fa{{$}}
// CHECK-LABEL: stab:
// CHECK-NEXT: .byte 17
// CHECK-NEXT: .short __mcs51_bankcall_fb{{$}}
// CHECK-NEXT: .byte 34
// CHECK-NEXT: .short __mcs51_bankcall_fa{{$}}

// RELOC-LABEL: .rela.mcs51.code.rodata.ctab {
// RELOC-NEXT: 0x0 R_8051_16 __mcs51_bankcall_fa 0x0
// RELOC-NEXT: 0x2 R_8051_16 __mcs51_bankcall_fb 0x0
// RELOC-NEXT: 0x4 R_8051_16 __mcs51_bankcall_fc 0x0
// RELOC-NEXT: }
// RELOC-LABEL: .rela.mcs51.xdata.init.xtab {
// RELOC-NEXT: 0x0 R_8051_16 __mcs51_bankcall_fc 0x0
// RELOC-NEXT: 0x2 R_8051_16 __mcs51_bankcall_fb 0x0
// RELOC-NEXT: 0x4 R_8051_16 __mcs51_bankcall_fa 0x0
// RELOC-NEXT: }
// RELOC-LABEL: .rela.mcs51.code.rodata.stab {
// RELOC-NEXT: 0x1 R_8051_16 __mcs51_bankcall_fb 0x0
// RELOC-NEXT: 0x4 R_8051_16 __mcs51_bankcall_fa 0x0
// RELOC-NEXT: }
