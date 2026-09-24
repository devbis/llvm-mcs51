add_llvm_component_library(LLVMMCS51Desc
  MCS51AsmBackend.cpp
  MCS51ELFObjectWriter.cpp
  MCS51InstPrinter.cpp
  MCS51MCCodeEmitter.cpp
  MCS51MCAsmInfo.cpp
  MCS51MCTargetDesc.cpp
  LINK_COMPONENTS MC MCS51Info Support
  ADD_TO_COMPONENT MCS51
)

add_dependencies(LLVMMCS51Desc MCS51CommonTableGen)
