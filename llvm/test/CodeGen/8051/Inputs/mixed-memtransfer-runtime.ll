target triple = "mcs51"

@External = external addrspace(4) global [4 x i8]
@Output = external addrspace(4) global [4 x i8]
declare void @llvm.memcpy.p4.p0.i16(ptr addrspace(4), ptr, i16, i1 immarg)
declare void @llvm.memmove.p0.p0.i16(ptr, ptr, i16, i1 immarg)

define i8 @copy_select(i8 %choose, i16 %count) {
entry:
  %local = alloca [4 x i8]
  %l1 = getelementptr i8, ptr %local, i16 1
  %l2 = getelementptr i8, ptr %local, i16 2
  %l3 = getelementptr i8, ptr %local, i16 3
  store i8 11, ptr %local
  store i8 12, ptr %l1
  store i8 13, ptr %l2
  store i8 14, ptr %l3
  %external = addrspacecast ptr addrspace(4) @External to ptr
  %local_selected = icmp ne i8 %choose, 0
  %src = select i1 %local_selected, ptr %local, ptr %external
  call void @llvm.memcpy.p4.p0.i16(ptr addrspace(4) @Output, ptr %src, i16 %count, i1 false)
  ret i8 0
}

define i8 @copy_phi(i8 %choose, i16 %count) {
entry:
  %local = alloca [4 x i8]
  %l1 = getelementptr i8, ptr %local, i16 1
  %l2 = getelementptr i8, ptr %local, i16 2
  %l3 = getelementptr i8, ptr %local, i16 3
  store i8 11, ptr %local
  store i8 12, ptr %l1
  store i8 13, ptr %l2
  store i8 14, ptr %l3
  %external = addrspacecast ptr addrspace(4) @External to ptr
  %local_selected = icmp ne i8 %choose, 0
  br i1 %local_selected, label %stack, label %xdata
stack:
  br label %join
xdata:
  br label %join
join:
  %src = phi ptr [ %local, %stack ], [ %external, %xdata ]
  call void @llvm.memcpy.p4.p0.i16(ptr addrspace(4) @Output, ptr %src, i16 %count, i1 false)
  ret i8 0
}

define i8 @move_selected(i8 %choose, i8 %backward, i16 %count) {
entry:
  %local = alloca [4 x i8]
  %l1 = getelementptr i8, ptr %local, i16 1
  %l2 = getelementptr i8, ptr %local, i16 2
  %l3 = getelementptr i8, ptr %local, i16 3
  store i8 1, ptr %local
  store i8 2, ptr %l1
  store i8 3, ptr %l2
  store i8 4, ptr %l3
  %e1 = getelementptr i8, ptr addrspace(4) @External, i16 1
  %e2 = getelementptr i8, ptr addrspace(4) @External, i16 2
  %e3 = getelementptr i8, ptr addrspace(4) @External, i16 3
  store i8 1, ptr addrspace(4) @External
  store i8 2, ptr addrspace(4) %e1
  store i8 3, ptr addrspace(4) %e2
  store i8 4, ptr addrspace(4) %e3
  %external = addrspacecast ptr addrspace(4) @External to ptr
  %local_selected = icmp ne i8 %choose, 0
  %base = select i1 %local_selected, ptr %local, ptr %external
  %next = getelementptr i8, ptr %base, i16 1
  %back = icmp ne i8 %backward, 0
  %src = select i1 %back, ptr %base, ptr %next
  %dst = select i1 %back, ptr %next, ptr %base
  call void @llvm.memmove.p0.p0.i16(ptr %dst, ptr %src, i16 %count, i1 false)
  %local0 = load volatile i8, ptr %local
  %local1 = load volatile i8, ptr %l1
  %local2 = load volatile i8, ptr %l2
  %local3 = load volatile i8, ptr %l3
  %ext0 = load volatile i8, ptr addrspace(4) @External
  %ext1 = load volatile i8, ptr addrspace(4) %e1
  %ext2 = load volatile i8, ptr addrspace(4) %e2
  %ext3 = load volatile i8, ptr addrspace(4) %e3
  %v0 = select i1 %local_selected, i8 %local0, i8 %ext0
  %v1 = select i1 %local_selected, i8 %local1, i8 %ext1
  %v2 = select i1 %local_selected, i8 %local2, i8 %ext2
  %v3 = select i1 %local_selected, i8 %local3, i8 %ext3
  %zero = icmp eq i16 %count, 0
  %m0 = select i1 %back, i8 1, i8 2
  %m1 = select i1 %back, i8 1, i8 3
  %m2 = select i1 %back, i8 2, i8 4
  %m3 = select i1 %back, i8 3, i8 4
  %want0 = select i1 %zero, i8 1, i8 %m0
  %want1 = select i1 %zero, i8 2, i8 %m1
  %want2 = select i1 %zero, i8 3, i8 %m2
  %want3 = select i1 %zero, i8 4, i8 %m3
  %c0 = icmp ne i8 %v0, %want0
  %c1 = icmp ne i8 %v1, %want1
  %c2 = icmp ne i8 %v2, %want2
  %c3 = icmp ne i8 %v3, %want3
  %a = or i1 %c0, %c1
  %b = or i1 %c2, %c3
  %bad = or i1 %a, %b
  %result = zext i1 %bad to i8
  ret i8 %result
}

declare void @llvm.memcpy.p0.p4.i16(ptr, ptr addrspace(4), i16, i1 immarg)

define i8 @copy_destination(i8 %choose, i16 %count) {
entry:
  %local = alloca [4 x i8]
  %l1 = getelementptr i8, ptr %local, i16 1
  %l2 = getelementptr i8, ptr %local, i16 2
  %l3 = getelementptr i8, ptr %local, i16 3
  store i8 1, ptr %local
  store i8 2, ptr %l1
  store i8 3, ptr %l2
  store i8 4, ptr %l3
  %e1 = getelementptr i8, ptr addrspace(4) @External, i16 1
  %e2 = getelementptr i8, ptr addrspace(4) @External, i16 2
  %e3 = getelementptr i8, ptr addrspace(4) @External, i16 3
  store i8 1, ptr addrspace(4) @External
  store i8 2, ptr addrspace(4) %e1
  store i8 3, ptr addrspace(4) %e2
  store i8 4, ptr addrspace(4) %e3
  %external = addrspacecast ptr addrspace(4) @External to ptr
  %local_selected = icmp ne i8 %choose, 0
  %dst = select i1 %local_selected, ptr %local, ptr %external
  call void @llvm.memcpy.p0.p4.i16(ptr %dst, ptr addrspace(4) @Output, i16 %count, i1 false)
  %local0 = load volatile i8, ptr %local
  %local1 = load volatile i8, ptr %l1
  %local2 = load volatile i8, ptr %l2
  %local3 = load volatile i8, ptr %l3
  %ext0 = load volatile i8, ptr addrspace(4) @External
  %ext1 = load volatile i8, ptr addrspace(4) %e1
  %ext2 = load volatile i8, ptr addrspace(4) %e2
  %ext3 = load volatile i8, ptr addrspace(4) %e3
  %v0 = select i1 %local_selected, i8 %local0, i8 %ext0
  %v1 = select i1 %local_selected, i8 %local1, i8 %ext1
  %v2 = select i1 %local_selected, i8 %local2, i8 %ext2
  %v3 = select i1 %local_selected, i8 %local3, i8 %ext3
  %zero = icmp eq i16 %count, 0
  %want0 = select i1 %zero, i8 1, i8 31
  %want1 = select i1 %zero, i8 2, i8 32
  %want2 = select i1 %zero, i8 3, i8 33
  %want3 = select i1 %zero, i8 4, i8 34
  %c0 = icmp ne i8 %v0, %want0
  %c1 = icmp ne i8 %v1, %want1
  %c2 = icmp ne i8 %v2, %want2
  %c3 = icmp ne i8 %v3, %want3
  %a = or i1 %c0, %c1
  %b = or i1 %c2, %c3
  %bad = or i1 %a, %b
  %result = zext i1 %bad to i8
  ret i8 %result
}
