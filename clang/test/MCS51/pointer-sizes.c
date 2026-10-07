// RUN: %clang_cc1 -triple mcs51 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple mcs51 -O1 -disable-llvm-passes -emit-llvm %s -o - \
// RUN:   | FileCheck %s

// Clang's pointer widths must match the MCS-51 DataLayout
// (p1/p2/p3:8, p0/p4/p5:16, p8:32); a 4-byte generic pointer is
// {address low, address high, tag, 0}.
// expected-no-diagnostics

typedef unsigned char u8;

_Static_assert(sizeof(u8 __data *) == 1, "");
_Static_assert(sizeof(u8 __idata *) == 1, "");
_Static_assert(sizeof(u8 __pdata *) == 1, "");
_Static_assert(sizeof(u8 __xdata *) == 2, "");
_Static_assert(sizeof(u8 __code *) == 2, "");
_Static_assert(sizeof(u8 *) == 2, "");
_Static_assert(sizeof(u8 __generic *) == 4, "");
_Static_assert(_Alignof(u8 __generic *) == 1, "");
_Static_assert(_Alignof(u8 __data *) == 1, "");

struct S {
  u8 tag;
  u8 __generic *p;
  u8 __data *d;
  u8 __xdata *x;
};
_Static_assert(__builtin_offsetof(struct S, p) == 1, "");
_Static_assert(__builtin_offsetof(struct S, d) == 5, "");
_Static_assert(__builtin_offsetof(struct S, x) == 6, "");
_Static_assert(sizeof(struct S) == 8, "");

typedef u8 __generic *gp;
_Static_assert(sizeof(gp[3]) == 12, "");

// CHECK: target datalayout = "{{.*}}-p1:8:8-p2:8:8-p3:8:8-p4:16:8-p5:16:8-{{.*}}-p8:32:8-
// CHECK: %struct.S = type { i8, ptr addrspace(8), ptr addrspace(1), ptr addrspace(4) }
struct S s;

// Indexing an array of generic pointers scales by 4 bytes.
// CHECK-LABEL: @nth(
// CHECK: getelementptr inbounds{{.*}} [3 x ptr addrspace(8)], ptr addrspace(4) {{.*}}, i16 0, i16 %
// CHECK: load ptr addrspace(8), ptr addrspace(4) {{.*}}, align 1
__xdata gp table[3];
gp nth(u8 i) { return table[i]; }

// CHECK-LABEL: @diff(
// CHECK: ptrtoint ptr addrspace(4) %{{.*}} to i16
// CHECK: sdiv exact i16 %{{.*}}, 4
int diff(gp __xdata *a, gp __xdata *b) { return a - b; }

// CHECK-LABEL: define{{.*}} ptr addrspace(8) @pass(ptr addrspace(8)
gp pass(gp p) { return p; }
