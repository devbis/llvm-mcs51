#include "MCS51TargetMachine.h"
#include "MCS51.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/Support/Compiler.h"

#define DEBUG_TYPE "mcs51-isel"

using namespace llvm;

static SDValue getMCS51AbsoluteAddress(SelectionDAG &DAG, SDValue Addr,
                                       SDLoc DL, bool AllowConstant,
                                       bool &IsGlobal) {
  int64_t Offset = 0;
  while (Addr) {
    if (Addr.getOpcode() == ISD::ADDRSPACECAST ||
        Addr.getOpcode() == ISD::BITCAST) {
      Addr = Addr.getOperand(0);
      continue;
    }
    if (Addr.getOpcode() == ISD::ADD) {
      if (auto *C = dyn_cast<ConstantSDNode>(Addr.getOperand(0))) {
        Offset += C->getSExtValue();
        Addr = Addr.getOperand(1);
        continue;
      }
      if (auto *C = dyn_cast<ConstantSDNode>(Addr.getOperand(1))) {
        Offset += C->getSExtValue();
        Addr = Addr.getOperand(0);
        continue;
      }
    }
    break;
  }

  if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr)) {
    IsGlobal = true;
    return DAG.getTargetGlobalAddress(GA->getGlobal(), DL, MVT::i16,
                                      GA->getOffset() + Offset,
                                      GA->getTargetFlags());
  }
  if (Addr.getOpcode() == ISD::TargetGlobalAddress) {
    IsGlobal = true;
    auto *GA = cast<GlobalAddressSDNode>(Addr);
    return DAG.getTargetGlobalAddress(GA->getGlobal(), DL, MVT::i16,
                                      GA->getOffset() + Offset,
                                      GA->getTargetFlags());
  }
  if (AllowConstant)
    if (auto *C = dyn_cast<ConstantSDNode>(Addr))
      return DAG.getTargetConstant(C->getZExtValue() + Offset, DL, MVT::i16);
  return SDValue();
}

namespace {
static bool getFrameAddress(SDValue Ptr, int &FI, int64_t &Offset) {
  Offset = 0;
  if (Ptr.getOpcode() == ISD::FrameIndex) {
    FI = cast<FrameIndexSDNode>(Ptr)->getIndex();
    return true;
  }
  if (Ptr.getOpcode() != ISD::ADD)
    return false;
  SDValue Base = Ptr.getOperand(0);
  SDValue Displacement = Ptr.getOperand(1);
  if (Base.getOpcode() != ISD::FrameIndex) {
    std::swap(Base, Displacement);
    if (Base.getOpcode() != ISD::FrameIndex)
      return false;
  }
  auto *C = dyn_cast<ConstantSDNode>(Displacement);
  if (!C)
    return false;
  FI = cast<FrameIndexSDNode>(Base)->getIndex();
  Offset = C->getSExtValue();
  return Offset >= -128 && Offset <= 127;
}

static bool getIndexedFrameAddress(SDValue Ptr, int &FI, SDValue &Index) {
  if (Ptr.getOpcode() != ISD::ADD)
    return false;
  SDValue Base = Ptr.getOperand(0);
  Index = Ptr.getOperand(1);
  if (Base.getOpcode() != ISD::FrameIndex) {
    std::swap(Base, Index);
    if (Base.getOpcode() != ISD::FrameIndex)
      return false;
  }
  if (isa<ConstantSDNode>(Index))
    return false;
  FI = cast<FrameIndexSDNode>(Base)->getIndex();
  return Index.getValueType() == MVT::i16;
}

class MCS51DAGToDAGISel final : public SelectionDAGISel {
public:
  MCS51DAGToDAGISel(MCS51TargetMachine &TM, CodeGenOptLevel OL)
      : SelectionDAGISel(TM, OL) {}

  void SelectCode(SDNode *N);
  bool CheckNodePredicate(SDValue Op, unsigned PredNo) const override;
  bool selectXDataMemory(SDNode *N);
  void Select(SDNode *N) override {
    SDLoc DL(N);
    if (N->getOpcode() == ISD::BUILD_PAIR &&
        N->getValueType(0) == MVT::i16) {
      SDValue Ops[] = {N->getOperand(0), N->getOperand(1)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::BUILDPAIR16, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if (N->getOpcode() == MCS51ISD::LOAD_STACK8) {
      SDValue Ops[] = {N->getOperand(1), N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::LOADSTACKARG8, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if (N->getOpcode() == MCS51ISD::LOAD_STACK16) {
      SDValue Ops[] = {N->getOperand(1), N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::LOADSTACKARG16, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if (N->getOpcode() == MCS51ISD::PUSH_ARG8) {
      SDValue Ops[] = {N->getOperand(1), N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::PUSHARG8, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if (N->getOpcode() == MCS51ISD::PUSH_ARG16) {
      SDValue Ops[] = {N->getOperand(1), N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::PUSHARG16, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if (N->getOpcode() == MCS51ISD::POP_ARG8) {
      SDValue Ops[] = {N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::POPARG8, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if (N->getOpcode() == MCS51ISD::PUSH_PAD8) {
      SDValue Ops[] = {N->getOperand(0)};
      SDNode *Res = CurDAG->getMachineNode(MCS51::PUSHPAD8, DL,
                                           N->getVTList(), Ops);
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return;
    }
    if ((N->getOpcode() == ISD::LOAD || N->getOpcode() == ISD::STORE) &&
        selectXDataMemory(N))
      return;
    SelectCode(N);
  }
};

bool MCS51DAGToDAGISel::selectXDataMemory(SDNode *N) {
  SDLoc DL(N);
  if (N->getOpcode() == ISD::LOAD) {
    auto *LD = cast<LoadSDNode>(N);
    unsigned AS = LD->getAddressSpace();
    int FI;
    int64_t Offset;
    SDValue Index;
    if (AS == MCS51::Default && LD->getMemoryVT() == MVT::i8 &&
        getIndexedFrameAddress(LD->getBasePtr(), FI, Index)) {
      SDValue Ops[] = {CurDAG->getTargetFrameIndex(FI, MVT::i16),
                       CurDAG->getTargetConstant(0, DL, MVT::i8), Index,
                       LD->getChain()};
      SDNode *Res = CurDAG->getMachineNode(MCS51::LOAD_FRAME8_INDEX, DL,
                                           N->getVTList(), Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {LD->getMemOperand()});
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
      CurDAG->RemoveDeadNode(N);
      return true;
    }
    if (AS == MCS51::Default && LD->getMemoryVT() == MVT::i8 &&
        getFrameAddress(LD->getBasePtr(), FI, Offset)) {
      SDValue Ops[] = {
          CurDAG->getTargetFrameIndex(FI, MVT::i16),
          CurDAG->getTargetConstant(Offset, DL, MVT::i8), LD->getChain()};
      SDNode *Res = CurDAG->getMachineNode(MCS51::LOAD_FRAME8, DL,
                                           N->getVTList(), Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {LD->getMemOperand()});
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
      CurDAG->RemoveDeadNode(N);
      return true;
    }
    if (AS == MCS51::Default && LD->getMemoryVT() == MVT::i16 &&
        getFrameAddress(LD->getBasePtr(), FI, Offset)) {
      SDValue Ops[] = {
          CurDAG->getTargetFrameIndex(FI, MVT::i16),
          CurDAG->getTargetConstant(Offset, DL, MVT::i8), LD->getChain()};
      SDNode *Res = CurDAG->getMachineNode(MCS51::LOAD_FRAME16, DL,
                                           N->getVTList(), Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {LD->getMemOperand()});
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
      CurDAG->RemoveDeadNode(N);
      return true;
    }
    if (AS == MCS51::Data || AS == MCS51::SFR) {
      unsigned Opcode;
      if (LD->getMemoryVT() == MVT::i8)
        Opcode = MCS51::LOADDIRECT8;
      else if (LD->getMemoryVT() == MVT::i16)
        Opcode = MCS51::LOADDIRECT16;
      else
        report_fatal_error("unsupported MCS-51 direct memory load width");
      SDValue Addr = LD->getBasePtr();
      if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr))
        Addr = CurDAG->getTargetGlobalAddress(
            GA->getGlobal(), DL, MVT::i8, GA->getOffset(), GA->getTargetFlags());
      else if (auto *C = dyn_cast<ConstantSDNode>(Addr))
        Addr = CurDAG->getTargetConstant(C->getZExtValue(), DL, MVT::i8);
      else if (Addr.getOpcode() != ISD::TargetGlobalAddress)
        return false;
      SDValue Ops[] = {Addr, LD->getChain()};
      SDNode *Res = CurDAG->getMachineNode(Opcode, DL,
                                           N->getVTList(), Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {LD->getMemOperand()});
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
      CurDAG->RemoveDeadNode(N);
      return true;
    }
    if (AS == MCS51::Bit) {
      if (LD->getMemoryVT() != MVT::i8)
        report_fatal_error("unsupported MCS-51 bit memory load width");
      SDValue Addr = LD->getBasePtr();
      if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr))
        Addr = CurDAG->getTargetGlobalAddress(
            GA->getGlobal(), DL, MVT::i8, GA->getOffset(), GA->getTargetFlags());
      else if (auto *C = dyn_cast<ConstantSDNode>(Addr))
        Addr = CurDAG->getTargetConstant(C->getZExtValue(), DL, MVT::i8);
      else if (Addr.getOpcode() != ISD::TargetGlobalAddress)
        return false;
      SDValue Ops[] = {Addr, LD->getChain()};
      SDNode *Res = CurDAG->getMachineNode(MCS51::LOADBIT8, DL,
                                           N->getVTList(), Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {LD->getMemOperand()});
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
      CurDAG->RemoveDeadNode(N);
      return true;
    }
    if ((AS == MCS51::XData || AS == MCS51::Default) &&
        (LD->getMemoryVT() == MVT::i8 || LD->getMemoryVT() == MVT::i16)) {
      bool IsGlobal = false;
      SDValue Addr = getMCS51AbsoluteAddress(
          *CurDAG, LD->getBasePtr(), DL, AS == MCS51::XData, IsGlobal);
      if (Addr && (AS == MCS51::XData || IsGlobal)) {
        unsigned Opcode = LD->getMemoryVT() == MVT::i8
                              ? MCS51::LOADXABS8
                              : MCS51::LOADXABS16;
        SDValue Ops[] = {Addr, LD->getChain()};
        SDNode *Res = CurDAG->getMachineNode(Opcode, DL, N->getVTList(), Ops);
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {LD->getMemOperand()});
        ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
        ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
        CurDAG->RemoveDeadNode(N);
        return true;
      }
      if (AS == MCS51::Default && !Addr) {
        unsigned Opcode = LD->getMemoryVT() == MVT::i8
                              ? MCS51::LOADX8
                              : MCS51::LOADX16;
        SDValue Ops[] = {LD->getBasePtr(), LD->getChain()};
        SDNode *Res = CurDAG->getMachineNode(Opcode, DL, N->getVTList(), Ops);
        CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {LD->getMemOperand()});
        ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
        ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
        CurDAG->RemoveDeadNode(N);
        return true;
      }
    }
    if (AS != MCS51::IData && AS != MCS51::PData &&
        AS != MCS51::XData && AS != MCS51::Code)
      return false;
    unsigned Opcode;
    if (LD->getMemoryVT() == MVT::i8) {
      Opcode = AS == MCS51::IData ? MCS51::LOADI8 :
               AS == MCS51::PData ? MCS51::LOADP8 :
               AS == MCS51::Code ? MCS51::LOADCODE8 : MCS51::LOADX8;
    } else if (LD->getMemoryVT() == MVT::i16 && AS == MCS51::IData) {
      Opcode = MCS51::LOADI16;
    } else if (LD->getMemoryVT() == MVT::i16 && AS == MCS51::PData) {
      Opcode = MCS51::LOADP16;
    } else if (LD->getMemoryVT() == MVT::i16 && AS == MCS51::XData) {
      Opcode = MCS51::LOADX16;
    } else if (LD->getMemoryVT() == MVT::i16 && AS == MCS51::Code) {
      Opcode = MCS51::LOADCODE16;
    } else {
      report_fatal_error("unsupported MCS-51 memory load width/address space");
    }
    SDValue Ops[] = {LD->getBasePtr(), LD->getChain()};
    SDNode *Res = CurDAG->getMachineNode(Opcode, DL, N->getVTList(), Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {LD->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    ReplaceUses(SDValue(N, 1), SDValue(Res, 1));
    CurDAG->RemoveDeadNode(N);
    return true;
  }

  auto *ST = cast<StoreSDNode>(N);
  unsigned AS = ST->getAddressSpace();
  if (AS == MCS51::Code)
    report_fatal_error("cannot store to MCS-51 code memory");
  if (AS == MCS51::Bit) {
    if (ST->getMemoryVT() != MVT::i8)
      report_fatal_error("unsupported MCS-51 bit memory store width");
    SDValue Addr = ST->getBasePtr();
    if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr))
      Addr = CurDAG->getTargetGlobalAddress(
          GA->getGlobal(), DL, MVT::i8, GA->getOffset(), GA->getTargetFlags());
    else if (auto *C = dyn_cast<ConstantSDNode>(Addr))
      Addr = CurDAG->getTargetConstant(C->getZExtValue(), DL, MVT::i8);
    else if (Addr.getOpcode() != ISD::TargetGlobalAddress)
      return false;
    if (auto *C = dyn_cast<ConstantSDNode>(ST->getValue())) {
      unsigned Opcode = (C->getZExtValue() & 1) ? MCS51::SETB_BIT
                                                : MCS51::CLR_BIT;
      SDValue Ops[] = {Addr, ST->getChain()};
      SDNode *Res = CurDAG->getMachineNode(Opcode, DL, MVT::Other, Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return true;
    }
    SDValue Ops[] = {Addr, ST->getValue(), ST->getChain()};
    SDNode *Res = CurDAG->getMachineNode(MCS51::STOREBIT8, DL, MVT::Other,
                                         Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    CurDAG->RemoveDeadNode(N);
    return true;
  }
  int FI;
  int64_t Offset;
  SDValue Index;
  if (AS == MCS51::Default && ST->getMemoryVT() == MVT::i8 &&
      getIndexedFrameAddress(ST->getBasePtr(), FI, Index)) {
    SDValue Ops[] = {CurDAG->getTargetFrameIndex(FI, MVT::i16),
                     CurDAG->getTargetConstant(0, DL, MVT::i8), Index,
                     ST->getValue(), ST->getChain()};
    SDNode *Res = CurDAG->getMachineNode(MCS51::STORE_FRAME8_INDEX, DL,
                                         MVT::Other, Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    CurDAG->RemoveDeadNode(N);
    return true;
  }
  if (AS == MCS51::Default && ST->getMemoryVT() == MVT::i8 &&
      getFrameAddress(ST->getBasePtr(), FI, Offset)) {
    SDValue Ops[] = {CurDAG->getTargetFrameIndex(FI, MVT::i16),
                     CurDAG->getTargetConstant(Offset, DL, MVT::i8),
                     ST->getValue(), ST->getChain()};
    SDNode *Res = CurDAG->getMachineNode(MCS51::STORE_FRAME8, DL,
                                         MVT::Other, Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    CurDAG->RemoveDeadNode(N);
    return true;
  }
  if (AS == MCS51::Default && ST->getMemoryVT() == MVT::i16 &&
      getFrameAddress(ST->getBasePtr(), FI, Offset)) {
    SDValue Ops[] = {CurDAG->getTargetFrameIndex(FI, MVT::i16),
                     CurDAG->getTargetConstant(Offset, DL, MVT::i8),
                     ST->getValue(), ST->getChain()};
    SDNode *Res = CurDAG->getMachineNode(MCS51::STORE_FRAME16, DL,
                                         MVT::Other, Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    CurDAG->RemoveDeadNode(N);
    return true;
  }
  if (AS == MCS51::Data || AS == MCS51::SFR) {
    unsigned Opcode;
    if (ST->getMemoryVT() == MVT::i8)
      Opcode = MCS51::STOREDIRECT8;
    else if (ST->getMemoryVT() == MVT::i16)
      Opcode = MCS51::STOREDIRECT16;
    else
      report_fatal_error("unsupported MCS-51 direct memory store width");
    SDValue Addr = ST->getBasePtr();
    if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr))
      Addr = CurDAG->getTargetGlobalAddress(
          GA->getGlobal(), DL, MVT::i8, GA->getOffset(), GA->getTargetFlags());
    else if (auto *C = dyn_cast<ConstantSDNode>(Addr))
      Addr = CurDAG->getTargetConstant(C->getZExtValue(), DL, MVT::i8);
    else if (Addr.getOpcode() != ISD::TargetGlobalAddress)
      return false;
    SDValue Ops[] = {Addr, ST->getValue(), ST->getChain()};
    SDNode *Res = CurDAG->getMachineNode(Opcode, DL,
                                         MVT::Other, Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    CurDAG->RemoveDeadNode(N);
    return true;
  }
  if ((AS == MCS51::XData || AS == MCS51::Default) &&
      (ST->getMemoryVT() == MVT::i8 || ST->getMemoryVT() == MVT::i16)) {
    bool IsGlobal = false;
    SDValue Addr = getMCS51AbsoluteAddress(
        *CurDAG, ST->getBasePtr(), DL, AS == MCS51::XData, IsGlobal);
    if (Addr && (AS == MCS51::XData || IsGlobal)) {
      unsigned Opcode = ST->getMemoryVT() == MVT::i8
                            ? MCS51::STOREXABS8
                            : MCS51::STOREXABS16;
      SDValue Ops[] = {Addr, ST->getValue(), ST->getChain()};
      SDNode *Res = CurDAG->getMachineNode(Opcode, DL, MVT::Other, Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return true;
    }
    if (AS == MCS51::Default && !Addr) {
      unsigned Opcode = ST->getMemoryVT() == MVT::i8
                            ? MCS51::STOREX8
                            : MCS51::STOREX16;
      SDValue Ops[] = {ST->getBasePtr(), ST->getValue(), ST->getChain()};
      SDNode *Res = CurDAG->getMachineNode(Opcode, DL, MVT::Other, Ops);
      CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
      ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
      CurDAG->RemoveDeadNode(N);
      return true;
    }
  }
  if (AS != MCS51::IData && AS != MCS51::PData && AS != MCS51::XData)
    return false;
  unsigned Opcode;
  if (ST->getMemoryVT() == MVT::i8)
    Opcode = AS == MCS51::IData ? MCS51::STOREI8 :
             AS == MCS51::PData ? MCS51::STOREP8 : MCS51::STOREX8;
  else if (ST->getMemoryVT() == MVT::i16 && AS == MCS51::IData)
    Opcode = MCS51::STOREI16;
  else if (ST->getMemoryVT() == MVT::i16 && AS == MCS51::PData)
    Opcode = MCS51::STOREP16;
  else
    report_fatal_error("unsupported MCS-51 data memory store width");
  if (AS == MCS51::XData) {
    SDValue Ops[] = {ST->getBasePtr(), ST->getValue(), ST->getChain()};
    SDNode *Res = CurDAG->getMachineNode(Opcode, DL, MVT::Other, Ops);
    CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
    ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
    CurDAG->RemoveDeadNode(N);
    return true;
  }
  SDValue Ops[] = {ST->getBasePtr(), ST->getValue(), ST->getChain()};
  SDNode *Res = CurDAG->getMachineNode(Opcode, DL, MVT::Other, Ops);
  CurDAG->setNodeMemRefs(cast<MachineSDNode>(Res), {ST->getMemOperand()});
  ReplaceUses(SDValue(N, 0), SDValue(Res, 0));
  CurDAG->RemoveDeadNode(N);
  return true;
}

class MCS51DAGToDAGISelLegacy final : public SelectionDAGISelLegacy {
public:
  static char ID;
  MCS51DAGToDAGISelLegacy(MCS51TargetMachine &TM, CodeGenOptLevel OL)
      : SelectionDAGISelLegacy(
            ID, std::make_unique<MCS51DAGToDAGISel>(TM, OL)) {}
};
} // namespace

#define GET_DAGISEL_BODY MCS51DAGToDAGISel
#include "MCS51GenDAGISel.inc"

char MCS51DAGToDAGISelLegacy::ID = 0;

INITIALIZE_PASS(MCS51DAGToDAGISelLegacy, "mcs51-isel",
                "MCS-51 DAG->DAG Instruction Selection", false, false)

FunctionPass *llvm::createMCS51ISelDag(MCS51TargetMachine &TM,
                                       CodeGenOptLevel OL) {
  return new MCS51DAGToDAGISelLegacy(TM, OL);
}
