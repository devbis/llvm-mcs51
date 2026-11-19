typedef unsigned char u8;
__attribute__((section(".mcs51.xdata.simresult"))) volatile __xdata u8 Result;
extern u8 read_idata(__idata const u8 *);
extern void write_idata(__idata u8 *, u8);
extern u8 read_generic(__generic const u8 *);
extern void write_generic(__generic u8 *, u8);
int main(void) {
  u8 local[2];
  local[0] = 0x42;
  local[1] = 0x17;
  u8 bad = read_idata((__idata u8 *)local) != 0x42;
  write_idata((__idata u8 *)(local + 1), 0x69);
  bad |= (local[1] != 0x69) << 1;
  bad |= (read_generic((__generic u8 *)local) != 0x42) << 2;
  write_generic((__generic u8 *)(local + 1), 0x35);
  bad |= (local[1] != 0x35) << 3;
  Result = bad;
  for (;;) {}
}
