// RUN: clang -target mcs51 -fsyntax-only %s
// RUN: clang -target mcs51 -S -emit-llvm -o - %s | FileCheck %s --check-prefix=IR
// RUN: clang -target mcs51 -### -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=DEFAULT
// RUN: clang -target mcs51 -fsigned-char -### -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=SIGNED --implicit-check-not=-fno-signed-char

// DEFAULT: "-cc1" "-triple" "mcs51"
// DEFAULT-SAME: "-fno-signed-char"
// SIGNED: "-cc1" "-triple" "mcs51"

#ifndef __8051__
#error "missing MCS-51 predefined macro"
#endif
#ifndef __CHAR_UNSIGNED__
#error "plain char must be unsigned on MCS-51"
#endif

_Static_assert(sizeof(void *) == 2, "near pointers are 16-bit");
_Static_assert(sizeof(char) == 1, "char is 8-bit");
_Static_assert(sizeof(int) == 2, "int is 16-bit");
_Static_assert(sizeof(long) == 4, "long is 32-bit");
_Static_assert(sizeof(float) == 4, "float is 32-bit");
_Static_assert(sizeof(double) == 4, "double is 32-bit");

int entry(void) { return 0; }

// IR: target datalayout = "e-p:16:8-p1:8:8-p2:8:8-p3:8:8-p4:16:8-p5:16:8-p6:8:8-p7:8:8-i1:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8:16"
// IR: target triple = "mcs51"
// IR: define dso_local i16 @entry()
