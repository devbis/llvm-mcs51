//===- MCS51.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "InputSection.h"
#include "OutputSections.h"
#include "SymbolTable.h"
#include "Symbols.h"
#include "Target.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"
#include <cstring>

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
  bool relaxOnce(int pass) const override;
  void finalizeRelax(int passes) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
  void relocateAlloc(InputSection &sec, uint8_t *buf) const override;
};
} // namespace

namespace {
// Track DPTR only through a deliberately small set of instructions whose
// encodings cannot write DPL or DPH. Unknown instructions stop relaxation.
static bool preservesDPTR(ArrayRef<uint8_t> Bytes) {
  for (size_t I = 0; I < Bytes.size();) {
    uint8_t Op = Bytes[I];
    size_t Len = 1;
    if (Op == 0x74 || (Op >= 0x78 && Op <= 0x7f) || Op == 0x24 || Op == 0x34 ||
        Op == 0x44 || Op == 0x54 || Op == 0x64 || Op == 0x94 ||
        (Op >= 0x28 && Op <= 0x2f) || (Op >= 0x68 && Op <= 0x6f) ||
        (Op >= 0x98 && Op <= 0x9f) || (Op >= 0x08 && Op <= 0x0f) ||
        (Op >= 0x18 && Op <= 0x1f) || (Op >= 0xf8 && Op <= 0xff) ||
        Op == 0xe0 || Op == 0xf0 || Op == 0xa4 || Op == 0xc3 || Op == 0xe4 ||
        Op == 0x00) {
      if (Op == 0x74 || (Op >= 0x78 && Op <= 0x7f) || Op == 0x24 ||
          Op == 0x34 || Op == 0x44 || Op == 0x54 || Op == 0x64 || Op == 0x94)
        Len = 2;
    } else if (Op == 0x75) {
      if (I + 2 >= Bytes.size() || Bytes[I + 1] == 0x82 || Bytes[I + 1] == 0x83)
        return false;
      Len = 3;
    } else {
      return false;
    }
    if (I + Len > Bytes.size())
      return false;
    I += Len;
  }
  return true;
}

} // namespace

RelExpr MCS51::getRelExpr(RelType type, const Symbol &, const uint8_t *) const {
  switch (type) {
  case R_8051_NONE:
    return R_NONE;
  case R_8051_8:
  case R_8051_LO8:
  case R_8051_HI8:
  case R_8051_16:
  case R_8051_16_BE:
  case R_8051_DPTR16:
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

bool MCS51::relaxOnce(int Pass) const {
  SmallVector<InputSection *, 0> Storage;
  bool Changed = false;
  for (OutputSection *OSec : ctx.outputSections) {
    if (!(OSec->flags & SHF_EXECINSTR))
      continue;
    for (InputSection *Sec : getInputSections(*OSec, Storage)) {
      MutableArrayRef<Relocation> Relocs = Sec->relocs();
      if (Pass == 0) {
        bool HasDPTRReloc = llvm::any_of(Relocs, [](const Relocation &R) {
          return R.type == R_8051_DPTR16;
        });
        if (!HasDPTRReloc)
          continue;
        Sec->relaxAux = make<RelaxAux>();
        Sec->relaxAux->relocDeltas =
            std::make_unique<uint32_t[]>(Relocs.size());
        std::fill_n(Sec->relaxAux->relocDeltas.get(), Relocs.size(), 0);
        for (ELFFileBase *File : ctx.objectFiles)
          for (Symbol *Sym : File->getSymbols()) {
            auto *D = dyn_cast<Defined>(Sym);
            auto *Input =
                D ? dyn_cast_or_null<InputSection>(D->section) : nullptr;
            if (D && Input == Sec && (D->file == File || D->scriptDefined)) {
              Sec->relaxAux->anchors.push_back({D->value, D, false});
              Sec->relaxAux->anchors.push_back({D->value + D->size, D, true});
            }
          }
        llvm::sort(Sec->relaxAux->anchors,
                   [](const SymbolAnchor &A, const SymbolAnchor &B) {
                     return std::make_pair(A.offset, A.end) <
                            std::make_pair(B.offset, B.end);
                   });
      }
      if (!Sec->relaxAux)
        continue;

      RelaxAux &Aux = *Sec->relaxAux;
      uint32_t Delta = 0;
      uint64_t PreviousTarget = 0;
      uint64_t PreviousRelocOffset = 0;
      bool PreviousIsFunction = false;
      bool HavePrevious = false;
      bool CanTrackDPTR = true;
      ArrayRef<SymbolAnchor> Anchors = ArrayRef(Aux.anchors);
      for (auto [I, R] : llvm::enumerate(Relocs)) {
        uint32_t Remove = 0;
        if (R.type == R_8051_DPTR16) {
          if (R.offset == 0 || R.offset >= Sec->content().size() ||
              Sec->content().size() - R.offset < 2) {
            Err(ctx) << Sec->getLocation(R.offset)
                     << "invalid R_8051_DPTR16 relocation offset";
            CanTrackDPTR = false;
            HavePrevious = false;
          } else {
            uint64_t OpcodeOffset = R.offset - 1;
            uint64_t Target =
                Sec->getRelocTargetVA(ctx, R, Sec->getVA() + R.offset - Delta);
            auto *TargetDef = R.sym ? dyn_cast<Defined>(R.sym) : nullptr;
            auto *TargetSec =
                TargetDef ? dyn_cast_or_null<InputSection>(TargetDef->section)
                          : nullptr;
            bool IsDataAddress = TargetSec &&
                                 !(TargetSec->flags & SHF_EXECINSTR) &&
                                 !R.sym->isFunc() && !R.sym->isPreemptible;
            bool Safe = HavePrevious && OpcodeOffset >= PreviousRelocOffset + 2;
            if (Safe) {
              for (const Relocation &Other : Relocs) {
                if (Other.offset > PreviousRelocOffset + 1 &&
                    Other.offset < OpcodeOffset) {
                  Safe = false;
                  break;
                }
              }
            }
            if (Safe) {
              ArrayRef<uint8_t> Between(
                  Sec->content().data() + PreviousRelocOffset + 2,
                  OpcodeOffset - (PreviousRelocOffset + 2));
              Safe = preservesDPTR(Between);
            }
            if (Safe && CanTrackDPTR && IsDataAddress && !PreviousIsFunction &&
                Target == static_cast<uint16_t>(PreviousTarget + 1)) {
              Remove = 2;
            }
            if (HavePrevious && !Safe)
              CanTrackDPTR = false;
            if (CanTrackDPTR && (!HavePrevious || Safe)) {
              PreviousTarget = Target;
              PreviousRelocOffset = R.offset;
              PreviousIsFunction = !IsDataAddress;
              HavePrevious = IsDataAddress;
            } else {
              // A branch, call, unrecognized instruction, or another relocation
              // may bypass this load. Stop tracking for this input section so a
              // later basic block cannot inherit a stale DPTR value.
              HavePrevious = false;
            }
          }
        }
        for (; !Anchors.empty() && Anchors.front().offset <= R.offset;
             Anchors = Anchors.drop_front()) {
          const SymbolAnchor &A = Anchors.front();
          if (A.end)
            A.d->size = A.offset - Delta - A.d->value;
          else
            A.d->value = A.offset - Delta;
        }
        Delta += Remove;
        if (Aux.relocDeltas[I] != Delta) {
          Aux.relocDeltas[I] = Delta;
          Changed = true;
        }
      }
      for (const SymbolAnchor &A : Anchors) {
        if (A.end)
          A.d->size = A.offset - Delta - A.d->value;
        else
          A.d->value = A.offset - Delta;
      }
    }
  }
  return Changed;
}

void MCS51::finalizeRelax(int Passes) const {
  SmallVector<InputSection *, 0> Storage;
  for (OutputSection *OSec : ctx.outputSections) {
    if (!(OSec->flags & SHF_EXECINSTR))
      continue;
    for (InputSection *Sec : getInputSections(*OSec, Storage)) {
      if (!Sec->relaxAux || !Sec->relaxAux->relocDeltas)
        continue;
      RelaxAux &Aux = *Sec->relaxAux;
      MutableArrayRef<Relocation> Relocs = Sec->relocs();
      ArrayRef<uint8_t> Old = Sec->content();
      uint32_t TotalRemoved =
          Relocs.empty() ? 0 : Aux.relocDeltas[Relocs.size() - 1];
      if (!TotalRemoved)
        continue;
      uint8_t *New = ctx.bAlloc.Allocate<uint8_t>(Old.size() - TotalRemoved);
      size_t Src = 0, Dst = 0;
      uint32_t PreviousDelta = 0;
      for (size_t I = 0; I < Relocs.size(); ++I) {
        Relocation &R = Relocs[I];
        uint32_t Delta = Aux.relocDeltas[I];
        uint32_t RemovedHere = Delta - PreviousDelta;
        PreviousDelta = Delta;
        if (!RemovedHere)
          continue;
        size_t OpcodeOffset = R.offset - 1;
        size_t CopySize = OpcodeOffset - Src;
        std::memcpy(New + Dst, Old.data() + Src, CopySize);
        Dst += CopySize;
        New[Dst++] = 0xa3; // INC DPTR replaces MOV DPTR,#symbol.
        Src = R.offset + 2;
        R.type = R_8051_NONE;
      }
      std::memcpy(New + Dst, Old.data() + Src, Old.size() - Src);
      Sec->content_ = New;
      Sec->size = Old.size() - TotalRemoved;
      Sec->bytesDropped = TotalRemoved;

      uint32_t Delta = 0;
      for (size_t I = 0; I < Relocs.size(); ++I) {
        Relocation &R = Relocs[I];
        R.offset -= Delta;
        Delta = Aux.relocDeltas[I];
      }
    }
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
  case R_8051_LO8:
    *loc = static_cast<uint8_t>(val);
    break;
  case R_8051_HI8:
    *loc = static_cast<uint8_t>(val >> 8);
    break;
  case R_8051_16:
    checkUInt(ctx, loc, val, 16, rel);
    write16le(loc, val);
    break;
  case R_8051_16_BE:
  case R_8051_DPTR16:
    checkUInt(ctx, loc, val, 16, rel);
    loc[0] = static_cast<uint8_t>(val >> 8);
    loc[1] = static_cast<uint8_t>(val);
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
      if (rel.type == R_8051_11 && targetBank != callerBank)
        Err(ctx) << "MCS-51 AJMP/ACALL cannot cross from bank " << callerBank
                 << " to bank " << targetBank << " for function '"
                 << rel.sym->getName() << "'";
      bool isLongCall = rel.type == R_8051_16_BE &&
                        (sec.flags & SHF_EXECINSTR) && rel.offset > 0 &&
                        sec.content()[rel.offset - 1] == 0x12;
      bool isFunctionAddress =
          (rel.type == R_8051_16 || rel.type == R_8051_DPTR16) && !isLongCall;
      bool needsThunk =
          targetBank && (targetBank != callerBank || isFunctionAddress);
      if (needsThunk && !isBankThunk && !isAutoBankThunk) {
        // Some declarations in another translation unit do not carry a
        // section attribute. Resolve those calls and function pointers here,
        // after bank placement has made the target bank known.
        std::string thunkName =
            (Twine("__mcs51_bankcall_") + rel.sym->getName()).str();
        auto *thunk = dyn_cast_or_null<Defined>(ctx.symtab->find(thunkName));
        if (thunk)
          val = thunk->getVA(ctx, rel.addend);
        else
          Err(ctx) << "MCS-51 reference to banked function '"
                   << rel.sym->getName() << "' from section '" << sec.name
                   << "' has no bank-call trampoline";
      }
    }
    if (isAutoBankThunk && rel.type == R_8051_8) {
      // The auto-bank trampoline names its callee in this byte field. Resolve
      // the bank after the linker has assigned the function input section.
      *loc = targetBank;
      continue;
    }
    if (rel.type == R_8051_11) {
      checkUInt(ctx, loc, val, 16, rel);
      if ((val & ~uint64_t(0x7ff)) != ((place + 2) & ~uint64_t(0x7ff)))
        Err(ctx)
            << "MCS-51 AJMP/ACALL target is outside the current 2 KiB page";
      loc[0] = (loc[0] & 0x1f) | ((val >> 3) & 0xe0);
      loc[1] = static_cast<uint8_t>(val);
      continue;
    }
    relocate(loc, rel, val);
  }
}

void elf::setMCS51TargetInfo(Ctx &ctx) { ctx.target.reset(new MCS51(ctx)); }
