//===-- MCS51.h - MCS-51 target interface -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MCS51_MCS51_H
#define LLVM_LIB_TARGET_MCS51_MCS51_H

namespace llvm {
namespace MCS51 {

/// LLVM IR address spaces used by the MCS-51 target. Address space zero is
/// the default near pointer; the others model the distinct 8051 memory buses.
enum AddressSpace {
  Default = 0,
  Data = 1,
  IData = 2,
  PData = 3,
  XData = 4,
  Code = 5,
  Bit = 6,
  SFR = 7,
};

} // namespace MCS51
} // namespace llvm

#endif
