// The driver enables MCS-51 linker relaxation by default; -mno-relax turns it
// off.
// RUN: %clang -### --target=mcs51 -c %s 2>&1 | FileCheck %s --check-prefix=RELAX
// RUN: %clang -### --target=mcs51 -mno-relax -mrelax -c %s 2>&1 | FileCheck %s --check-prefix=RELAX
// RUN: %clang -### --target=mcs51 -mno-relax -c %s 2>&1 | FileCheck %s --check-prefix=NORELAX

// RELAX: "-target-feature" "+relax"
// NORELAX-NOT: "+relax"
