// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin %s -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s --check-prefix=LINK
// RUN: llvm-objdump -d %t.elf | FileCheck %s --check-prefix=DIS

char *strcpy(char *Destination, const char *Source);
int strcmp(const char *LHS, const char *RHS);

const char Source[] = {'8', '0', '5', '1', 0};
char Destination[5];

int main(void) {
  strcpy(Destination, Source);
  return strcmp(Destination, Source);
}

// LINK-DAG: Name: strcpy
// LINK-DAG: Name: strcmp
// DIS-LABEL: <main>:
// DIS: lcall
// DIS: lcall
