// The MCS-51 size pipeline runs the machine outliner twice more. Leave an
// explicit LLVM override in control when one is provided.
// RUN: %clang -target mcs51 -mcpu=cc2530 -Oz -### -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=OZ %s
// RUN: %clang -target mcs51 -mcpu=cc2530 -O2 -### -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=O2 %s
// RUN: %clang -target mcs51 -mcpu=cc2530 -Oz -mllvm \
// RUN:   -machine-outliner-reruns=0 -### -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CUSTOM %s

// OZ: "-mllvm" "-machine-outliner-reruns=2"
// O2-NOT: machine-outliner-reruns
// CUSTOM: "-mllvm" "-machine-outliner-reruns=0"
// CUSTOM-NOT: "-machine-outliner-reruns=2"
