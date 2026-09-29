#include "MCS51TargetMachine.h"
#include "MCS51MachineFunctionInfo.h"
#include "MCS51InstrInfo.h"
#include "MCS51.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "TargetInfo/MCS51TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Utils.h"
#include <optional>

using namespace llvm;

namespace {
class MCS51RemoveOptNone final : public FunctionPass {
public:
  static char ID;
  MCS51RemoveOptNone() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    if (!F.hasFnAttribute(Attribute::OptimizeNone))
      return false;
    F.removeFnAttr(Attribute::OptimizeNone);
    return true;
  }
};

char MCS51RemoveOptNone::ID = 0;

// Integer promotions turn byte-sized mask tests into i16 operations. The
// generic optimizer does not always push a zero extension through the AND,
// leaving the 8051 to materialize and compare both bytes. Narrow tests against
// an i8 zero-extended value back to i8 before instruction selection.
class MCS51NarrowByteMaskTests final : public FunctionPass {
public:
  static char ID;
  MCS51NarrowByteMaskTests() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    SmallVector<ICmpInst *, 16> Tests;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *Cmp = dyn_cast<ICmpInst>(&I))
          Tests.push_back(Cmp);

    bool Changed = false;
    for (ICmpInst *Cmp : Tests) {
      if (Cmp->getPredicate() != ICmpInst::ICMP_EQ &&
          Cmp->getPredicate() != ICmpInst::ICMP_NE)
        continue;
      auto *Zero = dyn_cast<ConstantInt>(Cmp->getOperand(1));
      Value *TestValue = Cmp->getOperand(0);
      if (!Zero) {
        Zero = dyn_cast<ConstantInt>(Cmp->getOperand(0));
        TestValue = Cmp->getOperand(1);
      }
      if (!Zero || !Zero->isZero())
        continue;

      auto *And = dyn_cast<BinaryOperator>(TestValue);
      if (!And || And->getOpcode() != Instruction::And ||
          !And->getType()->isIntegerTy(16))
        continue;
      auto *Mask = dyn_cast<ConstantInt>(And->getOperand(1));
      Value *ExtendedByte = And->getOperand(0);
      if (!Mask) {
        Mask = dyn_cast<ConstantInt>(And->getOperand(0));
        ExtendedByte = And->getOperand(1);
      }
      if (!Mask || Mask->getValue().getActiveBits() > 8)
        continue;
      auto *ZExt = dyn_cast<ZExtInst>(ExtendedByte);
      if (!ZExt || !ZExt->getSrcTy()->isIntegerTy(8))
        continue;

      IRBuilder<> Builder(Cmp);
      Value *NarrowMask = ConstantInt::get(Builder.getInt8Ty(),
                                            Mask->getZExtValue());
      Value *NarrowAnd = Builder.CreateAnd(ZExt->getOperand(0), NarrowMask,
                                           And->getName() + ".byte");
      Value *NarrowCmp = Builder.CreateICmp(
          Cmp->getPredicate(), NarrowAnd,
          ConstantInt::get(Builder.getInt8Ty(), 0), Cmp->getName() + ".byte");
      Cmp->replaceAllUsesWith(NarrowCmp);
      Cmp->eraseFromParent();
      if (And->use_empty())
        And->eraseFromParent();
      if (ZExt->use_empty())
        ZExt->eraseFromParent();
      Changed = true;
    }
    return Changed;
  }
};

char MCS51NarrowByteMaskTests::ID = 0;

// Turn an add of a zero-or-value select into a conditional accumulator update.
// Keeping this as control flow lets the backend branch on the original test
// instead of materializing a boolean, selecting zero/value, then adding.
class MCS51FoldConditionalByteAdd final : public FunctionPass {
public:
  static char ID;
  MCS51FoldConditionalByteAdd() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    SmallVector<BinaryOperator *, 16> Adds;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *Add = dyn_cast<BinaryOperator>(&I))
          if (Add->getOpcode() == Instruction::Add &&
              Add->getType()->isIntegerTy(8))
            Adds.push_back(Add);

    bool Changed = false;
    for (BinaryOperator *Add : Adds) {
      SelectInst *Select = dyn_cast<SelectInst>(Add->getOperand(0));
      Value *Accumulator = Add->getOperand(1);
      if (!Select) {
        Select = dyn_cast<SelectInst>(Add->getOperand(1));
        Accumulator = Add->getOperand(0);
      }
      if (!Select || Select->getType() != Add->getType())
        continue;
      auto *TrueZero = dyn_cast<ConstantInt>(Select->getTrueValue());
      auto *FalseZero = dyn_cast<ConstantInt>(Select->getFalseValue());
      bool ZeroOnTrue = TrueZero && TrueZero->isZero();
      bool ZeroOnFalse = FalseZero && FalseZero->isZero();
      if (ZeroOnTrue == ZeroOnFalse || Select->getParent() != Add->getParent() ||
          !Select->hasOneUse())
        continue;

      IRBuilder<> Builder(Add);
      Value *SelectedValue = ZeroOnTrue ? Select->getFalseValue()
                                        : Select->getTrueValue();
      Value *Updated = Builder.CreateAdd(Accumulator, SelectedValue,
                                         Add->getName() + ".cond");
      cast<BinaryOperator>(Updated)->copyIRFlags(Add);
      Value *NewSelect = Builder.CreateSelect(
          Select->getCondition(), ZeroOnTrue ? Accumulator : Updated,
          ZeroOnTrue ? Updated : Accumulator, Select->getName() + ".cond");
      Add->replaceAllUsesWith(NewSelect);
      Add->eraseFromParent();
      Changed = true;
    }
    return Changed;
  }
};

char MCS51FoldConditionalByteAdd::ID = 0;

class MCS51AccCopyHoisting final : public MachineFunctionPass {
public:
  static char ID;
  MCS51AccCopyHoisting() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    MachineRegisterInfo &MRI = MF.getRegInfo();
    bool Changed = false;
    SmallVector<Register, 32> AccValues;
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : MBB)
        for (const MachineOperand &MO : MI.operands())
          if (MO.isReg() && MO.isDef() && MO.getReg().isVirtual() &&
              MRI.getRegClass(MO.getReg()) == &MCS51::MCS51ARegRegClass)
            AccValues.push_back(MO.getReg());

    for (Register AccValue : AccValues) {
      MachineInstr *Def = MRI.getVRegDef(AccValue);
      if (!Def || !Def->getParent())
        continue;
      MachineBasicBlock &MBB = *Def->getParent();
      SmallVector<MachineInstr *, 4> Copies;
      bool OnlyCopies = true;
      for (MachineInstr &Use : MRI.use_nodbg_instructions(AccValue)) {
        if (!Use.isCopy() || Use.getOperand(1).getReg() != AccValue ||
            MRI.getRegClass(Use.getOperand(0).getReg()) !=
                &MCS51::MCS51GPR8RegClass ||
            !MRI.hasOneDef(Use.getOperand(0).getReg())) {
          OnlyCopies = false;
          break;
        }
        Copies.push_back(&Use);
      }
      if (!OnlyCopies || Copies.empty())
        continue;

      // Keep the accumulator value live only until its first GPR snapshot,
      // even when its original copies are in successor blocks. Later copies
      // read the saved GPR value instead.
      MachineInstr *Snapshot = Copies.front();
      Register SnapshotReg = Snapshot->getOperand(0).getReg();
      if (Def->getOpcode() == MCS51::MOV_A_IMM) {
        const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
        BuildMI(MBB, std::next(Def->getIterator()), Def->getDebugLoc(),
                TII->get(MCS51::MOV_RN_IMM), SnapshotReg)
            .addImm(Def->getOperand(1).getImm());
        Snapshot->eraseFromParent();
        for (MachineOperand &MO : Def->operands())
          if (MO.isReg() && MO.isDef() && MO.getReg() == AccValue)
            MO.setReg(MCS51::A);
      } else {
        MachineBasicBlock::iterator InsertPt =
            Def->isPHI() ? MBB.getFirstNonPHI()
                         : std::next(Def->getIterator());
        MBB.splice(InsertPt, Snapshot->getParent(), Snapshot->getIterator());
        Snapshot->getOperand(1).setIsKill(true);
      }
      for (unsigned I = 1; I < Copies.size(); ++I)
        Copies[I]->getOperand(1).setReg(SnapshotReg);
      MRI.clearKillFlags(SnapshotReg);
      Changed = true;
    }
    return Changed;
  }
};

char MCS51AccCopyHoisting::ID = 0;

class MCS51PostRAPeephole final : public MachineFunctionPass {
public:
  static char ID;
  MCS51PostRAPeephole() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
    bool Changed = false;
    for (MachineBasicBlock &MBB : MF) {
      std::optional<int> R0StackOffset;
      std::optional<int> R1StackOffset;
      const GlobalValue *DPTRGlobal = nullptr;
      int64_t DPTRGlobalOffset = 0;
      auto I = MBB.begin();
      while (I != MBB.end()) {
        // Keep DPTR live across nearby accesses to consecutive bytes of the
        // same global. Re-loading a 16-bit XDATA address costs three bytes;
        // incrementing DPTR costs one and leaves A and the flags untouched.
        if (I->getOpcode() == MCS51::MOV_DPTR_IMM &&
            I->getNumOperands() > 1 && I->getOperand(1).isGlobal()) {
          const MachineOperand &Address = I->getOperand(1);
          const GlobalValue *Global = Address.getGlobal();
          int64_t Offset = Address.getOffset();
          if (DPTRGlobal == Global && Offset == DPTRGlobalOffset + 1) {
            BuildMI(MBB, I, I->getDebugLoc(), TII->get(MCS51::INC_DPTR));
            I = MBB.erase(I);
            DPTRGlobalOffset = Offset;
            Changed = true;
            continue;
          }
          DPTRGlobal = Global;
          DPTRGlobalOffset = Offset;
          ++I;
          continue;
        }
        if (I->getOpcode() == MCS51::INC_DPTR && DPTRGlobal) {
          ++DPTRGlobalOffset;
          ++I;
          continue;
        }
        if (I->modifiesRegister(MCS51::DPTR, TRI))
          DPTRGlobal = nullptr;

        // Frame-index elimination forms R1 = SP + offset before most stack
        // accesses. Reuse its current value for an adjacent stack byte.
        auto AddressAdd = std::next(I);
        auto AddressMove = AddressAdd == MBB.end() ? MBB.end()
                                                   : std::next(AddressAdd);
        if (AddressMove != MBB.end() &&
            I->getOpcode() == MCS51::MOV_A_DIRECT &&
            I->getOperand(1).isImm() && I->getOperand(1).getImm() == 0x81 &&
            AddressAdd->getOpcode() == MCS51::ADD_A_IMM &&
            AddressAdd->getOperand(1).isImm() &&
            AddressMove->getOpcode() == MCS51::MOV_RN_A &&
            (AddressMove->getOperand(0).getReg() == MCS51::R0 ||
             AddressMove->getOperand(0).getReg() == MCS51::R1)) {
          Register AddressReg = AddressMove->getOperand(0).getReg();
          std::optional<int> &CachedOffset =
              AddressReg == MCS51::R0 ? R0StackOffset : R1StackOffset;
          int NewOffset =
              static_cast<int8_t>(AddressAdd->getOperand(1).getImm());
          if (CachedOffset) {
            int Delta = static_cast<int8_t>(
                static_cast<uint8_t>(NewOffset - *CachedOffset));
            if (Delta == 0) {
              auto AfterAddress = std::next(AddressMove);
              I->eraseFromParent();
              AddressAdd->eraseFromParent();
              AddressMove->eraseFromParent();
              CachedOffset = NewOffset;
              I = AfterAddress;
              Changed = true;
              continue;
            }
            auto AfterAddress = std::next(AddressMove);
            if (Delta >= -3 && Delta <= 3) {
              unsigned Opcode = Delta > 0 ? MCS51::INC_RN : MCS51::DEC_RN;
              int Steps = Delta > 0 ? Delta : -Delta;
              for (int Count = 0; Count < Steps; ++Count)
                BuildMI(MBB, I, AddressMove->getDebugLoc(), TII->get(Opcode))
                    .addReg(AddressReg, RegState::Define)
                    .addReg(AddressReg);
            } else {
              BuildMI(MBB, I, AddressMove->getDebugLoc(),
                      TII->get(MCS51::MOV_A_RN))
                  .addReg(AddressReg);
              BuildMI(MBB, I, AddressMove->getDebugLoc(),
                      TII->get(MCS51::ADD_A_IMM), MCS51::A)
                  .addImm(static_cast<uint8_t>(Delta));
              BuildMI(MBB, I, AddressMove->getDebugLoc(),
                      TII->get(MCS51::MOV_RN_A))
                  .addReg(AddressReg, RegState::Define);
            }
            I->eraseFromParent();
            AddressAdd->eraseFromParent();
            AddressMove->eraseFromParent();
            CachedOffset = NewOffset;
            I = AfterAddress;
            Changed = true;
            continue;
          }
          CachedOffset = NewOffset;
          I = std::next(AddressMove);
          continue;
        }

        // Calls may use R1 in the callee, and pushes/pops change the SP base.
        auto PointerStep = std::next(I);
        auto PointerStepMove = PointerStep == MBB.end()
                                   ? MBB.end()
                                   : std::next(PointerStep);
        Register PointerReg;
        if (I->getOpcode() == MCS51::MOV_A_RN)
          PointerReg = I->getOperand(0).getReg();
        std::optional<int> *PointerOffset =
            PointerReg == MCS51::R0 ? &R0StackOffset
            : PointerReg == MCS51::R1 ? &R1StackOffset
                                      : nullptr;
        if (PointerOffset && *PointerOffset &&
            PointerStepMove != MBB.end() &&
            I->getOpcode() == MCS51::MOV_A_RN &&
            (PointerStep->getOpcode() == MCS51::INC_A ||
             PointerStep->getOpcode() == MCS51::DEC_A) &&
            PointerStepMove->getOpcode() == MCS51::MOV_RN_A &&
            PointerStepMove->getOperand(0).getReg() == PointerReg) {
          **PointerOffset += PointerStep->getOpcode() == MCS51::INC_A ? 1 : -1;
          I = std::next(PointerStepMove);
          continue;
        }

        if (I->isCall() || I->getOpcode() == MCS51::PUSH_DIRECT ||
            I->getOpcode() == MCS51::PUSH_PSW ||
            I->getOpcode() == MCS51::POP_DIRECT ||
            I->getOpcode() == MCS51::POP_PSW) {
          R0StackOffset.reset();
          R1StackOffset.reset();
        } else {
          if (R0StackOffset && I->modifiesRegister(MCS51::R0, TRI))
            R0StackOffset.reset();
          if (R1StackOffset && I->modifiesRegister(MCS51::R1, TRI))
            R1StackOffset.reset();
        }

        if (I->getOpcode() != MCS51::MOV_RN_A) {
          ++I;
          continue;
        }

        auto First = I;
        auto Second = std::next(First);
        if (Second == MBB.end()) {
          ++I;
          continue;
        }
        // A value copied from A to a general register and immediately
        // copied back is still in A. Drop both copies when the register's
        // last use is the reload; this commonly appears before compares
        // whose operand class excludes A.
        if (First->getOpcode() == MCS51::MOV_RN_A &&
            Second->getOpcode() == MCS51::MOV_A_RN &&
            First->getOperand(0).getReg() == Second->getOperand(0).getReg() &&
            Second->getOperand(0).isKill()) {
          First->eraseFromParent();
          Second->eraseFromParent();
          Changed = true;
          I = MBB.begin();
          continue;
        }

        auto Third = std::next(Second);
        if (Third == MBB.end()) {
          ++I;
          continue;
        }

        auto IsDeadAfter = [&](MachineInstr &MI, Register Reg) {
          if (MI.getOperand(0).isKill())
            return true;
          if (!MBB.succ_empty())
            return false;
          for (auto J = std::next(MI.getIterator()); J != MBB.end(); ++J)
            if (J->readsRegister(Reg, TRI))
              return false;
          return true;
        };

        if ((Second->getOpcode() == MCS51::INC_RN ||
             Second->getOpcode() == MCS51::DEC_RN) &&
            Third->getOpcode() == MCS51::MOV_A_RN &&
            First->getOperand(0).getReg() == Second->getOperand(1).getReg() &&
            Second->getOperand(0).getReg() == Third->getOperand(0).getReg() &&
            IsDeadAfter(*Third, Third->getOperand(0).getReg())) {
          auto Next = std::next(Third);
          unsigned AccumulatorOpcode = Second->getOpcode() == MCS51::INC_RN
                                           ? MCS51::INC_A
                                           : MCS51::DEC_A;
          BuildMI(MBB, Second, Second->getDebugLoc(),
                  TII->get(AccumulatorOpcode));
          First->eraseFromParent();
          Second->eraseFromParent();
          Third->eraseFromParent();
          I = Next;
          Changed = true;
          continue;
        }

        if (Second->getOpcode() == MCS51::MOV_A_RN &&
            (Third->getOpcode() == MCS51::INC_A ||
             Third->getOpcode() == MCS51::DEC_A) &&
            First->getOperand(0).getReg() == Second->getOperand(0).getReg() &&
            IsDeadAfter(*Second, Second->getOperand(0).getReg())) {
          First->eraseFromParent();
          Second->eraseFromParent();
          I = Third;
          Changed = true;
          continue;
        }

        if (Second->getOpcode() != MCS51::INC_RN &&
            Second->getOpcode() != MCS51::DEC_RN) {
          ++I;
          continue;
        }

        ++I;
      }
    }
    return Changed;
  }
};

char MCS51PostRAPeephole::ID = 0;

class MCS51TargetObjectFile final : public TargetLoweringObjectFileELF {
public:
  MCSection *SelectSectionForGlobal(const GlobalObject *GO, SectionKind Kind,
                                    const TargetMachine &TM) const override {
    const auto *GV = dyn_cast<GlobalVariable>(GO);
    if (GV && !GV->hasSection() &&
        (GV->getAddressSpace() == MCS51::PData ||
         GV->getAddressSpace() == MCS51::Bit ||
         GV->getAddressSpace() == MCS51::Data ||
         GV->getAddressSpace() == MCS51::IData)) {
      bool IsBit = GV->getAddressSpace() == MCS51::Bit;
      bool IsPData = GV->getAddressSpace() == MCS51::PData;
      bool IsData = GV->getAddressSpace() == MCS51::Data;
      StringRef Name;
      if (IsBit)
        Name = Kind.isBSS() ? ".mcs51.bit.bss" : ".mcs51.bit";
      else if (IsPData)
        Name = Kind.isBSS() ? ".mcs51.pdata.bss" : ".mcs51.pdata";
      else if (IsData)
        Name = Kind.isBSS() ? ".mcs51.data1.bss" : ".mcs51.data1";
      else
        Name = Kind.isBSS() ? ".mcs51.data2.bss" : ".mcs51.data2";
      unsigned Type = Kind.isBSS() ? ELF::SHT_NOBITS : ELF::SHT_PROGBITS;
      unsigned Flags = ELF::SHF_ALLOC;
      if (!Kind.isReadOnly())
        Flags |= ELF::SHF_WRITE;
      return getContext().getELFSection(Name, Type, Flags);
    }
    return TargetLoweringObjectFileELF::SelectSectionForGlobal(GO, Kind, TM);
  }
};

constexpr StringLiteral MCS51DataLayout =
    "e-p:16:8-p1:8:8-p2:8:8-p3:8:8-p4:16:8-p5:16:8-p6:8:8-p7:8:8-"
    "p8:32:8-A2-"
    "i1:8-i8:8-i16:8-i32:8-i64:8-f32:8-f64:8-n8:16";
}

namespace {
class MCS51PassConfig final : public TargetPassConfig {
public:
  MCS51PassConfig(MCS51TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  MCS51TargetMachine &getMCS51TargetMachine() const {
    return getTM<MCS51TargetMachine>();
  }

  void addFastRegAlloc() override {
    // The fast allocator can create more spill slots than the 8051's 8-bit
    // stack frame can address, even for functions that the optimized
    // allocator handles comfortably. Use the optimized allocation pipeline
    // at -O0 as well, unless the user explicitly selected an allocator.
    if (usingDefaultRegAlloc())
      addOptimizedRegAlloc();
    else
      TargetPassConfig::addFastRegAlloc();
  }

  void addIRPasses() override {
    addPass(createMCS51OverlayPass());
    addPass(createMCS51StackAddressLoweringPass());
    TargetPassConfig::addIRPasses();
    if (getOptLevel() == CodeGenOptLevel::None) {
      addPass(new MCS51RemoveOptNone());
      addPass(createPromoteMemoryToRegisterPass());
    }
  }

  bool addInstSelector() override {
    addPass(createMCS51ISelDag(getMCS51TargetMachine(), getOptLevel()));
    return false;
  }

  bool addPreISel() override {
    addPass(createMCS51GenericPointerLoweringPass());
    addPass(new MCS51NarrowByteMaskTests());
    addPass(new MCS51FoldConditionalByteAdd());
    if (getOptLevel() == CodeGenOptLevel::None) {
      addPass(createInstructionCombiningPass());
      addPass(createCFGSimplificationPass());
    }
    return false;
  }

  void addPreRegAlloc() override { addPass(new MCS51AccCopyHoisting()); }

  void addPreEmitPass() override {
    addPass(new MCS51PostRAPeephole());
    addPass(&BranchRelaxationPassID);
  }
};
} // namespace

MCS51TargetMachine::MCS51TargetMachine(
    const Target &T, const Triple &TT, StringRef CPU, StringRef FS,
    const TargetOptions &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(
          T, MCS51DataLayout, TT, CPU, FS, Options,
          RM.value_or(Reloc::Static),
          getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<MCS51TargetObjectFile>()),
      Subtarget(TT, CPU, FS, *this) {
  initAsmInfo();
}

bool MCS51TargetMachine::addPassesToEmitFile(
    PassManagerBase &PM, raw_pwrite_stream &Out, raw_pwrite_stream *DwoOut,
    CodeGenFileType FileType, bool DisableVerify,
    MachineModuleInfoWrapperPass *MMIWP) {
  return CodeGenTargetMachineImpl::addPassesToEmitFile(
      PM, Out, DwoOut, FileType, DisableVerify, MMIWP);
}

TargetPassConfig *MCS51TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new MCS51PassConfig(*this, PM);
}

MachineFunctionInfo *MCS51TargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return MCS51MachineFunctionInfo::create<MCS51MachineFunctionInfo>(
      Allocator, F, STI);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMCS51Target() {
  RegisterTargetMachine<MCS51TargetMachine> X(getTheMCS51Target());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeMCS51AsmPrinterPass(PR);
  initializeMCS51DAGToDAGISelLegacyPass(PR);
}
