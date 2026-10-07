; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -O2 -verify-machineinstrs -o - %s | FileCheck %s

; The address of a word add that only feeds a memory access is built in front
; of that access. DPTR reuse must not count increments across pseudos that are
; expanded later: here the load of p[19] and the select sit between the store
; to p[14] and the store to p[16].

; CHECK-LABEL: store_after_load_and_select:
; CHECK:         add a, #14
; CHECK:         movx @dptr, a
; CHECK-COUNT-5: inc dptr
; CHECK-NEXT:    movx a, @dptr
; CHECK:         cjne
; CHECK:       .LBB0_3:
; CHECK-NOT:     inc dptr
; CHECK:         add a, #16
; CHECK-NEXT:    mov 130, a
; CHECK-NEXT:    clr a
; CHECK-NEXT:    addc a,
; CHECK-NEXT:    mov 131, a
; CHECK-NOT:     inc dptr
; CHECK:         movx @dptr, a
; CHECK-NEXT:    ret
define void @store_after_load_and_select(ptr %p, i8 %ch) {
entry:
  %a14 = getelementptr inbounds i8, ptr %p, i16 14
  store i8 %ch, ptr %a14, align 1
  %a19 = getelementptr inbounds i8, ptr %p, i16 19
  %k = load i8, ptr %a19, align 1
  %is3 = icmp eq i8 %k, 3
  %z = zext i1 %is3 to i8
  %a16 = getelementptr inbounds i8, ptr %p, i16 16
  store i8 %z, ptr %a16, align 1
  ret void
}

; Accesses expanded in order still step DPTR instead of reloading it.
; CHECK-LABEL: ascending_stores:
; CHECK:         add a, #14
; CHECK:         mov a, r7
; CHECK-NEXT:    movx @dptr, a
; CHECK-NEXT:    inc dptr
; CHECK-NEXT:    mov a, r6
; CHECK-NEXT:    movx @dptr, a
; CHECK-NEXT:    inc dptr
; CHECK-NEXT:    inc dptr
; CHECK-NEXT:    mov a, r5
; CHECK-NEXT:    movx @dptr, a
; CHECK-NEXT:    ret
define void @ascending_stores(ptr %p, i8 %a, i8 %b, i8 %c) {
entry:
  %a14 = getelementptr inbounds i8, ptr %p, i16 14
  store volatile i8 %a, ptr %a14, align 1
  %a15 = getelementptr inbounds i8, ptr %p, i16 15
  store volatile i8 %b, ptr %a15, align 1
  %a17 = getelementptr inbounds i8, ptr %p, i16 17
  store volatile i8 %c, ptr %a17, align 1
  ret void
}
