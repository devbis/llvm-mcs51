//===-- MCS51CodeConstantEscape.cpp - code constants seen as data ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Explicit CODE (address space 5) objects are read with MOVC. Their address
// can still be converted to the default/XDATA pointer type, for example when
// a constant table is passed to memcpy. A default pointer is read with MOVX,
// so a CODE address would read unrelated XDATA.
//
// This pass gives every such conversion an initialized copy of the constant
// object in the default (XDATA) pointer space and rewrites the conversion to
// address the copy. Direct
// CODE accesses keep using the CODE object, and conversions to tagged generic
// pointers are left alone because they preserve the memory space. Conversions
// whose CODE object cannot be identified are rejected.
//
//===----------------------------------------------------------------------===//

#include "MCS51.h"
#include "MCS51TargetMachine.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

namespace {
class MCS51CodeConstantEscape final : public ModulePass {
public:
  static char ID;
  MCS51CodeConstantEscape() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return "MCS-51 code constant escape to data pointers";
  }

  bool runOnModule(Module &M) override {
    SmallVector<ConstantExpr *, 8> ConstantCasts;
    SmallVector<WeakTrackingVH, 8> InstructionCasts;
    collectConstantCasts(M, ConstantCasts);
    for (Function &F : M)
      for (Instruction &I : instructions(F))
        if (auto *Cast = dyn_cast<AddrSpaceCastInst>(&I))
          if (needsDataCopy(Cast->getSrcAddressSpace(),
                            Cast->getDestAddressSpace(), &F, M))
            InstructionCasts.emplace_back(Cast);

    if (ConstantCasts.empty() && InstructionCasts.empty())
      return false;

    bool Changed = false;
    for (ConstantExpr *Cast : ConstantCasts)
      Changed |= rewriteCodeLoads(Cast, Cast->getOperand(0));
    for (WeakTrackingVH &CastHandle : InstructionCasts)
      if (auto *Cast = dyn_cast_or_null<AddrSpaceCastInst>(CastHandle))
        Changed |= rewriteCodeLoads(Cast, Cast->getPointerOperand());

    const DataLayout &DL = M.getDataLayout();
    for (ConstantExpr *Cast : ConstantCasts) {
      Cast->removeDeadConstantUsers();
      if (Cast->use_empty())
        continue;
      Constant *Source = Cast->getOperand(0);
      APInt Offset(DL.getIndexTypeSizeInBits(Source->getType()), 0);
      const Value *Base =
          Source->stripAndAccumulateConstantOffsets(DL, Offset, true);
      GlobalVariable *Copy = getDataCopy(M, Base, nullptr);
      if (!Copy)
        continue;
      Constant *Address = Copy;
      if (!Offset.isZero())
        Address = ConstantExpr::getGetElementPtr(
            Type::getInt8Ty(M.getContext()), Copy,
            ConstantInt::get(M.getContext(), Offset));
      if (Address->getType() != Cast->getType())
        Address = ConstantExpr::getAddrSpaceCast(Address, Cast->getType());
      Cast->replaceAllUsesWith(Address);
      Changed = true;
    }

    for (WeakTrackingVH &CastHandle : InstructionCasts) {
      auto *Cast = dyn_cast_or_null<AddrSpaceCastInst>(CastHandle);
      if (!Cast)
        continue;
      if (Cast->use_empty()) {
        Cast->eraseFromParent();
        continue;
      }
      Value *Source = Cast->getPointerOperand();
      Value *Base = getUnderlyingObject(Source, 0);
      GlobalVariable *Copy = getDataCopy(M, Base, Cast->getFunction());
      if (!Copy)
        continue;
      IRBuilder<> B(Cast);
      Type *IndexType = DL.getIndexType(Source->getType());
      Value *Offset = B.CreateSub(B.CreatePtrToInt(Source, IndexType),
                                  B.CreatePtrToInt(Base, IndexType),
                                  "code.copy.offset");
      Value *Address = B.CreateGEP(B.getInt8Ty(), Copy, Offset,
                                   Cast->getName() + ".xdata");
      if (Address->getType() != Cast->getType())
        Address = B.CreateAddrSpaceCast(Address, Cast->getType());
      Cast->replaceAllUsesWith(Address);
      Cast->eraseFromParent();
      Changed = true;
    }

    for (auto &Entry : Copies) {
      GlobalVariable *Original = Entry.first;
      Original->removeDeadConstantUsers();
      if (Original->use_empty() && Original->hasLocalLinkage())
        Original->eraseFromParent();
    }
    Copies.clear();
    return Changed;
  }

private:
  DenseMap<GlobalVariable *, GlobalVariable *> Copies;

  // Rebuilds Pointer, a value derived from the converted address Cast only
  // through GEPs, as the same address in CODE.
  static Value *getCodeAddress(Value *Pointer, Value *Cast, Value *CodeSource,
                               DenseMap<Value *, Value *> &CodeAddresses) {
    if (Pointer == Cast)
      return CodeSource;
    if (Value *Known = CodeAddresses.lookup(Pointer))
      return Known;
    auto *GEP = cast<GEPOperator>(Pointer);
    Value *Base = getCodeAddress(GEP->getPointerOperand(), Cast, CodeSource,
                                 CodeAddresses);
    SmallVector<Value *, 4> Indices(GEP->indices());
    Value *Result;
    if (auto *Instruction = dyn_cast<GetElementPtrInst>(GEP)) {
      IRBuilder<> B(Instruction);
      Result = B.CreateGEP(GEP->getSourceElementType(), Base, Indices,
                           Instruction->getName() + ".code",
                           GEP->getNoWrapFlags());
    } else {
      SmallVector<Constant *, 4> ConstantIndices;
      for (Value *Index : Indices)
        ConstantIndices.push_back(cast<Constant>(Index));
      Result = ConstantExpr::getGetElementPtr(
          GEP->getSourceElementType(), cast<Constant>(Base), ConstantIndices,
          GEP->getNoWrapFlags());
    }
    CodeAddresses[Pointer] = Result;
    return Result;
  }

  // Turns loads through the converted address into CODE loads. Returns true
  // if any load was rewritten.
  static bool rewriteCodeLoads(Value *Cast, Value *CodeSource) {
    SmallVector<LoadInst *, 8> Loads;
    SmallVector<Value *, 8> Worklist(1, Cast);
    SmallPtrSet<Value *, 8> Seen;
    while (!Worklist.empty()) {
      Value *Pointer = Worklist.pop_back_val();
      if (!Seen.insert(Pointer).second)
        continue;
      for (User *U : Pointer->users()) {
        if (auto *Load = dyn_cast<LoadInst>(U)) {
          if (Load->getPointerOperand() == Pointer && !Load->isAtomic())
            Loads.push_back(Load);
        } else if (auto *GEP = dyn_cast<GEPOperator>(U)) {
          if (GEP->getPointerOperand() == Pointer)
            Worklist.push_back(GEP);
        }
      }
    }
    if (Loads.empty())
      return false;

    DenseMap<Value *, Value *> CodeAddresses;
    SmallVector<WeakTrackingVH, 8> OldPointers;
    for (LoadInst *Load : Loads) {
      Value *Pointer = Load->getPointerOperand();
      Value *CodePointer =
          getCodeAddress(Pointer, Cast, CodeSource, CodeAddresses);
      auto *CodeLoad =
          new LoadInst(Load->getType(), CodePointer, Load->getName(),
                       Load->isVolatile(), Load->getAlign(),
                       Load->getIterator());
      CodeLoad->copyMetadata(*Load);
      CodeLoad->setDebugLoc(Load->getDebugLoc());
      Load->replaceAllUsesWith(CodeLoad);
      Load->eraseFromParent();
      OldPointers.emplace_back(Pointer);
    }
    for (WeakTrackingVH &PointerHandle : OldPointers)
      if (Value *Pointer = PointerHandle)
        RecursivelyDeleteTriviallyDeadInstructions(Pointer);
    return true;
  }

  static bool isDataPointerSpace(unsigned AddressSpace) {
    return AddressSpace == MCS51::Default || AddressSpace == MCS51::XData;
  }

  // Returns true for conversions from CODE that must be redirected to an XDATA
  // copy. Conversions into other non-generic spaces cannot be represented and
  // are diagnosed.
  static bool needsDataCopy(unsigned From, unsigned To, const Function *F,
                            Module &M) {
    if (From != MCS51::Code || To == MCS51::Code || To == MCS51::Generic)
      return false;
    if (isDataPointerSpace(To))
      return true;
    diagnose(M, F,
             "MCS-51 cannot convert a CODE address to a DATA, IDATA, PDATA, "
             "BIT or SFR pointer");
    return false;
  }

  static void diagnose(Module &M, const Function *F, const Twine &Message) {
    if (F)
      M.getContext().diagnose(DiagnosticInfoUnsupported(*F, Message));
    else
      M.getContext().emitError(Message);
  }

  static void collectConstantCasts(Module &M,
                                   SmallVectorImpl<ConstantExpr *> &Casts) {
    SmallVector<Constant *, 16> Worklist;
    SmallPtrSet<Constant *, 16> Seen;
    for (GlobalVariable &GV : M.globals())
      if (GV.getAddressSpace() == MCS51::Code)
        Worklist.push_back(&GV);
    while (!Worklist.empty()) {
      Constant *C = Worklist.pop_back_val();
      for (User *U : C->users()) {
        auto *CE = dyn_cast<ConstantExpr>(U);
        if (!CE || !Seen.insert(CE).second)
          continue;
        if (CE->getOpcode() == Instruction::AddrSpaceCast) {
          if (needsDataCopy(
                  CE->getOperand(0)->getType()->getPointerAddressSpace(),
                  CE->getType()->getPointerAddressSpace(), nullptr, M))
            Casts.push_back(CE);
          continue;
        }
        Worklist.push_back(CE);
      }
    }
  }

  GlobalVariable *getDataCopy(Module &M, const Value *Base,
                              const Function *F) {
    auto *Original = dyn_cast_or_null<GlobalVariable>(
        const_cast<Value *>(Base));
    if (!Original || Original->getAddressSpace() != MCS51::Code) {
      diagnose(M, F,
               "MCS-51 cannot identify the CODE object of an address used as "
               "an XDATA pointer; use a generic pointer or copy it explicitly");
      return nullptr;
    }
    if (!Original->hasDefinitiveInitializer()) {
      diagnose(M, F,
               "MCS-51 CODE object '" + Original->getName() +
                   "' is used as an XDATA pointer but its initializer is not "
                   "available in this translation unit");
      return nullptr;
    }

    GlobalVariable *&Copy = Copies[Original];
    if (Copy)
      return Copy;
    Copy = new GlobalVariable(
        M, Original->getValueType(), /*isConstant=*/true,
        GlobalValue::InternalLinkage, Original->getInitializer(),
        Original->getName() + ".xdata", nullptr,
        GlobalVariable::NotThreadLocal, MCS51::Default);
    Copy->setAlignment(Original->getAlign());
    Copy->setUnnamedAddr(Original->getUnnamedAddr());
    return Copy;
  }
};

char MCS51CodeConstantEscape::ID = 0;
} // namespace

ModulePass *llvm::createMCS51CodeConstantEscapePass() {
  return new MCS51CodeConstantEscape();
}
