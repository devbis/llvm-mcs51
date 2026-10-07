// RUN: clang -target mcs51 -mcpu=cc2530 -std=c99 -O0 -mllvm -verify-machineinstrs -c %s -o /dev/null
// RUN: clang -target mcs51 -mcpu=cc2530 -std=c99 -O1 -mllvm -verify-machineinstrs -c %s -o /dev/null
// RUN: clang -target mcs51 -mcpu=cc2530 -std=c99 -O2 -mllvm -verify-machineinstrs -c %s -o /dev/null
// RUN: clang -target mcs51 -mcpu=cc2530 -std=c99 -O3 -mllvm -verify-machineinstrs -c %s -o /dev/null
// RUN: clang -target mcs51 -mcpu=cc2530 -std=c99 -Os -mllvm -verify-machineinstrs -c %s -o /dev/null
// RUN: clang -target mcs51 -mcpu=cc2530 -std=c99 -Oz -mllvm -verify-machineinstrs -c %s -o /dev/null

typedef unsigned char u8;
typedef unsigned short u16;

// More than four word arguments force stack arguments. The live values force
// spills, and the volatile selection makes instruction selection split the
// call sequence across blocks after some argument bytes have already been
// pushed. Spill frame indices in those blocks must use the actual entry SP.
extern u16 many(u8, u8, u8, u8,
                u16, u16, u16, u16, u16, u16, u16, u16,
                u16, u16, u16, u16, u16, u16, u16, u16,
                u16, u16, u16, u16, u16, u16, u16, u16,
                u16, u16, u16, u16, u16, u16, u16, u16);

#define DECL(N) u16 a##N = input[N]
u16 frame_cfg_spills(u8 selector, volatile u8 *flag, volatile u16 *input) {
  u8 local = selector;
  DECL(0);  DECL(1);  DECL(2);  DECL(3);  DECL(4);  DECL(5);  DECL(6);  DECL(7);
  DECL(8);  DECL(9);  DECL(10); DECL(11); DECL(12); DECL(13); DECL(14); DECL(15);
  DECL(16); DECL(17); DECL(18); DECL(19); DECL(20); DECL(21); DECL(22); DECL(23);
  DECL(24); DECL(25); DECL(26); DECL(27); DECL(28); DECL(29); DECL(30); DECL(31);
  u16 chosen = *flag ? a0 : a1;
  u16 result = many(selector, selector + 1, selector + 2, selector + 3,
                    chosen, a2, a3, a4, a5, a6, a7, a8,
                    a9, a10, a11, a12, a13, a14, a15, a16,
                    a17, a18, a19, a20, a21, a22, a23, a24,
                    a25, a26, a27, a28, a29, a30, a31, (u16)&local);
  return result + a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 +
         a11 + a12 + a13 + a14 + a15 + a16 + a17 + a18 + a19 + a20 + a21 +
         a22 + a23 + a24 + a25 + a26 + a27 + a28 + a29 + a30 + a31;
}
