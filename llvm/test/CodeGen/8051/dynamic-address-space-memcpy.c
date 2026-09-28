// RUN: clang -target mcs51 -O0 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s

void copy_code_to_xdata(__xdata unsigned char *Destination,
                        __code const unsigned char *Source,
                        unsigned int Count) {
  __builtin_memcpy(Destination, Source, Count);
}

void copy_xdata_to_idata(__idata unsigned char *Destination,
                         __xdata const unsigned char *Source,
                         unsigned int Count) {
  __builtin_memcpy(Destination, Source, Count);
}

void copy_default(unsigned char *Destination, const unsigned char *Source,
                  unsigned int Count) {
  __builtin_memcpy(Destination, Source, Count);
}

void copy_stack_to_xdata(__xdata unsigned char *Destination,
                         unsigned int Count) {
  unsigned char Local[8] = {0};
  __builtin_memcpy(Destination, &Local[2], Count < sizeof(Local) - 2
                                                ? Count
                                                : sizeof(Local) - 2);
}

typedef __generic unsigned char *GenericBytePointer;
void copy_generic(GenericBytePointer Destination, GenericBytePointer Source,
                  unsigned int Count) {
  __builtin_memcpy(Destination, Source, Count);
}

// CHECK-LABEL: copy_code_to_xdata:
// CHECK: movc a, @a+dptr
// CHECK: movx @dptr, a
// CHECK-NOT: lcall memcpy
// CHECK-LABEL: copy_xdata_to_idata:
// CHECK: movx a, @dptr
// CHECK: mov @r0, a
// CHECK-NOT: lcall memcpy
// CHECK-LABEL: copy_default:
// CHECK: movx a, @dptr
// CHECK: movx @dptr, a
// CHECK-NOT: lcall memcpy
// CHECK-LABEL: copy_stack_to_xdata:
// CHECK: mov a, @r0
// CHECK: movx @dptr, a
// CHECK-NOT: lcall memcpy
// CHECK-LABEL: copy_generic:
// CHECK: lcall __mcs51_gptrget8
// CHECK: lcall __mcs51_gptrput8
// CHECK-NOT: lcall memcpy
