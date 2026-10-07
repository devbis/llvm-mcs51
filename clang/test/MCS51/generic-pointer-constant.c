// RUN: %clang_cc1 -triple mcs51 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple mcs51 -emit-llvm -DCODEGEN %s -o - | FileCheck %s

// Converting a pointer to a space with a different pointer width is not an
// identity on its integer value: a generic pointer adds a space tag
// ((u8 __generic *)(u8 __data *)0x30 is 0x400030, not 0x30). The constant
// evaluator must not fold such conversions of integer-valued pointers.

typedef unsigned char u8;
typedef u8 __generic *gp;

#ifndef CODEGEN
// Folding the offset into the integer value used to drop the tag and emit
// 0x33 here.
gp const __code bad = (gp)(u8 __data *)0x30 + 3; // expected-error {{initializer element is not a compile-time constant}}

// Converted object addresses and null remain constant initializers.
__data u8 d[4];
gp const __code good[4] = {(gp)d, (gp)d + 2, (gp)(u8 __data *)0,
                           (gp)(u8 __data *)0x30};

// Pointers of the same width are still folded.
_Static_assert((u8 __xdata *)(u8 *)0x1234 == (u8 __xdata *)0x1234, "");
#else
// A converted literal address is emitted as a constant cast.
// CHECK: @lit = {{.*}}constant ptr addrspace(8) addrspacecast (ptr addrspace(1) inttoptr (i8 48 to ptr addrspace(1)) to ptr addrspace(8))
gp const __code lit = (gp)(u8 __data *)0x30;

// The conversion is left to code generation instead of an integer constant.
// CHECK-LABEL: @f(
// CHECK: ret ptr addrspace(8) getelementptr {{.*}}(i8, ptr addrspace(8) addrspacecast (ptr addrspace(1) inttoptr (i8 48 to ptr addrspace(1)) to ptr addrspace(8)), i32 3)
// CHECK-NOT: inttoptr (i32 51
gp f(void) { return (gp)(u8 __data *)0x30 + 3; }
#endif
