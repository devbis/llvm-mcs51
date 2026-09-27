// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin %s -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s --check-prefix=LINK

typedef struct {
  unsigned char Low;
  unsigned char High;
  unsigned char Tag;
} TaggedByte;

__attribute__((noinline)) TaggedByte identity(TaggedByte Value) {
  return Value;
}

__attribute__((noinline)) unsigned char call_identity(void) {
  TaggedByte Input = {0x11, 0x22, 0x44};
  TaggedByte Result = identity(Input);
  return Result.Low + Result.High + Result.Tag;
}

typedef struct {
  unsigned char Bytes[7];
} LargeValue;

__attribute__((noinline)) LargeValue copy_large(LargeValue Value) {
  return Value;
}

__attribute__((noinline)) unsigned char call_copy_large(void) {
  LargeValue Input = {{1, 2, 3, 4, 5, 6, 7}};
  LargeValue Result = copy_large(Input);
  return Result.Bytes[0] + Result.Bytes[6];
}

int main(unsigned char Choice) {
  if (Choice)
    return call_identity();
  return call_copy_large();
}

// CHECK-LABEL: identity:
// CHECK: ret
// CHECK-LABEL: call_identity:
// CHECK: lcall identity
// CHECK-LABEL: copy_large:
// CHECK: ret
// CHECK-LABEL: call_copy_large:
// CHECK: lcall copy_large

// LINK-DAG: Name: identity
// LINK-DAG: Name: copy_large
