// RUN: clang -target mcs51 -O2 -mllvm -verify-machineinstrs -S %s -o - | FileCheck %s
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin %s -Wl,--no-gc-sections -o %t.elf
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

typedef struct {
  unsigned char Bytes[9];
} ExpandedArgument;

__attribute__((noinline)) unsigned char sum_expanded(ExpandedArgument Value) {
  return Value.Bytes[0] + Value.Bytes[8];
}

__attribute__((noinline)) unsigned char call_sum_expanded(void) {
  ExpandedArgument Input = {{1, 2, 3, 4, 5, 6, 7, 8, 9}};
  return sum_expanded(Input);
}

typedef struct {
  unsigned char Bytes[9];
} LargeReturn;

__attribute__((noinline)) LargeReturn return_large(void) {
  LargeReturn Result = {{1, 2, 3, 4, 5, 6, 7, 8, 9}};
  return Result;
}

__attribute__((noinline)) unsigned char call_return_large(void) {
  LargeReturn Result = return_large();
  return Result.Bytes[0] + Result.Bytes[8];
}

typedef union {
  unsigned char Bytes[9];
  unsigned int Words[5];
} LargeUnion;

__attribute__((noinline)) unsigned char sum_union(LargeUnion Value) {
  return Value.Bytes[0] + Value.Bytes[8];
}

__attribute__((noinline)) unsigned char call_sum_union(void) {
  LargeUnion Input = {.Bytes = {1, 2, 3, 4, 5, 6, 7, 8, 9}};
  return sum_union(Input);
}

int main(void) {
  return call_identity();
}

// CHECK-LABEL: identity:
// CHECK: ret
// CHECK-LABEL: call_identity:
// CHECK: lcall identity
// CHECK-LABEL: copy_large:
// CHECK: ret
// CHECK-LABEL: call_copy_large:
// CHECK: lcall copy_large
// CHECK-LABEL: sum_expanded:
// CHECK: ret
// CHECK-LABEL: call_sum_expanded:
// CHECK: lcall sum_expanded
// CHECK-LABEL: return_large:
// CHECK: movc a, @a+dptr
// CHECK: mov @r{{[01]}}, a
// CHECK: ret
// CHECK-LABEL: call_return_large:
// CHECK: lcall return_large
// CHECK-LABEL: sum_union:
// CHECK: ret
// CHECK-LABEL: call_sum_union:
// CHECK: lcall sum_union
// LINK-DAG: Name: identity
// LINK-DAG: Name: copy_large
// LINK-DAG: Name: sum_expanded
// LINK-DAG: Name: return_large
// LINK-DAG: Name: sum_union
