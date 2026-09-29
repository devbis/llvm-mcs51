add_llvm_component_group(MCS51)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

# Keep the target as a selectable LLVM component while the code generator is
# built up. This component will eventually be superseded by MCS51CodeGen.
add_llvm_component_library(LLVMMCS51
  MCS51.cpp
  PARTIAL_SOURCES_INTENDED
  LINK_COMPONENTS Support TargetParser
  ADD_TO_COMPONENT MCS51
)

set(LLVM_TARGET_DEFINITIONS MCS51.td)

tablegen(LLVM MCS51GenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM MCS51GenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM MCS51GenCallingConv.inc -gen-callingconv)
tablegen(LLVM MCS51GenDAGISel.inc -gen-dag-isel)
tablegen(LLVM MCS51GenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM MCS51GenInstrInfo.inc -gen-instr-info)
tablegen(LLVM MCS51GenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM MCS51GenRegisterInfo.inc -gen-register-info)
tablegen(LLVM MCS51GenSDNodeInfo.inc -gen-sd-node-info)
tablegen(LLVM MCS51GenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(MCS51CommonTableGen)

set_source_files_properties(mcs51-runtime.c PROPERTIES HEADER_FILE_ONLY TRUE)

add_llvm_target(MCS51CodeGen
  MCS51AsmPrinter.cpp
  MCS51FrameLowering.cpp
  MCS51GenericPointerLowering.cpp
  MCS51Overlay.cpp
  MCS51InstrInfo.cpp
  MCS51ISelDAGToDAG.cpp
  MCS51ISelLowering.cpp
  MCS51RegisterInfo.cpp
  MCS51SelectionDAGInfo.cpp
  MCS51StackAddressLowering.cpp
  MCS51Subtarget.cpp
  MCS51TargetMachine.cpp
  mcs51-runtime.c

  DEPENDS
  MCS51CommonTableGen

  LINK_COMPONENTS
  AsmPrinter
  CodeGen
  Core
  MC
  InstCombine
  MCS51Desc
  MCS51Info
  SelectionDAG
  Scalar
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  MCS51
)

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
