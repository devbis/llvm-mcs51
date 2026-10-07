// Const objects without a memory qualifier live in CODE. When their address is
// passed as an ordinary (XDATA) pointer, the callee must see the object's
// bytes, so the pointer refers to an initialized XDATA copy.
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -S %s -o - | FileCheck %s

void *memcpy(void *Destination, const void *Source, unsigned Size);

static const unsigned char Request[3] = {3, 8, 0};
unsigned char Output[16];

unsigned char read_request(unsigned char Index) { return Request[Index]; }

void copy_request(void) { memcpy(Output, Request, sizeof(Request)); }

void copy_string(void) { memcpy(Output, "0123456789abcdef", 16); }

// CHECK-LABEL: read_request:
// CHECK: #lo8(Request)
// CHECK: movc a, @a+dptr

// CHECK-LABEL: copy_request:
// CHECK: #lo8(Request.xdata)
// CHECK: lcall __mcs51_bankcall_memcpy

// CHECK-LABEL: copy_string:
// CHECK: #lo8(.str.xdata)
// CHECK: lcall __mcs51_bankcall_memcpy

// CHECK: .section .rodata.Request,"a",@progbits
// CHECK-NEXT: Request:
// CHECK-NEXT: .asciz "\003\b"
// CHECK-DAG: .section .data..str.xdata,"aw",@progbits
// CHECK-DAG: .asciz "0123456789abcdef"
// CHECK-DAG: .section .data.Request.xdata,"aw",@progbits
// CHECK-NOT: .rodata.str
