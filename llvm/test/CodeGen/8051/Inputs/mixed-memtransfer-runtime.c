typedef unsigned char u8;
typedef unsigned short u16;
__xdata u8 External[4];
__xdata u8 Output[4];
__attribute__((section(".mcs51.xdata.simresult")))
volatile __xdata u8 Result;
extern u8 copy_select(u8, u16);
extern u8 copy_phi(u8, u16);
extern u8 move_selected(u8, u8, u16);
extern u8 copy_destination(u8, u16);

static u8 check_copy(u8 choice, u8 phi, u16 count) {
  u8 i;
  for (i = 0; i < 4; ++i) {
    External[i] = 21 + i;
    Output[i] = 0x55;
  }
  if (phi)
    copy_phi(choice, count);
  else
    copy_select(choice, count);
  for (i = 0; i < 4; ++i)
    if (Output[i] != (count ? (choice ? 11 + i : 21 + i) : 0x55))
      return 1;
  return 0;
}

int main(void) {
  u8 bad = 0;
  bad |= check_copy(0, 0, 4);
  bad |= check_copy(1, 0, 4);
  bad |= check_copy(0, 1, 4);
  bad |= check_copy(1, 1, 4);
  bad |= check_copy(0, 0, 0);
  bad |= check_copy(1, 1, 0);
  Output[0] = 31;
  Output[1] = 32;
  Output[2] = 33;
  Output[3] = 34;
  bad |= copy_destination(0, 4);
  bad |= copy_destination(1, 4);
  bad |= copy_destination(0, 0);
  bad |= copy_destination(1, 0);
  bad |= move_selected(0, 0, 3);
  bad |= move_selected(0, 1, 3);
  bad |= move_selected(1, 0, 3);
  bad |= move_selected(1, 1, 3);
  bad |= move_selected(0, 0, 0);
  bad |= move_selected(1, 1, 0);
  Result = bad;
  for (;;) {}
}
