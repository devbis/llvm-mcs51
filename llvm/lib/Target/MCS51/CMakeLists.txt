add_llvm_component_group(MCS51)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

# Keep the target as a selectable LLVM component while the code generator is
# built up. This component will eventually be superseded by MCS51CodeGen.
add_llvm_component_library(LLVMMCS51
  MCS51.cpp
  LINK_COMPONENTS Support TargetParser
  ADD_TO_COMPONENT MCS51
)

set(LLVM_TARGET_DEFINITIONS MCS51.td)

tablegen(LLVM MCS51GenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM MCS51GenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM MCS51GenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM MCS51GenInstrInfo.inc -gen-instr-info)
tablegen(LLVM MCS51GenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM MCS51GenRegisterInfo.inc -gen-register-info)
tablegen(LLVM MCS51GenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(MCS51CommonTableGen)

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
