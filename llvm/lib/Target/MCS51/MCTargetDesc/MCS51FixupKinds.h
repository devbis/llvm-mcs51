//===-- MCS51FixupKinds.h - MCS-51 fixup kinds ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MCS51_FIXUP_KINDS_H
#define LLVM_MCS51_FIXUP_KINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace MCS51 {

enum Fixups {
  fixup_8 = FirstTargetFixupKind,
  fixup_16,
  fixup_16_be,
  fixup_dptr16,
  fixup_pcrel8,
  fixup_11,
  fixup_lo8,
  fixup_hi8,
  NumTargetFixupKinds
};

} // namespace MCS51
} // namespace llvm

#endif
