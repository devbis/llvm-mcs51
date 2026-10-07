#include "MCS51TargetMachine.h"
#include "MCS51TargetTransformInfo.h"
#include "MCS51MachineFunctionInfo.h"
#include "MCS51InstrInfo.h"
#include "MCS51.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "TargetInfo/MCS51TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Utils.h"
#include <optional>
#include <utility>
#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

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

// Large volatile XDATA operands are expensive to keep in caller registers on
// the 8051. Outline their unsigned i32 comparison into one target runtime
// helper, matching the helper-call strategy used by established 8051
// compilers. The helper performs the volatile reads in source order.
class MCS51OutlineXDataI32Compare final : public FunctionPass {
public:
  static char ID;
  MCS51OutlineXDataI32Compare() : FunctionPass(ID) {}

  static Function *getOrCreateHelper(Module &M) {
    if (Function *Existing = M.getFunction("__mcs51_xdata_ult32"))
      return Existing;

    LLVMContext &Context = M.getContext();
    Type *I32Ty = Type::getInt32Ty(Context);
    Type *I1Ty = Type::getInt1Ty(Context);
    Type *XDataPtrTy = PointerType::get(Context, 4);
    FunctionType *FTy = FunctionType::get(
        I1Ty, {XDataPtrTy, XDataPtrTy}, /*isVarArg=*/false);
    Function *Helper = Function::Create(
        FTy, GlobalValue::WeakODRLinkage, "__mcs51_xdata_ult32", M);
    Helper->addFnAttr(Attribute::NoInline);
    Helper->addFnAttr(Attribute::NoUnwind);
    Helper->addFnAttr(Attribute::OptimizeForSize);
    Helper->getArg(0)->setName("lhs");
    Helper->getArg(1)->setName("rhs");

    BasicBlock *Entry = BasicBlock::Create(Context, "entry", Helper);
    IRBuilder<> Builder(Entry);
    LoadInst *LHS = Builder.CreateLoad(I32Ty, Helper->getArg(0), "lhs.value");
    LHS->setVolatile(true);
    LHS->setAlignment(Align(1));
    LoadInst *RHS = Builder.CreateLoad(I32Ty, Helper->getArg(1), "rhs.value");
    RHS->setVolatile(true);
    RHS->setAlignment(Align(1));
    Builder.CreateRet(Builder.CreateICmpULT(LHS, RHS, "less"));
    return Helper;
  }

  static Function *getOrCreateSentinelHelper(Module &M) {
    if (Function *Existing =
            M.getFunction("__mcs51_xdata_ult32_or_max"))
      return Existing;

    LLVMContext &Context = M.getContext();
    Type *I32Ty = Type::getInt32Ty(Context);
    Type *I1Ty = Type::getInt1Ty(Context);
    Type *XDataPtrTy = PointerType::get(Context, 4);
    FunctionType *FTy = FunctionType::get(
        I1Ty, {XDataPtrTy, XDataPtrTy}, /*isVarArg=*/false);
    Function *Helper = Function::Create(
        FTy, GlobalValue::WeakODRLinkage,
        "__mcs51_xdata_ult32_or_max", M);
    Helper->addFnAttr(Attribute::NoInline);
    Helper->addFnAttr(Attribute::NoUnwind);
    Helper->addFnAttr(Attribute::OptimizeForSize);
    Helper->getArg(0)->setName("lhs");
    Helper->getArg(1)->setName("rhs");

    BasicBlock *Entry = BasicBlock::Create(Context, "entry", Helper);
    BasicBlock *IsMax = BasicBlock::Create(Context, "is.max", Helper);
    BasicBlock *Compare = BasicBlock::Create(Context, "compare", Helper);
    IRBuilder<> Builder(Entry);
    LoadInst *LHS = Builder.CreateLoad(I32Ty, Helper->getArg(0), "lhs.value");
    LHS->setVolatile(true);
    LHS->setAlignment(Align(1));
    Value *IsMaxValue = Builder.CreateICmpEQ(
        LHS, ConstantInt::get(I32Ty, UINT32_MAX), "lhs.is.max");
    Builder.CreateCondBr(IsMaxValue, IsMax, Compare);
    Builder.SetInsertPoint(IsMax);
    Builder.CreateRet(ConstantInt::getTrue(Context));
    Builder.SetInsertPoint(Compare);
    LoadInst *RHS = Builder.CreateLoad(I32Ty, Helper->getArg(1), "rhs.value");
    RHS->setVolatile(true);
    RHS->setAlignment(Align(1));
    Builder.CreateRet(Builder.CreateICmpULT(LHS, RHS, "less"));
    return Helper;
  }

  static bool outlineSentinelCompare(Function &F) {
    if (F.size() != 3)
      return false;

    ICmpInst *IsMax = nullptr;
    ICmpInst *IsLess = nullptr;
    LoadInst *LHS = nullptr;
    LoadInst *RHS = nullptr;
    CondBrInst *Dispatch = nullptr;
    PHINode *Result = nullptr;
    StoreInst *ResultStore = nullptr;
    ReturnInst *Return = nullptr;
    ZExtInst *Extended = nullptr;

    for (BasicBlock &BB : F)
      for (Instruction &I : BB) {
        if (auto *Cmp = dyn_cast<ICmpInst>(&I)) {
          if (Cmp->getPredicate() == ICmpInst::ICMP_EQ)
            IsMax = Cmp;
          else if (Cmp->getPredicate() == ICmpInst::ICMP_ULT)
            IsLess = Cmp;
        } else if (auto *Load = dyn_cast<LoadInst>(&I)) {
          if (!LHS)
            LHS = Load;
          else if (!RHS)
            RHS = Load;
          else
            return false;
        } else if (auto *Branch = dyn_cast<CondBrInst>(&I)) {
          Dispatch = Branch;
        } else if (auto *Phi = dyn_cast<PHINode>(&I)) {
          Result = Phi;
        } else if (auto *Store = dyn_cast<StoreInst>(&I)) {
          ResultStore = Store;
        } else if (auto *Ret = dyn_cast<ReturnInst>(&I)) {
          Return = Ret;
        } else if (auto *ZExt = dyn_cast<ZExtInst>(&I)) {
          Extended = ZExt;
        }
      }

    if (!IsMax || !IsLess || !LHS || !RHS || !Dispatch || !Result ||
        !ResultStore || !Return || !Extended ||
        Dispatch->getCondition() != IsMax || !LHS->isVolatile() ||
        !RHS->isVolatile() || !LHS->getType()->isIntegerTy(32) ||
        LHS->getType() != RHS->getType() || !ResultStore->isVolatile() ||
        !ResultStore->getValueOperand()->getType()->isIntegerTy(8) ||
        !isa<ConstantInt>(IsMax->getOperand(1)) ||
        !cast<ConstantInt>(IsMax->getOperand(1))->isMinusOne() ||
        IsMax->getOperand(0) != LHS || IsLess->getOperand(0) != LHS ||
        IsLess->getOperand(1) != RHS || Extended->getOperand(0) != IsLess ||
        ResultStore->getValueOperand() != Result ||
        Extended->getType() != Result->getType() ||
        !LHS->hasNUses(2) || !RHS->hasOneUse() ||
        !isa<PointerType>(LHS->getPointerOperandType()) ||
        !isa<PointerType>(RHS->getPointerOperandType()) ||
        cast<PointerType>(LHS->getPointerOperandType())->getAddressSpace() != 4 ||
        cast<PointerType>(RHS->getPointerOperandType())->getAddressSpace() != 4 ||
        !isa<PointerType>(ResultStore->getPointerOperandType()) ||
        cast<PointerType>(ResultStore->getPointerOperandType())
                ->getAddressSpace() != 4 ||
        !isa<Constant>(LHS->getPointerOperand()) ||
        !isa<Constant>(RHS->getPointerOperand()) ||
        !isa<Constant>(ResultStore->getPointerOperand()) ||
        Return->getReturnValue())
      return false;

    BasicBlock *Entry = Dispatch->getParent();
    BasicBlock *Continue = nullptr;
    BasicBlock *Join = Result->getParent();
    for (unsigned I = 0; I != 2; ++I) {
      BasicBlock *Successor = Dispatch->getSuccessor(I);
      if (Successor == Join)
        continue;
      if (Continue)
        return false;
      Continue = Successor;
    }
    if (!Continue || Continue == Entry || Join == Entry ||
        pred_size(Join) != 2 || Result->getNumIncomingValues() != 2 ||
        !isa<UncondBrInst>(Continue->getTerminator()) ||
        cast<UncondBrInst>(Continue->getTerminator())->getSuccessor(0) != Join)
      return false;

    bool HasTrueEdge = false;
    bool HasCompareEdge = false;
    for (unsigned I = 0; I != Result->getNumIncomingValues(); ++I) {
      Value *Incoming = Result->getIncomingValue(I);
      BasicBlock *IncomingBlock = Result->getIncomingBlock(I);
      if (IncomingBlock == Entry) {
        auto *Constant = dyn_cast<ConstantInt>(Incoming);
        HasTrueEdge = Constant && Constant->isOne();
      } else if (IncomingBlock == Continue) {
        HasCompareEdge = Incoming == Extended;
      }
    }
    if (!HasTrueEdge || !HasCompareEdge || Dispatch->getSuccessor(0) != Join)
      return false;

    SmallPtrSet<Instruction *, 16> ExpectedInstructions = {
        LHS, IsMax, Dispatch, RHS, IsLess, Extended,
        cast<Instruction>(Continue->getTerminator()), Result, ResultStore,
        Return};
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (!I.isDebugOrPseudoInst() && !ExpectedInstructions.contains(&I))
          return false;

    Value *LHSAddress = LHS->getPointerOperand();
    Value *RHSAddress = RHS->getPointerOperand();
    Value *ResultAddress = ResultStore->getPointerOperand();
    Align ResultAlign = ResultStore->getAlign();
    DebugLoc Debug = ResultStore->getDebugLoc();
    Function *Helper = getOrCreateSentinelHelper(*F.getParent());
    F.deleteBody();
    BasicBlock *NewEntry = BasicBlock::Create(F.getContext(), "entry", &F);
    IRBuilder<> Builder(NewEntry);
    CallInst *Call = Builder.CreateCall(Helper, {LHSAddress, RHSAddress},
                                        "counter.valid");
    Call->setDebugLoc(Debug);
    Value *ByteResult = Builder.CreateZExt(Call, Builder.getInt8Ty());
    StoreInst *Store = Builder.CreateStore(ByteResult, ResultAddress);
    Store->setVolatile(true);
    Store->setAlignment(ResultAlign);
    Store->setDebugLoc(Debug);
    Builder.CreateRetVoid();
    return true;
  }

  bool runOnFunction(Function &F) override {
    if (F.getName().starts_with("__mcs51_"))
      return false;

    if (outlineSentinelCompare(F))
      return true;

    SmallVector<ICmpInst *, 4> Comparisons;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *Cmp = dyn_cast<ICmpInst>(&I))
          if (Cmp->getPredicate() == ICmpInst::ICMP_ULT &&
              Cmp->getOperand(0)->getType()->isIntegerTy(32))
            Comparisons.push_back(Cmp);

    bool Changed = false;
    for (ICmpInst *Cmp : Comparisons) {
      auto *LHS = dyn_cast<LoadInst>(Cmp->getOperand(0));
      auto *RHS = dyn_cast<LoadInst>(Cmp->getOperand(1));
      if (!LHS || !RHS || !LHS->isVolatile() || !RHS->isVolatile() ||
          !LHS->hasOneUse() || !RHS->hasOneUse() ||
          LHS->getType() != RHS->getType() ||
          !LHS->getType()->isIntegerTy(32))
        continue;
      auto *LHSPtrTy = dyn_cast<PointerType>(LHS->getPointerOperandType());
      auto *RHSPtrTy = dyn_cast<PointerType>(RHS->getPointerOperandType());
      if (!LHSPtrTy || !RHSPtrTy || LHSPtrTy->getAddressSpace() != 4 ||
          RHSPtrTy->getAddressSpace() != 4)
        continue;

      Function *Helper = getOrCreateHelper(*F.getParent());
      IRBuilder<> Builder(Cmp);
      CallInst *Call = Builder.CreateCall(
          Helper, {LHS->getPointerOperand(), RHS->getPointerOperand()},
          Cmp->getName() + ".outlined");
      Call->setDebugLoc(Cmp->getDebugLoc());
      Cmp->replaceAllUsesWith(Call);
      Cmp->eraseFromParent();
      if (LHS->use_empty())
        LHS->eraseFromParent();
      if (RHS->use_empty())
        RHS->eraseFromParent();
      Changed = true;
    }
    return Changed;
  }
};

char MCS51OutlineXDataI32Compare::ID = 0;

// Integer promotions turn byte-sized mask tests into i16 operations. The
// generic optimizer does not always push a zero extension through the AND,
// leaving the 8051 to materialize and compare both bytes. Narrow tests against
// an i8 zero-extended value back to i8 before instruction selection.
class MCS51NarrowByteMaskTests final : public FunctionPass {
public:
  static char ID;
  MCS51NarrowByteMaskTests() : FunctionPass(ID) {}

  static PHINode *getSmallLoopIndex(Value *Index, unsigned &LimitOut) {
    auto *IndexPhi = dyn_cast<PHINode>(Index);
    if (!IndexPhi)
      if (auto *ZExt = dyn_cast<ZExtInst>(Index))
        IndexPhi = dyn_cast<PHINode>(ZExt->getOperand(0));
    if (!IndexPhi || !IndexPhi->getType()->isIntegerTy())
      return nullptr;

    auto *Branch = dyn_cast<CondBrInst>(IndexPhi->getParent()->getTerminator());
    if (!Branch)
      return nullptr;
    auto *ExitTest = dyn_cast<ICmpInst>(Branch->getCondition());
    if (!ExitTest || ExitTest->getPredicate() != ICmpInst::ICMP_EQ)
      return nullptr;

    auto *ControlPhi = dyn_cast<PHINode>(ExitTest->getOperand(0));
    ConstantInt *Limit = dyn_cast<ConstantInt>(ExitTest->getOperand(1));
    if (!ControlPhi) {
      ControlPhi = dyn_cast<PHINode>(ExitTest->getOperand(1));
      Limit = dyn_cast<ConstantInt>(ExitTest->getOperand(0));
    }
    if (!ControlPhi || !Limit ||
        ControlPhi->getParent() != IndexPhi->getParent() ||
        ControlPhi->getType() != Limit->getType() || Limit->isZero() ||
        Limit->getValue().uge(8))
      return nullptr;

    auto IsOne = [](Value *V) {
      auto *C = dyn_cast<ConstantInt>(V);
      return C && C->isOne();
    };
    auto GetLoopEdges = [&](PHINode *Phi)
        -> std::pair<BasicBlock *, BasicBlock *> {
      if (Phi->getNumIncomingValues() != 2)
        return {nullptr, nullptr};
      BasicBlock *Backedge = nullptr;
      BasicBlock *Entry = nullptr;
      bool HasZeroEntry = false;
      for (unsigned I = 0; I != 2; ++I) {
        Value *Incoming = Phi->getIncomingValue(I);
        BasicBlock *IncomingBlock = Phi->getIncomingBlock(I);
        auto *Add = dyn_cast<BinaryOperator>(Incoming);
        if (Add && Add->getOpcode() == Instruction::Add &&
            Add->getType() == Phi->getType() &&
            ((Add->getOperand(0) == Phi && IsOne(Add->getOperand(1))) ||
             (Add->getOperand(1) == Phi && IsOne(Add->getOperand(0))))) {
          auto *LatchBranch =
              dyn_cast<UncondBrInst>(IncomingBlock->getTerminator());
          if (!LatchBranch ||
              LatchBranch->getSuccessor(0) != Phi->getParent())
            return {nullptr, nullptr};
          Backedge = IncomingBlock;
        } else if (auto *Initial = dyn_cast<ConstantInt>(Incoming)) {
          HasZeroEntry = Initial->isZero();
          Entry = IncomingBlock;
        } else {
          return {nullptr, nullptr};
        }
      }
      if (!HasZeroEntry || !Backedge || Entry == Backedge)
        return {nullptr, nullptr};
      return {Entry, Backedge};
    };

    auto IndexEdges = GetLoopEdges(IndexPhi);
    auto ControlEdges = GetLoopEdges(ControlPhi);
    BasicBlock *IndexEntry = IndexEdges.first;
    BasicBlock *Backedge = IndexEdges.second;
    BasicBlock *ControlEntry = ControlEdges.first;
    BasicBlock *ControlBackedge = ControlEdges.second;

    auto CanReachBackedge = [&](BasicBlock *Start) {
      SmallVector<BasicBlock *, 8> Worklist(1, Start);
      SmallPtrSet<BasicBlock *, 16> Visited;
      while (!Worklist.empty()) {
        BasicBlock *Current = Worklist.pop_back_val();
        if (Current == Backedge)
          return true;
        if (!Visited.insert(Current).second)
          continue;
        for (BasicBlock *Successor : successors(Current))
          Worklist.push_back(Successor);
      }
      return false;
    };

    // The loop header exits when the induction variable reaches Limit. Its
    // only recurrent value is Phi + 1, so every executed shift count is below
    // Limit and fits in three bits.
    if (!IndexEntry || IndexEntry != ControlEntry || !Backedge ||
        Backedge != ControlBackedge ||
        CanReachBackedge(Branch->getSuccessor(0)) ||
        !CanReachBackedge(Branch->getSuccessor(1)))
      return nullptr;
    LimitOut = Limit->getZExtValue();
    return IndexPhi;
  }

  bool runOnFunction(Function &F) override {
    bool Changed = false;
    SmallVector<ICmpInst *, 16> Tests;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *Cmp = dyn_cast<ICmpInst>(&I))
          Tests.push_back(Cmp);

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
      if (And && And->getOpcode() == Instruction::And &&
          And->getType()->isIntegerTy(16)) {
        auto *Shift = dyn_cast<BinaryOperator>(And->getOperand(0));
        Value *ExtendedByte = And->getOperand(1);
        if (!Shift) {
          Shift = dyn_cast<BinaryOperator>(And->getOperand(1));
          ExtendedByte = And->getOperand(0);
        }
        auto *One = Shift && Shift->getOpcode() == Instruction::Shl
                        ? dyn_cast<ConstantInt>(Shift->getOperand(0))
                        : nullptr;
        auto *ZExt = dyn_cast<ZExtInst>(ExtendedByte);
        unsigned IndexLimit = 0;
        PHINode *Index = Shift ? getSmallLoopIndex(Shift->getOperand(1),
                                                  IndexLimit)
                               : nullptr;
        if (One && One->isOne() && ZExt &&
            ZExt->getSrcTy()->isIntegerTy(8) && Index) {
          IRBuilder<> Builder(Cmp);
          Value *ShiftCount = Index;
          if (Index->getType() != Builder.getInt8Ty())
            ShiftCount = Builder.CreateTrunc(
                Index, Builder.getInt8Ty(), Cmp->getName() + ".bit.index");
          Value *TestedBits = nullptr;
          if (IndexLimit == 2) {
            Value *IsZero = Builder.CreateICmpEQ(
                ShiftCount, ConstantInt::get(ShiftCount->getType(), 0),
                Cmp->getName() + ".index.zero");
            Value *Mask = Builder.CreateSelect(
                IsZero, ConstantInt::get(Builder.getInt8Ty(), 1),
                ConstantInt::get(Builder.getInt8Ty(), 2),
                Cmp->getName() + ".bit.mask");
            TestedBits = Builder.CreateAnd(ZExt->getOperand(0), Mask,
                                           Cmp->getName() + ".selected.bit");
          } else {
            Value *ShiftedByte = Builder.CreateLShr(
                ZExt->getOperand(0), ShiftCount,
                Cmp->getName() + ".bit.shifted");
            TestedBits = Builder.CreateAnd(
                ShiftedByte, ConstantInt::get(Builder.getInt8Ty(), 1),
                Cmp->getName() + ".bit");
          }
          Value *NarrowCmp = Builder.CreateICmp(
              Cmp->getPredicate(), TestedBits,
              ConstantInt::get(Builder.getInt8Ty(), 0),
              Cmp->getName() + ".byte");
          Cmp->replaceAllUsesWith(NarrowCmp);
          Cmp->eraseFromParent();
          Changed = true;
          continue;
        }
      }

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

// Sink selected constant bytes into their return edges for IDATA globals.
// A common store otherwise keeps the value in a register across the branch
// join and may overwrite the indirect pointer register needed by later edges.
class MCS51SinkIdataConstantStore final : public FunctionPass {
public:
  static char ID;
  MCS51SinkIdataConstantStore() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    SmallVector<BasicBlock *, 4> Blocks;
    for (BasicBlock &BB : F)
      Blocks.push_back(&BB);

    for (BasicBlock *Join : Blocks) {
      auto First = Join->getFirstNonPHIIt();
      auto *Store = First == Join->end() ? nullptr
                                         : dyn_cast<StoreInst>(&*First);
      auto *Ret = Store ? dyn_cast<ReturnInst>(Store->getNextNode()) : nullptr;
      auto *Selected = Store ? dyn_cast<PHINode>(Store->getValueOperand())
                             : nullptr;
      auto *PointerTy = Store ? dyn_cast<PointerType>(
                                   Store->getPointerOperandType())
                             : nullptr;
      bool HasUnexpectedPhiUse = false;
      for (PHINode &Phi : Join->phis())
        if (&Phi == Selected ? !Phi.hasOneUse() || *Phi.user_begin() != Store
                             : !Phi.use_empty())
          HasUnexpectedPhiUse = true;
      if (!Store || !Store->isVolatile() || !Ret || Ret->getReturnValue() ||
          !Selected || Selected->getParent() != Join ||
          !Selected->getType()->isIntegerTy(8) || !PointerTy ||
          PointerTy->getAddressSpace() != 2 ||
          !isa<Constant>(Store->getPointerOperand()) ||
          Selected->getNumIncomingValues() != pred_size(Join) ||
          HasUnexpectedPhiUse)
        continue;

      struct StoreEdge {
        BasicBlock *Pred;
        ConstantInt *Value;
        SelectInst *Choice;
        BasicBlock *Dispatch;
      };
      SmallVector<StoreEdge, 4> Edges;
      SmallPtrSet<BasicBlock *, 8> SeenPreds;
      bool CanSink = true;
      for (unsigned I = 0; I != Selected->getNumIncomingValues(); ++I) {
        BasicBlock *Pred = Selected->getIncomingBlock(I);
        if (!SeenPreds.insert(Pred).second) {
          CanSink = false;
          break;
        }
        Value *Incoming = Selected->getIncomingValue(I);
        ConstantInt *Constant = dyn_cast<ConstantInt>(Incoming);
        SelectInst *Choice = dyn_cast<SelectInst>(Incoming);
        if (Choice && Choice->getParent() != Pred)
          Choice = nullptr;
        auto *TrueValue = Choice
                              ? dyn_cast<ConstantInt>(Choice->getTrueValue())
                              : nullptr;
        auto *FalseValue = Choice
                               ? dyn_cast<ConstantInt>(Choice->getFalseValue())
                               : nullptr;
        if ((!Constant && (!Choice || !TrueValue || !FalseValue)) ||
            (Constant &&
             Constant->getType() != Store->getValueOperand()->getType()) ||
            (Choice &&
             (Choice->getType() != Store->getValueOperand()->getType() ||
              Choice->getNextNode() != Pred->getTerminator()))) {
          CanSink = false;
          break;
        }
        auto *Term = Pred->getTerminator();
        unsigned JoinSuccessor = Term->getNumSuccessors();
        for (unsigned S = 0; S != Term->getNumSuccessors(); ++S)
          if (Term->getSuccessor(S) == Join) {
            if (JoinSuccessor != Term->getNumSuccessors()) {
              CanSink = false;
              break;
            }
            JoinSuccessor = S;
          }
        if (!CanSink || JoinSuccessor == Term->getNumSuccessors()) {
          CanSink = false;
          break;
        }
        Edges.push_back({Pred, Constant, Choice, nullptr});
      }
      if (!CanSink)
        continue;

      auto MakeStoreReturnBlock = [&](ConstantInt *Value,
                                      const Twine &Name) {
        BasicBlock *Edge = BasicBlock::Create(F.getContext(), Name, &F, Join);
        IRBuilder<> Builder(Edge);
        StoreInst *EdgeStore =
            Builder.CreateStore(Value, Store->getPointerOperand());
        EdgeStore->setVolatile(true);
        EdgeStore->setAlignment(Store->getAlign());
        EdgeStore->copyMetadata(*Store);
        EdgeStore->setDebugLoc(Store->getDebugLoc());
        auto *EdgeReturn = Builder.CreateRetVoid();
        EdgeReturn->setDebugLoc(Ret->getDebugLoc());
        return Edge;
      };

      for (StoreEdge &Edge : Edges) {
        Instruction *Term = Edge.Pred->getTerminator();
        unsigned JoinSuccessor = 0;
        while (Term->getSuccessor(JoinSuccessor) != Join)
          ++JoinSuccessor;
        if (Edge.Choice) {
          Edge.Dispatch = BasicBlock::Create(F.getContext(),
                                             "mcs51.store.dispatch", &F, Join);
          BasicBlock *TrueEdge = MakeStoreReturnBlock(
              cast<ConstantInt>(Edge.Choice->getTrueValue()),
              "mcs51.store.true");
          BasicBlock *FalseEdge = MakeStoreReturnBlock(
              cast<ConstantInt>(Edge.Choice->getFalseValue()),
              "mcs51.store.false");
          CondBrInst::Create(Edge.Choice->getCondition(), TrueEdge, FalseEdge,
                             Edge.Dispatch);
        } else {
          Edge.Dispatch =
              MakeStoreReturnBlock(Edge.Value, "mcs51.store.edge");
        }
        Term->setSuccessor(JoinSuccessor, Edge.Dispatch);
      }

      Join->eraseFromParent();
      for (StoreEdge &Edge : Edges)
        if (Edge.Choice && Edge.Choice->use_empty())
          Edge.Choice->eraseFromParent();
      return true;
    }
    return false;
  }
};

char MCS51SinkIdataConstantStore::ID = 0;

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
            !Use.getOperand(0).getReg().isVirtual() ||
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

class MCS51RedundantSpillCopyElimination final : public MachineFunctionPass {
public:
  static char ID;
  MCS51RedundantSpillCopyElimination() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
    bool Changed = false;
    for (MachineBasicBlock &MBB : MF) {
      for (auto I = MBB.begin(); I != MBB.end();) {
        MachineInstr *Load = &*I++;
        if (Load->getOpcode() != MCS51::SPILL_LOAD16 || I == MBB.end())
          continue;
        MachineInstr *Store = &*I;
        if (Store->getOpcode() != MCS51::SPILL_STORE16 ||
            Load->getOperand(0).getReg() != Store->getOperand(2).getReg() ||
            Load->getOperand(1).getIndex() !=
                Store->getOperand(0).getIndex() ||
            Load->getOperand(2).getImm() != Store->getOperand(1).getImm())
          continue;
        // The store writes back the value that was just loaded, so it is
        // always redundant. The load itself is only dead when nothing reads
        // the register afterwards.
        Register Reg = Load->getOperand(0).getReg();
        bool UsedAfter = false;
        bool Redefined = false;
        for (MachineInstr *Later = Store->getNextNode(); Later;
             Later = Later->getNextNode()) {
          if (Later->readsRegister(Reg, TRI)) {
            UsedAfter = true;
            break;
          }
          if (Later->modifiesRegister(Reg, TRI)) {
            Redefined = true;
            break;
          }
        }
        if (!UsedAfter && !Redefined)
          for (const MachineBasicBlock *Succ : MBB.successors())
            for (const MachineBasicBlock::RegisterMaskPair &LiveIn :
                 Succ->liveins())
              if (TRI->regsOverlap(Reg, LiveIn.PhysReg))
                UsedAfter = true;
        MachineInstr *Next = Store->getNextNode();
        I = Next ? Next->getIterator() : MBB.end();
        Store->eraseFromParent();
        if (!UsedAfter)
          Load->eraseFromParent();
        Changed = true;
      }
    }
    return Changed;
  }
};

char MCS51RedundantSpillCopyElimination::ID = 0;

class MCS51PostRAPeephole final : public MachineFunctionPass {
public:
  static char ID;
  MCS51PostRAPeephole() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
    MachineRegisterInfo &MRI = MF.getRegInfo();
    bool Changed = false;
    // Lowering expands ADDDPTR16ri after SelectionDAG has marked the earlier
    // MOVC use as the final DPTR use. That pseudo appends INC_DPTR, so those
    // pre-existing kill flags are stale by the time register allocation ends.
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : MBB)
        for (MachineOperand &MO : MI.operands())
          if (MO.isReg() && MO.getReg() == MCS51::DPTR && MO.isUse())
            MO.setIsKill(false);

    bool HasObservablePSWAccess = false;
    bool HasObservableDPTRAccess = false;
    for (const MachineBasicBlock &MBB : MF)
      for (const MachineInstr &MI : MBB) {
        // The sign-extension fold below uses SUBB, which changes AC and OV.
        // Those flags can be observed through PSW SFR/bit accesses or inline
        // assembly, so keep the original rotates in functions that expose
        // machine state this way.
        if (MI.isInlineAsm()) {
          HasObservablePSWAccess = true;
          HasObservableDPTRAccess = true;
        }
        bool HasVolatileMMO = false;
        for (const MachineMemOperand *MMO : MI.memoperands())
          HasVolatileMMO |= MMO->isVolatile();
        for (const MachineOperand &MO : MI.operands())
          if (MO.isImm()) {
            if (MO.getImm() >= 0xD0 && MO.getImm() <= 0xD7)
              HasObservablePSWAccess = true;
            if (HasVolatileMMO &&
                (MO.getImm() == 0x82 || MO.getImm() == 0x83))
              HasObservableDPTRAccess = true;
          }
      }
    for (MachineBasicBlock &MBB : MF)
      for (auto I = MBB.begin(); I != MBB.end();) {
        MachineInstr *MI = &*I++;
        // XRL A,#0 preserves A, so it is redundant even when its result feeds
        // a branch. Word-sized zero comparisons otherwise retain this no-op
        // after lowering each byte independently.
        if (MI->getOpcode() == MCS51::XRL_A_IMM &&
            MI->getNumOperands() > 1 && MI->getOperand(1).isImm() &&
            MI->getOperand(1).getImm() == 0) {
          MRI.clearKillFlags(MCS51::A);
          MI->eraseFromParent();
          Changed = true;
          continue;
        }
        if (MI->getOpcode() == MCS51::MOV_A_IMM &&
            MI->getNumOperands() > 1 && MI->getOperand(1).isImm() &&
            MI->getOperand(1).getImm() == 0) {
          BuildMI(*MI->getParent(), MI->getIterator(), MI->getDebugLoc(),
                  TII->get(MCS51::CLR_A));
          MI->eraseFromParent();
          Changed = true;
          continue;
        }
        if (MI->getOpcode() == MCS51::MOV_A_IMM &&
            MI->getOperand(0).isDead()) {
          MI->eraseFromParent();
          Changed = true;
          continue;
        }
        if (!MI->isDead(MRI))
          continue;
        MI->eraseFromParent();
        Changed = true;
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
        MachineInstr *CopySPToA = &*I;
        MachineInstr *AddZero = CopySPToA->getNextNode();
        MachineInstr *CopyAToR1 = AddZero ? AddZero->getNextNode() : nullptr;
        bool LoadsSP = CopySPToA->getOpcode() == MCS51::MOV_A_SP ||
                       (CopySPToA->getOpcode() == MCS51::MOV_A_DIRECT &&
                        CopySPToA->getNumOperands() > 1 &&
                        CopySPToA->getOperand(1).isImm() &&
                        CopySPToA->getOperand(1).getImm() == 0x81);
        // The rewrite drops the address from A, so skip it when a later
        // instruction still reads A.
        auto AReadAfter = [&](MachineInstr *Start) {
          for (auto J = std::next(Start->getIterator()); J != MBB.end(); ++J) {
            if (J->readsRegister(MCS51::A, TRI))
              return true;
            if (J->modifiesRegister(MCS51::A, TRI))
              return false;
          }
          return false;
        };
        if (LoadsSP && AddZero &&
            CopyAToR1 && !AReadAfter(CopyAToR1) &&
            AddZero->getOpcode() == MCS51::ADD_A_IMM &&
            AddZero->getNumOperands() > 1 &&
            AddZero->getOperand(1).isImm() &&
            AddZero->getOperand(1).getImm() == 0 &&
            CopyAToR1->getOpcode() == MCS51::MOV_RN_A &&
            CopyAToR1->getNumOperands() &&
            CopyAToR1->getOperand(0).isReg() &&
            CopyAToR1->getOperand(0).getReg() == MCS51::R1) {
          BuildMI(MBB, CopySPToA, CopySPToA->getDebugLoc(),
                  TII->get(MCS51::MOV_RN_DIRECT))
              .addReg(MCS51::R1, RegState::Define)
              .addImm(0x81);
          auto Next = CopyAToR1->getNextNode();
          CopySPToA->eraseFromParent();
          AddZero->eraseFromParent();
          CopyAToR1->eraseFromParent();
          I = Next ? Next->getIterator() : MBB.end();
          Changed = true;
          continue;
        }

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

        // A sign mask used only to conditionally add a byte can be replaced
        // with a branch around the addition. This avoids materializing
        // 0x00/0xff, masking the value, and adding zero on the false path.
        if (!HasObservablePSWAccess && I->getOpcode() == MCS51::MOV_C_BIT &&
            I->getNumOperands() && I->getOperand(0).isImm()) {
          MachineInstr *ClearA = I->getNextNode();
          MachineInstr *Subtract = ClearA ? ClearA->getNextNode() : nullptr;
          MachineInstr *And = Subtract ? Subtract->getNextNode() : nullptr;
          MachineInstr *Add = And ? And->getNextNode() : nullptr;
          MachineInstr *Store = Add ? Add->getNextNode() : nullptr;
          if (ClearA && Subtract && And && Add && Store &&
              ClearA->getOpcode() == MCS51::CLR_A &&
              Subtract->getOpcode() == MCS51::SUBB_A_IMM &&
              Subtract->getNumOperands() > 1 &&
              Subtract->getOperand(1).isImm() &&
              Subtract->getOperand(1).getImm() == 0 &&
              And->getOpcode() == MCS51::ANL_A_RN &&
              And->getNumOperands() && And->getOperand(0).isReg() &&
              Add->getOpcode() == MCS51::ADD_A_RN &&
              Add->getNumOperands() && Add->getOperand(0).isReg() &&
              Store->getOpcode() == MCS51::MOV_RN_A &&
              Store->getNumOperands() && Store->getOperand(0).isReg()) {
            MachineFunction &MF = *MBB.getParent();
            Register ValueReg = And->getOperand(0).getReg();
            Register SumReg = Add->getOperand(0).getReg();
            MachineBasicBlock *Tail = MBB.splitAt(*Store);
            if (Tail == &MBB) {
              Tail = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
              MF.insert(std::next(MBB.getIterator()), Tail);
              Tail->transferSuccessorsAndUpdatePHIs(&MBB);
              MBB.addSuccessor(Tail);
            }
            Tail->splice(Tail->begin(), &MBB, Store->getIterator());
            MachineBasicBlock *AddBlock =
                MF.CreateMachineBasicBlock(MBB.getBasicBlock());
            MF.insert(Tail->getIterator(), AddBlock);
            while (!MBB.succ_empty())
              MBB.removeSuccessor(MBB.succ_begin());
            MBB.addSuccessor(AddBlock);
            MBB.addSuccessor(Tail);
            AddBlock->addSuccessor(Tail);
            AddBlock->addLiveIn(MCS51::A);
            AddBlock->addLiveIn(MCS51::R1);
            AddBlock->addLiveIn(ValueReg);
            Tail->addLiveIn(MCS51::A);
            Tail->addLiveIn(MCS51::R1);

            // MOV_C_BIT captured the original condition before loading the
            // accumulator with the running sum. MOV A,Rn preserves carry, so
            // JNC can skip the add without losing the selected bit.
            BuildMI(MBB, MBB.end(), I->getDebugLoc(),
                    TII->get(MCS51::MOV_A_RN))
                .addReg(SumReg);
            BuildMI(MBB, MBB.end(), I->getDebugLoc(), TII->get(MCS51::JNC))
                .addMBB(Tail);
            BuildMI(*AddBlock, AddBlock->end(), Add->getDebugLoc(),
                    TII->get(MCS51::ADD_A_RN))
                .addReg(ValueReg);

            SmallVector<MachineInstr *, 4> Replaced;
            Replaced.append({ClearA, Subtract, And, Add});
            for (MachineInstr *MI : Replaced)
              MI->eraseFromParent();
            I = MBB.end();
            Changed = true;
            continue;
          }
        }

        // Moving a selected bit into A.7 with repeated CLR C / RLC A pairs
        // is unnecessary when the shifted accumulator is cleared immediately
        // afterward. Read the original bit directly from the bit-addressable
        // accumulator instead.
        if (I->getOpcode() == MCS51::MOV_A_RN) {
          SmallVector<MachineInstr *, 14> ShiftSequence;
          auto Cursor = std::next(I);
          unsigned ShiftCount = 0;
          while (ShiftCount != 7 && Cursor != MBB.end() &&
                 Cursor->getOpcode() == MCS51::CLR_C) {
            MachineInstr *Rotate = Cursor->getNextNode();
            if (!Rotate || Rotate->getOpcode() != MCS51::RLC_A)
              break;
            ShiftSequence.push_back(&*Cursor);
            ShiftSequence.push_back(Rotate);
            Cursor = std::next(Rotate->getIterator());
            ++ShiftCount;
          }
          MachineInstr *CarryFromMSB =
              Cursor != MBB.end() ? &*Cursor : nullptr;
          MachineInstr *ClearA = CarryFromMSB
                                     ? CarryFromMSB->getNextNode()
                                     : nullptr;
          MachineInstr *Subtract = ClearA ? ClearA->getNextNode() : nullptr;
          if (ShiftCount && CarryFromMSB && ClearA && Subtract &&
              CarryFromMSB->getOpcode() == MCS51::MOV_C_BIT &&
              CarryFromMSB->getNumOperands() &&
              CarryFromMSB->getOperand(0).isImm() &&
              CarryFromMSB->getOperand(0).getImm() == 0xE7 &&
              ClearA->getOpcode() == MCS51::CLR_A &&
              Subtract->getOpcode() == MCS51::SUBB_A_IMM &&
              Subtract->getNumOperands() > 1 &&
              Subtract->getOperand(1).isImm() &&
              Subtract->getOperand(1).getImm() == 0) {
            CarryFromMSB->getOperand(0).setImm(0xE0 + 7 - ShiftCount);
            for (MachineInstr *MI : ShiftSequence)
              MI->eraseFromParent();
            I = CarryFromMSB->getIterator();
            Changed = true;
            continue;
          }
        }

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

        // After saving a masked operand, keep it in A and commute the add
        // instead of reloading the running sum into A.
        if (I->getOpcode() == MCS51::ANL_A_RN) {
          MachineInstr *SaveMask = I->getNextNode();
          MachineInstr *LoadSum = SaveMask ? SaveMask->getNextNode() : nullptr;
          MachineInstr *Add = LoadSum ? LoadSum->getNextNode() : nullptr;
          MachineInstr *AddImmediate = Add ? Add->getNextNode() : nullptr;
          MachineInstr *StoreSum =
              AddImmediate ? AddImmediate->getNextNode() : nullptr;
          if (SaveMask && LoadSum && Add && AddImmediate && StoreSum &&
              SaveMask->getOpcode() == MCS51::MOV_RN_A &&
              LoadSum->getOpcode() == MCS51::MOV_A_RN &&
              Add->getOpcode() == MCS51::ADD_A_RN &&
              AddImmediate->getOpcode() == MCS51::ADD_A_IMM &&
              StoreSum->getOpcode() == MCS51::MOV_RN_A &&
              SaveMask->getNumOperands() && LoadSum->getNumOperands() &&
              Add->getNumOperands() && StoreSum->getNumOperands() &&
              SaveMask->getOperand(0).isReg() &&
              LoadSum->getOperand(0).isReg() &&
              Add->getOperand(0).isReg() &&
              StoreSum->getOperand(0).isReg()) {
            Register MaskReg = SaveMask->getOperand(0).getReg();
            Register SumReg = LoadSum->getOperand(0).getReg();
            if (MaskReg != SumReg && Add->getOperand(0).getReg() == MaskReg &&
                StoreSum->getOperand(0).getReg() == SumReg) {
              bool MaskDies = Add->getOperand(0).isKill();
              Add->getOperand(0).setReg(SumReg);
              Add->getOperand(0).setIsKill(false);
              if (MaskDies)
                SaveMask->eraseFromParent();
              LoadSum->eraseFromParent();
              Changed = true;
              continue;
            }
          }
        }

        // Frame-index elimination forms R1 = SP + offset before most stack
        // accesses. Reuse its current value for an adjacent stack byte.
        if (I->getOpcode() == MCS51::MOV_RN_DIRECT &&
            I->getNumOperands() > 1 && I->getOperand(0).isReg() &&
            I->getOperand(1).isImm() && I->getOperand(1).getImm() == 0x81) {
          Register AddressReg = I->getOperand(0).getReg();
          if (AddressReg == MCS51::R0 || AddressReg == MCS51::R1) {
            std::optional<int> &CachedOffset =
                AddressReg == MCS51::R0 ? R0StackOffset : R1StackOffset;
            // MOV Rn,SP followed by INC/DEC steps names a stack address. When
            // the register already holds a nearby one, step from there.
            int Target = 0;
            auto Last = std::next(I);
            unsigned Steps = 0;
            while (Last != MBB.end() &&
                   (Last->getOpcode() == MCS51::INC_RN ||
                    Last->getOpcode() == MCS51::DEC_RN) &&
                   Last->getOperand(0).isReg() &&
                   Last->getOperand(0).getReg() == AddressReg) {
              Target += Last->getOpcode() == MCS51::INC_RN ? 1 : -1;
              ++Steps;
              ++Last;
            }
            if (CachedOffset && std::abs(Target - *CachedOffset) < int(Steps) + 1) {
              int Delta = Target - *CachedOffset;
              auto Insert = I;
              MRI.clearKillFlags(AddressReg);
              for (int N = 0; N != std::abs(Delta); ++N)
                BuildMI(MBB, Insert, I->getDebugLoc(),
                        TII->get(Delta > 0 ? MCS51::INC_RN : MCS51::DEC_RN))
                    .addReg(AddressReg, RegState::Define)
                    .addReg(AddressReg);
              while (I != Last)
                I = MBB.erase(I);
              CachedOffset = Target;
              Changed = true;
              continue;
            }
            CachedOffset = 0;
            ++I;
            continue;
          }
        }
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
          // The rewrite below drops the address from A. Keep the sequence when
          // a later instruction still reads it.
          bool AddressInAUsed = false;
          for (auto J = std::next(AddressMove); J != MBB.end(); ++J) {
            if (J->readsRegister(MCS51::A, TRI)) {
              AddressInAUsed = true;
              break;
            }
            if (J->modifiesRegister(MCS51::A, TRI))
              break;
          }
          if (CachedOffset && AddressInAUsed) {
            CachedOffset = NewOffset;
            I = std::next(AddressMove);
            continue;
          }
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
              // The previous SP-relative pointer value may have had its last
              // use marked killed. Reusing that physical register extends
              // its live range to the adjusted address, so remove stale kill
              // flags before inserting the new use.
              MRI.clearKillFlags(AddressReg);
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
        Register StepReg = I->getNumOperands() && I->getOperand(0).isReg()
                               ? I->getOperand(0).getReg()
                               : Register();
        std::optional<int> *StepOffset =
            StepReg == MCS51::R0 ? &R0StackOffset
            : StepReg == MCS51::R1 ? &R1StackOffset
                                   : nullptr;
        if (StepOffset && *StepOffset &&
            (I->getOpcode() == MCS51::INC_RN ||
             I->getOpcode() == MCS51::DEC_RN)) {
          **StepOffset += I->getOpcode() == MCS51::INC_RN ? 1 : -1;
          ++I;
          continue;
        }
        if (PointerOffset && *PointerOffset &&
            PointerStepMove != MBB.end() &&
            I->getOpcode() == MCS51::MOV_A_RN &&
            (PointerStep->getOpcode() == MCS51::INC_A ||
             PointerStep->getOpcode() == MCS51::DEC_A) &&
            PointerStepMove->getOpcode() == MCS51::MOV_RN_A &&
            PointerStepMove->getOperand(0).getReg() == PointerReg) {
          **PointerOffset += PointerStep->getOpcode() == MCS51::INC_A ? 1 : -1;
          // Keep stack cursor updates in the pointer register. The generic
          // lowering uses MOV A,Rn / INC|DEC A / MOV Rn,A, but MCS-51 has
          // single-instruction INC Rn and DEC Rn forms that preserve A.
          MRI.clearKillFlags(PointerReg);
          unsigned Opcode = PointerStep->getOpcode() == MCS51::INC_A
                                ? MCS51::INC_RN
                                : MCS51::DEC_RN;
          BuildMI(MBB, I, I->getDebugLoc(), TII->get(Opcode), PointerReg)
              .addReg(PointerReg);
          auto AfterStep = std::next(PointerStepMove);
          I->eraseFromParent();
          PointerStep->eraseFromParent();
          PointerStepMove->eraseFromParent();
          I = AfterStep;
          Changed = true;
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
          // Rescanning from the top of the block starts with no known
          // R0/R1 values and no tracked DPTR global.
          R0StackOffset.reset();
          R1StackOffset.reset();
          DPTRGlobal = nullptr;
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

    // Byte truncation can leave behind a zero-extension round trip through
    // DPTR: copy a GPR byte into DPH, move it to DPL with DPH cleared, then
    // copy DPL back to a GPR. The source byte already has the final value.
    // Keep the accumulator result and destination copy, but drop the DPTR
    // traffic when no source code can observe DPL/DPH.
    if (!HasObservableDPTRAccess)
      for (MachineBasicBlock &MBB : MF) {
        for (auto I = MBB.begin(); I != MBB.end();) {
          MachineInstr *CopyToA = &*I++;
          if (CopyToA->getOpcode() != MCS51::MOV_A_RN)
            continue;
          auto At = I;
          auto MatchDirectWrite = [&](MachineBasicBlock::iterator Pos,
                                      int64_t Address) {
            return Pos != MBB.end() &&
                   Pos->getOpcode() == MCS51::MOV_DIRECT_A &&
                   Pos->getNumOperands() >= 1 &&
                   Pos->getOperand(0).isImm() &&
                   Pos->getOperand(0).getImm() == Address &&
                   Pos->memoperands_empty();
          };
          auto MatchDirectRead = [&](MachineBasicBlock::iterator Pos,
                                     int64_t Address) {
            return Pos != MBB.end() &&
                   Pos->getOpcode() == MCS51::MOV_A_DIRECT &&
                   Pos->getNumOperands() >= 2 &&
                   Pos->getOperand(1).isImm() &&
                   Pos->getOperand(1).getImm() == Address &&
                   Pos->memoperands_empty();
          };
          if (!MatchDirectWrite(At, 0x83))
            continue;
          ++At;
          if (!MatchDirectRead(At, 0x83))
            continue;
          ++At;
          if (!MatchDirectWrite(At, 0x82))
            continue;
          ++At;
          if (At == MBB.end() || At->getOpcode() != MCS51::CLR_A ||
              !At->memoperands_empty())
            continue;
          ++At;
          if (!MatchDirectWrite(At, 0x83))
            continue;
          ++At;
          if (!MatchDirectRead(At, 0x82))
            continue;
          ++At;
          if (At == MBB.end() || At->getOpcode() != MCS51::MOV_RN_A ||
              At->getNumOperands() == 0 ||
              !MCS51::MCS51GPR8RegClass.contains(
                  At->getOperand(0).getReg()) ||
              !At->memoperands_empty())
            continue;

          Register Src = CopyToA->getOperand(0).getReg();
          Register Dst = At->getOperand(0).getReg();
          MachineBasicBlock::iterator After = std::next(At);
          bool AccumulatorDead = true;
          for (MachineInstr *Next = After == MBB.end() ? nullptr : &*After;
               Next;
               Next = Next->getNextNode()) {
            if (Next->readsRegister(MCS51::A, TRI)) {
              AccumulatorDead = false;
              break;
            }
            if (Next->modifiesRegister(MCS51::A, TRI))
              break;
          }
          if (Src == Dst) {
            if (AccumulatorDead)
              CopyToA->eraseFromParent();
            else
              CopyToA->getOperand(0).setIsKill(false);
          } else {
            BuildMI(MBB, I, CopyToA->getDebugLoc(),
                    TII->get(MCS51::MOV_RN_A), Dst);
          }
          while (I != After) {
            MachineInstr *Drop = &*I++;
            Drop->eraseFromParent();
          }
          Changed = true;
        }
      }

    // A low/high DPTR byte store may become dead after the round-trip above is
    // removed. Drop it when a full DPTR reload overwrites it before any use.
    if (!HasObservableDPTRAccess)
      for (MachineBasicBlock &MBB : MF) {
        for (auto I = MBB.begin(); I != MBB.end();) {
          MachineInstr *Write = &*I++;
          if (Write->getOpcode() != MCS51::MOV_DIRECT_A ||
              Write->getNumOperands() == 0 ||
              !Write->getOperand(0).isImm() ||
              (Write->getOperand(0).getImm() != 0x82 &&
               Write->getOperand(0).getImm() != 0x83) ||
              !Write->memoperands_empty())
            continue;
          bool Overwritten = false;
          for (MachineInstr *Next = Write->getNextNode(); Next;
               Next = Next->getNextNode()) {
            if (Next->readsRegister(MCS51::DPTR, TRI))
              break;
            if (Next->getOpcode() == MCS51::MOV_DPTR_IMM) {
              Overwritten = true;
              break;
            }
            if ((Next->getOpcode() == MCS51::MOV_DIRECT_A ||
                 Next->getOpcode() == MCS51::MOV_DIRECT_IMM) &&
                Next->getNumOperands() > 0 && Next->getOperand(0).isImm() &&
                Next->getOperand(0).getImm() ==
                    Write->getOperand(0).getImm()) {
              Overwritten = true;
              break;
            }
            if (Next->isTerminator())
              break;
          }
          if (Overwritten) {
            Write->eraseFromParent();
            Changed = true;
          }
        }
      }

    // Re-run the consecutive-global-address fold after removing dead DPL/DPH
    // writes; they otherwise hide sequential XDATA stores from the earlier
    // DPTR address tracking pass.
    for (MachineBasicBlock &MBB : MF) {
      const GlobalValue *KnownGlobal = nullptr;
      int64_t KnownOffset = 0;
      for (auto I = MBB.begin(); I != MBB.end();) {
        if (I->getOpcode() == MCS51::MOV_DPTR_IMM &&
            I->getNumOperands() > 1 && I->getOperand(1).isGlobal()) {
          const MachineOperand &Address = I->getOperand(1);
          const GlobalValue *Global = Address.getGlobal();
          int64_t Offset = Address.getOffset();
          if (KnownGlobal == Global && Offset == KnownOffset + 1) {
            BuildMI(MBB, I, I->getDebugLoc(), TII->get(MCS51::INC_DPTR));
            I = MBB.erase(I);
            KnownOffset = Offset;
            Changed = true;
            continue;
          }
          KnownGlobal = Global;
          KnownOffset = Offset;
          ++I;
          continue;
        }
        if (I->getOpcode() == MCS51::INC_DPTR && KnownGlobal) {
          ++KnownOffset;
          ++I;
          continue;
        }
        if (I->modifiesRegister(MCS51::DPTR, TRI))
          KnownGlobal = nullptr;
        ++I;
      }
    }

    // A volatile byte increment can use the native read/modify/write
    // instruction when the accumulator result is dead. Keep both memory
    // operands on the replacement so alias and volatile information survives.
    if (!HasObservablePSWAccess)
      for (MachineBasicBlock &MBB : MF) {
        for (auto I = MBB.begin(); I != MBB.end();) {
          MachineInstr *Load = &*I++;
          if (I == MBB.end() || I->getOpcode() != MCS51::INC_A)
            continue;
          MachineInstr *Increment = &*I++;
          if (I == MBB.end())
            continue;
          MachineInstr *Store = &*I++;
          unsigned ReplacementOpcode = 0;
          unsigned AddressOperand = 0;
          switch (Load->getOpcode()) {
          case MCS51::MOV_A_DIRECT:
            if (Store->getOpcode() == MCS51::MOV_DIRECT_A &&
                Load->getNumOperands() >= 2 && Store->getNumOperands() >= 1 &&
                Load->getOperand(1).isIdenticalTo(Store->getOperand(0))) {
              ReplacementOpcode = MCS51::INC_DIRECT;
              AddressOperand = 1;
            }
            break;
          case MCS51::MOV_A_IND_RI:
            if (Store->getOpcode() == MCS51::MOV_IND_RI_A &&
                Load->getNumOperands() >= 2 && Store->getNumOperands() >= 2 &&
                Load->getOperand(0).isIdenticalTo(Store->getOperand(0))) {
              Register Ptr = Load->getOperand(0).getReg();
              if (Ptr == MCS51::R0)
                ReplacementOpcode = MCS51::INC_R0_IND;
              else if (Ptr == MCS51::R1)
                ReplacementOpcode = MCS51::INC_R1_IND;
            }
            break;
          default:
            break;
          }
          if (!ReplacementOpcode)
            continue;

          bool AccumulatorDead = true;
          for (MachineInstr *Next = Store->getNextNode(); Next;
               Next = Next->getNextNode()) {
            if (Next->readsRegister(MCS51::A, TRI)) {
              AccumulatorDead = false;
              break;
            }
            if (Next->modifiesRegister(MCS51::A, TRI))
              break;
          }
          if (!AccumulatorDead)
            continue;
          for (const MachineBasicBlock::RegisterMaskPair &LiveOut :
               MBB.liveouts()) {
            if (!TRI->regsOverlap(MCS51::A, LiveOut.PhysReg))
              continue;
            for (MachineBasicBlock *Succ : MBB.successors()) {
              bool RedefinedBeforeUse = false;
              for (MachineInstr &SuccMI : *Succ) {
                if (SuccMI.isDebugInstr())
                  continue;
                if (SuccMI.readsRegister(MCS51::A, TRI))
                  break;
                if (SuccMI.modifiesRegister(MCS51::A, TRI)) {
                  RedefinedBeforeUse = true;
                  break;
                }
                if (SuccMI.isTerminator())
                  break;
              }
              if (!RedefinedBeforeUse) {
                AccumulatorDead = false;
                break;
              }
            }
            if (!AccumulatorDead)
              break;
          }
          if (!AccumulatorDead)
            continue;

          MachineInstrBuilder DirectIncrement =
              BuildMI(MBB, Load, Load->getDebugLoc(),
                      TII->get(ReplacementOpcode));
          if (ReplacementOpcode == MCS51::INC_DIRECT)
            DirectIncrement.add(Load->getOperand(AddressOperand));
          for (MachineMemOperand *MMO : Load->memoperands())
            DirectIncrement.addMemOperand(MMO);
          for (MachineMemOperand *MMO : Store->memoperands())
            DirectIncrement.addMemOperand(MMO);
          Load->eraseFromParent();
          Increment->eraseFromParent();
          Store->eraseFromParent();
          Changed = true;
        }
      }

    // Late byte extraction can leave a GPR-to-A move immediately before
    // reading DPH into A. DPH overwrites the accumulator without using it,
    // so that first move is redundant. Keep this pattern narrow: other moves
    // may feed values carried across basic-block boundaries.
    for (MachineBasicBlock &MBB : MF) {
      for (auto I = MBB.begin(); I != MBB.end();) {
        MachineInstr *Move = &*I++;
        if (Move->getOpcode() != MCS51::MOV_A_RN)
          continue;
        MachineInstr *ReadDPH = Move->getNextNode();
        if (!ReadDPH || ReadDPH->getOpcode() != MCS51::MOV_A_DIRECT ||
            ReadDPH->getNumOperands() < 2 ||
            !ReadDPH->getOperand(1).isImm() ||
            ReadDPH->getOperand(1).getImm() != 0x83 ||
            !Move->memoperands_empty())
          continue;
        Move->eraseFromParent();
        Changed = true;
      }
    }

    SmallVector<MachineBasicBlock *, 16> Blocks;
    for (MachineBasicBlock &MBB : MF)
      Blocks.push_back(&MBB);
    fullyRecomputeLiveIns(Blocks);
    return Changed;
  }
};

char MCS51PostRAPeephole::ID = 0;

class MCS51BranchIslandSharing final : public MachineFunctionPass {
public:
  static char ID;
  MCS51BranchIslandSharing() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const auto &TII =
        *static_cast<const MCS51InstrInfo *>(MF.getSubtarget().getInstrInfo());
    DenseMap<MachineBasicBlock *, SmallVector<MachineBasicBlock *, 4>>
        IslandsByTarget;
    bool Changed = false;

    for (auto I = MF.begin(); I != MF.end();) {
      MachineBasicBlock *Island = &*I++;
      MachineBasicBlock *Source = nullptr;
      MachineBasicBlock *Fallthrough = nullptr;
      MachineBasicBlock *Target = nullptr;
      if (!isBranchIsland(*Island, TII, Source, Fallthrough, Target))
        continue;

      SmallVector<std::pair<MBBSectionID, uint64_t>, 4> SectionOffsets;
      DenseMap<MachineBasicBlock *, uint64_t> BlockOffsets;
      uint64_t Offset = 0;
      for (MachineBasicBlock &MBB : MF) {
        auto SectionOffset = llvm::find_if(
            SectionOffsets, [&](const auto &Entry) {
              return Entry.first == MBB.getSectionID();
            });
        if (SectionOffset == SectionOffsets.end()) {
          SectionOffsets.emplace_back(MBB.getSectionID(), 0);
          SectionOffset = std::prev(SectionOffsets.end());
        }
        Offset = alignTo(SectionOffset->second, MBB.getAlignment());
        BlockOffsets[&MBB] = Offset;
        for (const MachineInstr &MI : MBB)
          Offset += TII.getInstSizeInBytes(MI);
        SectionOffset->second = Offset;
      }

      MachineInstr *Branch = &*Source->getLastNonDebugInstr();
      uint64_t BranchOffset = BlockOffsets.lookup(Source);
      for (MachineInstr &MI : *Source) {
        if (&MI == Branch)
          break;
        BranchOffset += TII.getInstSizeInBytes(MI);
      }

      MachineBasicBlock *SharedIsland = nullptr;
      for (MachineBasicBlock *Candidate : IslandsByTarget[Target]) {
        if (!Candidate->sameSection(Source) ||
            !haveSameLiveIns(*Island, *Candidate))
          continue;
        int64_t BranchOffsetToIsland =
            static_cast<int64_t>(BlockOffsets.lookup(Candidate)) -
            static_cast<int64_t>(BranchOffset);
        if (TII.isBranchOffsetInRange(
                Branch->getOpcode(), BranchOffsetToIsland)) {
          SharedIsland = Candidate;
          break;
        }
      }

      if (!SharedIsland) {
        IslandsByTarget[Target].push_back(Island);
        continue;
      }

      SmallVector<MachineOperand, 1> Condition;
      Condition.push_back(MachineOperand::CreateImm(Branch->getOpcode()));
      if (TII.reverseBranchCondition(Condition)) {
        IslandsByTarget[Target].push_back(Island);
        continue;
      }

      DebugLoc DL = Branch->getDebugLoc();
      TII.removeBranch(*Source);
      TII.insertBranch(*Source, SharedIsland, nullptr, Condition, DL);
      Source->replaceSuccessor(Island, SharedIsland);
      Island->removeSuccessor(Target);
      Island->eraseFromParent();
      Changed = true;
    }
    return Changed;
  }

private:
  static bool haveSameLiveIns(const MachineBasicBlock &A,
                              const MachineBasicBlock &B) {
    if (std::distance(A.liveins().begin(), A.liveins().end()) !=
        std::distance(B.liveins().begin(), B.liveins().end()))
      return false;
    for (const MachineBasicBlock::RegisterMaskPair &LiveInA : A.liveins()) {
      bool Found = false;
      for (const MachineBasicBlock::RegisterMaskPair &LiveInB : B.liveins())
        if (LiveInA.PhysReg == LiveInB.PhysReg &&
            LiveInA.LaneMask == LiveInB.LaneMask) {
          Found = true;
          break;
        }
      if (!Found)
        return false;
    }
    return true;
  }

  static bool isBranchIsland(MachineBasicBlock &Island,
                             const MCS51InstrInfo &TII,
                             MachineBasicBlock *&Source,
                             MachineBasicBlock *&Fallthrough,
                             MachineBasicBlock *&Target) {
    if (Island.hasAddressTaken() || Island.pred_size() != 1 ||
        Island.succ_size() != 1)
      return false;

    MachineInstr *Jump = nullptr;
    for (MachineInstr &MI : Island) {
      if (MI.isDebugInstr())
        continue;
      if (Jump)
        return false;
      Jump = &MI;
    }
    if (!Jump || Jump->getOpcode() != MCS51::LJMP)
      return false;

    Source = *Island.pred_begin();
    if (Source->getNextNode() != &Island ||
        !Source->getLastNonDebugInstr().isValid() ||
        Source->succ_size() != 2)
      return false;

    MachineInstr &Branch = *Source->getLastNonDebugInstr();
    Fallthrough = Island.getNextNode();
    Target = TII.getBranchDestBlock(*Jump);
    if (!Fallthrough || !Target ||
        TII.getBranchDestBlock(Branch) != Fallthrough ||
        *Island.succ_begin() != Target ||
        !Source->sameSection(&Island) ||
        !Island.sameSection(Fallthrough))
      return false;

    bool HasIslandEdge = false;
    bool HasFallthroughEdge = false;
    for (MachineBasicBlock *Succ : Source->successors()) {
      HasIslandEdge |= Succ == &Island;
      HasFallthroughEdge |= Succ == Fallthrough;
    }
    return HasIslandEdge && HasFallthroughEdge;
  }
};

char MCS51BranchIslandSharing::ID = 0;

// Direct reads of SFR addresses 0x82 and 0x83 return the bytes of DPTR, but
// the instructions only name the address. Without a use of DPTR the copy that
// loaded it looks dead and is deleted, and the read returns whatever DPTR
// happened to hold. Record the use on every such read.

static cl::opt<unsigned> UnrollThreshold(
    "mcs51-unroll-threshold", cl::Hidden, cl::init(30),
    cl::desc("Cost threshold for fully unrolling constant-trip loops"));
static cl::opt<bool> CheckDptrReads("mcs51-check-dptr-reads", cl::Hidden,
                                    cl::desc("Report direct DPL/DPH reads with no DPTR write in the block"));

// Blocks that custom inserters create inside a call sequence start with no
// recorded call frame size, but PEI needs it to resolve frame indices by the
// number of argument bytes already pushed. Recompute it for every block.
// Several stores of the same constant into direct or register bytes are
// shorter through A: one load of A and 2-byte (or 1-byte) stores replace the
// 3-byte (or 2-byte) immediate moves. Applies where A is dead.
class MCS51ConstantByteGrouping final : public MachineFunctionPass {
public:
  static char ID;
  MCS51ConstantByteGrouping() : MachineFunctionPass(ID) {}

  static bool isCandidate(const MachineInstr &MI) {
    return (MI.getOpcode() == MCS51::MOV_IM_IMM ||
            MI.getOpcode() == MCS51::MOV_RN_IMM) &&
           MI.getOperand(1).isImm();
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const auto &TII =
        *static_cast<const MCS51InstrInfo *>(MF.getSubtarget().getInstrInfo());
    const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
    bool Changed = false;
    // MOV Rn,A / MOV A,Rn (either order) leaves A unchanged: drop the second.
    for (MachineBasicBlock &MBB : MF) {
      MachineInstr *Prev = nullptr;
      for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
        if (MI.isDebugInstr())
          continue;
        unsigned Opc = MI.getOpcode();
        if (Prev && ((Prev->getOpcode() == MCS51::MOV_RN_A &&
                      Opc == MCS51::MOV_A_RN) ||
                     (Prev->getOpcode() == MCS51::MOV_A_RN &&
                      Opc == MCS51::MOV_RN_A))) {
          // Both forms name the R register as their first operand.
          Register PrevReg = Prev->getOperand(0).getReg();
          Register Reg = MI.getOperand(0).getReg();
          if (PrevReg == Reg) {
            bool Killed = Opc == MCS51::MOV_A_RN && MI.getOperand(0).isKill();
            MI.eraseFromParent();
            // With the register dead afterwards, the store was only for MI.
            if (Killed)
              Prev->eraseFromParent();
            Changed = true;
            Prev = nullptr;
            continue;
          }
        }
        // MOV A,Rn; INC/DEC A; MOV Rn,A with A dead afterwards: INC/DEC Rn.
        if (MI.getOpcode() == MCS51::MOV_RN_A && Prev &&
            (Prev->getOpcode() == MCS51::INC_A ||
             Prev->getOpcode() == MCS51::DEC_A)) {
          MachineInstr *Load = Prev->getPrevNode();
          while (Load && Load->isDebugInstr())
            Load = Load->getPrevNode();
          Register R = MI.getOperand(0).getReg();
          if (Load && Load->getOpcode() == MCS51::MOV_A_RN &&
              Load->getOperand(0).getReg() == R) {
            // A must be dead after the sequence.
            bool ADead = true;
            auto It = std::next(MI.getIterator());
            for (; It != MBB.end(); ++It) {
              if (It->isDebugInstr())
                continue;
              if (It->readsRegister(MCS51::A, TRI)) {
                ADead = false;
                break;
              }
              if (It->isCall() || It->modifiesRegister(MCS51::A, TRI))
                break;
            }
            if (ADead && It == MBB.end())
              for (MachineBasicBlock *Succ : MBB.successors())
                ADead &= !Succ->isLiveIn(MCS51::A);
            if (ADead) {
              bool IsInc = Prev->getOpcode() == MCS51::INC_A;
              BuildMI(MBB, MI, MI.getDebugLoc(),
                      TII.get(IsInc ? MCS51::INC_RN : MCS51::DEC_RN), R)
                  .addReg(R);
              MI.eraseFromParent();
              Prev->eraseFromParent();
              Load->eraseFromParent();
              Changed = true;
              Prev = nullptr;
              continue;
            }
          }
        }
        // MOV Rn,#imm followed by MOV A,Rn with Rn dead: load A directly.
        if (Prev && Prev->getOpcode() == MCS51::MOV_RN_IMM &&
            Opc == MCS51::MOV_A_RN && Prev->getOperand(1).isImm() &&
            Prev->getOperand(0).getReg() == MI.getOperand(0).getReg() &&
            MI.getOperand(0).isKill()) {
          BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(MCS51::MOV_A_IMM),
                  MCS51::A)
              .addImm(Prev->getOperand(1).getImm());
          MI.eraseFromParent();
          Prev->eraseFromParent();
          Changed = true;
          Prev = nullptr;
          continue;
        }
        Prev = &MI;
      }
    }
    // A single-block loop that steps a register and tests it for zero becomes
    // DJNZ: DEC Rn; MOV A,Rn; JNZ self, or the same counting up to zero from a
    // constant (the start value is negated).
    for (MachineBasicBlock &MBB : MF) {
      if (MBB.succ_size() != 2 || !MBB.isSuccessor(&MBB))
        continue;
      auto Term = MBB.getFirstTerminator();
      if (Term == MBB.end() || Term->getOpcode() != MCS51::JNZ ||
          Term->getOperand(0).getMBB() != &MBB || Term == MBB.begin())
        continue;
      auto LoadA = std::prev(Term);
      if (LoadA->getOpcode() != MCS51::MOV_A_RN || LoadA == MBB.begin())
        continue;
      auto Step = std::prev(LoadA);
      Register R = LoadA->getOperand(0).getReg();
      if ((Step->getOpcode() != MCS51::DEC_RN &&
           Step->getOpcode() != MCS51::INC_RN) ||
          Step->getOperand(0).getReg() != R)
        continue;
      // A is not live around the loop.
      bool ALive = false;
      for (MachineBasicBlock *Succ : MBB.successors())
        ALive |= Succ->isLiveIn(MCS51::A);
      if (ALive)
        continue;
      // The loop must be short enough for DJNZ's 8-bit reach.
      unsigned Bytes = 0;
      for (const MachineInstr &MI : MBB)
        Bytes += TII.getInstSizeInBytes(MI);
      if (Bytes > 100)
        continue;
      // R is not used elsewhere in the loop.
      bool OtherUse = false;
      for (MachineInstr &MI : MBB)
        if (&MI != &*Step && &MI != &*LoadA && !MI.isDebugInstr() &&
            (MI.readsRegister(R, TRI) || MI.modifiesRegister(R, TRI)))
          OtherUse = true;
      if (OtherUse)
        continue;
      MachineBasicBlock *Exit = nullptr;
      for (MachineBasicBlock *Succ : MBB.successors())
        if (Succ != &MBB)
          Exit = Succ;
      if (!Exit || Exit->isLiveIn(R))
        continue;
      if (Step->getOpcode() == MCS51::INC_RN) {
        // Counting up to zero from a constant: count down from its negation.
        MachineBasicBlock *Pre = nullptr;
        for (MachineBasicBlock *P : MBB.predecessors())
          if (P != &MBB)
            Pre = Pre ? nullptr : P;
        if (!Pre || MBB.pred_size() != 2)
          continue;
        MachineInstr *Init = nullptr;
        for (auto It = Pre->rbegin(); It != Pre->rend(); ++It) {
          if (It->isDebugInstr())
            continue;
          if (It->modifiesRegister(R, TRI) || It->readsRegister(R, TRI)) {
            if (It->getOpcode() == MCS51::MOV_RN_IMM &&
                It->getOperand(0).getReg() == R && It->getOperand(1).isImm())
              Init = &*It;
            break;
          }
        }
        if (!Init)
          continue;
        Init->getOperand(1).setImm((-Init->getOperand(1).getImm()) & 0xff);
      }
      DebugLoc DL = Term->getDebugLoc();
      MachineBasicBlock *Target = &MBB;
      BuildMI(MBB, Term, DL, TII.get(MCS51::DJNZ_RN), R).addReg(R).addMBB(Target);
      Term->eraseFromParent();
      LoadA->eraseFromParent();
      Step->eraseFromParent();
      Changed = true;
    }
    for (MachineBasicBlock &MBB : MF) {
      for (auto I = MBB.begin(); I != MBB.end(); ++I) {
        if (!isCandidate(*I))
          continue;
        int64_t Value = I->getOperand(1).getImm();
        // Gather the stores of Value up to the first instruction touching A,
        // which must overwrite A (or the block must end with A dead).
        SmallVector<MachineInstr *, 8> Group;
        bool ADead = false;
        auto J = I;
        for (; J != MBB.end(); ++J) {
          if (J->isDebugInstr())
            continue;
          if (isCandidate(*J)) {
            if (J->getOperand(1).getImm() == Value)
              Group.push_back(&*J);
            continue;
          }
          if (J->isInlineAsm() || J->readsRegister(MCS51::A, TRI) ||
              J->isCall() ||
              llvm::any_of(J->explicit_operands(),
                           [](const MachineOperand &MO) {
                             return MO.isImm() && MO.getImm() == 0xe0;
                           })) {
            ADead = J->isCall() && !J->readsRegister(MCS51::A, TRI) &&
                    J->definesRegister(MCS51::A, TRI);
            break;
          }
          if (J->modifiesRegister(MCS51::A, TRI)) {
            ADead = true;
            break;
          }
        }
        if (J == MBB.end()) {
          ADead = true;
          for (MachineBasicBlock *Succ : MBB.successors())
            ADead &= !Succ->isLiveIn(MCS51::A);
        }
        unsigned Needed = Value == 0 ? 2 : 3;
        if (!ADead || Group.size() < Needed) {
          continue;
        }
        const DebugLoc &DL = I->getDebugLoc();
        if (Value == 0)
          BuildMI(MBB, *Group.front(), DL, TII.get(MCS51::CLR_A));
        else
          BuildMI(MBB, *Group.front(), DL, TII.get(MCS51::MOV_A_IMM),
                  MCS51::A)
              .addImm(Value);
        for (MachineInstr *MI : Group) {
          Register Dst = MI->getOperand(0).getReg();
          if (MI->getOpcode() == MCS51::MOV_IM_IMM)
            BuildMI(MBB, *MI, MI->getDebugLoc(), TII.get(MCS51::MOV_IM_A),
                    Dst)
                .addReg(MCS51::A);
          else
            BuildMI(MBB, *MI, MI->getDebugLoc(), TII.get(MCS51::MOV_RN_A),
                    Dst);
          MI->eraseFromParent();
        }
        Changed = true;
        // I was erased with the group; resume after J, the instruction that
        // ended the scan (J may be the block end, which must not be
        // dereferenced).
        if (J == MBB.end())
          break;
        I = J;
      }
    }
    return Changed;
  }

  StringRef getPassName() const override {
    return "MCS-51 constant byte grouping";
  }
};
char MCS51ConstantByteGrouping::ID = 0;

class MCS51CallFramePropagation final : public MachineFunctionPass {
public:
  static char ID;
  MCS51CallFramePropagation() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    DenseMap<const MachineBasicBlock *, int64_t> Entry;
    SmallVector<MachineBasicBlock *, 16> Work;
    Entry[&MF.front()] = 0;
    Work.push_back(&MF.front());
    while (!Work.empty()) {
      MachineBasicBlock *MBB = Work.pop_back_val();
      int64_t Size = Entry[MBB];
      for (const MachineInstr &MI : *MBB) {
        if (MI.getOpcode() == MCS51::ADJCALLSTACKDOWN)
          Size = MI.getOperand(0).getImm();
        else if (MI.getOpcode() == MCS51::ADJCALLSTACKUP)
          Size = 0;
      }
      for (MachineBasicBlock *Succ : MBB->successors())
        if (Entry.try_emplace(Succ, Size).second)
          Work.push_back(Succ);
    }
    bool Changed = false;
    for (MachineBasicBlock &MBB : MF) {
      auto It = Entry.find(&MBB);
      unsigned Size = It == Entry.end() ? 0 : It->second;
      if (MBB.getCallFrameSize() != Size) {
        MBB.setCallFrameSize(Size);
        Changed = true;
      }
    }
    return Changed;
  }

  StringRef getPassName() const override {
    return "MCS-51 call frame size propagation";
  }
};
char MCS51CallFramePropagation::ID = 0;

class MCS51DirectDptrReads final : public MachineFunctionPass {
public:
  static char ID;
  MCS51DirectDptrReads() : MachineFunctionPass(ID) {}

  // DPTR is reserved, so the allocator cannot see a direct read of DPL/DPH.
  // Give each such read an implicit DPTR use, which later passes consult, and
  // optionally verify that every read follows a write of DPTR in its block.
  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;
    const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
    for (MachineBasicBlock &MBB : MF) {
      bool Defined = false;
      for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
        if (MI.isInlineAsm()) {
          // A memory operand is printed as @dptr (or @r0/@r1): move a word
          // address into DPTR in front of the statement.
          for (unsigned I = InlineAsm::MIOp_FirstOperand;
               I < MI.getNumOperands();) {
            const MachineOperand &FlagOp = MI.getOperand(I);
            if (!FlagOp.isImm()) {
              ++I;
              continue;
            }
            InlineAsm::Flag Flags(FlagOp.getImm());
            unsigned Count = Flags.getNumOperandRegisters();
            if (Flags.isMemKind() && Count == 1 && I + 1 < MI.getNumOperands()) {
              MachineOperand &Address = MI.getOperand(I + 1);
              if (Address.isReg() && Address.getReg().isVirtual() &&
                  MF.getRegInfo().getRegClass(Address.getReg()) ==
                      &MCS51::MCS51GPR16RegClass) {
                BuildMI(MBB, MI, MI.getDebugLoc(),
                        TII.get(TargetOpcode::COPY), MCS51::DPTR)
                    .addReg(Address.getReg());
                Address.setReg(MCS51::DPTR);
                Address.setIsKill(false);
                Changed = true;
              }
            }
            I += Count + 1;
          }
        }
        if (readsDptrByte(MI)) {
          if (CheckDptrReads && !Defined) {
            errs() << "DPTR-READ-WITHOUT-DEF in " << MF.getName() << " bb."
                   << MBB.getNumber() << ": ";
            MI.print(errs());
          }
          if (!MI.readsRegister(MCS51::DPTR, /*TRI=*/nullptr)) {
            MI.addOperand(MF, MachineOperand::CreateReg(
                                  MCS51::DPTR, /*isDef=*/false, /*isImp=*/true));
            Changed = true;
          }
        }
        if (MI.isCall())
          Defined = false;
        for (const MachineOperand &MO : MI.operands())
          if (MO.isReg() && MO.isDef() && !MI.isCall() &&
              (MO.getReg() == MCS51::DPTR || MO.getReg() == MCS51::DPL ||
               MO.getReg() == MCS51::DPH))
            Defined = true;
      }
    }
    return Changed;
  }

private:
  static bool hasClobber(const MachineBasicBlock &MBB) {
    for (const MachineInstr &MI : MBB)
      if (MI.isCall())
        return true;
    return false;
  }

  static bool isDptrAddress(const MachineInstr &MI, unsigned Operand) {
    return Operand < MI.getNumOperands() && MI.getOperand(Operand).isImm() &&
           (MI.getOperand(Operand).getImm() == 0x82 ||
            MI.getOperand(Operand).getImm() == 0x83);
  }

  static bool readsDptrByte(const MachineInstr &MI) {
    switch (MI.getOpcode()) {
    case MCS51::MOV_A_DIRECT:
    case MCS51::ADD_A_DIRECT:
    case MCS51::ADDC_A_DIRECT:
    case MCS51::SUBB_A_DIRECT:
    case MCS51::ANL_A_DIRECT:
    case MCS51::ORL_A_DIRECT:
    case MCS51::XRL_A_DIRECT:
    case MCS51::MOV_RN_DIRECT:
    case MCS51::XCH_A_DIRECT:
      return isDptrAddress(MI, 1);
    case MCS51::CJNE_A_DIRECT:
    case MCS51::PUSH_DIRECT:
    case MCS51::MOV_R0_IND_DIRECT:
    case MCS51::MOV_R1_IND_DIRECT:
    case MCS51::INC_DIRECT:
    case MCS51::DEC_DIRECT:
    case MCS51::DJNZ_DIRECT:
    case MCS51::ANL_DIRECT_A:
    case MCS51::ORL_DIRECT_A:
    case MCS51::XRL_DIRECT_A:
    case MCS51::ANL_DIRECT_IMM:
    case MCS51::ORL_DIRECT_IMM:
    case MCS51::XRL_DIRECT_IMM:
      return isDptrAddress(MI, 0);
    case MCS51::MOV_DIRECT_DIRECT:
      return isDptrAddress(MI, 1);
    default:
      return false;
    }
  }
};

char MCS51DirectDptrReads::ID = 0;

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
    addPass(new MCS51SinkIdataConstantStore());
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
    addPass(new MCS51OutlineXDataI32Compare());
    addPass(new MCS51NarrowByteMaskTests());
    addPass(new MCS51FoldConditionalByteAdd());
    if (getOptLevel() != CodeGenOptLevel::None) {
      // The generic -Oz pipeline leaves tiny fixed-trip loops rolled when
      // unrolling costs a few bytes. On MCS-51, the induction variable and
      // loop control often need stack slots and indirect accesses, so unroll
      // small loops whose trip counts can be proven before instruction
      // selection.
      addPass(createLoopSimplifyPass());
      addPass(createLoopUnrollPass(/*OptLevel=*/3, /*OnlyWhenForced=*/false,
                                   /*ForgetAllSCEV=*/false,
                                   /*Threshold=*/UnrollThreshold, /*Count=*/-1,
                                   /*AllowPartial=*/0, /*Runtime=*/0,
                                   /*UpperBound=*/0, /*AllowPeeling=*/0));
      addPass(createCFGSimplificationPass());
      addPass(createInstructionCombiningPass());
    }
    if (getOptLevel() == CodeGenOptLevel::None) {
      addPass(createInstructionCombiningPass());
      addPass(createCFGSimplificationPass());
    }
    return false;
  }

  void addMachineSSAOptimization() override {
    addPass(new MCS51DirectDptrReads());
    TargetPassConfig::addMachineSSAOptimization();
  }

  void addPreRegAlloc() override { addPass(new MCS51AccCopyHoisting()); }

  void addPostRegAlloc() override {
    addPass(new MCS51CallFramePropagation());
  }

  void addPostRewrite() override {
    addPass(new MCS51RedundantSpillCopyElimination());
  }

  void addPreEmitPass() override {
    addPass(new MCS51PostRAPeephole());
    addPass(new MCS51ConstantByteGrouping());
    addPass(&BranchRelaxationPassID);
    addPass(new MCS51BranchIslandSharing());
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

TargetTransformInfo
MCS51TargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(std::make_unique<MCS51TTIImpl>(this, F));
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
