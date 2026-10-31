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

enum class MCS51AddressSpace { Unknown, Code, XData };

static MCS51AddressSpace getMCS51AddressSpace(StringRef name) {
  if (name.starts_with(".mcs51.code.") || name == ".rodata" ||
      name.starts_with(".rodata.") || name.starts_with(".bank"))
    return MCS51AddressSpace::Code;
  if (name.starts_with(".mcs51.xdata.") || name == ".data" ||
      name.starts_with(".data.") || name == ".bss" ||
      name.starts_with(".bss."))
    return MCS51AddressSpace::XData;
  return MCS51AddressSpace::Unknown;
}

static StringRef getMCS51AddressSpaceName(MCS51AddressSpace space) {
  switch (space) {
  case MCS51AddressSpace::Code:
    return "CODE";
  case MCS51AddressSpace::XData:
    return "XDATA";
  case MCS51AddressSpace::Unknown:
    return "unknown";
  }
  llvm_unreachable("invalid MCS51 address space");
}

static bool isMCS51XDataInitSection(StringRef name) {
  return name == ".mcs51.xdata.init" ||
         name.starts_with(".mcs51.xdata.init.");
}

static MCS51AddressSpace getMCS51AccessSpace(RelType type) {
  switch (type) {
  case R_8051_DPTR16_XDATA:
  case R_8051_LO8_XDATA:
  case R_8051_HI8_XDATA:
    return MCS51AddressSpace::XData;
  case R_8051_DPTR16_CODE:
  case R_8051_LO8_CODE:
  case R_8051_HI8_CODE:
    return MCS51AddressSpace::Code;
  default:
    return MCS51AddressSpace::Unknown;
  }
}

// A call or function pointer to a declaration is emitted against a weak
// external bank-call trampoline because the callee's bank is unknown at
// compile time. If the callee was placed in common flash, return it so the
// reference can bypass the trampoline. Common functions are already entered
// directly under any FMAP value (same-unit calls, function pointers), so this
// keeps a single entry protocol for them; it also matters for the ABI, since
// direct entry places stack arguments right below the return address.
static const Defined *getCommonTargetOfExternalBankThunk(Ctx &ctx,
                                                        const Symbol &sym) {
  constexpr StringRef prefix = "__mcs51_bankcall_";
  auto *thunk = dyn_cast<Defined>(&sym);
  if (!thunk || !sym.getName().starts_with(prefix))
    return nullptr;
  auto *thunkSec = dyn_cast_or_null<InputSection>(thunk->section);
  if (!thunkSec ||
      !thunkSec->name.starts_with(".text.autobankthunks.external."))
    return nullptr;
  auto *target = dyn_cast_or_null<Defined>(
      ctx.symtab->find(sym.getName().drop_front(prefix.size())));
  if (!target || !target->isFunc())
    return nullptr;
  auto *targetSec = dyn_cast_or_null<InputSection>(target->section);
  if (!targetSec || !(targetSec->flags & SHF_EXECINSTR) ||
      getMCS51CodeBank(targetSec->getOutputSection()))
    return nullptr;
  return target;
}

static Defined *findMCS51DefinedSymbol(Ctx &ctx, InputFile *file,
                                       StringRef name) {
  if (file && file->kind() == InputFile::ObjKind)
    for (Symbol *symbol : file->getSymbols())
      if (symbol->getName() == name)
        if (auto *defined = dyn_cast<Defined>(symbol))
          return defined;
  if (Symbol *symbol = ctx.symtab->find(name))
    return dyn_cast<Defined>(symbol);
  return nullptr;
}

static Defined *findMCS51BankCallTarget(Ctx &ctx, const Defined &thunk) {
  constexpr StringLiteral Prefix("__mcs51_bankcall_");
  if (!thunk.getName().starts_with(Prefix))
    return nullptr;
  return findMCS51DefinedSymbol(
      ctx, thunk.file, thunk.getName().drop_front(Prefix.size()));
}

static Defined *findMCS51BankCallThunk(Ctx &ctx, const Defined &function) {
  std::string name = (Twine("__mcs51_bankcall_") + function.getName()).str();
  return findMCS51DefinedSymbol(ctx, function.file, name);
}

static bool getMCS51SectionTargetOffset(const Relocation &R, InputSection *Sec,
                                        uint64_t &TargetOffset) {
  if (!R.sym || !R.sym->isSection())
    return false;
  auto *Target = dyn_cast<Defined>(R.sym);
  auto *TargetInput =
      Target ? dyn_cast_or_null<InputSection>(Target->section) : nullptr;
  if (TargetInput != Sec || Target->value > Sec->size)
    return false;
  int64_t Base = Target->value;
  if (R.addend < -Base || R.addend > static_cast<int64_t>(Sec->size) - Base)
    return false;
  TargetOffset = Base + R.addend;
  return true;
}

static uint32_t getMCS51RemovedBefore(ArrayRef<Relocation> Relocs,
                                      const uint32_t *RelocDeltas,
                                      uint64_t TargetOffset) {
  auto It = llvm::lower_bound(
      Relocs, TargetOffset,
      [](const Relocation &R, uint64_t Offset) { return R.offset < Offset; });
  size_t Index = std::distance(Relocs.begin(), It);
  return Index ? RelocDeltas[Index - 1] : 0;
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
  case R_8051_RELAX:
    return R_RELAX_HINT;
  case R_8051_8:
  case R_8051_LO8:
  case R_8051_HI8:
  case R_8051_LO8_XDATA:
  case R_8051_HI8_XDATA:
  case R_8051_LO8_CODE:
  case R_8051_HI8_CODE:
  case R_8051_BIT8:
  case R_8051_BANK8:
  case R_8051_16:
  case R_8051_16_BE:
  case R_8051_DPTR16:
  case R_8051_DPTR16_XDATA:
  case R_8051_DPTR16_CODE:
  case R_8051_24:
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
  if (!ctx.arg.relax)
    return false;
  SmallVector<InputSection *, 0> Storage;
  bool Changed = false;
  for (OutputSection *OSec : ctx.outputSections) {
    if (!(OSec->flags & SHF_EXECINSTR))
      continue;
    for (InputSection *Sec : getInputSections(*OSec, Storage)) {
      // Converted IAR code keeps its conditional and short branches as
      // assembler-resolved bytes without relocations, so shrinking a call in
      // such a section would leave those branches off target.
      if (Sec->name.starts_with(".mcs51.autobank.iar_") ||
          Sec->name.starts_with(".text.__mcs51_iarcode_"))
        continue;
      MutableArrayRef<Relocation> Relocs = Sec->relocs();
      auto isRelaxableCodeTransfer = [&](const Relocation &R) {
        if (R.type != R_8051_16_BE || !R.sym || R.offset == 0 ||
            R.offset >= Sec->content().size())
          return false;
        // Relocation may bypass this trampoline and redirect directly to its
        // callee after bank placement. Until that final target is known, the
        // trampoline's page is not a safe basis for shortening the transfer.
        if (R.sym->getName().starts_with("__mcs51_bankcall_"))
          return false;
        uint8_t Opcode = Sec->content()[R.offset - 1];
        if (Opcode != 0x12 && Opcode != 0x02) // LCALL or LJMP
          return false;
        auto *Target = dyn_cast<Defined>(R.sym);
        auto *TargetInput =
            Target ? dyn_cast_or_null<InputSection>(Target->section) : nullptr;
        return TargetInput && (TargetInput->flags & SHF_EXECINSTR);
      };
      if (Pass == 0) {
        bool HasRelaxable = llvm::any_of(Relocs, [&](const Relocation &R) {
          if (R.type == R_8051_RELAX || R.type == R_8051_DPTR16 ||
              R.type == R_8051_DPTR16_XDATA ||
              R.type == R_8051_DPTR16_CODE)
            return true;
          return isRelaxableCodeTransfer(R);
        });
        if (!HasRelaxable)
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
        if (R.type == R_8051_DPTR16 || R.type == R_8051_DPTR16_XDATA ||
            R.type == R_8051_DPTR16_CODE) {
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
            // Only an assembler that kept every reference spanning this
            // instruction symbolic marks it relaxable; deleting bytes under
            // an assembler-resolved branch would retarget that branch.
            bool Relaxable = I + 1 < Relocs.size() &&
                             Relocs[I + 1].type == R_8051_RELAX &&
                             Relocs[I + 1].offset == R.offset;
            if (Safe && Relaxable && CanTrackDPTR && IsDataAddress &&
                !PreviousIsFunction &&
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
        } else if (isRelaxableCodeTransfer(R)) {
          // Direct transfers within a bank can use the two-byte ACALL/AJMP
          // encoding when the destination is in the same 2 KiB page. This
          // also covers local basic-block jumps, whose relocations name their
          // input section rather than a function symbol. Removing one byte
          // lets the normal address assignment loop account for the shorter
          // instruction.
          auto *target = dyn_cast<Defined>(R.sym);
          auto *targetInput =
              target ? dyn_cast_or_null<InputSection>(target->section)
                     : nullptr;
          unsigned targetBank =
              targetInput ? getMCS51CodeBank(targetInput->getOutputSection())
                          : 0;
          unsigned callerBank = getMCS51CodeBank(Sec->getOutputSection());
          // R.offset points to the high byte after the opcode. The shortened
          // instruction is two bytes long, so its next PC is one byte past
          // that relocation field (opcode + 2), not two bytes past it.
          uint64_t opcodeVA = Sec->getVA() + R.offset - 1 - Delta;
          uint64_t destinationVA =
              Sec->getRelocTargetVA(ctx, R, opcodeVA);
          uint64_t targetOffset;
          if (getMCS51SectionTargetOffset(R, Sec, targetOffset))
            destinationVA -= getMCS51RemovedBefore(
                Relocs, Aux.relocDeltas.get(), targetOffset);
          uint64_t nextPC = opcodeVA + 2;
          if (targetInput && targetBank == callerBank &&
              (destinationVA & ~uint64_t(0x7ff)) ==
                  (nextPC & ~uint64_t(0x7ff)))
            Remove = 1;
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

      // Section-symbol relocations encode the target's original section offset
      // in the addend. Keep those targets pointed at the same byte after code
      // has been shortened.
      for (Relocation &R : Relocs) {
        uint64_t TargetOffset;
        if (getMCS51SectionTargetOffset(R, Sec, TargetOffset))
          R.addend -= getMCS51RemovedBefore(Relocs, Aux.relocDeltas.get(),
                                            TargetOffset);
      }

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
        // Keep the input offset: R.offset is rewritten below for an 11-bit
        // relocation, while Src must continue to index the original bytes.
        size_t OriginalRelocOffset = R.offset;
        size_t OpcodeOffset = OriginalRelocOffset - 1;
        size_t CopySize = OpcodeOffset - Src;
        std::memcpy(New + Dst, Old.data() + Src, CopySize);
        Dst += CopySize;
        if (RemovedHere == 1) {
          // Replace LCALL/LJMP (opcode, high address, low address) with
          // ACALL/AJMP (opcode, low address). R.offset already points at the
          // low byte in the shortened instruction after earlier removals.
          New[Dst++] = Old[OpcodeOffset] == 0x12 ? 0x11 : 0x01;
          New[Dst++] = Old[OriginalRelocOffset + 1];
          R.type = R_8051_11;
          R.offset = OpcodeOffset;
        } else {
          New[Dst++] = 0xa3; // INC DPTR replaces MOV DPTR,#symbol.
          Src = OriginalRelocOffset + 2;
          R.type = R_8051_NONE;
          if (I + 1 < Relocs.size() && Relocs[I + 1].type == R_8051_RELAX)
            Relocs[I + 1].type = R_8051_NONE;
        }
        if (RemovedHere == 1)
          Src = OriginalRelocOffset + 2;
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
  case R_8051_RELAX:
    break;
  case R_8051_8:
    checkUInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  case R_8051_LO8:
    *loc = static_cast<uint8_t>(val);
    break;
  case R_8051_HI8:
  case R_8051_HI8_XDATA:
  case R_8051_HI8_CODE:
    *loc = static_cast<uint8_t>(val >> 8);
    break;
  case R_8051_LO8_XDATA:
  case R_8051_LO8_CODE:
    *loc = static_cast<uint8_t>(val);
    break;
  case R_8051_BIT8: {
    // The IAR virtual-bit register is specified as a byte address in the
    // bit-addressable DATA area. Its bit address wraps after multiplying the
    // byte address by eight; the RELA addend is already a bit offset.
    uint64_t Base = val - rel.addend;
    uint64_t BitAddress = (Base * 8 + rel.addend) & 0xff;
    checkUInt(ctx, loc, BitAddress, 7, rel);
    *loc = static_cast<uint8_t>(BitAddress);
    break;
  }
  case R_8051_BANK8:
    checkUInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  case R_8051_24:
    checkUInt(ctx, loc, val, 24, rel);
    loc[0] = static_cast<uint8_t>(val);
    loc[1] = static_cast<uint8_t>(val >> 8);
    loc[2] = static_cast<uint8_t>(val >> 16);
    break;
  case R_8051_16:
    checkUInt(ctx, loc, val, 16, rel);
    write16le(loc, val);
    break;
  case R_8051_16_BE:
  case R_8051_DPTR16:
  case R_8051_DPTR16_XDATA:
  case R_8051_DPTR16_CODE:
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
  if (isMCS51XDataInitSection(sec.name)) {
    auto hasDefinition = [&](StringRef name) {
      return dyn_cast_or_null<Defined>(ctx.symtab->find(name)) != nullptr;
    };
    if (sec.getOutputSection()->name != ".data" ||
        !hasDefinition("__mcs51_data_start") ||
        !hasDefinition("__mcs51_data_end") ||
        !hasDefinition("__mcs51_data_load"))
      Err(ctx) << sec.getLocation(0)
               << "initialized MCS-51 XDATA section '" << sec.name
               << "' has no startup copy path; define __mcs51_data_start, "
                  "__mcs51_data_end, and __mcs51_data_load in the linker "
                  "script and copy the CODE load image to XDATA before main";
  }
  uint64_t secAddr = sec.getOutputSection()->addr + sec.outSecOff;
  for (const Relocation &rel : sec.relocs()) {
    uint8_t *loc = buf + rel.offset;
    uint64_t place = secAddr + rel.offset;
    uint64_t val = sec.getRelocTargetVA(ctx, rel, place);
    MCS51AddressSpace sourceSpace = getMCS51AccessSpace(rel.type);
    if (rel.sym && sourceSpace != MCS51AddressSpace::Unknown) {
      auto *target = dyn_cast<Defined>(rel.sym);
      auto *targetSec = target
                            ? dyn_cast_or_null<InputSection>(target->section)
                            : nullptr;
      MCS51AddressSpace targetSpace =
          targetSec ? getMCS51AddressSpace(targetSec->name)
                    : MCS51AddressSpace::Unknown;
      if (sourceSpace != targetSpace) {
        StringRef targetName = rel.sym->getName();
        if (targetName.empty() && targetSec && rel.addend >= 0) {
          uint64_t targetOffset = rel.addend;
          if (auto *sectionSymbol = dyn_cast<Defined>(rel.sym))
            targetOffset += sectionSymbol->value;
          if (Defined *enclosing = targetSec->getEnclosingSymbol(targetOffset))
            targetName = enclosing->getName();
        }
        if (targetName.empty())
          targetName = "<section symbol>";
        Err(ctx) << sec.getLocation(rel.offset)
                 << "MCS-51 " << getMCS51AddressSpaceName(sourceSpace)
                 << " access from section '" << sec.name << "' targets symbol '"
                 << targetName << "' in section '"
                 << (targetSec ? targetSec->name : StringRef("<absolute>"))
                 << "' (" << getMCS51AddressSpaceName(targetSpace)
                 << "); CODE and XDATA are separate address spaces";
      }
    }
    bool isBankThunk = sec.name.starts_with(".text.bankthunks.");
    bool isAutoBankThunk = sec.name.starts_with(".text.autobankthunks.");
    if (rel.sym && rel.type != R_8051_24 && rel.type != R_8051_BANK8 &&
        !isBankThunk && !isAutoBankThunk)
      if (const Defined *direct =
              getCommonTargetOfExternalBankThunk(ctx, *rel.sym))
        val = direct->getVA(ctx, rel.addend);
    unsigned targetBank = 0;
    Defined *target = nullptr;
    if (rel.sym) {
      // The assembler rewrites references to local functions against their
      // STT_SECTION symbol, so the callee bank cannot depend on isFunc().
      target = dyn_cast<Defined>(rel.sym);
      auto *targetInput =
          target ? dyn_cast_or_null<InputSection>(target->section) : nullptr;
      if (targetInput && (targetInput->flags & SHF_EXECINSTR))
        targetBank = getMCS51CodeBank(targetInput->getOutputSection());
    }
    if ((rel.type == R_8051_24 || rel.type == R_8051_BANK8) && rel.sym) {
      if (rel.type == R_8051_BANK8) {
        relocate(loc, rel, targetBank);
        continue;
      }
      val = (static_cast<uint64_t>(targetBank) << 16) | (val & 0xffff);
      relocate(loc, rel, val);
      continue;
    }
    if (rel.sym && rel.sym->getName().starts_with("__mcs51_bankcall_")) {
      if (auto *thunk = dyn_cast<Defined>(rel.sym)) {
        if (Defined *function = findMCS51BankCallTarget(ctx, *thunk)) {
          auto *functionInput =
              dyn_cast_or_null<InputSection>(function->section);
          unsigned functionBank = functionInput
                                      ? getMCS51CodeBank(
                                            functionInput->getOutputSection())
                                      : 0;
          unsigned callerBank = getMCS51CodeBank(sec.getOutputSection());
          bool isDirectTransfer =
              rel.type == R_8051_16_BE && (sec.flags & SHF_EXECINSTR) &&
              rel.offset > 0 &&
              (sec.content()[rel.offset - 1] == 0x12 ||
               sec.content()[rel.offset - 1] == 0x02);
          // A function in common flash, or a function called from its own
          // bank, does not need the bank-switching trampoline.
          // IAR banked functions return through ?BRET and always need the
          // thunk that pushes the bank byte, even from the same bank.
          bool isIARThunk = thunk->section &&
                            thunk->section->name.starts_with(
                                ".text.autobankthunks.iar_");
          if (!isIARThunk &&
              (!functionBank ||
               (isDirectTransfer && functionBank == callerBank)))
            val = function->getVA(ctx, rel.addend);
        }
      }
    }
    // Local functions are referenced through their STT_SECTION symbol; resolve
    // the function symbol whose section it names so the bank checks apply.
    Defined *funcTarget = nullptr;
    if (rel.sym && rel.sym->isFunc())
      funcTarget = target;
    else if (rel.sym && rel.sym->isSection() && target) {
      auto *targetInput = dyn_cast_or_null<InputSection>(target->section);
      if (targetInput && (targetInput->flags & SHF_EXECINSTR) &&
          targetInput->name.starts_with(".text.") && rel.addend == 0) {
        for (Symbol *candidate : targetInput->file->getSymbols()) {
          auto *def = dyn_cast<Defined>(candidate);
          if (def && def->isFunc() && def->section == targetInput &&
              def->value == target->value && !def->getName().empty() &&
              !def->getName().starts_with(".L") &&
              findMCS51BankCallThunk(ctx, *def)) {
            funcTarget = def;
            break;
          }
        }
      }
    }
    if (funcTarget) {
      unsigned callerBank = getMCS51CodeBank(sec.getOutputSection());
      if (rel.type == R_8051_11 && targetBank != callerBank)
        Err(ctx) << sec.getLocation(rel.offset)
                 << "MCS-51 AJMP/ACALL cannot cross from bank " << callerBank
                 << " to bank " << targetBank << " for function '"
                 << rel.sym->getName() << "'";
      bool isLongCall = rel.type == R_8051_16_BE &&
                        (sec.flags & SHF_EXECINSTR) && rel.offset > 0 &&
                        sec.content()[rel.offset - 1] == 0x12;
      bool isLongJump = rel.type == R_8051_16_BE &&
                        (sec.flags & SHF_EXECINSTR) && rel.offset > 0 &&
                        sec.content()[rel.offset - 1] == 0x02;
      bool isDirectCodeTransfer =
          rel.type == R_8051_11 || isLongCall || isLongJump;
      bool isFunctionAddress =
          (rel.type == R_8051_16 || rel.type == R_8051_DPTR16 ||
           rel.type == R_8051_LO8 || rel.type == R_8051_HI8) &&
          !isLongCall;
      // IAR banked functions return through ?BRET and must always be entered
      // through the thunk that pushes the bank byte, even from their own bank.
      bool isIARBanked = false;
      if (isLongCall || isLongJump)
        if (Defined *thunk = findMCS51BankCallThunk(ctx, *funcTarget))
          if (auto *thunkSec = dyn_cast_or_null<InputSection>(thunk->section))
            isIARBanked =
                thunkSec->name.starts_with(".text.autobankthunks.iar_");
      bool needsThunk = targetBank &&
                        ((isDirectCodeTransfer &&
                          (targetBank != callerBank || isIARBanked)) ||
                         isFunctionAddress);
      if (needsThunk && !isBankThunk && !isAutoBankThunk) {
        // Some declarations in another translation unit do not carry a
        // section attribute. Resolve those calls and function pointers here,
        // after bank placement has made the target bank known.
        auto *thunk =
            findMCS51BankCallThunk(ctx, *funcTarget);
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
        Err(ctx) << sec.getLocation(rel.offset)
                 << "MCS-51 AJMP/ACALL target is outside the current 2 KiB page";
      loc[0] = (loc[0] & 0x1f) | ((val >> 3) & 0xe0);
      loc[1] = static_cast<uint8_t>(val);
      continue;
    }
    relocate(loc, rel, val);
  }
}

void elf::setMCS51TargetInfo(Ctx &ctx) { ctx.target.reset(new MCS51(ctx)); }
