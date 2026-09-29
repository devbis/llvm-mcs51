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
    bool HasObservablePSWAccess = false;
    for (const MachineBasicBlock &MBB : MF)
      for (const MachineInstr &MI : MBB) {
        // The sign-extension fold below uses SUBB, which changes AC and OV.
        // Those flags can be observed through PSW SFR/bit accesses or inline
        // assembly, so keep the original rotates in functions that expose
        // machine state this way.
        if (MI.isInlineAsm())
          HasObservablePSWAccess = true;
        for (const MachineOperand &MO : MI.operands())
          if (MO.isImm() && MO.getImm() >= 0xD0 && MO.getImm() <= 0xD7)
            HasObservablePSWAccess = true;
      }
    for (MachineBasicBlock &MBB : MF) {
      SmallVector<MachineInstr *, 4> Calls;
      for (MachineInstr &MI : MBB)
        if (MI.isCall())
          Calls.push_back(&MI);

      for (MachineInstr *Call : Calls) {
        for (MachineInstr *I = Call->getNextNode(); I;) {
          MachineInstr *Add = I->getNextNode();
          MachineInstr *Store = Add ? Add->getNextNode() : nullptr;
          if (!Store || I->getOpcode() != MCS51::MOV_A_RN ||
              Add->getOpcode() != MCS51::ADD_A_IMM ||
              Store->getOpcode() != MCS51::MOV_RN_A ||
              I->getOperand(0).getReg() != Store->getOperand(0).getReg()) {
            I = I->getNextNode();
            continue;
          }

          Register Reg = I->getOperand(0).getReg();
          MachineInstr *Def = Call;
          bool FoundDef = false;
          while ((Def = Def->getPrevNode())) {
            if (!Def->modifiesRegister(Reg, TRI))
              continue;
            FoundDef = Def->getOpcode() == MCS51::MOV_RN_A &&
                       Def->getOperand(0).getReg() == Reg;
            break;
          }
          if (!FoundDef || Call->readsRegister(Reg, TRI) ||
              Call->modifiesRegister(Reg, TRI)) {
            I = Store->getNextNode();
            continue;
          }

          // The post-call value is only a delayed byte add if the original
          // register value and A/C are unobserved between its definition and
          // the call. In that case perform the add while the load is still in
          // A, then discard the post-call round trip.
          bool SafeToHoist = true;
          bool ARedefined = false;
          bool CRedefined = false;
          for (MachineInstr *J = Def->getNextNode(); J != Call;
               J = J->getNextNode()) {
            if (J->readsRegister(Reg, TRI) || J->modifiesRegister(Reg, TRI)) {
              SafeToHoist = false;
              break;
            }
            if ((!ARedefined && J->readsRegister(MCS51::A, TRI)) ||
                (!CRedefined && J->readsRegister(MCS51::C, TRI))) {
              SafeToHoist = false;
              break;
            }
            ARedefined |= J->modifiesRegister(MCS51::A, TRI);
            CRedefined |= J->modifiesRegister(MCS51::C, TRI);
          }
          for (MachineInstr *J = Call->getNextNode(); SafeToHoist && J != I;
               J = J->getNextNode())
            if (J->readsRegister(Reg, TRI) || J->modifiesRegister(Reg, TRI))
              SafeToHoist = false;
          if (!SafeToHoist) {
            I = Store->getNextNode();
            continue;
          }

          BuildMI(MBB, Def, Def->getDebugLoc(), TII->get(MCS51::ADD_A_IMM),
                  MCS51::A)
              .addImm(Add->getOperand(1).getImm());
          MachineInstr *AfterStore = Store->getNextNode();
          I->eraseFromParent();
          Add->eraseFromParent();
          Store->eraseFromParent();
          I = AfterStore;
          Changed = true;
        }
      }
    }
    for (MachineBasicBlock &MBB : MF) {
      std::optional<int> R0StackOffset;
      std::optional<int> R1StackOffset;
      const GlobalValue *DPTRGlobal = nullptr;
      int64_t DPTRGlobalOffset = 0;
      auto I = MBB.begin();
      while (I != MBB.end()) {
        auto ADeadOnSuccessors = [&]() {
          bool Dead = !MBB.succ_empty();
          for (MachineBasicBlock *Succ : MBB.successors()) {
            bool Redefined = false;
            for (MachineInstr &SuccMI : *Succ) {
              if (SuccMI.isPHI())
                continue;
              if (SuccMI.readsRegister(MCS51::A, TRI))
                break;
              if (SuccMI.modifiesRegister(MCS51::A, TRI)) {
                Redefined = true;
                break;
              }
            }
            Dead &= Redefined;
          }
          return Dead;
        };

        // A compare against 2^n followed by materializing its carry in A is
        // just a test of the high n bits. Avoid SUBB/CLR/RLC when only the
        // branch result is live; explicitly clear carry to preserve RLC's
        // final carry value.
        if (!HasObservablePSWAccess && I->getOpcode() == MCS51::MOV_A_RN &&
            ADeadOnSuccessors()) {
          MachineInstr *ClearCarry = I->getNextNode();
          MachineInstr *Subtract = ClearCarry ? ClearCarry->getNextNode()
                                               : nullptr;
          MachineInstr *ClearA = Subtract ? Subtract->getNextNode() : nullptr;
          MachineInstr *Rotate = ClearA ? ClearA->getNextNode() : nullptr;
          MachineInstr *Branch = Rotate ? Rotate->getNextNode() : nullptr;
          if (ClearCarry && Subtract && ClearA && Rotate && Branch &&
              ClearCarry->getOpcode() == MCS51::CLR_C &&
              Subtract->getOpcode() == MCS51::SUBB_A_IMM &&
              Subtract->getNumOperands() > 1 &&
              Subtract->getOperand(1).isImm() &&
              ClearA->getOpcode() == MCS51::CLR_A &&
              Rotate->getOpcode() == MCS51::RLC_A &&
              (Branch->getOpcode() == MCS51::JZ ||
               Branch->getOpcode() == MCS51::JNZ) &&
              Branch->getOperand(0).isMBB()) {
            unsigned Limit = Subtract->getOperand(1).getImm() & 0xff;
            bool IsPowerOfTwo = Limit && !(Limit & (Limit - 1));
            if (IsPowerOfTwo && Limit < 0x100) {
              unsigned Mask = (0x100 - Limit) & 0xff;
              unsigned TargetOpcode = Branch->getOpcode() == MCS51::JNZ
                                          ? MCS51::JZ
                                          : MCS51::JNZ;
              MachineBasicBlock *Target = Branch->getOperand(0).getMBB();
              DebugLoc Loc = I->getDebugLoc();
              BuildMI(MBB, ClearCarry, Loc, TII->get(MCS51::ANL_A_IMM),
                      MCS51::A)
                  .addImm(Mask);
              BuildMI(MBB, ClearCarry, Loc, TII->get(MCS51::CLR_C));
              BuildMI(MBB, Branch, Branch->getDebugLoc(),
                      TII->get(TargetOpcode))
                  .addMBB(Target);
              MachineInstr *AfterBranch = Branch->getNextNode();
              ClearCarry->eraseFromParent();
              Subtract->eraseFromParent();
              ClearA->eraseFromParent();
              Rotate->eraseFromParent();
              Branch->eraseFromParent();
              I = AfterBranch ? AfterBranch->getIterator() : MBB.end();
              Changed = true;
              continue;
            }
          }
        }

        // Testing one bit after an AND is cheaper as a bit-addressed branch.
        // Keep the load of the source byte, but avoid materializing the masked
        // value in A when neither outgoing path observes it.
        if (!HasObservablePSWAccess && I->getOpcode() == MCS51::MOV_A_RN) {
          MachineInstr *And = I->getNextNode();
          MachineInstr *Branch = And ? And->getNextNode() : nullptr;
          if (And && Branch && And->getOpcode() == MCS51::ANL_A_IMM &&
              And->getNumOperands() > 1 && And->getOperand(1).isImm() &&
              (Branch->getOpcode() == MCS51::JZ ||
               Branch->getOpcode() == MCS51::JNZ) &&
              Branch->getOperand(0).isMBB()) {
            unsigned Mask = And->getOperand(1).getImm() & 0xff;
            bool IsSingleBit = Mask && !(Mask & (Mask - 1));
            if (IsSingleBit && ADeadOnSuccessors()) {
              unsigned Bit = 0;
              while ((Mask >> Bit) != 1)
                ++Bit;
              unsigned BitAddress = 0xE0 + Bit;
              unsigned Opcode = Branch->getOpcode() == MCS51::JZ
                                    ? MCS51::JNB
                                    : MCS51::JB;
              BuildMI(MBB, Branch, Branch->getDebugLoc(), TII->get(Opcode))
                  .addImm(BitAddress)
                  .addMBB(Branch->getOperand(0).getMBB());
              MachineInstr *AfterBranch = Branch->getNextNode();
              And->eraseFromParent();
              Branch->eraseFromParent();
              I = AfterBranch ? AfterBranch->getIterator() : MBB.end();
              Changed = true;
              continue;
            }
          }
        }

        // Repeatedly copying A.7 to carry and rotating right seven times
        // sign-extends the original high bit to 0x00/0xff. Preserve both A
        // and carry with the shorter equivalent: carry = A.7; A = 0 - carry.
        if (!HasObservablePSWAccess && I->getOpcode() == MCS51::MOV_C_BIT &&
            I->getNumOperands() > 0 &&
            I->getOperand(0).isImm() && I->getOperand(0).getImm() == 0xE7) {
          SmallVector<MachineInstr *, 14> RotateSequence;
          auto Cursor = std::next(I);
          bool IsSignExtend = true;
          for (unsigned Count = 0; Count != 7; ++Count) {
            if (Cursor == MBB.end() ||
                Cursor->getOpcode() != MCS51::RRC_A) {
              IsSignExtend = false;
              break;
            }
            RotateSequence.push_back(&*Cursor++);
            if (Count == 6)
              break;
            if (Cursor == MBB.end() ||
                Cursor->getOpcode() != MCS51::MOV_C_BIT ||
                !Cursor->getOperand(0).isImm() ||
                Cursor->getOperand(0).getImm() != 0xE7) {
              IsSignExtend = false;
              break;
            }
            RotateSequence.push_back(&*Cursor++);
          }
          if (IsSignExtend) {
            auto Insert = std::next(I);
            BuildMI(MBB, Insert, I->getDebugLoc(), TII->get(MCS51::CLR_A));
            BuildMI(MBB, Insert, I->getDebugLoc(),
                    TII->get(MCS51::SUBB_A_IMM), MCS51::A).addImm(0);
            for (MachineInstr *MI : RotateSequence)
              MI->eraseFromParent();
            I = std::next(I);
            Changed = true;
            continue;
          }
        }

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

        // A spill value can be round-tripped through a temporary GPR while
        // the stack pointer is adjusted. INC/DEC Rn preserves A, so when the
        // reload is the last use the save and reload are redundant.
        Register RoundTripReg = First->getOperand(0).getReg();
        bool RoundTripValueDead = true;
        bool RoundTripRedefined = false;
        for (auto J = std::next(Third); RoundTripValueDead && J != MBB.end();
             ++J) {
          if (J->readsRegister(RoundTripReg, TRI)) {
            RoundTripValueDead = false;
            break;
          }
          if (J->modifiesRegister(RoundTripReg, TRI)) {
            RoundTripRedefined = true;
            break;
          }
        }
        if (!RoundTripRedefined)
          for (const MachineBasicBlock::RegisterMaskPair &LiveOut :
               MBB.liveouts())
            if (TRI->regsOverlap(RoundTripReg, LiveOut.PhysReg)) {
              RoundTripValueDead = false;
              break;
            }
        if (First->getOpcode() == MCS51::MOV_RN_A &&
            (Second->getOpcode() == MCS51::INC_RN ||
             Second->getOpcode() == MCS51::DEC_RN) &&
            Third->getOpcode() == MCS51::MOV_A_RN &&
            RoundTripReg == Third->getOperand(0).getReg() &&
            RoundTripValueDead &&
            Second->getOperand(0).getReg() != RoundTripReg &&
            !Second->modifiesRegister(MCS51::A, TRI)) {
          First->eraseFromParent();
          Third->eraseFromParent();
          I = Second;
          Changed = true;
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
