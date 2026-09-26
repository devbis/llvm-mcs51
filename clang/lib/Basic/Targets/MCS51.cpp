//===--- MCS51.cpp - MCS-51 target feature support ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCS51.h"
#include "clang/Basic/MacroBuilder.h"

using namespace clang;
using namespace clang::targets;

void MCS51TargetInfo::getTargetDefines(const LangOptions &,
                                      MacroBuilder &Builder) const {
  Builder.defineMacro("__mcs51__");
  Builder.defineMacro("__8051__");
  Builder.defineMacro("__data", "__attribute__((address_space(1)))");
  Builder.defineMacro("__idata", "__attribute__((address_space(2)))");
  Builder.defineMacro("__pdata", "__attribute__((address_space(3)))");
  Builder.defineMacro("__xdata", "__attribute__((address_space(4)))");
  Builder.defineMacro("__code", "__attribute__((address_space(5)))");
  Builder.defineMacro("__bit", "__attribute__((address_space(6)))");
  Builder.defineMacro("__sbit", "__attribute__((address_space(6)))");
  Builder.defineMacro("__sfr", "__attribute__((address_space(7)))");
  if (IsCC2530)
    Builder.defineMacro("__CC2530__");
}
