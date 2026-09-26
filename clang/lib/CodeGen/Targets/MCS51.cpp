//===- MCS51.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"
#include "llvm/ADT/StringExtras.h"

using namespace clang;
using namespace clang::CodeGen;

namespace {
class MCS51TargetCodeGenInfo final : public TargetCodeGenInfo {
public:
  explicit MCS51TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<DefaultABIInfo>(CGT)) {}

  LangAS getGlobalVarAddressSpace(CodeGenModule &CGM,
                                  const VarDecl *D) const override {
    if (D && D->getType().getAddressSpace() != LangAS::Default)
      return D->getType().getAddressSpace();

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
