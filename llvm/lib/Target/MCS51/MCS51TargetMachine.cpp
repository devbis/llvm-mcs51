#include "MCS51TargetMachine.h"
#include "MCS51MachineFunctionInfo.h"
#include "MCS51InstrInfo.h"
#include "MCS51.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "TargetInfo/MCS51TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

namespace {
class MCS51PostRAPeephole final : public MachineFunctionPass {
public:
  static char ID;
  MCS51PostRAPeephole() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
    bool Changed = false;
    for (MachineBasicBlock &MBB : MF) {
      auto I = MBB.begin();
      while (I != MBB.end()) {
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

  void addIRPasses() override {
    addPass(createMCS51OverlayPass());
    addPass(createMCS51StackAddressLoweringPass());
    TargetPassConfig::addIRPasses();
  }

  bool addInstSelector() override {
    addPass(createMCS51ISelDag(getMCS51TargetMachine(), getOptLevel()));
    return false;
  }

  bool addPreISel() override {
    addPass(createMCS51GenericPointerLoweringPass());
    return false;
  }

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
