typedef unsigned char u8;
__attribute__((noinline)) u8 read_byte(const u8 *p) { return *p; }
__attribute__((noinline)) void write_byte(u8 *p, u8 v) { *p = v; }
