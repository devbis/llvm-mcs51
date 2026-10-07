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
#include "llvm/IR/Function.h"
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

/// Whether a function section requests link-time CC2530 bank assignment.
inline bool isMCS51AutoBankSection(StringRef Section) {
  return Section.starts_with(".mcs51.autobank.");
}

/// Whether a function is placed by the linker into a CC2530 code bank. Such
/// functions are always entered through their bank-call trampoline.
inline bool isMCS51AutoBankFunction(const Function &F, StringRef CPU,
                                    bool FunctionSections) {
  if (F.hasSection() && isMCS51AutoBankSection(F.getSection()))
    return true;
  // Runtime helpers and startup support must stay in common flash so calls
  // from any bank can reach them directly without another bank-call thunk.
  if (F.getName().starts_with("__mcs51_"))
    return false;
  if (!CPU.equals_insensitive("cc2530") || !FunctionSections ||
      F.getName() == "main" || F.hasFnAttribute("interrupt"))
    return false;
  if (!F.hasSection())
    return true;
  StringRef Section = F.getSection();
  return Section.starts_with(".text.") && Section != ".text.main" &&
         !Section.starts_with(".text.main.") &&
         !Section.starts_with(".text.startup") &&
         !Section.starts_with(".text.bankthunks.") &&
         !Section.starts_with(".text.autobankthunks.");
}

/// Whether a function lives in a code bank, either an explicit .bankN.*
/// section or a linker-assigned auto-bank section.
inline bool isMCS51BankedFunction(const Function &F, StringRef CPU,
                                  bool FunctionSections) {
  return (F.hasSection() && getMCS51CodeBank(F.getSection())) ||
         isMCS51AutoBankFunction(F, CPU, FunctionSections);
}

/// Bytes a bank-call trampoline leaves between the caller's stack arguments
/// and the callee's return address: the saved FMAP and the trampoline's own
/// LCALL return address.
constexpr unsigned MCS51BankThunkFrameBytes = 3;

/// Distance from SP at entry to the lowest-addressed incoming stack argument
/// byte. Every banked function with stack arguments is entered through its
/// trampoline (see LowerCall and the bank-call redirection in the
/// AsmPrinter), so its arguments sit below the trampoline frame as well as
/// the return address.
inline unsigned getMCS51IncomingStackArgBias(const Function &F, StringRef CPU,
                                             bool FunctionSections) {
  return 2 + (isMCS51BankedFunction(F, CPU, FunctionSections)
                  ? MCS51BankThunkFrameBytes
                  : 0);
}

inline std::string getMCS51BankThunkName(StringRef FunctionName) {
  return (Twine("__mcs51_bankcall_") + FunctionName).str();
}

} // namespace llvm

#endif
