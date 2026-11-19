typedef unsigned char u8;
typedef u8 (*callback_t)(u8);
#define FMAP (*(volatile __sfr u8 *)0x9f)
#define RESULT ((volatile __xdata u8 *)0x1e00)
extern u8 simple(u8);
extern u8 wide(unsigned long long);
extern u8 asmf(u8);
volatile callback_t callback = simple;
__attribute__((noinline, section(".bank2.cross")))
u8 cross(u8 x) {
  u8 before = FMAP;
  u8 value = simple(x);
  if (FMAP != before || before != 2) return 0xee;
  return value;
}
__attribute__((noinline, section(".bank1.caller")))
u8 caller(u8 x) {
  u8 bad = 0;
  u8 before = FMAP;
  if (before != 1) bad |= 1;
  if (simple(x) != 10 || FMAP != before) bad |= 2;
  if (wide(0x0807060504030201ULL) != 5 || FMAP != before) bad |= 4;
  if (callback(x) != 10 || FMAP != before) bad |= 8;
  if (cross(x) != 10 || FMAP != before) bad |= 16;
  if (asmf(x) != 12 || FMAP != before) bad |= 32;
  return bad;
}
int main(void) {
  RESULT[0] = 0xcc;
  RESULT[1] = caller(3);
  RESULT[2] = FMAP;
  RESULT[0] = 0xa5;
  for (;;) {}
}
