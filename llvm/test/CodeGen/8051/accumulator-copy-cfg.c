// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -nostdlib -mllvm -verify-machineinstrs -S %s -o %t.s
// RUN: FileCheck %s < %t.s

unsigned char accumulator_copy_cfg(unsigned char lhs, unsigned char rhs,
                                   unsigned char choose) {
  unsigned char value;
  if (choose)
    value = (unsigned char)(lhs + rhs);
  else
    value = (unsigned char)(lhs - rhs);

  if (lhs == 0)
    return value;
  return (unsigned char)(value ^ rhs);
}

unsigned char accumulator_constant_cfg(unsigned char choose,
                                       unsigned char value) {
  unsigned char snapshot;
  if (choose)
    snapshot = 0xff;
  else
    snapshot = value;
  if (value == 0)
    return snapshot;
  return (unsigned char)(snapshot + value);
}

// Rematerialize the constant into its GPR while keeping the A update.
// CHECK-LABEL: accumulator_constant_cfg:
// CHECK: mov a, #-1
// CHECK: mov r{{[0-7]}}, #-1
