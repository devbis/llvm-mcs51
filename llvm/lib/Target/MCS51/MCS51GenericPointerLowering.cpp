//===-- MCS51GenericPointerLowering.cpp - generic pointer calls ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCS51.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/ErrorHandling.h"
#include <string>

using namespace llvm;

namespace {
class MCS51GenericPointerLowering final : public FunctionPass {
public:
  static char ID;
  MCS51GenericPointerLowering() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    SmallVector<Instruction *, 16> Worklist;
    for (Instruction &I : instructions(F)) {
      if (auto *LI = dyn_cast<LoadInst>(&I)) {
        if (LI->getPointerAddressSpace() == MCS51::Generic)
          Worklist.push_back(LI);
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (SI->getPointerAddressSpace() == MCS51::Generic)
          Worklist.push_back(SI);
      }
    }

    bool Changed = false;
    for (Instruction *I : Worklist) {
      if (auto *LI = dyn_cast<LoadInst>(I)) {
        lowerLoad(*LI);
        Changed = true;
      } else {
        lowerStore(*cast<StoreInst>(I));
        Changed = true;
      }
    }
    return Changed;
  }

  StringRef getPassName() const override {
    return "MCS-51 generic pointer lowering";
  }

private:
  static SmallVector<Value *, 4> getPointerParts(IRBuilder<> &B,
                                                 Value *Pointer) {
    LLVMContext &C = B.getContext();
    Type *I8 = Type::getInt8Ty(C);
    Type *I32 = Type::getInt32Ty(C);
    Value *Bits = B.CreatePtrToInt(Pointer, I32, "gptr.bits");
    SmallVector<Value *, 4> Parts;
    for (unsigned Offset : {0u, 8u, 16u, 24u}) {
      Value *Part = Bits;
      if (Offset)
        Part = B.CreateLShr(Bits, Offset, "gptr.part.shift");
      Parts.push_back(B.CreateTrunc(Part, I8, "gptr.part"));
    }
    return Parts;
  }

  static Type *getMemoryType(Type *Ty, LLVMContext &C) {
    return Ty->isIntegerTy(1) ? Type::getInt8Ty(C) : Ty;
  }

  static StringRef getSuffix(Type *Ty) {
    if (Ty->isIntegerTy(1) || Ty->isIntegerTy(8))
      return "8";
    if (Ty->isIntegerTy(16))
      return "16";
    if (Ty->isIntegerTy(32))
      return "32";
    if (Ty->isIntegerTy(64))
      return "64";
    if (Ty->isFloatTy())
      return "f32";
    report_fatal_error("unsupported MCS-51 generic pointer access type");
  }

  static CallInst *createCall(IRBuilder<> &B, Module &M, StringRef Name,
                              Type *RetTy, ArrayRef<Value *> Args) {
    SmallVector<Type *, 12> ArgTys;
    for (Value *Arg : Args)
      ArgTys.push_back(Arg->getType());
    FunctionCallee Callee =
        M.getOrInsertFunction(Name, FunctionType::get(RetTy, ArgTys, false));
    CallInst *Call = B.CreateCall(Callee, Args);
    Call->setCallingConv(CallingConv::C);
    return Call;
  }

  static void lowerLoad(LoadInst &LI) {
    IRBuilder<> B(&LI);
    Module &M = *LI.getModule();
    LLVMContext &C = M.getContext();
    Type *MemTy = getMemoryType(LI.getType(), C);
    SmallVector<Value *, 4> Args = getPointerParts(B, LI.getPointerOperand());
    std::string Name =
        (Twine("__mcs51_gptrget") + getSuffix(LI.getType())).str();
    Type *CallTy = MemTy;
    if (getSuffix(LI.getType()) == "8")
      CallTy = Type::getInt16Ty(C);
    CallInst *Call = createCall(B, M, Name, CallTy, Args);
    if (LI.isVolatile())
      Call->setConvergent();
    else
      Call->addFnAttr(Attribute::ReadOnly);
    Call->setDebugLoc(LI.getDebugLoc());
    Value *Result = Call;
    if (CallTy != LI.getType())
      Result = B.CreateTrunc(Result, LI.getType(), "gptr.bool");
    LI.replaceAllUsesWith(Result);
    LI.eraseFromParent();
  }

  static void lowerStore(StoreInst &SI) {
    IRBuilder<> B(&SI);
    Module &M = *SI.getModule();
    LLVMContext &C = M.getContext();
    Type *I8 = Type::getInt8Ty(C);
    Type *ValueTy = SI.getValueOperand()->getType();
    Type *MemTy = getMemoryType(ValueTy, C);
    Value *Stored = SI.getValueOperand();
    if (ValueTy->isIntegerTy(1))
      Stored = B.CreateZExt(Stored, I8, "gptr.bool");
    else if (ValueTy->isFloatTy())
      Stored = B.CreateBitCast(Stored, Type::getInt32Ty(C), "gptr.float.bits");

    SmallVector<Value *, 12> Args = getPointerParts(B, SI.getPointerOperand());
    unsigned NumBytes = MemTy->getPrimitiveSizeInBits().getFixedValue() / 8;
    for (unsigned I = 0; I < NumBytes; ++I) {
      Value *Part = Stored;
      if (I)
        Part = B.CreateLShr(Stored, I * 8, "gptr.value.shift");
      Args.push_back(B.CreateTrunc(Part, I8, "gptr.value.byte"));
    }
    std::string Name = (Twine("__mcs51_gptrput") + getSuffix(ValueTy)).str();
    CallInst *Call = createCall(B, M, Name, Type::getVoidTy(C), Args);
    if (SI.isVolatile())
      Call->setConvergent();
    Call->setDebugLoc(SI.getDebugLoc());
    SI.eraseFromParent();
  }
};
} // namespace

char MCS51GenericPointerLowering::ID = 0;

namespace llvm {
FunctionPass *createMCS51GenericPointerLoweringPass() {
  return new MCS51GenericPointerLowering();
}
} // namespace llvm
