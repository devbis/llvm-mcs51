typedef unsigned char u8;
typedef unsigned short u16;
#define RESULT ((volatile __xdata u8 *)0x1e00)
extern u8 read_byte(const u8 *p);
extern void write_byte(u8 *p, u8 v);
int main(void) {
  u8 local[2];
  local[0] = 0x42;
  local[1] = 0x17;
#ifdef MANUAL_STACK_ALIAS
  u8 *pointer = (u8 *)((u16)local | 0x1f00);
#else
  u8 *pointer = local;
#endif
  RESULT[0] = 0xcc;
  RESULT[1] = read_byte(pointer);
  write_byte(pointer + 1, 0x69);
  RESULT[2] = local[1];
  RESULT[0] = 0xa5;
  for (;;) {}
}
