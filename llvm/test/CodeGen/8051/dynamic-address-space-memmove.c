// RUN: clang -target mcs51 -O0 -S -mllvm -verify-machineinstrs %s -o - | FileCheck %s

void move_xdata(__xdata unsigned char *Destination,
                __xdata const unsigned char *Source, unsigned int Count) {
  __builtin_memmove(Destination, Source, Count);
}

void move_code_to_xdata(__xdata unsigned char *Destination,
                        __code const unsigned char *Source,
                        unsigned int Count) {
  __builtin_memmove(Destination, Source, Count);
}

void move_idata(__idata unsigned char *Destination,
                __idata const unsigned char *Source, unsigned int Count) {
  __builtin_memmove(Destination, Source, Count);
}

void move_data_idata(__data unsigned char *Destination,
                     __idata const unsigned char *Source,
                     unsigned int Count) {
  __builtin_memmove(Destination, Source, Count);
}

void move_stack(unsigned int Count) {
  unsigned char Local[8] = {0};
  __builtin_memmove(&Local[1], Local,
                    Count < sizeof(Local) - 1 ? Count : sizeof(Local) - 1);
}

void move_stack_to_idata(__idata unsigned char *Destination,
                         unsigned int Count) {
  unsigned char Local[8] = {0};
  __builtin_memmove(Destination, &Local[1],
                    Count < sizeof(Local) - 1 ? Count : sizeof(Local) - 1);
}

typedef __generic unsigned char *GenericBytePointer;
void move_generic(GenericBytePointer Destination, GenericBytePointer Source,
                  unsigned int Count) {
  __builtin_memmove(Destination, Source, Count);
}

void move_generic_xdata(GenericBytePointer Destination,
                        __xdata const unsigned char *Source,
                        unsigned int Count) {
  __builtin_memmove(Destination, Source, Count);
}

// CHECK-LABEL: move_xdata:
// CHECK: movx a, @dptr
// CHECK: movx @dptr, a
// CHECK-NOT: lcall memmove
// CHECK-LABEL: move_code_to_xdata:
// CHECK: movc a, @a+dptr
// CHECK: movx @dptr, a
// CHECK-NOT: lcall memmove
// CHECK-LABEL: move_idata:
// CHECK: mov a, @r0
// CHECK: mov @r0, a
// CHECK-NOT: lcall memmove
// CHECK-LABEL: move_data_idata:
// CHECK: mov a, @r0
// CHECK: mov @r0, a
// CHECK-NOT: lcall memmove
// CHECK-LABEL: move_stack:
// CHECK: mov a, @r0
// CHECK: mov @r0, a
// CHECK-NOT: lcall memmove
// CHECK-LABEL: move_stack_to_idata:
// CHECK: mov a, @r0
// CHECK-NOT: lcall memmove
// CHECK-LABEL: move_generic:
// CHECK: lcall __mcs51_gptrget8
// CHECK: lcall __mcs51_gptrput8
// CHECK-NOT: lcall memmove
// CHECK-LABEL: move_generic_xdata:
// CHECK: movx a, @dptr
// CHECK: lcall __mcs51_gptrput8
// CHECK-NOT: lcall memmove
