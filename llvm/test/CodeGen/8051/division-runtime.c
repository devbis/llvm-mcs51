// RUN: clang -target mcs51 -O2 -S %s -o - | FileCheck %s --check-prefix=CALL
// RUN: clang -target mcs51 -mcpu=cc2530 -O2 %s -o %t.elf
// RUN: llvm-readobj --symbols %t.elf | FileCheck %s --check-prefix=LINK
// RUN: FileCheck %s --check-prefix=IHEX < %t.hex

unsigned int divide_unsigned_word(unsigned int Numerator,
                                  unsigned int Denominator) {
  return Numerator / Denominator;
}

int divide_signed_word(int Numerator, int Denominator) {
  return Numerator / Denominator;
}

unsigned int remainder_unsigned_word(unsigned int Numerator,
                                     unsigned int Denominator) {
  return Numerator % Denominator;
}

int remainder_signed_word(int Numerator, int Denominator) {
  return Numerator % Denominator;
}

unsigned long divide_unsigned_long(unsigned long Numerator,
                                   unsigned long Denominator) {
  return Numerator / Denominator;
}

long divide_signed_long(long Numerator, long Denominator) {
  return Numerator / Denominator;
}

unsigned long remainder_unsigned_long(unsigned long Numerator,
                                      unsigned long Denominator) {
  return Numerator % Denominator;
}

long remainder_signed_long(long Numerator, long Denominator) {
  return Numerator % Denominator;
}

unsigned long long shift_left_long_long(unsigned long long Value,
                                        unsigned Amount) {
  return Value << Amount;
}

unsigned long long shift_right_unsigned_long_long(unsigned long long Value,
                                                  unsigned Amount) {
  return Value >> Amount;
}

long long shift_right_signed_long_long(long long Value, unsigned Amount) {
  return Value >> Amount;
}

unsigned long long divide_unsigned_long_long(unsigned long long Numerator,
                                             unsigned long long Denominator) {
  return Numerator / Denominator;
}

long long divide_signed_long_long(long long Numerator,
                                  long long Denominator) {
  return Numerator / Denominator;
}

unsigned long long remainder_unsigned_long_long(unsigned long long Numerator,
                                                unsigned long long Denominator) {
  return Numerator % Denominator;
}

long long remainder_signed_long_long(long long Numerator,
                                     long long Denominator) {
  return Numerator % Denominator;
}

int main(void) {
  volatile unsigned long Numerator = 70001;
  volatile unsigned long Denominator = 191;
  return divide_unsigned_word((unsigned int)Numerator,
                              (unsigned int)Denominator) +
         divide_signed_word((int)Numerator, (int)Denominator) +
         remainder_unsigned_word((unsigned int)Numerator,
                                 (unsigned int)Denominator) +
         remainder_signed_word((int)Numerator, (int)Denominator) +
         (int)divide_unsigned_long(Numerator, Denominator) +
         (int)divide_signed_long((long)Numerator, (long)Denominator) +
         (int)remainder_unsigned_long(Numerator, Denominator) +
         (int)remainder_signed_long((long)Numerator, (long)Denominator);
}

// CALL-LABEL: divide_unsigned_word:
// CALL: lcall __udivhi3
// CALL-LABEL: divide_signed_word:
// CALL: lcall __divhi3
// CALL-LABEL: remainder_unsigned_word:
// CALL: lcall __umodhi3
// CALL-LABEL: remainder_signed_word:
// CALL: lcall __modhi3
// CALL-LABEL: divide_unsigned_long:
// CALL: lcall __udivsi3
// CALL-LABEL: divide_signed_long:
// CALL: lcall __divsi3
// CALL-LABEL: remainder_unsigned_long:
// CALL: lcall __umodsi3
// CALL-LABEL: remainder_signed_long:
// CALL: lcall __modsi3
// CALL-LABEL: shift_left_long_long:
// CALL: lcall __ashldi3
// CALL-LABEL: shift_right_unsigned_long_long:
// CALL: lcall __lshrdi3
// CALL-LABEL: shift_right_signed_long_long:
// CALL: lcall __ashrdi3
// CALL-LABEL: divide_unsigned_long_long:
// CALL: lcall __udivdi3
// CALL-LABEL: divide_signed_long_long:
// CALL: lcall __divdi3
// CALL-LABEL: remainder_unsigned_long_long:
// CALL: lcall __umoddi3
// CALL-LABEL: remainder_signed_long_long:
// CALL: lcall __moddi3

// LINK-DAG: Name: __udivhi3
// LINK-DAG: Name: __divhi3
// LINK-DAG: Name: __umodhi3
// LINK-DAG: Name: __modhi3
// LINK-DAG: Name: __udivsi3
// LINK-DAG: Name: __divsi3
// LINK-DAG: Name: __umodsi3
// LINK-DAG: Name: __modsi3
// IHEX: :00000001FF
