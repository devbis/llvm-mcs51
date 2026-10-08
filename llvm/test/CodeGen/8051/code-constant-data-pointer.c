// Unqualified const objects use default XDATA. Explicit __code objects remain
// in CODE; converting their addresses to XDATA pointers uses a startup copy.
// RUN: clang -target mcs51 -mcpu=cc2530 -Oz -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -S %s -o - | FileCheck %s

void *memcpy(void *Destination, const void *Source, unsigned Size);

static const unsigned char Request[3] = {3, 8, 0};
static __code const unsigned char CodeRequest[3] = {4, 9, 1};
unsigned char Output[16];

unsigned char read_request(unsigned char Index) { return Request[Index]; }
unsigned char read_code_request(unsigned char Index) {
  return CodeRequest[Index];
}

void copy_request(void) { memcpy(Output, Request, sizeof(Request)); }
void copy_code_request(void) {
  memcpy(Output, (const void *)CodeRequest, sizeof(CodeRequest));
}

void copy_string(void) { memcpy(Output, "0123456789abcdef", 16); }

// CHECK-LABEL: read_request:
// CHECK: #xlo8(Request)
// CHECK: #xhi8(Request)
// CHECK: movx a, @dptr

// CHECK-LABEL: read_code_request:
// CHECK: #clo8(CodeRequest)
// CHECK: #chi8(CodeRequest)
// CHECK: movc a, @a+dptr

// CHECK-LABEL: copy_request:
// CHECK: #xlo8(Request)
// CHECK: lcall __mcs51_bankcall_memcpy

// CHECK-LABEL: copy_code_request:
// CHECK: #xlo8(CodeRequest.xdata)
// CHECK: lcall __mcs51_bankcall_memcpy

// CHECK-LABEL: copy_string:
// CHECK: #xlo8(.str.xdata)
// CHECK: lcall __mcs51_bankcall_memcpy

// CHECK: .section .mcs51.xdata.init.Request,"aw",@progbits
// CHECK-NEXT: Request:
// CHECK-NEXT: .asciz "\003\b"
// CHECK: .section .mcs51.code.rodata.CodeRequest,"a",@progbits
// CHECK-NEXT: CodeRequest:
// CHECK-NEXT: .ascii "\004\t\001"
// CHECK-DAG: .section .mcs51.xdata.init..str.xdata,"aw",@progbits
// CHECK-DAG: .asciz "0123456789abcdef"
// CHECK-DAG: .section .mcs51.xdata.init.CodeRequest.xdata,"aw",@progbits
// CHECK: CodeRequest.xdata:
// CHECK-NEXT: .ascii "\004\t\001"
// CHECK-NOT: .mcs51.code.rodata.Request
