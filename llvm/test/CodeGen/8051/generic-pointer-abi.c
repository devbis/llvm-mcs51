// RUN: clang -target mcs51 -S -emit-llvm %s -o - | FileCheck %s
// RUN: clang -target mcs51 -S -O0 -mllvm -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=MC

typedef __generic unsigned char *generic_byte_ptr;

generic_byte_ptr preserve_generic_pointer(generic_byte_ptr Pointer) {
  return Pointer;
}

// CHECK: target datalayout = {{.*}}p8:32:8{{.*}}
// CHECK: define{{.*}} ptr addrspace(8) @preserve_generic_pointer(ptr addrspace(8)
// MC-LABEL: preserve_generic_pointer:
// MC: ret
