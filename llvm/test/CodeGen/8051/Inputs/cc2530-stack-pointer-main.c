typedef unsigned char u8;
typedef unsigned short u16;
#define RESULT ((volatile __xdata u8 *)0x1e00)
extern u8 read_byte(const u8 *p);
extern void write_byte(u8 *p, u8 v);
extern u8 *identity(u8 *p);
extern u8 *idata_to_default(__idata u8 *p);
extern u8 *data_to_default(__data u8 *p);
u8 * volatile Saved;
volatile u8 Choice;
volatile u8 Index;
u8 External[2];
__idata u8 IDataValue;
__data u8 DataValue;
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
  u8 bad = 0;
  Saved = pointer;
  if (read_byte(Saved) != 0x42) bad |= 1;
  if (read_byte(identity(pointer)) != 0x42) bad |= 2;
  External[0] = 0x33;
  Choice = 1;
  u8 *selected = Choice ? pointer : External;
  if (read_byte(selected) != 0x42) bad |= 4;
  Choice = 0;
  selected = Choice ? pointer : External;
  if (read_byte(selected) != 0x33) bad |= 8;
  Index = 1;
  if (read_byte(pointer + Index) != 0x69) bad |= 16;
  if (idata_to_default((__idata u8 *)0) != (u8 *)0) bad |= 32;
  if (read_byte(idata_to_default((__idata u8 *)local)) != 0x42) bad |= 64;
  IDataValue = 0x2f;
  Choice = 1;
  __idata u8 *idata_selected = Choice ? (__idata u8 *)local : &IDataValue;
  if (read_byte((u8 *)idata_selected) != 0x42) bad |= 128;
  Choice = 0;
  idata_selected = Choice ? (__idata u8 *)local : &IDataValue;
  if (read_byte((u8 *)idata_selected) != 0x2f) bad |= 128;
  DataValue = 0x39;
  if (data_to_default((__data u8 *)0) != (u8 *)0) bad |= 128;
  if (read_byte(data_to_default(&DataValue)) != 0x39) bad |= 128;
  RESULT[3] = bad;
  RESULT[0] = 0xa5;
  for (;;) {}
}
