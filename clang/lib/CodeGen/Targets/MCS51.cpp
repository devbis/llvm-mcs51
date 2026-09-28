//===- MCS51.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"

using namespace clang;
using namespace clang::CodeGen;

namespace {
class MCS51ABIInfo final : public DefaultABIInfo {
  bool canExpandAggregate(QualType Ty) const {
    Ty = Ty.getCanonicalType();
    if (Ty->isScalarType())
      return true;
    if (const auto *ArrayTy = getContext().getAsConstantArrayType(Ty))
      return canExpandAggregate(ArrayTy->getElementType());

    const auto *RecordTy = Ty->getAs<RecordType>();
    if (!RecordTy || (!RecordTy->getDecl()->isStruct() &&
                      !RecordTy->getDecl()->isUnion()))
      return false;
    for (const FieldDecl *Field : RecordTy->getDecl()->fields())
      if (Field->isBitField() || !canExpandAggregate(Field->getType()))
        return false;
    return true;
  }

  llvm::Type *getAggregateCoerceType(QualType Ty) const {
    uint64_t Size = getContext().getTypeSize(Ty);
    if (!Size || Size > 64)
      return nullptr;

    unsigned Width = 8;
    while (Width < Size)
      Width *= 2;
    return llvm::IntegerType::get(getVMContext(), Width);
  }

  ABIArgInfo coerceAggregateToWords(QualType Ty) const {
    // Keep bit-field layout intact by coercing the object representation
    // instead of trying to expand individual C fields. i16 chunks match the
    // MCS-51 argument stack ABI; a final odd byte remains an i8 argument.
    auto *I8Ty = llvm::Type::getInt8Ty(getVMContext());
    auto *I16Ty = llvm::Type::getInt16Ty(getVMContext());
    unsigned NumBytes = getContext().getTypeSizeInChars(Ty).getQuantity();
    llvm::SmallVector<llvm::Type *, 8> Elements;
    while (NumBytes >= 2) {
      Elements.push_back(I16Ty);
      NumBytes -= 2;
    }
    if (NumBytes)
      Elements.push_back(I8Ty);
    auto *CoerceTy = llvm::StructType::get(getVMContext(), Elements,
                                           /*isPacked=*/true);
    llvm::Type *UnpaddedTy = CoerceTy;
    if (Elements.size() == 1)
      UnpaddedTy = Elements.front();
    return ABIArgInfo::getCoerceAndExpand(CoerceTy, UnpaddedTy);
  }

public:
  explicit MCS51ABIInfo(CodeGen::CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  ABIArgInfo classifyReturnType(QualType RetTy) const {
    if (isAggregateTypeForABI(RetTy)) {
      if (llvm::Type *CoerceTy = getAggregateCoerceType(RetTy))
        return ABIArgInfo::getDirect(CoerceTy);
      if (!getRecordArgABI(RetTy, getCXXABI()))
        return ABIArgInfo::getIndirect(
            getContext().getTypeAlignInChars(RetTy),
            /*AddrSpace=*/2, /*ByVal=*/false);
    }
    return DefaultABIInfo::classifyReturnType(RetTy);
  }

  ABIArgInfo classifyArgumentType(QualType Ty) const {
    Ty = useFirstFieldIfTransparentUnion(Ty);
    if (isAggregateTypeForABI(Ty)) {
      if (Ty->getAs<RecordType>() && getRecordArgABI(Ty, getCXXABI()))
        return DefaultABIInfo::classifyArgumentType(Ty);
      if (llvm::Type *CoerceTy = getAggregateCoerceType(Ty))
        return ABIArgInfo::getDirect(CoerceTy);
      if (canExpandAggregate(Ty))
        return ABIArgInfo::getExpand();
      return coerceAggregateToWords(Ty);
    }
    return DefaultABIInfo::classifyArgumentType(Ty);
  }

  void computeInfo(CGFunctionInfo &FI) const override {
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
    for (auto &I : FI.arguments())
      I.info = classifyArgumentType(I.type);
  }
};

class MCS51TargetCodeGenInfo final : public TargetCodeGenInfo {
public:
  explicit MCS51TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<MCS51ABIInfo>(CGT)) {}

  LangAS getGlobalVarAddressSpace(CodeGenModule &CGM,
                                  const VarDecl *D) const override {
    if (D && D->getType().getAddressSpace() != LangAS::Default)
      return D->getType().getAddressSpace();

    if (D && D->getType().isConstQualified())
      return getLangASFromTargetAS(5);

    // The default MCS-51 data model places unqualified globals in XDATA.
    // Explicit address-space-qualified variables keep their declared space.
    return getLangASFromTargetAS(4);
  }

  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &M) const override {
    if (GV->isDeclaration())
      return;
    const auto *FD = dyn_cast_or_null<FunctionDecl>(D);
    const auto *InterruptAttr = FD ? FD->getAttr<MCS51InterruptAttr>() : nullptr;
    if (!InterruptAttr)
      return;
    auto *F = cast<llvm::Function>(GV);
    F->addFnAttr(llvm::Attribute::NoInline);
    F->addFnAttr("interrupt", llvm::utostr(InterruptAttr->getNumber()));
  }
};
} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createMCS51TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<MCS51TargetCodeGenInfo>(CGM.getTypes());
}
