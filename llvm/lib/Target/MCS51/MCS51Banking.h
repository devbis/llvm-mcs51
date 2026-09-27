//===-- MCS51Banking.h - MCS-51 code bank helpers --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MCS51_MCS51BANKING_H
#define LLVM_LIB_TARGET_MCS51_MCS51BANKING_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include <string>

namespace llvm {

/// Return the CC2530 flash bank encoded by a section named .bankN.*.
/// Bank zero is reserved for the common/root image and means "not banked".
inline unsigned getMCS51CodeBank(StringRef Section) {
  if (!Section.starts_with(".bank"))
    return 0;
  Section = Section.drop_front(5);
  size_t Dot = Section.find('.');
  if (Dot == StringRef::npos)
    return 0;
  unsigned Bank = 0;
  if (Section.take_front(Dot).getAsInteger(10, Bank) || Bank < 1 || Bank > 7)
    return 0;
  return Bank;
}

inline std::string getMCS51BankThunkName(StringRef FunctionName) {
  return (Twine("__mcs51_bankcall_") + FunctionName).str();
}

} // namespace llvm

#endif
