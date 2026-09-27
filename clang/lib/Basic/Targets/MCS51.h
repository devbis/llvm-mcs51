//===--- MCS51.h - MCS-51 target feature support ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_MCS51_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_MCS51_H

#include "clang/Basic/TargetInfo.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY MCS51TargetInfo : public TargetInfo {
  bool IsCC2530;

public:
  MCS51TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : TargetInfo(Triple), IsCC2530(Opts.CPU == "cc2530") {
    TLSSupported = false;
    PointerWidth = 16;
    PointerAlign = 8;
    IntWidth = 16;
    IntAlign = 8;
    LongWidth = 32;
    LongAlign = 8;
    LongLongWidth = 64;
    LongLongAlign = 8;
    FloatWidth = 32;
    FloatAlign = 8;
    DoubleWidth = LongDoubleWidth = 32;
    DoubleAlign = LongDoubleAlign = 8;
    DoubleFormat = LongDoubleFormat = &llvm::APFloat::IEEEsingle();
    SizeType = UnsignedInt;
    PtrDiffType = SignedInt;
    IntPtrType = SignedInt;
    SigAtomicType = SignedChar;
    // AS0 is a near 16-bit pointer. The explicitly qualified memory spaces
    // use the native width of their 8051 address bus.
    resetDataLayout("e-p:16:8-p1:8:8-p2:8:8-p3:8:8-p4:16:8-p5:16:8-"
                    "p6:8:8-p7:8:8-i1:8-i8:8-i16:8-i32:8-i64:8-"
                    "f32:8-f64:8-n8:16");
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override {
    return {};
  }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::MCS51BuiltinVaList;
  }

  bool isValidCPUName(StringRef Name) const override {
    return Name == "generic" || Name == "cc2530";
  }
  bool setCPU(StringRef Name) override {
    if (!isValidCPUName(Name))
      return false;
    IsCC2530 = Name == "cc2530";
    return true;
  }
  void fillValidCPUList(SmallVectorImpl<StringRef> &Values) const override {
    Values.push_back("generic");
    Values.push_back("cc2530");
  }

  bool allowsLargerPreferedTypeAlignment() const override { return false; }
  bool hasFeature(StringRef Feature) const override {
    return Feature == "mcs51" || Feature == "8051";
  }
  std::string_view getClobbers() const override { return ""; }
  ArrayRef<const char *> getGCCRegNames() const override {
    static const char *const Names[] = {"a", "b", "c", "dptr", "dpl",
                                        "dph", "sp", "psw", "pc", "r0",
                                        "r1", "r2", "r3", "r4", "r5",
                                        "r6", "r7"};
    return Names;
  }
  ArrayRef<GCCRegAlias> getGCCRegAliases() const override { return {}; }
  bool validateAsmConstraint(const char *&Name,
                             ConstraintInfo &Info) const override {
    switch (*Name) {
    case 'r': // Register-bank registers R0-R7, or DPTR for 16-bit operands.
      Info.setAllowsRegister();
      return true;
    default:
      return false;
    }
  }
};

} // namespace targets
} // namespace clang

#endif
