#include "MCS51SelectionDAGInfo.h"
#include "MCS51.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/SelectionDAG.h"

#define GET_SDNODE_DESC
#include "MCS51GenSDNodeInfo.inc"
#include "llvm/Support/Casting.h"

using namespace llvm;

MCS51SelectionDAGInfo::MCS51SelectionDAGInfo()
    : SelectionDAGGenTargetInfo(MCS51GenSDNodeInfo) {}

MCS51SelectionDAGInfo::~MCS51SelectionDAGInfo() = default;

static bool containsFrameIndex(SDValue Value) {
  SmallVector<SDValue, 8> Worklist(1, Value);
  while (!Worklist.empty()) {
    SDValue Current = Worklist.pop_back_val();
    if (Current.getOpcode() == ISD::FrameIndex)
      return true;
    if (Current.getOpcode() == ISD::ADD ||
        Current.getOpcode() == ISD::BITCAST ||
        Current.getOpcode() == ISD::ADDRSPACECAST)
      for (SDValue Operand : Current->ops())
        Worklist.push_back(Operand);
  }
  return false;
}

static bool usesAddressSpaceAwareMemcpy(unsigned AddressSpace) {
  switch (AddressSpace) {
  case MCS51::Data:
  case MCS51::IData:
  case MCS51::PData:
  case MCS51::Code:
  case MCS51::SFR:
    return true;
  default:
    return false;
  }
}

SDValue MCS51SelectionDAGInfo::EmitTargetCodeForMemcpy(
    SelectionDAG &DAG, const SDLoc &DL, SDValue Chain, SDValue Dst,
    SDValue Src, SDValue Size, Align DstAlign, Align SrcAlign,
    bool IsVolatile, bool AlwaysInline, MachinePointerInfo DstPtrInfo,
    MachinePointerInfo SrcPtrInfo) const {
  auto *ConstantSize = dyn_cast<ConstantSDNode>(Size);
  if (!ConstantSize)
    return SDValue();

  uint64_t Count = ConstantSize->getZExtValue();
  bool NeedsAddressSpaceAwareCopy =
      usesAddressSpaceAwareMemcpy(SrcPtrInfo.getAddrSpace()) ||
      usesAddressSpaceAwareMemcpy(DstPtrInfo.getAddrSpace());

  // Clang represents an MCS-51 stack object as an alloca in AS0 and casts its
  // pointer to IDATA for ABI uses. LLVM's generic memcpy lowering casts that
  // pointer back to AS0; recover the actual bus before constructing memory
  // operations, or the copy would incorrectly use MOVX instead of @Ri.
  if (Dst.getOpcode() == ISD::ADDRSPACECAST) {
    auto *Cast = cast<AddrSpaceCastSDNode>(Dst);
    if (usesAddressSpaceAwareMemcpy(Cast->getSrcAddressSpace())) {
      NeedsAddressSpaceAwareCopy = true;
      Dst = Dst.getOperand(0);
      DstPtrInfo = MachinePointerInfo(Cast->getSrcAddressSpace());
    }
  }
  if (Src.getOpcode() == ISD::ADDRSPACECAST) {
    auto *Cast = cast<AddrSpaceCastSDNode>(Src);
    if (usesAddressSpaceAwareMemcpy(Cast->getSrcAddressSpace())) {
      NeedsAddressSpaceAwareCopy = true;
      Src = Src.getOperand(0);
      SrcPtrInfo = MachinePointerInfo(Cast->getSrcAddressSpace());
    }
  }
  bool HasStackObject = containsFrameIndex(Dst) || containsFrameIndex(Src);
  if (!AlwaysInline && !NeedsAddressSpaceAwareCopy && !HasStackObject)
    return SDValue();

  MachineMemOperand::Flags Flags =
      IsVolatile ? MachineMemOperand::MOVolatile : MachineMemOperand::MONone;
  for (uint64_t Offset = 0; Offset < Count; ++Offset) {
    SDValue SrcOffset = DAG.getObjectPtrOffset(
        DL, Src, TypeSize::getFixed(Offset));
    SDValue DstOffset = DAG.getObjectPtrOffset(
        DL, Dst, TypeSize::getFixed(Offset));
    SDValue Load = DAG.getLoad(MVT::i8, DL, Chain, SrcOffset,
                               SrcPtrInfo.getWithOffset(Offset), Align(1),
                               Flags);
    Chain = DAG.getStore(Load.getValue(1), DL, Load, DstOffset,
                         DstPtrInfo.getWithOffset(Offset), Align(1), Flags);
  }
  return Chain;
}
