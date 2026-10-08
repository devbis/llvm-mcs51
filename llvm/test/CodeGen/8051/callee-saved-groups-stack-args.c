// RUN: clang -target mcs51 -mcpu=generic -Oz -ffreestanding -S %s -o - | FileCheck %s

typedef unsigned char u8;

extern u8 crc8(u8, u8);
extern void put(u8);
extern void debug_begin(void);
extern void debug_hex(unsigned int, u8);
extern void debug_end(void);

static __xdata u8 tx[73];

// The fifth and later byte arguments are stack-passed. Under register pressure,
// the callee-save list can interleave R5 (added to complete the R2..R7 helper
// group) after the IM registers. Restore helpers must still follow the actual
// order in which the grouped save helpers pushed their values.
void mixed_groups(u8 seq, u8 flags, u8 status, u8 cmd, u8 type, u8 len,
                  const __xdata u8 *payload) {
  u8 crc;
  u8 i;
  u8 n = 0;
  if (len > 64) len = 64;
  tx[n++] = 0x66;
  tx[n++] = 0xbb;
  tx[n++] = seq;
  tx[n++] = flags;
  tx[n++] = status;
  tx[n++] = cmd;
  tx[n++] = len;
  if (len) tx[n++] = type;
  for (i = 0; i < len; ++i) tx[n++] = payload[i];
  crc = 0;
  for (i = 0; i < n; ++i) crc = crc8(crc, tx[i]);
  tx[n++] = crc;
  for (i = 0; i < n; ++i) put(tx[i]);
  debug_begin();
  for (i = 0; i < n; ++i) debug_hex(tx[i], 2);
  debug_end();
}

// CHECK-LABEL: mixed_groups:
// CHECK: lcall __mcs51_save_r6
// CHECK-NEXT: lcall __mcs51_save_im4
// CHECK: lcall __mcs51_restore_im4
// CHECK-NEXT: ljmp __mcs51_return_r6
