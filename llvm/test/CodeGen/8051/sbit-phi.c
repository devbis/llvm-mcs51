// RUN: clang -target mcs51 -Oz -c %s -o %t.o
// RUN: clang -target mcs51 -O2 -c %s -o %t.o
// RUN: clang -target mcs51 -Oz -S %s -o - | FileCheck %s
// CHECK-LABEL: scan:
// CHECK-DAG: mov c, P36
// CHECK-DAG: mov c, P37
// CHECK-DAG: mov c, P47
// CHECK-LABEL: scan_two:
// CHECK-DAG: mov c, P36
// CHECK-DAG: mov c, P37
// CHECK-NOT: switch.table.scan:

extern volatile __sbit unsigned char P36, P37, P47;
unsigned char scan(unsigned char i) {
  if (i == 0) return !P36;
  if (i == 1) return !P37;
  if (i == 2) return !P47;
  return 0;
}

unsigned char scan_two(unsigned char i) {
  if (i == 0) return !P36;
  if (i == 1) return !P37;
  return 0;
}
