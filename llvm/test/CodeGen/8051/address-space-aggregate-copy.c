// RUN: clang -target mcs51 -mcpu=cc2530 -O0 -mllvm -verify-machineinstrs \
// RUN:   -S %s -o - | FileCheck %s

typedef struct {
  unsigned char First;
  unsigned char Second;
} Pair;

__data Pair DataSource;
__data Pair DataDestination;
__pdata Pair PDataSource;
__pdata Pair PDataDestination;

void copy_data_to_data(void) { DataDestination = DataSource; }
void copy_pdata_to_pdata(void) { PDataDestination = PDataSource; }

// CHECK-LABEL: copy_data_to_data:
// CHECK: mov {{[0-9]+}}, DataSource
// CHECK: mov {{[0-9]+}}, DataSource+1
// CHECK: mov DataDestination, {{[0-9]+}}
// CHECK: mov DataDestination+1, {{[0-9]+}}
// CHECK-NOT: lcall memcpy

// CHECK-LABEL: copy_pdata_to_pdata:
// CHECK: mov r0, #PDataSource
// CHECK: movx a, @r0
// CHECK: mov [[LO:[0-9]+]], a
// CHECK: inc r0
// CHECK: movx a, @r0
// CHECK: mov [[HI:[0-9]+]], a
// CHECK: mov a, [[LO]]
// CHECK: mov r0, #PDataDestination
// CHECK: movx @r0, a
// CHECK: inc r0
// CHECK: mov a, [[HI]]
// CHECK: movx @r0, a
// CHECK-NOT: lcall memcpy
