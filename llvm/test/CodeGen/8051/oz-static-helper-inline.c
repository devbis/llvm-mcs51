// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -S -emit-llvm %s -o - | FileCheck %s

static unsigned char helper(const unsigned char *data, unsigned short length) {
  unsigned char sum = 0;
  if (length & 0x0001) sum += data[0];
  if (length & 0x0002) sum += data[1];
  if (length & 0x0004) sum += data[2];
  if (length & 0x0008) sum += data[3];
  if (length & 0x0010) sum += data[4];
  if (length & 0x0020) sum += data[5];
  if (length & 0x0040) sum += data[6];
  if (length & 0x0080) sum += data[7];
  if (length & 0x0100) sum += data[8];
  if (length & 0x0200) sum += data[9];
  if (length & 0x0400) sum += data[10];
  if (length & 0x0800) sum += data[11];
  if (length & 0x1000) sum += data[12];
  if (length & 0x2000) sum += data[13];
  if (length & 0x4000) sum += data[14];
  if (length & 0x8000) sum += data[15];
  return sum;
}

static unsigned char tiny(unsigned short value) { return value + 1; }

unsigned char caller(const unsigned char *data, unsigned short length) {
  return helper(data, length) + tiny(length);
}

// Keep a larger last-call-to-static helper out of the caller, while still
// inlining a small static helper under -Oz.
// CHECK-LABEL: define dso_local{{.*}} @caller(
// CHECK-NOT: call {{.*}}@tiny(
// CHECK: call {{.*}} @helper(
// CHECK-NOT: call {{.*}}@tiny(
// CHECK: ret
