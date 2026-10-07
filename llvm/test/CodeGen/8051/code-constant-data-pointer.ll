; A CODE object whose address becomes a default/XDATA pointer is read through
; an initialized XDATA copy. Direct CODE reads and generic pointers keep using
; the CODE object.
; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -data-sections -o - %s | FileCheck %s

declare void @sink(ptr)
declare void @sink_generic(ptr addrspace(8))

@table = internal addrspace(5) constant [4 x i8] c"\01\02\03\04", align 1
@key = private unnamed_addr addrspace(5) constant [3 x i8] c"abc", align 1
@generic_only = internal addrspace(5) constant [2 x i8] c"\05\06", align 1
@names = addrspace(5) constant [1 x ptr] [ptr addrspacecast (ptr addrspace(5) getelementptr (i8, ptr addrspace(5) @key, i16 1) to ptr)], align 1
@xdata_constant = addrspace(4) constant [2 x i8] c"\07\08", align 1
@code_writable = addrspace(5) global [2 x i8] c"\09\0a", align 1

; CHECK-LABEL: direct_code_read:
; CHECK: mov 50, #lo8(table)
; CHECK: movc a, @a+dptr
define i8 @direct_code_read(i16 %i) {
  %p = getelementptr i8, ptr addrspace(5) @table, i16 %i
  %v = load i8, ptr addrspace(5) %p
  ret i8 %v
}

; CHECK-LABEL: escaped_constant:
; CHECK: mov 48, #lo8(table.xdata+2)
; CHECK-NEXT: mov 49, #hi8(table.xdata+2)
; CHECK-NEXT: lcall __mcs51_bankcall_sink
define void @escaped_constant() {
  call void @sink(ptr addrspacecast (ptr addrspace(5) getelementptr (i8, ptr addrspace(5) @table, i16 2) to ptr))
  ret void
}

; CHECK-LABEL: escaped_variable_index:
; CHECK: mov 50, #lo8(table.xdata)
; CHECK-NEXT: mov 51, #hi8(table.xdata)
; CHECK: lcall __mcs51_bankcall_sink
define void @escaped_variable_index(i16 %i) {
  %p = getelementptr i8, ptr addrspace(5) @table, i16 %i
  %d = addrspacecast ptr addrspace(5) %p to ptr
  call void @sink(ptr %d)
  ret void
}

; Element reads through the converted address stay in CODE.
; CHECK-LABEL: read_through_cast:
; CHECK: mov 50, #lo8(table)
; CHECK: movc a, @a+dptr
; CHECK-NEXT: add a, #4
; CHECK-NOT: movx
; CHECK: ret
define i8 @read_through_cast(i16 %i) {
  %p = getelementptr i8, ptr addrspacecast (ptr addrspace(5) @table to ptr), i16 %i
  %v = load i8, ptr %p
  %w = load i8, ptr getelementptr (i8, ptr addrspacecast (ptr addrspace(5) @table to ptr), i16 3)
  %r = add i8 %v, %w
  ret i8 %r
}

; A read through a phi cannot be attributed to CODE and uses the copy.
; CHECK-LABEL: read_through_phi:
; CHECK: table.xdata
; CHECK: movx a, @dptr
define i8 @read_through_phi(i1 %c, ptr %other) {
entry:
  br i1 %c, label %code, label %join
code:
  br label %join
join:
  %p = phi ptr [ addrspacecast (ptr addrspace(5) @table to ptr), %code ], [ %other, %entry ]
  %v = load i8, ptr %p
  ret i8 %v
}

; CHECK-LABEL: generic_pointer:
; CHECK: mov 48, #lo8(generic_only)
; CHECK-NEXT: mov 49, #hi8(generic_only)
; CHECK-NEXT: mov 50, #128
; CHECK: lcall __mcs51_bankcall_sink_generic
define void @generic_pointer() {
  call void @sink_generic(ptr addrspace(8) addrspacecast (ptr addrspace(5) @generic_only to ptr addrspace(8)))
  ret void
}

; CHECK-LABEL: read_xdata_constant:
; CHECK: movx a, @dptr
define i8 @read_xdata_constant(i16 %i) {
  %p = getelementptr i8, ptr addrspace(4) @xdata_constant, i16 %i
  %v = load i8, ptr addrspace(4) %p
  ret i8 %v
}

; CHECK-LABEL: read_code_writable:
; CHECK: movc a, @a+dptr
define i8 @read_code_writable(i16 %i) {
  %p = getelementptr i8, ptr addrspace(5) @code_writable, i16 %i
  %v = load i8, ptr addrspace(5) %p
  ret i8 %v
}

; CHECK: .section .rodata.table,"a",@progbits
; CHECK: .section .rodata.generic_only,"a",@progbits
; CHECK: .section .rodata.names,"a",@progbits
; CHECK-LABEL: names:
; CHECK-NEXT: .short key.xdata+1
; CHECK: .section .data.xdata_constant,"aw",@progbits
; CHECK: .section .rodata.code_writable,"a",@progbits
; CHECK: .section .data.key.xdata,"aw",@progbits
; CHECK: key.xdata:
; CHECK-NEXT: .ascii "abc"
; CHECK: .section .data.table.xdata,"aw",@progbits
; CHECK: table.xdata:
; CHECK-NEXT: .ascii "\001\002\003\004"
; CHECK-NOT: generic_only.xdata
