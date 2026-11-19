typedef unsigned char u8;
__attribute__((noinline, section(".mcs51.autobank.simple")))
u8 simple(u8 x) { return x + 7; }
__attribute__((noinline, section(".mcs51.autobank.wide")))
u8 wide(unsigned long long x) { return (u8)x + (u8)(x >> 24); }
__attribute__((noinline, section(".mcs51.autobank.asm")))
u8 asmf(u8 x) { __asm__ volatile ("nop"); return x + 9; }
