#include "MCS51RegisterInfo.h"
#include "MCS51.h"
#include "MCTargetDesc/MCS51MCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "MCS51GenRegisterInfo.inc"

MCS51RegisterInfo::MCS51RegisterInfo() : MCS51GenRegisterInfo(MCS51::PC) {}

const MCPhysReg *
MCS51RegisterInfo::getCalleeSavedRegs(const MachineFunction *) const {
  static const MCPhysReg CalleeSavedRegs[] = {0};
  return CalleeSavedRegs;
}

const uint32_t *MCS51RegisterInfo::getCallPreservedMask(
    const MachineFunction &, CallingConv::ID) const {
  return nullptr;
}

BitVector MCS51RegisterInfo::getReservedRegs(const MachineFunction &) const {
  BitVector Reserved(getNumRegs());
  Reserved.set(MCS51::PC);
  Reserved.set(MCS51::SP);
  Reserved.set(MCS51::PSW);
  return Reserved;
}

bool MCS51RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator, int,
                                            unsigned,
                                            RegScavenger *) const {
  llvm_unreachable("MCS-51 frame index elimination is not implemented");
}

Register MCS51RegisterInfo::getFrameRegister(const MachineFunction &) const {
  return MCS51::SP;
}

const TargetRegisterClass *
MCS51RegisterInfo::getPointerRegClass(unsigned) const {
  return &MCS51::MCS51PTRRegClass;
}
