; RUN: llc -O0 -mtriple=mcs51 -o - %s | FileCheck %s
; RUN: llc -O0 -mtriple=mcs51 -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS
; RUN: llvm-readobj --relocations %t.o | FileCheck %s --check-prefix=RELOC

@sfr_port = external addrspace(7) global i8
@data_byte = external addrspace(1) global i8
@sfr_word = external addrspace(7) global i16
@data_word = external addrspace(1) global i16
@xdata_byte = external addrspace(4) global i8
@xdata_word = external addrspace(4) global i16

define i8 @read_xdata(i8 %address) {
entry:
  %wide = zext i8 %address to i16
  %pointer = inttoptr i16 %wide to ptr addrspace(4)
  %value = load i8, ptr addrspace(4) %pointer, align 1
  ret i8 %value
}

define void @write_xdata(i8 %address, i8 %value) {
entry:
  %wide = zext i8 %address to i16
  %pointer = inttoptr i16 %wide to ptr addrspace(4)
  store i8 %value, ptr addrspace(4) %pointer, align 1
  ret void
}

define i8 @read_idata(i8 %address) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(2)
  %value = load i8, ptr addrspace(2) %pointer, align 1
  ret i8 %value
}

define void @write_idata(i8 %address, i8 %value) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(2)
  store i8 %value, ptr addrspace(2) %pointer, align 1
  ret void
}

define i16 @read_idata_word(i8 %address) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(2)
  %value = load i16, ptr addrspace(2) %pointer, align 1
  ret i16 %value
}

define void @write_idata_word(i8 %address) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(2)
  store i16 4660, ptr addrspace(2) %pointer, align 1
  ret void
}

define i8 @read_pdata(i8 %address) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(3)
  %value = load i8, ptr addrspace(3) %pointer, align 1
  ret i8 %value
}

define void @write_pdata(i8 %address, i8 %value) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(3)
  store i8 %value, ptr addrspace(3) %pointer, align 1
  ret void
}

define i16 @read_pdata_word(i8 %address) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(3)
  %value = load i16, ptr addrspace(3) %pointer, align 1
  ret i16 %value
}

define void @write_pdata_word(i8 %address) {
entry:
  %pointer = inttoptr i8 %address to ptr addrspace(3)
  store i16 4660, ptr addrspace(3) %pointer, align 1
  ret void
}

define i8 @read_code(i8 %address) {
entry:
  %wide = zext i8 %address to i16
  %pointer = inttoptr i16 %wide to ptr addrspace(5)
  %value = load i8, ptr addrspace(5) %pointer, align 1
  ret i8 %value
}

define i16 @read_xdata_word(i8 %address) {
entry:
  %wide = zext i8 %address to i16
  %pointer = inttoptr i16 %wide to ptr addrspace(4)
  %value = load i16, ptr addrspace(4) %pointer, align 1
  ret i16 %value
}

define i16 @read_code_word(i8 %address) {
entry:
  %wide = zext i8 %address to i16
  %pointer = inttoptr i16 %wide to ptr addrspace(5)
  %value = load i16, ptr addrspace(5) %pointer, align 1
  ret i16 %value
}

define i8 @read_sfr() {
entry:
  %value = load i8, ptr addrspace(7) @sfr_port, align 1
  ret i8 %value
}

define void @write_sfr(i8 %value) {
entry:
  store i8 %value, ptr addrspace(7) @sfr_port, align 1
  ret void
}

define i8 @read_direct_constant() {
entry:
  %pointer = inttoptr i8 144 to ptr addrspace(7)
  %value = load i8, ptr addrspace(7) %pointer, align 1
  ret i8 %value
}

define i8 @read_data_symbol() {
entry:
  %value = load i8, ptr addrspace(1) @data_byte, align 1
  ret i8 %value
}

define i16 @read_data_word() {
entry:
  %value = load i16, ptr addrspace(1) @data_word, align 1
  ret i16 %value
}

define void @write_sfr_word() {
entry:
  store i16 4660, ptr addrspace(7) @sfr_word, align 1
  ret void
}

define i8 @read_xdata_symbol() {
entry:
  %value = load i8, ptr addrspace(4) @xdata_byte, align 1
  ret i8 %value
}

define void @write_xdata_symbol(i8 %value) {
entry:
  store i8 %value, ptr addrspace(4) @xdata_byte, align 1
  ret void
}

define i16 @read_xdata_symbol_word() {
entry:
  %value = load i16, ptr addrspace(4) @xdata_word, align 1
  ret i16 %value
}

define void @write_xdata_symbol_word(i16 %value) {
entry:
  store i16 %value, ptr addrspace(4) @xdata_word, align 1
  ret void
}

; CHECK-LABEL: read_xdata:
; CHECK: mov 131, #0
; CHECK: mov a, r7
; CHECK: mov 130, a
; CHECK: movx a, @dptr
; CHECK: ret

; CHECK-LABEL: write_xdata:
; CHECK: mov 131, #0
; CHECK: mov a, r7
; CHECK: mov 130, a
; CHECK: mov a, r6
; CHECK: movx @dptr, a
; CHECK: ret

; CHECK-LABEL: read_idata:
; CHECK: mov a, @r{{[01]}}
; CHECK: ret

; CHECK-LABEL: write_idata:
; CHECK: mov a, r6
; CHECK: mov @r{{[01]}}, a
; CHECK: ret

; CHECK-LABEL: read_idata_word:
; CHECK: mov a, @r{{[01]}}
; CHECK: mov r{{[0-7]}}, a
; CHECK: mov a, r{{[01]}}
; CHECK: inc a
; CHECK: mov r{{[01]}}, a
; CHECK: mov a, @r{{[01]}}
; CHECK: mov 131, a
; CHECK: mov a, r{{[0-7]}}
; CHECK: mov 130, a
; CHECK: ret

; CHECK-LABEL: write_idata_word:
; CHECK: mov a, 130
; CHECK: mov r{{[0-7]}}, a
; CHECK: mov a, 131
; CHECK: mov r{{[0-7]}}, a
; CHECK: mov a, r{{[0-7]}}
; CHECK: mov @r{{[01]}}, a
; CHECK: mov a, r{{[01]}}
; CHECK: inc a
; CHECK: mov r{{[01]}}, a
; CHECK: mov a, r{{[0-7]}}
; CHECK: mov @r{{[01]}}, a
; CHECK: ret

; CHECK-LABEL: read_pdata:
; CHECK: movx a, @r{{[01]}}
; CHECK: ret

; CHECK-LABEL: write_pdata:
; CHECK: mov a, r6
; CHECK: movx @r{{[01]}}, a
; CHECK: ret

; CHECK-LABEL: read_pdata_word:
; CHECK: movx a, @r{{[01]}}
; CHECK: mov r{{[0-7]}}, a
; CHECK: mov a, r{{[01]}}
; CHECK: inc a
; CHECK: mov r{{[01]}}, a
; CHECK: movx a, @r{{[01]}}
; CHECK: mov 131, a
; CHECK: mov a, r{{[0-7]}}
; CHECK: mov 130, a
; CHECK: ret

; CHECK-LABEL: write_pdata_word:
; CHECK: mov a, 130
; CHECK: mov r{{[0-7]}}, a
; CHECK: mov a, 131
; CHECK: mov r{{[0-7]}}, a
; CHECK: mov a, r{{[0-7]}}
; CHECK: movx @r{{[01]}}, a
; CHECK: mov a, r{{[01]}}
; CHECK: inc a
; CHECK: mov r{{[01]}}, a
; CHECK: mov a, r{{[0-7]}}
; CHECK: movx @r{{[01]}}, a
; CHECK: ret

; CHECK-LABEL: read_code:
; CHECK: mov 131, #0
; CHECK: mov a, r7
; CHECK: mov 130, a
; CHECK: clr a
; CHECK: movc a, @a+dptr
; CHECK: ret

; CHECK-LABEL: read_xdata_word:
; CHECK: movx a, @dptr
; CHECK: mov r0, a
; CHECK: inc dptr
; CHECK: movx a, @dptr
; CHECK: mov 131, a
; CHECK: mov a, r0
; CHECK: mov 130, a
; CHECK: ret

; CHECK-LABEL: read_code_word:
; CHECK: movc a, @a+dptr
; CHECK: mov r0, a
; CHECK: inc dptr
; CHECK: movc a, @a+dptr
; CHECK: mov 131, a
; CHECK: mov a, r0
; CHECK: mov 130, a
; CHECK: ret

; CHECK-LABEL: read_sfr:
; CHECK: mov a, sfr_port
; CHECK: ret

; CHECK-LABEL: write_sfr:
; CHECK: mov a, r7
; CHECK: mov sfr_port, a
; CHECK: ret

; CHECK-LABEL: read_direct_constant:
; CHECK: mov a, -112
; CHECK: ret

; CHECK-LABEL: read_data_symbol:
; CHECK: mov a, data_byte
; CHECK: ret

; CHECK-LABEL: read_data_word:
; CHECK: mov a, data_word
; CHECK: mov r{{[0-7]}}, a
; CHECK: mov a, data_word+1
; CHECK: mov r{{[0-7]}}, a
; CHECK: mov a, r{{[0-7]}}
; CHECK: mov 131, a
; CHECK: mov a, r{{[0-7]}}
; CHECK: mov 130, a
; CHECK: ret

; CHECK-LABEL: write_sfr_word:
; CHECK: mov dptr, #4660
; CHECK: mov a, 130
; CHECK: mov r{{[0-7]}}, a
; CHECK: mov a, 131
; CHECK: mov r{{[0-7]}}, a
; CHECK: mov a, r{{[0-7]}}
; CHECK: mov sfr_word, a
; CHECK: mov a, r{{[0-7]}}
; CHECK: mov sfr_word+1, a
; CHECK: ret

; CHECK-LABEL: read_xdata_symbol:
; CHECK: mov dptr, #xdata_byte
; CHECK: movx a, @dptr
; CHECK: ret

; CHECK-LABEL: write_xdata_symbol:
; CHECK: mov dptr, #xdata_byte
; CHECK: movx @dptr, a
; CHECK: ret

; CHECK-LABEL: read_xdata_symbol_word:
; CHECK: mov dptr, #xdata_word
; CHECK: movx a, @dptr
; CHECK: inc dptr
; CHECK: movx a, @dptr
; CHECK: ret

; CHECK-LABEL: write_xdata_symbol_word:
; CHECK: mov dptr, #xdata_word
; CHECK: movx @dptr, a
; CHECK: inc dptr
; CHECK: movx @dptr, a
; CHECK: ret

; DIS-LABEL: <read_idata>:
; DIS: mov a, @r0
; DIS-LABEL: <write_idata>:
; DIS: mov @r0, a
; DIS-LABEL: <read_pdata>:
; DIS: movx a, @r0
; DIS-LABEL: <write_pdata>:
; DIS: movx @r0, a
; DIS-LABEL: <read_code>:
; DIS: movc a, @a+dptr
; DIS-LABEL: <read_xdata_word>:
; DIS: movx a, @dptr
; DIS: inc dptr
; DIS: movx a, @dptr
; DIS-LABEL: <read_code_word>:
; DIS: movc a, @a+dptr
; DIS: inc dptr
; DIS: movc a, @a+dptr
; DIS-LABEL: <read_direct_constant>:
; DIS: mov a, 144

; RELOC: R_8051_8 sfr_port 0x0
; RELOC: R_8051_8 data_byte 0x0
; RELOC: R_8051_8 data_word 0x0
; RELOC: R_8051_8 data_word 0x1
; RELOC: R_8051_8 sfr_word 0x0
; RELOC: R_8051_8 sfr_word 0x1
; RELOC: R_8051_16_BE xdata_byte 0x0
; RELOC: R_8051_16_BE xdata_byte 0x0
; RELOC: R_8051_16_BE xdata_word 0x0
; RELOC: R_8051_16_BE xdata_word 0x0
