// RUN: %clang -target mcs51 -mcpu=cc2530 -flto -Oz -c %s -o %t.o
// RUN: %clang -target mcs51 -mcpu=cc2530 -flto -Oz -### %t.o -o %t.elf 2>&1 \
// RUN:   | FileCheck --check-prefix=CC2530-OZ %s
// RUN: %clang -target mcs51 -mcpu=cc2530 -flto -O2 -### %t.o -o %t.elf 2>&1 \
// RUN:   | FileCheck --check-prefix=CC2530-O2 %s

// CC2530-OZ: "-plugin-opt=O1"
// CC2530-O2: "-plugin-opt=O2"

void test(void) {}
