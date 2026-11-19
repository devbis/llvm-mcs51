typedef unsigned char u8;
__attribute__((noinline)) u8 read_idata(__idata const u8 *p) { return *p; }
__attribute__((noinline)) void write_idata(__idata u8 *p, u8 v) { *p = v; }
__attribute__((noinline)) u8 read_generic(__generic const u8 *p) { return *p; }
__attribute__((noinline)) void write_generic(__generic u8 *p, u8 v) { *p = v; }
