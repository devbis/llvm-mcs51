// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -mabi=iar -S %s -o - | FileCheck %s --check-prefix=IAR
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -S %s -o - | FileCheck %s --check-prefix=CLANG

typedef unsigned short u16;

volatile u16 inputs[12];
volatile u16 result;
void barrier(void);

__attribute__((noinline)) void keep_values_live(void) {
  u16 v0 = inputs[0];
  u16 v1 = inputs[1];
  u16 v2 = inputs[2];
  u16 v3 = inputs[3];
  u16 v4 = inputs[4];
  u16 v5 = inputs[5];
  u16 v6 = inputs[6];
  u16 v7 = inputs[7];
  u16 v8 = inputs[8];
  u16 v9 = inputs[9];
  u16 v10 = inputs[10];
  u16 v11 = inputs[11];
  barrier();
  __asm__ volatile(""
                   : "+r"(v0), "+r"(v1), "+r"(v2), "+r"(v3),
                     "+r"(v4), "+r"(v5), "+r"(v6), "+r"(v7),
                     "+r"(v8), "+r"(v9), "+r"(v10), "+r"(v11));
  result = v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11;
}

__attribute__((noinline)) void keep_one_word_live(void) {
  u16 value = inputs[0];
  barrier();
  __asm__ volatile("" : "+r"(value));
  result = value;
}

__attribute__((noinline)) void clobber_r6_r7(void) {
  __asm__ volatile("" : : : "r6", "r7");
  barrier();
}

int main(void) {
  keep_values_live();
  keep_one_word_live();
  return 0;
}

// IAR functions save large prefixes of their callee-saved imaginary registers
// through the shared runtime helper. The stack and DPTR saves remain local.
// IAR-LABEL: keep_values_live:
// IAR: push 130
// IAR: push 131
// IAR: lcall __mcs51_save_iar_im0_8
// IAR: lcall __mcs51_save_iar_im{{[3-9]|1[0-6]}}
// IAR: lcall barrier
// IAR: lcall __mcs51_restore_iar_im{{[3-9]|1[0-6]}}
// IAR: lcall __mcs51_restore_iar_im0_8
// IAR: pop 131
// IAR: pop 130
// IAR: ret

// A single 16-bit value occupies the two-register IM0..IM1 prefix.
// IAR-LABEL: keep_one_word_live:
// IAR: push 130
// IAR: push 131
// IAR: lcall __mcs51_save_iar_im0_2
// IAR: lcall barrier
// IAR: lcall __mcs51_restore_iar_im0_2
// IAR: ret

// Prologue helpers and direct calls do not require a DPTR frame when the
// function body itself leaves DPTR untouched.
// IAR-LABEL: clobber_r6_r7:
// IAR-NOT: push 130
// IAR-NOT: push 131
// IAR: lcall __mcs51_save_iar_r67_2
// IAR: lcall barrier
// IAR: ljmp __mcs51_return_r67_2

// The default Clang ABI is not changed by the IAR-only call and save rules.
// CLANG-LABEL: keep_values_live:
// CLANG-NOT: "?XSP"
// CLANG: lcall barrier
// The standard Clang ABI retains its register convention and can tail-return
// through the helper because it does not need to restore DPTR.
// CLANG-LABEL: clobber_r6_r7:
// CLANG: lcall __mcs51_save_r67_2
// CLANG: lcall barrier
// CLANG: ljmp __mcs51_return_r67_2
