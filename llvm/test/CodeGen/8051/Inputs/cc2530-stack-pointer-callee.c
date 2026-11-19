typedef unsigned char u8;
__attribute__((noinline)) u8 read_byte(const u8 *p) { return *p; }
__attribute__((noinline)) void write_byte(u8 *p, u8 v) { *p = v; }

__attribute__((noinline)) u8 *identity(u8 *p) { return p; }
__attribute__((noinline)) u8 *idata_to_default(__idata u8 *p) {
  return (u8 *)p;
}
__attribute__((noinline)) u8 *data_to_default(__data u8 *p) {
  return (u8 *)p;
}
