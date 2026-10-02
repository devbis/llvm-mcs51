; RUN: llc -mtriple=mcs51 -mcpu=cc2530 -O0 -verify-machineinstrs \
; RUN:   -filetype=asm %s -o - | FileCheck %s

target datalayout = "e-p:16:8-p1:8:8-p2:8:8-p3:8:8-p4:16:8-p5:16:8-p6:8:8-p7:8:8-p8:32:8-A2-i1:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8:16"
target triple = "mcs51"

@flags = addrspace(4) global [6 x i8] zeroinitializer
@padding = addrspace(4) global i8 0

define i8 @branch_islands() {
entry:
  %f0 = load volatile i8, ptr addrspace(4) getelementptr inbounds ([6 x i8], ptr addrspace(4) @flags, i16 0, i16 0), align 1
  %c0 = icmp ne i8 %f0, 0
  br i1 %c0, label %failure, label %check1

check1:
  %f1 = load volatile i8, ptr addrspace(4) getelementptr inbounds ([6 x i8], ptr addrspace(4) @flags, i16 0, i16 1), align 1
  %c1 = icmp ne i8 %f1, 0
  br i1 %c1, label %failure, label %check2

check2:
  %f2 = load volatile i8, ptr addrspace(4) getelementptr inbounds ([6 x i8], ptr addrspace(4) @flags, i16 0, i16 2), align 1
  %c2 = icmp ne i8 %f2, 0
  br i1 %c2, label %failure, label %check3

check3:
  %f3 = load volatile i8, ptr addrspace(4) getelementptr inbounds ([6 x i8], ptr addrspace(4) @flags, i16 0, i16 3), align 1
  %c3 = icmp ne i8 %f3, 0
  br i1 %c3, label %failure, label %check4

check4:
  %f4 = load volatile i8, ptr addrspace(4) getelementptr inbounds ([6 x i8], ptr addrspace(4) @flags, i16 0, i16 4), align 1
  %c4 = icmp ne i8 %f4, 0
  br i1 %c4, label %failure, label %check5

check5:
  %f5 = load volatile i8, ptr addrspace(4) getelementptr inbounds ([6 x i8], ptr addrspace(4) @flags, i16 0, i16 5), align 1
  %c5 = icmp ne i8 %f5, 0
  br i1 %c5, label %failure, label %padding

padding:
  %p0 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p1 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p2 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p3 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p4 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p5 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p6 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p7 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p8 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p9 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p10 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p11 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p12 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p13 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p14 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p15 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p16 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p17 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p18 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p19 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p20 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p21 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p22 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p23 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p24 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p25 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p26 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p27 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p28 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p29 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p30 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p31 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p32 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p33 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p34 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p35 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p36 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p37 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p38 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p39 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p40 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p41 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p42 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p43 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p44 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p45 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p46 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p47 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p48 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p49 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p50 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p51 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p52 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p53 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p54 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p55 = load volatile i8, ptr addrspace(4) @padding, align 1
  %p56 = load volatile i8, ptr addrspace(4) @padding, align 1
  br label %success

success:
  ret i8 0

failure:
  ret i8 1
}

; CHECK-LABEL: branch_islands:
; CHECK-COUNT-1: ljmp
; CHECK-NOT: ljmp
; CHECK: ret
