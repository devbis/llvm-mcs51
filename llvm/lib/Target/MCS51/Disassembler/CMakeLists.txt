add_llvm_component_library(LLVMMCS51Disassembler
  MCS51Disassembler.cpp
  LINK_COMPONENTS MC MCDisassembler MCS51Info Support
  ADD_TO_COMPONENT MCS51
)

add_dependencies(LLVMMCS51Disassembler MCS51CommonTableGen)
