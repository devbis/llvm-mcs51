//===- MCS51.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Target.h"
#include "InputSection.h"
#include "OutputSections.h"
#include "Symbols.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::ELF;
using namespace llvm::support::endian;
using namespace lld;
using namespace lld::elf;

namespace {
static unsigned getMCS51CodeBank(const OutputSection *section) {
  if (!section || !section->name.starts_with(".bank"))
    return 0;
  StringRef name = section->name.drop_front(5);
  unsigned bank = 0;
  if (name.getAsInteger(10, bank) || bank < 1 || bank > 7)
    return 0;
  return bank;
}

class MCS51 final : public TargetInfo {
public:
  MCS51(Ctx &ctx) : TargetInfo(ctx) { defaultImageBase = 0; }
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
  void relocateAlloc(InputSection &sec, uint8_t *buf) const override;
};
} // namespace

RelExpr MCS51::getRelExpr(RelType type, const Symbol &, const uint8_t *) const {
  switch (type) {
  case R_8051_NONE:
    return R_NONE;
  case R_8051_8:
  case R_8051_16:
    return R_ABS;
  case R_8051_PCREL8:
    return R_PC;
  case R_8051_11:
    return R_ABS;
  default:
    Err(ctx) << "unsupported MCS-51 relocation type " << type;
    return R_NONE;
  }
}

void MCS51::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_8051_NONE:
    break;
  case R_8051_8:
    checkUInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  case R_8051_16:
    checkUInt(ctx, loc, val, 16, rel);
    write16le(loc, val);
    break;
  case R_8051_PCREL8:
    checkInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  case R_8051_11:
    // Allocatable sections use relocateAlloc to provide the final place.
    checkUInt(ctx, loc, val, 16, rel);
    loc[0] = (loc[0] & 0x1f) | ((val >> 3) & 0xe0);
    loc[1] = static_cast<uint8_t>(val);
    break;
  default:
    Err(ctx) << "unrecognized MCS-51 relocation type " << rel.type;
  }
}

void MCS51::relocateAlloc(InputSection &sec, uint8_t *buf) const {
  uint64_t secAddr = sec.getOutputSection()->addr + sec.outSecOff;
  for (const Relocation &rel : sec.relocs()) {
    uint8_t *loc = buf + rel.offset;
    uint64_t place = secAddr + rel.offset;
    uint64_t val = sec.getRelocTargetVA(ctx, rel, place);
    bool isBankThunk = sec.name.starts_with(".text.bankthunks.");
    bool isAutoBankThunk = sec.name.starts_with(".text.autobankthunks.");
    unsigned targetBank = 0;
    if (rel.sym && rel.sym->isFunc()) {
      auto *target = dyn_cast<Defined>(rel.sym);
      auto *targetInput =
          target ? dyn_cast<InputSection>(target->section) : nullptr;
      targetBank =
          targetInput ? getMCS51CodeBank(targetInput->getOutputSection()) : 0;
      unsigned callerBank = getMCS51CodeBank(sec.getOutputSection());
      // Codegen emits calls through these common-area stubs. Any other
      // cross-bank relocation would use the shared 0x8000 VMA without setting
      // FMAP, which silently calls whichever bank is currently selected.
      if ((rel.type == R_8051_16 || rel.type == R_8051_11) && targetBank &&
          targetBank != callerBank && !isBankThunk && !isAutoBankThunk)
        Err(ctx) << "MCS-51 reference to banked function '"
                 << rel.sym->getName()
                 << "' must use its bank-call trampoline; declare the same "
                    "banked section on the function declaration";
    }
    if (isAutoBankThunk && rel.type == R_8051_8 && targetBank) {
      // The auto-bank trampoline names its callee in this byte field. Resolve
      // the bank after the linker has assigned the function input section.
      *loc = targetBank;
      continue;
    }
    if (rel.type == R_8051_11) {
      checkUInt(ctx, loc, val, 16, rel);
      if ((val & ~uint64_t(0x7ff)) != ((place + 2) & ~uint64_t(0x7ff)))
        Err(ctx) << "MCS-51 AJMP/ACALL target is outside the current 2 KiB page";
      loc[0] = (loc[0] & 0x1f) | ((val >> 3) & 0xe0);
      loc[1] = static_cast<uint8_t>(val);
      continue;
    }
    relocate(loc, rel, val);
  }
}

void elf::setMCS51TargetInfo(Ctx &ctx) { ctx.target.reset(new MCS51(ctx)); }
